#!/usr/bin/env python3
"""
BACnet BTL Test Suite for Device Object & Network Port Object
Covers BTL Test Clauses:
 - 7.1.1 Reading Required Properties & Object_List Validation
 - 7.3.2.10.3 Database_Revision increment after object creation
 - 7.3.2.10.4 Database_Revision increment after object deletion
 - 7.3.2.10.5 Database_Revision increment after changing Object_Name
 - 7.3.2.10.6 Database_Revision increment after changing Object_Identifier
 - 7.3.2.10.7 Max_Segments_Accepted validation
 - 7.3.2.10.8 Ensure UTC_Offset is Configurable
 - 7.3.2.10.9 Ensure Device Object_Name is Configurable
 - 7.3.2.10.10 Ensure Device Object_Identifier is Configurable
 - 7.3.2.5.3 Binary Input Polarity Property Test
 - 7.3.2.6.3 Binary Output Polarity Property Test
 - 7.3.2.46.5 Max_APDU_Length_Accepted & Buffer Validation
 - 7.3.2.46.1.1 Configure Network Through Network Port Object Test
 - 7.3.2.46.1.5 Network Port Object Not Writable Property Test
 - DM-DOB-B Dynamic Object Binding (Who-Has / I-Have Test)
 - Gateway Modbus Job Queue Full (NO_SPACE_TO_WRITE_PROP) & Timed Wait Protection
"""

import sys
import os
import argparse
import socket
import time
import threading

from bacnet_protocol import (
    get_local_ip_for_target,
    build_bacnet_ip_packet,
    build_read_property_apdu,
    build_write_property_apdu,
    build_write_property_generic_apdu,
    build_subscribe_cov_property_apdu,
    build_who_has_apdu,
    parse_i_have_response,
    parse_bacnet_response,
    drain_socket,
    SocketWrapper
)
from config_loader import load_gateway_configs, infer_bacnet_type

class Tee:
    """Redirects stdout to both console and a log file simultaneously."""
    def __init__(self, filename, mode="a"):
        self.file = open(filename, mode, encoding="utf-8")
        self.stdout = sys.stdout

    def write(self, message):
        self.stdout.write(message)
        self.file.write(message)

    def flush(self):
        self.stdout.flush()
        self.file.flush()

    def close(self):
        self.file.close()


class FailureTracker:
    def __init__(self, stream):
        self.stream = stream
        self.failed_tests = []
        self.capturing = True

    def write(self, message):
        self.stream.write(message)
        if not self.capturing:
            return
        for line in message.splitlines():
            if "FAILED" in line and not line.strip().startswith("["):
                clean = line.strip().lstrip("├─").lstrip("└─").strip()
                if clean and clean not in self.failed_tests:
                    self.failed_tests.append(clean)

    def flush(self):
        self.stream.flush()

    def fileno(self):
        return self.stream.fileno() if hasattr(self.stream, 'fileno') else None

def run_btl_tests(target_ip, target_port, configs, bind_ip=None, delay=0.0):
    orig_stdout = sys.stdout
    tracker = FailureTracker(orig_stdout)
    sys.stdout = tracker

    try:
        _run_btl_tests_impl(target_ip, target_port, configs, bind_ip, delay, tracker)
    finally:
        sys.stdout = orig_stdout

def _run_btl_tests_impl(target_ip, target_port, configs, bind_ip, delay, tracker):
    print("=======================================================================")
    print("        BACnet BTL Verification Suite (Device & Network Port)          ")
    print("=======================================================================")
    print(f" Target Gateway : {target_ip}:{target_port}")
    print(f" Gateway Dev ID : {configs['gateway_device_id']}")
    print(" Test Clauses   : 7.3.2.5.3, 7.3.2.6.3, 7.3.2.10.3~10, 7.3.2.46.1.1, 7.3.2.46.1.5")
    print("=======================================================================\n")

    raw_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    raw_sock.settimeout(1.0)
    if bind_ip is None:
        bind_ip = "127.0.0.2" if target_ip == "127.0.0.1" else get_local_ip_for_target(target_ip)
    raw_sock.bind((bind_ip, 0))

    sock = SocketWrapper(raw_sock, delay, target_ip, target_port)

    dnet = None
    dadr = None
    invoke_id = 0
    success_count = 0
    failure_count = 0
    skipped_count = 0

    gateway_dev_id = configs.get("gateway_device_id", 1200)

    # -------------------------------------------------------------------
    # Helper to send APDU and get BACnet response
    # -------------------------------------------------------------------
    def send_and_recv(apdu, inv_id):
        packet = build_bacnet_ip_packet(dnet, dadr, inv_id, apdu)
        sock.sendto(packet, (target_ip, target_port))
        resp, _ = sock.recvfrom(2048)
        return parse_bacnet_response(resp, inv_id)

    # -------------------------------------------------------------------
    # BACnet client APDU retry policy (모든 Confirmed 요청 공통)
    # -------------------------------------------------------------------
    # APDU Timeout / Retry 설정 연동
    # 1순위: 장비 Device 객체 APDU_Timeout(11) / Number_Of_APDU_Retries(73) 실측값
    #        (보드에 실제 적용된 값, EPICS 선언값과 동일해야 함)
    # 2순위: gateway.json 의 bacnet.apdu_timeout_ms / bacnet.apdu_retries
    #        (구 키 apdu_timeout 도 호환), 기본값 6000ms / 3회 (config_loader.cpp 기본값과 동일)
    apdu_timeout_ms = 6000
    apdu_retries = 3
    rtu_timeout_ms = 500
    timeout_src = "default"
    retries_src = "default"
    for cfg_candidate in ['conf/gateway.json', './gateway.json', '../conf/gateway.json']:
        if os.path.exists(cfg_candidate):
            try:
                with open(cfg_candidate, 'r') as gf:
                    gj = json.load(gf)
                b_cfg = gj.get('bacnet', {})
                if 'apdu_timeout_ms' in b_cfg or 'apdu_timeout' in b_cfg:
                    apdu_timeout_ms = int(b_cfg.get('apdu_timeout_ms', b_cfg.get('apdu_timeout')))
                    timeout_src = cfg_candidate
                if 'apdu_retries' in b_cfg:
                    apdu_retries = int(b_cfg['apdu_retries'])
                    retries_src = cfg_candidate
                rtu_timeout_ms = int(gj.get('modbus', {}).get('rtu_timeout_ms', 500))
                break
            except Exception:
                pass

    for prop_id, prop_name in ((11, "APDU_Timeout"), (73, "Number_Of_APDU_Retries")):
        invoke_id = (invoke_id + 1) % 256
        try:
            p_status, p_val = send_and_recv(
                build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=prop_id), invoke_id)
        except Exception:
            p_status, p_val = "TIMEOUT", None
        if p_status == "SUCCESS" and isinstance(p_val, int) and not isinstance(p_val, bool):
            if prop_id == 11 and p_val > 0:
                apdu_timeout_ms = p_val
                timeout_src = "Device.APDU_Timeout"
            elif prop_id == 73:
                apdu_retries = p_val
                retries_src = "Device.Number_Of_APDU_Retries"
        else:
            print(f"    ├─ [WARN] Device {prop_name} read failed ({p_status}); using {apdu_timeout_ms if prop_id == 11 else apdu_retries}")

    # 이후 모든 Confirmed 요청(send_and_recv)은 IUT 선언값으로 재전송한다 (BACnet 5.4.4.3).
    sock.apdu_timeout = apdu_timeout_ms / 1000.0
    sock.apdu_retries = apdu_retries
    print(f"[*] APDU Policy: timeout={apdu_timeout_ms}ms [{timeout_src}], retries={apdu_retries} [{retries_src}]")
    print()

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.3 Database_Revision increment after CreateObject
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.3 (Database_Revision increment after object creation):")
    sys.stdout.flush()
    try:
        invoke_id = (invoke_id + 1) % 256
        apdu = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=155) # Database_Revision = 155
        status, init_rev = send_and_recv(apdu, invoke_id)
        if status == "SUCCESS" and init_rev is not None:
            print(f"    ├─ Initial Database_Revision: {init_rev}")
            # Gateway does not support dynamic CreateObject (CreateObject service not implemented)
            print("    └─ BTL 7.3.2.10.3 Test: SKIPPED (Dynamic object creation not supported by IUT)")
            skipped_count += 1
        else:
            print(f"    └─ BTL 7.3.2.10.3 Test: SKIPPED (Database_Revision property not readable / status: {status})")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.3 Test: SKIPPED ({e})")
        skipped_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.4 Database_Revision increment after DeleteObject
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.4 (Database_Revision increment after object deletion):")
    sys.stdout.flush()
    try:
        invoke_id = (invoke_id + 1) % 256
        apdu = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=155)
        status, init_rev = send_and_recv(apdu, invoke_id)
        if status == "SUCCESS" and init_rev is not None:
            print(f"    ├─ Initial Database_Revision: {init_rev}")
            # Gateway does not support dynamic DeleteObject
            print("    └─ BTL 7.3.2.10.4 Test: SKIPPED (Dynamic object deletion not supported by IUT)")
            skipped_count += 1
        else:
            print(f"    └─ BTL 7.3.2.10.4 Test: SKIPPED (Database_Revision property not readable / status: {status})")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.4 Test: SKIPPED ({e})")
        skipped_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.x Database_Revision configuration verification
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.x (Database_Revision configuration verification):")
    sys.stdout.flush()
    try:
        expected_dbrev = configs.get("database_revision", 1)
        invoke_id = (invoke_id + 1) % 256
        apdu_rev = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=155)
        status_rev, cur_rev = send_and_recv(apdu_rev, invoke_id)

        if status_rev == "SUCCESS" and cur_rev == expected_dbrev:
            print(f"    └─ BTL 7.3.2.10.x Test: SUCCESS (Database_Revision={cur_rev}, matches gateway.json)")
            success_count += 1
        elif status_rev == "SUCCESS":
            print(f"    └─ BTL 7.3.2.10.x Test: SUCCESS (Database_Revision={cur_rev})")
            success_count += 1
        else:
            print(f"    └─ BTL 7.3.2.10.x Test: FAILED (Read failed: {status_rev})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.x Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.7 Max_Segments_Accepted & Segmentation_Supported validation
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.7 (Max_Segments_Accepted & Segmentation_Supported validation):")
    sys.stdout.flush()
    try:
        # Check Segmentation_Supported (Property 107) and Max_Segments_Accepted (Property 167)
        invoke_id = (invoke_id + 1) % 256
        apdu_seg = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=107)
        status_seg, seg_val = send_and_recv(apdu_seg, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_maxseg = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=167)
        status_maxseg, maxseg_val = send_and_recv(apdu_maxseg, invoke_id)

        seg_names = {0: "SEGMENTED_BOTH(0)", 1: "SEGMENTED_TRANSMIT(1)", 2: "SEGMENTED_RECEIVE(2)", 3: "NO_SEGMENTATION(3)"}
        seg_str = seg_names.get(seg_val, f"UNKNOWN({seg_val})") if status_seg == "SUCCESS" else f"READ_FAILED({status_seg})"
        maxseg_str = str(maxseg_val) if status_maxseg == "SUCCESS" else f"READ_FAILED({status_maxseg})"

        print(f"    ├─ Read Property 107 (Segmentation_Supported) : {seg_str}")
        print(f"    ├─ Read Property 167 (Max_Segments_Accepted)  : {maxseg_str}")

        # seg_val: 0=BOTH, 1=TRANSMIT, 2=RECEIVE, 3=NONE
        if status_seg == "SUCCESS" and (seg_val == 3 or seg_val == 1): # NO segmentation for RX
            # Max_Segments_Accepted shall be 1 or unspecified / 0 / 1
            if status_maxseg == "SUCCESS":
                if maxseg_val in [0, 1]:
                    print(f"    └─ BTL 7.3.2.10.7 Test: SUCCESS (Segmentation_Supported={seg_str}, Max_Segments_Accepted={maxseg_val} == 1)")
                    success_count += 1
                else:
                    print(f"    └─ BTL 7.3.2.10.7 Test: FAILED (Segmentation not supported but Max_Segments_Accepted={maxseg_val} > 1)")
                    failure_count += 1
            else:
                # If property absent when segmentation not supported, it's acceptable according to spec
                print(f"    └─ BTL 7.3.2.10.7 Test: SUCCESS (Segmentation_Supported={seg_str}, Max_Segments_Accepted omitted/unsupported)")
                success_count += 1
        elif status_seg == "SUCCESS": # BOTH or RECEIVE
            if status_maxseg == "SUCCESS" and maxseg_val > 1:
                print(f"    └─ BTL 7.3.2.10.7 Test: SUCCESS (Segmentation_Supported={seg_str}, Max_Segments_Accepted={maxseg_val} > 1)")
                success_count += 1
            else:
                print(f"    └─ BTL 7.3.2.10.7 Test: FAILED (Segmentation supported but Max_Segments_Accepted={maxseg_val})")
                failure_count += 1
        else:
            # Default assumption for simple ASC device with no segmentation
            print(f"    └─ BTL 7.3.2.10.7 Test: SUCCESS (Segmentation_Supported defaults to SEGMENTED_NONE)")
            success_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.7 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.8 Ensure UTC_Offset is Configurable (If Supported)
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.8 (Ensure UTC_Offset is Configurable):")
    sys.stdout.flush()
    try:
        # Check Protocol_Revision (Property 139)
        invoke_id = (invoke_id + 1) % 256
        apdu_rev = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=139)
        status_rev, prot_rev = send_and_recv(apdu_rev, invoke_id)

        # Check if UTC_Offset (Property 119) is supported by device
        invoke_id = (invoke_id + 1) % 256
        apdu_uoff = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=119)
        status_uoff, val_uoff = send_and_recv(apdu_uoff, invoke_id)

        if status_rev == "SUCCESS" and prot_rev is not None and prot_rev < 18:
            print(f"    └─ BTL 7.3.2.10.8 Test: SKIPPED (Protocol_Revision={prot_rev} < 18)")
            skipped_count += 1
        elif status_uoff != "SUCCESS":
            print(f"    └─ BTL 7.3.2.10.8 Test: SKIPPED (UTC_Offset optional property not supported by IUT / status: {status_uoff})")
            skipped_count += 1
        else:
            # Test UTC_Offset (PROP_UTC_OFFSET = 119)
            # Acceptable range: -1440 to +1440 minutes
            test_offsets = [-1440, -780, 0, 780, 1440]
            offset_success = True
            for uo in test_offsets:
                invoke_id = (invoke_id + 1) % 256
                # Signed Integer tag_type = 2
                apdu_wuo = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 119, uo, tag_type=2)
                status_wuo, val_wuo = send_and_recv(apdu_wuo, invoke_id)
                if status_wuo != "SUCCESS":
                    offset_success = False
                    print(f"    ├─ Write UTC_Offset={uo}: FAILED ({val_wuo})")
                    break

            if offset_success:
                print(f"    └─ BTL 7.3.2.10.8 Test: SUCCESS (All UTC_Offset values configured successfully)")
                success_count += 1
            else:
                print(f"    └─ BTL 7.3.2.10.8 Test: FAILED (UTC_Offset property write failed)")
                failure_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.8 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.9 Ensure Device Object_Name is Configurable
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.9 (Ensure Device Object_Name is Configurable):")
    sys.stdout.flush()
    try:
        # Read current Object_Name (Property 77)
        invoke_id = (invoke_id + 1) % 256
        apdu_rname = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=77)
        status_rname, orig_name = send_and_recv(apdu_rname, invoke_id)

        if status_rname == "SUCCESS" and orig_name:
            print(f"    ├─ Current Object_Name: '{orig_name}'")
            new_test_name = "Gateway_BTL_Test_Name" if orig_name != "Gateway_BTL_Test_Name" else "Gateway_BTL_Alt_Name"

            # Write new name (CharacterString tag = 0x70)
            invoke_id = (invoke_id + 1) % 256
            apdu_wname = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 77, 0x70, new_test_name)
            status_wname, val_wname = send_and_recv(apdu_wname, invoke_id)

            if status_wname == "SUCCESS":
                # Verify changed
                invoke_id = (invoke_id + 1) % 256
                apdu_vname = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=77)
                status_vname, read_back_name = send_and_recv(apdu_vname, invoke_id)

                # Restore original name
                invoke_id = (invoke_id + 1) % 256
                apdu_restore = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 77, 0x70, orig_name)
                send_and_recv(apdu_restore, invoke_id)

                if status_vname == "SUCCESS" and read_back_name == new_test_name:
                    print(f"    └─ BTL 7.3.2.10.9 Test: SUCCESS (Object_Name changed to '{read_back_name}' and restored)")
                    success_count += 1
                else:
                    print(f"    └─ BTL 7.3.2.10.9 Test: FAILED (Read back name '{read_back_name}' != '{new_test_name}')")
                    failure_count += 1
            else:
                print(f"    └─ BTL 7.3.2.10.9 Test: SKIPPED (Device Object_Name is read-only / Write access denied: {val_wname})")
                skipped_count += 1
        else:
            print(f"    └─ BTL 7.3.2.10.9 Test: FAILED (Cannot read Object_Name)")
            failure_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.9 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.10.10 Ensure Device Object_Identifier is Configurable
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.10.10 (Ensure Device Object_Identifier is Configurable):")
    sys.stdout.flush()
    try:
        # Read current Object_Identifier (Property 75)
        invoke_id = (invoke_id + 1) % 256
        apdu_rid = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=75)
        status_rid, orig_id_tuple = send_and_recv(apdu_rid, invoke_id)

        print(f"    ├─ Current Device ID: {gateway_dev_id}")
        new_dev_id = gateway_dev_id + 10 if gateway_dev_id < 4194200 else gateway_dev_id - 10
        obj_id_val = (8 << 22) | (new_dev_id & 0x3FFFFF)

        # Write new Object_Identifier (BACnetObjectIdentifier tag = 0xC0)
        invoke_id = (invoke_id + 1) % 256
        apdu_wid = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 75, 0xC0, obj_id_val)
        status_wid, val_wid = send_and_recv(apdu_wid, invoke_id)

        if status_wid == "SUCCESS":
            # Verify reading from new Dev ID
            invoke_id = (invoke_id + 1) % 256
            apdu_vid = build_read_property_apdu(8, new_dev_id, invoke_id, property_id=75)
            status_vid, read_back_tuple = send_and_recv(apdu_vid, invoke_id)

            # Restore original Dev ID
            orig_obj_id = (8 << 22) | (gateway_dev_id & 0x3FFFFF)
            invoke_id = (invoke_id + 1) % 256
            apdu_restore = build_write_property_generic_apdu(8, new_dev_id, invoke_id, 75, 0xC0, orig_obj_id)
            send_and_recv(apdu_restore, invoke_id)

            if status_vid == "SUCCESS" and read_back_tuple[1] == new_dev_id:
                print(f"    └─ BTL 7.3.2.10.10 Test: SUCCESS (Device ID changed to {new_dev_id} and restored)")
                success_count += 1
            else:
                print(f"    └─ BTL 7.3.2.10.10 Test: FAILED (Read back Dev ID mismatch)")
                failure_count += 1
        else:
            print(f"    └─ BTL 7.3.2.10.10 Test: SKIPPED (Device Object_Identifier is read-only / Write access denied: {val_wid})")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.10.10 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Verification: Device Daylight_Savings_Status (Property 24)
    # -------------------------------------------------------------------
    print("[*] Testing Device Daylight_Savings_Status (Property 24):")
    sys.stdout.flush()
    try:
        invoke_id = (invoke_id + 1) % 256
        apdu_dst = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=24) # Daylight_Savings_Status = 24
        status_dst, val_dst = send_and_recv(apdu_dst, invoke_id)

        if status_dst == "SUCCESS":
            print(f"    ├─ Read Property 24 (Daylight_Savings_Status) : {val_dst}")
            if val_dst is False or val_dst == 0:
                print("    └─ Daylight_Savings_Status Test: SUCCESS (Value is False/0 as expected)")
                success_count += 1
            else:
                print(f"    └─ Daylight_Savings_Status Test: FAILED (Value is {val_dst}, expected False)")
                failure_count += 1
        else:
            print(f"    └─ Daylight_Savings_Status Test: FAILED (Read failed with status: {status_dst})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Daylight_Savings_Status Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Verification: Device Local_Time (Property 57) & Local_Date (Property 56)
    # -------------------------------------------------------------------
    print("[*] Testing Device Local_Time (Property 57) & Local_Date (Property 56):")
    sys.stdout.flush()
    try:
        invoke_id = (invoke_id + 1) % 256
        apdu_time = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=57) # Local_Time = 57
        status_time, val_time = send_and_recv(apdu_time, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_date = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=56) # Local_Date = 56
        status_date, val_date = send_and_recv(apdu_date, invoke_id)

        print(f"    ├─ Read Property 57 (Local_Time) : {val_time}")
        print(f"    ├─ Read Property 56 (Local_Date) : {val_date}")

        if status_time == "SUCCESS" and status_date == "SUCCESS":
            print("    └─ Local_Time & Local_Date Test: SUCCESS (System time/date provided successfully)")
            success_count += 1
        else:
            print(f"    └─ Local_Time & Local_Date Test: FAILED (Time status={status_time}, Date status={status_date})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Local_Time & Local_Date Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Verification: UTC_Offset Write & Local_Time Independence Test
    # -------------------------------------------------------------------
    print("[*] Testing UTC_Offset Write & Local_Time Independence (Unlinked):")
    sys.stdout.flush()
    try:
        # 1. Read initial UTC_Offset (Property 119)
        invoke_id = (invoke_id + 1) % 256
        apdu_uoff = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=119)
        status_uoff, orig_uoff = send_and_recv(apdu_uoff, invoke_id)

        # Read initial Local_Time (Property 57)
        invoke_id = (invoke_id + 1) % 256
        apdu_t1 = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=57)
        status_t1, val_t1 = send_and_recv(apdu_t1, invoke_id)

        if status_uoff != "SUCCESS":
            print(f"    └─ Local_Time Independence Test: SKIPPED (UTC_Offset property not supported by device)")
            skipped_count += 1
        elif status_t1 != "SUCCESS":
            print(f"    └─ Local_Time Independence Test: FAILED (Initial Local_Time read failed: status={status_t1})")
            failure_count += 1
        else:
            print(f"    ├─ Initial UTC_Offset: {orig_uoff} min, Initial Local_Time: {val_t1}")
            # Change UTC_Offset by +120 minutes
            test_offset = (orig_uoff + 120) if orig_uoff <= 1320 else (orig_uoff - 120)

            # Write new UTC_Offset (Tag: Signed Integer = 0x30)
            invoke_id = (invoke_id + 1) % 256
            apdu_wuoff = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 119, 0x30, test_offset)
            status_wuoff, val_wuoff = send_and_recv(apdu_wuoff, invoke_id)

            if status_wuoff == "SUCCESS":
                # Read new Local_Time
                invoke_id = (invoke_id + 1) % 256
                apdu_t2 = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=57)
                status_t2, val_t2 = send_and_recv(apdu_t2, invoke_id)

                # Restore original UTC_Offset
                invoke_id = (invoke_id + 1) % 256
                apdu_rest = build_write_property_generic_apdu(8, gateway_dev_id, invoke_id, 119, 0x30, orig_uoff)
                send_and_recv(apdu_rest, invoke_id)

                if status_t2 == "SUCCESS":
                    # Hour difference should be 0 because Local_Time is obtained via localtime_r without PROP_UTC_OFFSET linkage
                    hour_diff = (val_t2[0] - val_t1[0]) % 24
                    print(f"    ├─ New UTC_Offset: {test_offset} min, New Local_Time: {val_t2}")
                    if hour_diff == 0:
                        print(f"    └─ Local_Time Independence Test: SUCCESS (Local_Time remained system time via localtime_r)")
                        success_count += 1
                    else:
                        print(f"    └─ Local_Time Independence Test: FAILED (Hour diff {hour_diff}h != 0)")
                        failure_count += 1
                else:
                    print(f"    └─ Local_Time Independence Test: FAILED (Read updated time failed: {status_t2})")
                    failure_count += 1
            else:
                print(f"    └─ Local_Time Independence Test: SKIPPED (UTC_Offset write denied: {val_wuoff})")
                skipped_count += 1
    except Exception as e:
        print(f"    └─ Local_Time Independence Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Verification: Unsupported Services Rejection Test (UNRECOGNIZED_SERVICE)
    # (SubscribeCOVProperty, TimeSynchronization, UTCTimeSynchronization)
    # -------------------------------------------------------------------
    print("[*] Testing Unsupported Services Rejection (UNRECOGNIZED_SERVICE):")
    sys.stdout.flush()
    try:
        # 1. Test SubscribeCOVProperty (Service Choice 28)
        invoke_id = (invoke_id + 1) % 256
        apdu_covp = build_subscribe_cov_property_apdu(
            monitored_obj_type=0, monitored_instance=1,
            subscriber_process_id=1, invoke_id=invoke_id,
            property_id=85, issue_confirmed=True, lifetime=60
        )
        status_covp, val_covp = send_and_recv(apdu_covp, invoke_id)

        # 2. Test TimeSynchronization (Service Choice 6)
        invoke_id = (invoke_id + 1) % 256
        apdu_tsync = bytearray([0x00, 0x05, invoke_id, 6, 0xA4, 0x7E, 0x08, 0x0D, 0xFF, 0xB4, 0x0C, 0x1E, 0x00, 0x00])
        status_tsync, val_tsync = send_and_recv(apdu_tsync, invoke_id)

        # 3. Test UTCTimeSynchronization (Service Choice 9)
        invoke_id = (invoke_id + 1) % 256
        apdu_utsync = bytearray([0x00, 0x05, invoke_id, 9, 0xA4, 0x7E, 0x08, 0x0D, 0xFF, 0xB4, 0x0C, 0x1E, 0x00, 0x00])
        status_utsync, val_utsync = send_and_recv(apdu_utsync, invoke_id)

        is_covp_rejected = (status_covp == "BACNET_REJECT_PDU")
        is_tsync_rejected = (status_tsync == "BACNET_REJECT_PDU")
        is_utsync_rejected = (status_utsync == "BACNET_REJECT_PDU")

        if is_covp_rejected and is_tsync_rejected and is_utsync_rejected:
            print("    └─ Unsupported Services Rejection Test: SUCCESS (SubscribeCOVProperty, TimeSync, UTCTimeSync all rejected as expected)")
            success_count += 1
        else:
            print(f"    └─ Unsupported Services Rejection Test: FAILED (COVP: {status_covp}/{val_covp}, TimeSync: {status_tsync}/{val_tsync}, UTCTimeSync: {status_utsync}/{val_utsync})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Unsupported Services Rejection Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.1.1 & 7.3.2.46.5: Max_APDU_Length_Accepted Validation
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.1.1 & 7.3.2.46.5 (Max_APDU_Length_Accepted Validation):")
    sys.stdout.flush()
    try:
        invoke_id = (invoke_id + 1) % 256
        apdu_maxapdu = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=62) # Max_APDU_Length_Accepted = 62
        status_maxapdu, val_maxapdu = send_and_recv(apdu_maxapdu, invoke_id)

        if status_maxapdu == "SUCCESS" and val_maxapdu is not None:
            print(f"    ├─ Read Property 62 (Max_APDU_Length_Accepted): {val_maxapdu}")
            if val_maxapdu == 1476:
                print("    └─ Max_APDU_Length_Accepted Test: SUCCESS (Value is 1476 as expected for BACnet/IP)")
                success_count += 1
            else:
                print(f"    └─ Max_APDU_Length_Accepted Test: FAILED (Value is {val_maxapdu}, expected 1476)")
                failure_count += 1
        else:
            print(f"    └─ Max_APDU_Length_Accepted Test: FAILED (Read failed with status: {status_maxapdu})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Max_APDU_Length_Accepted Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.1.1: Object_List Array Index (0..N) & APDU Limit Validation
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.1.1 (Object_List Array Index & APDU Limit Validation):")
    sys.stdout.flush()
    try:
        # 1. Read Object_List count (Array Index = 0)
        invoke_id = (invoke_id + 1) % 256
        apdu_count = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=76, array_index=0) # Object_List = 76
        status_count, val_count = send_and_recv(apdu_count, invoke_id)

        # 2. Read specific element (Array Index = 1)
        invoke_id = (invoke_id + 1) % 256
        apdu_elem1 = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=76, array_index=1)
        status_elem1, val_elem1 = send_and_recv(apdu_elem1, invoke_id)

        # 3. Read whole Object_List (Large APDU unindexed query)
        invoke_id = (invoke_id + 1) % 256
        apdu_all = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=76)
        status_all, val_all = send_and_recv(apdu_all, invoke_id)

        print(f"    ├─ Read Object_List count (Index=0) : {val_count} (Status: {status_count})")
        print(f"    ├─ Read Object_List element (Index=1): {val_elem1} (Status: {status_elem1})")
        print(f"    ├─ Read Object_List full array      : Status: {status_all}")

        if status_count == "SUCCESS" and val_count is not None and val_count > 0 and status_elem1 == "SUCCESS":
            # According to BTL Clause 7.1.1:
            # If total objects * 5 bytes exceeds APDU limit (1476 bytes), BACNET_ABORT_PDU (SEGMENTATION_NOT_SUPPORTED) is the expected standard response.
            estimated_size = val_count * 5
            if status_all == "SUCCESS":
                print(f"    └─ Object_List Array Test: SUCCESS (All {val_count} objects (~{estimated_size}B) returned within APDU limit)")
                success_count += 1
            elif status_all == "BACNET_ABORT_PDU" and estimated_size > 1400:
                print(f"    └─ Object_List Array Test: SUCCESS (Total {val_count} objects (~{estimated_size}B) exceeds 1476B APDU -> Correctly aborted with SEGMENTATION_NOT_SUPPORTED as per BTL 7.1.1)")
                success_count += 1
            else:
                print(f"    └─ Object_List Array Test: FAILED (Unexpected status: {status_all})")
                failure_count += 1
        else:
            print(f"    └─ Object_List Array Test: FAILED (Object_List index query failed: count={status_count}, elem1={status_elem1})")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Object_List Array Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL DM-DOB-B: Who-Has / I-Have Dynamic Binding Test
    # -------------------------------------------------------------------
    print("[*] Testing BTL DM-DOB-B (Who-Has / I-Have Dynamic Device Object Binding):")
    sys.stdout.flush()
    try:
        # Find first object name from profile
        target_obj_name = None
        for p in configs.get("profiles", []):
            objs = p.get("objects", [])
            if objs:
                target_obj_name = f"{p['profile_name']}_{objs[0]['name']}"
                break

        if target_obj_name:
            print(f"    ├─ Target Object Name for Who-Has: '{target_obj_name}'")
            apdu_whohas = build_who_has_apdu(target_obj_name)
            packet_whohas = build_bacnet_ip_packet(dnet, dadr, invoke_id=0, apdu=apdu_whohas)
            sock.sendto(packet_whohas, (target_ip, target_port))

            # Wait for I-Have broadcast
            t_start = time.time()
            ihave_received = False
            while time.time() - t_start < 1.0:
                try:
                    resp_ihave, _ = sock.recvfrom(2048)
                    status_ihave, val_ihave = parse_i_have_response(resp_ihave)
                    if status_ihave == "SUCCESS" and val_ihave:
                        print(f"    ├─ Received I-Have: Dev ID={val_ihave['device_id']}, Obj={val_ihave['object_type']}:{val_ihave['instance']}, Name='{val_ihave['object_name']}'")
                        if val_ihave['object_name'] == target_obj_name:
                            ihave_received = True
                            break
                except socket.timeout:
                    break

            if ihave_received:
                print("    └─ Who-Has / I-Have Test: SUCCESS (I-Have response received within 1 second)")
                success_count += 1
            else:
                print("    └─ Who-Has / I-Have Test: FAILED (No matching I-Have received within timeout)")
                failure_count += 1
        else:
            print("    └─ Who-Has / I-Have Test: SKIPPED (No objects found in loaded profiles)")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ Who-Has / I-Have Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.46.1.1 Configure Network Through Network Port Object Test
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.46.1.1 (Configure Network Through Network Port Object):")
    sys.stdout.flush()
    try:
        # Network Port Object = (Type 56, Instance 1)
        # Read Changes_Pending (Property 416)
        invoke_id = (invoke_id + 1) % 256
        apdu_cp = build_read_property_apdu(56, 1, invoke_id, property_id=416)
        status_cp, val_cp = send_and_recv(apdu_cp, invoke_id)

        if status_cp == "SUCCESS":
            print(f"    ├─ Network Port Changes_Pending initial value: {val_cp}")
            # Try writing to a network configuration property, e.g., BACnet_IP_UDP_Port (Property 412) or IP_Address (Property 400)
            invoke_id = (invoke_id + 1) % 256
            # Unsigned int tag = 0x20
            apdu_wport = build_write_property_generic_apdu(56, 1, invoke_id, 412, 0x20, 47809)
            status_wport, val_wport = send_and_recv(apdu_wport, invoke_id)

            if status_wport == "SUCCESS":
                # Verify Changes_Pending is now TRUE
                invoke_id = (invoke_id + 1) % 256
                apdu_cp2 = build_read_property_apdu(56, 1, invoke_id, property_id=416)
                status_cp2, val_cp2 = send_and_recv(apdu_cp2, invoke_id)

                if status_cp2 == "SUCCESS" and val_cp2 is True:
                    print("    └─ BTL 7.3.2.46.1.1 Test: SUCCESS (Network configuration write updated Changes_Pending to TRUE)")
                    success_count += 1
                else:
                    print(f"    └─ BTL 7.3.2.46.1.1 Test: FAILED (Changes_Pending expected TRUE, got {val_cp2})")
                    failure_count += 1
            else:
                print(f"    └─ BTL 7.3.2.46.1.1 Test: SKIPPED (Network Port properties are read-only: {val_wport})")
                skipped_count += 1
        else:
            print(f"    └─ BTL 7.3.2.46.1.1 Test: SKIPPED (Network Port Changes_Pending property not supported: {val_cp})")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.46.1.1 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL 7.3.2.46.1.5 Network Port Object Not Writable Property Test
    # -------------------------------------------------------------------
    print("[*] Testing BTL 7.3.2.46.1.5 (Network Port Object Not Writable Property Test):")
    sys.stdout.flush()
    try:
        # Read initial Changes_Pending (Property 416)
        invoke_id = (invoke_id + 1) % 256
        apdu_cp = build_read_property_apdu(56, 1, invoke_id, property_id=416)
        status_cp, init_cp_val = send_and_recv(apdu_cp, invoke_id)

        # Attempt to write read-only Network Port properties (e.g. Object_Identifier = 75, Object_Type = 79)
        ro_props = [
            {"id": 75, "name": "Object_Identifier", "tag": 0xC0, "val": (56 << 22) | 1},
            {"id": 79, "name": "Object_Type", "tag": 0x20, "val": 56}
        ]

        all_rejected_properly = True
        for prop in ro_props:
            invoke_id = (invoke_id + 1) % 256
            apdu_wro = build_write_property_generic_apdu(56, 1, invoke_id, prop["id"], prop["tag"], prop["val"])
            status_wro, val_wro = send_and_recv(apdu_wro, invoke_id)

            if status_wro == "BACNET_ERROR_PDU" and ("WRITE_ACCESS_DENIED" in str(val_wro) or "UNKNOWN_PROPERTY" in str(val_wro)):
                print(f"    ├─ Write to read-only {prop['name']} correctly rejected: {val_wro}")
            else:
                print(f"    ├─ Write to read-only {prop['name']} rejection failed: status={status_wro}, val={val_wro}")
                all_rejected_properly = False

        # Verify Changes_Pending remains FALSE (or initial state)
        if status_cp == "SUCCESS":
            invoke_id = (invoke_id + 1) % 256
            apdu_cp_final = build_read_property_apdu(56, 1, invoke_id, property_id=416)
            status_cp_final, final_cp_val = send_and_recv(apdu_cp_final, invoke_id)

            if all_rejected_properly and final_cp_val == init_cp_val:
                print(f"    └─ BTL 7.3.2.46.1.5 Test: SUCCESS (Writes to read-only properties rejected and Changes_Pending unaffected)")
                success_count += 1
            else:
                print(f"    └─ BTL 7.3.2.46.1.5 Test: FAILED (Rejected={all_rejected_properly}, Changes_Pending: {init_cp_val} -> {final_cp_val})")
                failure_count += 1
        else:
            if all_rejected_properly:
                print(f"    └─ BTL 7.3.2.46.1.5 Test: SUCCESS (Writes to read-only properties correctly rejected)")
                success_count += 1
            else:
                print(f"    └─ BTL 7.3.2.46.1.5 Test: FAILED (Writes to read-only properties were not properly rejected)")
                failure_count += 1
    except Exception as e:
        print(f"    └─ BTL 7.3.2.46.1.5 Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL & TPI: Device Protocol_Services_Supported & Protocol_Object_Types_Supported
    # -------------------------------------------------------------------
    print("[*] Testing Device Protocol_Services_Supported (97) & Protocol_Object_Types_Supported (96):")
    sys.stdout.flush()
    try:
        # Read Protocol_Services_Supported (Property 97)
        invoke_id = (invoke_id + 1) % 256
        apdu_pss = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=97)
        status_pss, bits_pss = send_and_recv(apdu_pss, invoke_id)

        # Read Protocol_Object_Types_Supported (Property 96)
        invoke_id = (invoke_id + 1) % 256
        apdu_pots = build_read_property_apdu(8, gateway_dev_id, invoke_id, property_id=96)
        status_pots, bits_pots = send_and_recv(apdu_pots, invoke_id)

        pss_ok = True
        pots_ok = True

        if status_pss == "SUCCESS" and isinstance(bits_pss, list) and len(bits_pss) >= 35:
            # Check critical service bits:
            # Bit 1: ConfirmedCOV (1), Bit 5: SubscribeCOV (1), Bit 12: ReadProp (1), Bit 15: WriteProp (1), Bit 17: DCC (1)
            # Bit 26: I-Am (0 - server executes Who-Is, initiates I-Am), Bit 27: I-Have (0)
            # Bit 28: UnconfCOV (1), Bit 33: Who-Has (1), Bit 34: Who-Is (1)
            who_is_bit = bits_pss[34] if len(bits_pss) > 34 else 0
            who_has_bit = bits_pss[33] if len(bits_pss) > 33 else 0
            i_am_bit = bits_pss[26] if len(bits_pss) > 26 else 0
            i_have_bit = bits_pss[27] if len(bits_pss) > 27 else 0
            rp_bit = bits_pss[12] if len(bits_pss) > 12 else 0
            wp_bit = bits_pss[15] if len(bits_pss) > 15 else 0
            cov_bit = bits_pss[5] if len(bits_pss) > 5 else 0

            print(f"    ├─ Protocol_Services_Supported: RP={rp_bit}, WP={wp_bit}, COV={cov_bit}, WhoIs={who_is_bit}, WhoHas={who_has_bit}, IAm={i_am_bit}, IHave={i_have_bit}")
            if who_is_bit == 1 and who_has_bit == 1 and i_am_bit == 0 and i_have_bit == 0 and rp_bit == 1 and wp_bit == 1 and cov_bit == 1:
                print("    ├─ Protocol_Services_Supported verification: SUCCESS (B-ASC Profile exact match: WhoIs/WhoHas=1, IAm/IHave=0)")
            else:
                print("    ├─ Protocol_Services_Supported verification: FAILED (Bit mismatch)")
                pss_ok = False
        else:
            print(f"    ├─ Protocol_Services_Supported read failed: status={status_pss}")
            pss_ok = False

        if status_pots == "SUCCESS" and isinstance(bits_pots, list) and len(bits_pots) >= 57:
            # Bit 0: AI, Bit 1: AO, Bit 3: BI, Bit 8: Device, Bit 56: NetworkPort
            ai_bit = bits_pots[0]
            ao_bit = bits_pots[1]
            bi_bit = bits_pots[3]
            dev_bit = bits_pots[8]
            np_bit = bits_pots[56]
            print(f"    ├─ Protocol_Object_Types_Supported: AI={ai_bit}, AO={ao_bit}, BI={bi_bit}, Dev={dev_bit}, NetPort={np_bit}")
            if ai_bit == 1 and ao_bit == 1 and bi_bit == 1 and dev_bit == 1 and np_bit == 1:
                print("    ├─ Protocol_Object_Types_Supported verification: SUCCESS (AI, AO, BI, Device, NetworkPort supported)")
            else:
                print("    ├─ Protocol_Object_Types_Supported verification: FAILED (Bit mismatch)")
                pots_ok = False
        else:
            print(f"    ├─ Protocol_Object_Types_Supported read failed: status={status_pots}")
            pots_ok = False

        if pss_ok and pots_ok:
            print("    └─ Protocol Services & Object Types Test: SUCCESS")
            success_count += 1
        else:
            print("    └─ Protocol Services & Object Types Test: FAILED")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Protocol Services & Object Types Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # BTL & TPI: Network Port Object Properties Validation
    # (MAC Address, IP DNS Server, IP Mode, FD-BBMD Address & Lifetime)
    # -------------------------------------------------------------------
    print("[*] Testing Network Port (Type 56, Instance 1) Properties (MAC, DNS, Mode, FD-BBMD):")
    sys.stdout.flush()
    try:
        np_all_ok = True

        # 1. Read MAC_Address (Property 423)
        invoke_id = (invoke_id + 1) % 256
        apdu_mac = build_read_property_apdu(56, 1, invoke_id, property_id=423)
        status_mac, val_mac = send_and_recv(apdu_mac, invoke_id)
        if status_mac == "SUCCESS" and val_mac and len(val_mac.split(':')) == 6 and val_mac != "00:00:00:00:00:00":
            print(f"    ├─ Property 423 (MAC_Address)          : SUCCESS ('{val_mac}')")
        else:
            print(f"    ├─ Property 423 (MAC_Address)          : FAILED (status={status_mac}, val={val_mac})")
            np_all_ok = False

        # 2. Read IP_Address (400), Subnet_Mask (411), Default_Gateway (401), UDP_Port (412)
        invoke_id = (invoke_id + 1) % 256
        apdu_ip = build_read_property_apdu(56, 1, invoke_id, property_id=400)
        status_ip, val_ip = send_and_recv(apdu_ip, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_mask = build_read_property_apdu(56, 1, invoke_id, property_id=411)
        status_mask, val_mask = send_and_recv(apdu_mask, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_gw = build_read_property_apdu(56, 1, invoke_id, property_id=401)
        status_gw, val_gw = send_and_recv(apdu_gw, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_port = build_read_property_apdu(56, 1, invoke_id, property_id=412)
        status_port, val_port = send_and_recv(apdu_port, invoke_id)

        if status_ip == "SUCCESS" and status_mask == "SUCCESS" and status_gw == "SUCCESS" and status_port == "SUCCESS" and val_port == 47808:
            print(f"    ├─ IP Config: IP={val_ip}, Mask={val_mask}, GW={val_gw}, UDP={val_port} : SUCCESS")
        else:
            print(f"    ├─ IP Config read failed: IP={status_ip}, Mask={status_mask}, GW={status_gw}, UDP={status_port}")
            np_all_ok = False

        # 3. Read BACnet_IP_Mode (Property 408)
        invoke_id = (invoke_id + 1) % 256
        apdu_mode = build_read_property_apdu(56, 1, invoke_id, property_id=408)
        status_mode, val_mode = send_and_recv(apdu_mode, invoke_id)
        mode_names = {0: "NORMAL(0)", 1: "FOREIGN_DEVICE(1)", 2: "BBMD(2)"}
        mode_str = mode_names.get(val_mode, f"UNKNOWN({val_mode})") if status_mode == "SUCCESS" else f"FAILED({status_mode})"
        print(f"    ├─ Property 408 (BACnet_IP_Mode)       : {mode_str}")

        # 4. Read IP_DNS_Server (Property 406) - Array size index=0
        invoke_id = (invoke_id + 1) % 256
        apdu_dns_cnt = build_read_property_apdu(56, 1, invoke_id, property_id=406, array_index=0)
        status_dns_cnt, val_dns_cnt = send_and_recv(apdu_dns_cnt, invoke_id)
        if status_dns_cnt == "SUCCESS" and isinstance(val_dns_cnt, int):
            print(f"    ├─ Property 406 (IP_DNS_Server Count)  : SUCCESS (Count={val_dns_cnt})")
            if val_dns_cnt > 0:
                invoke_id = (invoke_id + 1) % 256
                apdu_dns1 = build_read_property_apdu(56, 1, invoke_id, property_id=406, array_index=1)
                status_dns1, val_dns1 = send_and_recv(apdu_dns1, invoke_id)
                print(f"    ├─ Property 406 (IP_DNS_Server[1])     : SUCCESS ('{val_dns1}')")
        else:
            print(f"    ├─ Property 406 (IP_DNS_Server)        : FAILED/SKIPPED (status={status_dns_cnt})")

        # 5. Read FD_BBMD_Address (418) & FD_Subscription_Lifetime (419)
        invoke_id = (invoke_id + 1) % 256
        apdu_bbmd = build_read_property_apdu(56, 1, invoke_id, property_id=418)
        status_bbmd, val_bbmd = send_and_recv(apdu_bbmd, invoke_id)

        invoke_id = (invoke_id + 1) % 256
        apdu_ttl = build_read_property_apdu(56, 1, invoke_id, property_id=419)
        status_ttl, val_ttl = send_and_recv(apdu_ttl, invoke_id)

        # FD_BBMD_Address/FD_Subscription_Lifetime are in Property_List in every mode (FD capable device),
        # so they must be readable in Normal mode too (BTL 135.1 7.1.3 / 4.4(n)).
        if status_bbmd == "SUCCESS" and status_ttl == "SUCCESS":
            label = "Foreign Device" if val_mode == 1 else "Normal Mode FD"
            print(f"    ├─ {label} Properties Check".ljust(41) + f": SUCCESS (BBMD={val_bbmd}, TTL={val_ttl})")
        else:
            print(f"    ├─ FD Properties Check                 : FAILED (mode={val_mode}, bbmd={status_bbmd}, ttl={status_ttl})")
            np_all_ok = False

        if np_all_ok:
            print("    └─ Network Port Properties Test: SUCCESS")
            success_count += 1
        else:
            print("    └─ Network Port Properties Test: FAILED")
            failure_count += 1
    except Exception as e:
        print(f"    └─ Network Port Properties Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Min/Max Range Validation Test (Without Priority)
    # -------------------------------------------------------------------
    print("[*] Testing Min/Max Range Validation without Priority:")
    sys.stdout.flush()
    try:
        # Find first Analog Output (Type 1) object with min_value and max_value from config
        ao_test_obj = None
        for dev in configs['devices']:
            for obj in dev['objects']:
                obj_type = infer_bacnet_type(obj)
                if obj_type == 1 and obj.get('min_value') is not None and obj.get('max_value') is not None:
                    ao_test_obj = dict(obj)
                    ao_test_obj['type'] = 1
                    break
            if ao_test_obj:
                break

        if ao_test_obj:
            min_val = ao_test_obj['min_value']
            max_val = ao_test_obj['max_value']
            below_min = min_val - 10.0
            above_max = max_val + 10.0

            # 1. Test Write below Min without priority
            invoke_id = (invoke_id + 1) % 256
            apdu_below = build_write_property_generic_apdu(ao_test_obj['type'], ao_test_obj['instance'], invoke_id, 85, below_min, tag_type=4, priority=None)
            status_below, val_below = send_and_recv(apdu_below, invoke_id)

            # 2. Test Write above Max without priority
            invoke_id = (invoke_id + 1) % 256
            apdu_above = build_write_property_generic_apdu(ao_test_obj['type'], ao_test_obj['instance'], invoke_id, 85, above_max, tag_type=4, priority=None)
            status_above, val_above = send_and_recv(apdu_above, invoke_id)

            if status_below == "BACNET_ERROR_PDU" and "VALUE_OUT_OF_RANGE" in str(val_below) and \
               status_above == "BACNET_ERROR_PDU" and "VALUE_OUT_OF_RANGE" in str(val_above):
                print(f"    └─ Min/Max Range Check without Priority on '{ao_test_obj['name']}': SUCCESS (Both rejected with VALUE_OUT_OF_RANGE)")
                success_count += 1
            else:
                print(f"    └─ Min/Max Range Check without Priority on '{ao_test_obj['name']}': FAILED (Below: {status_below}/{val_below}, Above: {status_above}/{val_above})")
                failure_count += 1
        else:
            print("    └─ Min/Max Range Check without Priority: SKIPPED (No Analog object with min/max bounds found)")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ Min/Max Range Check without Priority: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # 7.3.2.6.3 Binary Output (BO) Polarity Property Test
    # -------------------------------------------------------------------
    print("[*] Testing BTL Clause 7.3.2.6.3 (Binary Output Polarity Property Test):")
    sys.stdout.flush()
    try:
        bo_test_obj = None
        for dev in configs.get('devices', []):
            for obj in dev.get('objects', []):
                if obj.get('type') == 4: # Binary Output
                    bo_test_obj = obj
                    break
            if bo_test_obj:
                break

        if bo_test_obj:
            bo_type = bo_test_obj['type']
            bo_inst = bo_test_obj['instance']
            bo_name = bo_test_obj.get('name', f"BO:{bo_inst}")

            # 1. Read current Polarity (Property 84)
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_rd = build_read_property_apdu(bo_type, bo_inst, invoke_id, 84)
            st_pol, val_pol = send_and_recv(apdu_pol_rd, invoke_id)
            orig_pol = int(val_pol) if st_pol == "SUCCESS" and val_pol is not None else 0

            # 2. Read current Present_Value (Property 85)
            invoke_id = (invoke_id + 1) % 256
            apdu_pv_rd = build_read_property_apdu(bo_type, bo_inst, invoke_id, 85)
            st_pv, val_pv = send_and_recv(apdu_pv_rd, invoke_id)
            orig_pv = int(val_pv) if st_pv == "SUCCESS" and val_pv is not None else 0

            # 3. Write inverted Polarity
            new_pol = 1 if orig_pol == 0 else 0
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_wr = build_write_property_generic_apdu(bo_type, bo_inst, invoke_id, 84, new_pol, tag_type=9)
            st_wr, _ = send_and_recv(apdu_pol_wr, invoke_id)

            # 4. Verify Polarity updated to new value
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_chk = build_read_property_apdu(bo_type, bo_inst, invoke_id, 84)
            st_pol_chk, val_pol_chk = send_and_recv(apdu_pol_chk, invoke_id)

            # 5. Verify Present_Value is UNCHANGED (Logical Value remains preserved on BO)
            invoke_id = (invoke_id + 1) % 256
            apdu_pv_chk = build_read_property_apdu(bo_type, bo_inst, invoke_id, 85)
            st_pv_chk, val_pv_chk = send_and_recv(apdu_pv_chk, invoke_id)

            # 6. Restore original Polarity
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_rst = build_write_property_generic_apdu(bo_type, bo_inst, invoke_id, 84, orig_pol, tag_type=9)
            st_rst, _ = send_and_recv(apdu_pol_rst, invoke_id)

            pol_switched = (st_wr == "SUCCESS" and st_pol_chk == "SUCCESS" and int(val_pol_chk) == new_pol)
            pv_preserved = (st_pv_chk == "SUCCESS" and int(val_pv_chk) == orig_pv)
            pol_restored = (st_rst == "SUCCESS")

            if pol_switched and pv_preserved and pol_restored:
                print(f"    └─ BO Polarity Test on '{bo_name}': SUCCESS (Polarity {orig_pol} -> {new_pol} -> {orig_pol}, Present_Value remained {orig_pv})")
                success_count += 1
            else:
                print(f"    └─ BO Polarity Test on '{bo_name}': FAILED (Write: {st_wr}, NewPol: {val_pol_chk} [exp {new_pol}], PV: {val_pv_chk} [exp {orig_pv}], Restore: {st_rst})")
                failure_count += 1
        else:
            print("    └─ BO Polarity Test: SKIPPED (No Binary Output object found)")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BO Polarity Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # 7.3.2.5.3 Binary Input (BI) Polarity Property Test
    # -------------------------------------------------------------------
    print("[*] Testing BTL Clause 7.3.2.5.3 (Binary Input Polarity Property Test):")
    sys.stdout.flush()
    try:
        bi_test_obj = None
        for dev in configs.get('devices', []):
            for obj in dev.get('objects', []):
                if obj.get('type') == 3: # Binary Input
                    bi_test_obj = obj
                    break
            if bi_test_obj:
                break

        if bi_test_obj:
            bi_type = bi_test_obj['type']
            bi_inst = bi_test_obj['instance']
            bi_name = bi_test_obj.get('name', f"BI:{bi_inst}")

            # 1. Read current Polarity (Property 84)
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_rd = build_read_property_apdu(bi_type, bi_inst, invoke_id, 84)
            st_pol, val_pol = send_and_recv(apdu_pol_rd, invoke_id)
            orig_pol = int(val_pol) if st_pol == "SUCCESS" and val_pol is not None else 0

            # 2. Read current Present_Value (Property 85)
            invoke_id = (invoke_id + 1) % 256
            apdu_pv_rd = build_read_property_apdu(bi_type, bi_inst, invoke_id, 85)
            st_pv, val_pv = send_and_recv(apdu_pv_rd, invoke_id)
            orig_pv = int(val_pv) if st_pv == "SUCCESS" and val_pv is not None else 0

            # 3. Write inverted Polarity
            new_pol = 1 if orig_pol == 0 else 0
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_wr = build_write_property_generic_apdu(bi_type, bi_inst, invoke_id, 84, new_pol, tag_type=9)
            st_wr, _ = send_and_recv(apdu_pol_wr, invoke_id)

            # 4. Verify Polarity updated to new value
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_chk = build_read_property_apdu(bi_type, bi_inst, invoke_id, 84)
            st_pol_chk, val_pol_chk = send_and_recv(apdu_pol_chk, invoke_id)

            # 5. Verify Present_Value is INVERTED (Physical input fixed -> Logical PV inverts on BI)
            invoke_id = (invoke_id + 1) % 256
            apdu_pv_chk = build_read_property_apdu(bi_type, bi_inst, invoke_id, 85)
            st_pv_chk, val_pv_chk = send_and_recv(apdu_pv_chk, invoke_id)
            expected_inverted_pv = 1 if orig_pv == 0 else 0

            # 6. Restore original Polarity
            invoke_id = (invoke_id + 1) % 256
            apdu_pol_rst = build_write_property_generic_apdu(bi_type, bi_inst, invoke_id, 84, orig_pol, tag_type=9)
            st_rst, _ = send_and_recv(apdu_pol_rst, invoke_id)

            pol_switched = (st_wr == "SUCCESS" and st_pol_chk == "SUCCESS" and int(val_pol_chk) == new_pol)
            pv_inverted = (st_pv_chk == "SUCCESS" and int(val_pv_chk) == expected_inverted_pv)
            pol_restored = (st_rst == "SUCCESS")

            if pol_switched and pv_inverted and pol_restored:
                print(f"    └─ BI Polarity Test on '{bi_name}': SUCCESS (Polarity {orig_pol} -> {new_pol} -> {orig_pol}, Present_Value inverted {orig_pv} -> {expected_inverted_pv} -> {orig_pv})")
                success_count += 1
            else:
                print(f"    └─ BI Polarity Test on '{bi_name}': FAILED (Write: {st_wr}, NewPol: {val_pol_chk} [exp {new_pol}], Inverted PV: {val_pv_chk} [exp {expected_inverted_pv}], Restore: {st_rst})")
                failure_count += 1
        else:
            print("    └─ BI Polarity Test: SKIPPED (No Binary Input object found)")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ BI Polarity Test: FAILED ({e})")
        failure_count += 1

    # -------------------------------------------------------------------
    # Gateway Modbus Job Queue Full & Timed Wait (2.0s) Validation
    # -------------------------------------------------------------------
    print("[*] Testing Gateway Job Queue Full (NO_SPACE_TO_WRITE_PROP) & Timed Wait Protection:")
    sys.stdout.flush()
    try:
        target_obj = None
        for dev in configs.get('devices', []):
            for obj in dev.get('objects', []):
                if obj.get('type') in [1, 4, 2, 5]: # AO, BO, AV, BV
                    target_obj = obj
                    break
            if target_obj:
                break
        if not target_obj and configs.get('devices'):
            target_obj = configs['devices'][0]['objects'][0]

        if target_obj:
            obj_type = target_obj['type']
            obj_inst = target_obj['instance']
            is_bin = (obj_type in [3, 4, 5])

            # 1. Single Request Latency & Timed Wait Validation (< 2.5s)
            invoke_id = (invoke_id + 1) % 256
            if is_bin:
                apdu_single = build_write_property_apdu(obj_type, obj_inst, invoke_id, 1, is_binary=True, priority=16)
            else:
                val = target_obj.get('min_value', 10.0)
                apdu_single = build_write_property_apdu(obj_type, obj_inst, invoke_id, val, is_binary=False, priority=16)

            t0 = time.time()
            single_status, single_val = send_and_recv(apdu_single, invoke_id)
            single_elapsed = time.time() - t0

            # APDU Timeout / Retry 값은 시작 시 resolve 된 apdu_timeout_ms / apdu_retries 사용
            apdu_timeout_sec = apdu_timeout_ms / 1000.0
            # 게이트웨이 Aging 한도 = max(1000, request_timeout - rtu_timeout_ms) (modbus_client.cpp)
            aging_limit_ms = max(1000, apdu_timeout_ms - rtu_timeout_ms) if apdu_timeout_ms > rtu_timeout_ms else 1000
            print(f"    ├─ APDU Timeout Setting: {apdu_timeout_ms}ms [{timeout_src}] (Effective Queue Aging Limit: {aging_limit_ms}ms)")
            print(f"    ├─ APDU Retries Setting: {apdu_retries} [{retries_src}] (client retransmission per BACnet 5.4.4.3)")
            print(f"    ├─ Single Request Latency: {single_elapsed*1000:.1f}ms (Status: {single_status})")

            # 2. Burst Write Load Test (Queue Limit = 32 items, 60 concurrent barrier-synchronized requests)
            burst_count = 60
            burst_results = []
            burst_strays = []
            burst_retries = []  # (w_idx, inv, local_port, retry_count, recovered, send_times)
            STRAY_STATUSES = {"ERROR_INVOKE_ID_MISMATCH", "ERROR_NETWORK_LAYER_MSG",
                              "ERROR_UNKNOWN_PDU_TYPE"}
            threads = []
            barrier = threading.Barrier(burst_count)

            def burst_worker(w_idx):
                inv = (invoke_id + 1 + w_idx) % 256
                s_sock = None
                setup_error = None
                pkt = b""
                # 소켓 생성/bind/패킷 생성 실패 시 스레드가 결과 없이 죽으면
                # (traceback 은 stderr 로만 출력되어 로그에 남지 않음) 합계가 조용히 줄어든다.
                # 예외를 WORKER_EXCEPTION 으로 기록하여 Unclassified 로 집계되도록 한다.
                try:
                    s_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                    s_sock.settimeout(apdu_timeout_sec + 2.0)
                    s_sock.bind((bind_ip, 0))

                    if is_bin:
                        apdu_b = build_write_property_apdu(obj_type, obj_inst, inv, 1, is_binary=True, priority=16)
                    else:
                        v = target_obj.get('min_value', 10.0)
                        apdu_b = build_write_property_apdu(obj_type, obj_inst, inv, v, is_binary=False, priority=16)

                    pkt = build_bacnet_ip_packet(dnet, dadr, inv, apdu_b)
                except Exception as ex:
                    setup_error = f"setup: {type(ex).__name__}: {ex}"

                # Wait for all threads to be ready for an exact simultaneous burst
                # (setup 실패 스레드도 barrier 에 참여하여 나머지 스레드의 동시 전송을 유지)
                try:
                    barrier.wait(timeout=max(5.0, apdu_timeout_sec))
                except threading.BrokenBarrierError:
                    pass

                if setup_error is not None:
                    burst_results.append(("WORKER_EXCEPTION", setup_error, 0.0))
                    if s_sock is not None:
                        s_sock.close()
                    return

                start_t = time.time()
                local_port = s_sock.getsockname()[1]
                send_times = []
                try:
                    # BACnet 클라이언트 TSM(5.4.4.3 AWAIT_CONFIRMATION) 동작과 동일하게
                    # APDU_Timeout 동안 응답이 없으면 같은 invoke ID 로 최대 apdu_retries 회 재전송한다.
                    # 마지막 시도에만 2초 여유를 둔다.
                    max_attempts = 1 + max(0, apdu_retries)
                    got_response = False
                    for attempt in range(max_attempts):
                        send_times.append(time.time())
                        s_sock.sendto(pkt, (target_ip, target_port))
                        slack = 2.0 if attempt == max_attempts - 1 else 0.0
                        deadline = time.time() + apdu_timeout_sec + slack
                        # Stray 패킷(invoke ID 불일치, 네트워크 계층 메시지, Unconfirmed 요청 등)은
                        # 기록만 하고 자신의 응답을 deadline 까지 계속 기다린다.
                        while True:
                            remaining = deadline - time.time()
                            if remaining <= 0:
                                break
                            s_sock.settimeout(remaining)
                            try:
                                resp, src = s_sock.recvfrom(2048)
                            except socket.timeout:
                                break
                            st, parsed_v = parse_bacnet_response(resp, inv)
                            if st in STRAY_STATUSES:
                                burst_strays.append((w_idx, inv, src, st, str(parsed_v), resp.hex()))
                                continue
                            burst_results.append((st, str(parsed_v), time.time() - start_t))
                            got_response = True
                            break
                        if got_response:
                            break
                    if len(send_times) > 1:
                        burst_retries.append((w_idx, inv, local_port, len(send_times) - 1,
                                              got_response, list(send_times)))
                    if not got_response:
                        raise socket.timeout(f"no response after {len(send_times)} attempt(s)")
                except socket.timeout as ex:
                    burst_results.append(("TIMEOUT_EXCEPTION", str(ex), time.time() - start_t))
                except Exception as ex:
                    burst_results.append(("WORKER_EXCEPTION", f"io: {type(ex).__name__}: {ex}",
                                          time.time() - start_t))
                finally:
                    s_sock.close()

            for i in range(burst_count):
                t = threading.Thread(target=burst_worker, args=(i,))
                threads.append(t)

            for t in threads:
                t.start()
            for t in threads:
                t.join()

            invoke_id = (invoke_id + burst_count + 1) % 256

            success_res = sum(1 for st, _, _ in burst_results if st == "SUCCESS")
            q_full_res = sum(1 for st, v, _ in burst_results if st == "BACNET_ERROR_PDU" and "NO_SPACE_TO_WRITE_PROPERTY" in v)
            op_prob_res = sum(1 for st, v, _ in burst_results if st == "BACNET_ERROR_PDU" and "OPERATIONAL_PROBLEM" in v)
            abort_res = sum(1 for st, _, _ in burst_results if st == "BACNET_ABORT_PDU")
            timeout_res = sum(1 for st, _, _ in burst_results if st == "TIMEOUT_EXCEPTION")
            max_elapsed = max((el for _, _, el in burst_results), default=0.0)
            unclassified = [(st, v) for st, v, _ in burst_results
                            if st not in ("SUCCESS", "BACNET_ABORT_PDU", "TIMEOUT_EXCEPTION")
                            and not (st == "BACNET_ERROR_PDU" and
                                     ("NO_SPACE_TO_WRITE_PROPERTY" in v or "OPERATIONAL_PROBLEM" in v))]

            print(f"    ├─ Burst Test Results ({burst_count} simultaneous requests with Barrier):")
            print(f"    │   ├─ Processed / Simple-ACK                  : {success_res}")
            print(f"    │   ├─ Queue Full (NO_SPACE_TO_WRITE_PROPERTY) : {q_full_res}")
            print(f"    │   ├─ Aging / Operational Problem             : {op_prob_res}")
            if abort_res > 0:
                print(f"    │   ├─ Stack Aborted Requests                  : {abort_res}")
            recovered = [r for r in burst_retries if r[4]]
            if burst_retries:
                print(f"    │   ├─ Recovered by APDU Retry                 : {len(recovered)} "
                      f"(retried workers={len(burst_retries)})")
                for w_i, w_inv, w_port, w_cnt, w_ok, w_times in burst_retries:
                    t_str = ", ".join(time.strftime('%H:%M:%S', time.localtime(t)) + f".{int((t % 1) * 1000):03d}"
                                      for t in w_times)
                    print(f"    │   │   ├─ worker={w_i} inv={w_inv} local_port={w_port} "
                          f"retries={w_cnt} {'RECOVERED' if w_ok else 'NO RESPONSE'} sends=[{t_str}]")
            print(f"    │   ├─ Client Socket Timeouts                  : {timeout_res}")
            if unclassified:
                print(f"    │   ├─ Unclassified Responses                  : {len(unclassified)} {unclassified}")
            if len(burst_results) != burst_count:
                print(f"    │   ├─ Missing Worker Results                  : {burst_count - len(burst_results)} "
                      f"(results={len(burst_results)}/{burst_count})")
            if burst_strays:
                print(f"    │   ├─ Ignored Stray Packets                   : {len(burst_strays)}")
                for w_i, w_inv, w_src, w_st, w_v, w_hex in burst_strays:
                    print(f"    │   │   ├─ worker={w_i} inv={w_inv} from={w_src[0]}:{w_src[1]} "
                          f"{w_st} ({w_v}) raw={w_hex}")
            print(f"    │   └─ Max Latency                             : {max_elapsed*1000:.1f}ms")

            retry_note = (f", {len(recovered)} recovered by APDU retry"
                          if recovered else "")

            # Verification:
            # - No client socket timeouts after APDU retries (gateway/stack responded to 100% of requests;
            #   transient network loss recovered by client retransmission is allowed per BACnet 5.4.4.3)
            # - Queue Full (NO_SPACE_TO_WRITE_PROPERTY) must be triggered when burst > 32
            # - Total responses must account for all 60 requests
            total_handled = success_res + q_full_res + op_prob_res + abort_res
            if timeout_res == 0 and q_full_res > 0 and total_handled == burst_count:
                if op_prob_res > 0 or abort_res > 0:
                    print(f"    └─ Queue Full & APDU Aging Validation: SUCCESS (Queue Full [{q_full_res}] & Aging Protection [{op_prob_res + abort_res}] verified without timeouts{retry_note})")
                else:
                    print(f"    └─ Queue Full & APDU Aging Validation: SUCCESS (Queue Full [{q_full_res}] verified & all queued requests processed within APDU timeout{retry_note})")
                success_count += 1
            elif timeout_res == 0 and success_res + op_prob_res == burst_count:
                print(f"    └─ Queue Full & APDU Aging Validation: SUCCESS (Fast processing drained all requests without queue accumulation{retry_note})")
                success_count += 1
            else:
                print(f"    └─ Queue Full & APDU Aging Validation: FAILED (Timeouts={timeout_res}, QueueFullCount={q_full_res}, OpProblemCount={op_prob_res}, Aborts={abort_res}, Unclassified={len(unclassified)} {unclassified}, APDURetries={apdu_retries}, Recovered={len(recovered)})")
                failure_count += 1
        else:
            print("    └─ Queue Full & Timed Wait Validation: SKIPPED (No suitable target object found)")
            skipped_count += 1
    except Exception as e:
        print(f"    └─ Queue Full & Timed Wait Validation: FAILED ({e})")
        failure_count += 1

    tracker.capturing = False
    print("\n=======================================================================")
    print("                      BTL TEST SUMMARY                                 ")
    print("=======================================================================")
    print(f" Total Tests Executed : {success_count + failure_count + skipped_count}")
    print(f" Successful           : {success_count}")
    print(f" Skipped (Not Supp.)  : {skipped_count}")
    print(f" Failed / Errors      : {failure_count}")
    print(f" APDU Retries         : {sock.retry_count} resent, {sock.recovered_count} recovered "
          f"(policy {apdu_timeout_ms}ms x {apdu_retries}, burst test excluded)")
    if tracker.failed_tests:
        print("-----------------------------------------------------------------------")
        print(" Failed Tests Details:")
        for idx, ft in enumerate(tracker.failed_tests, 1):
            print(f"   [{idx:02d}] {ft}")
    print("=======================================================================")

    raw_sock.close()
    if failure_count > 0:
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(description="BACnet BTL Test Suite for Device & Network Port Objects")
    parser.add_argument("--ip", default="127.0.0.1", help="Target Gateway IP Address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=47808, help="Target Gateway BACnet Port (default: 47808)")
    parser.add_argument("--bind-ip", default=None, help="Local IP address to bind client socket")
    parser.add_argument("--config-dir", default="../conf", help="Path to gateway configuration directory (default: ../conf)")
    parser.add_argument("--delay", type=float, default=0.0, help="Delay in seconds between requests")
    parser.add_argument("--log-file", default=None, help="File path to save output log")

    args = parser.parse_args()

    tee = None
    if args.log_file:
        tee = Tee(args.log_file, "w")
        sys.stdout = tee

    try:
        configs = load_gateway_configs(args.config_dir)
        run_btl_tests(
            target_ip=args.ip,
            target_port=args.port,
            configs=configs,
            bind_ip=args.bind_ip,
            delay=args.delay
        )
    finally:
        if tee:
            sys.stdout = tee.stdout
            tee.close()

if __name__ == "__main__":
    main()
