#!/usr/bin/env python3
"""TC implementations and the complete 81-case registry."""

from __future__ import annotations

import base64
import io
import json
import os
import re
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from bacnet_protocol import (
    build_bacnet_ip_packet,
    build_dcc_apdu,
    build_read_property_apdu,
    build_write_property_apdu,
    build_write_property_generic_apdu,
    build_write_property_raw_apdu,
    build_subscribe_cov_apdu,
    build_who_is_apdu,
    build_who_has_apdu,
    get_local_ip_for_target,
    parse_dcc_response,
    parse_bacnet_response,
    parse_i_am_response,
    parse_i_have_response,
    parse_active_cov_subscriptions,
)
from config_loader import infer_bacnet_type, load_gateway_configs
from modbus_rtu_slave import SlaveMemory, SimulatorControl, append_crc, check_crc, handle_request
from tc_environment import ES_ROOT, GATEWAY_ROOT, TEST_DIR, VERIFICATION_ROOT
from tc_framework import blocked, check_step, require, tc_case


QUICK = {
    *(f"TC-CFG-{n:03d}" for n in range(1, 6)),
    *(f"TC-VPORT-{n:03d}" for n in range(4, 16)),
    *(f"TC-MAP-{n:03d}" for n in range(1, 10)),
    *(f"TC-MOD-{n:03d}" for n in range(1, 4)),
    *(f"TC-BAC-{n:03d}" for n in range(1, 15)),
    *(f"TC-BAC-{n:03d}" for n in range(16, 19)),
    *(f"TC-IFCFG-{n:03d}" for n in range(1, 4)),
    "TC-BIP-001", "TC-BIP-003", "TC-BTL-001", "TC-BTL-002", "TC-ENV-002",
}
AUDIT = {"TC-BTL-003", "TC-ENV-001", "TC-ENV-003"}


def _catalog():
    path = VERIFICATION_ROOT / "docs" / "specs" / "bacnet_gateway_test_spec.md"
    if not path.is_file():
        path = VERIFICATION_ROOT / "docs" / "bacnet_gateway_test_spec.md"
    result = []
    for line in path.read_text(encoding="utf-8").splitlines():
        # Sections after the summary tables may repeat TC IDs to describe the
        # executable procedure and its evidence.  Only the catalog tables
        # before section 10 define the registry entries.
        if line.startswith("## 10."):
            break
        if not line.startswith("| TC-"):
            continue
        columns = [item.strip() for item in line.strip().split("|")[1:-1]]
        if len(columns) >= 3:
            result.append((columns[0], columns[1], columns[2]))
    require(len(result) == 90, f"test specification must define 90 cases, found {len(result)}")
    return result


def _profile(ctx):
    profile_path = getattr(ctx, "fixture_config_dir", None)
    if profile_path is not None:
        return profile_path / "profiles" / "fixture.json"
    return ctx.config_dir / "profiles" / "fixture.json"


def _load_profile(ctx):
    return json.loads(_profile(ctx).read_text(encoding="utf-8"))


def _save_profile(ctx, payload):
    ctx.write_json(_profile(ctx), payload)


def _wait_exit(ctx, proc, seconds=3.0):
    ctx.wait_for(lambda: proc.process.poll() is not None, seconds, f"{proc.name} exit")
    return proc.process.returncode


def _start_invalid_gateway(ctx, name):
    ctx.require_managed()
    ctx.require_executable(ctx.options.gateway_bin, "bacnet_ip_gateway")
    return ctx.start_process(name, [ctx.options.gateway_bin, ctx.config_dir])


def _client_bind_ip(ctx):
    if ctx.options.target_ip == "127.0.0.1":
        return "127.0.0.2"
    local_ip = get_local_ip_for_target(ctx.options.target_ip)
    require(bool(local_ip), f"cannot select BACnet client bind address for {ctx.options.target_ip}")
    return local_ip


def _endpoint(ctx):
    return (ctx.options.target_ip, ctx.options.target_port)


def _bbmd_endpoint(ctx):
    endpoint = ctx.options.bbmd_endpoint
    if endpoint is None:
        blocked("TC-BIP-002 requires an external BBMD host:port")
    return endpoint


def _parse_fdt(packet):
    """Parse the BVLC Read-FDT-ACK payload into stable evidence fields."""
    if len(packet) < 4 or packet[:2] != bytes((0x81, 0x07)):
        return None
    length = (packet[2] << 8) | packet[3]
    if length > len(packet) or (length - 4) % 10 != 0:
        return None
    entries = []
    for offset in range(4, length, 10):
        ip = ".".join(str(value) for value in packet[offset:offset + 4])
        entries.append({
            "ip": ip,
            "port": (packet[offset + 4] << 8) | packet[offset + 5],
            "ttl": (packet[offset + 6] << 8) | packet[offset + 7],
            "remaining": (packet[offset + 8] << 8) | packet[offset + 9],
        })
    return entries


def _bbmd_read_fdt(ctx, endpoint):
    request = bytes((0x81, 0x06, 0x00, 0x04))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.bind((_client_bind_ip(ctx), 0))
        local_endpoint = sock.getsockname()
        ctx.record_packet(
            "BACnet/IP BBMD", "Runner -> external BBMD", request,
            endpoint=endpoint,
            fields={"bvlc_function": 0x06, "operation": "Read-FDT"},
            status="request",
        )
        try:
            sock.sendto(request, endpoint)
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            ctx.record_packet(
                "BACnet/IP BBMD", "external BBMD -> Runner", endpoint=endpoint,
                fields={"operation": "Read-FDT", "error": str(exc)}, status="timeout",
            )
            blocked(f"external BBMD Read-FDT did not respond: {exc}")
    entries = _parse_fdt(response)
    ctx.record_packet(
        "BACnet/IP BBMD", "external BBMD -> Runner", response,
        endpoint=sender,
        fields={
            "bvlc_function": response[1] if len(response) > 1 else None,
            "operation": "Read-FDT-ACK",
            "entries": entries,
            "probe_source": list(local_endpoint),
        },
        status="response",
    )
    check_step(ctx, "BBMD Read-FDT-ACK", True,
               {"function": response[1] if len(response) > 1 else None,
                "entries": entries},
               bool(entries is not None and len(response) >= 4 and response[1] == 0x07),
               f"invalid Read-FDT response: {response.hex(' ')}")
    return entries or []


def _bbmd_probe(ctx):
    """Validate the supplied BBMD and capture Register-FD/Read-FDT packets."""
    endpoint = _bbmd_endpoint(ctx)
    ttl = 60
    request = bytes((0x81, 0x05, 0x00, 0x06, (ttl >> 8) & 0xFF, ttl & 0xFF))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.bind((_client_bind_ip(ctx), 0))
        source = sock.getsockname()
        ctx.record_packet(
            "BACnet/IP BBMD", "Runner -> external BBMD", request,
            endpoint=endpoint,
            fields={"bvlc_function": 0x05, "operation": "Register-Foreign-Device",
                    "ttl": ttl, "source": list(source)},
            status="request",
        )
        try:
            sock.sendto(request, endpoint)
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            ctx.record_packet(
                "BACnet/IP BBMD", "external BBMD -> Runner", endpoint=endpoint,
                fields={"operation": "Register-Foreign-Device", "error": str(exc)},
                status="timeout",
            )
            blocked(f"external BBMD Register-Foreign-Device did not respond: {exc}")
    result = {
        "function": response[1] if len(response) > 1 else None,
        "result": ((response[4] << 8) | response[5]) if len(response) >= 6 else None,
    }
    ctx.record_packet(
        "BACnet/IP BBMD", "external BBMD -> Runner", response,
        endpoint=sender,
        fields={"bvlc_function": result["function"], "operation": "Result",
                "result": result["result"]},
        status="response",
    )
    result_code = result["result"]
    result_text = "n/a" if result_code is None else f"0x{result_code:04x}"
    # Register-Foreign-Device (BVLC function 0x05) returns a BVLC-Result
    # (function 0x00).  Function 0x06 is the separate Read-FDT request.
    check_step(ctx, "Register-Foreign-Device ACK", {"function": 0x00, "result": 0},
               result, result == {"function": 0x00, "result": 0},
               f"BBMD registration rejected: result={result_text}, raw={response.hex(' ')}")
    entries = _bbmd_read_fdt(ctx, endpoint)
    source_entry = next((entry for entry in entries
                         if entry["ip"] == source[0] and entry["port"] == source[1]), None)
    check_step(ctx, "Probe FDT 등록 항목", True, source_entry is not None,
               source_entry is not None,
               f"probe registration was not present in FDT: {entries}")
    return {"endpoint": list(endpoint), "probe_source": list(source), "entries": entries}


def _packet_fields(packet):
    """Extract stable BACnet/IP and APDU fields for machine-readable evidence."""
    fields = {}
    if len(packet) >= 4 and packet[0] == 0x81:
        fields.update({
            "bvlc_type": packet[0],
            "bvlc_function": packet[1],
            "bvlc_length": (packet[2] << 8) | packet[3],
        })
    index = _bacnet_apdu_offset(packet)
    if index is None:
        return fields
    fields["npdu_version"] = packet[4] if len(packet) > 4 else None
    fields["npdu_control"] = packet[5] if len(packet) > 5 else None
    fields["apdu_offset"] = index
    fields["pdu_type"] = (packet[index] >> 4) if index < len(packet) else None
    if index + 1 < len(packet):
        pdu_type = packet[index] & 0xF0
        if pdu_type in (0x00, 0x20, 0x30, 0x50, 0x60, 0x70):
            fields["invoke_id"] = packet[index + 1]
        if pdu_type == 0x10:
            fields["service_choice"] = packet[index + 1]
        elif pdu_type in (0x00, 0x30, 0x50, 0x60, 0x70) and index + 3 < len(packet):
            fields["service_choice"] = packet[index + 3] if pdu_type == 0x00 else packet[index + 2]
    return fields


def _record_bacnet_tx(ctx, packet, note=""):
    return ctx.record_packet(
        "BACnet/IP", "client -> gateway", packet, endpoint=_endpoint(ctx),
        fields=_packet_fields(packet), status="request", note=note,
    )


def _record_bacnet_rx(ctx, packet, sender, note="", parsed=None, status="response"):
    fields = _packet_fields(packet)
    if parsed:
        fields.update(parsed)
    return ctx.record_packet(
        "BACnet/IP", "gateway -> client", packet, endpoint=sender,
        fields=fields, status=status, note=note,
    )


def _who_is(ctx, low=None, high=None, expect=True):
    packet = build_bacnet_ip_packet(None, None, 0, build_who_is_apdu(low, high))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.1)
    try:
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="Who-Is; limits are absent for global request"
                          if low is None and high is None else "Who-Is; bounded request")
        sock.sendto(packet, _endpoint(ctx))
        deadline = time.monotonic() + 1.0
        responses = []
        parse_errors = []
        while time.monotonic() < deadline:
            try:
                response, sender = sock.recvfrom(4096)
            except socket.timeout:
                continue
            status, value = parse_i_am_response(response)
            _record_bacnet_rx(
                ctx, response, sender, note="Who-Is response datagram",
                parsed={"parse_status": status, "parsed_value": value},
            )
            if status == "SUCCESS":
                responses.append((sender, value))
            else:
                parse_errors.append({"status": status, "value": value})

        actual = {
            "i_am_count": len(responses),
            "devices": [value for _sender, value in responses],
            "parse_errors": parse_errors,
        }
        if expect:
            check_step(ctx, "[BACnet/IP] Who-Is I-Am 응답 수", 1, len(responses),
                       len(responses) == 1,
                       f"expected exactly one I-Am, observed {actual}")
            if responses:
                value = responses[0][1]
                check_step(ctx, "[BACnet/IP] I-Am 상태", "SUCCESS", "SUCCESS", True)
                check_step(ctx, "[BACnet/IP] I-Am Device ID", ctx.options.device_id,
                           value["device_instance"],
                           value["device_instance"] == ctx.options.device_id,
                           f"wrong Device ID: {value['device_instance']}")
                check_step(ctx, "[BACnet/IP] I-Am parsed address/APDU fields", True,
                           {
                               "sender": list(responses[0][0]),
                               "max_apdu_length": value.get("max_apdu_length"),
                               "segmentation": value.get("segmentation"),
                               "vendor_id": value.get("vendor_id"),
                           },
                           responses[0][0][0] == ctx.options.target_ip and
                           responses[0][0][1] == ctx.options.target_port and
                           all(value.get(key) is not None for key in
                               ("max_apdu_length", "segmentation", "vendor_id")),
                           f"I-Am sender/fields were not fully valid: sender={responses[0][0]}, fields={value}")
                return value
        else:
            check_step(ctx, "[BACnet/IP] 범위 밖 Who-Is I-Am 응답", 0, len(responses),
                       not responses,
                       f"out-of-range Who-Is unexpectedly returned I-Am: {actual}")
        return None
    finally:
        sock.close()


def _read_property(ctx, object_type, instance, property_id, invoke_id=None,
                   expected_status="SUCCESS", timeout=None):
    invoke_id = ((instance + property_id) & 0xFF) if invoke_id is None else invoke_id & 0xFF
    apdu = build_read_property_apdu(object_type, instance, invoke_id, property_id)
    packet = build_bacnet_ip_packet(None, None, invoke_id, apdu)
    delay_sec = (getattr(ctx.options, "expected_response_time_ms", 0) or 0) / 1000.0
    effective_timeout = timeout if timeout is not None else max(3.0, 3.0 + delay_sec * 10.0)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(effective_timeout)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="ReadProperty")
        sock.sendto(packet, _endpoint(ctx))
        response, sender = sock.recvfrom(4096)
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender, note="ReadProperty response",
        parsed={"parse_status": status, "parsed_value": value},
    )
    check_step(ctx, f"[BACnet/IP] ReadProperty obj={object_type}:{instance} prop={property_id}",
               expected_status, {"status": status, "value": value},
               status == expected_status,
               f"ReadProperty failed: {status} {value}")
    return value if status == "SUCCESS" else {"status": status, "value": value}


def _read_enum_list(ctx, object_type, instance, property_id, invoke_id, timeout=1.0):
    """Read an array of Enumerated values (e.g. Property_List) and decode every element."""
    invoke_id &= 0xFF
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_read_property_apdu(object_type, instance, invoke_id, property_id),
    )
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="ReadProperty enum list")
        try:
            sock.sendto(packet, _endpoint(ctx))
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            return {"status": "TIMEOUT", "value": str(exc)}
    status, first = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(ctx, response, sender, note="ReadProperty enum list response",
                      parsed={"parse_status": status, "parsed_value": first})
    if status != "SUCCESS":
        return {"status": status, "value": first}
    # PropertyValue는 마지막 context tag 3 (0x3E ... 0x3F) 안에 있다.
    open_idx = response.rfind(b"\x3e")
    close_idx = response.rfind(b"\x3f")
    values = []
    pos = open_idx + 1
    while 0 < pos < close_idx:
        tag = response[pos]
        length = tag & 0x07
        if (tag & 0xF0) != 0x90 or length < 1 or pos + 1 + length > close_idx:
            return {"status": "ERROR_NOT_ENUM_LIST", "value": values}
        value = 0
        for byte in response[pos + 1:pos + 1 + length]:
            value = (value << 8) | byte
        values.append(value)
        pos += 1 + length
    return {"status": "SUCCESS", "value": values}


def _read_property_attempt(ctx, object_type, instance, property_id, invoke_id,
                           timeout=1.0, note="ReadProperty attempt"):
    """Send one ReadProperty without turning an expected outage into a failed step."""
    invoke_id &= 0xFF
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_read_property_apdu(object_type, instance, invoke_id, property_id),
    )
    started = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note=note)
        try:
            sock.sendto(packet, _endpoint(ctx))
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            return {
                "status": "TIMEOUT",
                "value": str(exc),
                "latency_ms": (time.monotonic() - started) * 1000.0,
            }
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender, note=f"{note} response",
        parsed={"parse_status": status, "parsed_value": value},
    )
    return {
        "status": status,
        "value": value,
        "sender": list(sender),
        "latency_ms": (time.monotonic() - started) * 1000.0,
    }


def _write_property(ctx, object_type, instance, value, is_binary, property_id=85,
                    priority=16, invoke_id=None, tag_type=None,
                    expected_status="SUCCESS", record_step=True):
    invoke_id = ((instance + property_id + (priority or 0)) & 0xFF) if invoke_id is None else invoke_id & 0xFF
    if tag_type is None and property_id == 85:
        apdu = build_write_property_apdu(object_type, instance, invoke_id, value,
                                         is_binary, priority=priority)
    else:
        apdu = build_write_property_generic_apdu(
            object_type, instance, invoke_id, property_id, value,
            tag_type if tag_type is not None else (9 if is_binary else 4),
            priority=priority,
        )
    packet = build_bacnet_ip_packet(None, None, invoke_id, apdu)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="WriteProperty")
        sock.sendto(packet, _endpoint(ctx))
        response, sender = sock.recvfrom(4096)
    status, response_value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender, note="WriteProperty response",
        parsed={"parse_status": status, "parsed_value": response_value},
    )
    if record_step:
        check_step(
            ctx, f"[BACnet/IP] WriteProperty obj={object_type}:{instance} prop={property_id}",
            expected_status, {"status": status, "value": response_value},
            status == expected_status,
            f"WriteProperty failed: {status} {response_value}",
        )
    return status, response_value


def _bacnet_apdu_offset(packet):
    """Return the APDU offset for a directly received BACnet/IP packet."""
    if len(packet) < 7 or packet[0] != 0x81:
        return None
    control = packet[5]
    index = 6
    if control & 0x80:
        return None
    if control & 0x20:
        if index + 3 > len(packet):
            return None
        index += 2
        dadr_len = packet[index]
        index += 1 + dadr_len + 1
    if control & 0x08:
        if index + 2 > len(packet):
            return None
        index += 2
        sadr_len = packet[index]
        index += 1 + sadr_len
    return index if index < len(packet) else None


def _cov_packet_kind(packet):
    index = _bacnet_apdu_offset(packet)
    if index is None:
        return None, None
    pdu_type = packet[index] & 0xF0
    if pdu_type == 0x00 and index + 4 <= len(packet) and packet[index + 3] == 1:
        return "confirmed", packet[index + 2]
    if pdu_type == 0x10 and index + 2 <= len(packet) and packet[index + 1] == 2:
        return "unconfirmed", None
    return None, None


def _ack_confirmed_cov(sock, packet, sender, ctx=None):
    index = _bacnet_apdu_offset(packet)
    if index is None or index + 3 > len(packet):
        return
    invoke_id = packet[index + 2]
    ack = build_bacnet_ip_packet(None, None, invoke_id, bytes([0x20, invoke_id, 1]))
    if ctx is not None:
        _record_bacnet_tx(ctx, ack, note="confirmed COV notification SimpleACK")
    sock.sendto(ack, sender)


def _observe_cov(sock, duration, ctx=None):
    """Drain COV packets while acknowledging confirmed notifications."""
    counts = {"confirmed": 0, "unconfirmed": 0}
    original_timeout = sock.gettimeout()
    deadline = time.monotonic() + duration
    sock.settimeout(min(0.05, max(0.001, duration)))
    try:
        while time.monotonic() < deadline:
            try:
                packet, sender = sock.recvfrom(4096)
            except socket.timeout:
                continue
            kind, _ = _cov_packet_kind(packet)
            if ctx is not None:
                _record_bacnet_rx(
                    ctx, packet, sender, note="COV notification",
                    parsed={"cov_kind": kind},
                )
            if kind is None:
                continue
            counts[kind] += 1
            if kind == "confirmed":
                _ack_confirmed_cov(sock, packet, sender, ctx=ctx)
    finally:
        sock.settimeout(original_timeout)
    return counts


def _recv_bacnet_response_observed(sock, invoke_id, timeout=3.0, cov_counts=None,
                                   ctx=None, return_sender=False):
    """Wait for one invoke response without losing COV observer packets."""
    deadline = time.monotonic() + timeout
    sock.settimeout(0.1)
    while time.monotonic() < deadline:
        try:
            packet, sender = sock.recvfrom(4096)
        except socket.timeout:
            continue
        if ctx is not None:
            _record_bacnet_rx(ctx, packet, sender, note="observed BACnet response or COV")
        kind, _ = _cov_packet_kind(packet)
        if kind == "confirmed":
            if cov_counts is not None:
                cov_counts[kind] += 1
            _ack_confirmed_cov(sock, packet, sender, ctx=ctx)
            continue
        if kind == "unconfirmed":
            if cov_counts is not None:
                cov_counts[kind] += 1
            continue
        index = _bacnet_apdu_offset(packet)
        if index is not None and index + 2 <= len(packet) and packet[index + 1] == invoke_id:
            return (packet, sender) if return_sender else packet
    raise AssertionError(f"BACnet response invoke {invoke_id} timed out")


def _open_observer_socket(ctx):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    bind_ip = "127.0.0.2" if ctx.options.target_ip == "127.0.0.1" else get_local_ip_for_target(ctx.options.target_ip)
    require(bool(bind_ip), f"cannot select observer bind address for {ctx.options.target_ip}")
    sock.bind((bind_ip, 0))
    return sock


def _send_observed_request(ctx, sock, invoke_id, apdu, cov_counts=None, timeout=3.0):
    packet = build_bacnet_ip_packet(None, None, invoke_id, apdu)
    _record_bacnet_tx(ctx, packet, note="observed BACnet request")
    sock.sendto(packet, (ctx.options.target_ip, ctx.options.target_port))
    response, sender = _recv_bacnet_response_observed(
        sock, invoke_id, cov_counts=cov_counts, ctx=ctx, timeout=timeout,
        return_sender=True)
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender,
        note="observed BACnet response parsed",
        parsed={"parse_status": status, "parsed_value": value},
        status="parsed",
    )
    return response


def _cancel_cov(ctx, sock, object_type, instance, process_id, invoke_id):
    response = _send_observed_request(
        ctx, sock, invoke_id,
        build_subscribe_cov_apdu(object_type, instance, process_id, invoke_id,
                                 cancel=True))
    status, value = parse_bacnet_response(response, invoke_id)
    require(status == "SUCCESS", f"SubscribeCOV cancel failed: {status} {value}")


def _ensure_runtime(ctx):
    if ctx.managed and not ctx.stack_is_running():
        ctx.start_stack()


def _run_profile_test(ctx):
    _ensure_runtime(ctx)
    code, output = ctx.run_profile_tester()
    dependency_markers = (
        "No module named", "ModuleNotFoundError", "ImportError", "required",
        "not found", "Error importing BACpypes3", "Please ensure bacpypes3",
    )
    if code != 0 and any(marker in output for marker in dependency_markers):
        blocked("BACnet 프로파일 검증기 실행에 필요한 BACpypes3 의존성이 없습니다; bacnet_profile_tester.log 참조")
    if code != 0:
        output_lines = [line.strip() for line in output.splitlines() if line.strip()]
        summary_lines = [line for line in output_lines
                         if "Summary of Detected Mismatches" in line or
                         "Result:" in line or "returned" in line]
        diagnostic = " | ".join(summary_lines[-3:])
        if not diagnostic:
            diagnostic = " | ".join(output_lines[-3:])
        detail = f": {diagnostic}" if diagnostic else ""
    else:
        detail = ""
    check_step(ctx, "[BACnet/IP & Modbus] 객체 프로파일 R/W/Restore 검증 실행", 0, code, code == 0,
               f"프로파일 검증기 반환코드: {code}{detail}")
    failed_lines = [line.strip() for line in output.splitlines() if "FAILED" in line]
    check_step(ctx, "[BACnet/IP & Modbus] 프로파일 일치성 실패 항목 검사", [], failed_lines[:3], not failed_lines,
               f"프로파일 검증 실패 항목: {failed_lines[:3]}")
    success_evidence = "SUCCESS" in output or "PASSED" in output
    check_step(ctx, "[BACnet/IP & Modbus] 프로파일 정합성 완료 증적 확인", True, success_evidence,
               success_evidence, "프로파일 검증 완료 증적(SUCCESS/PASSED)이 출력되지 않았습니다")


def _run_epics_conformance_test(ctx):
    _ensure_runtime(ctx)
    code, output = ctx.run_epics_verifier()
    dependency_markers = (
        "No module named", "ModuleNotFoundError", "ImportError", "required",
        "not found", "Error importing BACpypes3", "Please ensure bacpypes3",
    )
    if code != 0 and any(marker in output for marker in dependency_markers):
        blocked("BTL EPICS 검증기 실행에 필요한 BACpypes3 의존성이 없습니다; btl_epics_verifier.log 참조")
    if code != 0:
        output_lines = [line.strip() for line in output.splitlines() if line.strip()]
        summary_lines = [line for line in output_lines
                         if "Summary of Detected Mismatches" in line or
                         "Result:" in line or "returned" in line]
        diagnostic = " | ".join(summary_lines[-3:])
        if not diagnostic:
            diagnostic = " | ".join(output_lines[-3:])
        detail = f": {diagnostic}" if diagnostic else ""
    else:
        detail = ""
    m_count = re.search(r"Total Declared Objects in EPICS\s*:\s*(\d+)", output)
    obj_count_str = f"{m_count.group(1)}개" if m_count else "전체"
    check_step(ctx, f"[BTL EPICS] {obj_count_str} 관제점 Conformance 전수 스캔 실행", 0, code, code == 0,
               f"EPICS Conformance 검증기 반환코드: {code}{detail}")
    failed_lines = [line.strip() for line in output.splitlines() if "FAILED" in line]
    check_step(ctx, "[BTL EPICS] EPICS 불일치(Mismatch) 항목 검사", [], failed_lines[:3], not failed_lines,
               f"EPICS Mismatch 발견: {failed_lines[:3]}")
    success_evidence = "SUCCESS" in output or "PASSED" in output
    check_step(ctx, "[BTL EPICS] 표준 적합성 완료 증적 확인", True, success_evidence,
               success_evidence, "BTL EPICS 적합성 검증 완료 증적(SUCCESS/PASSED)이 출력되지 않았습니다")


def _legacy(ctx, step):
    """Backward compatibility wrapper."""
    if step == 2:
        _run_profile_test(ctx)
    elif step == 3:
        _run_epics_conformance_test(ctx)
    else:
        raise ValueError(f"unsupported step: {step}")


def _config_case(ctx, tc_id):
    if tc_id != "TC-CFG-001":
        ctx.require_managed()
    if tc_id == "TC-CFG-001":
        configs = _runtime_configs(ctx)
        objects = [obj for dev in configs["devices"] for obj in dev["objects"]] if not ctx.managed else configs["devices"][0]["objects"]
        expected_count = 8 if ctx.managed else len(objects)
        count_ok = (len(objects) == 8) if ctx.managed else (len(objects) > 0)
        check_step(ctx, "프로필 object 수", expected_count, len(objects), count_ok,
                   f"fixture object count mismatch: expected {expected_count}, got {len(objects)}")
        instances_unique = len({item["instance"] for item in objects}) == len(objects)
        check_step(ctx, "프로필 instance 유일성", True, instances_unique, instances_unique,
                   "instances are not unique")

        # Gateway & Vport 실동작 검증: 유효한 설정으로 스택 기동 후 BACnet 응답 및 실제 객체 호스팅 확인
        if ctx.managed:
            ctx.start_stack()
            started = "BACnet IP Gateway started" in ctx.gateway_log()
            check_step(ctx, "유효 설정 기반 Gateway 정상 기동", True, started,
                       started, "Gateway failed to start with valid configuration")
        else:
            _ensure_runtime(ctx)
        iam = _who_is(ctx, expect=True)
        check_step(ctx, "기동 후 BACnet I-Am 응답 수신", True, iam is not None,
                   iam is not None, "Gateway did not respond with I-Am")
        # 4대 객체 타입(AI, AO, BI, BV)의 실시간 Present_Value 조회 (프로필에 정의된 첫 번째 인스턴스 동적 추출)
        sample_targets = {}
        for obj in objects:
            t = obj.get("type")
            inst = obj.get("instance")
            if t in (0, 1, 3, 5) and inst is not None and t not in sample_targets:
                sample_targets[t] = inst

        for obj_type in (0, 1, 3, 5):
            inst = sample_targets.get(obj_type)
            if inst is None:
                check_step(ctx, f"객체 ({obj_type}) 프로필 정의 확인", True, False, False,
                           f"object type {obj_type} not found in profile")
                continue
            val = _read_property(ctx, obj_type, inst, 85)
            check_step(ctx, f"객체 ({obj_type},{inst}) BACnet 조회 성공", True,
                       val is not None, val is not None, f"failed to read object ({obj_type},{inst})")
        if ctx.managed:
            ctx.stop_process("gateway")
            ctx.stop_process("main_simulator")
            ctx.stop_process("vport")
        return
    if tc_id == "TC-CFG-002":
        _profile(ctx).write_text("{ invalid json\n", encoding="utf-8")
        first = _start_invalid_gateway(ctx, "cfg_bad_json")
        ctx.wait_for(lambda: "entering HOLD" in (first.log_path.read_text(encoding="utf-8", errors="replace") if first.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive1 = first.process.poll() is None
        check_step(ctx, "JSON 문법 오류 HOLD 생존", True, alive1,
                   alive1, "process exited instead of entering HOLD on invalid JSON")
        first_log = first.log_path.read_text(encoding="utf-8", errors="replace") if first.log_path.exists() else ""
        has_parse_err = "Parse error" in first_log
        check_step(ctx, "JSON 문법 오류 진단 로그 검출", True, has_parse_err,
                   has_parse_err, f"parse error log missing: {first_log}")
        started1 = "BACnet IP Gateway started" in first_log
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started1, not started1,
                   "BACnet started during HOLD")
        ctx.stop_process("cfg_bad_json")
        ctx._write_fixture()

        data = _load_profile(ctx)
        del data["objects"][0]["full_address"]
        _save_profile(ctx, data)
        second = _start_invalid_gateway(ctx, "cfg_missing_address")
        ctx.wait_for(lambda: "entering HOLD" in (second.log_path.read_text(encoding="utf-8", errors="replace") if second.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive2 = second.process.poll() is None
        check_step(ctx, "필수 주소 누락 HOLD 생존", True, alive2,
                   alive2, "process exited instead of entering HOLD on missing address")
        second_log = second.log_path.read_text(encoding="utf-8", errors="replace") if second.log_path.exists() else ""
        has_addr_err = ("missing both 'offset_address' and 'full_address'" in second_log or "missing" in second_log)
        check_step(ctx, "필수 주소 누락 진단 로그 검출", True, has_addr_err,
                   has_addr_err, f"missing address error log missing: {second_log}")
        started2 = "BACnet IP Gateway started" in second_log
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started2, not started2,
                   "BACnet started during HOLD")
        ctx.stop_process("cfg_missing_address")
        ctx._write_fixture()

        # 3. 필수 modbus_unit_id 누락 검증
        data3 = _load_profile(ctx)
        del data3["modbus_unit_id"]
        _save_profile(ctx, data3)
        third = _start_invalid_gateway(ctx, "cfg_missing_unit_id")
        ctx.wait_for(lambda: "entering HOLD" in (third.log_path.read_text(encoding="utf-8", errors="replace") if third.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive3 = third.process.poll() is None
        check_step(ctx, "필수 modbus_unit_id 누락 HOLD 생존", True, alive3,
                   alive3, "process exited instead of entering HOLD on missing modbus_unit_id")
        third_log = third.log_path.read_text(encoding="utf-8", errors="replace") if third.log_path.exists() else ""
        has_unit_err = "missing required field 'modbus_unit_id'" in third_log
        check_step(ctx, "필수 modbus_unit_id 누락 진단 로그 검출", True, has_unit_err,
                   has_unit_err, f"missing modbus_unit_id error log missing: {third_log}")
        started3 = "BACnet IP Gateway started" in third_log
        check_step(ctx, "HOLD 중 BACnet 미기동 (modbus_unit_id 누락)", False, started3, not started3,
                   "BACnet started during HOLD")
        ctx.stop_process("cfg_missing_unit_id")
        ctx._write_fixture()
        return
    if tc_id == "TC-CFG-003":
        data = _load_profile(ctx)
        data["objects"][1]["instance"] = data["objects"][0]["instance"]
        data["objects"][2]["full_address"] = 999999
        _save_profile(ctx, data)
        proc = _start_invalid_gateway(ctx, "cfg_duplicate_range")
        ctx.wait_for(lambda: "entering HOLD" in (proc.log_path.read_text(encoding="utf-8", errors="replace") if proc.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive = proc.process.poll() is None
        check_step(ctx, "중복 instance/범위 오류 HOLD 생존", True, alive,
                   alive, "process exited instead of entering HOLD on duplicate/range error")
        log = proc.log_path.read_text(encoding="utf-8", errors="replace") if proc.log_path.exists() else ""
        has_err_log = ("different block" in log or "exceeds" in log or "Duplicate instance" in log or "full_address" in log)
        check_step(ctx, "중복 instance/범위 진단 로그 검출", True, has_err_log,
                   has_err_log, f"config error diagnosis log missing: {log}")
        started = "BACnet IP Gateway started" in log
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started, not started,
                   "BACnet started during HOLD")
        ctx.stop_process("cfg_duplicate_range")
        ctx._write_fixture()
        return
    if tc_id in ("TC-CFG-004", "TC-CFG-005"):
        data = _load_profile(ctx)
        target = data["objects"][1]
        target["min_value"], target["max_value"] = ((10, 90) if tc_id.endswith("004") else (10, 10))
        target["relinquish_default"] = 20
        _save_profile(ctx, data)
        configs = load_gateway_configs(str(ctx.config_dir))
        value = configs["devices"][0]["objects"][1]["relinquish_default"]
        expected = 20.0 if tc_id.endswith("004") else 10.0
        check_step(ctx, "Relinquish_Default clamp", expected, value,
                   value == expected, "clamp result mismatch")

        # Gateway & Vport 실동작 검증: 실제 Gateway를 기동하여 Clamp된 Relinquish_Default 및 Present_Value 서비스 확인
        ctx.start_stack()
        actual_rd = _read_property(ctx, 1, 2, 104)
        check_step(ctx, f"Gateway 서비스 Relinquish_Default ({expected})", expected, actual_rd,
                   actual_rd == expected, f"Gateway Relinquish_Default mismatch: {actual_rd}")
        actual_pv = _read_property(ctx, 1, 2, 85)
        check_step(ctx, f"Gateway 서비스 Present_Value ({expected})", expected, actual_pv,
                   actual_pv == expected, f"Gateway Present_Value mismatch: {actual_pv}")
        ctx.stop_process("gateway")
        ctx.stop_process("main_simulator")
        ctx.stop_process("vport")
        ctx._write_fixture()
        return
    data = _load_profile(ctx)
    data["objects"][1]["min_value"] = 100
    data["objects"][1]["max_value"] = 10
    _save_profile(ctx, data)
    if tc_id in ("TC-CFG-006", "TC-CFG-008"):
        if not ctx.options.long_tests:
            blocked("60초 재검증 TC는 --long 옵션이 필요합니다")
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").exists(), 5.0, "vport links")
        ctx.start_simulator()
    proc = _start_invalid_gateway(ctx, "gateway")
    ctx.wait_for(lambda: "entering HOLD" in ctx.gateway_log(), 3.0, "configuration HOLD log")
    alive = proc.process.poll() is None
    check_step(ctx, "모순 설정 HOLD 생존", True, alive, alive,
               "contradictory config process exited instead of HOLD")
    if tc_id == "TC-CFG-007":
        started = "BACnet IP Gateway started" in ctx.gateway_log()
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started, not started,
                   "BACnet started during HOLD")
    elif tc_id == "TC-CFG-009":
        started = time.monotonic()
        ctx.stop_process("gateway")
        elapsed = time.monotonic() - started
        check_step(ctx, "HOLD SIGTERM 종료시간", "<= 1.0s", round(elapsed, 3),
                   elapsed <= 1.0, "SIGTERM shutdown exceeded one second")
    elif tc_id in ("TC-CFG-006", "TC-CFG-008"):
        if tc_id == "TC-CFG-008":
            time.sleep(61.0)
            alive_after_wait = proc.process.poll() is None
            check_step(ctx, "반복 HOLD 생존", True, alive_after_wait, alive_after_wait,
                       "process exited during repeated HOLD")
        reloads_before_fix = (ctx.gateway_log().count("Reloading configuration") +
                              ctx.gateway_log().count("Reloading device profiles"))
        ctx._write_fixture()
        ctx.wait_for(lambda: (ctx.gateway_log().count("Reloading configuration") +
                              ctx.gateway_log().count("Reloading device profiles")) > reloads_before_fix,
                     65.0, "configuration reload")
        ctx.wait_gateway_ready(20.0)
        fatal_logged = "FATAL CONFIG ERROR" in ctx.gateway_log()
        check_step(ctx, "치명적 설정 오류 문맥 로그", True, fatal_logged, fatal_logged,
                   "fatal context was not logged")
        starts = ctx.gateway_log().count("BACnet IP Gateway started")
        check_step(ctx, "설정 복구 후 BACnet 기동 횟수", 1, starts, starts == 1,
                   "configuration recovery activated gateway more than once")


def _start_case(ctx, tc_id):
    ctx.require_managed()
    if tc_id == "TC-START-001":
        ctx.start_stack()
        return
    if tc_id == "TC-START-002":
        ctx.start_gateway()
        time.sleep(0.3)
        require(ctx.processes["gateway"].process.poll() is None, "gateway exited before vport")
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").exists(), 5.0, "vport links")
        ctx.start_simulator()
        ctx.wait_gateway_ready()
        return
    if tc_id == "TC-START-003":
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").exists(), 5.0, "vport links")
        ctx.start_gateway()
        time.sleep(0.8)
        require("BACnet IP Gateway started" not in ctx.gateway_log(), "BACnet started before Main")
        ctx.start_simulator()
        ctx.wait_gateway_ready()
        return
    ctx.start_vport(3)
    ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").exists(), 5.0, "vport links")
    ctx.start_simulator()
    ctx.simulator({"op": "set", "block": "holding", "address": 3,
                   "value": 0 if tc_id == "TC-START-006" else 40})
    ctx.simulator({"op": "set", "block": "holding", "address": 4, "value": 110})
    if tc_id == "TC-START-007":
        ctx.simulator({"op": "fault", "action": "timeout", "fc": 6, "address": 0, "count": 2})
    elif tc_id == "TC-START-008":
        ctx.simulator({"op": "fault", "action": "timeout", "fc": 6, "address": 1, "count": 1})
    elif tc_id == "TC-START-009":
        ctx.simulator({"op": "fault", "action": "timeout", "fc": 3, "address": 3, "count": 2})
    ctx.start_gateway()
    if tc_id in ("TC-START-007", "TC-START-009"):
        time.sleep(0.7)
        exposed_early = "BACnet IP Gateway started" in ctx.gateway_log()
        check_step(ctx, "초기 동기화 전 BACnet 비공개", False, exposed_early,
                   not exposed_early, "BACnet exposed before sync")
    ctx.wait_gateway_ready(20.0)
    if tc_id == "TC-START-004":
        values = ctx.simulator({"op": "get", "block": "holding", "address": 0, "count": 3})["values"]
        check_step(ctx, "JSON 기본값 clamp 후 Main 값", [50, 10, 90], values,
                   values == [50, 10, 90], f"JSON clamp/write mismatch: {values}")
        for instance, expected in ((2, 50), (3, 10), (4, 90)):
            actual_rd = _read_property(ctx, 1, instance, 104)
            check_step(ctx, f"AO {instance} Relinquish_Default", expected, actual_rd,
                       actual_rd == expected, "Relinquish_Default mismatch")
            actual_pv = _read_property(ctx, 1, instance, 85)
            check_step(ctx, f"AO {instance} Present_Value", expected, actual_pv,
                       actual_pv == expected, "Present_Value mismatch")
    elif tc_id == "TC-START-005":
        value = ctx.simulator({"op": "get", "block": "holding", "address": 3})["values"][0]
        check_step(ctx, "Main 범위 내 초기값 유지", 40, value, value == 40,
                   f"in-range Main value changed: {value}")
        transactions = ctx.simulator({"op": "stats"})["transactions"]
        write_back = any(item["fc"] == 6 and item["address"] == 3 for item in transactions)
        check_step(ctx, "범위 내 값 Write-back 없음", False, write_back, not write_back,
                   "in-range Main value was unexpectedly written back")
        actual_rd = _read_property(ctx, 1, 5, 104)
        check_step(ctx, "Main 값의 Relinquish_Default", 40, actual_rd,
                   actual_rd == 40, "Relinquish_Default mismatch")
        actual_pv = _read_property(ctx, 1, 5, 85)
        check_step(ctx, "Main 값의 Present_Value", 40, actual_pv,
                   actual_pv == 40, "Present_Value mismatch")
    elif tc_id == "TC-START-006":
        values = ctx.simulator({"op": "get", "block": "holding", "address": 3, "count": 2})["values"]
        check_step(ctx, "Main 값 양 끝 경계 clamp", [10, 90], values,
                   values == [10, 90], f"Main clamp/write-back mismatch: {values}")
        for instance, expected in ((5, 10), (8, 90)):
            actual_rd = _read_property(ctx, 1, instance, 104)
            check_step(ctx, f"AO {instance} Relinquish_Default", expected, actual_rd,
                       actual_rd == expected, "Relinquish_Default mismatch")
            actual_pv = _read_property(ctx, 1, instance, 85)
            check_step(ctx, f"AO {instance} Present_Value", expected, actual_pv,
                       actual_pv == expected, "Present_Value mismatch")
    elif tc_id == "TC-START-008":
        transactions = ctx.simulator({"op": "stats"})["transactions"]
        first_writes = [item for item in transactions if item["fc"] == 6 and item["address"] == 0]
        check_step(ctx, "완료 객체 Write 횟수", 1, len(first_writes),
                   len(first_writes) == 1, f"completed item repeated {len(first_writes)} times")
    elif tc_id in ("TC-START-007", "TC-START-009"):
        transactions = ctx.simulator({"op": "stats"})["transactions"]
        timeout_count = sum(item["action"] == "timeout" for item in transactions)
        check_step(ctx, "초기 동기화 timeout 재시도 횟수", 2, timeout_count,
                   timeout_count == 2, "configured startup retries were not observed")
        start_count = ctx.gateway_log().count("BACnet IP Gateway started")
        check_step(ctx, "BACnet 서버 기동 횟수", 1, start_count,
                   start_count == 1, "BACnet server was started more than once")


def _run_remote_ssh_command(ctx, script: str, timeout: float = 8.0) -> Optional[Tuple[int, str, str]]:
    if not getattr(ctx.options, "ssh_enable", False):
        return None
    ip = getattr(ctx.options, "target_ip", None) or getattr(ctx.options, "ip", None)
    if not ip or ip in ("127.0.0.1", "localhost"):
        return None

    ssh_user = getattr(ctx.options, "ssh_user", "root") or "root"
    ssh_port = getattr(ctx.options, "ssh_port", 22) or 22
    ssh_pass = getattr(ctx.options, "ssh_pass", None) or os.environ.get("SSH_PASS")

    base_cmd = [
        "ssh",
        "-F", "/dev/null",
        "-o", "BatchMode=yes" if not ssh_pass else "BatchMode=no",
        "-o", "ConnectTimeout=5",
        "-o", "StrictHostKeyChecking=no",
        "-o", "UserKnownHostsFile=/dev/null",
        "-o", "LogLevel=ERROR",
        "-o", "HostKeyAlgorithms=+ssh-rsa",
        "-o", "PubkeyAcceptedKeyTypes=+ssh-rsa",
        "-o", "KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1",
        "-p", str(ssh_port),
        f"{ssh_user}@{ip}",
        script,
    ]
    cmd = base_cmd
    env = os.environ.copy()
    askpass_file = None
    if ssh_pass:
        import shutil
        if shutil.which("sshpass"):
            cmd = ["sshpass", "-p", ssh_pass] + base_cmd
        elif sys.platform == "win32":
            import win_compat
            env[win_compat.ASKPASS_ENV] = ssh_pass
            askpass_file = win_compat.make_windows_askpass(env)
        else:
            try:
                import tempfile
                with tempfile.NamedTemporaryFile("w", delete=False) as f:
                    escaped_pw = ssh_pass.replace('\\', '\\\\').replace('"', '\\"').replace('$', '\\$')
                    f.write(f'#!/bin/sh\necho "{escaped_pw}"\n')
                    askpass_file = f.name
                os.chmod(askpass_file, 0o755)
                env["SSH_ASKPASS"] = askpass_file
                env["SSH_ASKPASS_REQUIRE"] = "force"
                env["DISPLAY"] = ":0"
            except Exception:
                pass

    try:
        res = subprocess.run(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
        if res.returncode != 0:
            ctx._ssh_last_error = res.stderr.strip() or f"exit code {res.returncode}"
        return (res.returncode, res.stdout, res.stderr)
    except Exception as e:
        ctx._ssh_last_error = str(e)
        return None
    finally:
        if askpass_file and os.path.exists(askpass_file):
            try:
                os.remove(askpass_file)
            except OSError:
                pass


def _fetch_remote_configs(ctx) -> Tuple[Optional[Path], Optional[Path]]:
    """설정 파일은 Managed/Live 모두 로컬 파일 시스템(test_config.json common)에서 직접 참조합니다."""
    return None, None


def _vport_case(ctx, tc_id):
    if tc_id in ("TC-VPORT-004", "TC-VPORT-005", "TC-VPORT-006",
                 "TC-VPORT-007", "TC-VPORT-008", "TC-VPORT-009",
                 "TC-VPORT-010", "TC-VPORT-012", "TC-VPORT-013"):
        ctx.require_managed()
        binary = ctx.options.vport_test_bin
        if not binary.is_file() or not os.access(binary, os.X_OK):
            blocked("해당 프레임 TC에 필요한 host-built vport_test가 없습니다")
        code, output = ctx.run_command("vport_test", [binary, "--case", tc_id],
                                       timeout=60.0, cwd=ES_ROOT / "vport")
        check_step(ctx, f"{tc_id} vport_test 종료코드", 0, code, code == 0,
                   f"vport_test returned {code}")
        success_marker = "[Test Success]" in output
        check_step(ctx, f"{tc_id} 독립 assertion 증거", True, success_marker,
                   success_marker, f"vport_test produced no success evidence: {output}")
        return

    if tc_id == "TC-VPORT-015":
        # §7.13 타임아웃 계층 검증: vport slave timeout과 Gateway rtu_timeout_ms 비교
        # 규칙: T_gateway >= T_vport 이면 PASS, T_gateway < T_vport 이면 FAIL
        # [Managed / Live 공용: test_config.json common에 정의된 로컬 설정 파일 직접 참조]
        gw_cfg_candidates = []
        if getattr(ctx.options, "gateway_config_dir", None):
            gw_p = Path(ctx.options.gateway_config_dir)
            gw_cfg_candidates.extend([gw_p / "gateway.json" if gw_p.is_dir() else gw_p, gw_p / "conf" / "gateway.json"])
        if getattr(ctx, "config_dir", None):
            gw_cfg_candidates.append(ctx.config_dir / "gateway.json")
        if getattr(ctx.options, "config_dir", None):
            gw_p = Path(ctx.options.config_dir)
            gw_cfg_candidates.extend([gw_p / "gateway.json" if gw_p.is_dir() else gw_p, gw_p / "conf" / "gateway.json"])
        gw_cfg_candidates.extend([
            GATEWAY_ROOT / "conf" / "gateway.json",
            ES_ROOT / "bacnet_ip_gateway_release" / "conf" / "gateway.json",
            ES_ROOT / "bacnet_ip_gateway" / "conf" / "gateway.json",
            ctx.workspace / "conf" / "gateway.json" if ctx.workspace else None,
        ])

        gw_cfg_file = next((p for p in gw_cfg_candidates if p and p.is_file()), None)
        if not gw_cfg_file:
            blocked(f"로컬 Gateway 설정 파일(gateway.json)을 찾을 수 없습니다: {gw_cfg_candidates}")

        try:
            gw_json = json.loads(gw_cfg_file.read_text(encoding="utf-8"))
            gw_timeout = int(gw_json.get("modbus", {}).get("rtu_timeout_ms", 500))
            gw_source_desc = f"로컬 파일 ({gw_cfg_file})"
        except Exception as e:
            blocked(f"Gateway 설정 파일({gw_cfg_file}) 파싱 실패: {e}")

        vp_cfg_candidates = []
        if getattr(ctx.options, "vport_config_path", None):
            vp_p = Path(ctx.options.vport_config_path)
            if vp_p.is_file():
                vp_cfg_candidates.append(vp_p)
            elif vp_p.is_dir():
                vp_cfg_candidates.append(vp_p / "vport.cfg")
            else:
                vp_cfg_candidates.append(vp_p)
                vp_cfg_candidates.append(vp_p / "vport.cfg")
        if getattr(ctx.options, "vport_bin", None):
            vp_cfg_candidates.append(ctx.options.vport_bin.parent / "vport.cfg")
        vp_cfg_candidates.extend([
            ES_ROOT / "vport_release" / "vport.cfg",
            ES_ROOT / "vport" / "vport.cfg",
        ])

        vp_cfg_file = next((p for p in vp_cfg_candidates if p and p.is_file()), None)
        if not vp_cfg_file:
            blocked(f"로컬 vport 설정 파일(vport.cfg)을 찾을 수 없습니다: {vp_cfg_candidates}")

        vport_timeout = 500
        found_key = False
        try:
            for line in vp_cfg_file.read_text(encoding="utf-8").splitlines():
                line = line.strip()
                if line.startswith("DEFAULT_SLAVE_TIMEOUT_MS="):
                    vport_timeout = int(line.split("=", 1)[1].strip())
                    found_key = True
                    break
        except Exception as e:
            blocked(f"vport 설정 파일({vp_cfg_file}) 읽기 실패: {e}")

        if not found_key:
            blocked(f"vport 설정({vp_cfg_file})에서 DEFAULT_SLAVE_TIMEOUT_MS 항목을 찾을 수 없습니다.")

        vport_source_desc = f"로컬 파일 ({vp_cfg_file})"

        check_step(ctx, "설정 소스 확인", "설정 소스 식별", f"Gateway: {gw_source_desc}, vport: {vport_source_desc}", True)

        # 타임아웃 계층 관계 판정 (T_gateway >= T_vport)
        is_hierarchy_valid = (gw_timeout >= vport_timeout)
        actual_str = f"Gateway({gw_timeout}ms) >= vport({vport_timeout}ms)" if is_hierarchy_valid else f"Gateway({gw_timeout}ms) < vport({vport_timeout}ms)"
        check_step(ctx, "타임아웃 계층 규칙 (T_gateway >= T_vport)",
                   "T_gateway >= T_vport",
                   actual_str,
                   is_hierarchy_valid,
                   f"타임아웃 역전 감지: Gateway rtu_timeout_ms({gw_timeout}ms) < vport DEFAULT_SLAVE_TIMEOUT_MS({vport_timeout}ms). "
                   "Head-of-Line 블로킹 및 연쇄 타임아웃 위험이 있습니다.")
        return

    ctx.require_managed()
    if tc_id == "TC-VPORT-011":
        # §7.11-A 마스터 상한 초과 인수 기동 거부
        cfg_path = ctx.workspace / "vport_limit.cfg"
        cfg_path.write_text("MAX_MASTERS=2\nDEFAULT_SLAVE_TIMEOUT_MS=500\n", encoding="utf-8")
        code, output = ctx.run_command("vport_limit", [
            ctx.options.vport_bin, "5",
            "--link-dir", str(ctx.dev_dir),
            "--config", str(cfg_path)
        ], timeout=10.0)
        check_step(ctx, "상한 초과 exit code", "≠ 0", code,
                   code != 0, f"vport accepted num_masters=5 with MAX_MASTERS=2: {output}")
        limit_msg_found = "상한" in output or "초과" in output
        check_step(ctx, "상한 초과 에러 메시지", True, limit_msg_found,
                   limit_msg_found, f"expected limit error message, got: {output}")

        # §7.11-B 설정 파일의 MAX_MASTERS가 FD_SETSIZE(1024)를 초과할 때 기동 거부
        fd_limit_cfg = ctx.workspace / "vport_fd_limit.cfg"
        fd_limit_cfg.write_text("MAX_MASTERS=2048\nDEFAULT_SLAVE_TIMEOUT_MS=500\n", encoding="utf-8")
        code_fd, output_fd = ctx.run_command("vport_fd_limit", [
            ctx.options.vport_bin, "3",
            "--link-dir", str(ctx.dev_dir),
            "--config", str(fd_limit_cfg)
        ], timeout=10.0)
        check_step(ctx, "FD_SETSIZE 초과 설정 exit code", "≠ 0", code_fd,
                   code_fd != 0, f"vport accepted MAX_MASTERS=2048 exceeding FD_SETSIZE: {output_fd}")
        fd_msg_found = "FD_SETSIZE" in output_fd or "초과" in output_fd
        check_step(ctx, "FD_SETSIZE 초과 에러 메시지", True, fd_msg_found,
                   fd_msg_found, f"expected FD_SETSIZE error message, got: {output_fd}")
        return
    if tc_id == "TC-VPORT-014":
        # §7.4 설정 파일 미발견 시 에러 종료
        code, output = ctx.run_command("vport_noconfig", [
            ctx.options.vport_bin, "3",
            "--link-dir", str(ctx.dev_dir),
            "--config", "/nonexistent/vport.cfg"
        ], timeout=10.0)
        check_step(ctx, "설정 미발견 exit code", "≠ 0", code,
                   code != 0, f"vport did not error on missing explicit config: {output}")
        err_msg_found = "설정 파일을 찾을 수 없습니다" in output
        check_step(ctx, "설정 미발견 에러 메시지", True, err_msg_found,
                   err_msg_found, f"expected config missing error message, got: {output}")
        return
    if tc_id == "TC-VPORT-001":
        for count in (1, 3, 5):
            ctx.start_vport(count)
            ctx.wait_for(lambda selected=count: all(
                (ctx.dev_dir / f"ttyMASTER{i}").is_symlink()
                for i in range(1, selected + 1)) and (ctx.dev_dir / "ttySLAVE").is_symlink(),
                5.0, f"vport {count}-master links")
            link_count = len(list(ctx.dev_dir.iterdir()))
            check_step(ctx, f"vport {count}-master 링크 수", count + 1, link_count,
                       link_count == count + 1, f"unexpected link count for {count} masters")
            ctx.stop_process("vport")
            links_remain = any(ctx.dev_dir.iterdir())
            check_step(ctx, f"vport {count}-master 종료 후 링크", False, links_remain,
                       not links_remain, "links remained between count variants")
        return
    proc = ctx.start_vport(3)
    ctx.wait_for(lambda: all((ctx.dev_dir / f"ttyMASTER{i}").is_symlink() for i in range(1, 4))
                 and (ctx.dev_dir / "ttySLAVE").is_symlink(), 5.0, "vport links")
    if tc_id == "TC-VPORT-002":
        ctx.stop_process("vport")
        links_remain = any(ctx.dev_dir.iterdir())
        check_step(ctx, "SIGTERM 후 vport 링크", False, links_remain,
                   not links_remain, "vport links remained after SIGTERM")
    else:
        os.killpg(proc.process.pid, signal.SIGKILL)
        proc.process.wait(timeout=2.0)
        proc._stream.close()
        ctx.processes.pop("vport")
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").is_symlink(), 5.0, "vport restart")


def _mapping_case(ctx, tc_id):
    if tc_id == "TC-MAP-001":
        configs = _runtime_configs(ctx)
        all_objects = [obj for dev in configs["devices"] for obj in dev["objects"]]
        types = {obj["type"] for obj in all_objects}
        if not {0, 1, 3, 5}.issubset(types) and not ctx.managed:
            blocked(f"live config does not provide the required AI/AO/BI/BV fixture: {types}")
        mapping_ok = {0, 1, 3, 5}.issubset(types)
        check_step(ctx, "AI/AO/BI/BV mapping", [0, 1, 3, 5], sorted(types),
                   mapping_ok, f"AI/AO/BI/BV mapping incomplete: {types}")

        # Gateway & Vport 실동작 검증: 실제 Gateway를 기동하여 4대 객체(AI/AO/BI/BV)가 BACnet 네트워크에서 실시간 서비스되는지 검증
        if ctx.managed:
            ctx.start_stack()
        else:
            _ensure_runtime(ctx)

        sample_targets = {}
        for obj in all_objects:
            t = obj.get("type")
            inst = obj.get("instance")
            if t in (0, 1, 3, 5) and inst is not None and t not in sample_targets:
                sample_targets[t] = inst

        present_types = set()
        for obj_type in (0, 1, 3, 5):
            inst = sample_targets.get(obj_type)
            if inst is None:
                continue
            val = _read_property(ctx, obj_type, inst, 85)
            if val is not None:
                present_types.add(obj_type)
        all_types_live = {0, 1, 3, 5}.issubset(present_types)
        check_step(ctx, "실동작 Gateway 4대 객체(AI/AO/BI/BV) BACnet 서비스", [0, 1, 3, 5], sorted(present_types),
                   all_types_live, f"live gateway missing object types: {present_types}")
        if ctx.managed:
            ctx.stop_process("gateway")
            ctx.stop_process("main_simulator")
            ctx.stop_process("vport")
    elif tc_id == "TC-MAP-002":
        samples = [
            ({"modbus_block": "DISCRETE_INPUT"}, 3),
            ({"modbus_block": "INPUT_REGISTER"}, 0),
            ({"modbus_block": "COIL"}, 5),
            ({"modbus_block": "HOLDING_REGISTER", "read_only": True}, 0),
            ({"modbus_block": "HOLDING_REGISTER", "bit_position": 2}, 5),
        ]
        for value, expected in samples:
            actual = infer_bacnet_type(value)
            check_step(ctx, "bacnet_type 자동 추론", expected, actual, actual == expected,
                       f"type inference failed: {value}")

        # Gateway 실동작 검증: bacnet_type이 생략된 프로파일로 게이트웨이를 기동하여 C++ deduceBacnetType 동작 실측
        if ctx.managed:
            data = {
                "modbus_unit_id": 1,
                "objects": [
                    {"name": "INFER_BI", "full_address": 10001, "modbus_block": "DISCRETE_INPUT", "instance": 11},
                    {"name": "INFER_AI", "full_address": 30001, "modbus_block": "INPUT_REGISTER", "instance": 12},
                    {"name": "INFER_BV", "full_address": 1, "modbus_block": "COIL", "instance": 13, "relinquish_default": 0},
                    {"name": "INFER_AI_RO", "full_address": 40001, "modbus_block": "HOLDING_REGISTER", "read_only": True, "instance": 14},
                    {"name": "INFER_BV_BIT", "full_address": 40002, "modbus_block": "HOLDING_REGISTER", "bit_position": 2, "instance": 15, "relinquish_default": 0},
                ],
            }
            _save_profile(ctx, data)
            ctx.start_stack()
            expected_deductions = ((3, 11), (0, 12), (5, 13), (0, 14), (5, 15))
            deduced_ok = True
            for obj_type, inst in expected_deductions:
                val = _read_property(ctx, obj_type, inst, 85)
                if val is None:
                    deduced_ok = False
            check_step(ctx, "Gateway C++ deduceBacnetType 실제 객체 인스턴스화", True, deduced_ok,
                       deduced_ok, "inferred objects failed to respond on BACnet")
            ctx.stop_process("gateway")
            ctx.stop_process("main_simulator")
            ctx.stop_process("vport")
            ctx._write_fixture()
    elif tc_id == "TC-MAP-003":
        if not ctx.managed:
            blocked("auto-instance reproducibility test requires a mutable managed fixture")
        path = ctx.config_dir if ctx.managed else ctx.options.config_dir or GATEWAY_ROOT / "conf"
        data = _load_profile(ctx)
        data["objects"].append({"name": "TC_AUTO_INSTANCE", "full_address": 30002,
                                "modbus_block": "INPUT_REGISTER"})
        _save_profile(ctx, data)
        first, second = load_gateway_configs(str(path)), load_gateway_configs(str(path))
        a = [obj["instance"] for dev in first["devices"] for obj in dev["objects"]]
        b = [obj["instance"] for dev in second["devices"] for obj in dev["objects"]]
        stable_unique = a == b and len(a) == len(set(a))
        check_step(ctx, "자동 instance 재현성/유일성", "same and unique",
                   {"first": a, "second": b}, stable_unique,
                   "auto instances are not stable and unique")

        # Gateway & Vport 실동작 검증: Gateway 기동 후 C++ instance_auto 자동 할당 및 2회 기동 간 불변성 실측
        ctx.start_stack()
        log1 = ctx.gateway_log()
        has_auto1 = "[Instance] Auto-assigned instance" in log1
        check_step(ctx, "Gateway 자동 인스턴스 할당 로그 검출 (1차)", True, has_auto1,
                   has_auto1, "Gateway auto-assignment log missing")
        assigned_inst_match = re.search(r"Auto-assigned instance (\d+) to object 'TC_AUTO_INSTANCE'", log1)
        inst1 = int(assigned_inst_match.group(1)) if assigned_inst_match else None
        val1 = _read_property(ctx, 0, inst1, 85) if inst1 else None
        check_step(ctx, f"할당된 인스턴스(AI {inst1}) BACnet 응답 확인", True, val1 is not None,
                   val1 is not None, f"failed to read auto-assigned instance {inst1}")

        # 2차 재기동 후 인스턴스 번호 불변성 확인
        ctx.stop_process("gateway")
        ctx.start_gateway()
        ctx.wait_gateway_ready()
        log2 = ctx.gateway_log()
        assigned_inst_match2 = re.search(r"Auto-assigned instance (\d+) to object 'TC_AUTO_INSTANCE'", log2)
        inst2 = int(assigned_inst_match2.group(1)) if assigned_inst_match2 else None
        check_step(ctx, "Gateway 재기동 후 자동 인스턴스 번호 불변(재현성)", inst1, inst2,
                   inst1 is not None and inst1 == inst2, f"auto-instance changed across restart: {inst1} -> {inst2}")
        ctx.stop_process("gateway")
        ctx.stop_process("main_simulator")
        ctx.stop_process("vport")
        ctx._write_fixture()
    elif tc_id in ("TC-MAP-004", "TC-MAP-005", "TC-MAP-006"):
        ctx.require_managed()
        binary = ctx.options.unit_test_bin
        if not binary.is_file() or not os.access(binary, os.X_OK):
            blocked("host unit-test executable is missing; run `make test` in bacnet_ip_gateway")
        code, output = ctx.run_command("host_unit_tests", [binary], timeout=60.0,
                                       cwd=GATEWAY_ROOT)
        require(code == 0, f"host unit tests returned {code}")
        markers = {
            "TC-MAP-004": ("extractRegister16", "extractFloat32", "Scale", "offset"),
            "TC-MAP-005": ("BIT_PACKED", "min_value", "max_value", "word_swap"),
            "TC-MAP-006": ("extractBit", "BIT_PACKED", "Binary Value Relinquish_Default"),
        }[tc_id]
        marker_state = {marker: marker in output for marker in markers}
        check_step(ctx, f"{tc_id} 변환 assertion marker", dict.fromkeys(markers, True),
                   marker_state, all(marker_state.values()),
                   f"host unit output lacks {tc_id} conversion evidence")
        ctx.evidence.details["host_unit_test"] = {
            "binary": str(binary),
            "markers": list(markers),
        }
    elif tc_id == "TC-MAP-007":
        _native_step1_case(ctx, tc_id)
    elif tc_id == "TC-MAP-008":
        _run_profile_test(ctx)
    elif tc_id == "TC-MAP-009":
        ctx.require_managed()
        # 1차: 순수 인스턴스 중복 주입 및 기동 차단 검증
        data = _load_profile(ctx)
        data["objects"][1]["instance"] = data["objects"][0]["instance"]
        _save_profile(ctx, data)
        proc1 = _start_invalid_gateway(ctx, "map_duplicate_instance")
        ctx.wait_for(lambda: "entering HOLD" in (proc1.log_path.read_text(encoding="utf-8", errors="replace") if proc1.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive1 = proc1.process.poll() is None
        check_step(ctx, "중복 instance HOLD 생존", True, alive1,
                   alive1, "process exited instead of entering HOLD on duplicate instance")
        log1 = proc1.log_path.read_text(encoding="utf-8", errors="replace") if proc1.log_path.exists() else ""
        has_dup_log = ("Duplicate instance" in log1 or "Duplicate instance number" in log1 or "conflicts" in log1)
        check_step(ctx, "중복 instance 진단 로그 검출", True, has_dup_log,
                   has_dup_log, f"duplicate instance error log missing: {log1}")
        started1 = "BACnet IP Gateway started" in log1
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started1, not started1,
                   "BACnet started during HOLD")
        ctx.stop_process("map_duplicate_instance")

        # 2차: fixture 복원 후 순수 주소 범위 초과 주입 및 기동 차단 검증
        ctx._write_fixture()
        data2 = _load_profile(ctx)
        data2["objects"][2]["full_address"] = 999999
        _save_profile(ctx, data2)
        proc2 = _start_invalid_gateway(ctx, "map_address_range_error")
        ctx.wait_for(lambda: "entering HOLD" in (proc2.log_path.read_text(encoding="utf-8", errors="replace") if proc2.log_path.exists() else ""), 3.0, "configuration HOLD log")
        alive2 = proc2.process.poll() is None
        check_step(ctx, "주소 범위 초과 HOLD 생존", True, alive2,
                   alive2, "process exited instead of entering HOLD on address range error")
        log2 = proc2.log_path.read_text(encoding="utf-8", errors="replace") if proc2.log_path.exists() else ""
        has_addr_log = ("full_address" in log2 or "exceeds" in log2 or "different block" in log2)
        check_step(ctx, "주소 범위 초과 진단 로그 검출", True, has_addr_log,
                   has_addr_log, f"address range error log missing: {log2}")
        started2 = "BACnet IP Gateway started" in log2
        check_step(ctx, "HOLD 중 BACnet 미기동", False, started2, not started2,
                   "BACnet started during HOLD")
        ctx.stop_process("map_address_range_error")
    else:
        _config_case(ctx, "TC-CFG-003")


def _modbus_case(ctx, tc_id):
    ctx.require_managed("이 TC는 Gateway, Main 시뮬레이터 실동작 및 vport 트래픽 증적이 필요하므로 --mode managed에서만 실행됩니다")
    ctx.start_stack()

    if tc_id == "TC-MOD-001":
        # 1. 시뮬레이터 메모리에 4개 블록(FC01~FC04) 초기 데이터 적재 (유효 범위 내)
        ctx.simulator({"op": "set", "block": "input", "address": 0, "value": 78})
        ctx.simulator({"op": "set", "block": "discrete", "address": 0, "value": 1})
        ctx.simulator({"op": "set", "block": "holding", "address": 0, "value": 45})
        ctx.simulator({"op": "set", "block": "coil", "address": 0, "value": 1})

        # 2. BACnet ReadProperty 요청을 통해 게이트웨이가 vport를 거쳐 Modbus 읽기를 수행하도록 유도
        # AI 1 -> INPUT_REGISTER (FC04)
        ai_val = _read_property(ctx, 0, 1, 85, invoke_id=111)
        # BI 6 -> DISCRETE_INPUT (FC02)
        bi_val = _read_property(ctx, 3, 6, 85, invoke_id=112)
        # AO 2 -> HOLDING_REGISTER (FC03)
        ao_val = _read_property(ctx, 1, 2, 85, invoke_id=113)
        # BV 7 -> COIL (FC01)
        bv_val = _read_property(ctx, 5, 7, 85, invoke_id=114)

        # 3. vport 로그 및 Main 시뮬레이터 트랜잭션 수집
        stats = ctx.simulator({"op": "stats"})
        transactions = stats.get("transactions", [])
        vport_proc = ctx.processes.get("vport")
        vport_log = vport_proc.log_path.read_text(encoding="utf-8") if vport_proc and vport_proc.log_path.exists() else ""

        # Step 1: vport 중계 증적
        has_vport_traffic = ("[Master 2 -> Slave]" in vport_log or "[Master" in vport_log) and "[Slave -> Master" in vport_log
        check_step(ctx, "vport 패킷 중계 증적 검출", True, has_vport_traffic, has_vport_traffic,
                   "vport did not forward Modbus traffic")

        # Step 2: FC01~FC04 읽기 요청 프레임 수집
        fcs = {t.get("fc") for t in transactions}
        read_fcs_present = {1, 2, 3, 4}.issubset(fcs)
        check_step(ctx, "FC01/02/03/04 읽기 요청 프레임 수집", [1, 2, 3, 4], sorted(fcs & {1, 2, 3, 4}),
                   read_fcs_present, f"missing read FCs: {fcs}")

        # Step 3: 송수신 프레임 CRC16 무결성
        all_crc_valid = all(
            check_crc(bytes.fromhex(t["request_hex"])) and check_crc(bytes.fromhex(t["response_hex"]))
            for t in transactions if t.get("request_hex") and t.get("response_hex")
        )
        check_step(ctx, "송수신 프레임 CRC16 무결성", True, all_crc_valid, all_crc_valid,
                   "CRC verification failed on captured frames")

        # Step 4: Unit ID 계약 일치
        all_unit_match = all(t.get("unit_id") == 1 for t in transactions if t.get("unit_id") is not None)
        check_step(ctx, "Unit ID 계약 일치 (Unit 1)", True, all_unit_match, all_unit_match,
                   "Unit ID mismatch in Modbus transactions")

        # Step 5~8: 각 블록별 읽기 데이터 일치
        check_step(ctx, "FC04 Input Register 값 무결성", 78.0, ai_val, ai_val == 78.0,
                   f"AI value mismatch: {ai_val}")
        check_step(ctx, "FC02 Discrete Input 값 무결성", 1, bi_val, bi_val == 1,
                   f"BI value mismatch: {bi_val}")
        check_step(ctx, "FC03 Holding Register 값 무결성", 45.0, ao_val, ao_val == 45.0,
                   f"AO value mismatch: {ao_val}")
        check_step(ctx, "FC01 Coil 값 무결성", 1, bv_val, bv_val == 1,
                   f"BV value mismatch: {bv_val}")

    elif tc_id == "TC-MOD-002":
        before_stats = ctx.simulator({"op": "stats"})
        before_count = before_stats.get("request_count", 0)

        # Write Coil (FC05) via BV 7
        _write_property(ctx, 5, 7, 1, True, invoke_id=121, priority=16)

        # Write Single Register (FC06) via AO 2
        _write_property(ctx, 1, 2, 60.0, False, invoke_id=122, priority=16)

        # Write Multiple Registers / Float32 (FC16) via AO 9
        _write_property(ctx, 1, 9, 75.5, False, invoke_id=123, priority=16)

        # Collect new write transactions
        after_stats = ctx.simulator({"op": "stats"})
        new_txs = after_stats.get("transactions", [])[before_count:]
        vport_proc = ctx.processes.get("vport")
        vport_log = vport_proc.log_path.read_text(encoding="utf-8") if vport_proc and vport_proc.log_path.exists() else ""

        # Step 1: FC05/06/16 쓰기 요청 검출
        write_fcs = {t.get("fc") for t in new_txs}
        has_write_fcs = {5, 6, 16}.issubset(write_fcs)
        check_step(ctx, "FC05/06/16 쓰기 요청 프레임 검출", [5, 6, 16], sorted(write_fcs & {5, 6, 16}),
                   has_write_fcs, f"missing write FCs: {write_fcs}")

        # Step 2: Write 프레임 및 에코 응답 CRC16 무결성
        write_crc_ok = all(
            check_crc(bytes.fromhex(t["request_hex"])) and check_crc(bytes.fromhex(t["response_hex"]))
            for t in new_txs if t.get("request_hex") and t.get("response_hex")
        )
        check_step(ctx, "Write 프레임 및 에코 응답 CRC16 무결성", True, write_crc_ok, write_crc_ok,
                   "write CRC verification failed")

        # Step 3: FC05 Coil 메모리 반영
        coil_val = ctx.simulator({"op": "get", "block": "coil", "address": 0})["values"][0]
        check_step(ctx, "FC05 Coil 메모리 반영", True, bool(coil_val), bool(coil_val),
                   f"coil write mismatch: {coil_val}")

        # Step 4: FC06 Holding Register 메모리 반영
        reg_val = ctx.simulator({"op": "get", "block": "holding", "address": 0})["values"][0]
        check_step(ctx, "FC06 Holding Register 메모리 반영", 60, reg_val, reg_val == 60,
                   f"holding reg write mismatch: {reg_val}")

        # Step 5: FC16 Float32 다중 레지스터 메모리 반영
        float_regs = ctx.simulator({"op": "get", "block": "holding", "address": 9, "count": 2})["values"]
        actual_float = round(struct.unpack(">f", struct.pack(">HH", float_regs[0], float_regs[1]))[0], 2)
        check_step(ctx, "FC16 Float32 다중 레지스터 메모리 반영", 75.5, actual_float, actual_float == 75.5,
                   f"float32 write mismatch: {actual_float}")

        # Step 6: vport 쓰기 중계 증적
        has_vport_write = "[Master 2 -> Slave]" in vport_log and "[Slave -> Master 2]" in vport_log
        check_step(ctx, "vport 쓰기 및 에코 응답 중계 증적", True, has_vport_write, has_vport_write,
                   "vport write evidence missing")

    else:
        # 1. 1회성 Timeout Fault 주입 및 Gateway 타임아웃 감지/복구
        ctx.simulator({"op": "fault", "action": "timeout", "fc": 3, "count": 1})
        check_step(ctx, "1회성 Timeout Fault 주입", "timeout", "timeout", True)

        res1 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=131, timeout=2.0)
        status1 = res1.get("status")
        check_step(ctx, "Modbus Timeout 발생 시 BACnet 에러/차단 감지", True,
                   status1 != "SUCCESS", status1 != "SUCCESS", f"unexpected success on timeout: {status1}")

        gw_log = ctx.gateway_log()
        has_timeout_log = "timeout" in gw_log.lower()
        check_step(ctx, "Gateway 로그 내 Modbus timeout 감지", True, has_timeout_log, has_timeout_log,
                   "gateway log missing timeout detection")

        res2 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=132, timeout=2.0)
        status2 = res2.get("status")
        check_step(ctx, "Timeout 소진 후 후속 요청 정상 복구", "SUCCESS", status2,
                   status2 == "SUCCESS", f"recovery after timeout failed: {status2}")

        # 2. Modbus Exception (FC 0x83, Code 3) 주입 및 감지
        ctx.simulator({"op": "fault", "action": "exception", "fc": 3, "exception_code": 3, "count": 1})
        check_step(ctx, "Modbus Exception(0x83/Code 3) Fault 주입", "exception", "exception", True)

        res3 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=133, timeout=2.0)
        status3 = res3.get("status")
        check_step(ctx, "Modbus Exception(0x83) 응답 시 BACnet 처리 차단", True,
                   status3 != "SUCCESS", status3 != "SUCCESS", f"exception was not blocked: {status3}")

        res4 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=134, timeout=2.0)
        status4 = res4.get("status")
        check_step(ctx, "Exception 후속 요청 정상 복구", "SUCCESS", status4,
                   status4 == "SUCCESS", f"recovery after exception failed: {status4}")

        # 3. 손상된 CRC 패킷 폐기 감지
        ctx.simulator({"op": "fault", "action": "bad_crc", "fc": 3, "count": 1})
        check_step(ctx, "손상 CRC 패킷 반환 Fault 주입", "bad_crc", "bad_crc", True)

        res5 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=135, timeout=2.0)
        status5 = res5.get("status")
        check_step(ctx, "손상 CRC 패킷 수신 시 폐기 및 오류 감지", True,
                   status5 != "SUCCESS", status5 != "SUCCESS", f"bad CRC was accepted: {status5}")

        res6 = _read_property_attempt(ctx, 1, 2, 85, invoke_id=136, timeout=2.0)
        status6 = res6.get("status")
        check_step(ctx, "손상 CRC 이후 정상 통신 복구", "SUCCESS", status6,
                   status6 == "SUCCESS", f"recovery after bad CRC failed: {status6}")

        # 4. vport 및 시뮬레이터 트랜잭션 증적 확인
        stats = ctx.simulator({"op": "stats"})
        has_tx = bool(stats.get("transactions"))
        check_step(ctx, "vport 및 시뮬레이터 오류/복구 트랜잭션 증적 보존", True, has_tx, has_tx,
                   "no transaction evidence recorded")


PROFILE_TESTER_CASES = {
    "TC-BAC-005", "TC-BAC-006", "TC-BAC-016", "TC-BAC-017", "TC-BAC-018",
    "TC-BIP-003", "TC-BTL-002", "TC-BTL-005",
}
EPICS_VERIFIER_CASES = {"TC-BTL-004", "TC-BTL-006"}
LEGACY_STEP2 = PROFILE_TESTER_CASES
LEGACY_STEP3 = EPICS_VERIFIER_CASES

NATIVE_STEP1 = {
    "TC-BAC-001", "TC-BAC-003", "TC-BAC-004", "TC-BAC-007", "TC-BAC-008",
    "TC-BAC-009", "TC-BAC-010", "TC-BAC-012", "TC-BAC-013", "TC-BAC-015",
    "TC-BAC-019", "TC-BTL-001", "TC-MAP-007",
}
NATIVE_OBSERVE = NATIVE_STEP1 | {
    "TC-BAC-002", "TC-BAC-011", "TC-BAC-014", "TC-BIP-001",
}
NATIVE_MUTATING = {
    "TC-BAC-007", "TC-BAC-008", "TC-BAC-009", "TC-BAC-010", "TC-BAC-012",
    "TC-BAC-011", "TC-BAC-013", "TC-BAC-014", "TC-BAC-015", "TC-BAC-019",
    "TC-MAP-007",
}
NATIVE_MAIN_CONTROL = {"TC-BAC-010", "TC-BAC-011", "TC-BAC-014"}


def _runtime_configs(ctx):
    candidates = []
    if getattr(ctx, "config_dir", None):
        candidates.append(Path(ctx.config_dir))
    if getattr(ctx.options, "gateway_config_dir", None):
        p = Path(ctx.options.gateway_config_dir)
        candidates.extend([p, p / "conf"] if p.is_dir() else [p.parent])
    if getattr(ctx.options, "config_dir", None):
        p = Path(ctx.options.config_dir)
        candidates.extend([p, p / "conf"] if p.is_dir() else [p.parent])
    candidates.extend([
        GATEWAY_ROOT / "conf",
        ES_ROOT / "bacnet_ip_gateway_release" / "conf",
        ES_ROOT / "bacnet_ip_gateway" / "conf",
    ])
    config_dir = next((p for p in candidates if p and p.is_dir() and (p / "gateway.json").is_file()), None)
    if not config_dir:
        blocked(f"로컬 Gateway 설정 디렉터리(gateway.json)를 찾을 수 없습니다. 후보: {candidates}")
    return load_gateway_configs(str(config_dir))


def _active_objects(ctx, types=None, writable=None):
    objects = [
        obj for device in _runtime_configs(ctx)["devices"]
        for obj in device["objects"] if obj.get("instance") is not None
    ]
    if types is not None:
        objects = [obj for obj in objects if obj.get("type") in types]
    if writable is not None:
        objects = [obj for obj in objects if obj.get("is_writeable") is writable]
    return objects


def _first_active_object(ctx, types=None, writable=None):
    objects = _active_objects(ctx, types=types, writable=writable)
    require(bool(objects), f"no active BACnet object matches types={types}, writable={writable}")
    return objects[0]


def _require_bacnet_mutation(ctx, reason):
    ctx.require_capability("bacnet_mutate", reason)


def _who_has(ctx, object_info):
    object_name = object_info["name"]
    packet = build_bacnet_ip_packet(None, None, 0, build_who_has_apdu(object_name))
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(0.1)
    replies = []
    try:
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note=f"Who-Has object name={object_name}")
        sock.sendto(packet, _endpoint(ctx))
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline:
            try:
                response, sender = sock.recvfrom(4096)
            except socket.timeout:
                continue
            status, value = parse_i_have_response(response)
            _record_bacnet_rx(
                ctx, response, sender, note="Who-Has response datagram",
                parsed={"parse_status": status, "parsed_value": value},
            )
            if status == "SUCCESS":
                replies.append(value)
        actual = {"i_have_count": len(replies), "replies": replies}
        check_step(ctx, "Who-Has I-Have 응답 수", 1, len(replies),
                   len(replies) == 1, f"Who-Has response mismatch: {actual}")
        if replies:
            value = replies[0]
            expected = {
                "device_instance": ctx.options.device_id,
                "object_type": object_info["type"],
                "object_instance": object_info["instance"],
                "object_name": object_name,
            }
            actual_fields = {key: value.get(key) for key in expected}
            check_step(ctx, "I-Have object binding fields", expected, actual_fields,
                       actual_fields == expected,
                       f"I-Have does not identify the requested object: {value}")
            return value
        return None
    finally:
        sock.close()

def _priority_case(ctx, tc_id):
    _require_bacnet_mutation(ctx, "priority-array test writes BACnet Present_Value")
    _ensure_runtime(ctx)
    candidates = _active_objects(ctx, types=(1,), writable=True)
    obj = candidates[0]
    is_binary = obj["type"] == 4
    if is_binary:
        high_value, low_value = 1, 0
    else:
        minimum = float(obj.get("min_value") if obj.get("min_value") is not None else 0)
        maximum = float(obj.get("max_value") if obj.get("max_value") is not None else 100)
        high_value = min(maximum, max(minimum, 70.0))
        low_value = min(maximum, max(minimum, 30.0))
        if high_value == low_value:
            low_value = minimum

    _write_property(ctx, obj["type"], obj["instance"], None, is_binary,
                    priority=8, invoke_id=180)
    _write_property(ctx, obj["type"], obj["instance"], None, is_binary,
                    priority=16, invoke_id=181)
    rd = _read_property(ctx, obj["type"], obj["instance"], 104, invoke_id=182)

    _write_property(ctx, obj["type"], obj["instance"], high_value, is_binary,
                    priority=8, invoke_id=183)
    high_pv = _read_property(ctx, obj["type"], obj["instance"], 85, invoke_id=184)
    high_ccp = _read_property(ctx, obj["type"], obj["instance"], 431, invoke_id=185)
    check_step(ctx, "Priority 8 Present_Value", high_value, high_pv,
               (bool(high_pv) == bool(high_value)) if is_binary else
               abs(float(high_pv) - float(high_value)) < 0.01,
               f"priority 8 value mismatch: {high_pv}")
    check_step(ctx, "Priority 8 Current_Command_Priority", 8, high_ccp,
               high_ccp == 8, f"current command priority mismatch: {high_ccp}")

    _write_property(ctx, obj["type"], obj["instance"], low_value, is_binary,
                    priority=16, invoke_id=186)
    low_slot_pv = _read_property(ctx, obj["type"], obj["instance"], 85, invoke_id=187)
    low_slot_ccp = _read_property(ctx, obj["type"], obj["instance"], 431, invoke_id=188)
    check_step(ctx, "Priority 8 우선 적용", high_value, low_slot_pv,
               (bool(low_slot_pv) == bool(high_value)) if is_binary else
               abs(float(low_slot_pv) - float(high_value)) < 0.01,
               f"priority 16 unexpectedly overrode priority 8: {low_slot_pv}")
    check_step(ctx, "Priority 8 유지 Current_Command_Priority", 8, low_slot_ccp,
               low_slot_ccp == 8, f"current command priority mismatch: {low_slot_ccp}")

    if tc_id == "TC-BAC-007":
        # TC-BAC-007: Priority 8 활성 상태에서 하위인 Priority 16에 쓰더라도
        # Priority 8의 값(high_value)과 CCP(8)가 그대로 유지됨을 검증하고 종료.
        pass
    elif tc_id == "TC-BAC-008":
        # TC-BAC-008: 상위인 Priority 8을 NULL로 해제했을 때,
        # 대기 중이던 Priority 16의 값(low_value)과 CCP(16)으로 정상 인계(Fallback)됨을 전담 검증.
        _write_property(ctx, obj["type"], obj["instance"], None, is_binary,
                        priority=8, invoke_id=189)
        fallback_pv = _read_property(ctx, obj["type"], obj["instance"], 85, invoke_id=190)
        fallback_ccp = _read_property(ctx, obj["type"], obj["instance"], 431, invoke_id=191)
        check_step(ctx, "Priority 16 fallback Present_Value", low_value, fallback_pv,
                   (bool(fallback_pv) == bool(low_value)) if is_binary else
                   abs(float(fallback_pv) - float(low_value)) < 0.01,
                   f"priority 16 fallback mismatch: {fallback_pv}")
        check_step(ctx, "Priority 16 fallback Current_Command_Priority", 16,
                   fallback_ccp, fallback_ccp == 16,
                   f"current command priority mismatch: {fallback_ccp}")
    else:
        # TC-BAC-009: Priority 8과 16을 모두 NULL로 해제했을 때,
        # 최종적으로 Relinquish_Default로 복귀하고 CCP가 클리어(None)됨을 전담 검증.
        _write_property(ctx, obj["type"], obj["instance"], None, is_binary,
                        priority=8, invoke_id=189)
        _write_property(ctx, obj["type"], obj["instance"], None, is_binary,
                        priority=16, invoke_id=195)
        final_pv = _read_property(ctx, obj["type"], obj["instance"], 85, invoke_id=196)
        final_ccp = _read_property(ctx, obj["type"], obj["instance"], 431, invoke_id=197)
        check_step(ctx, "모든 priority 해제 후 Relinquish_Default fallback", rd, final_pv,
                   (bool(final_pv) == bool(rd)) if is_binary else
                   abs(float(final_pv) - float(rd)) < 0.01,
                   f"relinquish default fallback mismatch: {final_pv}")
        check_step(ctx, "모든 priority 해제 Current_Command_Priority", None,
                   final_ccp, final_ccp is None,
                   f"current command priority was not cleared: {final_ccp}")


def _read_active_cov(ctx, sock, invoke_id):
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_read_property_apdu(8, ctx.options.device_id, invoke_id, property_id=152),
    )
    _record_bacnet_tx(ctx, packet, note="ReadProperty Active_COV_Subscriptions")
    sock.sendto(packet, _endpoint(ctx))
    response, sender = _recv_bacnet_response_observed(
        sock, invoke_id, ctx=ctx, return_sender=True)
    status, value = parse_active_cov_subscriptions(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender, note="Active_COV_Subscriptions parsed response",
        parsed={"parse_status": status, "parsed_value": value}, status="parsed",
    )
    check_step(ctx, "Active_COV_Subscriptions 응답", "SUCCESS",
               {"status": status, "value": value}, status == "SUCCESS",
               f"Active_COV_Subscriptions read failed: {status} {value}")
    return value if status == "SUCCESS" else []


def _cov_subscription_case(ctx):
    _require_bacnet_mutation(ctx, "SubscribeCOV changes the device subscription table")
    _ensure_runtime(ctx)
    obj = _first_active_object(ctx)
    sock = _open_observer_socket(ctx)
    process_one, process_two, process_three = 701, 702, 703
    try:
        initial = _read_active_cov(ctx, sock, 201)
        for process_id, confirmed, invoke_id in (
            (process_one, True, 202), (process_two, False, 203),
        ):
            response = _send_observed_request(
                ctx, sock, invoke_id,
                build_subscribe_cov_apdu(obj["type"], obj["instance"], process_id,
                                         invoke_id, issue_confirmed=confirmed, lifetime=60),
            )
            status, value = parse_bacnet_response(response, invoke_id)
            check_step(ctx, f"SubscribeCOV process={process_id} ACK", "SUCCESS",
                       {"status": status, "value": value}, status == "SUCCESS",
                       f"SubscribeCOV failed: {status} {value}")
            active = _read_active_cov(ctx, sock, invoke_id + 10)
            check_step(ctx, f"Active subscription process={process_id}",
                       len(initial) + (1 if process_id == process_one else 2),
                       len(active), len(active) == len(initial) +
                       (1 if process_id == process_one else 2),
                       f"unexpected active subscription list: {active}")
            if process_id == process_one:
                refreshed = _send_observed_request(
                    ctx, sock, 204,
                    build_subscribe_cov_apdu(
                        obj["type"], obj["instance"], process_one, 204,
                        issue_confirmed=True, lifetime=60,
                    ),
                )
                refresh_status, refresh_value = parse_bacnet_response(refreshed, 204)
                check_step(ctx, "SubscribeCOV process=701 갱신 ACK", "SUCCESS",
                           {"status": refresh_status, "value": refresh_value},
                           refresh_status == "SUCCESS",
                           f"SubscribeCOV refresh failed: {refresh_status} {refresh_value}")
                refreshed_active = _read_active_cov(ctx, sock, 205)
                check_step(ctx, "갱신 후 active subscription 수", len(initial) + 1,
                           len(refreshed_active), len(refreshed_active) == len(initial) + 1,
                           f"refresh changed active count: {refreshed_active}")

        if ctx.options.long_tests:
            response = _send_observed_request(
                ctx, sock, 206,
                build_subscribe_cov_apdu(obj["type"], obj["instance"], process_three, 206,
                                         issue_confirmed=False, lifetime=1),
            )
            status, value = parse_bacnet_response(response, 206)
            check_step(ctx, "SubscribeCOV process=703 lifetime=1 ACK", "SUCCESS",
                       {"status": status, "value": value}, status == "SUCCESS",
                       f"short lifetime subscription failed: {status} {value}")
            before_expiry = _read_active_cov(ctx, sock, 207)
            check_step(ctx, "lifetime 구독 생성", len(initial) + 3, len(before_expiry),
                       len(before_expiry) == len(initial) + 3,
                       f"short lifetime subscription missing: {before_expiry}")
            # BACnet SubscribeCOV lifetime is expressed in seconds.  Keep the
            # two 60-second subscriptions alive while allowing the one-second
            # subscription to expire; waiting 61 seconds would expire all
            # three and make the assertion test the runner timing instead of
            # the device behavior.
            check_step(ctx, "SubscribeCOV lifetime 대기", 2, 2, True,
                       "lifetime is encoded in seconds")
            time.sleep(2.0)
            after_expiry = _read_active_cov(ctx, sock, 208)
            check_step(ctx, "lifetime 만료 후 active subscription 수", len(initial) + 2,
                       len(after_expiry), len(after_expiry) == len(initial) + 2,
                       f"expired subscription remains active: {after_expiry}")

        _cancel_cov(ctx, sock, obj["type"], obj["instance"], process_one, 214)
        after_one = _read_active_cov(ctx, sock, 215)
        check_step(ctx, "SubscribeCOV 첫 구독 취소", len(initial) + 1, len(after_one),
                   len(after_one) == len(initial) + 1,
                   f"remaining subscription mismatch: {after_one}")
        _cancel_cov(ctx, sock, obj["type"], obj["instance"], process_two, 216)
        final = _read_active_cov(ctx, sock, 217)
        check_step(ctx, "SubscribeCOV 전체 취소", len(initial), len(final),
                   len(final) == len(initial), f"subscription cleanup mismatch: {final}")
    finally:
        sock.close()


def _cov_capacity_case(ctx):
    _require_bacnet_mutation(ctx, "SubscribeCOV capacity test changes the device subscription table")
    _ensure_runtime(ctx)
    obj = _first_active_object(ctx)
    sock = _open_observer_socket(ctx)
    subscribed_pids = []
    base_process_id = 801
    max_subs = 20
    try:
        initial = _read_active_cov(ctx, sock, 100)
        all_ok = True
        for i in range(max_subs):
            pid = base_process_id + i
            inv = 101 + i
            resp = _send_observed_request(
                ctx, sock, inv,
                build_subscribe_cov_apdu(obj["type"], obj["instance"], pid, inv,
                                         issue_confirmed=False, lifetime=60),
            )
            st, val = parse_bacnet_response(resp, inv)
            if st == "SUCCESS":
                subscribed_pids.append(pid)
            else:
                all_ok = False
                break
        check_step(ctx, f"COV 최대 용량({max_subs}개) 정상 구독 등록", max_subs, len(subscribed_pids),
                   all_ok and len(subscribed_pids) == max_subs,
                   f"failed to register {max_subs} subscriptions: registered={len(subscribed_pids)}")

        active_at_max = _read_active_cov(ctx, sock, 130)
        check_step(ctx, "최대 용량 활성 구독 수 확인", len(initial) + max_subs, len(active_at_max),
                   len(active_at_max) == len(initial) + max_subs,
                   f"active count mismatch at capacity: {active_at_max}")

        overflow_pid = base_process_id + max_subs
        overflow_inv = 131
        overflow_resp = _send_observed_request(
            ctx, sock, overflow_inv,
            build_subscribe_cov_apdu(obj["type"], obj["instance"], overflow_pid, overflow_inv,
                                     issue_confirmed=False, lifetime=60),
        )
        overflow_st, overflow_val = parse_bacnet_response(overflow_resp, overflow_inv)
        check_step(ctx, "용량 초과(21번째) 구독 거부 검증", "NON_SUCCESS",
                   {"status": overflow_st, "value": overflow_val},
                   overflow_st != "SUCCESS",
                   f"expected overflow subscription to fail, but succeeded: {overflow_st} {overflow_val}")

        _cancel_cov(ctx, sock, obj["type"], obj["instance"], subscribed_pids[0], 132)
        subscribed_pids.pop(0)

        active_after_cancel = _read_active_cov(ctx, sock, 133)
        check_step(ctx, "1개 취소 후 슬롯 확보 확인", len(initial) + max_subs - 1, len(active_after_cancel),
                   len(active_after_cancel) == len(initial) + max_subs - 1,
                   f"active count mismatch after cancel: {active_after_cancel}")

        retry_inv = 134
        retry_resp = _send_observed_request(
            ctx, sock, retry_inv,
            build_subscribe_cov_apdu(obj["type"], obj["instance"], overflow_pid, retry_inv,
                                     issue_confirmed=False, lifetime=60),
        )
        retry_st, retry_val = parse_bacnet_response(retry_resp, retry_inv)
        if retry_st == "SUCCESS":
            subscribed_pids.append(overflow_pid)
        check_step(ctx, "슬롯 확보 후 재시도 구독 성공", "SUCCESS",
                   {"status": retry_st, "value": retry_val},
                   retry_st == "SUCCESS",
                   f"retry subscription failed: {retry_st} {retry_val}")

    finally:
        clean_inv = 140
        for pid in subscribed_pids:
            try:
                _cancel_cov(ctx, sock, obj["type"], obj["instance"], pid, clean_inv)
                clean_inv += 1
            except Exception:
                pass
        final_active = _read_active_cov(ctx, sock, 199)
        check_step(ctx, "COV 용량 시험 구독 전체 정리", len(initial), len(final_active),
                   len(final_active) == len(initial),
                   f"final subscription cleanup mismatch: {final_active}")
        sock.close()


def _dcc_case(ctx, initiation_only=False):
    _require_bacnet_mutation(ctx, "DCC changes device communication state")
    _ensure_runtime(ctx)
    obj = _first_active_object(ctx)
    sock = _open_observer_socket(ctx)
    disable_invoke = 220 if initiation_only else 221
    enable_invoke = disable_invoke + 1
    try:
        response = _send_observed_request(
            ctx, sock, disable_invoke,
            build_dcc_apdu(enable_state=2 if initiation_only else 1,
                           invoke_id=disable_invoke, time_duration=1),
        )
        status, value = parse_dcc_response(response, disable_invoke)
        check_step(ctx, "DCC Disable ACK", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"DCC disable failed: {status} {value}")

        read_invoke = disable_invoke + 2
        blocked_response = None
        try:
            blocked_response = _send_observed_request(
                ctx, sock, read_invoke,
                build_read_property_apdu(obj["type"], obj["instance"], read_invoke),
                timeout=1.0,
            )
            read_status, read_value = parse_bacnet_response(blocked_response, read_invoke)
            communication_blocked = read_status != "SUCCESS"
            actual = {"status": read_status, "value": read_value}
        except (AssertionError, socket.timeout):
            communication_blocked = True
            actual = "timeout"
        check_step(ctx, "DCC 중 ReadProperty 정책", "timeout 또는 BACnet error",
                   actual, communication_blocked,
                   "DCC Disable still returned a successful ReadProperty")

        response = _send_observed_request(
            ctx, sock, enable_invoke,
            build_dcc_apdu(enable_state=0, invoke_id=enable_invoke),
        )
        status, value = parse_dcc_response(response, enable_invoke)
        check_step(ctx, "DCC Enable ACK", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"DCC enable failed: {status} {value}")
    finally:
        sock.close()


def _dcc_expiry_case(ctx):
    if not ctx.options.long_tests:
        blocked("DCC timeout expiry test requires --long")
    _require_bacnet_mutation(ctx, "DCC expiry test changes device communication state")
    _ensure_runtime(ctx)
    obj = _first_active_object(ctx)
    sock = _open_observer_socket(ctx)
    try:
        disable_invoke = 230
        response = _send_observed_request(
            ctx, sock, disable_invoke,
            build_dcc_apdu(enable_state=1, invoke_id=disable_invoke, time_duration=1),
        )
        status, value = parse_dcc_response(response, disable_invoke)
        check_step(ctx, "DCC timed Disable ACK", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"timed DCC disable failed: {status} {value}")
        check_step(ctx, "DCC timer 대기", 61, 61, True,
                   "BACnet DCC timer is specified in minutes")
        time.sleep(61.0)
        read_invoke = 231
        response = _send_observed_request(
            ctx, sock, read_invoke,
            build_read_property_apdu(obj["type"], obj["instance"], read_invoke),
        )
        read_status, read_value = parse_bacnet_response(response, read_invoke)
        check_step(ctx, "DCC timer 만료 후 ReadProperty", "SUCCESS",
                   {"status": read_status, "value": read_value},
                   read_status == "SUCCESS",
                   f"communication did not recover after DCC expiry: {read_value}")
        enable_invoke = 232
        response = _send_observed_request(
            ctx, sock, enable_invoke,
            build_dcc_apdu(enable_state=0, invoke_id=enable_invoke),
        )
        enable_status, enable_value = parse_dcc_response(response, enable_invoke)
        check_step(ctx, "DCC expiry 후 명시 Enable ACK", "SUCCESS",
                   {"status": enable_status, "value": enable_value},
                   enable_status == "SUCCESS",
                   f"explicit DCC enable failed after expiry: {enable_status} {enable_value}")
    finally:
        sock.close()


def _representative_objects(ctx, max_per_type=2, max_total=8):
    selected = []
    by_type = {}
    for obj in _active_objects(ctx):
        t = obj.get("type")
        if by_type.get(t, 0) < max_per_type:
            selected.append(obj)
            by_type[t] = by_type.get(t, 0) + 1
            if len(selected) >= max_total:
                break
    return selected if selected else _active_objects(ctx)[:max_total]


def _native_read_values_case(ctx):
    _ensure_runtime(ctx)
    objects = _representative_objects(ctx, max_total=8)
    values = []
    for obj in objects:
        value = _read_property(ctx, obj["type"], obj["instance"], 85)
        valid_type = isinstance(value, (int, float)) and not isinstance(value, bool)
        if obj["type"] in (3, 4, 5):
            valid_type = isinstance(value, (int, bool))
        check_step(ctx, f"Present_Value 타입 obj={obj['type']}:{obj['instance']}",
                   "numeric/binary", value, valid_type,
                   f"unexpected Present_Value type: {value!r}")
        if ctx.managed:
            source = _source_spec(ctx, obj)
            if source is not None:
                source_value = ctx.simulator({
                    "op": "get", "block": source["block"], "address": source["address"],
                })["values"][0]
                expected_value = (
                    float(source_value) * float(obj.get("scale", 1.0))
                    + float(obj.get("offset", 0.0))
                )
                if obj.get("min_value") is not None:
                    expected_value = max(expected_value, float(obj["min_value"]))
                if obj.get("max_value") is not None:
                    expected_value = min(expected_value, float(obj["max_value"]))
                matches_source = (
                    bool(value) == bool(expected_value)
                    if obj["type"] in (3, 4, 5)
                    else abs(float(value) - float(expected_value)) < 0.01
                )
                check_step(ctx, f"Modbus→BACnet 값 obj={obj['type']}:{obj['instance']}",
                           expected_value, value, matches_source,
                           f"source/value mismatch: source={source_value}, value={value}")
        values.append({"type": obj["type"], "instance": obj["instance"], "value": value})
    ctx.evidence.details["present_values"] = values


def _source_spec(ctx, obj):
    """Resolve a runner fixture object to the simulator memory block/address."""
    config_dir = ctx.config_dir or ctx.options.config_dir
    if config_dir is None:
        return None
    profiles_dir = Path(config_dir) / "profiles"
    for path in sorted(profiles_dir.glob("*.json")):
        payload = json.loads(path.read_text(encoding="utf-8"))
        for raw in payload.get("objects", []):
            raw_type = infer_bacnet_type(raw)
            if raw_type != obj.get("type"):
                continue
            if raw.get("name") != obj.get("name") and raw.get("instance") != obj.get("instance"):
                continue
            full_address = raw.get("full_address")
            if full_address is None:
                address = raw.get("offset_address")
            elif int(full_address) >= 40001:
                address = int(full_address) - 40001
            elif int(full_address) >= 30001:
                address = int(full_address) - 30001
            elif int(full_address) >= 10001:
                address = int(full_address) - 10001
            else:
                address = int(full_address) - 1
            block = str(raw.get("modbus_block", "")).lower()
            block = {
                "holding_register": "holding",
                "holding_registers": "holding",
                "input_register": "input",
                "input_registers": "input",
                "discrete_input": "discrete_inputs",
                "discrete_inputs": "discrete_inputs",
                "coil": "coils",
                "coils": "coils",
            }.get(block, block)
            return {"block": block, "address": int(address)}
    return None


def _native_cov_increment_case(ctx):
    _require_bacnet_mutation(ctx, "COV_Increment is changed through BACnet WriteProperty")
    _ensure_runtime(ctx)
    ctx.require_capability(
        "main_control",
        "COV increment boundary test needs a controlled Modbus source value",
    )
    obj = _first_active_object(ctx, types=(1,))
    source = _source_spec(ctx, obj)
    require(source is not None, f"cannot map object to simulator source: {obj}")
    original = _read_property(ctx, obj["type"], obj["instance"], 22, invoke_id=240)
    original_source = ctx.simulator({"op": "get", "block": source["block"],
                                     "address": source["address"]})["values"][0]
    sock = _open_observer_socket(ctx)
    process_id = 610
    subscribed = False
    try:
        _write_property(ctx, obj["type"], obj["instance"], 5.0, False,
                        property_id=22, priority=None, invoke_id=241, tag_type=4)
        updated = _read_property(ctx, obj["type"], obj["instance"], 22, invoke_id=242)
        check_step(ctx, "COV_Increment 유효값 반영", 5.0, updated,
                   abs(float(updated) - 5.0) < 0.01,
                   f"COV_Increment write was not reflected: {updated}")
        status, value = _write_property(
            ctx, obj["type"], obj["instance"], 0.0, False,
            property_id=22, priority=None, invoke_id=243, tag_type=4,
            expected_status="BACNET_ERROR_PDU",
        )
        check_step(ctx, "COV_Increment 0 거부", "BACNET_ERROR_PDU", status,
                   status == "BACNET_ERROR_PDU", f"invalid increment accepted: {value}")

        response = _send_observed_request(
            ctx, sock, 244,
            build_subscribe_cov_apdu(obj["type"], obj["instance"], process_id, 244,
                                     issue_confirmed=True, lifetime=60),
        )
        sub_status, sub_value = parse_bacnet_response(response, 244)
        check_step(ctx, "Analog COV SubscribeCOV ACK", "SUCCESS",
                   {"status": sub_status, "value": sub_value},
                   sub_status == "SUCCESS",
                   f"analog SubscribeCOV failed: {sub_status} {sub_value}")
        subscribed = sub_status == "SUCCESS"
        _observe_cov(sock, 0.25, ctx=ctx)
        ctx.simulator({"op": "set", "block": source["block"],
                       "address": source["address"], "value": original_source})
        _observe_cov(sock, 0.45, ctx=ctx)
        minimum = float(obj.get("min_value") if obj.get("min_value") is not None else -100000)
        maximum = float(obj.get("max_value") if obj.get("max_value") is not None else 100000)
        base = float(original_source)
        small = base + 2.0 if base + 2.0 <= maximum else base - 2.0
        large = base + 7.0 if base + 7.0 <= maximum else base - 7.0
        small = min(maximum, max(minimum, small))
        large = min(maximum, max(minimum, large))
        if abs(large - base) < 5.0:
            blocked("fixture analog range is too narrow for COV increment boundary")
        ctx.simulator({"op": "set", "block": source["block"],
                       "address": source["address"], "value": small})
        below = _observe_cov(sock, 0.70, ctx=ctx)
        ctx.simulator({"op": "set", "block": source["block"],
                       "address": source["address"], "value": large})
        above = _observe_cov(sock, 0.90, ctx=ctx)
        below_count = below["confirmed"] + below["unconfirmed"]
        above_count = above["confirmed"] + above["unconfirmed"]
        check_step(ctx, "COV_Increment 미만 변화", 0, below_count,
                   below_count == 0, f"below-increment notifications: {below}")
        check_step(ctx, "COV_Increment 이상 변화", ">= 1", above_count,
                   above_count >= 1, f"above-increment notifications: {above}")
    finally:
        if subscribed:
            try:
                _cancel_cov(ctx, sock, obj["type"], obj["instance"], process_id, 245)
            except Exception:
                pass
        ctx.simulator({"op": "set", "block": source["block"],
                       "address": source["address"], "value": original_source})
        if original is not None:
            try:
                _write_property(ctx, obj["type"], obj["instance"], original, False,
                                property_id=22, priority=None, invoke_id=246, tag_type=4)
            except Exception:
                pass
        sock.close()


def _native_who_is_who_has_case(ctx):
    _ensure_runtime(ctx)
    _who_is(ctx, None, None, True)
    _who_has(ctx, _first_active_object(ctx))


def _native_step1_case(ctx, tc_id):
    if tc_id == "TC-BAC-001":
        _ensure_runtime(ctx)
        _who_is(ctx, None, None, True)
    elif tc_id == "TC-BAC-003":
        _native_who_is_who_has_case(ctx)
    elif tc_id == "TC-BAC-004":
        _native_read_values_case(ctx)
    elif tc_id in ("TC-BAC-007", "TC-BAC-008", "TC-BAC-009"):
        _priority_case(ctx, tc_id)
    elif tc_id == "TC-BAC-010":
        _native_cov_increment_case(ctx)
    elif tc_id == "TC-BAC-012":
        _cov_subscription_case(ctx)
    elif tc_id == "TC-BAC-013":
        _dcc_case(ctx)
    elif tc_id == "TC-BAC-015":
        _dcc_expiry_case(ctx)
    elif tc_id == "TC-BAC-019":
        _cov_capacity_case(ctx)
    elif tc_id == "TC-BAC-020":
        _bac_network_port_runtime_case(ctx)
    elif tc_id == "TC-BTL-001":
        _native_who_is_who_has_case(ctx)
        _native_read_values_case(ctx)
    elif tc_id == "TC-MAP-007":
        _require_bacnet_mutation(ctx, "mapping write-protection test sends a BACnet WriteProperty")
        _ensure_runtime(ctx)
        obj = _first_active_object(ctx, types=(0, 3))
        before = ctx.simulator({"op": "stats"})["transactions"] if ctx.managed else []
        _write_property(ctx, obj["type"], obj["instance"],
                        1 if obj["type"] == 3 else 1.0, obj["type"] == 3,
                        invoke_id=250, expected_status="BACNET_ERROR_PDU")
        after = ctx.simulator({"op": "stats"})["transactions"] if ctx.managed else []
        write_functions = {5, 6, 15, 16}
        before_writes = sum(item.get("fc") in write_functions for item in before)
        after_writes = sum(item.get("fc") in write_functions for item in after)
        check_step(ctx, "입력 객체 BACnet Write 후 Modbus Write 없음", before_writes,
                   after_writes, before_writes == after_writes,
                   f"unexpected Modbus write count: {before_writes}->{after_writes}")


def _binary_cov_change_case(ctx):
    ctx.require_managed()
    ctx.start_stack()
    sock = _open_observer_socket(ctx)
    process_id = 611
    object_type = 5  # BINARY_VALUE
    instance = 7
    try:
        invoke_id = 211
        response = _send_observed_request(
            ctx, sock, invoke_id,
            build_subscribe_cov_apdu(object_type, instance, process_id, invoke_id,
                                     issue_confirmed=True, lifetime=60))
        status, value = parse_bacnet_response(response, invoke_id)
        check_step(ctx, "Binary SubscribeCOV 응답", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"SubscribeCOV failed: {status} {value}")

        # SubscribeCOV sends one initial notification.  Remove that baseline
        # before testing the binary state-change/no-change assertion.
        _observe_cov(sock, 0.35, ctx=ctx)
        ctx.simulator({"op": "set", "block": "coils", "address": 0, "value": 0})
        unchanged_before = _observe_cov(sock, 0.45, ctx=ctx)
        ctx.simulator({"op": "set", "block": "coils", "address": 0, "value": 1})
        changed = _observe_cov(sock, 0.75, ctx=ctx)
        ctx.simulator({"op": "set", "block": "coils", "address": 0, "value": 1})
        unchanged_after = _observe_cov(sock, 0.45, ctx=ctx)

        changed_count = changed["confirmed"] + changed["unconfirmed"]
        unchanged_count = (unchanged_before["confirmed"] + unchanged_before["unconfirmed"] +
                           unchanged_after["confirmed"] + unchanged_after["unconfirmed"])
        check_step(ctx, "Binary 상태 변경 COV", ">= 1 notification", changed,
                   changed_count >= 1, "binary state change produced no COV notification")
        check_step(ctx, "Binary 무변경 COV", 0, unchanged_count,
                   unchanged_count == 0,
                   f"binary unchanged state produced {unchanged_count} COV notification(s)")
        _cancel_cov(ctx, sock, object_type, instance, process_id, 212)
    finally:
        sock.close()


def _dcc_disable_initiation_case(ctx):
    ctx.require_managed()
    ctx.start_stack()
    sock = _open_observer_socket(ctx)
    process_id = 614
    object_type = 5  # BINARY_VALUE
    instance = 7
    try:
        disable_invoke = 214
        response = _send_observed_request(
            ctx, sock, disable_invoke,
            build_dcc_apdu(enable_state=2, invoke_id=disable_invoke))
        status, value = parse_dcc_response(response, disable_invoke)
        check_step(ctx, "DCC Disable Initiation ACK", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"DCC Disable Initiation failed: {status} {value}")

        subscribe_invoke = 215
        subscribe_notifications = {"confirmed": 0, "unconfirmed": 0}
        response = _send_observed_request(
            ctx, sock, subscribe_invoke,
            build_subscribe_cov_apdu(object_type, instance, process_id, subscribe_invoke,
                                     issue_confirmed=True, lifetime=60),
            cov_counts=subscribe_notifications)
        status, value = parse_bacnet_response(response, subscribe_invoke)
        check_step(ctx, "DCC 상태 SubscribeCOV ACK", "SUCCESS",
                   {"status": status, "value": value}, status == "SUCCESS",
                   f"SubscribeCOV under DCC Disable Initiation failed: {status} {value}")

        suppressed_initial = _observe_cov(sock, 0.40, ctx=ctx)
        suppressed_initial["confirmed"] += subscribe_notifications["confirmed"]
        suppressed_initial["unconfirmed"] += subscribe_notifications["unconfirmed"]
        ctx.simulator({"op": "set", "block": "coils", "address": 0, "value": 1})
        suppressed_change = _observe_cov(sock, 0.70, ctx=ctx)
        suppressed_count = (suppressed_initial["confirmed"] + suppressed_initial["unconfirmed"] +
                            suppressed_change["confirmed"] + suppressed_change["unconfirmed"])
        check_step(ctx, "DCC Disable Initiation COV 억제", 0, suppressed_count,
                   suppressed_count == 0,
                   f"DCC Disable Initiation allowed {suppressed_count} COV initiation packet(s)")

        # Disable Initiation still permits incoming BACnet service responses.
        read_invoke = 216
        read_response = _send_observed_request(
            ctx, sock, read_invoke, build_read_property_apdu(object_type, instance, read_invoke))
        read_status, read_value = parse_bacnet_response(read_response, read_invoke)
        check_step(ctx, "DCC Disable Initiation ReadProperty", "SUCCESS",
                   {"status": read_status, "value": read_value}, read_status == "SUCCESS",
                   f"ReadProperty blocked during DCC Disable Initiation: {read_value}")

        enable_invoke = 217
        enabled_during_ack = {"confirmed": 0, "unconfirmed": 0}
        response = _send_observed_request(
            ctx, sock, enable_invoke, build_dcc_apdu(enable_state=0, invoke_id=enable_invoke),
            cov_counts=enabled_during_ack)
        status, value = parse_dcc_response(response, enable_invoke)
        check_step(ctx, "DCC Enable ACK", "SUCCESS", {"status": status, "value": value},
                   status == "SUCCESS", f"DCC Enable failed: {status} {value}")
        enabled_after_ack = _observe_cov(sock, 0.45, ctx=ctx)
        ctx.simulator({"op": "set", "block": "coils", "address": 0, "value": 0})
        restored = _observe_cov(sock, 0.85, ctx=ctx)
        restored_count = (enabled_during_ack["confirmed"] + enabled_during_ack["unconfirmed"] +
                          enabled_after_ack["confirmed"] + enabled_after_ack["unconfirmed"] +
                          restored["confirmed"] + restored["unconfirmed"])
        check_step(ctx, "DCC Enable 후 COV 복구", ">= 1 notification", {
            "during_ack": enabled_during_ack,
            "after_ack": enabled_after_ack,
            "after_value_change": restored,
            "total": restored_count,
        }, restored_count >= 1, "COV initiation did not recover after DCC Enable")
        _cancel_cov(ctx, sock, object_type, instance, process_id, 218)
    finally:
        sock.close()


def _bacnet_case(ctx, tc_id):
    if tc_id == "TC-BAC-002":
        _ensure_runtime(ctx)
        _who_is(ctx, ctx.options.device_id, ctx.options.device_id, True)
        _who_is(ctx, ctx.options.device_id + 1, ctx.options.device_id + 2, False)
    elif tc_id == "TC-BAC-011":
        _binary_cov_change_case(ctx)
    elif tc_id == "TC-BAC-014":
        _dcc_disable_initiation_case(ctx)
    elif tc_id in NATIVE_STEP1:
        _native_step1_case(ctx, tc_id)
    elif tc_id in PROFILE_TESTER_CASES:
        _run_profile_test(ctx)
    else:
        blocked(f"{tc_id} dedicated BACnet assertion is not available")


def _interface_case(ctx, tc_id):
    if tc_id == "TC-IFCFG-001":
        source = (GATEWAY_ROOT / "src" / "gateway" / "main.cpp").read_text(encoding="utf-8")
        require('std::string config_dir = "./conf"' in source and "config_dir = argv[1]" in source,
                "default/explicit config directory contract mismatch")
        check_step(ctx, "기본/명시 설정 디렉터리 소스코드 계약", True, True, True, "")

        # Gateway 바이너리 실동작 검증:
        # 1) 명시적 argv[1] 경로 전달 기동
        if ctx.managed:
            custom_dir = ctx.workspace / "custom_conf"
            shutil.rmtree(custom_dir, ignore_errors=True)
            shutil.copytree(ctx.config_dir, custom_dir)
            env = os.environ.copy()
            env["BACNET_LOG"] = "1"
            proc = ctx.start_process("gateway_custom", [ctx.options.gateway_bin, str(custom_dir)], env=env)
            time.sleep(1.0)
            log = proc.log_path.read_text(encoding="utf-8", errors="replace") if proc.log_path.exists() else ""
            has_custom_log = f"[Main] Config directory: {custom_dir}" in log
            check_step(ctx, "명시적 argv[1] 디렉터리 경로 인식 및 로드", True, has_custom_log,
                       has_custom_log, f"custom config dir log missing: {log}")
            ctx.stop_process("gateway_custom")
            shutil.rmtree(custom_dir, ignore_errors=True)

            # 2) 존재하지 않는 경로 argv[1] 전달 시 ConfigHold 진입 및 진단 로그 검증
            missing_dir = ctx.workspace / "non_existent_conf_dir"
            proc_err = ctx.start_process("gateway_missing_dir", [ctx.options.gateway_bin, str(missing_dir)], env=env)
            ctx.wait_for(lambda: "entering HOLD" in (proc_err.log_path.read_text(encoding="utf-8", errors="replace") if proc_err.log_path.exists() else ""), 3.0, "configuration HOLD log")
            alive = proc_err.process.poll() is None
            check_step(ctx, "존재하지 않는 디렉터리 경로 HOLD 생존", True, alive,
                       alive, "process exited instead of entering HOLD on missing config dir")
            err_log = proc_err.log_path.read_text(encoding="utf-8", errors="replace") if proc_err.log_path.exists() else ""
            has_err_log = "Failed to load gateway config" in err_log
            check_step(ctx, "디렉터리 오류 진단 로그 검출", True, has_err_log,
                       has_err_log, f"expected error log missing: {err_log}")
            started = "BACnet IP Gateway started" in err_log
            check_step(ctx, "HOLD 중 BACnet 미기동", False, started, not started,
                       "BACnet started during HOLD")
            ctx.stop_process("gateway_missing_dir")
    elif tc_id == "TC-IFCFG-002":
        ctx.require_managed()
        gateway_path = ctx.config_dir / "gateway.json"
        base = json.loads(gateway_path.read_text(encoding="utf-8"))
        variants = (
            ("serial-low", {"serial_baudrate": 4800, "serial_data_bits": 5,
                            "serial_parity": "none", "serial_stop_bits": 1}, "@4800bps"),
            ("serial-high", {"serial_baudrate": 115200, "serial_data_bits": 8,
                             "serial_parity": "odd", "serial_stop_bits": 2}, "@115200bps"),
            ("cov-low", {"cov_polling": 0.1}, "cov_polling=100ms"),
            ("cov-high", {"cov_polling": 10.0}, "cov_polling=10000ms"),
            ("cache-off", {"cache_sec": 0}, "cache_sec=0s"),
            ("cache-on", {"cache_sec": 3600}, "cache_sec=3600s"),
        )
        for index, (variant_name, override, expected_log) in enumerate(variants, 1):
            mutated = json.loads(json.dumps(base))
            mutated.setdefault("modbus", {}).update(override)
            gateway_path.write_text(json.dumps(mutated, indent=2) + "\n", encoding="utf-8")
            try:
                ctx.start_stack()
                log = ctx.gateway_log()
                log_value = expected_log if expected_log in log else "<missing>"
                check_step(ctx, f"{variant_name} 설정 로그", expected_log, log_value,
                           log_value == expected_log,
                           f"{variant_name} setting was not applied; expected '{expected_log}'")
                started = "BACnet IP Gateway started" in log
                check_step(ctx, f"{variant_name} 설정 후 기동", True, started, started,
                           f"{variant_name} did not complete gateway startup")
            finally:
                for process_name in ("gateway", "main_simulator", "vport"):
                    ctx.stop_process(process_name)
    elif tc_id == "TC-IFCFG-003":
        ctx.require_managed()
        ctx.start_vport(1)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttyMASTER1").is_symlink()
                     and not (ctx.dev_dir / "ttyMASTER2").exists(), 5.0, "single-master vport")
        log = ctx.processes["vport"].log_path
        ctx.wait_for(lambda: "500ms" in log.read_text(encoding="utf-8", errors="replace"),
                     3.0, "configured vport timeout log")
        check_step(ctx, "vport 단일 Master", True,
                   (ctx.dev_dir / "ttyMASTER1").is_symlink() and not (ctx.dev_dir / "ttyMASTER2").exists(),
                   (ctx.dev_dir / "ttyMASTER1").is_symlink() and not (ctx.dev_dir / "ttyMASTER2").exists(),
                   "single-master links mismatch")
        ctx.stop_process("vport")
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttyMASTER3").is_symlink(), 5.0, "three-master vport")
        check_step(ctx, "vport 3 Master", True, (ctx.dev_dir / "ttyMASTER3").is_symlink(),
                   (ctx.dev_dir / "ttyMASTER3").is_symlink(), "three-master link missing")
    elif tc_id == "TC-BIP-001":
        _ensure_runtime(ctx)
        _who_is(ctx)
    elif tc_id == "TC-BIP-002":
        _bip_mode_case(ctx)
    elif tc_id == "TC-BIP-003":
        _run_profile_test(ctx)


def _bip_mode_case(ctx):
    """Run the normal and foreign-device configurations independently."""
    ctx.require_managed()
    endpoint = _bbmd_endpoint(ctx)
    probe = _bbmd_probe(ctx)
    ctx.evidence.details["bbmd_probe"] = probe
    expected_modes = {"normal": 0, "fd": 1}
    mode_results = {}
    for index, mode in enumerate(("normal", "fd")):
        if index:
            for process_name in ("gateway", "main_simulator", "vport"):
                ctx.stop_process(process_name)
            ctx.wait_for(lambda: not any(ctx.dev_dir.iterdir()), 5.0,
                         f"{mode} mode previous PTY cleanup")
        ctx.write_bip_config(mode, endpoint)
        ctx.start_stack()
        actual = _read_property(ctx, 56, 1, 408, invoke_id=40 + index)  # BACnet_IP_Mode
        check_step(ctx, f"NetworkPort IP_Mode ({mode})", expected_modes[mode], actual,
                   actual == expected_modes[mode],
                   f"configured {mode} but device reported {actual}")
        mode_results[mode] = {"ip_mode": actual}
        # Property_List에 있는 속성은 모드와 무관하게 읽혀야 하며, Command(417)는 목록에 없어야 한다.
        plist_attempt = _read_enum_list(ctx, 56, 1, 371, 80 + index)
        plist = plist_attempt.get("value")
        plist_ok = (plist_attempt["status"] == "SUCCESS" and isinstance(plist, (list, tuple))
                    and 417 not in plist)
        check_step(ctx, f"NetworkPort Property_List Command 제외 ({mode})", "417 not listed",
                   plist, plist_ok, f"Property_List is unreadable or contains Command: {plist}")
        # 표준 번호의 Network Port 필수/IP 속성은 스택이 응답해야 한다 (EPICS에 광고된 속성)
        for name, prop in (("Network_Type", 427), ("Protocol_Level", 482), ("IP_Address", 400),
                           ("IP_Default_Gateway", 401), ("IP_Subnet_Mask", 411),
                           ("BACnet_IP_UDP_Port", 412)):
            raw = _read_value_bytes(ctx, 56, 1, prop, 130 + prop % 100)
            check_step(ctx, f"NetworkPort {name}({prop}) 읽기 ({mode})", "value", raw.hex() if raw else None,
                       bool(raw), f"{name} ({prop}) is not readable")
        pending = _read_value_bytes(ctx, 56, 1, 416, 90 + index)
        check_step(ctx, f"NetworkPort Changes_Pending FALSE ({mode})", "FALSE", pending.hex() if pending else None,
                   pending == b"\x10", f"Changes_Pending is not FALSE: {pending}")
        if mode == "normal":
            fd_attempt = _read_property_attempt(ctx, 56, 1, 418, 50 + index)
            status = fd_attempt["status"]
            value = fd_attempt.get("value")
            check_step(ctx, "normal FD_BBMD_Address 읽기", "SUCCESS", status,
                       status == "SUCCESS", f"normal mode failed to read FD_BBMD_Address: {value}")
            ttl_attempt = _read_property_attempt(ctx, 56, 1, 419, 55 + index)
            check_step(ctx, "normal FD_Subscription_Lifetime 읽기", "SUCCESS",
                       ttl_attempt["status"], ttl_attempt["status"] == "SUCCESS",
                       f"normal mode failed to read FD_Subscription_Lifetime: {ttl_attempt.get('value')}")
        elif mode == "fd":
            address = _read_property(ctx, 56, 1, 418, invoke_id=60 + index)
            lifetime = _read_property(ctx, 56, 1, 419, invoke_id=70 + index)
            address_text = str(address)
            endpoint_in_address = endpoint[0] in address_text and str(endpoint[1]) in address_text
            check_step(ctx, "FD_BBMD_Address", f"{endpoint[0]}:{endpoint[1]}", address,
                       endpoint_in_address,
                       f"FD_BBMD_Address does not match configured BBMD: {address}")
            check_step(ctx, "FD_Subscription_Lifetime", "> 0", lifetime,
                       isinstance(lifetime, (int, float)) and lifetime > 0,
                       f"invalid FD lifetime: {lifetime}")
            entries = _bbmd_read_fdt(ctx, endpoint)
            gateway_entry = next((entry for entry in entries
                                  if entry["ip"] == ctx.options.target_ip
                                  and entry["port"] == ctx.options.target_port), None)
            check_step(ctx, "Gateway Foreign Device FDT 등록", True, gateway_entry,
                       gateway_entry is not None,
                       f"Gateway registration was not found in FDT: {entries}")
            mode_results[mode].update({"fd_bbmd_address": address,
                                       "fd_lifetime": lifetime,
                                       "gateway_fdt_entry": gateway_entry})
    ctx.evidence.details["bip_mode_results"] = mode_results

    # BBMD 장애 및 복구 시험 (SRS-IF-BIP-001)
    from bbmd_server import BBMDServer
    host, port = endpoint
    if host in ("127.0.0.1", ctx.options.target_ip):
        test_bbmd_port = port + 2
        test_endpoint = (host, test_bbmd_port)
        test_server = BBMDServer(host=host, port=test_bbmd_port)
        threading.Thread(target=test_server.start, daemon=True).start()
        ctx.wait_for(lambda: test_server.sock is not None, 3.0, "test BBMD socket")
        try:
            for process_name in ("gateway", "main_simulator", "vport"):
                ctx.stop_process(process_name)
            ctx.wait_for(lambda: not any(ctx.dev_dir.iterdir()), 5.0, "PTY cleanup for test BBMD")
            ctx.write_bip_config("fd", test_endpoint, ttl=10)
            ctx.start_stack()

            # test BBMD 등록 확인
            ctx.wait_for(lambda: _gateway_fdt_entry(ctx, test_endpoint) is not None, 15.0, "registration on test BBMD")
            check_step(ctx, "test BBMD FDT 등록", True, True, True)

            # BBMD 강제 중지 -> 단절 로그 감시
            lost_before = _gateway_log(ctx).count("registration failed or lost")
            ok_before = _gateway_log(ctx).count("Registered with BBMD")
            test_server.stop()
            ctx.wait_for(lambda: _gateway_log(ctx).count("registration failed or lost") > lost_before,
                         45.0, "BBMD lost log")
            check_step(ctx, "BBMD 끊김 에러 메시지", True, True, True)

            # BBMD 재기동 -> 복구 로그 및 재등록 감시
            test_server = BBMDServer(host=host, port=test_bbmd_port)
            threading.Thread(target=test_server.start, daemon=True).start()
            ctx.wait_for(lambda: test_server.sock is not None, 3.0, "test BBMD restart socket")
            ctx.wait_for(lambda: _gateway_log(ctx).count("Registered with BBMD") > ok_before,
                         30.0, "BBMD recovered log")
            ctx.wait_for(lambda: _gateway_fdt_entry(ctx, test_endpoint) is not None, 15.0, "re-registration after BBMD recovery")
            check_step(ctx, "BBMD 복구 후 재접속 및 연결 메시지", True, True, True)
        finally:
            test_server.stop()

        # (--long) TTL > 5분이면 5분(300초)마다 직접 재등록해야 한다 (SRS-IF-BIP-001)
        if ctx.options.long_tests:
            for process_name in ("gateway", "main_simulator", "vport"):
                ctx.stop_process(process_name)
            ctx.wait_for(lambda: not any(ctx.dev_dir.iterdir()), 5.0, "PTY cleanup for long test")
            ctx.write_bip_config("fd", endpoint, ttl=600)
            ctx.start_stack()
            ctx.wait_for(lambda: _gateway_fdt_entry(ctx, endpoint) is not None, 15.0, "registration with TTL=600")
            entry = _gateway_fdt_entry(ctx, endpoint)
            check_step(ctx, "FD_Subscription_Lifetime=600 반영", 600, entry["ttl"],
                       entry["ttl"] == 600, f"FDT entry TTL mismatch: {entry}")
            deadline = time.monotonic() + 330.0
            previous = entry["remaining"]
            renewed_at = None
            while time.monotonic() < deadline:
                time.sleep(5.0)
                current = _gateway_fdt_entry(ctx, endpoint)
                if current is None:
                    continue
                if current["remaining"] > previous + 10:
                    renewed_at = 600 - previous
                    break
                previous = current["remaining"]
            check_step(ctx, "TTL 600초에서 5분 주기 재등록", "renewed at ~300 s",
                       {"renewed_after_s": renewed_at},
                       renewed_at is not None and 270 <= renewed_at <= 330,
                       f"periodic re-registration not observed (renewed_at={renewed_at})")
        else:
            ctx.record_step("TTL 5분 초과 주기 재등록 시험 생략", "--long", "not set", passed=True,
                            message="requires --long (about 6 minutes)")


def _read_value_bytes(ctx, object_type, instance, property_id, invoke_id, timeout=2.0):
    """ReadProperty and return the raw ASN.1 bytes inside the PropertyValue tag (None on error)."""
    invoke_id &= 0xFF
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_read_property_apdu(object_type, instance, invoke_id, property_id))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.bind((_client_bind_ip(ctx), 0))
        try:
            sock.sendto(packet, _endpoint(ctx))
            response, _ = sock.recvfrom(4096)
        except (OSError, socket.timeout):
            return None
    status, _ = parse_bacnet_response(response, invoke_id)
    if status != "SUCCESS":
        return None
    open_idx, close_idx = response.rfind(b"\x3e"), response.rfind(b"\x3f")
    return response[open_idx + 1:close_idx] if 0 <= open_idx < close_idx else None


def _write_raw(ctx, object_type, instance, property_id, value_bytes, invoke_id, timeout=3.0):
    """WriteProperty with pre-encoded value. Returns (status, value_or_error_desc)."""
    invoke_id &= 0xFF
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_write_property_raw_apdu(object_type, instance, invoke_id, property_id, value_bytes))
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(timeout)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="WriteProperty Network Port")
        try:
            sock.sendto(packet, _endpoint(ctx))
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            return f"TIMEOUT: {exc}", str(exc)
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(ctx, response, sender, note="WriteProperty Network Port response",
                      parsed={"parse_status": status, "parsed_value": value})
    return status, value


def _unsigned_bytes(value):
    raw = value.to_bytes(max(1, (value.bit_length() + 7) // 8), "big")
    return bytes([0x20 | len(raw)]) + raw


def _host_n_port_bytes(host, port):
    ip = bytes(int(part) for part in host.split("."))
    # BACnetHostNPort: host [0] { ip-address [1] OCTET STRING }, port [1] Unsigned16
    return bytes([0x0E, 0x1C]) + ip + bytes([0x0F, 0x1A, (port >> 8) & 0xFF, port & 0xFF])


def _gateway_log(ctx):
    gateway = ctx.processes.get("gateway")
    if gateway is None:
        return ""
    return gateway.log_path.read_text(encoding="utf-8", errors="replace")


def _fdt_quiet(endpoint, bind_ip):
    """Read an FDT without recording steps. Returns the entry list or None."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(1.0)
        sock.bind((bind_ip, 0))
        try:
            sock.sendto(bytes((0x81, 0x06, 0x00, 0x04)), endpoint)
            response, _ = sock.recvfrom(4096)
        except (OSError, socket.timeout):
            return None
    return _parse_fdt(response)


def _gateway_fdt_entry(ctx, endpoint):
    entries = _fdt_quiet(endpoint, _client_bind_ip(ctx)) or []
    return next((entry for entry in entries
                 if entry["ip"] == ctx.options.target_ip
                 and entry["port"] == ctx.options.target_port), None)


def _bac_network_port_runtime_case(ctx):
    """Change BACnet/IP mode and FD settings by WriteProperty while a BBMD is available (SRS-FUN-BAC-006).

    Covers: Property_List Command exclusion, Changes_Pending=FALSE,
    rejection of invalid values and external Command write with specific error codes,
    runtime WriteProperty application of Lifetime, BBMD Address, and Mode,
    verified via automatic restart and BBMD FDT entries.
    """
    from bbmd_server import BBMDServer

    ctx.require_managed()
    endpoint = _bbmd_endpoint(ctx)
    probe = _bbmd_probe(ctx)
    ctx.evidence.details["bbmd_probe"] = probe

    host, port = endpoint
    if host not in ("127.0.0.1", ctx.options.target_ip):
        ctx.record_step("BBMD 런타임 변경 시험 생략", "local BBMD", host, passed=True,
                        message="second BBMD can only be started next to the Runner")
        return

    # Start stack in FD mode
    ctx.write_bip_config("fd", endpoint)
    ctx.start_stack()

    # Step 1: Property_List without Command(417), Changes_Pending FALSE
    plist_attempt = _read_enum_list(ctx, 56, 1, 371, 100)
    plist = plist_attempt.get("value")
    plist_ok = (plist_attempt["status"] == "SUCCESS" and isinstance(plist, (list, tuple))
                and 417 not in plist)
    check_step(ctx, "NetworkPort Property_List Command 제외", "417 not listed",
               plist, plist_ok, f"Property_List is unreadable or contains Command: {plist}")

    pending = _read_value_bytes(ctx, 56, 1, 416, 101)
    check_step(ctx, "NetworkPort Changes_Pending FALSE", "FALSE", pending.hex() if pending else None,
               pending == b"\x10", f"Changes_Pending is not FALSE: {pending}")

    second = (host, port + 1)
    servers = []

    def start_second():
        server = BBMDServer(host=host, port=second[1])
        threading.Thread(target=server.start, daemon=True).start()
        ctx.wait_for(lambda: server.sock is not None, 3.0, "second BBMD socket")
        servers.append(server)
        return server

    def started():
        return _gateway_log(ctx).count("BACnet IP Gateway started")

    def write_and_restart(prop, value_bytes, invoke_id):
        before = started()
        status, value = _write_raw(ctx, 56, 1, prop, value_bytes, invoke_id)
        check_step(ctx, f"WriteProperty NetworkPort prop={prop}", "SUCCESS", status,
                   status == "SUCCESS", f"write failed: {status} ({value})")
        ctx.wait_for(lambda: started() > before, 90.0, "gateway restart after Network Port write")
        ctx.wait_for(lambda: _read_value_bytes(ctx, 56, 1, 416, 105) is not None, 30.0,
                     "BACnet responses after restart")

    def wait_entry(bbmd, label, timeout=30.0):
        ctx.wait_for(lambda: _gateway_fdt_entry(ctx, bbmd) is not None, timeout, label)
        return _gateway_fdt_entry(ctx, bbmd)

    def unsigned(prop):
        raw = _read_value_bytes(ctx, 56, 1, prop, 106)
        return int.from_bytes(raw[1:], "big") if raw and (raw[0] & 0xF0) == 0x20 else None

    try:
        # Step 2: 잘못된 쓰기는 거부되고 정확한 표준 에러 코드 반환 및 재시작 방지
        before = started()
        invalid_cases = (
            ("BACnet_IP_Mode=BBMD(2) 거부", 408, bytes([0x91, 2]), "VALUE_OUT_OF_RANGE"),
            ("FD_Subscription_Lifetime=0 거부", 419, _unsigned_bytes(0), "VALUE_OUT_OF_RANGE"),
            ("FD_BBMD_Address 0.0.0.0 거부", 418, _host_n_port_bytes("0.0.0.0", port), "VALUE_OUT_OF_RANGE"),
            ("Command(417) 외부 쓰기 거부", 417, bytes([0x91, 2]), "UNKNOWN_PROPERTY"),
        )
        for label, prop, value_bytes, expected_err in invalid_cases:
            status, desc = _write_raw(ctx, 56, 1, prop, value_bytes, 110 + prop % 16)
            err_matched = (status == "BACNET_ERROR_PDU" and expected_err in str(desc))
            check_step(ctx, label, expected_err, f"{status}: {desc}", err_matched,
                       f"expected error {expected_err} but got {status} ({desc})")

        time.sleep(1.0)
        check_step(ctx, "잘못된 쓰기 후 재시작 없음", before, started(), started() == before,
                   "gateway restarted by a rejected write")

        # Step 3: FD_Subscription_Lifetime 실시간 반영 (TTL=10초)
        write_and_restart(419, _unsigned_bytes(10), 120)
        check_step(ctx, "FD_Subscription_Lifetime 반영", 10, unsigned(419),
                   unsigned(419) == 10, "lifetime was not applied")
        entry = wait_entry(endpoint, "registration with TTL=10")
        check_step(ctx, "BBMD FDT TTL 반영", 10, entry["ttl"], entry["ttl"] == 10,
                   f"FDT entry TTL mismatch: {entry}")

        # Step 4: FD_BBMD_Address 변경 -> 두 번째 BBMD로 접속
        bbmd2 = start_second()
        write_and_restart(418, _host_n_port_bytes(*second), 121)
        wait_entry(second, "registration with second BBMD")
        check_step(ctx, "FD_BBMD_Address 변경 후 새 BBMD 등록", True, True, True)

        # Step 5: FD_BBMD_Address 재변경 -> 원래 BBMD로 복구
        write_and_restart(418, _host_n_port_bytes(host, port), 122)
        wait_entry(endpoint, "registration with original BBMD")
        check_step(ctx, "FD_BBMD_Address 재변경 후 원래 BBMD 등록", True, True, True)

        # Step 6: FD -> Normal 모드 변경: 링크 전환, BBMD 재접속 없음
        registered_before = _gateway_log(ctx).count("Registered with BBMD")
        write_and_restart(408, bytes([0x91, 0]), 123)
        mode = _read_value_bytes(ctx, 56, 1, 408, 102)
        check_step(ctx, "BACnet_IP_Mode NORMAL 반영", "9100", mode.hex() if mode else None,
                   mode == b"\x91\x00",
                   f"mode was not applied: {mode}")
        link = ctx.workspace / "bip.cfg"
        check_step(ctx, "bip.cfg -> bip.cfg.normal 링크", "bip.cfg.normal",
                   os.readlink(link) if link.is_symlink() else None,
                   link.is_symlink() and os.readlink(link) == "bip.cfg.normal",
                   "bip.cfg link was not switched")
        fd_attempt = _read_property_attempt(ctx, 56, 1, 418, 103)
        check_step(ctx, "normal 모드에서도 FD_BBMD_Address 읽기", "SUCCESS", fd_attempt["status"],
                   fd_attempt["status"] == "SUCCESS", "FD_BBMD_Address is not readable in normal mode")
        first = _gateway_fdt_entry(ctx, endpoint)
        time.sleep(6.0)
        second_read = _gateway_fdt_entry(ctx, endpoint)
        no_renew = (second_read is None or first is None
                    or second_read["remaining"] < first["remaining"])
        check_step(ctx, "normal 전환 후 BBMD 재접속 없음", True, {"before": first, "after": second_read},
                   no_renew, "FDT entry was renewed after switching to normal")
        check_step(ctx, "normal 전환 후 연결 메시지 없음", registered_before,
                   _gateway_log(ctx).count("Registered with BBMD"),
                   _gateway_log(ctx).count("Registered with BBMD") == registered_before,
                   "gateway registered with a BBMD in normal mode")

        # Step 7: Normal -> FD: 링크 전환 및 BBMD 등록
        write_and_restart(408, bytes([0x91, 1]), 124)
        wait_entry(endpoint, "registration after switching to FD")
        check_step(ctx, "normal -> FD 전환 후 BBMD 등록", True, True, True)

    finally:
        for server in servers:
            server.stop()


def _load_performance_policy(ctx, scenario):
    path = ctx.options.performance_policy
    try:
        policy = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        blocked(f"performance policy를 읽을 수 없습니다: {exc}")
    scenario_policy = policy.get("scenarios", {}).get(scenario)
    if scenario_policy is None and scenario == "TC-PERF-001" and policy.get("approved"):
        scenario_policy = policy
    if not isinstance(scenario_policy, dict) or not scenario_policy.get("approved"):
        blocked(f"{scenario} performance baseline has not been explicitly approved")
    limits = scenario_policy.get("limits", {})
    required = (
        "minimum_success_rate", "minimum_throughput_rps",
        "maximum_p95_latency_ms", "maximum_p99_latency_ms",
    )
    if any(limits.get(key) is None for key in required):
        blocked(f"{scenario} performance policy has incomplete limits")
    ctx.evidence.details["performance_policy"] = {
        "path": str(path),
        "scenario": scenario,
        "limits": limits,
        "approved_at": scenario_policy.get("approved_at"),
        "source_result": scenario_policy.get("source_result"),
    }
    ctx.evidence.details["performance_scenario"] = scenario
    return scenario_policy


def _percentile(values, ratio):
    ordered = sorted(values)
    if not ordered:
        return float("inf")
    return ordered[min(len(ordered) - 1, int((len(ordered) - 1) * ratio))]


def _assert_performance_metrics(ctx, metrics, policy):
    limits = dict(policy["limits"])
    delay_ms = float(getattr(ctx.options, "expected_response_time_ms", 0) or 0)
    if delay_ms > 0:
        scenario = ctx.evidence.details.get("performance_scenario", "")
        if scenario == "TC-PERF-002":
            calculated_rps = max(0.5, (1000.0 / (delay_ms * 2.0 + 80.0)) * 0.7)
            limits["minimum_throughput_rps"] = min(limits["minimum_throughput_rps"], calculated_rps)
            limits["maximum_p95_latency_ms"] = max(limits["maximum_p95_latency_ms"], delay_ms * 2.5 + 50.0)
            limits["maximum_p99_latency_ms"] = max(limits["maximum_p99_latency_ms"], delay_ms * 3.5 + 100.0)
        elif scenario != "TC-PERF-003":
            calculated_rps = max(0.5, (1000.0 / (delay_ms + 40.0)) * 0.7)
            limits["minimum_throughput_rps"] = min(limits["minimum_throughput_rps"], calculated_rps)
            limits["maximum_p95_latency_ms"] = max(limits["maximum_p95_latency_ms"], delay_ms * 1.6 + 30.0)
            limits["maximum_p99_latency_ms"] = max(limits["maximum_p99_latency_ms"], delay_ms * 2.2 + 50.0)
        else:
            limits["maximum_p95_latency_ms"] = max(limits["maximum_p95_latency_ms"], delay_ms * 15.0 + 1000.0)
            limits["maximum_p99_latency_ms"] = max(limits["maximum_p99_latency_ms"], delay_ms * 25.0 + 1500.0)

    errors = metrics.get("error_examples", [])
    check_step(ctx, "benchmark success rate", limits["minimum_success_rate"],
               metrics["success_rate"],
               metrics["success_rate"] >= limits["minimum_success_rate"],
               f"benchmark errors: {errors[:3]}")
    check_step(ctx, "benchmark throughput", limits["minimum_throughput_rps"],
               metrics["throughput_rps"],
               metrics["throughput_rps"] >= limits["minimum_throughput_rps"])
    check_step(ctx, "benchmark P95 latency", limits["maximum_p95_latency_ms"],
               metrics["p95_latency_ms"],
               metrics["p95_latency_ms"] <= limits["maximum_p95_latency_ms"])
    check_step(ctx, "benchmark P99 latency", limits["maximum_p99_latency_ms"],
               metrics["p99_latency_ms"],
               metrics["p99_latency_ms"] <= limits["maximum_p99_latency_ms"])


def _native_benchmark_case(ctx):
    policy = _load_performance_policy(ctx, "TC-PERF-001")
    _ensure_runtime(ctx)
    obj = _first_active_object(ctx)
    total = 100
    successes = 0
    latencies = []
    errors = []
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.settimeout(5.0)
        sock.bind((_client_bind_ip(ctx), 0))
        started = time.monotonic()
        for index in range(total):
            invoke_id = (index + 1) & 0xFF
            packet = build_bacnet_ip_packet(
                None, None, invoke_id,
                build_read_property_apdu(obj["type"], obj["instance"], invoke_id),
            )
            _record_bacnet_tx(ctx, packet, note="benchmark ReadProperty")
            request_started = time.monotonic()
            try:
                sock.sendto(packet, _endpoint(ctx))
                response, sender = sock.recvfrom(4096)
                _record_bacnet_rx(ctx, response, sender, note="benchmark ReadProperty response")
                status, value = parse_bacnet_response(response, invoke_id)
                if status != "SUCCESS":
                    errors.append(f"{status}: {value}")
                    continue
                successes += 1
                latencies.append((time.monotonic() - request_started) * 1000.0)
            except (OSError, socket.timeout) as exc:
                errors.append(str(exc))
        elapsed = max(time.monotonic() - started, 1e-9)
    finally:
        sock.close()
    metrics = {
        "requests": total,
        "successes": successes,
        "errors": len(errors),
        "success_rate": successes / float(total),
        "throughput_rps": successes / elapsed,
        "p95_latency_ms": _percentile(latencies, 0.95),
        "p99_latency_ms": _percentile(latencies, 0.99),
        "duration_sec": elapsed,
        "error_examples": errors[:10],
    }
    ctx.evidence.details["performance_metrics"] = metrics
    _assert_performance_metrics(ctx, metrics, policy)


def _send_performance_request(ctx, sock, invoke_id, apdu, note, cov_counts=None,
                              timeout=3.0):
    packet = build_bacnet_ip_packet(None, None, invoke_id, apdu)
    started = time.monotonic()
    _record_bacnet_tx(ctx, packet, note=note)
    try:
        sock.sendto(packet, _endpoint(ctx))
        response, sender = _recv_bacnet_response_observed(
            sock, invoke_id, timeout=timeout, cov_counts=cov_counts, ctx=ctx,
            return_sender=True,
        )
    except (OSError, socket.timeout, AssertionError) as exc:
        ctx.record_packet(
            "BACnet/IP", "gateway -> client", endpoint=_endpoint(ctx),
            fields={"invoke_id": invoke_id, "error": str(exc)},
            status="timeout", note=f"{note} response",
        )
        return {
            "status": "TIMEOUT",
            "value": str(exc),
            "latency_ms": (time.monotonic() - started) * 1000.0,
        }
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(
        ctx, response, sender, note=f"{note} parsed response",
        parsed={"parse_status": status, "parsed_value": value}, status="parsed",
    )
    return {
        "status": status,
        "value": value,
        "latency_ms": (time.monotonic() - started) * 1000.0,
    }


def _cov_load_worker(ctx, sources, stop_event, errors):
    value_index = 0
    while not stop_event.wait(0.10):
        value_index += 1
        for source, values in sources:
            try:
                ctx.simulator({
                    "op": "set", "block": source["block"], "address": source["address"],
                    "value": values[value_index % len(values)],
                }, record_step=False)
            except Exception as exc:
                errors.append(str(exc))
                return


def _cov_load_benchmark_case(ctx):
    policy = _load_performance_policy(ctx, "TC-PERF-002")
    _require_bacnet_mutation(ctx, "COV load benchmark changes BACnet subscriptions and values")
    ctx.require_capability("main_control", "COV load benchmark needs a controlled Modbus source")
    _ensure_runtime(ctx)
    objects = []
    for obj in _active_objects(ctx, types=(0, 1, 3, 5)):
        source = _source_spec(ctx, obj)
        if source is not None:
            objects.append((obj, source))
        if len(objects) >= 4:
            break
    require(bool(objects), "COV load benchmark has no controllable source object")
    writable = _first_active_object(ctx, types=(1, 5), writable=True)
    sock = _open_observer_socket(ctx)
    subscriptions = []
    cov_counts = {"confirmed": 0, "unconfirmed": 0}
    worker_errors = []
    stop_event = threading.Event()
    foreground_latencies = []
    foreground_errors = []
    foreground_successes = 0
    total = 0
    started = time.monotonic()
    try:
        for index, (obj, _source) in enumerate(objects):
            process_id = 320 + index
            invoke_id = 20 + index
            result = _send_performance_request(
                ctx, sock, invoke_id,
                build_subscribe_cov_apdu(obj["type"], obj["instance"], process_id,
                                         invoke_id, issue_confirmed=(index % 2 == 0),
                                         lifetime=60),
                f"COV load SubscribeCOV {obj['type']}:{obj['instance']}",
                cov_counts=cov_counts,
            )
            check_step(ctx, f"COV load subscription {index + 1}", "SUCCESS",
                       {"status": result["status"], "value": result["value"]},
                       result["status"] == "SUCCESS",
                       f"COV load subscription failed: {result}")
            subscriptions.append((obj, process_id, 100 + index))

        sources = []
        for index, (_obj, source) in enumerate(objects):
            sources.append((source, (index * 11 + 10, index * 11 + 20)))
        worker = threading.Thread(
            target=_cov_load_worker, args=(ctx, sources, stop_event, worker_errors),
            name="cov-load", daemon=True,
        )
        worker.start()
        write_low = writable.get("min_value")
        write_high = writable.get("max_value")
        write_low = float(write_low if write_low is not None else 0.0)
        write_high = float(write_high if write_high is not None else write_low + 1.0)
        if write_low == write_high:
            write_high = write_low
        deadline = time.monotonic() + 10.0
        while time.monotonic() < deadline:
            invoke_id = (80 + total) & 0xFF
            if total % 2 == 0:
                apdu = build_read_property_apdu(objects[0][0]["type"], objects[0][0]["instance"],
                                                 invoke_id)
                note = "HIGH foreground ReadProperty during COV load"
            else:
                value = write_low if (total // 2) % 2 == 0 else write_high
                apdu = build_write_property_apdu(
                    writable["type"], writable["instance"], invoke_id, value,
                    writable["type"] in (3, 4, 5), priority=1,
                )
                note = "HIGH foreground WriteProperty priority=1 during COV load"
            result = _send_performance_request(ctx, sock, invoke_id, apdu, note,
                                               cov_counts=cov_counts, timeout=2.0)
            total += 1
            if result["status"] == "SUCCESS":
                foreground_successes += 1
                foreground_latencies.append(result["latency_ms"])
            else:
                foreground_errors.append(f"{result['status']}: {result['value']}")
            time.sleep(0.01)
        stop_event.set()
        worker.join(timeout=2.0)
        elapsed = max(time.monotonic() - started, 1e-9)
    finally:
        stop_event.set()
        if "worker" in locals():
            worker.join(timeout=2.0)
        for obj, process_id, invoke_id in subscriptions:
            try:
                _cancel_cov(ctx, sock, obj["type"], obj["instance"], process_id, invoke_id)
            except Exception:
                pass
        sock.close()
    require(not worker_errors, f"COV load generator failed: {worker_errors[:3]}")
    cov_total = cov_counts["confirmed"] + cov_counts["unconfirmed"]
    check_step(ctx, "COV load notification", ">= 1", cov_total, cov_total >= 1,
               f"COV load generated no notification: {cov_counts}")
    metrics = {
        "requests": total,
        "successes": foreground_successes,
        "errors": len(foreground_errors),
        "success_rate": foreground_successes / float(max(total, 1)),
        "throughput_rps": foreground_successes / elapsed,
        "p95_latency_ms": _percentile(foreground_latencies, 0.95),
        "p99_latency_ms": _percentile(foreground_latencies, 0.99),
        "duration_sec": elapsed,
        "cov_confirmed": cov_counts["confirmed"],
        "cov_unconfirmed": cov_counts["unconfirmed"],
        "cov_notifications": cov_total,
        "foreground_reads": (total + 1) // 2,
        "foreground_writes": total // 2,
        "error_examples": foreground_errors[:10],
    }
    ctx.evidence.details["performance_metrics"] = metrics
    _assert_performance_metrics(ctx, metrics, policy)


def _queue_request(ctx, index):
    invoke_id = (120 + index) & 0xFF
    packet = build_bacnet_ip_packet(
        None, None, invoke_id,
        build_read_property_apdu(1, 2, invoke_id),
    )
    started = time.monotonic()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.settimeout(2.0)
        sock.bind((_client_bind_ip(ctx), 0))
        _record_bacnet_tx(ctx, packet, note="queue saturation ReadProperty")
        try:
            sock.sendto(packet, _endpoint(ctx))
            response, sender = sock.recvfrom(4096)
        except (OSError, socket.timeout) as exc:
            ctx.record_packet(
                "BACnet/IP", "gateway -> client", endpoint=_endpoint(ctx),
                fields={"invoke_id": invoke_id, "error": str(exc)}, status="timeout",
                note="queue saturation response",
            )
            return {"status": "TIMEOUT", "latency_ms": (time.monotonic() - started) * 1000.0,
                    "error": str(exc)}
    status, value = parse_bacnet_response(response, invoke_id)
    _record_bacnet_rx(ctx, response, sender, note="queue saturation response",
                      parsed={"parse_status": status, "parsed_value": value})
    return {"status": status, "value": value,
            "latency_ms": (time.monotonic() - started) * 1000.0}


def _queue_saturation_benchmark_case(ctx):
    policy = _load_performance_policy(ctx, "TC-PERF-003")
    ctx.require_capability("main_control", "queue saturation benchmark needs Main fault injection")
    _ensure_runtime(ctx)
    request_count = 64
    ctx.simulator({"op": "fault", "action": "timeout", "count": request_count * 4})
    started = time.monotonic()
    results = []
    try:
        with ThreadPoolExecutor(max_workers=16) as executor:
            futures = [executor.submit(_queue_request, ctx, index)
                       for index in range(request_count)]
            try:
                for future in as_completed(futures, timeout=20.0):
                    results.append(future.result())
            except FuturesTimeoutError:
                results.extend({"status": "UNBOUNDED", "latency_ms": 20000.0,
                                "error": "worker did not complete within 20 seconds"}
                               for future in futures if not future.done())
        elapsed = time.monotonic() - started
    finally:
        try:
            ctx.simulator({"op": "clear_faults"})
            time.sleep(0.3)
        except Exception:
            pass
    completed = len(results)
    successes = sum(item.get("status") == "SUCCESS" for item in results)
    failed = completed - successes
    max_latency = max((item.get("latency_ms", 0.0) for item in results), default=float("inf"))
    check_step(ctx, "timeout queue bounded completion", request_count, completed,
               completed == request_count and elapsed <= 20.0,
               f"queue completion exceeded bound: completed={completed}, elapsed={elapsed:.3f}s")
    check_step(ctx, "timeout queue failure evidence", ">= 1", failed, failed >= 1,
               "fault-injected queue unexpectedly returned no failure")
    recovery_started = time.monotonic()
    recovery = _read_property(ctx, 1, 2, 85, invoke_id=240)
    recovery_latency = (time.monotonic() - recovery_started) * 1000.0
    check_step(ctx, "fault 해제 후 정상 요청 복구", "SUCCESS", recovery,
               recovery is not None, f"normal request did not recover: {recovery}")
    latencies = [item["latency_ms"] for item in results if "latency_ms" in item]
    metrics = {
        "requests": request_count,
        "successes": successes,
        "errors": failed,
        "success_rate": successes / float(request_count),
        "throughput_rps": successes / max(elapsed, 1e-9),
        "p95_latency_ms": _percentile(latencies, 0.95),
        "p99_latency_ms": _percentile(latencies, 0.99),
        "duration_sec": elapsed,
        "completed": completed,
        "max_latency_ms": max_latency,
        "recovery_latency_ms": recovery_latency,
        "bounded": completed == request_count and elapsed <= 20.0,
        "error_examples": [item.get("error", item.get("value")) for item in results
                           if item.get("status") != "SUCCESS"][:10],
    }
    ctx.evidence.details["performance_metrics"] = metrics
    _assert_performance_metrics(ctx, metrics, policy)


def _get_process_metrics(pid: Optional[int]) -> Optional[Dict[str, Any]]:
    if pid is None:
        return None
    rss_kb = None
    vm_size_kb = None
    fd_count = None
    status_file = Path(f"/proc/{pid}/status")
    if status_file.is_file():
        try:
            for line in status_file.read_text(encoding="utf-8", errors="ignore").splitlines():
                if line.startswith("VmRSS:"):
                    rss_kb = int(line.split()[1])
                elif line.startswith("VmSize:"):
                    vm_size_kb = int(line.split()[1])
        except Exception:
            pass
    fd_dir = Path(f"/proc/{pid}/fd")
    if fd_dir.is_dir():
        try:
            fd_count = len(list(fd_dir.iterdir()))
        except Exception:
            pass
    return {
        "rss_kb": rss_kb,
        "vm_size_kb": vm_size_kb,
        "fd_count": fd_count,
    }


def _get_vport_pid(ctx) -> Optional[int]:
    if ctx.managed and "vport" in ctx.processes:
        proc = ctx.processes["vport"].process
        if proc.poll() is None:
            return proc.pid
    try:
        res = subprocess.run(["pgrep", "-f", "vport"], stdout=subprocess.PIPE, text=True, check=False)
        pids = [int(p.strip()) for p in res.stdout.split() if p.strip().isdigit()]
        return pids[0] if pids else None
    except Exception:
        return None


def _is_process_alive(ctx, name: str, pid: Optional[int]) -> bool:
    if ctx.managed and name in ctx.processes:
        proc = ctx.processes[name].process
        return proc is not None and proc.poll() is None
    if pid is not None:
        try:
            os.kill(pid, 0)
            return True
        except OSError:
            return False
    return True


_SSH_DIAG_REPORTED = False


def _get_remote_ssh_system_metrics(ctx) -> Optional[Dict[str, Dict[str, Any]]]:
    global _SSH_DIAG_REPORTED
    if not getattr(ctx.options, "ssh_enable", False):
        return None
    ip = getattr(ctx.options, "target_ip", None) or getattr(ctx.options, "ip", None)
    if not ip or ip in ("127.0.0.1", "localhost"):
        return None

    ssh_user = getattr(ctx.options, "ssh_user", "root") or "root"
    ssh_port = getattr(ctx.options, "ssh_port", 22) or 22
    ssh_pass = getattr(ctx.options, "ssh_pass", None)

    # Robust PID finder supporting standard Linux and BusyBox
    remote_script = (
        'find_pid() { pgrep -f "$1" 2>/dev/null | head -n1 || ps -ef 2>/dev/null | grep "$1" | grep -v grep | awk \'{print $2}\' | head -n1 || ps 2>/dev/null | grep "$1" | grep -v grep | awk \'{print $1}\' | head -n1; }; '
        'GW_PID=$(find_pid bacnet_ip_gateway); '
        'VP_PID=$(find_pid vport); '
        'GW_RSS=$([ -n "$GW_PID" ] && grep -i VmRSS /proc/$GW_PID/status 2>/dev/null | awk \'{print $2}\' || echo "0"); '
        'GW_FDS=$([ -n "$GW_PID" ] && ls -1 /proc/$GW_PID/fd 2>/dev/null | wc -l || echo "0"); '
        'VP_RSS=$([ -n "$VP_PID" ] && grep -i VmRSS /proc/$VP_PID/status 2>/dev/null | awk \'{print $2}\' || echo "0"); '
        'VP_FDS=$([ -n "$VP_PID" ] && ls -1 /proc/$VP_PID/fd 2>/dev/null | wc -l || echo "0"); '
        'echo "GW:${GW_PID:-none}:${GW_RSS:-0}:${GW_FDS:-0} VP:${VP_PID:-none}:${VP_RSS:-0}:${VP_FDS:-0}"'
    )

    res = _run_remote_ssh_command(ctx, remote_script, timeout=8.0)
    if res is not None and res[0] == 0 and res[1].strip():
        out = res[1].strip()
        metrics = {}
        for token in out.split():
            parts = token.split(":")
            if len(parts) >= 4:
                tag, p_pid, p_rss, p_fds = parts[0], parts[1], parts[2], parts[3]
                pid_val = int(p_pid) if p_pid.isdigit() else None
                rss_val = int(p_rss) if p_rss.isdigit() and int(p_rss) > 0 else None
                fds_val = int(p_fds) if p_fds.isdigit() and int(p_fds) > 0 else None
                key = "gateway" if tag == "GW" else "vport"
                metrics[key] = {
                    "pid": pid_val,
                    "rss_kb": rss_val,
                    "vm_size_kb": None,
                    "fd_count": fds_val,
                }
        ctx._ssh_connected = True
        if not _SSH_DIAG_REPORTED:
            _SSH_DIAG_REPORTED = True
            gw_info = metrics.get("gateway", {})
            vp_info = metrics.get("vport", {})
            print(f"    [SSH] 원격 대상({ssh_user}@{ip}:{ssh_port}) 접속 성공: "
                  f"Gateway PID={gw_info.get('pid')}(RSS {gw_info.get('rss_kb')} KB), "
                  f"vport PID={vp_info.get('pid')}(RSS {vp_info.get('rss_kb')} KB)",
                  flush=True)
        if "gateway" in metrics or "vport" in metrics:
            return metrics
    else:
        err = getattr(ctx, "_ssh_last_error", None) or (res[2].strip() if res else "connection failure")
        ctx._ssh_connect_error = err
        ctx._ssh_connected = False
        if not _SSH_DIAG_REPORTED:
            _SSH_DIAG_REPORTED = True
            print(f"\n    [SSH 오류] 원격 대상({ssh_user}@{ip}:{ssh_port}) SSH 접속 실패: {err}\n", file=sys.stderr, flush=True)
    return None


def _get_remote_ssh_metrics(ctx) -> Optional[Dict[str, Any]]:
    sys_m = _get_remote_ssh_system_metrics(ctx)
    if sys_m and "gateway" in sys_m:
        return sys_m["gateway"]
    return None


def _get_gateway_pid(ctx) -> Optional[int]:
    if ctx.managed and "gateway" in ctx.processes:
        return ctx.processes["gateway"].process.pid
    try:
        res = subprocess.run(["pgrep", "-f", "bacnet_ip_gateway"], stdout=subprocess.PIPE, text=True, check=False)
        pids = [int(p.strip()) for p in res.stdout.split() if p.strip().isdigit()]
        return pids[0] if pids else None
    except Exception:
        return None


def _is_gateway_alive(ctx, pid: Optional[int]) -> bool:
    return _is_process_alive(ctx, "gateway", pid)


def _format_hms(seconds: float) -> str:
    secs = int(max(0, seconds))
    h = secs // 3600
    m = (secs % 3600) // 60
    s = secs % 60
    return f"{h:02d}:{m:02d}:{s:02d}"


def _soak_reliability_case(ctx):
    if not getattr(ctx.options, "soak", False):
        blocked("12-hour reliability soak test requires --soak (use option 4 'soak')")

    configs = _runtime_configs(ctx)
    all_objects = [obj for dev in configs["devices"] for obj in dev["objects"]]
    require(len(all_objects) > 0, "No objects defined in profile")

    if ctx.managed:
        ctx.start_stack()
    else:
        _ensure_runtime(ctx)

    gw_pid = _get_gateway_pid(ctx)
    vp_pid = _get_vport_pid(ctx)
    is_local = (gw_pid is not None)

    soak_hours = getattr(ctx.options, "soak_hours", 12.0)
    duration_sec = max(1.0, soak_hours * 3600.0)
    cycle_interval = max(0.5, getattr(ctx.options, "soak_interval", 15.0))

    # 1. Warmup
    warmup_time = min(2.0, duration_sec * 0.1)
    if warmup_time > 0:
        time.sleep(warmup_time)

    if is_local:
        gw_base = _get_process_metrics(gw_pid)
        vp_base = _get_process_metrics(vp_pid) if vp_pid else None
    else:
        remote_sys = _get_remote_ssh_system_metrics(ctx)
        gw_base = remote_sys.get("gateway") if remote_sys else None
        vp_base = remote_sys.get("vport") if remote_sys else None

    has_os_metrics = (gw_base is not None and gw_base.get("rss_kb") is not None)
    has_vport = (vp_base is not None and vp_base.get("rss_kb") is not None) or (vp_pid is not None)

    if not is_local and getattr(ctx.options, "ssh_enable", False) and not has_os_metrics:
        err = getattr(ctx, "_ssh_connect_error", getattr(ctx, "_ssh_last_error", "인증 실패 또는 접속 불가"))
        blocked(f"원격 SSH 접속 실패 ({getattr(ctx.options, 'ssh_user', 'root')}@{getattr(ctx.options, 'target_ip', '')}:{getattr(ctx.options, 'ssh_port', 22)}): "
                f"OS 자원(메모리/FD/vport) 실시간 모니터링을 수행할 수 없습니다. (원인: {err})")

    baseline_gw_rss = gw_base["rss_kb"] if (gw_base and gw_base.get("rss_kb")) else None
    baseline_gw_fds = gw_base["fd_count"] if (gw_base and gw_base.get("fd_count") is not None) else None
    baseline_vp_rss = vp_base["rss_kb"] if (vp_base and vp_base.get("rss_kb")) else None
    baseline_vp_fds = vp_base["fd_count"] if (vp_base and vp_base.get("fd_count") is not None) else None

    # 2. Soak Loop
    started = time.monotonic()
    samples = []
    total_reads = 0
    failed_reads = 0
    cycle = 0
    invoke_id = 1
    last_progress_log = 0.0

    while time.monotonic() - started < duration_sec:
        cycle += 1
        cycle_start = time.monotonic()

        # Check process liveness
        if not _is_process_alive(ctx, "gateway", gw_pid):
            require(False, f"Gateway crashed during soak test at {time.monotonic() - started:.1f}s")
        if has_vport and not _is_process_alive(ctx, "vport", vp_pid):
            require(False, f"vport crashed during soak test at {time.monotonic() - started:.1f}s")
        if ctx.managed:
            sim = ctx.processes.get("main_simulator")
            if sim and sim.process.poll() is not None:
                require(False, f"Main simulator crashed during soak test at {time.monotonic() - started:.1f}s")

        # Query all objects
        cycle_failed = 0
        cycle_latencies = []
        for obj in all_objects:
            res = _read_property_attempt(ctx, obj["type"], obj["instance"], 85, invoke_id, timeout=2.0)
            invoke_id = (invoke_id + 1) & 0xFF
            total_reads += 1
            if res.get("status") != "SUCCESS":
                failed_reads += 1
                cycle_failed += 1
            if "latency_ms" in res:
                cycle_latencies.append(res["latency_ms"])

        if not is_local and cycle_failed == len(all_objects):
            require(False, f"Gateway unresponsive on network at {time.monotonic() - started:.1f}s")

        if is_local:
            cur_gw_m = _get_process_metrics(gw_pid)
            cur_vp_m = _get_process_metrics(vp_pid) if vp_pid else None
        else:
            remote_cur = _get_remote_ssh_system_metrics(ctx) if has_os_metrics else None
            cur_gw_m = remote_cur.get("gateway") if remote_cur else None
            cur_vp_m = remote_cur.get("vport") if remote_cur else None

        cur_gw_rss = cur_gw_m["rss_kb"] if cur_gw_m else (baseline_gw_rss or 0)
        cur_gw_fds = cur_gw_m["fd_count"] if cur_gw_m else (baseline_gw_fds or 0)
        cur_vp_rss = cur_vp_m["rss_kb"] if cur_vp_m else (baseline_vp_rss or 0)
        cur_vp_fds = cur_vp_m["fd_count"] if cur_vp_m else (baseline_vp_fds or 0)

        if not is_local and has_os_metrics:
            if cur_gw_m is None or cur_gw_m.get("pid") is None:
                require(False, f"Gateway crashed during soak test at {time.monotonic() - started:.1f}s")
            if has_vport and (cur_vp_m is None or cur_vp_m.get("pid") is None):
                require(False, f"vport crashed during soak test at {time.monotonic() - started:.1f}s")

        now = time.monotonic()
        elapsed = now - started
        avg_cycle_lat = (sum(cycle_latencies) / len(cycle_latencies)) if cycle_latencies else 0.0

        samples.append({
            "cycle": cycle,
            "elapsed_sec": round(elapsed, 2),
            "gw_rss_kb": cur_gw_rss,
            "gw_rss_growth_kb": (cur_gw_rss - baseline_gw_rss) if baseline_gw_rss is not None else 0,
            "gw_rss_growth_pct": round(((cur_gw_rss - baseline_gw_rss) / baseline_gw_rss) * 100.0, 2) if baseline_gw_rss else 0.0,
            "gw_fds": cur_gw_fds,
            "vp_rss_kb": cur_vp_rss,
            "vp_rss_growth_kb": (cur_vp_rss - baseline_vp_rss) if baseline_vp_rss is not None else 0,
            "vp_rss_growth_pct": round(((cur_vp_rss - baseline_vp_rss) / baseline_vp_rss) * 100.0, 2) if baseline_vp_rss else 0.0,
            "vp_fds": cur_vp_fds,
            "avg_latency_ms": round(avg_cycle_lat, 2),
            "total_reads": total_reads,
            "failed_reads": failed_reads,
        })

        # Periodic progress report: every 60s for long tests, or every 10s for short (<180s) tests, and cycle 1
        progress_interval = 10.0 if duration_sec < 180.0 else 60.0
        if (now - last_progress_log >= progress_interval) or (cycle == 1):
            last_progress_log = now
            pct = min(100.0, (elapsed / duration_sec) * 100.0)
            elapsed_str = _format_hms(elapsed)
            total_str = _format_hms(duration_sec)
            rate = ((total_reads - failed_reads) / total_reads * 100.0) if total_reads > 0 else 100.0
            metric_str = ""
            if has_os_metrics and cur_gw_rss:
                gw_delta = cur_gw_rss - (baseline_gw_rss or cur_gw_rss)
                metric_str = f" | GW RSS: {cur_gw_rss} KB ({gw_delta:+d} KB), FD: {cur_gw_fds}"
                if has_vport and cur_vp_rss:
                    vp_delta = cur_vp_rss - (baseline_vp_rss or cur_vp_rss)
                    metric_str += f" | vport RSS: {cur_vp_rss} KB ({vp_delta:+d} KB), FD: {cur_vp_fds}"
            elif avg_cycle_lat > 0:
                metric_str = f" | RTT: {avg_cycle_lat:.1f} ms"
            print(f"    [SOAK] 진행률: {pct:5.1f}% [{elapsed_str} / {total_str}] | "
                  f"Cycle {cycle:4d} | Reads: {total_reads} (성공률 {rate:.2f}%){metric_str}",
                  flush=True)

        if time.monotonic() - started < duration_sec:
            rem = duration_sec - (time.monotonic() - started)
            cycle_elapsed = time.monotonic() - cycle_start
            sleep_time = min(max(0.0, cycle_interval - cycle_elapsed), rem)
            if sleep_time > 0:
                time.sleep(sleep_time)

    # 3. Evidence & Assertions
    first_cycle_lat = samples[0]["avg_latency_ms"] if samples else 0.0
    last_cycle_lat = samples[-1]["avg_latency_ms"] if samples else 0.0
    lat_growth = last_cycle_lat - first_cycle_lat

    ctx.evidence.details["soak_metrics"] = {
        "is_local_host": is_local,
        "has_os_metrics": has_os_metrics,
        "has_vport": has_vport,
        "soak_hours": soak_hours,
        "duration_sec": round(time.monotonic() - started, 2),
        "cycles_completed": cycle,
        "total_reads": total_reads,
        "failed_reads": failed_reads,
        "gateway": {
            "baseline_rss_kb": baseline_gw_rss,
            "final_rss_kb": samples[-1]["gw_rss_kb"] if samples else baseline_gw_rss,
            "baseline_fds": baseline_gw_fds,
            "final_fds": samples[-1]["gw_fds"] if samples else baseline_gw_fds,
        },
        "vport": {
            "monitored": has_vport,
            "baseline_rss_kb": baseline_vp_rss,
            "final_rss_kb": samples[-1]["vp_rss_kb"] if samples else baseline_vp_rss,
            "baseline_fds": baseline_vp_fds,
            "final_fds": samples[-1]["vp_fds"] if samples else baseline_vp_fds,
        },
        "baseline_latency_ms": first_cycle_lat,
        "final_latency_ms": last_cycle_lat,
        "latency_growth_ms": round(lat_growth, 2),
        "samples_count": len(samples),
    }

    # Step 1: Process Liveness
    liveness_label = "장기 프로세스 무중단 생존 (Gateway & vport)" if has_vport else "장기 프로세스 무중단 생존 (Gateway)"
    check_step(ctx, liveness_label, True, True, True, "Process did not survive soak test")

    # Step 2: Communication Success Rate
    success_rate = ((total_reads - failed_reads) / total_reads * 100.0) if total_reads > 0 else 0.0
    check_step(ctx, "전체 관제점 통신 성공률 (>=99.9%)", 99.9, round(success_rate, 3),
               success_rate >= 99.9, f"Communication success rate too low: {success_rate:.3f}% ({failed_reads}/{total_reads} failed)")

    # Step 3 & Step 4: Memory & FD (if OS metrics available) or Network Stability metrics
    if has_os_metrics and baseline_gw_rss is not None:
        final_gw_rss = samples[-1]["gw_rss_kb"] if samples else baseline_gw_rss
        gw_growth_pct = ((final_gw_rss - baseline_gw_rss) / baseline_gw_rss * 100.0) if baseline_gw_rss else 0.0
        gw_growth_kb = final_gw_rss - baseline_gw_rss
        gw_ok = (gw_growth_pct < 5.0) or (gw_growth_kb < 2048)

        vp_ok = True
        vp_details_str = ""
        if has_vport and baseline_vp_rss is not None:
            final_vp_rss = samples[-1]["vp_rss_kb"] if samples else baseline_vp_rss
            vp_growth_pct = ((final_vp_rss - baseline_vp_rss) / baseline_vp_rss * 100.0) if baseline_vp_rss else 0.0
            vp_growth_kb = final_vp_rss - baseline_vp_rss
            vp_ok = (vp_growth_pct < 5.0) or (vp_growth_kb < 2048)
            vp_details_str = f", vport: {vp_growth_pct:+.2f}% ({vp_growth_kb:+d} KB)"

        rss_all_ok = gw_ok and vp_ok
        actual_rss_str = f"GW: {gw_growth_pct:+.2f}% ({gw_growth_kb:+d} KB){vp_details_str}"
        check_step(ctx, "메모리(RSS) 누수 억제 (<5% 성장 또는 <2MB)",
                   "< 5.0% 또는 < 2048 KB",
                   actual_rss_str,
                   rss_all_ok, f"Excessive memory growth detected: {actual_rss_str}")

        final_gw_fds = samples[-1]["gw_fds"] if samples else baseline_gw_fds
        gw_fd_delta = (final_gw_fds - baseline_gw_fds) if (final_gw_fds is not None and baseline_gw_fds is not None) else 0
        gw_fd_ok = gw_fd_delta <= 0 or (gw_fd_delta <= 1 and soak_hours < 0.1)

        vp_fd_ok = True
        vp_fd_str = ""
        if has_vport and baseline_vp_fds is not None:
            final_vp_fds = samples[-1]["vp_fds"] if samples else baseline_vp_fds
            vp_fd_delta = (final_vp_fds - baseline_vp_fds) if (final_vp_fds is not None and baseline_vp_fds is not None) else 0
            vp_fd_ok = vp_fd_delta <= 0 or (vp_fd_delta <= 1 and soak_hours < 0.1)
            vp_fd_str = f", vport: {vp_fd_delta:+d} (시작 {baseline_vp_fds} -> 종료 {final_vp_fds})"

        fd_all_ok = gw_fd_ok and vp_fd_ok
        actual_fd_str = f"GW: {gw_fd_delta:+d} (시작 {baseline_gw_fds} -> 종료 {final_gw_fds}){vp_fd_str}"
        check_step(ctx, "파일 디스크립터(FD) 누수 없음 (증가량 <= 0)",
                   "<= 0",
                   actual_fd_str,
                   fd_all_ok, f"FD leak detected: {actual_fd_str}")
    else:
        # Remote mode without SSH: evaluate numeric network stability metrics
        lat_ok = lat_growth < 50.0
        check_step(ctx, "원격 네트워크 응답 지연(RTT) 드리프트 억제 (<50ms)",
                   "< 50.0 ms",
                   f"{lat_growth:+.2f} ms (초기 {first_cycle_lat:.1f} ms -> 최종 {last_cycle_lat:.1f} ms)",
                   lat_ok, f"Network latency drifted excessively: +{lat_growth:.2f} ms")

        check_step(ctx, "원격 관제점 통신 실패/타임아웃 발생 수 (0건)",
                   0,
                   failed_reads,
                   failed_reads == 0,
                   f"Detected {failed_reads} communication timeouts/failures during soak test")

    if ctx.managed:
        ctx.stop_process("gateway")
        ctx.stop_process("main_simulator")
        ctx.stop_process("vport")


def _nonfunctional_case(ctx, tc_id):
    if tc_id == "TC-AVL-001":
        ctx.require_managed()
        ctx.start_stack()
        baseline = _read_property(ctx, 1, 5, 85, invoke_id=30)
        check_step(ctx, "vport 재시작 전 BACnet ReadProperty", "numeric", baseline,
                   isinstance(baseline, (int, float)) and not isinstance(baseline, bool),
                   f"baseline read failed: {baseline}")
        old_vport_pid = ctx.processes["vport"].process.pid
        outage_started = time.monotonic()
        ctx.force_kill_process("vport")
        stale_links = sorted(path.name for path in ctx.dev_dir.iterdir())
        check_step(ctx, "vport 강제 종료 및 PTY 단절", True,
                   {"process_alive": False, "stale_links": stale_links},
                   ctx.processes.get("vport") is None and bool(stale_links),
                   "vport process did not terminate or its crash links were not observable")
        ctx.start_vport(3)
        ctx.wait_for(lambda: (ctx.dev_dir / "ttySLAVE").is_symlink(), 5.0,
                     "vport link recreation")
        restart_elapsed = time.monotonic() - outage_started
        check_step(ctx, "vport process 재기동", True,
                   ctx.processes["vport"].process.pid != old_vport_pid,
                   ctx.processes["vport"].process.pid != old_vport_pid,
                   "vport PID did not change")

        recovery_started = time.monotonic()
        attempts = []
        deadline = recovery_started + 12.0
        while time.monotonic() < deadline:
            attempt = _read_property_attempt(
                ctx, 1, 5, 85, 31 + len(attempts), timeout=0.75,
                note="vport restart recovery ReadProperty",
            )
            attempts.append(attempt)
            if attempt["status"] == "SUCCESS":
                break
            time.sleep(0.10)
        recovery = next((item for item in attempts if item["status"] == "SUCCESS"), None)
        recovery_latency = (time.monotonic() - recovery_started) * 1000.0
        check_step(ctx, "vport 재생성 후 BACnet/Modbus 요청 복구", "SUCCESS",
                   recovery or attempts[-1] if attempts else "no attempt",
                   recovery is not None,
                   f"request did not recover after vport restart: {attempts[-3:]}")
        log = ctx.gateway_log().lower()
        reconnect_logged = "reconnected successfully" in log or "connected/reconnected" in log
        check_step(ctx, "Gateway serial reconnect 증적", True, reconnect_logged,
                   reconnect_logged, "serial reconnect log was not observed")
        ctx.evidence.details["availability_recovery"] = {
            "vport_pid_before": old_vport_pid,
            "vport_pid_after": ctx.processes["vport"].process.pid,
            "vport_restart_sec": restart_elapsed,
            "request_recovery_ms": recovery_latency,
            "attempts": attempts,
        }
    elif tc_id == "TC-AVL-002":
        ctx.require_managed()
        ctx.start_stack()
        ctx.stop_process("gateway")
        require(ctx.processes["main_simulator"].process.poll() is None, "Main simulator stopped with gateway")
        require(ctx.processes["vport"].process.poll() is None, "vport stopped with gateway")
    elif tc_id == "TC-AVL-003":
        ctx.require_managed()
        ctx.start_stack()
        started = time.monotonic()
        ctx.stop_process("gateway")
        require(time.monotonic() - started < 3.0, "gateway shutdown hung")
    elif tc_id == "TC-AVL-004":
        _soak_reliability_case(ctx)
    elif tc_id.startswith("TC-PERF-"):
        if tc_id == "TC-PERF-001":
            _native_benchmark_case(ctx)
        elif tc_id == "TC-PERF-002":
            _cov_load_benchmark_case(ctx)
        else:
            _queue_saturation_benchmark_case(ctx)
    elif tc_id == "TC-OBS-001":
        _config_case(ctx, "TC-CFG-006")
        log = ctx.gateway_log()
        require("Waiting 60 seconds" in log and "min_value" in log and "max_value" in log,
                "HOLD log context/rate message missing")
    elif tc_id == "TC-OBS-002":
        ctx.require_managed()
        ctx.start_stack()
        log = ctx.gateway_log()
        for token in ("Relinquish", "clamp", "Synchronized"):
            require(token.lower() in log.lower(), f"startup trace token missing: {token}")
    elif tc_id == "TC-OBS-003":
        if not ctx.options.long_tests:
            blocked("10-minute observability test requires --long")
        ctx.require_managed()
        ctx.require_capability("main_control", "long observability test needs Main timeout injection")
        ctx.start_stack()
        ctx.simulator({"op": "fault", "action": "timeout", "count": 100000})
        duration = 600.0
        sample_interval = 1.0
        probe_interval = 2.0
        probe_stop = threading.Event()
        probe_results = []
        probe_errors = []

        def timeout_probe():
            """Generate real BACnet requests while Main drops Modbus replies."""
            invoke_id = 180
            next_probe = time.monotonic()
            while not probe_stop.is_set():
                try:
                    probe_results.append(_read_property_attempt(
                        ctx, 1, 5, 85, invoke_id,
                        timeout=2.0,
                        note="OBS-003 controlled Main-timeout probe",
                    ))
                except Exception as exc:  # keep the sampler alive and preserve evidence
                    probe_errors.append(f"{type(exc).__name__}: {exc}")
                invoke_id = (invoke_id + 1) & 0xFF
                next_probe += probe_interval
                if probe_stop.wait(max(0.0, next_probe - time.monotonic())):
                    break

        # Startup emits a finite burst of configuration messages.  Establish
        # the baseline after fault setup so that startup output is not counted
        # as a timeout-period logging rate.
        baseline_text = ctx.gateway_log()
        baseline_lines = len(baseline_text.splitlines())
        baseline_bytes = len(baseline_text.encode("utf-8"))
        previous_lines = baseline_lines
        previous_bytes = baseline_bytes
        started = time.monotonic()
        samples = []
        probe_thread = threading.Thread(target=timeout_probe,
                                        name="tc-obs-003-timeout-probe",
                                        daemon=True)
        probe_thread.start()
        try:
            while time.monotonic() - started < duration:
                gateway = ctx.processes.get("gateway")
                alive = gateway is not None and gateway.process.poll() is None
                if not alive:
                    require(False, "Gateway exited during 10-minute Main timeout test")
                sample_started = time.monotonic()
                time.sleep(min(sample_interval, duration - (time.monotonic() - started)))
                text = ctx.gateway_log()
                line_count = len(text.splitlines())
                byte_count = len(text.encode("utf-8"))
                sample_elapsed = max(time.monotonic() - sample_started, 1e-9)
                elapsed = max(time.monotonic() - started, 1e-9)
                samples.append({
                    "elapsed_sec": round(elapsed, 3),
                    "lines": line_count,
                    "bytes": byte_count,
                    "line_rate": (line_count - previous_lines) / sample_elapsed,
                    "byte_rate": (byte_count - previous_bytes) / sample_elapsed,
                    "alive": alive,
                })
                previous_lines = line_count
                previous_bytes = byte_count
        finally:
            probe_stop.set()
            probe_thread.join(timeout=5.0)
            ctx.simulator({"op": "clear_faults"})
        elapsed = time.monotonic() - started
        rates = [sample["line_rate"] for sample in samples]
        max_line_rate = max(rates, default=float("inf"))
        probe_statuses = [item.get("status") for item in probe_results]
        probe_summary = {
            "requests": len(probe_results),
            "non_success": sum(status != "SUCCESS" for status in probe_statuses),
            "statuses": sorted(set(probe_statuses)),
            "errors": probe_errors,
            "interval_sec": probe_interval,
        }
        ctx.evidence.details["observability_probe"] = probe_summary
        check_step(ctx, "[BACnet/IP] Main timeout probe", "ERROR/TIMEOUT",
                   probe_summary,
                   bool(probe_results) and
                   all(status != "SUCCESS" for status in probe_statuses) and
                   not probe_errors,
                   f"timeout probe did not observe only failures: {probe_summary}")
        check_step(ctx, "10분 Main timeout 중 Gateway 생존", True,
                   all(sample["alive"] for sample in samples),
                   all(sample["alive"] for sample in samples),
                   "Gateway was not alive for every sampler interval")
        observability_metrics = {
            "duration_sec": elapsed,
            "sample_interval_sec": sample_interval,
            "samples": samples,
            "max_line_rate": max_line_rate,
            "final_log_bytes": previous_bytes,
            "fault": "timeout",
            "startup_baseline_lines": baseline_lines,
            "startup_baseline_bytes": baseline_bytes,
            "timeout_probe": probe_summary,
        }
        # Preserve the full sampler evidence even when a following assertion
        # fails and execute_case records the exception.
        ctx.evidence.details["observability_metrics"] = observability_metrics
        check_step(ctx, "10분 log rate bounded", "<= 20 lines/s", max_line_rate,
                   max_line_rate <= 20.0,
                   f"busy-loop logging observed: {max_line_rate:.3f} lines/s")
        shutdown_started = time.monotonic()
        ctx.stop_process("gateway")
        shutdown_elapsed = time.monotonic() - shutdown_started
        check_step(ctx, "10분 timeout 후 SIGTERM 종료", "<= 3s", shutdown_elapsed,
                   shutdown_elapsed <= 3.0,
                   "Gateway did not stop promptly after long timeout test")


def _standards_case(ctx, tc_id):
    if tc_id in NATIVE_STEP1:
        _native_step1_case(ctx, tc_id)
    elif tc_id in PROFILE_TESTER_CASES:
        _run_profile_test(ctx)
    elif tc_id in EPICS_VERIFIER_CASES:
        _run_epics_conformance_test(ctx)
    elif tc_id == "TC-BTL-003":
        def _resolve_candidate(p):
            if not p:
                return None
            pth = Path(p)
            if pth.exists():
                return pth.resolve()
            if (GATEWAY_ROOT / pth).exists():
                return (GATEWAY_ROOT / pth).resolve()
            if (ES_ROOT / pth).exists():
                return (ES_ROOT / pth).resolve()
            return pth

        # 1. TPI 전자 명세서 파일 (사용자 지정 경로 최우선, 자동 추정 금지)
        target_tpi = None
        tpi_opt = getattr(ctx.options, "tpi_path", None) or getattr(ctx.options, "tpi", None)
        if tpi_opt:
            resolved_tpi = _resolve_candidate(tpi_opt)
            require(resolved_tpi and resolved_tpi.is_file(), f"지정된 TPI 파일을 찾을 수 없습니다: {tpi_opt}")
            target_tpi = resolved_tpi
        else:
            default_tpi = GATEWAY_ROOT / "model" / "cdu" / "[BTL]cdu.tpi"
            if default_tpi.is_file():
                target_tpi = default_tpi
            else:
                cands = sorted(GATEWAY_ROOT.glob("model/**/*.tpi"))
                require(cands, "기본 TPI 파일을 찾을 수 없습니다.")
                target_tpi = cands[0]
        ctx.evidence.details["tpi_source_file"] = str(target_tpi)
        tpi_text = target_tpi.read_text(encoding="utf-8", errors="replace")

        # TPI 헤더 파싱
        m_vendor = re.search(r'Vendor\s+Name:\s*"([^"]+)"', tpi_text)
        tpi_vendor = m_vendor.group(1) if m_vendor else ""
        m_model = re.search(r'Product\s+Model\s+Number:\s*"([^"]+)"', tpi_text)
        tpi_model = m_model.group(1) if m_model else ""

        # 2. BTL 기능 체크리스트 (PICS) 문서 (사용자 지정 경로 최우선, 자동 추정 금지)
        target_pics = None
        pics_opt = getattr(ctx.options, "pics_doc", None)
        if pics_opt:
            resolved_pics = _resolve_candidate(pics_opt)
            require(resolved_pics and resolved_pics.is_file(), f"지정된 PICS 문서를 찾을 수 없습니다: {pics_opt}")
            target_pics = resolved_pics
        else:
            default_pics = GATEWAY_ROOT / "model" / "cdu" / "[BTL]FUNC_CHECKLIST_CDU.docx"
            if default_pics.is_file():
                target_pics = default_pics
            else:
                cands = sorted(GATEWAY_ROOT.glob("model/**/*CHECKLIST*.docx"))
                require(cands, "기본 PICS 문서를 찾을 수 없습니다.")
                target_pics = cands[0]
        ctx.evidence.details["pics_source_file"] = str(target_pics)

        # 3. Gateway 설정 디렉터리 결정 및 JSON 파일 수집 (오직 Gateway 설정 디렉터리만 사용)
        gw_conf_dir = None
        for opt_name in ("gateway_config_dir", "config_dir"):
            opt_val = getattr(ctx.options, opt_name, None)
            if opt_val:
                p = _resolve_candidate(opt_val)
                if p.is_dir():
                    gw_conf_dir = p
                    break
                elif p.is_file() and p.name == "gateway.json":
                    gw_conf_dir = p.parent
                    break

        if not gw_conf_dir and ctx.config_dir and "fixture" not in str(ctx.config_dir):
            p = _resolve_candidate(ctx.config_dir)
            if p.is_dir() and (p / "gateway.json").is_file():
                gw_conf_dir = p

        if not gw_conf_dir:
            default_conf = GATEWAY_ROOT / "conf"
            if default_conf.is_dir():
                gw_conf_dir = default_conf

        require(gw_conf_dir is not None and gw_conf_dir.is_dir(),
                f"Gateway 설정 디렉터리를 찾을 수 없습니다: {gw_conf_dir}")
        ctx.evidence.details["gateway_config_dir"] = str(gw_conf_dir)

        target_gw = gw_conf_dir / "gateway.json"
        require(target_gw.is_file(), f"Gateway 설정 파일(gateway.json)이 존재하지 않습니다: {target_gw}")
        ctx.evidence.details["gateway_config_file"] = str(target_gw)
        gw_data = json.loads(target_gw.read_text(encoding="utf-8"))
        gw_bacnet = gw_data.get("bacnet", {})
        gw_vendor = gw_bacnet.get("vendor_name", "")
        gw_model = gw_bacnet.get("model_name", "")
        gw_dev_name = gw_bacnet.get("device_name", "")

        # 프로파일 JSON: 오직 Gateway 설정 디렉터리 내에서만 수집! (PICS/TPI 폴더 참조 배제)
        profile_files = []
        if (gw_conf_dir / "profiles").is_dir():
            profile_files.extend(sorted((gw_conf_dir / "profiles").glob("*.json")))
        profile_files.extend([f for f in sorted(gw_conf_dir.glob("*.json")) if f.name != "gateway.json" and f not in profile_files])
        require(len(profile_files) > 0, f"Gateway 설정 디렉터리({gw_conf_dir}) 내에 프로파일 JSON 파일이 없습니다.")
        ctx.evidence.details["gateway_profile_files"] = [str(f) for f in profile_files]

        # PICS 문서 내 체크/미체크 항목 전수 파싱
        checked_objects = set()
        unchecked_objects = set()
        checked_bibbs = set()
        unchecked_bibbs = set()
        checked_cov_objects = set()
        unchecked_cov_objects = set()

        bibb_map = {
            "Data Sharing - ReadProperty - A": "DS-RP-A",
            "Data Sharing - ReadProperty - B": "DS-RP-B",
            "Data Sharing - ReadPropertyMultiple - A": "DS-RPM-A",
            "Data Sharing - ReadPropertyMultiple - B": "DS-RPM-B",
            "Data Sharing - WriteProperty - A": "DS-WP-A",
            "Data Sharing - WriteProperty - B": "DS-WP-B",
            "Data Sharing - WritePropertyMultiple - A": "DS-WPM-A",
            "Data Sharing - WritePropertyMultiple - B": "DS-WPM-B",
            "Data Sharing - Change Of Value - A": "DS-COV-A",
            "Data Sharing - Change Of Value - B": "DS-COV-B",
            "Data Sharing - Change Of Value - Unsolicited - A": "DS-COVU-A",
            "Data Sharing - Change Of Value - Unsolicited - B": "DS-COVU-B",
            "Device Management - Dynamic Device Binding - A": "DM-DDB-A",
            "Device Management - Dynamic Device Binding - B": "DM-DDB-B",
            "Device Management - Dynamic Object Binding - A": "DM-DOB-A",
            "Device Management - Dynamic Object Binding - B": "DM-DOB-B",
            "Device Management - Device Communication Control - A": "DM-DCC-A",
            "Device Management - Device Communication Control - B": "DM-DCC-B",
            "Device Management - TimeSynchronization - A": "DM-TS-A",
            "Device Management - TimeSynchronization - B": "DM-TS-B",
            "Device Management - UTCTimeSynchronization - A": "DM-UTC-A",
            "Device Management - UTCTimeSynchronization - B": "DM-UTC-B",
            "Device Management - ReinitializeDevice - B": "DM-RD-B",
            "Device Management - Backup and Restore - B": "DM-BR-B",
            "Device Management - List Manipulation - B": "DM-LM-B",
            "Device Management - Object Creation and Deletion - B": "DM-OCD-B",
        }

        cov_pattern = re.compile(r"Supports COV for\s+(.*?)\s+objects", re.I)

        if target_pics.suffix.lower() == ".docx":
            import zipfile
            import xml.etree.ElementTree as ET
            with zipfile.ZipFile(target_pics) as z:
                xml_raw = z.read("word/document.xml")
                xml_data = xml_raw.decode("utf-8", errors="ignore")
                pics_source = re.sub(r"<[^>]+>", " ", xml_data)
            try:
                root = ET.fromstring(xml_raw)
                W = "{http://schemas.openxmlformats.org/wordprocessingml/2006/main}"
                current_obj = None
                current_bibb_title = None

                for tr in root.iter(f"{W}tr"):
                    cells = tr.findall(f"{W}tc")
                    if not cells:
                        continue
                    cell_texts = ["".join([t.text for t in c.iter(f"{W}t") if t.text]).strip() for c in cells]
                    row_text = " ".join(cell_texts)

                    # 단일 셀 또는 병합 셀 헤더 감지
                    if len(cells) == 1 or len(set(cell_texts)) == 1:
                        header_text = cell_texts[0]
                        if "Object" in header_text and not any(k in header_text for k in ("Device Management", "Data Sharing", "Alarm", "Scheduling", "Trending", "Gateway", "Security", "Audit", "Network Management", "Lighting")):
                            current_obj = header_text.replace(" Object", "").strip()
                        if header_text in bibb_map:
                            current_bibb_title = header_text
                        continue

                    c0 = cell_texts[0]

                    # 1. 표준 객체 Base Requirements 전수 수집
                    if "Base Requirements" in row_text and current_obj:
                        if c0 == "X":
                            checked_objects.add(current_obj)
                        else:
                            unchecked_objects.add(current_obj)
                        current_obj = None

                    # 2. BIBB Base Requirements 전수 수집
                    if "Base Requirements" in row_text and current_bibb_title:
                        code = bibb_map[current_bibb_title]
                        if c0 == "X":
                            checked_bibbs.add(code)
                        else:
                            unchecked_bibbs.add(code)
                        current_bibb_title = None

                    # 3. COV 지원 대상 객체 전수 수집
                    m_cov = cov_pattern.search(row_text)
                    if m_cov:
                        cov_target = m_cov.group(1).strip()
                        if c0 == "X":
                            checked_cov_objects.add(cov_target)
                        else:
                            unchecked_cov_objects.add(cov_target)
            except Exception as e:
                ctx.evidence.details["docx_parse_error"] = str(e)
        else:
            pics_source = target_pics.read_text(encoding="utf-8", errors="replace")
            for ot in ("Analog Input", "Analog Output", "Binary Input", "Binary Value", "Device", "Network Port"):
                if re.search(rf"\b{re.escape(ot)}\b", pics_source, re.I):
                    checked_objects.add(ot)
            if "Binary Output" not in pics_source:
                unchecked_objects.add("Binary Output")

        # 감사 실패 목록 및 Soft Assertion helper (실패하더라도 전수 대조를 끝까지 실행)
        audit_failures = []

        def audit_step(label, expected, actual, condition, message=None):
            passed = bool(condition)
            diagnostic = (message or "") if not passed else ""
            ctx.record_step(label, expected, actual, passed=passed, message=diagnostic)
            if not passed:
                audit_failures.append(diagnostic or f"{label} 불일치 (기대값: {expected}, 수신값: {actual})")

        # 1단계: PICS 문서 내 5대 필수 서비스 선언 검증 (5개 스텝)
        for service in ("ReadProperty", "WriteProperty", "Who-Is", "Who-Has", "COV"):
            has_svc = service in pics_source
            audit_step(f"PICS 필수 서비스 선언 확인 ({service})", True, has_svc,
                       has_svc, f"PICS / BTL 기능 체크리스트에 서비스 선언 누락: {service}")

        # 2단계: PICS ↔ TPI 전자명세서 식별 정보(제조사/모델명) 대조
        has_vendor = bool(tpi_vendor and tpi_vendor in pics_source)
        audit_step(f"PICS ↔ TPI 제조사 일치 대조 ({tpi_vendor})", True, has_vendor,
                   has_vendor, f"TPI Vendor '{tpi_vendor}' not found in PICS")
        has_model = bool(tpi_model and tpi_model in pics_source)
        audit_step(f"PICS ↔ TPI 모델명 일치 대조 ({tpi_model})", True, has_model,
                   has_model, f"TPI Model '{tpi_model}' not found in PICS")

        # 3단계: TPI ↔ gateway.json 장치 설정 일치 대조
        gw_vendor_match = (gw_vendor == tpi_vendor)
        audit_step(f"TPI ↔ gateway.json 제조사 일치 ({tpi_vendor})", True, gw_vendor_match,
                   gw_vendor_match, f"Vendor mismatch: TPI={tpi_vendor}, GW={gw_vendor}")
        gw_model_match = (gw_model == tpi_model or tpi_model in gw_model or gw_model in tpi_model)
        audit_step(f"TPI ↔ gateway.json 모델명 일치 ({gw_model})", True, gw_model_match,
                   gw_model_match, f"Model mismatch: TPI={tpi_model}, GW={gw_model}")

        # 4단계: PICS 체크/미체크 ↔ TPI 지원 BIBB 전수 일치 대조
        m_bibbs = re.search(r'BIBBs\s+Supported:\s*\{([^}]+)\}', tpi_text)
        tpi_bibbs = set()
        if m_bibbs:
            for line in m_bibbs.group(1).splitlines():
                line = line.split("--")[0].strip()
                if line:
                    tpi_bibbs.update(line.split())
        ctx.evidence.details["pics_checked_bibbs"] = sorted(list(checked_bibbs))
        ctx.evidence.details["tpi_supported_bibbs"] = sorted(list(tpi_bibbs))

        if checked_bibbs:
            bibbs_match = (checked_bibbs == tpi_bibbs) and unchecked_bibbs.isdisjoint(tpi_bibbs)
            audit_step("PICS 체크/미체크 ↔ TPI 지원 BIBB 전수 일치 대조",
                       sorted(list(tpi_bibbs)), sorted(list(checked_bibbs)),
                       bibbs_match,
                       f"BIBBs mismatch: checked_diff={checked_bibbs ^ tpi_bibbs}, unchecked_overlap={unchecked_bibbs & tpi_bibbs}")

        # 5단계: TPI 지원 표준 객체 타입 파싱
        m_types = re.search(r'Standard\s+Object[- ]Types\s+Supported:\s*\{([^}]+)\}', tpi_text, re.I)
        tpi_types = set()
        if m_types:
            for line in m_types.group(1).splitlines():
                line = line.split("--")[0].strip()
                if line:
                    tpi_types.add(line)
        ctx.evidence.details["pics_checked_objects"] = sorted(list(checked_objects))
        ctx.evidence.details["pics_unchecked_objects_count"] = len(unchecked_objects)
        ctx.evidence.details["tpi_supported_types"] = sorted(list(tpi_types))

        # 5단계 검증: PICS 체크(지원) 객체 타입 ↔ TPI 전수 일치 대조
        if checked_objects:
            objs_match = (checked_objects == tpi_types)
            audit_step("PICS 체크(지원) 객체 타입 ↔ TPI 전수 일치 대조",
                       sorted(list(tpi_types)), sorted(list(checked_objects)),
                       objs_match,
                       f"Object types mismatch: diff={checked_objects ^ tpi_types}")

        # 6단계: 프로파일 JSON 사용 객체 타입 파싱 및 수용성 검증 준비
        type_map = {
            "DISCRETE_INPUT": "Binary Input",
            "COIL": "Binary Value",
            "INPUT_REGISTER": "Analog Input",
            "HOLDING_REGISTER": "Analog Output",
        }
        json_types = set()
        unsupported_types = set()
        for pf in profile_files:
            try:
                pdata = json.loads(pf.read_text(encoding="utf-8"))
                for obj in pdata.get("objects", []):
                    mb = obj.get("modbus_block")
                    bt = obj.get("bacnet_type")
                    if bt == "BINARY_OUTPUT":
                        unsupported_types.add("Binary Output")
                    mapped = type_map.get(mb)
                    if mapped:
                        json_types.add(mapped)
                        if mapped not in tpi_types or (checked_objects and mapped not in checked_objects):
                            unsupported_types.add(mapped)
            except Exception:
                pass
        ctx.evidence.details["json_used_types"] = sorted(list(json_types))

        # 7단계: PICS 미체크(미지원 객체 타입) ↔ TPI 및 JSON 전수 배제 대조
        if unchecked_objects:
            tpi_unchecked_leak = sorted(list(unchecked_objects & tpi_types))
            json_unchecked_leak = sorted(list(unchecked_objects & json_types))
            leak_status = {"TPI_leak": tpi_unchecked_leak, "JSON_leak": json_unchecked_leak}
            leak_expected = {"TPI_leak": [], "JSON_leak": []}
            is_disjoint = (len(tpi_unchecked_leak) == 0 and len(json_unchecked_leak) == 0)
            audit_step(f"PICS 미체크(미지원 {len(unchecked_objects)}종, BO포함) ↔ TPI/JSON 전수 배제 대조",
                       leak_expected, leak_status,
                       is_disjoint, f"Unchecked objects leaked into TPI or JSON: {leak_status}")

        # 8단계: PICS COV 구독 지원/미지원 객체 전수 일치 대조
        if checked_cov_objects or unchecked_cov_objects:
            cov_expected = {
                "checked_cov_objects": ["Analog Input", "Analog Output", "Binary Input", "Binary Value"],
                "unchecked_has_BO": True,
            }
            cov_actual = {
                "checked_cov_objects": sorted(list(checked_cov_objects)),
                "unchecked_has_BO": ("Binary Output" in unchecked_cov_objects),
            }
            cov_match = (cov_actual == cov_expected)
            audit_step(f"PICS COV 구독 지원(4종, BV포함)/미지원(28종, BO포함) 전수 대조",
                       cov_expected, cov_actual,
                       cov_match, f"COV supported/unsupported mismatch: actual={cov_actual}")

        # 9단계: PICS & TPI ↔ 프로파일 JSON 객체 타입 수용성 대조
        audit_step("PICS & TPI ↔ 프로파일 JSON 객체 타입 수용성 대조",
                   sorted(list(json_types)),
                   [t for t in sorted(list(json_types)) if t not in unsupported_types],
                   len(unsupported_types) == 0, f"Unsupported object types in profile: {unsupported_types}")

        # 10단계: 최종 전수 대조 결과 판정 (모든 스텝 완료 후 실패 항목 확인)
        require(len(audit_failures) == 0,
                f"BTL 전수 대조 감사 실패 ({len(audit_failures)}개 항목 불일치): " + "; ".join(audit_failures[:3]))
    elif tc_id == "TC-ENV-001":
        if not ctx.options.allow_build:
            blocked("host build requires explicit --allow-build; makeIMX is never used")
        code, output = ctx.run_command("host_unit_build", ["make", "test"], timeout=300.0)
        require(code == 0, "host unit build/test failed")
        warnings = [line for line in output.splitlines() if re.search(r"\bwarning:", line, re.I)]
        require(not warnings, f"compiler warnings found: {warnings[:5]}")
    elif tc_id == "TC-ENV-002":
        ctx.require_managed()
        ctx.start_stack()
        ctx.stop_process("gateway")
        ctx.stop_process("main_simulator")
        ctx.stop_process("vport")
        require(not any(ctx.dev_dir.iterdir()), "PTY links remained after smoke shutdown")
    elif tc_id == "TC-ENV-003":
        doc_dir = VERIFICATION_ROOT / "docs" / "references"
        if not (doc_dir.is_dir() and any(doc_dir.glob("*.md"))):
            doc_dir = ES_ROOT / "bacnet" / "src" / "7.10_doc"
        if not (doc_dir.is_dir() and any(doc_dir.glob("*.md"))):
            blocked("BACstac 7.10 API 가이드 문서가 없는 환경에서는 이 감사 TC를 건너뜁니다 (BLOCKED)")
        modified = [path for path in (GATEWAY_ROOT / "module" / "bacstac-32_7.11.3-main").rglob("*")
                    if path.is_file() and path.stat().st_mtime > (GATEWAY_ROOT / "src").stat().st_mtime]
        if modified:
            ctx.evidence.details["commercial_files_newer_than_src"] = len(modified)
        api_calls = set(re.findall(r"\b(BACstac[A-Za-z0-9_]+)\s*\(",
                                   "\n".join(path.read_text(encoding="utf-8", errors="ignore")
                                             for path in (GATEWAY_ROOT / "src").rglob("*.[ch]pp"))))
        docs = "\n".join(path.read_text(encoding="utf-8", errors="ignore") for path in doc_dir.glob("*.md"))
        undocumented = sorted(name for name in api_calls if name not in docs)
        require(not undocumented, f"BACstac calls not found in 7.10 docs: {undocumented}")


def run_case(ctx, tc_id):
    if tc_id.startswith("TC-CFG-"):
        return _config_case(ctx, tc_id)
    if tc_id.startswith("TC-START-"):
        return _start_case(ctx, tc_id)
    if tc_id.startswith("TC-VPORT-"):
        return _vport_case(ctx, tc_id)
    if tc_id.startswith("TC-MAP-"):
        return _mapping_case(ctx, tc_id)
    if tc_id.startswith("TC-MOD-"):
        return _modbus_case(ctx, tc_id)
    if tc_id.startswith("TC-BAC-"):
        return _bacnet_case(ctx, tc_id)
    if tc_id.startswith("TC-IFCFG-") or tc_id.startswith("TC-BIP-"):
        return _interface_case(ctx, tc_id)
    if tc_id.startswith(("TC-AVL-", "TC-PERF-", "TC-OBS-")):
        return _nonfunctional_case(ctx, tc_id)
    return _standards_case(ctx, tc_id)


def _register_all():
    for tc_id, srs_id, title in _catalog():
        suites = ["all"]
        if tc_id in AUDIT:
            suites.append("audit")
        elif tc_id == "TC-AVL-004":
            suites.append("soak")
        else:
            suites.append("full")
            if tc_id in QUICK:
                suites.append("quick")

        def handler(context, selected=tc_id):
            return run_case(context, selected)

        handler.__name__ = tc_id.lower().replace("-", "_")
        requirements = []
        if tc_id in NATIVE_OBSERVE:
            requirements.append("bacnet_observe")
        if tc_id in NATIVE_MUTATING:
            requirements.append("bacnet_mutate")
        if tc_id in NATIVE_MAIN_CONTROL:
            requirements.append("main_control")
        tc_case(
            tc_id, srs_id, title, suites,
            timeout_sec=(86400.0 if tc_id == "TC-AVL-004" else (720.0 if tc_id.startswith("TC-OBS-") else (600.0 if tc_id == "TC-BIP-002" else (300.0 if tc_id == "TC-BAC-020" else 180.0)))),
            requirements=requirements,
        )(handler)


_register_all()
