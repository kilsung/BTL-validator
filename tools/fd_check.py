#!/usr/bin/env python3
"""
FD (Foreign Device) 검증 모듈
=======================================================
ASHRAE 135-2020 Annex J.5 기반 FD 기능 검증

검증 시나리오:
  검증 툴 PC가 임시 BBMD 역할(수신 전용)을 하며,
  DUT(ACPi)가 FD로 등록 요청(Register-Foreign-Device)을 전송하는지 확인.
  등록 성공 후 BACnet WhoIs/I-Am 통신을 검증.

테스트 항목:
  FD-01  FD-Register-Recv    DUT의 Register-FD 수신 및 BVLC-Result(Success) 응답
  FD-02  WhoIs-I-Am          WhoIs 전송 → DUT I-Am 응답 확인
  FD-03  FD-Renew            DUT의 등록 갱신(재등록) 여부 확인 (선택)

DQA 제출용 결과 딕셔너리 구조 (bbmd_check.py 와 동일):
  {
    'test_id':      'FD-01',
    'name':         'FD-Register-Recv',
    'sent_hex':     '81 00 00 06 00 00',   # BVLC-Result(Success)
    'received_hex': '81 05 00 06 ...',     # Register-FD from DUT
    'status':       'PASS' | 'FAIL' | 'SKIP',
    'detail':       'DUT 10.175.191.120:47808 FD 등록 완료 (TTL=3600s)',
    'duration_ms':  120,
    'standard_ref': 'ASHRAE 135 Annex J.5',
  }
"""

import socket
import struct
import time
import logging
import threading
from typing import Dict, List, Optional, Tuple

log = logging.getLogger(__name__)

# ── BVLCI 코드 (ASHRAE 135 Annex J, Table J-1) ─────────────────────────────
BVLC_TYPE               = 0x81
BVLC_RESULT             = 0x00
BVLC_REGISTER_FD        = 0x05
BVLC_ORIGINAL_UNICAST   = 0x0A
BVLC_ORIGINAL_BROADCAST = 0x0B
BVLC_FORWARDED_NPDU     = 0x04
BVLC_DISTRIBUTE_BCAST   = 0x09

BVLC_RESULT_OK = 0x0000

DEFAULT_WAIT_TIMEOUT  = 60.0  # FD 등록 대기 최대 시간 (초)
DEFAULT_WHOIS_TIMEOUT = 5.0   # WhoIs 응답 대기 시간 (초)
BACNET_PORT   = 47808          # DUT BACnet 표준 포트
FD_BBMD_PORT  = 47808          # BBMD 포트 (BACnet 표준: 47808)
                               # FD 검증 전 BAC0 disconnect 후 사용


# ── 헬퍼 ──────────────────────────────────────────────────────────────────

def _hex_str(data: bytes) -> str:
    """bytes → 'XX XX XX ...' 형식 문자열"""
    return ' '.join(f'{b:02X}' for b in data)


def _pack_bvlc(func: int, payload: bytes = b'') -> bytes:
    """BVLCI 패킷 구성"""
    length = 4 + len(payload)
    return struct.pack('!BBH', BVLC_TYPE, func, length) + payload


def _pack_bvlc_result(result_code: int = BVLC_RESULT_OK) -> bytes:
    """BVLC-Result 응답 패킷 생성"""
    return _pack_bvlc(BVLC_RESULT, struct.pack('!H', result_code))


def _make_result(test_id: str, name: str, standard_ref: str,
                 sent: bytes, recv: Optional[bytes],
                 status: str, detail: str,
                 duration_ms: int) -> Dict:
    return {
        'test_id':      test_id,
        'name':         name,
        'standard_ref': standard_ref,
        'sent_hex':     _hex_str(sent) if sent else '(없음)',
        'received_hex': _hex_str(recv) if recv else '(응답 없음)',
        'status':       status,
        'detail':       detail,
        'duration_ms':  duration_ms,
    }


def _parse_fd_registration(data: bytes) -> Optional[Dict]:
    """
    Register-Foreign-Device 패킷 파싱.
    payload: TTL(2B)
    반환: {'ttl': int} 또는 None
    """
    if len(data) < 6 or data[0] != BVLC_TYPE or data[1] != BVLC_REGISTER_FD:
        return None
    ttl = struct.unpack_from('!H', data, 4)[0] if len(data) >= 6 else 0
    return {'ttl': ttl}


# ── FD-01: FD 등록 수신 검증 ──────────────────────────────────────────────

def _open_shared_sock(listen_ip: str, listen_port: int) -> Tuple[Optional[socket.socket], Optional[str]]:
    """
    FD-01/FD-02가 공유할 UDP 소켓 생성 및 바인드.
    반환: (sock, 에러메시지 | None)
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    if hasattr(socket, 'SO_REUSEPORT'):
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    last_err = None
    for attempt in range(5):
        try:
            sock.bind((listen_ip, listen_port))
            return sock, None
        except OSError as e:
            last_err = e
            if attempt < 4:
                time.sleep(0.5)
    sock.close()
    return None, (
        f'포트 {listen_port} 바인드 실패: {last_err}. '
        '검증 툴이 BACnet에 연결된 상태에서는 포트가 점유됩니다. '
        '"FD 검증 실행" 버튼을 누르면 BACnet 연결이 자동 해제되어 재시도됩니다. '
        '여전히 실패 시 터미널에서 포트 점유 프로세스를 확인하세요: '
        f'sudo ss -ulnp | grep {listen_port}'
    )


def test_fd_register_recv(listen_ip: str = '0.0.0.0',
                          listen_port: int = BACNET_PORT,
                          wait_timeout: float = DEFAULT_WAIT_TIMEOUT,
                          progress_cb=None,
                          sock: Optional[socket.socket] = None
                          ) -> Tuple[Dict, Optional[str]]:
    """
    FD-01: DUT가 검증 PC(BBMD 역할)로 Register-Foreign-Device 전송하는지 확인.

    sock: 공유 소켓 (None이면 내부에서 생성·소멸). FD-01/FD-02 공유 시 외부에서 전달.
    반환: (result_dict, dut_ip_str | None)
    """
    t0 = time.monotonic()
    own_sock = sock is None

    if own_sock:
        sock, err = _open_shared_sock(listen_ip, listen_port)
        if sock is None:
            dur = int((time.monotonic() - t0) * 1000)
            return _make_result(
                'FD-01', 'FD-Register-Recv', 'ASHRAE 135 Annex J.5',
                b'', None, 'FAIL', err, dur,
            ), None

    sock.settimeout(2.0)
    dut_ip = None
    dut_port = None
    fd_data = None
    response_pkt = _pack_bvlc_result(BVLC_RESULT_OK)

    if progress_cb:
        progress_cb(f'FD-01: 포트 {listen_port}에서 DUT Register-FD 수신 대기 중... (최대 {wait_timeout:.0f}초)')

    deadline = time.monotonic() + wait_timeout
    while time.monotonic() < deadline:
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            remain = int(deadline - time.monotonic())
            if progress_cb and remain % 5 == 0 and remain > 0:
                progress_cb(f'FD-01: 대기 중... 잔여 {remain}초 (DUT IP에서 Register-FD 패킷 미수신)')
            continue
        except Exception as e:
            log.warning('[FD-01] 수신 오류: %s', e)
            break

        if len(data) < 4 or data[0] != BVLC_TYPE:
            continue

        if data[1] == BVLC_REGISTER_FD:
            fd_data = data
            dut_ip, dut_port = addr
            # BVLC-Result(Success) 응답
            try:
                sock.sendto(response_pkt, addr)
            except Exception as e:
                log.warning('[FD-01] 응답 전송 실패: %s', e)
            log.info('[FD-01] Register-FD 수신: from=%s:%d', dut_ip, dut_port)
            break

    if own_sock:
        sock.close()
    dur = int((time.monotonic() - t0) * 1000)

    if fd_data is None:
        return _make_result(
            'FD-01', 'FD-Register-Recv', 'ASHRAE 135 Annex J.5',
            b'', None, 'FAIL',
            f'타임아웃 {wait_timeout:.0f}초 내 Register-FD 미수신. '
            'DUT 설정 확인: bacstac_rt.conf의 Foreign_Device=1, Fd_Bbmd_Ip=검증PC_IP',
            dur,
        ), None

    fd_info = _parse_fd_registration(fd_data)
    ttl = fd_info['ttl'] if fd_info else 0
    return _make_result(
        'FD-01', 'FD-Register-Recv', 'ASHRAE 135 Annex J.5',
        response_pkt,  # 전송: BVLC-Result(Success)
        fd_data,       # 수신: Register-FD from DUT
        'PASS',
        f'DUT {dut_ip}:{dut_port} → Register-FD 수신 완료. TTL={ttl}초. '
        f'BVLC-Result(0x0000=Success) 응답 전송.',
        dur,
    ), dut_ip


# ── FD-02: WhoIs → I-Am 검증 ──────────────────────────────────────────────

def test_fd_whois_iam(dut_ip: str,
                     dut_port: int = BACNET_PORT,
                     own_ip: str = '0.0.0.0',
                     own_port: int = BACNET_PORT,
                     timeout: float = DEFAULT_WHOIS_TIMEOUT,
                     sock: Optional[socket.socket] = None) -> Dict:
    """
    FD-02: WhoIs → I-Am 검증.
    FD 등록 후 DUT에 WhoIs를 Original-Unicast로 전송 → I-Am 수신 확인.

    sock: 공유 소켓 (FD-01과 같은 포트 47808 소켓을 재사용해야 함).
          DUT(FD)는 I-Am을 BBMD(=검증PC, 포트 47808)로 Distribute-Broadcast 전송하므로
          반드시 포트 47808 소켓으로 수신해야 한다.
    WhoIs APDU: 10 08 (Unconfirmed-Request, WhoIs)
    I-Am  APDU: 10 00 ...
    """
    t0 = time.monotonic()

    # NPDU + APDU (Direct-unicast WhoIs)
    npdu = bytes([0x01, 0x00])           # control=0x00 (data), NPDU version 1
    apdu = bytes([0x10, 0x08])           # Unconfirmed-Request, WhoIs (전체 범위)
    payload = npdu + apdu
    pkt = _pack_bvlc(BVLC_ORIGINAL_UNICAST, payload)

    own_sock = sock is None
    if own_sock:
        # 공유 소켓 없으면 새로 생성 (포트 47808 바인드 필수)
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        try:
            sock.bind((own_ip, own_port))
        except OSError as e:
            dur = int((time.monotonic() - t0) * 1000)
            sock.close()
            return _make_result(
                'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
                pkt, None, 'FAIL',
                f'소켓 바인드 실패(포트 {own_port}): {e}. '
                'DUT(FD)의 I-Am은 BBMD(포트 47808)로 전송되므로 포트 47808 필수.',
                dur,
            )

    try:
        sock.sendto(pkt, (dut_ip, dut_port))
        log.info('[FD-02] WhoIs 전송: → %s:%d', dut_ip, dut_port)
    except Exception as e:
        dur = int((time.monotonic() - t0) * 1000)
        if own_sock:
            sock.close()
        return _make_result(
            'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
            pkt, None, 'FAIL',
            f'WhoIs 전송 실패: {e}',
            dur,
        )

    # DUT(FD)는 I-Am을 BBMD(=우리, 포트 47808)로 Distribute-Broadcast(0x09) 전송.
    # 또는 Original-Unicast(0x0A) / Forwarded-NPDU(0x04) 형태도 허용.
    old_timeout = sock.gettimeout()
    sock.settimeout(timeout)
    received = None
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            data, addr = sock.recvfrom(4096)
        except socket.timeout:
            break
        except Exception:
            break

        if len(data) < 4 or data[0] != BVLC_TYPE:
            continue

        # FD-01 루프 중 잔류 Register-FD 패킷 무시
        if data[1] == BVLC_REGISTER_FD:
            continue

        # NPDU 시작 위치 결정
        if data[1] == BVLC_FORWARDED_NPDU:
            npdu_off = 10
        else:
            npdu_off = 4

        inner = data[npdu_off:]
        if len(inner) < 2:
            continue

        # NPDU 건너뛰기 (version(1) + control(1) + 옵션)
        npdu_ctrl = inner[1] if len(inner) > 1 else 0
        npdu_len = 2
        if npdu_ctrl & 0x20 and len(inner) >= npdu_len + 3:
            npdu_len += 3 + inner[npdu_len + 2]
        if npdu_ctrl & 0x08 and len(inner) >= npdu_len + 3:
            npdu_len += 3 + inner[npdu_len + 2]
        if npdu_ctrl & 0x20 or npdu_ctrl & 0x08:
            npdu_len += 1  # hop count

        apdu_inner = inner[npdu_len:]
        # I-Am: PDU-type=0x10 (Unconfirmed), Service=0x00 (I-Am)
        if len(apdu_inner) >= 2 and apdu_inner[0] == 0x10 and apdu_inner[1] == 0x00:
            log.info('[FD-02] I-Am 수신: from=%s bvlc_func=0x%02X', addr, data[1])
            received = data
            break

    sock.settimeout(old_timeout)
    if own_sock:
        sock.close()
    dur = int((time.monotonic() - t0) * 1000)

    if received is None:
        return _make_result(
            'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
            pkt, None, 'FAIL',
            f'WhoIs 전송 후 {timeout:.0f}초 내 I-Am 미수신. DUT({dut_ip}) BACnet 서버 동작 확인 필요.',
            dur,
        )

    return _make_result(
        'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
        pkt, received,
        'PASS',
        f'I-Am 응답 수신 from {dut_ip}:{dut_port}. WhoIs-I-Am 교환 성공.',
        dur,
    )


# ── 전체 FD 검증 실행 ─────────────────────────────────────────────────────

def run_fd_tests(listen_ip: str = '0.0.0.0',
                 listen_port: int = FD_BBMD_PORT,   # 47808: BACnet 표준 포트 (BAC0 disconnect 후 사용)
                 dut_port: int = BACNET_PORT,        # 47808: DUT BACnet 포트
                 wait_timeout: float = DEFAULT_WAIT_TIMEOUT,
                 run_whois: bool = True,
                 progress_cb=None) -> Dict:
    """
    FD 전체 검증 실행.

    반환:
      {
        'local_ip':   ...,
        'timestamp':  ...,
        'tests':      [result_dict, ...],
        'summary':    {'total': N, 'pass': N, 'fail': N, 'skip': N},
        'fd_registered': True | False,
        'dut_ip':     '...' | None,
      }
    """
    from datetime import datetime

    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    results: List[Dict] = []
    fd_registered = False
    dut_ip = None

    log.info('[FD] 검증 시작: listen=%s:%d dut_port=%d timeout=%ds',
             listen_ip, listen_port, dut_port, wait_timeout)

    # ── 소켓 하나를 FD-01/FD-02가 공유 ─────────────────────────────────
    # FD-01에서 열고, FD-02도 같은 소켓(포트 47808)으로 I-Am 수신.
    # DUT(FD)는 I-Am을 BBMD(=우리, 포트 47808)로 Distribute-Broadcast 전송하므로
    # 두 테스트 사이에 소켓을 유지해야 한다.
    shared_sock, err = _open_shared_sock(listen_ip, listen_port)
    if shared_sock is None:
        fail_result = _make_result(
            'FD-01', 'FD-Register-Recv', 'ASHRAE 135 Annex J.5',
            b'', None, 'FAIL', err, 0,
        )
        results.append(fail_result)
        results.append(_make_result(
            'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
            b'', None, 'SKIP', 'FD-01 소켓 바인드 실패로 건너뜀.', 0,
        ))
        summary = {
            'total': 2, 'pass': 0, 'fail': 1, 'skip': 1,
        }
        return {
            'local_ip': listen_ip, 'timestamp': timestamp,
            'tests': results, 'summary': summary,
            'fd_registered': False, 'dut_ip': None,
        }

    try:
        # FD-01: Register-FD 수신 대기 (공유 소켓 전달)
        r01, dut_ip = test_fd_register_recv(
            listen_ip=listen_ip,
            listen_port=listen_port,
            wait_timeout=wait_timeout,
            progress_cb=progress_cb,
            sock=shared_sock,           # ← 공유 소켓
        )
        results.append(r01)
        fd_registered = (r01['status'] == 'PASS')

        if not fd_registered:
            results.append(_make_result(
                'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
                b'', None, 'SKIP',
                'FD-01 실패. DUT FD 등록 미확인으로 건너뜀.',
                0,
            ))
        else:
            if run_whois and dut_ip:
                # DUT가 BVLC-Result(Success) 수신 후 fRegistered=TRUE 처리를 완료할 때까지 대기.
                # 너무 빨리 WhoIs를 보내면 DUT가 아직 Status 74 상태일 수 있음.
                _REG_SETTLE_SEC = 2.0
                if progress_cb:
                    progress_cb(f'FD-01 완료 — DUT 등록 처리 대기 중 ({_REG_SETTLE_SEC:.0f}초)...')
                time.sleep(_REG_SETTLE_SEC)
                if progress_cb:
                    progress_cb(f'FD-02: DUT({dut_ip})에 WhoIs 전송 중...')
                r02 = test_fd_whois_iam(
                    dut_ip=dut_ip,
                    dut_port=dut_port,          # DUT BACnet 포트 (47808)
                    timeout=DEFAULT_WHOIS_TIMEOUT,
                    sock=shared_sock,           # ← 공유 소켓 (FD_BBMD_PORT=47809 유지)
                )
                results.append(r02)
            else:
                results.append(_make_result(
                    'FD-02', 'WhoIs-I-Am', 'ASHRAE 135 §20.1.2 / Annex J',
                    b'', None, 'SKIP',
                    'WhoIs 테스트 비활성화 또는 DUT IP 미확인.',
                    0,
                ))

    finally:
        shared_sock.close()

    # 요약 통계
    summary = {
        'total': len(results),
        'pass':  sum(1 for r in results if r['status'] == 'PASS'),
        'fail':  sum(1 for r in results if r['status'] == 'FAIL'),
        'skip':  sum(1 for r in results if r['status'] == 'SKIP'),
    }

    log.info('[FD] 검증 완료: %s', summary)
    return {
        'local_ip':      listen_ip,
        'timestamp':     timestamp,
        'tests':         results,
        'summary':       summary,
        'fd_registered': fd_registered,
        'dut_ip':        dut_ip,
    }


# ── 리포트 생성 ────────────────────────────────────────────────────────────

def generate_fd_report(result: Dict, output_dir: str = '.') -> Tuple[str, str]:
    """
    FD 검증 결과를 Excel 리포트로 저장.
    반환: (파일 전체 경로, 파일명)
    """
    try:
        import openpyxl
        from openpyxl.styles import Font, PatternFill, Alignment
    except ImportError:
        raise RuntimeError('openpyxl 미설치. pip install openpyxl')

    from datetime import datetime
    import os

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = 'FD 검증 결과'

    # ── 헤더 ─────────────────────────────────────────────────────────────
    header_fill = PatternFill('solid', fgColor='1B2A3B')
    header_font = Font(color='ECF0F1', bold=True, size=10)
    headers = ['테스트 ID', '테스트명', '표준 조항', '상태', '응답(ms)', '세부 결과',
               '전송 패킷(HEX)', '수신 패킷(HEX)']

    # 제목 행
    ws.merge_cells('A1:H1')
    title_cell = ws['A1']
    title_cell.value = f'FD(Foreign Device) 검증 결과 — {result.get("timestamp", "")} (DUT: {result.get("dut_ip", "N/A")})'
    title_cell.font = Font(bold=True, size=12, color='ECF0F1')
    title_cell.fill = PatternFill('solid', fgColor='1B2A3B')
    title_cell.alignment = Alignment(horizontal='center', vertical='center')
    ws.row_dimensions[1].height = 22

    for col, h in enumerate(headers, 1):
        cell = ws.cell(row=2, column=col, value=h)
        cell.font = header_font
        cell.fill = header_fill
        cell.alignment = Alignment(horizontal='center', vertical='center')

    # ── 데이터 행 ─────────────────────────────────────────────────────────
    fill_pass = PatternFill('solid', fgColor='D5F5E3')
    fill_fail = PatternFill('solid', fgColor='FADBD8')
    fill_skip = PatternFill('solid', fgColor='EAECEE')

    status_fills = {'PASS': fill_pass, 'FAIL': fill_fail, 'SKIP': fill_skip}

    for row_idx, t in enumerate(result.get('tests', []), start=3):
        fill = status_fills.get(t['status'], fill_skip)
        values = [
            t['test_id'], t['name'], t['standard_ref'], t['status'],
            t['duration_ms'], t['detail'], t['sent_hex'], t['received_hex'],
        ]
        for col, val in enumerate(values, 1):
            cell = ws.cell(row=row_idx, column=col, value=val)
            cell.fill = fill
            cell.alignment = Alignment(wrap_text=True, vertical='center')
            cell.font = Font(size=9)

    # 요약 행
    summary = result.get('summary', {})
    sum_row = len(result.get('tests', [])) + 4
    ws.cell(row=sum_row, column=1, value='요약').font = Font(bold=True, size=10)
    ws.cell(row=sum_row, column=2,
            value=f"전체:{summary.get('total',0)}  PASS:{summary.get('pass',0)}  FAIL:{summary.get('fail',0)}  SKIP:{summary.get('skip',0)}")

    # 열 너비
    col_widths = [12, 20, 28, 8, 10, 60, 50, 50]
    for i, w in enumerate(col_widths, 1):
        ws.column_dimensions[openpyxl.utils.get_column_letter(i)].width = w

    # 파일 저장
    ts = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'BTL_FD_Report_{ts}.xlsx'
    filepath = os.path.join(output_dir, filename)
    wb.save(filepath)
    log.info('[FD] 리포트 저장: %s', filepath)
    return filepath, filename
