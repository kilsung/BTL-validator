#!/usr/bin/env python3
"""
BBMD (BACnet/IP Broadcast Management Device) 검증 모듈
======================================================
ASHRAE 135-2020 Annex J 기반 BBMD 기능 검증

테스트 항목:
  BBMD-01  Read-BDT          Broadcast Distribution Table 읽기 응답 확인
  BBMD-02  Register-FD       Foreign Device 등록 응답 확인
  BBMD-03  Read-FDT          Foreign Device Table 읽기 응답 확인
  BBMD-04  Delete-FDT-Entry  FDT 항목 삭제 응답 확인 (정리)
  BBMD-05  WhoIs-via-BBMD    Distribute-Broadcast → I-Am 수신 확인

DQA 제출용 결과 딕셔너리 구조:
  {
    'test_id':      'BBMD-01',
    'name':         'Read-BDT',
    'sent_hex':     '81 02 00 04',
    'received_hex': '81 03 00 ...',
    'status':       'PASS' | 'FAIL' | 'SKIP',
    'detail':       '항목 2개 수신',
    'duration_ms':  45,
    'standard_ref': 'ASHRAE 135 Annex J.4',
  }
"""

import socket
import struct
import time
import logging
from typing import List, Dict, Optional, Tuple

log = logging.getLogger(__name__)

# ── BVLCI Function Codes (ASHRAE 135 Annex J, Table J-1) ─────────────────
BVLC_TYPE               = 0x81
BVLC_RESULT             = 0x00   # BVLC-Result (공통 응답)
BVLC_WRITE_BDT          = 0x01
BVLC_READ_BDT           = 0x02
BVLC_READ_BDT_ACK       = 0x03
BVLC_FORWARDED_NPDU     = 0x04
BVLC_REGISTER_FD        = 0x05
BVLC_READ_FDT           = 0x06
BVLC_READ_FDT_ACK       = 0x07
BVLC_DELETE_FDT_ENTRY   = 0x08
BVLC_DISTRIBUTE_BCAST   = 0x09
BVLC_ORIGINAL_UNICAST   = 0x0A
BVLC_ORIGINAL_BROADCAST = 0x0B

# ── BVLC-Result 코드 (Table J-2) ──────────────────────────────────────────
BVLC_RESULT_OK            = 0x0000   # 성공
BVLC_RESULT_WR_BDT_NAK   = 0x0010
BVLC_RESULT_RD_BDT_NAK   = 0x0020
BVLC_RESULT_REGFD_NAK     = 0x0030
BVLC_RESULT_RD_FDT_NAK   = 0x0040
BVLC_RESULT_DEL_FDT_NAK  = 0x0050
BVLC_RESULT_DISTBCAST_NAK = 0x0060

_RESULT_NAMES = {
    BVLC_RESULT_OK:            'Successful completion',
    BVLC_RESULT_WR_BDT_NAK:   'Write-BDT NAK',
    BVLC_RESULT_RD_BDT_NAK:   'Read-BDT NAK',
    BVLC_RESULT_REGFD_NAK:    'Register-Foreign-Device NAK',
    BVLC_RESULT_RD_FDT_NAK:   'Read-FDT NAK',
    BVLC_RESULT_DEL_FDT_NAK:  'Delete-FDT-Entry NAK',
    BVLC_RESULT_DISTBCAST_NAK:'Distribute-Broadcast NAK',
}

DEFAULT_TIMEOUT  = 5.0   # 응답 대기 (초)
FD_LIFETIME_SEC  = 60    # Register-FD 수명 (초)
BACNET_PORT      = 47808


# ── 헬퍼 ──────────────────────────────────────────────────────────────────

def _pack_bvlc(func: int, payload: bytes = b'') -> bytes:
    """BVLCI 패킷 구성: [0x81][func][length 2B][payload]"""
    length = 4 + len(payload)
    return struct.pack('!BBH', BVLC_TYPE, func, length) + payload


def _hex_str(data: bytes) -> str:
    """bytes → 'XX XX XX ...' 형식 문자열"""
    return ' '.join(f'{b:02X}' for b in data)


def _get_local_ip(target_ip: str) -> str:
    """대상 IP 방향으로 나가는 로컬 IP 반환"""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect((target_ip, BACNET_PORT))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '0.0.0.0'


def _parse_bdt_entries(data: bytes) -> List[Dict]:
    """
    Read-BDT-Ack 페이로드 파싱.
    각 항목: IP(4B) + Port(2B) + Mask(4B) = 10 bytes
    """
    entries = []
    offset = 4  # BVLC 헤더 4 bytes 스킵
    while offset + 10 <= len(data):
        ip_bytes = data[offset:offset + 4]
        port     = struct.unpack_from('!H', data, offset + 4)[0]
        mask     = data[offset + 6: offset + 10]
        ip_str   = '.'.join(str(b) for b in ip_bytes)
        mask_str = '.'.join(str(b) for b in mask)
        entries.append({'ip': ip_str, 'port': port, 'mask': mask_str})
        offset  += 10
    return entries


def _parse_fdt_entries(data: bytes) -> List[Dict]:
    """
    Read-FDT-Ack 페이로드 파싱.
    각 항목: IP(4B) + Port(2B) + TTL-원래(2B) + TTL-남은(2B) = 10 bytes
    """
    entries = []
    offset = 4  # BVLC 헤더 4 bytes 스킵
    while offset + 10 <= len(data):
        ip_bytes    = data[offset:offset + 4]
        port        = struct.unpack_from('!H', data, offset + 4)[0]
        ttl_orig    = struct.unpack_from('!H', data, offset + 6)[0]
        ttl_remain  = struct.unpack_from('!H', data, offset + 8)[0]
        ip_str      = '.'.join(str(b) for b in ip_bytes)
        entries.append({
            'ip': ip_str, 'port': port,
            'ttl_original': ttl_orig, 'ttl_remaining': ttl_remain,
        })
        offset += 10
    return entries


def _send_recv(sock: socket.socket,
               bbmd_ip: str, bbmd_port: int,
               pkt: bytes,
               timeout: float = DEFAULT_TIMEOUT) -> Optional[bytes]:
    """패킷 전송 후 응답 수신 (타임아웃 시 None)"""
    sock.settimeout(timeout)
    try:
        sock.sendto(pkt, (bbmd_ip, bbmd_port))
        data, _ = sock.recvfrom(4096)
        return data
    except socket.timeout:
        return None
    except Exception as e:
        log.warning('[BBMD] 소켓 오류: %s', e)
        return None


def _make_result(test_id: str, name: str, standard_ref: str,
                 sent: bytes, recv: Optional[bytes],
                 status: str, detail: str,
                 duration_ms: int) -> Dict:
    return {
        'test_id':      test_id,
        'name':         name,
        'standard_ref': standard_ref,
        'sent_hex':     _hex_str(sent),
        'received_hex': _hex_str(recv) if recv else '(응답 없음)',
        'status':       status,
        'detail':       detail,
        'duration_ms':  duration_ms,
    }


# ── 개별 테스트 함수 ───────────────────────────────────────────────────────

def test_read_bdt(sock: socket.socket,
                  bbmd_ip: str, bbmd_port: int,
                  timeout: float = DEFAULT_TIMEOUT) -> Dict:
    """
    BBMD-01: Read-Broadcast-Distribution-Table
    BACnet/IP 장치가 BBMD 역할을 하는지 1차 판별하는 핵심 테스트.
    """
    pkt = _pack_bvlc(BVLC_READ_BDT)   # 81 02 00 04
    t0  = time.monotonic()
    resp = _send_recv(sock, bbmd_ip, bbmd_port, pkt, timeout)
    dur  = int((time.monotonic() - t0) * 1000)

    if resp is None:
        return _make_result(
            'BBMD-01', 'Read-BDT', 'ASHRAE 135 Annex J.4',
            pkt, None, 'FAIL',
            'Read-BDT 응답 없음 (타임아웃). BBMD 기능 미지원 또는 방화벽 차단.',
            dur,
        )

    if len(resp) < 4 or resp[0] != BVLC_TYPE:
        return _make_result(
            'BBMD-01', 'Read-BDT', 'ASHRAE 135 Annex J.4',
            pkt, resp, 'FAIL',
            f'BACnet/IP 응답 아님 (첫 바이트: 0x{resp[0]:02X})',
            dur,
        )

    func_code = resp[1]
    if func_code == BVLC_READ_BDT_ACK:
        entries = _parse_bdt_entries(resp)
        return _make_result(
            'BBMD-01', 'Read-BDT', 'ASHRAE 135 Annex J.4',
            pkt, resp, 'PASS',
            f'Read-BDT-Ack 수신. BDT 항목 {len(entries)}개: '
            + ', '.join(f"{e['ip']}:{e['port']}/{e['mask']}" for e in entries),
            dur,
        )

    if func_code == BVLC_RESULT:
        result_code = struct.unpack_from('!H', resp, 4)[0] if len(resp) >= 6 else 0xFFFF
        name = _RESULT_NAMES.get(result_code, f'0x{result_code:04X}')
        return _make_result(
            'BBMD-01', 'Read-BDT', 'ASHRAE 135 Annex J.4',
            pkt, resp, 'FAIL',
            f'BVLC-Result 수신: {name}. 장치가 BBMD Read-BDT를 거부.',
            dur,
        )

    return _make_result(
        'BBMD-01', 'Read-BDT', 'ASHRAE 135 Annex J.4',
        pkt, resp, 'FAIL',
        f'예상치 못한 Function Code: 0x{func_code:02X}',
        dur,
    )


def test_register_foreign_device(sock: socket.socket,
                                  bbmd_ip: str, bbmd_port: int,
                                  ttl: int = FD_LIFETIME_SEC,
                                  timeout: float = DEFAULT_TIMEOUT) -> Dict:
    """
    BBMD-02: Register-Foreign-Device
    검증 툴 자신을 BBMD에 Foreign Device로 등록 → 성공(0x0000) 응답 확인.
    """
    # payload: TTL (2 bytes big-endian)
    payload = struct.pack('!H', ttl)
    pkt     = _pack_bvlc(BVLC_REGISTER_FD, payload)  # 81 05 00 06 [TTL]
    t0      = time.monotonic()
    resp    = _send_recv(sock, bbmd_ip, bbmd_port, pkt, timeout)
    dur     = int((time.monotonic() - t0) * 1000)

    if resp is None:
        return _make_result(
            'BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5',
            pkt, None, 'FAIL',
            f'Register-FD 응답 없음 (TTL={ttl}s). BBMD FD 등록 미지원.',
            dur,
        )

    if len(resp) < 6 or resp[0] != BVLC_TYPE:
        return _make_result(
            'BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5',
            pkt, resp, 'FAIL',
            f'유효하지 않은 응답 (len={len(resp)})',
            dur,
        )

    func_code = resp[1]
    if func_code == BVLC_RESULT:
        result_code = struct.unpack_from('!H', resp, 4)[0]
        if result_code == BVLC_RESULT_OK:
            return _make_result(
                'BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5',
                pkt, resp, 'PASS',
                f'BVLC-Result(0x0000=Success). FD 등록 성공 (TTL={ttl}s).',
                dur,
            )
        name = _RESULT_NAMES.get(result_code, f'0x{result_code:04X}')
        return _make_result(
            'BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5',
            pkt, resp, 'FAIL',
            f'BVLC-Result({name}). FD 등록 거부.',
            dur,
        )

    return _make_result(
        'BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5',
        pkt, resp, 'FAIL',
        f'예상치 못한 Function Code: 0x{func_code:02X}',
        dur,
    )


def test_read_fdt(sock: socket.socket,
                  bbmd_ip: str, bbmd_port: int,
                  timeout: float = DEFAULT_TIMEOUT) -> Dict:
    """
    BBMD-03: Read-Foreign-Device-Table
    FDT 읽기 지원 여부 확인 (Register-FD 성공 후 실행하면 등록 항목 포함).
    """
    pkt = _pack_bvlc(BVLC_READ_FDT)   # 81 06 00 04
    t0  = time.monotonic()
    resp = _send_recv(sock, bbmd_ip, bbmd_port, pkt, timeout)
    dur  = int((time.monotonic() - t0) * 1000)

    if resp is None:
        return _make_result(
            'BBMD-03', 'Read-FDT', 'ASHRAE 135 Annex J.6',
            pkt, None, 'FAIL',
            'Read-FDT 응답 없음 (타임아웃).',
            dur,
        )

    if len(resp) < 4 or resp[0] != BVLC_TYPE:
        return _make_result(
            'BBMD-03', 'Read-FDT', 'ASHRAE 135 Annex J.6',
            pkt, resp, 'FAIL',
            f'유효하지 않은 응답 (len={len(resp)})',
            dur,
        )

    func_code = resp[1]
    if func_code == BVLC_READ_FDT_ACK:
        entries = _parse_fdt_entries(resp)
        detail = (
            f'Read-FDT-Ack 수신. FDT 항목 {len(entries)}개.'
        )
        if entries:
            detail += ' [' + ', '.join(
                f"{e['ip']}:{e['port']} TTL={e['ttl_remaining']}s"
                for e in entries
            ) + ']'
        return _make_result(
            'BBMD-03', 'Read-FDT', 'ASHRAE 135 Annex J.6',
            pkt, resp, 'PASS', detail, dur,
        )

    if func_code == BVLC_RESULT:
        result_code = struct.unpack_from('!H', resp, 4)[0] if len(resp) >= 6 else 0xFFFF
        name = _RESULT_NAMES.get(result_code, f'0x{result_code:04X}')
        return _make_result(
            'BBMD-03', 'Read-FDT', 'ASHRAE 135 Annex J.6',
            pkt, resp, 'FAIL',
            f'BVLC-Result({name}).',
            dur,
        )

    return _make_result(
        'BBMD-03', 'Read-FDT', 'ASHRAE 135 Annex J.6',
        pkt, resp, 'FAIL',
        f'예상치 못한 Function Code: 0x{func_code:02X}',
        dur,
    )


def test_delete_fdt_entry(sock: socket.socket,
                           bbmd_ip: str, bbmd_port: int,
                           local_ip: str,
                           local_port: int = BACNET_PORT,
                           timeout: float = DEFAULT_TIMEOUT) -> Dict:
    """
    BBMD-04: Delete-Foreign-Device-Table-Entry
    Register-FD로 등록한 자신의 항목을 삭제 → 정리 및 지원 여부 확인.
    payload: IP(4B) + Port(2B) = 6 bytes
    """
    try:
        ip_bytes = bytes(int(x) for x in local_ip.split('.'))
    except Exception:
        return _make_result(
            'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
            b'', None, 'SKIP',
            f'로컬 IP 파싱 실패: {local_ip}',
            0,
        )

    payload = ip_bytes + struct.pack('!H', local_port)
    pkt     = _pack_bvlc(BVLC_DELETE_FDT_ENTRY, payload)  # 81 08 00 0A [IP][Port]
    t0      = time.monotonic()
    resp    = _send_recv(sock, bbmd_ip, bbmd_port, pkt, timeout)
    dur     = int((time.monotonic() - t0) * 1000)

    if resp is None:
        return _make_result(
            'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
            pkt, None, 'FAIL',
            'Delete-FDT-Entry 응답 없음 (타임아웃).',
            dur,
        )

    if len(resp) < 6 or resp[0] != BVLC_TYPE:
        return _make_result(
            'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
            pkt, resp, 'FAIL',
            f'유효하지 않은 응답 (len={len(resp)})',
            dur,
        )

    func_code = resp[1]
    if func_code == BVLC_RESULT:
        result_code = struct.unpack_from('!H', resp, 4)[0]
        if result_code == BVLC_RESULT_OK:
            return _make_result(
                'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
                pkt, resp, 'PASS',
                f'BVLC-Result(0x0000=Success). FDT 항목 삭제 성공.',
                dur,
            )
        name = _RESULT_NAMES.get(result_code, f'0x{result_code:04X}')
        # Delete-FDT NAK은 항목 없음일 수 있어 WARN으로 처리
        status = 'FAIL'
        if result_code == BVLC_RESULT_DEL_FDT_NAK:
            status = 'WARN'  # 항목 없음(이미 삭제됨)으로 해석 가능
        return _make_result(
            'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
            pkt, resp, status,
            f'BVLC-Result({name}). 삭제 대상 항목 없음 또는 권한 거부.',
            dur,
        )

    return _make_result(
        'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
        pkt, resp, 'FAIL',
        f'예상치 못한 Function Code: 0x{func_code:02X}',
        dur,
    )


def test_whois_via_bbmd(sock: socket.socket,
                         bbmd_ip: str, bbmd_port: int,
                         device_id: int,
                         recv_timeout: float = 8.0) -> Dict:
    """
    BBMD-05: WhoIs via Distribute-Broadcast-To-Network
    BBMD에 WhoIs를 감싼 Distribute-Broadcast 전송 → 장치로부터 I-Am 수신 확인.
    Foreign Device로 등록된 상태에서 실행해야 I-Am이 포워딩됨.

    패킷 구조:
      BVLC: 81 09 [length]  (Distribute-Broadcast-To-Network)
      NPDU: 01 20 FF FF 00 FF  (global broadcast, hop count=255)
      APDU: 10 08             (Unconfirmed-Request, WhoIs, 전체 범위)
    """
    # WhoIs NPDU + APDU (global broadcast)
    npdu = bytes([0x01, 0x20, 0xFF, 0xFF, 0x00, 0xFF])
    apdu = bytes([0x10, 0x08])
    payload = npdu + apdu
    pkt = _pack_bvlc(BVLC_DISTRIBUTE_BCAST, payload)

    t0   = time.monotonic()
    sent_hex = _hex_str(pkt)

    try:
        sock.sendto(pkt, (bbmd_ip, bbmd_port))
    except Exception as e:
        dur = int((time.monotonic() - t0) * 1000)
        return _make_result(
            'BBMD-05', 'WhoIs via BBMD', 'ASHRAE 135 Annex J.9',
            pkt, None, 'FAIL',
            f'패킷 전송 실패: {e}',
            dur,
        )

    # I-Am 또는 BVLC-Result(NAK) 수신 대기
    sock.settimeout(recv_timeout)
    received_bytes = None
    status  = 'FAIL'
    detail  = 'I-Am 응답 없음 (타임아웃).'

    deadline = time.monotonic() + recv_timeout
    while time.monotonic() < deadline:
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            break
        except Exception:
            break

        if len(data) < 4:
            continue

        # BVLC-Result NAK 확인
        if data[0] == BVLC_TYPE and data[1] == BVLC_RESULT and len(data) >= 6:
            result_code = struct.unpack_from('!H', data, 4)[0]
            if result_code != BVLC_RESULT_OK:
                name = _RESULT_NAMES.get(result_code, f'0x{result_code:04X}')
                received_bytes = data
                status = 'FAIL'
                detail = f'BVLC-Result({name}). BBMD가 배포 거부.'
                break

        # Forwarded-NPDU 또는 Original-Unicast 내 I-Am 확인
        # I-Am APDU: 10 00 [ObjectId 4B] [MaxAPDU 4B] [Segmentation 1B] [VendorId 2B] ...
        npdu_start = 4
        if data[1] == BVLC_FORWARDED_NPDU:
            # Forwarded: BVLC(4B) + 원본 IP(4B) + 원본 Port(2B) = 10B
            npdu_start = 10

        apdu_data = data[npdu_start:]
        # NPDU 길이 건너뛰기 (단순화: NPDU control byte만 확인)
        if len(apdu_data) >= 2:
            # NPDU skip
            npdu_ctrl = apdu_data[1] if len(apdu_data) > 1 else 0
            npdu_len = 2
            if npdu_ctrl & 0x20:   # dest specifier
                if len(apdu_data) >= npdu_len + 3:
                    npdu_len += 3 + apdu_data[npdu_len + 2]  # net(2)+len(1)+adr(n)
            if npdu_ctrl & 0x08:   # src specifier
                if len(apdu_data) >= npdu_len + 3:
                    npdu_len += 3 + apdu_data[npdu_len + 2]
            if npdu_ctrl & 0x08 or npdu_ctrl & 0x20:
                npdu_len += 1   # hop count

            apdu_inner = apdu_data[npdu_len:]
            if len(apdu_inner) >= 2 and apdu_inner[0] == 0x10 and apdu_inner[1] == 0x00:
                received_bytes = data
                status = 'PASS'
                detail = f'I-Am 응답 수신 from {addr[0]}:{addr[1]}.'
                break

    dur = int((time.monotonic() - t0) * 1000)
    return _make_result(
        'BBMD-05', 'WhoIs via BBMD', 'ASHRAE 135 Annex J.9',
        pkt, received_bytes, status, detail, dur,
    )


# ── 전체 테스트 실행 ──────────────────────────────────────────────────────

def run_bbmd_tests(bbmd_ip: str,
                   bbmd_port: int = BACNET_PORT,
                   device_id: int = 4194302,
                   timeout: float = DEFAULT_TIMEOUT,
                   run_whois: bool = True,
                   fd_ttl: int = FD_LIFETIME_SEC) -> Dict:
    """
    BBMD 전체 검증 실행.
    반환:
      {
        'bbmd_ip':    ...,
        'bbmd_port':  ...,
        'local_ip':   ...,
        'timestamp':  ...,
        'tests':      [result_dict, ...],
        'summary':    { 'total':N, 'pass':N, 'fail':N, 'skip':N, 'warn':N },
        'bbmd_supported': True | False,   # BBMD-01 PASS 여부
      }
    """
    from datetime import datetime

    local_ip  = _get_local_ip(bbmd_ip)
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    results   = []

    log.info('[BBMD] 검증 시작: target=%s:%d local=%s', bbmd_ip, bbmd_port, local_ip)

    # ── UDP 소켓 생성 (로컬 포트 47808 바인딩 시도, 실패 시 임의 포트) ──
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    local_port = BACNET_PORT
    try:
        sock.bind(('', local_port))
    except OSError:
        sock.bind(('', 0))
        local_port = sock.getsockname()[1]
        log.info('[BBMD] 포트 47808 사용 중 → 임시 포트 %d 사용', local_port)

    try:
        # BBMD-01: Read-BDT
        r01 = test_read_bdt(sock, bbmd_ip, bbmd_port, timeout)
        results.append(r01)
        bbmd_supported = (r01['status'] == 'PASS')

        if not bbmd_supported:
            # BBMD 미지원 시 나머지 테스트 SKIP
            for tid, name, ref in [
                ('BBMD-02', 'Register-Foreign-Device', 'ASHRAE 135 Annex J.5'),
                ('BBMD-03', 'Read-FDT',               'ASHRAE 135 Annex J.6'),
                ('BBMD-04', 'Delete-FDT-Entry',       'ASHRAE 135 Annex J.7'),
                ('BBMD-05', 'WhoIs via BBMD',         'ASHRAE 135 Annex J.9'),
            ]:
                results.append(_make_result(
                    tid, name, ref, b'', None, 'SKIP',
                    'BBMD-01 실패. 장치가 BBMD 기능을 지원하지 않아 건너뜀.',
                    0,
                ))
        else:
            # BBMD-02: Register-FD
            r02 = test_register_foreign_device(
                sock, bbmd_ip, bbmd_port, fd_ttl, timeout)
            results.append(r02)

            # BBMD-03: Read-FDT (Register 성공 여부와 무관하게 실행)
            r03 = test_read_fdt(sock, bbmd_ip, bbmd_port, timeout)
            results.append(r03)

            # BBMD-04: Delete-FDT-Entry (Register 성공 시에만 의미 있음)
            if r02['status'] == 'PASS':
                r04 = test_delete_fdt_entry(
                    sock, bbmd_ip, bbmd_port, local_ip, local_port, timeout)
            else:
                r04 = _make_result(
                    'BBMD-04', 'Delete-FDT-Entry', 'ASHRAE 135 Annex J.7',
                    b'', None, 'SKIP',
                    'Register-FD 실패 → 삭제 테스트 건너뜀.',
                    0,
                )
            results.append(r04)

            # BBMD-05: WhoIs via BBMD (선택적)
            if run_whois:
                # FD 재등록 (WhoIs 배포 전 FD로 등록해야 포워딩 가능)
                if r02['status'] == 'PASS':
                    test_register_foreign_device(
                        sock, bbmd_ip, bbmd_port, fd_ttl, timeout)
                r05 = test_whois_via_bbmd(
                    sock, bbmd_ip, bbmd_port, device_id, recv_timeout=8.0)
                # WhoIs 후 FD 정리
                test_delete_fdt_entry(
                    sock, bbmd_ip, bbmd_port, local_ip, local_port, timeout)
            else:
                r05 = _make_result(
                    'BBMD-05', 'WhoIs via BBMD', 'ASHRAE 135 Annex J.9',
                    b'', None, 'SKIP',
                    '사용자 요청으로 건너뜀.',
                    0,
                )
            results.append(r05)

    finally:
        sock.close()

    # 요약 통계
    stat = {'total': len(results), 'pass': 0, 'fail': 0, 'skip': 0, 'warn': 0}
    for r in results:
        s = r['status'].lower()
        if s in stat:
            stat[s] += 1

    log.info('[BBMD] 검증 완료: PASS=%d FAIL=%d SKIP=%d WARN=%d',
             stat['pass'], stat['fail'], stat['skip'], stat['warn'])

    return {
        'bbmd_ip':       bbmd_ip,
        'bbmd_port':     bbmd_port,
        'local_ip':      local_ip,
        'local_port':    local_port,
        'timestamp':     timestamp,
        'tests':         results,
        'summary':       stat,
        'bbmd_supported': bbmd_supported if 'bbmd_supported' in dir() else False,
    }


# ── 엑셀 리포트 생성 ──────────────────────────────────────────────────────

def generate_bbmd_report(run_result: Dict,
                          output_dir: str = '.') -> Tuple[str, str]:
    """
    BBMD 검증 결과를 엑셀 리포트로 저장.
    반환: (절대경로, 파일명)
    """
    try:
        import openpyxl
        from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
        from openpyxl.utils import get_column_letter
    except ImportError:
        raise RuntimeError('openpyxl 미설치. pip install openpyxl')

    from datetime import datetime
    from pathlib import Path

    ts       = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'BBMD_Validation_Report_{ts}.xlsx'
    filepath = str(Path(output_dir) / filename)

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = 'BBMD 검증 결과'

    # ── 색상 팔레트 ──
    C_HEADER   = PatternFill('solid', fgColor='1B2A3B')
    C_PASS     = PatternFill('solid', fgColor='D5F5E3')
    C_FAIL     = PatternFill('solid', fgColor='FADBD8')
    C_SKIP     = PatternFill('solid', fgColor='F2F3F4')
    C_WARN     = PatternFill('solid', fgColor='FEF9E7')
    C_TITLE    = PatternFill('solid', fgColor='2980B9')
    C_SUMMARY  = PatternFill('solid', fgColor='EBF5FB')

    font_white = Font(bold=True, color='FFFFFF', size=11)
    font_bold  = Font(bold=True, size=10)
    font_norm  = Font(size=10)
    align_c    = Alignment(horizontal='center', vertical='center')
    align_l    = Alignment(horizontal='left', vertical='center', wrap_text=True)

    thin = Side(style='thin', color='D5D8DC')
    border = Border(left=thin, right=thin, top=thin, bottom=thin)

    row = 1

    # ── 제목 ──────────────────────────────────────────────────
    ws.merge_cells(f'A{row}:G{row}')
    ws[f'A{row}'] = 'BACnet BBMD 검증 결과'
    ws[f'A{row}'].font = Font(bold=True, color='FFFFFF', size=14)
    ws[f'A{row}'].fill = C_TITLE
    ws[f'A{row}'].alignment = align_c
    ws.row_dimensions[row].height = 32
    row += 1

    # ── 검증 정보 ──────────────────────────────────────────────
    info_rows = [
        ('BBMD IP 주소',  run_result.get('bbmd_ip', '')),
        ('BBMD UDP 포트', str(run_result.get('bbmd_port', ''))),
        ('검증 도구 IP',  run_result.get('local_ip', '')),
        ('검증 일시',     run_result.get('timestamp', '')),
        ('BBMD 지원 여부', '지원' if run_result.get('bbmd_supported') else '미지원'),
    ]
    for label, value in info_rows:
        ws[f'A{row}'] = label
        ws[f'A{row}'].font = font_bold
        ws[f'A{row}'].fill = C_SUMMARY
        ws[f'A{row}'].alignment = align_c
        ws.merge_cells(f'B{row}:G{row}')
        ws[f'B{row}'] = value
        ws[f'B{row}'].font = font_norm
        ws[f'B{row}'].alignment = align_l
        for col in range(1, 8):
            ws.cell(row=row, column=col).border = border
        row += 1

    row += 1  # 빈 행

    # ── 요약 통계 ──────────────────────────────────────────────
    ws.merge_cells(f'A{row}:G{row}')
    ws[f'A{row}'] = '검증 요약'
    ws[f'A{row}'].font = font_white
    ws[f'A{row}'].fill = C_HEADER
    ws[f'A{row}'].alignment = align_c
    ws.row_dimensions[row].height = 22
    row += 1

    stat = run_result.get('summary', {})
    for label, key, fill in [
        ('전체', 'total', C_SUMMARY),
        ('PASS', 'pass',  C_PASS),
        ('FAIL', 'fail',  C_FAIL),
        ('SKIP', 'skip',  C_SKIP),
        ('WARN', 'warn',  C_WARN),
    ]:
        ws[f'A{row}'] = label
        ws[f'A{row}'].font = font_bold
        ws[f'A{row}'].fill = fill
        ws[f'A{row}'].alignment = align_c
        ws.merge_cells(f'B{row}:G{row}')
        ws[f'B{row}'] = stat.get(key, 0)
        ws[f'B{row}'].font = font_norm
        ws[f'B{row}'].alignment = align_l
        for col in range(1, 8):
            ws.cell(row=row, column=col).border = border
        row += 1

    row += 1  # 빈 행

    # ── 테스트 결과 테이블 ─────────────────────────────────────
    headers = ['테스트 ID', '테스트명', '표준 조항', '상태',
               '응답시간(ms)', '세부 결과', '전송 패킷 (HEX)']
    col_widths = [12, 22, 22, 10, 14, 45, 35]

    for ci, (h, w) in enumerate(zip(headers, col_widths), start=1):
        cell = ws.cell(row=row, column=ci, value=h)
        cell.font = font_white
        cell.fill = C_HEADER
        cell.alignment = align_c
        cell.border = border
        ws.column_dimensions[get_column_letter(ci)].width = w
    ws.row_dimensions[row].height = 22
    row += 1

    status_fill = {
        'PASS': C_PASS, 'FAIL': C_FAIL,
        'SKIP': C_SKIP, 'WARN': C_WARN,
    }
    for r in run_result.get('tests', []):
        fill = status_fill.get(r['status'], C_SKIP)
        row_data = [
            r['test_id'],
            r['name'],
            r['standard_ref'],
            r['status'],
            r['duration_ms'],
            r['detail'],
            r['sent_hex'],
        ]
        for ci, val in enumerate(row_data, start=1):
            cell = ws.cell(row=row, column=ci, value=val)
            cell.font  = font_bold if ci == 4 else font_norm
            cell.fill  = fill
            cell.alignment = align_c if ci in (1, 4, 5) else align_l
            cell.border = border
        ws.row_dimensions[row].height = 40
        row += 1

    row += 1
    # ── 수신 패킷 상세 시트 (Wireshark 스타일) ─────────────────────
    ws2 = wb.create_sheet('수신 패킷 상세')

    # 컬럼 너비 설정
    ws2_cols = {
        'A': 12, 'B': 22, 'C': 10, 'D': 8,
        'E': 14, 'F': 30, 'G': 10, 'H': 55,
    }
    for col_letter, width in ws2_cols.items():
        ws2.column_dimensions[col_letter].width = width

    ws2_headers = [
        '테스트 ID', '테스트명 / 표준 조항', '방향', '상태',
        'BVLC Function', 'Function명', 'Length(bytes)', '페이로드 상세'
    ]
    for ci, h in enumerate(ws2_headers, start=1):
        cell = ws2.cell(row=1, column=ci, value=h)
        cell.font = font_white
        cell.fill = C_HEADER
        cell.alignment = align_c
        cell.border = border
    ws2.row_dimensions[1].height = 22

    ws2_row = 2
    for r in run_result.get('tests', []):
        tx_parsed = _parse_bvlc_hex(r.get('sent_hex', ''))
        rx_parsed = _parse_bvlc_hex(r.get('received_hex', ''))
        status_fill2 = status_fill.get(r['status'], C_SKIP)

        for direction, pkt, parsed in [
            ('TX (전송)', r.get('sent_hex', ''), tx_parsed),
            ('RX (수신)', r.get('received_hex', ''), rx_parsed),
        ]:
            row_data = [
                r['test_id'],
                f"{r['name']}\n{r['standard_ref']}",
                direction,
                r['status'] if direction.startswith('TX') else '',
                parsed.get('func_hex', pkt),
                parsed.get('func_name', ''),
                parsed.get('length', ''),
                parsed.get('detail', pkt),
            ]
            for ci, val in enumerate(row_data, start=1):
                cell = ws2.cell(row=ws2_row, column=ci, value=val)
                if ci == 4:
                    cell.font = font_bold
                    cell.fill = status_fill2
                elif direction.startswith('TX'):
                    cell.fill = PatternFill('solid', fgColor='FEF9E7')
                else:
                    rx_fill = C_PASS if r['status'] == 'PASS' else (C_FAIL if r['status'] == 'FAIL' else C_SKIP)
                    cell.fill = rx_fill
                cell.font = font_bold if ci == 4 else font_norm
                cell.alignment = align_c if ci in (1, 3, 4, 5, 7) else align_l
                cell.border = border
            ws2.row_dimensions[ws2_row].height = 38
            ws2_row += 1

    wb.save(filepath)
    log.info('[BBMD] 리포트 저장: %s', filepath)
    return filepath, filename


def _parse_bvlc_hex(hex_str: str) -> dict:
    """
    BVLC 16진수 문자열을 파싱하여 필드별 딕셔너리 반환.
    예: '81 03 00 0E 0A AF BF 78 BA C0 FF FF FF FF'
    """
    FUNC_NAMES = {
        0x00: 'BVLC-Result',
        0x01: 'Write-Broadcast-Distribution-Table',
        0x02: 'Read-Broadcast-Distribution-Table',
        0x03: 'Read-BDT-Ack',
        0x04: 'Forwarded-NPDU',
        0x05: 'Register-Foreign-Device',
        0x06: 'Read-Foreign-Device-Table',
        0x07: 'Read-FDT-Ack',
        0x08: 'Delete-Foreign-Device-Table-Entry',
        0x09: 'Distribute-Broadcast-to-Network',
        0x0A: 'Original-Unicast-NPDU',
        0x0B: 'Original-Broadcast-NPDU',
    }
    RESULT_CODES = {
        0x0000: 'Successful',
        0x0010: 'Write-BDT NAK',
        0x0020: 'Read-BDT NAK',
        0x0030: 'Register-FD NAK',
        0x0040: 'Read-FDT NAK',
        0x0050: 'Delete-FDT-Entry NAK',
    }

    empty = {'func_hex': '', 'func_name': '', 'length': '', 'detail': hex_str}
    if not hex_str or hex_str.strip() in ('(응답 없음)', ''):
        empty['detail'] = '(응답 없음)'
        return empty

    try:
        b = [int(x, 16) for x in hex_str.strip().split() if x]
    except ValueError:
        return empty

    if len(b) < 4:
        return empty

    if b[0] != 0x81:
        return {**empty, 'func_name': f'비BACnet/IP (Type=0x{b[0]:02X})'}

    func    = b[1]
    length  = (b[2] << 8) | b[3]
    fname   = FUNC_NAMES.get(func, f'Unknown(0x{func:02X})')
    func_hex = f'0x{func:02X}'
    lines   = []

    # 함수별 페이로드 파싱
    if func == 0x00 and len(b) >= 6:  # BVLC-Result
        code = (b[4] << 8) | b[5]
        rname = RESULT_CODES.get(code, f'Unknown(0x{code:04X})')
        lines.append(f'Result Code: {rname} (0x{code:04X})')

    elif func == 0x03 and len(b) >= 4:  # Read-BDT-Ack
        n = (len(b) - 4) // 10
        lines.append(f'BDT 항목 수: {n}개')
        for i in range(n):
            off = 4 + i * 10
            if off + 10 > len(b):
                break
            ip   = '.'.join(str(x) for x in b[off:off+4])
            port = (b[off+4] << 8) | b[off+5]
            mask = '.'.join(str(x) for x in b[off+6:off+10])
            lines.append(f'  BDT[{i}]: IP={ip}, Port={port}, Mask={mask}')

    elif func == 0x05 and len(b) >= 6:  # Register-Foreign-Device
        ttl = (b[4] << 8) | b[5]
        lines.append(f'TTL: {ttl}초')

    elif func == 0x07 and len(b) >= 4:  # Read-FDT-Ack
        n = (len(b) - 4) // 10
        lines.append(f'FDT 항목 수: {n}개')
        for i in range(n):
            off = 4 + i * 10
            if off + 10 > len(b):
                break
            ip    = '.'.join(str(x) for x in b[off:off+4])
            port  = (b[off+4] << 8) | b[off+5]
            ttl_o = (b[off+6] << 8) | b[off+7]
            ttl_r = (b[off+8] << 8) | b[off+9]
            lines.append(f'  FDT[{i}]: IP={ip}, Port={port}, TTL-원본={ttl_o}s, TTL-잔여={ttl_r}s')

    elif func in (0x08,) and len(b) >= 10:  # Delete-FDT-Entry
        ip   = '.'.join(str(x) for x in b[4:8])
        port = (b[8] << 8) | b[9]
        lines.append(f'삭제 대상: IP={ip}, Port={port}')

    if not lines:
        payload = b[4:]
        if payload:
            lines.append('Payload: ' + ' '.join(f'{x:02X}' for x in payload))

    detail = f'[HEX] {hex_str}\n' + '\n'.join(lines)
    return {
        'func_hex':  func_hex,
        'func_name': fname,
        'length':    length,
        'detail':    detail,
    }
