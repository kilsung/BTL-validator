#!/usr/bin/env python3
"""
modbus_rtu_slave.py - Modbus RTU Slave Simulator

Replaces diagslave for integration testing. Opens a serial/PTY port and responds
to Modbus RTU requests for the configured slave ID, implementing:
  FC01 - Read Coils
  FC02 - Read Discrete Inputs
  FC03 - Read Holding Registers
  FC04 - Read Input Registers
  FC05 - Write Single Coil
  FC06 - Write Single Register
  FC15 - Write Multiple Coils
  FC16 - Write Multiple Registers

Usage:
  python3 modbus_rtu_slave.py <port> <slave_id> [baudrate]
  Example: python3 modbus_rtu_slave.py /dev/ttyRVSLAVE0 1 19200
"""

import sys
import os
import time
import signal
from datetime import datetime
import struct
import argparse
import select
import array
import json
import socket
import threading

# ---------------------------------------------------------------------------
# CRC-16 (Modbus)
# ---------------------------------------------------------------------------
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

def append_crc(data: bytes) -> bytes:
    c = crc16(data)
    return data + bytes([c & 0xFF, (c >> 8) & 0xFF])

def check_crc(data: bytes) -> bool:
    if len(data) < 2:
        return False
    return crc16(data) == 0

# ---------------------------------------------------------------------------
# Slave memory model
# ---------------------------------------------------------------------------
class SlaveMemory:
    def __init__(self):
        self.coils           = [False] * 65536   # FC01 / FC05 / FC15
        self.discrete_inputs = [False] * 65536   # FC02 (read-only)
        self.holding_regs    = [0]    * 65536   # FC03 / FC06 / FC16
        self.input_regs      = [0]    * 65536   # FC04 (read-only)

        # Pre-populate discrete inputs and input regs with some varied data
        for i in range(65536):
            self.discrete_inputs[i] = bool((i // 2) % 2)
            self.input_regs[i]      = (i * 7 + 1) & 0xFFFF


class SimulatorControl:
    """Optional JSONL control socket used by the managed TC runner."""

    BLOCKS = {
        "coil": "coils",
        "coils": "coils",
        "discrete": "discrete_inputs",
        "discrete_input": "discrete_inputs",
        "discrete_inputs": "discrete_inputs",
        "holding": "holding_regs",
        "holding_register": "holding_regs",
        "holding_registers": "holding_regs",
        "input": "input_regs",
        "input_register": "input_regs",
        "input_registers": "input_regs",
    }

    def __init__(self, socket_path: str, memory: SlaveMemory):
        self.socket_path = socket_path
        self.memory = memory
        self.lock = threading.Lock()
        self.running = threading.Event()
        self.running.set()
        self.faults = []
        self.transactions = []
        self.thread = None
        self.server = None

    def start(self):
        if not self.socket_path:
            return
        try:
            os.unlink(self.socket_path)
        except FileNotFoundError:
            pass
        self.server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.server.bind(self.socket_path)
        self.server.listen(4)
        self.server.settimeout(0.2)
        self.thread = threading.Thread(target=self._serve, name="modbus-control", daemon=True)
        self.thread.start()
        print(f"[Simulator Control] Listening on {self.socket_path}", flush=True)

    def stop(self):
        self.running.clear()
        if self.server is not None:
            try:
                self.server.close()
            except OSError:
                pass
        if self.thread is not None:
            self.thread.join(timeout=1.0)
        if self.socket_path:
            try:
                os.unlink(self.socket_path)
            except FileNotFoundError:
                pass

    @staticmethod
    def _frame_address(frame: bytes):
        if len(frame) >= 4:
            return (frame[2] << 8) | frame[3]
        return None

    def consume_fault(self, frame: bytes):
        unit_id = frame[0] if frame else None
        fc = frame[1] if len(frame) > 1 else None
        address = self._frame_address(frame)
        with self.lock:
            for index, rule in enumerate(self.faults):
                if rule.get("unit_id") not in (None, unit_id):
                    continue
                if rule.get("fc") not in (None, fc):
                    continue
                if rule.get("address") not in (None, address):
                    continue
                selected = dict(rule)
                remaining = int(rule.get("count", 1)) - 1
                if remaining <= 0:
                    self.faults.pop(index)
                else:
                    rule["count"] = remaining
                return selected
        return None

    def record(self, request: bytes, response: bytes, action: str = "normal"):
        item = {
            "timestamp": datetime.now().isoformat(timespec="milliseconds"),
            "unit_id": request[0] if request else None,
            "fc": request[1] if len(request) > 1 else None,
            "address": self._frame_address(request),
            "request_hex": request.hex(" "),
            "response_hex": response.hex(" ") if response else "",
            "action": action,
        }
        with self.lock:
            self.transactions.append(item)
            if len(self.transactions) > 10000:
                del self.transactions[:-5000]

    def _block(self, name: str):
        attr = self.BLOCKS.get(str(name).lower())
        if attr is None:
            raise ValueError(f"unknown block: {name}")
        return getattr(self.memory, attr)

    def _reset_memory(self):
        self.memory.coils[:] = [False] * 65536
        self.memory.holding_regs[:] = [0] * 65536
        self.memory.discrete_inputs[:] = [bool((i // 2) % 2) for i in range(65536)]
        self.memory.input_regs[:] = [(i * 7 + 1) & 0xFFFF for i in range(65536)]

    def _dispatch(self, request):
        op = request.get("op")
        if op == "ping":
            return {"ok": True, "result": "pong"}
        if op == "reset":
            with self.lock:
                self._reset_memory()
                self.faults.clear()
                self.transactions.clear()
            return {"ok": True}
        if op == "set":
            address = int(request["address"])
            values = request.get("values", [request.get("value")])
            if not isinstance(values, list):
                values = [values]
            with self.lock:
                block = self._block(request["block"])
                if address < 0 or address + len(values) > len(block):
                    raise ValueError("address range is outside simulator memory")
                for offset, value in enumerate(values):
                    block[address + offset] = bool(value) if isinstance(block[0], bool) else int(value) & 0xFFFF
            return {"ok": True, "count": len(values)}
        if op == "get":
            address = int(request["address"])
            count = int(request.get("count", 1))
            with self.lock:
                block = self._block(request["block"])
                if address < 0 or count < 1 or address + count > len(block):
                    raise ValueError("address range is outside simulator memory")
                values = list(block[address:address + count])
            return {"ok": True, "values": values}
        if op == "fault":
            action = request.get("action")
            if action not in {
                "timeout", "delay", "bad_crc", "wrong_unit", "exception",
                "fragment", "partial"
            }:
                raise ValueError(f"unsupported fault action: {action}")
            rule = {
                key: request[key]
                for key in (
                    "action", "unit_id", "fc", "address", "count", "delay_ms",
                    "wrong_unit_id", "exception_code", "fragment_sizes",
                    "fragment_delay_ms", "partial_length"
                ) if key in request
            }
            rule["count"] = int(rule.get("count", 1))
            with self.lock:
                self.faults.append(rule)
            return {"ok": True, "fault": rule}
        if op == "clear_faults":
            with self.lock:
                self.faults.clear()
            return {"ok": True}
        if op == "stats":
            with self.lock:
                transactions = list(self.transactions)
                faults = [dict(item) for item in self.faults]
            return {
                "ok": True,
                "request_count": len(transactions),
                "transactions": transactions,
                "pending_faults": faults,
            }
        if op == "shutdown":
            self.running.clear()
            return {"ok": True}
        raise ValueError(f"unsupported operation: {op}")

    def _serve_client(self, connection):
        buffer = b""
        while self.running.is_set():
            chunk = connection.recv(4096)
            if not chunk:
                return
            buffer += chunk
            while b"\n" in buffer:
                line, buffer = buffer.split(b"\n", 1)
                if not line.strip():
                    continue
                try:
                    response = self._dispatch(json.loads(line.decode("utf-8")))
                except Exception as exc:
                    response = {"ok": False, "error": str(exc)}
                connection.sendall((json.dumps(response, sort_keys=True) + "\n").encode("utf-8"))

    def _serve(self):
        while self.running.is_set():
            try:
                connection, _ = self.server.accept()
            except socket.timeout:
                continue
            except OSError:
                break
            with connection:
                connection.settimeout(2.0)
                try:
                    self._serve_client(connection)
                except (OSError, socket.timeout):
                    pass

# ---------------------------------------------------------------------------
# Request handler - returns RTU response bytes or None on address error
# ---------------------------------------------------------------------------
def handle_request(data: bytes, memory: SlaveMemory, slave_id: int) -> bytes:
    if len(data) < 4:
        return None

    fc = data[1]
    payload = data[2:-2]   # strip address byte and 2 CRC bytes

    def exc(code: int) -> bytes:
        return append_crc(bytes([slave_id, 0x80 | fc, code]))

    # FC01 - Read Coils
    if fc == 0x01:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        qty    = (payload[2] << 8) | payload[3]
        if qty < 1 or qty > 2000:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        byte_count = (qty + 7) // 8
        coil_bytes = bytearray(byte_count)
        for i in range(qty):
            if memory.coils[addr + i]:
                coil_bytes[i // 8] |= (1 << (i % 8))
        return append_crc(bytes([slave_id, 0x01, byte_count]) + bytes(coil_bytes))

    # FC02 - Read Discrete Inputs
    elif fc == 0x02:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        qty    = (payload[2] << 8) | payload[3]
        if qty < 1 or qty > 2000:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        byte_count = (qty + 7) // 8
        di_bytes = bytearray(byte_count)
        for i in range(qty):
            if memory.discrete_inputs[addr + i]:
                di_bytes[i // 8] |= (1 << (i % 8))
        return append_crc(bytes([slave_id, 0x02, byte_count]) + bytes(di_bytes))

    # FC03 - Read Holding Registers
    elif fc == 0x03:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        qty    = (payload[2] << 8) | payload[3]
        if qty < 1 or qty > 125:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        byte_count = qty * 2
        reg_bytes = bytearray()
        for i in range(qty):
            v = memory.holding_regs[addr + i] & 0xFFFF
            reg_bytes += bytes([v >> 8, v & 0xFF])
        return append_crc(bytes([slave_id, 0x03, byte_count]) + bytes(reg_bytes))

    # FC04 - Read Input Registers
    elif fc == 0x04:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        qty    = (payload[2] << 8) | payload[3]
        if qty < 1 or qty > 125:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        byte_count = qty * 2
        reg_bytes = bytearray()
        for i in range(qty):
            v = memory.input_regs[addr + i] & 0xFFFF
            reg_bytes += bytes([v >> 8, v & 0xFF])
        return append_crc(bytes([slave_id, 0x04, byte_count]) + bytes(reg_bytes))

    # FC05 - Write Single Coil
    elif fc == 0x05:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        value  = (payload[2] << 8) | payload[3]
        if value not in (0x0000, 0xFF00):
            return exc(0x03)
        if addr > 65535:
            return exc(0x02)
        memory.coils[addr] = (value == 0xFF00)
        # Echo response
        return append_crc(bytes([slave_id, 0x05]) + bytes(payload[:4]))

    # FC06 - Write Single Register
    elif fc == 0x06:
        if len(payload) < 4:
            return exc(0x03)
        addr   = (payload[0] << 8) | payload[1]
        value  = (payload[2] << 8) | payload[3]
        if addr > 65535:
            return exc(0x02)
        memory.holding_regs[addr] = value
        # Echo response
        return append_crc(bytes([slave_id, 0x06]) + bytes(payload[:4]))

    # FC15 - Write Multiple Coils
    elif fc == 0x0F:
        if len(payload) < 5:
            return exc(0x03)
        addr       = (payload[0] << 8) | payload[1]
        qty        = (payload[2] << 8) | payload[3]
        byte_count = payload[4]
        if qty < 1 or qty > 1968:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        expected_bytes = (qty + 7) // 8
        if byte_count != expected_bytes or len(payload) < 5 + byte_count:
            return exc(0x03)
        coil_data = payload[5:5 + byte_count]
        for i in range(qty):
            memory.coils[addr + i] = bool((coil_data[i // 8] >> (i % 8)) & 1)
        return append_crc(bytes([slave_id, 0x0F,
                                  (addr >> 8) & 0xFF, addr & 0xFF,
                                  (qty  >> 8) & 0xFF, qty  & 0xFF]))

    # FC16 - Write Multiple Registers
    elif fc == 0x10:
        if len(payload) < 5:
            return exc(0x03)
        addr       = (payload[0] << 8) | payload[1]
        qty        = (payload[2] << 8) | payload[3]
        byte_count = payload[4]
        if qty < 1 or qty > 123:
            return exc(0x03)
        if addr + qty > 65536:
            return exc(0x02)
        if byte_count != qty * 2 or len(payload) < 5 + byte_count:
            return exc(0x03)
        reg_data = payload[5:5 + byte_count]
        for i in range(qty):
            memory.holding_regs[addr + i] = (reg_data[i*2] << 8) | reg_data[i*2 + 1]
        return append_crc(bytes([slave_id, 0x10,
                                  (addr >> 8) & 0xFF, addr & 0xFF,
                                  (qty  >> 8) & 0xFF, qty  & 0xFF]))

    else:
        return exc(0x01)   # Illegal Function Code

# ---------------------------------------------------------------------------
# Serial port helper (raw POSIX termios - no pyserial dependency) & Inotify Watcher
# ---------------------------------------------------------------------------
import ctypes

class InotifyPortWatcher:
    """Linux inotify-based directory watcher for serial/PTY port nodes."""
    def __init__(self, device_path: str):
        self.device_path = device_path
        self.dir_path = os.path.dirname(os.path.abspath(device_path)) or '.'
        self.file_name = os.path.basename(device_path)
        self.inotify_fd = -1
        self.watch_wd = -1

        try:
            libc = ctypes.CDLL(None)
            # inotify_init1(IN_NONBLOCK | IN_CLOEXEC) -> 0x80800
            self.inotify_fd = libc.inotify_init1(0x80800)
            if self.inotify_fd >= 0:
                # IN_CREATE(0x100) | IN_DELETE(0x200) | IN_ATTRIB(0x4) | IN_MOVED_TO(0x80)
                mask = 0x100 | 0x200 | 0x4 | 0x80
                dir_bytes = self.dir_path.encode('utf-8')
                self.watch_wd = libc.inotify_add_watch(self.inotify_fd, dir_bytes, mask)
        except Exception:
            self.close()

    def wait_for_device(self, timeout_sec: float = 1.0) -> bool:
        if self.inotify_fd >= 0:
            p = select.poll()
            p.register(self.inotify_fd, select.POLLIN)
            events = p.poll(int(timeout_sec * 1000))
            if events:
                try:
                    os.read(self.inotify_fd, 4096)
                except Exception:
                    pass
                return True
        else:
            time.sleep(timeout_sec)
        return False

    def close(self):
        if self.inotify_fd >= 0:
            try:
                os.close(self.inotify_fd)
            except Exception:
                pass
            self.inotify_fd = -1
            self.watch_wd = -1

def open_serial_safe(port: str, baudrate: int) -> int:
    try:
        import fcntl
        import termios

        baud_map = {
            1200: termios.B1200,
            2400: termios.B2400,
            4800: termios.B4800,
            9600: termios.B9600,
            19200: termios.B19200,
            38400: termios.B38400,
            57600: termios.B57600,
            115200: termios.B115200,
        }
        fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        attrs = termios.tcgetattr(fd)
        baud = baud_map.get(baudrate, termios.B19200)
        attrs[4] = baud
        attrs[5] = baud
        attrs[2] = (termios.CS8 | termios.CLOCAL | termios.CREAD)
        attrs[3] = 0
        attrs[0] = 0
        attrs[1] = 0
        attrs[6][termios.VMIN]  = 0
        attrs[6][termios.VTIME] = 5

        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)
        return fd
    except Exception:
        return -1

def extract_valid_frame(buf: bytes, slave_id: int) -> int:
    """
    버퍼의 맨 앞(buf[0])에서 시작하는 유효한 Modbus RTU 프레임이 있는지
    바이트 정보를 분석하여 크기를 추정하고 CRC를 검산합니다.
    유효한 프레임이 식별되면 그 프레임의 전체 바이트 크기(int)를 반환하고,
    그렇지 않으면 None을 반환합니다.
    """
    if len(buf) < 4:
        return None

    fc = buf[1]

    # 1. 예외 응답 프레임 (Function Code | 0x80)
    if (fc & 0x80):
        if len(buf) >= 5 and check_crc(buf[:5]):
            return 5

    # 2. 고정 크기 (8바이트) 프레임들 (FC05, FC06)
    if fc in (0x05, 0x06):
        if len(buf) >= 8 and check_crc(buf[:8]):
            return 8

    # 3. Read 계열 (FC01, FC02, FC03, FC04)
    if fc in (0x01, 0x02, 0x03, 0x04):
        if len(buf) >= 8 and check_crc(buf[:8]):
            qty = (buf[4] << 8) | buf[5]
            if 1 <= qty <= 2000:
                return 8

        byte_cnt = buf[2]
        if 1 <= byte_cnt <= 250:
            resp_len = 3 + byte_cnt + 2
            if len(buf) >= resp_len and check_crc(buf[:resp_len]):
                return resp_len

    # 4. Write Multiple 계열 (FC15, FC16)
    if fc in (0x0F, 0x10):
        if len(buf) >= 8 and check_crc(buf[:8]):
            return 8

        if len(buf) >= 7:
            byte_cnt = buf[6]
            req_len = 7 + byte_cnt + 2
            if len(buf) >= req_len and check_crc(buf[:req_len]):
                return req_len

    return None


def read_rtu_frame(fd: int, slave_id: int, leftover: bytes = b'', timeout: float = 1.0):
    """
    Read exactly one Modbus RTU frame from the serial port.
    Returns: (frame_bytes, rest_bytes, is_healthy_bool)
    """
    buf = bytearray(leftover)
    deadline = time.time() + timeout
    gap_timeout = 0.025   # 25ms inter-byte gap

    p = select.poll()
    p.register(fd, select.POLLIN | select.POLLHUP | select.POLLERR | select.POLLNVAL)

    while time.time() < deadline:
        expected_length = extract_valid_frame(bytes(buf), slave_id)
        if expected_length is not None and len(buf) >= expected_length:
            frame = bytes(buf[:expected_length])
            rest  = bytes(buf[expected_length:])
            return frame, rest, True

        events = p.poll(int(gap_timeout * 1000))
        if events:
            for poll_fd, event in events:
                if event & (select.POLLHUP | select.POLLERR | select.POLLNVAL):
                    return b'', b'', False

                if event & select.POLLIN:
                    want = 256
                    if len(buf) >= 3 and buf[1] in (0x01, 0x02, 0x03, 0x04) and not (buf[1] & 0x80):
                        byte_cnt = buf[2]
                        if 1 <= byte_cnt <= 250:
                            want = max(1, (3 + byte_cnt + 2) - len(buf))
                    elif len(buf) >= 7 and buf[1] in (0x0F, 0x10):
                        byte_cnt = buf[6]
                        want = max(1, (7 + byte_cnt + 2) - len(buf))

                    try:
                        chunk = os.read(fd, want)
                        if chunk:
                            buf += chunk
                            continue
                        else:
                            # EOF on PTY slave (Master closed)
                            return b'', b'', False
                    except OSError as e:
                        if e.errno in (5, 9, 19, 32): # EIO, EBADF, ENODEV, EPIPE
                            return b'', b'', False
                        raise

        if buf:
            return bytes(buf), b'', True

    return bytes(buf), b'', True

class Tee:
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

# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Modbus RTU Slave Simulator (diagslave replacement)")
    parser.add_argument("port",     help="Serial port device (e.g. /dev/ttyRVSLAVE0)")
    parser.add_argument("slave_id", type=int, help="Modbus Slave ID (1-247)")
    parser.add_argument("--baudrate", "-b", type=int, default=19200,
                        help="Serial baudrate (default: 19200)")
    parser.add_argument("--delay", "-d", type=float, default=0.0,
                        help="Response delay in seconds before sending Modbus response (default: 0.0)")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Print each transaction to stdout")
    parser.add_argument("--control-socket", default=None,
                        help="Optional Unix-domain JSONL control socket path")
    parser.add_argument("--log-file", default=None,
                        help="Simulator log path (default: verification/log/modbus_rtu_slave_<id>.log)")
    args = parser.parse_args()

    log_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "log")
    log_file = args.log_file or os.path.join(log_dir, f"modbus_rtu_slave_{args.slave_id}.log")
    os.makedirs(os.path.dirname(os.path.abspath(log_file)), exist_ok=True)
    
    tee = Tee(log_file, "w")
    sys.stdout = tee
    sys.stderr = tee

    if not (1 <= args.slave_id <= 247):
        print(f"Error: slave_id must be 1-247, got {args.slave_id}", file=sys.stderr)
        sys.exit(1)

    memory   = SlaveMemory()
    running  = True
    control = SimulatorControl(args.control_socket, memory) if args.control_socket else None

    def on_signal(signum, frame):
        nonlocal running
        running = False

    signal.signal(signal.SIGTERM, on_signal)
    signal.signal(signal.SIGINT,  on_signal)

    if control is not None:
        control.start()

    watcher = InotifyPortWatcher(args.port)
    fd = open_serial_safe(args.port, args.baudrate)
    if fd >= 0:
        print(f"[Slave {args.slave_id}] Listening on {args.port} @ {args.baudrate} baud", flush=True)
    else:
        print(f"[Slave {args.slave_id}] Serial port {args.port} not yet available. Starting in auto-reconnect standby mode...", flush=True)

    leftover = b''
    BROADCAST_FUNCTION_CODES = (0x05, 0x06, 0x0F, 0x10)

    while running and (control is None or control.running.is_set()):
        # Check if connected
        if fd < 0:
            leftover = b''
            # inotify or 1-second fallback timeout
            watcher.wait_for_device(1.0)
            if not running:
                break
            fd = open_serial_safe(args.port, args.baudrate)
            if fd >= 0:
                print(f"[Slave {args.slave_id}] Serial port {args.port} connected/reopened successfully @ {args.baudrate} baud.", flush=True)
            continue

        try:
            frame, leftover, is_healthy = read_rtu_frame(fd, args.slave_id, leftover)
            if not is_healthy:
                print(f"[Slave {args.slave_id}] Port hangup/disconnection detected on {args.port}. Waiting for reconnection...", flush=True)
                try:
                    os.close(fd)
                except Exception:
                    pass
                fd = -1
                continue
        except Exception as e:
            print(f"[Slave {args.slave_id}] Read error: {e}", file=sys.stderr, flush=True)
            leftover = b''
            try:
                os.close(fd)
            except Exception:
                pass
            fd = -1
            time.sleep(0.1)
            continue

        if not frame:
            continue

        is_broadcast = (frame[0] == 0x00 and len(frame) >= 2
                         and frame[1] in BROADCAST_FUNCTION_CODES)

        is_ours = (frame[0] == args.slave_id or is_broadcast)
        crc_ok = check_crc(frame)

        now_str = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        if is_ours:
            print(f"[{now_str}][Slave {args.slave_id}] Raw Rx: {frame.hex(' ')} (CRC: {crc_ok})", flush=True)
        elif args.verbose and not crc_ok:
            print(f"[{now_str}][Slave {args.slave_id}] Raw Rx (Foreign Error): {frame.hex(' ')} (CRC: {crc_ok})", flush=True)

        if not is_ours or not crc_ok or frame[0] == 0x00:
            continue

        fault = control.consume_fault(frame) if control is not None else None
        fault_action = fault.get("action") if fault else "normal"
        if fault_action == "timeout":
            if control is not None:
                control.record(frame, b"", fault_action)
            print(f"[{now_str}][Slave {args.slave_id}] Fault injected: timeout", flush=True)
            continue

        response = handle_request(frame, memory, args.slave_id)
        if response is None:
            continue

        if fault_action == "exception":
            exception_code = int(fault.get("exception_code", 2)) & 0xFF
            response = append_crc(bytes([args.slave_id, frame[1] | 0x80, exception_code]))
        elif fault_action == "wrong_unit":
            wrong_unit = int(fault.get("wrong_unit_id", (args.slave_id % 247) + 1))
            response = append_crc(bytes([wrong_unit]) + response[1:-2])
        elif fault_action == "bad_crc" and len(response) >= 2:
            response = response[:-1] + bytes([response[-1] ^ 0xFF])
        elif fault_action == "partial":
            partial_length = max(1, int(fault.get("partial_length", len(response) - 1)))
            response = response[:partial_length]

        effective_delay = args.delay
        if fault_action == "delay":
            effective_delay += max(0.0, float(fault.get("delay_ms", 0)) / 1000.0)
        if effective_delay > 0.0:
            time.sleep(effective_delay)

        now_str_tx = datetime.now().strftime('%H:%M:%S.%f')[:-3]
        delay_info = f" (delay={effective_delay*1000:.1f}ms)" if effective_delay > 0.0 else ""
        print(f"[{now_str_tx}][Slave {args.slave_id}] Tx{delay_info}: {response.hex(' ')}", flush=True)

        if control is not None:
            control.record(frame, response, fault_action)

        try:
            if fault_action == "fragment":
                sizes = [int(value) for value in fault.get("fragment_sizes", []) if int(value) > 0]
                fragment_delay = max(0.0, float(fault.get("fragment_delay_ms", 10)) / 1000.0)
                offset = 0
                for size in sizes:
                    if offset >= len(response):
                        break
                    os.write(fd, response[offset:offset + size])
                    offset += size
                    if offset < len(response):
                        time.sleep(fragment_delay)
                if offset < len(response):
                    os.write(fd, response[offset:])
            else:
                os.write(fd, response)
        except OSError as e:
            if e.errno in (5, 9, 19, 32): # EIO, EBADF, ENODEV, EPIPE
                print(f"[Slave {args.slave_id}] Write failed due to port hangup ({e}), closing port.", flush=True)
                try:
                    os.close(fd)
                except Exception:
                    pass
                fd = -1
            else:
                print(f"[Slave {args.slave_id}] Write error: {e}", file=sys.stderr, flush=True)
        except Exception as e:
            print(f"[Slave {args.slave_id}] Write error: {e}", file=sys.stderr, flush=True)

    if fd >= 0:
        try:
            os.close(fd)
        except Exception:
            pass
    watcher.close()
    if control is not None:
        control.stop()
    print(f"[Slave {args.slave_id}] Stopped.", flush=True)

if __name__ == "__main__":
    main()
