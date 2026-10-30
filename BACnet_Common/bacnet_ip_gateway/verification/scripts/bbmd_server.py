#!/usr/bin/env python3
"""
Standalone Lightweight BACnet BBMD (BACnet Broadcast Management Device) Server
Supports:
  - BVLC Register-Foreign-Device (0x05)
  - BVLC Read-Foreign-Device-Table (0x06 / 0x07)
  - BVLC Delete-Foreign-Device-Table-Entry (0x08)
  - BVLC Distribute-Broadcast-To-Network (0x09 -> 0x04)
  - FDT Lifetime Aging & Expiry
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import threading
import time
from typing import Dict, Tuple


BVLC_TYPE = 0x81

BVLC_RESULT = 0x00
BVLC_WRITE_BDT = 0x01
BVLC_READ_BDT = 0x02
BVLC_READ_BDT_ACK = 0x03
BVLC_FORWARDED_NPDU = 0x04
BVLC_REGISTER_FOREIGN_DEVICE = 0x05
BVLC_READ_FOREIGN_DEVICE_TABLE = 0x06
BVLC_READ_FOREIGN_DEVICE_TABLE_ACK = 0x07
BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY = 0x08
BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK = 0x09

BVLC_RESULT_SUCCESS = 0x0000
BVLC_RESULT_REGISTER_FD_NAK = 0x0030
BVLC_RESULT_READ_FDT_NAK = 0x0040


class FDTEntry:
    def __init__(self, ip: str, port: int, ttl: int):
        self.ip = ip
        self.port = port
        self.ttl = ttl
        self.registered_time = time.time()
        self.expires_at = self.registered_time + ttl

    @property
    def remaining(self) -> int:
        rem = int(self.expires_at - time.time())
        return max(0, rem)

    @property
    def is_expired(self) -> bool:
        return time.time() >= self.expires_at


class BBMDServer:
    def __init__(self, host: str = "0.0.0.0", port: int = 47808):
        self.host = host
        self.port = port
        self.fdt: Dict[Tuple[str, int], FDTEntry] = {}
        self.bdt = []
        self.lock = threading.Lock()
        self.running = False
        self.sock: socket.socket | None = None
        self._aging_thread: threading.Thread | None = None

    def start(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        except (AttributeError, OSError):
            pass
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind((self.host, self.port))
        self.running = True

        self._aging_thread = threading.Thread(target=self._fdt_aging_loop, daemon=True)
        self._aging_thread.start()

        bound_ip, bound_port = self.sock.getsockname()
        print(f"[BBMD] Server listening on {bound_ip}:{bound_port}", flush=True)

        try:
            while self.running:
                try:
                    data, addr = self.sock.recvfrom(4096)
                except OSError:
                    break
                if data:
                    self._handle_packet(data, addr)
        finally:
            self.stop()

    def stop(self):
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None
        print("[BBMD] Server stopped", flush=True)

    def _fdt_aging_loop(self):
        while self.running:
            time.sleep(1.0)
            with self.lock:
                expired_keys = [k for k, entry in self.fdt.items() if entry.is_expired]
                for k in expired_keys:
                    print(f"[BBMD] FDT Entry expired: {k[0]}:{k[1]}", flush=True)
                    del self.fdt[k]

    def _handle_packet(self, data: bytes, addr: Tuple[str, int]):
        if len(data) < 4 or data[0] != BVLC_TYPE:
            return

        func = data[1]
        length = (data[2] << 8) | data[3]

        if func == BVLC_REGISTER_FOREIGN_DEVICE:
            self._handle_register_fd(data, addr)
        elif func == BVLC_READ_FOREIGN_DEVICE_TABLE:
            self._handle_read_fdt(addr)
        elif func == BVLC_DELETE_FOREIGN_DEVICE_TABLE_ENTRY:
            self._handle_delete_fdt_entry(data, addr)
        elif func == BVLC_DISTRIBUTE_BROADCAST_TO_NETWORK:
            self._handle_distribute_broadcast(data, addr)
        elif func == BVLC_READ_BDT:
            self._handle_read_bdt(addr)

    def _handle_register_fd(self, data: bytes, addr: Tuple[str, int]):
        if len(data) < 6:
            self._send_result(addr, BVLC_RESULT_REGISTER_FD_NAK)
            return

        ttl = (data[4] << 8) | data[5]
        with self.lock:
            self.fdt[addr] = FDTEntry(addr[0], addr[1], ttl)
        print(f"[BBMD] Registered Foreign Device: {addr[0]}:{addr[1]} (TTL={ttl}s)", flush=True)
        self._send_result(addr, BVLC_RESULT_SUCCESS)

    def _handle_read_fdt(self, addr: Tuple[str, int]):
        with self.lock:
            valid_entries = [e for e in self.fdt.values() if not e.is_expired]
            entry_count = len(valid_entries)
            payload_len = 4 + entry_count * 10
            response = bytearray(payload_len)
            response[0] = BVLC_TYPE
            response[1] = BVLC_READ_FOREIGN_DEVICE_TABLE_ACK
            response[2] = (payload_len >> 8) & 0xFF
            response[3] = payload_len & 0xFF

            offset = 4
            for entry in valid_entries:
                ip_parts = [int(p) for p in entry.ip.split(".")]
                response[offset:offset + 4] = bytes(ip_parts)
                response[offset + 4] = (entry.port >> 8) & 0xFF
                response[offset + 5] = entry.port & 0xFF
                response[offset + 6] = (entry.ttl >> 8) & 0xFF
                response[offset + 7] = entry.ttl & 0xFF
                rem = entry.remaining
                response[offset + 8] = (rem >> 8) & 0xFF
                response[offset + 9] = rem & 0xFF
                offset += 10

        print(f"[BBMD] Read-FDT from {addr[0]}:{addr[1]} -> returning {entry_count} entries", flush=True)
        if self.sock:
            self.sock.sendto(response, addr)

    def _handle_delete_fdt_entry(self, data: bytes, addr: Tuple[str, int]):
        if len(data) >= 10:
            target_ip = ".".join(str(b) for b in data[4:8])
            target_port = (data[8] << 8) | data[9]
            target_key = (target_ip, target_port)
            with self.lock:
                if target_key in self.fdt:
                    del self.fdt[target_key]
                    print(f"[BBMD] Deleted FDT entry: {target_ip}:{target_port}", flush=True)
        self._send_result(addr, BVLC_RESULT_SUCCESS)

    def _handle_read_bdt(self, addr: Tuple[str, int]):
        payload_len = 4
        response = bytearray([BVLC_TYPE, BVLC_READ_BDT_ACK, (payload_len >> 8) & 0xFF, payload_len & 0xFF])
        if self.sock:
            self.sock.sendto(response, addr)

    def _handle_distribute_broadcast(self, data: bytes, addr: Tuple[str, int]):
        if len(data) <= 4:
            return
        npdu = data[4:]
        # Build Forwarded-NPDU (0x04)
        ip_parts = [int(p) for p in addr[0].split(".")]
        forward_header = bytearray([
            BVLC_TYPE, BVLC_FORWARDED_NPDU, 0, 0,
            ip_parts[0], ip_parts[1], ip_parts[2], ip_parts[3],
            (addr[1] >> 8) & 0xFF, addr[1] & 0xFF,
        ])
        forward_packet = forward_header + npdu
        total_len = len(forward_packet)
        forward_packet[2] = (total_len >> 8) & 0xFF
        forward_packet[3] = total_len & 0xFF

        with self.lock:
            recipients = [k for k, e in self.fdt.items() if k != addr and not e.is_expired]

        if self.sock:
            for recipient in recipients:
                try:
                    self.sock.sendto(forward_packet, recipient)
                except OSError:
                    pass

    def _send_result(self, addr: Tuple[str, int], result_code: int):
        packet = bytearray([
            BVLC_TYPE, BVLC_RESULT, 0x00, 0x06,
            (result_code >> 8) & 0xFF, result_code & 0xFF,
        ])
        if self.sock:
            self.sock.sendto(packet, addr)


def main():
    parser = argparse.ArgumentParser(description="Standalone BACnet BBMD Server")
    parser.add_argument("--ip", default="0.0.0.0", help="IP address to bind (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=47808, help="UDP port (default: 47808)")
    args = parser.parse_args()

    server = BBMDServer(host=args.ip, port=args.port)
    try:
        server.start()
    except KeyboardInterrupt:
        server.stop()


if __name__ == "__main__":
    main()
