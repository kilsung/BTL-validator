"""Windows compatibility shims for the verification scripts (no-op on POSIX).

Windows reports an ICMP port-unreachable for a previously sent UDP datagram as
ConnectionResetError (WinError 10054) on the next recvfrom(), while Linux
silently ignores it. BACnet TCs rely on Linux semantics (no reply == timeout),
so every UDP socket created on Windows disables SIO_UDP_CONNRESET.
"""

import os
import socket
import sys
import tempfile

ASKPASS_ENV = "BTL_SSH_ASKPASS_PW"


def make_windows_askpass(env):
    """Windows OpenSSH용 SSH_ASKPASS(.cmd) 생성 → 삭제할 파일 경로 반환.

    POSIX용 '#!/bin/sh echo' 스크립트는 Windows에서 실행되지 않으므로 .cmd에서
    현재 Python으로 환경변수의 암호를 출력한다 (암호는 파일에 기록하지 않음).
    호출 전에 env[ASKPASS_ENV]에 암호를 넣어 두어야 한다.
    """
    fd, path = tempfile.mkstemp(suffix=".cmd", prefix="btl-askpass-")
    with os.fdopen(fd, "w", encoding="mbcs", newline="") as f:
        f.write("@echo off\r\n"
                f'"{sys.executable}" -c "import os,sys;'
                f"sys.stdout.write(os.environ.get('{ASKPASS_ENV}','')+chr(10))\"\r\n")
    env["SSH_ASKPASS"] = path
    env["SSH_ASKPASS_REQUIRE"] = "force"
    env.setdefault("DISPLAY", ":0")
    return path

if sys.platform == "win32" and not getattr(socket.socket, "_udp_connreset_disabled", False):
    import ctypes
    from ctypes import wintypes

    _SIO_UDP_CONNRESET = 0x9800000C
    _ws2_32 = ctypes.WinDLL("ws2_32")
    _ws2_32.WSAIoctl.argtypes = (
        ctypes.c_size_t, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD,
        ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD),
        ctypes.c_void_p, ctypes.c_void_p,
    )
    _BaseSocket = socket.socket

    def _disable_udp_connreset(sock):
        flag = wintypes.BOOL(False)
        returned = wintypes.DWORD(0)
        _ws2_32.WSAIoctl(sock.fileno(), _SIO_UDP_CONNRESET, ctypes.byref(flag),
                         ctypes.sizeof(flag), None, 0, ctypes.byref(returned), None, None)

    class _UdpSafeSocket(_BaseSocket):
        _udp_connreset_disabled = True

        def __init__(self, family=-1, type=-1, proto=-1, fileno=None):
            super().__init__(family, type, proto, fileno)
            if self.type == socket.SOCK_DGRAM:
                _disable_udp_connreset(self)

    socket.socket = _UdpSafeSocket
