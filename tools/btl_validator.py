#!/usr/bin/env python3
"""
BACnet BTL 검증 자동화 툴 v1.0
================================
에어컨(Aircon_List) 관제점 기반 BACnet BTL 인증 적합성 자동 검증

실행: python3 btl_validator.py
브라우저: http://<서버IP>:5000
"""

import os
import sys
import json
import time
import re
import asyncio
import threading
import logging
from datetime import datetime
from queue import Queue, Empty
from pathlib import Path
import concurrent.futures
from concurrent.futures import Future as ConcurrentFuture

from flask import Flask, render_template, jsonify, request, Response, send_file
from parsers import DEVICE_REGISTRY
from bbmd_check import run_bbmd_tests, generate_bbmd_report
from fd_check import run_fd_tests, generate_fd_report

# ── 선택적 의존성 ─────────────────────────────────────────────────────────
try:
    import xlrd
    XLRD_OK = True
except ImportError:
    xlrd = None
    XLRD_OK = False

try:
    import BAC0
    BACNET_OK = True
except ImportError:
    BAC0 = None
    BACNET_OK = False

try:
    import openpyxl
    from openpyxl.styles import Font, PatternFill, Alignment
    from openpyxl.utils import get_column_letter
    OPENPYXL_OK = True
except ImportError:
    openpyxl = None
    OPENPYXL_OK = False

# ── PyInstaller 호환 경로 헬퍼 ──────────────────────────────────────────────
def _get_resource_dir() -> Path:
    """읽기 전용 리소스 디렉토리 (templates, parsers 등).
    PyInstaller 번들: sys._MEIPASS / 일반 실행: 스크립트 디렉토리
    """
    if getattr(sys, 'frozen', False):
        return Path(sys._MEIPASS)
    return Path(__file__).parent


def _get_data_dir() -> Path:
    """쓰기 가능 데이터 디렉토리 (uploads, logs, .btl_last_file 등).
    PyInstaller 번들: exe 파일 옆 디렉토리 / 일반 실행: 스크립트 디렉토리
    """
    if getattr(sys, 'frozen', False):
        return Path(sys.executable).parent
    return Path(__file__).parent


# ── 상수 정의 ─────────────────────────────────────────────────────────────
# PyInstaller(frozen): exe 에 맞밖 폴더 / Linux 실행: tools/../ (기존 위치)
if getattr(sys, 'frozen', False):
    DEFAULT_EXCEL = str(_get_data_dir() / 'acs5_Instance_number_bacnet_20250324_acpi_260506.xls')
else:
    DEFAULT_EXCEL = str(Path(__file__).parent.parent /
        'acs5_Instance_number_bacnet_20250324_acpi_260506.xls')

UPLOAD_DIR = _get_data_dir() / 'uploads'
UPLOAD_DIR.mkdir(exist_ok=True)

OBJ_TYPE_MAP = {
    'AI': 'analogInput',
    'AO': 'analogOutput',
    'AV': 'analogValue',
    'BI': 'binaryInput',
    'BO': 'binaryOutput',
    'BV': 'binaryValue',
    'MI': 'multiStateInput',
    'MO': 'multiStateOutput',
}

# BTL 필수 프로퍼티 (ASHRAE 135 기준)
REQUIRED_PROPS = {
    'AI': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'units'],
    'AO': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'units',
           'priorityArray', 'relinquishDefault'],
    'AV': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'units'],
    'BI': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'polarity'],
    'BO': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'polarity',
           'priorityArray', 'relinquishDefault'],
    'BV': ['presentValue', 'statusFlags', 'eventState', 'outOfService'],
    'MI': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'numberOfStates'],
    'MO': ['presentValue', 'statusFlags', 'eventState', 'outOfService', 'numberOfStates',
           'priorityArray', 'relinquishDefault'],
}

VALID_TYPES = set(OBJ_TYPE_MAP.keys())

# ── 전역 상태 ─────────────────────────────────────────────────────────────
# 서버 IP 자동 감지
def _detect_server_ip():
    try:
        import socket
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except Exception:
        return '0.0.0.0'

SERVER_IP = _detect_server_ip()

# ── EXPIO 모듈 인스턴스 범위 ──────────────────────────────────────────────
# DEVICE_EXP_IO = 8 → inst = (8<<16)|(msid<<8)|(slot+1)
# msid=0 기준: ACS IO slot 0-13 → 524289-524302 / ACU slot 14-38 → 524303-524327
_EXPIO_ACS_IO_RANGE = range(524289, 524303)   # pexpmb000 (ACS IO)
_EXPIO_ACU_RANGE    = range(524303, 524328)   # pexpmb100/200/300 (ACU UI/UO/IO)
_EXPIO_ACS_IO_SLOTS = range(0, 14)            # pexpmb000 슬롯 (0-based, 0-13)
_EXPIO_ACU_SLOTS    = range(14, 39)           # pexpmb100/200/300 슬롯 (0-based, 14-38)
# ACS IO AI(UI) 슬롯→기대값: slot4=5.0, slot5=6.0, slot6=7.0, slot7=8.0
_EXPIO_UI_EXPECTED  = {4: 5.0, 5: 6.0, 6: 7.0, 7: 8.0}
_EXPIO_UI_TOLERANCE = 0.6   # ÷10 BCD 변환 오차 허용
# ACS IO BI(DI) 슬롯→기대값: slot11~13 = 1 (Active)
_EXPIO_DI_SLOTS     = range(11, 14)           # di_11, di_12, di_13
_EXPIO_DI_EXPECTED  = 1.0
# ACU AI(UI) 슬롯→기대값: slot33~38 = 20.0
_ACU_UI_SLOTS       = range(33, 39)           # ui_0~5 (ACU UI/IO)
_ACU_UI_EXPECTED    = 20.0
_ACU_UI_TOLERANCE   = 0.6
# ACU BI(DI) 슬롯: ACP5 BACnet에 구현되지 않음 (V_ACU: IO_ACU_UIO/UI/UO만 존재, DI 없음) → 전체 SKIP 예상
_ACU_DI_SLOTS       = range(20, 29)           # di_0~8 (ACU IO DI)
_ACU_DI_EXPECTED    = 1.0

state = {
    'connected': False,
    'bacnet': None,
    'device_ip': '',
    'device_id': 4194302,
    'own_ip': SERVER_IP,
    'points': [],
    'results': [],
    'running': False,
    'event_queue': Queue(),
    'sse_queues':   [],         # SSE 연결별 Queue 리스트 (다중 연결 안전)
    'report_file': None,
    'log_file': None,
    'b2_count': 0,          # 장치 대수 (0=1대, 1=2대 …)
    'uploaded_excel': None, # 업로드된 파일 경로
    'device_type': 'Aircon', # 선택된 장치 타입
    'bacnet_port': 47808,   # BACnet UDP 포트 (BAC0.lite에 전달)
    'expio_subtypes': set(), # 장치에서 감지된 expio-module facilitySubType 집합
    'expio_modules':  {},   # {'acs_io': [(slaveId, name)], 'acu': [(slaveId, name, subtype)]}
    'odu_subtypes':   {},    # ODU b2(facility_addr) → facilitySubType 매핑 {0: 'super4', 1: 'super3', ...}
    # ── BBMD 검증 전용 상태 ───────────────────────────────────────
    'bbmd_results':   None,  # 마지막 BBMD 검증 결과 (run_bbmd_tests 반환값)
    'bbmd_running':   False, # BBMD 검증 진행 중 여부
    'bbmd_report':    None,  # 마지막 BBMD 리포트 파일명
    # ── FD 검증 전용 상태 ─────────────────────────────────────────
    'fd_results':     None,  # 마지막 FD 검증 결과 (run_fd_tests 반환값)
    'fd_running':     False, # FD 검증 진행 중 여부
    'fd_report':      None,  # 마지막 FD 리포트 파일명
    'fd_progress':    '',    # FD 검증 진행 메시지
}

# ── 마지막 사용 엑셀 파일 경로 영속 저장 파일 ──────────────────────────────
_TOOLS_DIR = _get_data_dir()
_LAST_FILE_PATH = _TOOLS_DIR / '.btl_last_file'


def _abs_path(p: str) -> str:
    """상대경로를 tools/ 기준 절대경로로 변환 (절대경로는 그대로 반환)"""
    if p and not os.path.isabs(p):
        return str(_TOOLS_DIR / p)
    return p


def _rel_path(p: str) -> str:
    """절대경로를 tools/ 기준 상대경로로 변환 (실패 시 원본 반환)"""
    try:
        return str(Path(p).relative_to(_TOOLS_DIR))
    except ValueError:
        return p


def _find_default_excel() -> str:
    """기본 엑셀 파일 경로 자동 탐색.
    우선순위: .btl_last_file 저장 경로 → uploads/ 폴더 → DEFAULT_EXCEL
    """
    # 1. 마지막 사용 파일 저장소
    try:
        if _LAST_FILE_PATH.exists():
            _last = _abs_path(_LAST_FILE_PATH.read_text(encoding='utf-8').strip())
            if _last and Path(_last).exists():
                return _last
    except Exception:
        pass

    # 2. uploads/ 폴더의 첫 번째 .xls/.xlsx 파일
    try:
        for ext in ('*.xls', '*.xlsx'):
            found = sorted(UPLOAD_DIR.glob(ext))
            if found:
                return str(found[0])
    except Exception:
        pass

    # 3. DEFAULT_EXCEL (parent.parent 또는 exe 에 맞박 폴더)
    if DEFAULT_EXCEL and os.path.exists(DEFAULT_EXCEL):
        return DEFAULT_EXCEL

    return ''


_initial_excel = _find_default_excel()
if _initial_excel:
    state['uploaded_excel'] = _initial_excel


app = Flask(__name__, template_folder=str(_get_resource_dir() / 'templates'))
logging.basicConfig(level=logging.INFO, format='%(asctime)s %(levelname)s %(message)s')

# ── 노이즈 로그 원천 차단 (BAC0 REQUEST + Werkzeug access) ─────────────────
class _NoiseFilter(logging.Filter):
    """BAC0/bacpypes3 폴링 DEBUG 로그와 Werkzeug HTTP access 로그를 핸들러 레벨에서 차단.
    이름 기반 차단은 BAC0 내부 unnamed logger를 잡지 못하므로,
    DEBUG 레벨은 우리 자신(btl_validator)에서 발생한 것만 허용하는 방식으로 역전.
    """
    _OWN_MODULE = __name__   # 직접 실행 시 '__main__', 모듈로 import 시 'btl_validator'

    def filter(self, record: logging.LogRecord) -> bool:
        # DEBUG: 우리 모듈에서 발생한 것만 허용, 나머지(BAC0/bacpypes3/etc.) 전부 차단
        if record.levelno < logging.INFO:
            if record.name != self._OWN_MODULE:
                return False
        # BAC0/bacpypes 라이브러리 로그는 root handler에서 차단
        # (BAC0 자체 handler가 이미 출력하므로 중복 방지)
        if record.name.startswith(('BAC0', 'bacpypes')):
            return False
        # Werkzeug HTTP access 로그 억제 (/api/status 폴링 등)
        if record.name == 'werkzeug' and record.levelno == logging.INFO:
            return False
        return True

_noise_filter = _NoiseFilter()
for _h in logging.getLogger().handlers:
    _h.addFilter(_noise_filter)
for _lib in ('BAC0', 'BAC0.scripts', 'BAC0.scripts.Lite',
             'BAC0.core', 'BAC0.core.io', 'BAC0.core.io.Read',
             'BAC0.core.io.Write', 'bacpypes3', 'werkzeug'):
    _lg_lib = logging.getLogger(_lib)
    _lg_lib.addFilter(_noise_filter)   # logger 레벨에서도 추가 차단 (이중 방어)
    if _lib != 'werkzeug':
        _lg_lib.propagate = False   # root logger로 전파 차단 (_NoiseFilter의 2차 방어선)

# ── 모든 logger/handler에 필터 강제 적용 (BAC0 동적 handler 포함) ───────────
def _force_add_filter_to_all_handlers(filter_obj):
    for logger_name in logging.root.manager.loggerDict:
        _lg = logging.getLogger(logger_name)
        for _h in getattr(_lg, 'handlers', []):
            _h.addFilter(filter_obj)
        # BAC0/bacpypes 계열은 propagate 차단 (root handler 중복 출력 방지)
        if logger_name.startswith(('BAC0', 'bacpypes')):
            _lg.propagate = False
    for _h in logging.getLogger().handlers:
        _h.addFilter(filter_obj)

_force_add_filter_to_all_handlers(_noise_filter)

# ── 로그 파일 저장 설정 ────────────────────────────────────────────────────
LOG_DIR = _get_data_dir() / 'logs'
LOG_DIR.mkdir(exist_ok=True)

_current_log_file: str = ''


def _start_log_file(label: str = 'btl') -> str:
    """검증 시작 시 새 로그 파일 생성; 파일명 반환"""
    global _current_log_file
    ts = datetime.now().strftime('%Y%m%d_%H%M%S')
    filename = f'{label}_{ts}.log'
    filepath = str(LOG_DIR / filename)
    _current_log_file = filename

    # 기존 FileHandler 제거 후 새 파일 핸들러 추가
    root = logging.getLogger()
    for h in list(root.handlers):
        if isinstance(h, logging.FileHandler):
            h.close()
            root.removeHandler(h)

    fh = logging.FileHandler(filepath, encoding='utf-8')
    fh.setLevel(logging.DEBUG)
    fh.setFormatter(logging.Formatter(
        '%(asctime)s %(levelname)-8s %(message)s'))
    fh.addFilter(_noise_filter)  # 새 파일 핸들러에도 동일 필터 적용
    root.addHandler(fh)
    root.setLevel(logging.DEBUG)  # 파일에는 DEBUG 이상 모두 기록

    # BAC0 노이즈 로거 추가 억제: level=WARNING으로 올려 DEBUG 생성 자체를 줄이고,
    # propagate=False로 root(파일) handler로의 전파도 차단 (_NoiseFilter의 2차 방어선)
    for _noisy in ('BAC0', 'BAC0.scripts', 'BAC0.scripts.Lite',
                   'BAC0.core', 'BAC0.core.io', 'BAC0.core.io.Read',
                   'BAC0.core.io.Write', 'bacpypes3'):
        _lg = logging.getLogger(_noisy)
        _lg.setLevel(logging.WARNING)
        _lg.propagate = False
    # 로그 파일 핸들러 추가 후에도 모든 handler에 필터 재적용
    _force_add_filter_to_all_handlers(_noise_filter)
    return filename

# ── asyncio 루프 (BAC0 2025.x async API 브릿지) ──────────────────────────
# Flask는 동기(sync) 스레드, BAC0 2025.x는 완전 async → 별도 이벤트 루프 스레드로 브릿지
_aio_loop: asyncio.AbstractEventLoop = None
_aio_loop_ready = threading.Event()


def _start_aio_loop():
    global _aio_loop
    _aio_loop = asyncio.new_event_loop()
    asyncio.set_event_loop(_aio_loop)
    _aio_loop_ready.set()
    _aio_loop.run_forever()


_aio_thread = threading.Thread(target=_start_aio_loop, daemon=True, name='bacnet-aio')
_aio_thread.start()
_aio_loop_ready.wait(timeout=5)  # 루프 준비 대기


def aio_run(coro, timeout: float = 15.0):
    """Flask 동기 스레드에서 asyncio 코루틴 동기 실행 (BAC0 async API 브릿지)"""
    future = asyncio.run_coroutine_threadsafe(coro, _aio_loop)
    return future.result(timeout=timeout)
log = logging.getLogger(__name__)


# ═══════════════════════════════════════════════════════
#  BACnet 통신 헬퍼
# ═══════════════════════════════════════════════════════
def _bv_to_int(val) -> int:
    """BACnet BinaryPV 값을 int로 변환 (active=1, inactive=0)"""
    if val is None:
        return -1
    s = str(val).lower()
    if 'active' in s and 'inactive' not in s:
        return 1
    if 'inactive' in s:
        return 0
    try:
        return int(val)
    except Exception:
        return -1


# ═══════════════════════════════════════════════════════
#  엑셀 파싱
# ═══════════════════════════════════════════════════════
# (parsers/ 로 이전됨 — parsers/__init__.py DEVICE_REGISTRY 참조)


# ═══════════════════════════════════════════════════════
#  ODU 고정 에러 값 (point_no 1~43에 대한 기대값, 모든 ODU 대수에 동일 적용)
# ═══════════════════════════════════════════════════════
# key = point_no (1~43), value = expected 현재값
ODU_EXPECTED: dict = {
     1:    1.0,
     2:    3.0,
     3:   60.0,
     4: 2761.0,
     5:  843.0,
     6:   15.3,
     7:   20.1,
     8:   27.1,
     9: 1920.0,
    10:   80.0,
    11:    0.0,
    12:   70.0,
    13:   31.2,
    14:    3.0,
    15:   25.0,
    16:   75.0,
    17:   20.0,
    18:   50.0,
    19:   33.0,
    20:    0.0,
    21:    0.0,
    22:   71.8,
    23:   70.7,
    24:    9.3,
    25:   17.7,
    26:  280.0,
    27:    0.0,
    28:  304.0,
    29:  312.0,
    30:    1.0,
    31:    1.0,
    32:    1.0,
    33:    1.0,
    34:    1.0,
    35:    1.0,
    36:    0.0,
    37:    1.0,
    38:    0.0,
    39:    0.0,
    40:    0.0,
    41:    0.0,
    42:    0.0,
    43:    0.0,
}
# 비교 허용 오차 (정수는 엄격하게, 실수는 소수점 오차 감안)
ODU_TOLERANCE = 0.5

# ═══════════════════════════════════════════════════════
#  AIR_SCROLL2 고정 기대값 (pno 2~38, 슬롯 2~38에 대한 기대값)
#  pno 1(BV), 39~42(MO/MI/AV/AV) 는 기존 제어 테스트로 검증
# ═══════════════════════════════════════════════════════
SCROLL2_EXPECTED: dict = {
     2:     1.0,   # BI ChilledWaterFlowSwitch
     3:     1.0,   # BI ChilledWaterPumpOutput
     4:     1.0,   # BI ChilledWaterPumpInterlock
     5: 30001.0,   # AI MainPcbVersion
     6:  3000.2,   # AI TotalCurrentConsumption
     7:  3000.3,   # AI OutsideAirTemp
     8:  3000.4,   # AI ChilledWaterLeavingTemp
     9:  3000.5,   # AI ChilledWaterEnteringTemp
    10: 30009.0,   # AI ErrorCycle
    11: 30010.0,   # AI ErrorCode
    12: 30011.0,   # AI LeftToStartTime  (slope=1.0, raw register 30011 그대로)
    13: 30013.0,   # AI TotalOperTimeHigh
    14: 30014.0,   # AI TotalOperTimeLow
    15:  3010.1,   # AI Cycle1_1WaterLeavingTemp
    16:  3010.2,   # AI Cycle1_2WaterLeavingTemp
    17: 30113.0,   # AI Cycle1_1InvCompFreq
    18: 30114.0,   # AI Cycle1_2InvCompFreq
    19: 30115.0,   # AI Cycle1_1HighPressure
    20: 30116.0,   # AI Cycle1_2HighPressure
    21: 30117.0,   # AI Cycle1_1LowPressure
    22: 30118.0,   # AI Cycle1_2LowPressure
    23:  3020.1,   # AI Cycle2_1WaterLeavingTemp
    24:  3020.2,   # AI Cycle2_2WaterLeavingTemp
    25: 30213.0,   # AI Cycle2_1InvCompFreq
    26: 30214.0,   # AI Cycle2_2InvCompFreq
    27: 30215.0,   # AI Cycle2_1HighPressure
    28: 30216.0,   # AI Cycle2_2HighPressure
    29: 30217.0,   # AI Cycle2_1LowPressure
    30: 30218.0,   # AI Cycle2_2LowPressure
    31:  3030.1,   # AI Cycle3_1WaterLeavingTemp
    32:  3030.2,   # AI Cycle3_2WaterLeavingTemp
    33: 30313.0,   # AI Cycle3_1InvCompFreq
    34: 30314.0,   # AI Cycle3_2InvCompFreq
    35: 30315.0,   # AI Cycle3_1HighPressure
    36: 30316.0,   # AI Cycle3_2HighPressure
    37: 30317.0,   # AI Cycle3_1LowPressure
    38: 30318.0,   # AI Cycle3_2LowPressure
}
SCROLL2_TOLERANCE = 0.5



def _bac_val_to_float(val, obj_short: str):
    """BACnet present-value → float 변환 (ODU 폭잣 비교용).
    BI/BO/BV : Active=1.0, Inactive=0.0
    AI/AO/AV : float() 시도
    """
    if val is None:
        return None
    s = str(val)
    if obj_short in ('BI', 'BO', 'BV'):
        sl = s.lower()
        if 'inactive' in sl:
            return 0.0
        if 'active' in sl:
            return 1.0
    try:
        return float(s)
    except (ValueError, TypeError):
        return None


# ═══════════════════════════════════════════════════════
#  BACnet 통신 래퍼
# ═══════════════════════════════════════════════════════
def bacnet_read(addr, obj_long, instance, prop, arr_idx=None):
    """BACnet ReadProperty — WhoIs 우회, read_property 직접 호출; 반환: (값, 상태문자열)"""
    if not BACNET_OK or state['bacnet'] is None:
        return None, 'NO_DEVICE'

    async def _read():
        # BAC0 build_rp_request로 Address/ObjectIdentifier/PropertyIdentifier 파싱
        # → WhoIs 재시도 루프 없이 _app.read_property() 직접 호출
        args_split = [addr, obj_long, str(instance), prop]
        if arr_idx is not None:
            args_split.append(str(arr_idx))
        device_address, object_identifier, property_identifier, arr_index = \
            state['bacnet'].build_rp_request(args_split)
        _app = state['bacnet'].this_application.app
        return await _app.read_property(
            device_address, object_identifier, property_identifier, arr_index)

    try:
        val = aio_run(_read(), timeout=12)
        return val, 'OK'
    except TimeoutError:
        return None, 'TIMEOUT'
    except BaseException as e:
        # bacpypes3.primitivedata.Error 등 Exception 비상속 예외도 캐치
        if isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
        err_str = str(e)
        if 'NoResponse' in type(e).__name__ or 'NoResponse' in err_str:
            return None, 'NO_RESPONSE'
        if 'unknown-object' in err_str or 'unknown_object' in err_str:
            return None, 'UNKNOWN_OBJECT'
        return None, err_str


def bacnet_write(addr, obj_long, instance, value):
    """BACnet WriteProperty — presentValue 직접 쓰기; 반환: (성공여부, 상태문자열)"""
    if not BACNET_OK or state['bacnet'] is None:
        return False, 'NO_DEVICE'

    async def _write():
        args = f'{addr} {obj_long} {instance} presentValue {value}'
        (device_address, object_identifier, property_identifier,
         val, arr_index, prio) = state['bacnet'].build_wp_request(args)
        _app = state['bacnet'].this_application.app
        return await _app.write_property(
            device_address, object_identifier, property_identifier,
            val, arr_index, prio)

    try:
        aio_run(_write(), timeout=12)
        return True, 'OK'
    except TimeoutError:
        return False, 'TIMEOUT'
    except BaseException as e:
        if isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
        err_str = str(e)[:120]
        if 'NoResponse' in type(e).__name__ or 'NoResponse' in err_str:
            return False, 'NO_RESPONSE'
        if 'unknown-object' in err_str or 'unknown_object' in err_str:
            return False, 'UNKNOWN_OBJECT'
        return False, err_str


def _relinquish_async(addr, obj_long, inst):
    """Priority 16 null 해제 — 동기 블로킹 (race condition 방지).
    fire-and-forget 방식은 다음 테스트 도중에 relinquish가 실행되어
    장치의 CNI 처리를 방해할 수 있으므로 반드시 완료 후 반환한다.
    """
    def _do_relinquish():
        try:
            args = f'{addr} {obj_long} {inst} presentValue null - 16'
            (da, oi, pi, v, ai, pr) = state['bacnet'].build_wp_request(args)
            _app = state['bacnet'].this_application.app

            async def _rel():
                await asyncio.wait_for(
                    _app.write_property(da, oi, pi, v, ai, pr), timeout=3.0)
            aio_run(_rel(), timeout=4)
        except Exception:
            pass
    _do_relinquish()


# ═══════════════════════════════════════════════════════
#  타입별 Write 검증 헬퍼
# ═══════════════════════════════════════════════════════

# Write 후 readback 폴링 파라미터
_POLL_INITIAL   = 1.0   # write 후 최소 대기 (초) — CNI 반영 보장
_POLL_INTERVAL  = 0.2   # 재시도 간격 (초)
_POLL_TIMEOUT   = 10.0  # 기본 최대 대기 (초) — ACS 반영 지연 실측 ~5.2s 대응 (구: 4.0s)
_POLL_TIMEOUT_V = 10.0  # Vent 장치용 최대 대기 (초) — 동일 지연 패턴 대응
_POLL_TIMEOUT_CHILLER_MO = 10.0  # Chiller MO 최대 대기 (초) — 냉난방 모드 전환 지연 대응
_POLL_TIMEOUT_AHU = 10.0  # AHU 장치용 최대 대기 (초) — 댐퍼/모드 반영 지연 대응 (실측 ~5.2s)

# ── All 버튼 병렬 검증 전용 타임아웃 ──────────────────────────────────────
# 여러 워커가 동시에 BACnet I/O → 단일 asyncio 큐 경쟁으로 per-read 지연 발생.
# 워커 수=4 기준 큐 대기: 최대 3×0.8s ≈ 2.4s → 단독 폴 창(3.0s)을 거의 소진.
# _POLL_INITIAL 2.0s + 폴 창 10.0s: 큐 대기(2.4s)를 감안해도 폴 시도 4~5회 확보.
_POLL_INITIAL_PARALLEL = 2.0    # 병렬 모드 write→readback 최소 대기 (단독: 1.0)
_POLL_TIMEOUT_PARALLEL = 12.0   # 병렬 모드 폴링 타임아웃 (단독: 4.0)


def _poll_initial() -> float:
    """단독/병렬 모드에 따른 write 후 최소 대기 시간 반환."""
    return _POLL_INITIAL_PARALLEL if state.get('all_running') else _POLL_INITIAL


def _poll_timeout(point: dict) -> float:
    """장치 타입에 따른 write 후 readback 폴링 타임아웃 반환."""
    dt = point.get('device_type', '')
    if dt == 'Vent':
        return _POLL_TIMEOUT_V
    if dt == 'AHU':
        return _POLL_TIMEOUT_AHU
    return _POLL_TIMEOUT


def _poll_bool_readback(addr: str, obj_long: str, inst: int,
                        expected_int: int, timeout: float) -> tuple:
    """BinaryOutput/BinaryValue readback 폴링.
    expected_int: 기대값 (0 또는 1)
    반환: (passed:bool, readback_err_str)
    """
    _init = _poll_initial()
    time.sleep(_init)  # write 후 최소 대기 — CNI 반영 보장 (병렬 모드에서는 2.0s)
    deadline = time.time() + max(timeout - _init, 0)
    rb_val, rb_err = None, 'timeout'
    _last_logged = object()  # sentinel: 치음 한 번은 반드시 출력
    while True:
        rb_val, rb_err = bacnet_read(addr, obj_long, inst, 'presentValue')
        got = _bv_to_int(rb_val)
        # 값이 바뀌었을 때만 로그 출력 (일정한 간격 폴링시 반복 출력 방지)
        if got != _last_logged:
            log.debug('[poll_bool] %s:%s got=%s expected=%s', obj_long, inst, got, expected_int)
            _last_logged = got
        if rb_err == 'OK' and got == expected_int:
            return True, 'OK'
        if time.time() >= deadline:
            break
        time.sleep(_POLL_INTERVAL)
    return False, f'timeout(got={_bv_to_int(rb_val)},expected={expected_int})'


def _poll_int_readback(addr: str, obj_long: str, inst: int,
                       expected_int: int, timeout: float) -> tuple:
    """MultiState readback 폴링.
    반환: (passed:bool, readback_err_str)
    """
    _init = _poll_initial()
    time.sleep(_init)  # write 후 최소 대기 (병렬 모드에서는 2.0s)
    deadline = time.time() + max(timeout - _init, 0)
    rb_val, rb_err = None, 'timeout'
    last_int = None
    while True:
        rb_val, rb_err = bacnet_read(addr, obj_long, inst, 'presentValue')
        if rb_err == 'OK' and rb_val is not None:
            try:
                cur = int(rb_val)
                if cur != last_int:  # 값 변화 시만 로그
                    log.debug('[poll_int] %s:%s got=%s expected=%s', obj_long, inst, cur, expected_int)
                    last_int = cur
                if last_int == expected_int:
                    return True, 'OK'
            except Exception:
                pass
        if time.time() >= deadline:
            break
        time.sleep(_POLL_INTERVAL)
    return False, f'timeout(got={last_int},expected={expected_int})'


def _poll_float_readback(addr: str, obj_long: str, inst: int,
                         expected: float, tolerance: float, timeout: float) -> tuple:
    """AnalogValue/AnalogOutput readback 폴링 (허용 오차 포함).
    반환: (passed:bool, readback_err_str)
    """
    _init = _poll_initial()
    time.sleep(_init)  # write 후 최소 대기 (병렬 모드에서는 2.0s)
    deadline = time.time() + max(timeout - _init, 0)
    rb_val, rb_err = None, 'timeout'
    last_val = None
    while True:
        rb_val, rb_err = bacnet_read(addr, obj_long, inst, 'presentValue')
        if rb_err == 'OK' and rb_val is not None:
            try:
                cur = float(rb_val)
                if last_val is None or abs(cur - last_val) > 1e-6:  # 값 변화 시만 로그
                    log.debug('[poll_float] %s:%s got=%.2f expected=%.2f', obj_long, inst, cur, expected)
                    last_val = cur
                if abs(cur - expected) <= tolerance:
                    return True, 'OK'
            except Exception:
                pass
        if time.time() >= deadline:
            break
        time.sleep(_POLL_INTERVAL)
    return False, f'timeout(got={last_val},expected={expected})'


def _write_test_expio_av(point: dict, addr: str, obj_long: str):
    """EXPIO AV: min / max / mid 3회 write→readback, 모두 성공이면 PASS.
    point에 minValue/maxValue 없으면 0.0/100.0 기본값 사용.
    """
    inst    = point['instance']
    av_min  = float(point.get('minValue', 0.0))
    av_max  = float(point.get('maxValue', 100.0))
    av_mid  = round((av_min + av_max) / 2.0, 4)
    poll_to = _poll_timeout(point)
    tol     = max(abs(av_max - av_min) * 0.01, 0.5)  # 범위의 1% or 0.5 허용

    sub = []
    for label, wval in [('min', av_min), ('max', av_max), ('mid', av_mid)]:
        ok, w_err = bacnet_write(addr, obj_long, inst, wval)
        if not ok:
            sub.append((label, False, f'write:{w_err}'))
            continue
        passed, rb_err = _poll_float_readback(addr, obj_long, inst, wval, tol, poll_to)
        sub.append((label, passed, f'write{wval}→{"OK" if passed else rb_err}'))

    details = ', '.join(f'{l}:{d}' for l, _, d in sub)
    if all(ok for _, ok, _ in sub):
        return 'PASS', details
    return 'FAIL', details


def _write_test_bo(point: dict, addr: str, obj_long: str):
    """BO: 1 write → 짝 BI가 1인지 확인, 0 write → 짝 BI가 0인지 확인.
    pair BI가 없을 때만 BO 자체 readback으로 fallback.
    반환: (write_result, write_detail)
    """
    inst      = point['instance']
    pair_inst = point.get('pair_instance')
    pair_long = OBJ_TYPE_MAP.get('BI', 'binaryInput')

    # 현재값 읽기
    cur_val, cur_err = bacnet_read(addr, obj_long, inst, 'presentValue')
    if cur_err != 'OK':
        return 'FAIL', f'현재값읽기실패:{cur_err}'
    cur_int = _bv_to_int(cur_val)

    cni_lower = point.get('cni', '').strip().lower()
    # CNI=humidify, DXHRV 장치: 전열교환기 가습은 On만 제어 가능 → write1만 검증
    # Vent(일반, is_dxhrv=False)는 기존 로직대로 write0/write1 모두 검증
    if cni_lower == 'humidify' and point.get('is_dxhrv', False):
        test_vals = [1]
    # 테스트 순서: 현재와 다른값 먼저, 그 다음 원래값
    else:
        test_vals = [1 - cur_int, cur_int] if cur_int in (0, 1) else [1, 0]

    sub_results = []
    poll_timeout = _poll_timeout(point)
    for v in test_vals:
        # BinaryOutput은 'active'/'inactive' 문자열이 필요 (정수 전달 시 AbortPDU 유발)
        bac_val = 'active' if v == 1 else 'inactive'
        ok, w_err = bacnet_write(addr, obj_long, inst, bac_val)
        if not ok:
            sub_results.append((v, False, w_err))
            continue

        if pair_inst:
            # 짝 BI의 presentValue로 실제 물리 상태 확인 (폴링)
            # pair_inst가 지정된 경우 BI 피드백이 권위 있는 판단 기준임.
            # BI가 반영되지 않으면 장치 미연결 또는 제어 실패로 FAIL 처리.
            passed, poll_err = _poll_bool_readback(addr, pair_long, pair_inst, v, poll_timeout)
            if not passed:
                w_err = f'pair_BI:{poll_err}'
        else:
            # pair 없을 때 BO 자체 readback으로 fallback (폴링)
            passed, poll_err = _poll_bool_readback(addr, obj_long, inst, v, poll_timeout)
            if not passed:
                w_err = f'self_readback:{poll_err}'

        sub_results.append((v, passed, w_err if not passed else ''))

    # CNI=oper(기동명령)는 검증 후 ON(active) 유지 → 이후 관제점 검증 가능하도록
    if point.get('cni', '').strip().lower() == 'oper':
        bacnet_write(addr, obj_long, inst, 'active')
        log.info('[BO] CNI=oper inst=%s 검증 완료 → ON(active) 유지, %gs 안정화 대기',
                 inst, _POLL_INITIAL)
        time.sleep(_POLL_INITIAL)   # active write 후 장치 CNI 처리 완료 보장
    else:
        _relinquish_async(addr, obj_long, inst)
    overall = 'PASS' if all(s[1] for s in sub_results) else 'FAIL'
    detail  = ', '.join(f"write{v}→{'OK' if p else 'NG'}" for v, p, _ in sub_results)
    return overall, detail


def _write_test_mo(point: dict, addr: str, obj_long: str):
    """MO: 1~N 상태 순차 write → 매번 짝 MI가 실제로 해당 상태로 바뀌는지 확인.
    pair MI가 없을 때만 MO 자체 readback으로 fallback.
    반환: (write_result, write_detail)
    """
    inst      = point['instance']
    pair_inst = point.get('pair_instance')
    pair_long = OBJ_TYPE_MAP.get('MI', 'multiStateInput')
    n_states  = point.get('number_of_states', 0)

    # number_of_states 미확인 시 장치에서 직접 읽기
    if n_states == 0:
        ns_val, ns_err = bacnet_read(addr, obj_long, inst, 'numberOfStates')
        if ns_err == 'OK' and ns_val is not None:
            try:
                n_states = int(ns_val)
            except Exception:
                n_states = 0
    if n_states == 0:
        return 'SKIP', '상태수 미확인'

    # 현재값 읽기 → 현재값과 다른값 먼저 테스트
    cur_val, _ = bacnet_read(addr, obj_long, inst, 'presentValue')
    try:
        cur_int = int(cur_val) if cur_val is not None else 1
    except Exception:
        cur_int = 1

    # state_texts 가져오기: point에 없으면 장치에서 직접 읽기
    state_texts = point.get('state_texts', [])
    if not state_texts:
        # arr_idx 없이 읽으면 BACnet이 전체 배열을 반환
        _tv, _terr = bacnet_read(addr, obj_long, inst, 'stateText')
        if _terr == 'OK' and _tv is not None:
            # BAC0는 Array 타입을 리스트로 반환; 아니면 단일 문자열
            if isinstance(_tv, (list, tuple)):
                state_texts = [str(x) for x in _tv]
            elif hasattr(_tv, '__iter__') and not isinstance(_tv, str):
                state_texts = [str(x) for x in _tv]
        # 개별 읽기 fallback (전체 읽기 실패 시)
        if not state_texts:
            _read_texts = []
            for _i in range(1, n_states + 1):
                _tv2, _terr2 = bacnet_read(addr, obj_long, inst, 'stateText', arr_idx=_i)
                _read_texts.append(str(_tv2) if (_tv2 is not None and _terr2 == 'OK') else '')
            if any(t for t in _read_texts if t):
                state_texts = _read_texts
    all_states = list(range(1, n_states + 1))
    log.info('[MO] inst=%s n_states=%s stateText=%s', inst, n_states, state_texts)
    # "not use" state 제외 (예: "not use(2)", "not use" 등 모두 처리)
    def _is_not_use(text: str) -> bool:
        return text.strip().lower().startswith('not use')
    valid_states = [s for s in all_states
                    if not (0 <= s - 1 < len(state_texts) and
                            _is_not_use(state_texts[s - 1]))]
    skipped_states = [s for s in all_states if s not in valid_states]
    if not valid_states:
        return 'SKIP', 'all states are not use'
    # 현재 상태 제외 → 맨 뒤로 배치
    test_order = [s for s in valid_states if s != cur_int] + \
                 ([cur_int] if cur_int in valid_states else [])

    sub_results = []
    is_chiller = (point.get('device_type', '') == 'Chiller')
    poll_timeout = _POLL_TIMEOUT_CHILLER_MO if is_chiller else _poll_timeout(point)
    pair_state_map = point.get('pair_state_map', {})
    for s in test_order:
        ok, w_err = bacnet_write(addr, obj_long, inst, s)
        if not ok:
            sub_results.append((s, False, w_err))
            continue

        # pair_state_map이 있으면 MI 기대 state를 변환 (예: MO state1 → MI state2)
        expected_mi = pair_state_map.get(s, s)

        if pair_inst:
            # 짝 MI의 presentValue로 실제 물리 상태 확인 (폴링)
            passed, poll_err = _poll_int_readback(addr, pair_long, pair_inst, expected_mi, poll_timeout)
            if not passed:
                w_err = f'pair_MI:{poll_err}'
        else:
            # pair 없을 때 MO 자체 readback으로 fallback (폴링)
            passed, poll_err = _poll_int_readback(addr, obj_long, inst, s, poll_timeout)
            if not passed:
                w_err = f'self_readback:{poll_err}'

        sub_results.append((s, passed, w_err if not passed else ''))

    # 검증 후 처리: __post_write_state가 있으면 해당 state로 복귀 (neutral 상태 보장)
    # 예: User Mode MO → Both=0인 None(state3)으로 복귀
    post_state = point.get('judge_condition', {}).get('__post_write_state')
    if post_state:
        bacnet_write(addr, obj_long, inst, post_state)
        log.info('[MO] 검증 완료 → 복귀 state%d write (inst=%s)', post_state, inst)
    else:
        _relinquish_async(addr, obj_long, inst)
    overall = 'PASS' if all(s[1] for s in sub_results) else 'FAIL'
    detail  = ', '.join(f"state{s}→{'OK' if p else 'NG'}" for s, p, _ in sub_results)
    if skipped_states:
        detail += ', skip(' + ','.join(f'state{s}' for s in skipped_states) + ')'
    return overall, detail


def _write_test_av(point: dict, addr: str, obj_long: str):
    """AV: MIN, (MIN+MAX)/2, MAX 값 3회 write → 매번 짝 AI가 실제로 바뀌는지 확인.
    pair AI가 없을 때만 AV 자체 readback으로 fallback.
    llim_instance/ulim_instance가 설정된 경우 BACnet에서 실제 범위를 동적으로 읽어 사용.
    반환: (write_result, write_detail)
    """
    av_min = point.get('av_min')
    av_max = point.get('av_max')
    inst   = point['instance']

    # ── BACnet 동적 범위 읽기 (settempLlim/Ulim 등) ─────────────────────
    # 파서가 연결한 llim_instance/ulim_instance AI/AV의 presentValue를 읽어
    # 엑셀 기본값 av_min/av_max를 ACS 실제 제한값으로 대체한다.
    llim_inst = point.get('llim_instance')
    if llim_inst is not None:
        llim_long = OBJ_TYPE_MAP.get(point.get('llim_type', 'AI'), 'analogInput')
        dyn_min, min_err = bacnet_read(addr, llim_long, llim_inst, 'presentValue')
        if min_err == 'OK' and dyn_min is not None:
            try:
                av_min = float(dyn_min)
                log.info('[AV] inst=%d llim %s:%d → av_min=%.1f (BACnet 동적 범위)',
                         inst, llim_long, llim_inst, av_min)
            except Exception:
                pass
        else:
            log.warning('[AV] inst=%d llim %s:%d 읽기 실패(%s), 엑셀 기본값 사용',
                        inst, llim_long, llim_inst, min_err)

    ulim_inst = point.get('ulim_instance')
    if ulim_inst is not None:
        ulim_long = OBJ_TYPE_MAP.get(point.get('ulim_type', 'AI'), 'analogInput')
        dyn_max, max_err = bacnet_read(addr, ulim_long, ulim_inst, 'presentValue')
        if max_err == 'OK' and dyn_max is not None:
            try:
                av_max = float(dyn_max)
                log.info('[AV] inst=%d ulim %s:%d → av_max=%.1f (BACnet 동적 범위)',
                         inst, ulim_long, ulim_inst, av_max)
            except Exception:
                pass
        else:
            log.warning('[AV] inst=%d ulim %s:%d 읽기 실패(%s), 엑셀 기본값 사용',
                        inst, ulim_long, ulim_inst, max_err)

    # settempLlim AV: 상한 제약 (Llim < Ulim) → av_max = ulim_read - 1.0
    # ulim_instance에서 읽은 settempUlim 현재값보다 반드시 낮아야 함
    cni_name = point.get('cni', '')
    if ulim_inst is not None and cni_name.lower() == 'settempllim' and av_max is not None:
        old_max = av_max
        av_max  = max(round(av_max - 1.0, 1), av_min if av_min is not None else 0.0)
        log.info('[AV] inst=%d settempLlim 상한 보정: ulim=%.1f → av_max=%.1f', inst, old_max, av_max)

    if av_min is None or av_max is None:
        return 'SKIP', 'MIN/MAX 미정의'

    pair_inst = point.get('pair_instance')
    pair_long = OBJ_TYPE_MAP.get('AI', 'analogInput')
    av_mid    = round((av_min + av_max) / 2, 1)

    # 현재값 읽기 → 가장 다른값 먼저 테스트
    cur_val, _ = bacnet_read(addr, obj_long, inst, 'presentValue')
    try:
        cur_float = float(cur_val) if cur_val is not None else av_min
    except Exception:
        cur_float = av_min
    test_vals = sorted([av_min, av_mid, av_max],
                       key=lambda v: -abs(v - cur_float))

    TOLERANCE = 0.6   # AI/AV 리드백 허용 오차
    sub_results = []
    poll_timeout = _poll_timeout(point)
    for v in test_vals:
        ok, w_err = bacnet_write(addr, obj_long, inst, v)
        if not ok:
            sub_results.append((v, False, w_err))
            continue

        if pair_inst:
            # 짝 AI의 presentValue로 실제 물리 상태 확인 (폴링)
            # pair_inst가 지정된 경우 AI 피드백이 권위 있는 판단 기준임.
            # AI가 반영되지 않으면 장치 미연결 또는 제어 실패로 FAIL 처리.
            passed, poll_err = _poll_float_readback(addr, pair_long, pair_inst, v, TOLERANCE, poll_timeout)
            if not passed:
                w_err = f'pair_AI:{poll_err}'
        else:
            # pair 없을 때 AV 자체 readback (폴링)
            passed, poll_err = _poll_float_readback(addr, obj_long, inst, v, TOLERANCE, poll_timeout)
            if not passed:
                w_err = f'self_readback:{poll_err}'

        sub_results.append((v, passed, w_err if not passed else ''))

    overall = 'PASS' if all(s[1] for s in sub_results) else 'FAIL'
    detail  = ', '.join(f"{v}→{'OK' if p else 'NG'}" for v, p, _ in sub_results)
    return overall, detail


def _write_test_bv(point: dict, addr: str, obj_long: str):
    """BV: 현재값과 다른 값으로 write→read, 이후 원래 값으로 복구 write→read.
    현재값 0 → write1→read, write0→read
    현재값 1 → write0→read, write1→read
    두 단계 모두 성공이면 PASS.
    반환: (write_result, write_detail)
    """
    inst    = point['instance']
    poll_to = _poll_timeout(point)

    # 현재값 읽기
    cur_val, cur_err = bacnet_read(addr, obj_long, inst, 'presentValue')
    cur_int = _bv_to_int(cur_val) if cur_err == 'OK' and cur_val is not None else 0

    # 1단계: 현재값의 반대로 write → read
    val_a = 1 - cur_int
    ok_a, w_err_a = bacnet_write(addr, obj_long, inst, val_a)
    if not ok_a:
        return 'FAIL', f'write{val_a}:{w_err_a}'
    pass_a, err_a = _poll_bool_readback(addr, obj_long, inst, val_a, poll_to)

    # 2단계: 원래 값으로 복구 write → read
    val_b = cur_int
    ok_b, w_err_b = bacnet_write(addr, obj_long, inst, val_b)
    if not ok_b:
        detail = f'write{val_a}→{"OK" if pass_a else err_a}, write{val_b}:{w_err_b}'
        return 'FAIL', detail
    pass_b, err_b = _poll_bool_readback(addr, obj_long, inst, val_b, poll_to)

    detail = (f'write{val_a}→{"OK" if pass_a else err_a}, '
               f'write{val_b}→{"OK" if pass_b else err_b}')
    return ('PASS' if pass_a and pass_b else 'FAIL'), detail


def _write_test_ao(point: dict, addr: str, obj_long: str):
    """AO: 현재값과 다른값 1회 쓰기. Pair AI가 있으면 pair 확인.
    엑셀 minValue/maxValue가 있으면 write 값을 범위 내로 보정.
    반환: (write_result, write_detail)
    """


    inst      = point['instance']
    pair_inst = point.get('pair_instance')
    pair_long = OBJ_TYPE_MAP.get('AI', '') if pair_inst else ''

    # Excel에서 파싱된 min/max 읽기 (하위호환 키 포함)
    def _to_float_or_none(v):
        if v is None:
            return None
        if isinstance(v, str):
            s = v.strip()
            if s in ('', '-', 'None', 'none', 'nan', 'NaN'):
                return None
            v = s
        try:
            return float(v)
        except (TypeError, ValueError):
            return None

    ao_min = _to_float_or_none(
        point.get('minValue', point.get('minvalue', point.get('av_min', point.get('AV_MIN'))))
    )
    ao_max = _to_float_or_none(
        point.get('maxValue', point.get('maxvalue', point.get('av_max', point.get('AV_MAX'))))
    )

    cur_val, cur_err = bacnet_read(addr, obj_long, inst, 'presentValue')
    try:
        cur_float = float(cur_val) if cur_err == 'OK' and cur_val is not None else 0.0
    except Exception:
        cur_float = 0.0
    write_val = round(cur_float + 1.0, 1) if cur_float != 0.0 else 1.0

    if ao_max is not None and write_val > ao_max:
        write_val = ao_min
    if ao_min is not None and write_val < ao_min:
        write_val = ao_min

    ok, w_err = bacnet_write(addr, obj_long, inst, write_val)
    if not ok:
        _relinquish_async(addr, obj_long, inst)
        return 'FAIL', f'write:{w_err}'

    poll_to = _poll_timeout(point)
    if pair_inst:
        rb_passed, rb_err = _poll_float_readback(addr, pair_long, pair_inst, write_val, 1.0, poll_to)
    else:
        rb_passed, rb_err = _poll_float_readback(addr, obj_long, inst, write_val, 1.0, poll_to)

    _relinquish_async(addr, obj_long, inst)
    return ('PASS' if rb_passed else 'FAIL'), f'write{write_val}→{"OK" if rb_passed else rb_err}'


# ═══════════════════════════════════════════════════════
#  DS-COV-B 검증 (ASHRAE 135 §13)
# ═══════════════════════════════════════════════════════

# short type → bacpypes3 ObjectIdentifier 타입명 매핑
_COV_BAC0_TYPE = {
    'AI': 'analogInput',   'AO': 'analogOutput',  'AV': 'analogValue',
    'BI': 'binaryInput',   'BO': 'binaryOutput',  'BV': 'binaryValue',
    'MI': 'multiStateInput','MO': 'multiStateOutput','MV':'multiStateValue',
}


def _cov_subscribe(addr: str, bac0_type: str, inst: int,
                   lifetime: int, callback) -> int:
    """asyncio 컨텍스트에서 BAC0 cov() 호출 후 process_identifier 반환.
    내부적으로 asyncio.create_task() 를 사용하므로 aio_run() 으로 감쌈.
    """
    async def _do():
        from BAC0.scripts.Base import Base
        state['bacnet'].cov(
            address=addr,
            objectID=(bac0_type, inst),
            lifetime=lifetime,
            confirmed=False,
            callback=callback,
        )
        return Base._last_cov_identifier
    return aio_run(_do(), timeout=10.0)


def _cov_cancel(task_id: int):
    """COV 구독 취소 — task await으로 장치에 SubscribeCOV(lifetime=0) cancel 패킷 전달 보장.
    bac0의 cancel_cov는 로컬 task.stop()만 호출하고 await하지 않아
    change_of_value __aexit__의 cancel 패킷이 장치에 미전달됨.
    """
    try:
        from BAC0.scripts.Base import Base
        sub = Base._running_cov_tasks.pop(task_id, None)
        if sub is None:
            return  # 이미 취소되었거나 알 수 없는 task
        sub.stop()
        _task = getattr(sub, 'task', None)
        if _task and not _task.done():
            async def _await_cancel_task():
                try:
                    await asyncio.wait_for(_task, timeout=3.0)
                except Exception:
                    pass
            aio_run(_await_cancel_task(), timeout=4)
    except Exception:
        pass


def _cov_test_one(addr: str, write_point: dict, sub_point: dict = None) -> dict:
    """
    DS-COV-B 단일 페어 검증 시나리오:
      write_point : 값을 Write 할 오브젝트 (AV, MO, BO 등)
      sub_point   : COVNotification 을 Subscribe 할 오브젝트 (AI, MI, BI 등)
                    None 이면 write_point 자체를 Subscribe

      1. sub_point 의 covIncrement 읽기 (AI/AV)
      2. SubscribeCOV (sub_point, unconfirmed, lifetime=60s)
      3. Write trigger value → write_point
      4. Wait COV Notification on sub_point (max 8s)
      5. Verify notification PV == trigger value
      6. Unsubscribe (cancel)
    """
    w_short   = write_point['object_type']
    w_long    = OBJ_TYPE_MAP.get(w_short, w_short.lower())
    w_inst    = write_point['instance']

    # Subscribe 대상 결정 (pair 없으면 write_point 자체)
    if sub_point is None:
        sub_point = write_point
    s_short   = sub_point['object_type']
    s_long    = OBJ_TYPE_MAP.get(s_short, s_short.lower())
    s_inst    = sub_point['instance']
    s_bac0    = _COV_BAC0_TYPE.get(s_short, s_short.lower())

    if w_short == s_short and w_inst == s_inst:
        pair_label = f'{w_short}:{w_inst}'
    else:
        pair_label = f'{w_short}:{w_inst}, {s_short}:{s_inst}'

    r = {
        'idx':          write_point['idx'],
        'instance':     w_inst,
        'name':         f'{write_point["name"]} [{pair_label}]',
        'object_type':  w_short,
        'device_type':  write_point.get('device_type', ''),
        'subscribe':    '–',
        'trigger':      '–',
        'notification': '–',
        'unsubscribe':  '–',
        'overall':      'FAIL',
        'detail':       '',
    }

    if state['bacnet'] is None:
        r['detail'] = 'BACnet 미연결'
        r['overall'] = 'SKIP'
        return r

    # ── 1. sub_point 현재 PV 읽기 (trigger 계산 기준) ───────────────
    # COV는 write_point 값이 변해야 발생하므로, write_point의 현재값과 다른 값을 write해야 함
    # (s_short != w_short 페어에서 s_pv_val만 보면 w_point가 이미 해당 값일 수 있어 COV 안 발생)
    s_pv_val, s_pv_err = bacnet_read(addr, s_long, s_inst, 'presentValue')
    if s_pv_err != 'OK':
        r['detail'] = f'sub_point({s_short}:{s_inst}) PV 읽기 실패: {s_pv_err}'
        r['overall'] = 'SKIP'
        return r
    log.info('[COV] sub_point %s:%d 현재PV=%s', s_short, s_inst, s_pv_val)

    # w_point 현재 PV 읽기 (s_short != w_short 페어에서 trigger 계산 기준으로 사용)
    w_pv_val = None
    if w_short != s_short:
        _wv, _we = bacnet_read(addr, w_long, w_inst, 'presentValue')
        if _we == 'OK':
            w_pv_val = _wv
            log.info('[COV] write_point %s:%d 현재PV=%s', w_short, w_inst, w_pv_val)
        else:
            log.warning('[COV] write_point %s:%d PV 읽기 실패 (무시): %s', w_short, w_inst, _we)

    # ── COV_Increment 읽기 (sub_point 가 AI/AV 인 경우) ─────────────
    cov_inc = 1.0
    if s_short in ('AI', 'AV'):
        ci_val, ci_err = bacnet_read(addr, s_long, s_inst, 'covIncrement')
        if ci_err == 'OK' and ci_val is not None:
            try:
                cov_inc = float(ci_val)
            except (ValueError, TypeError):
                pass

    # ── 2. SubscribeCOV (sub_point) ──────────────────────────────────
    notif_q = Queue()

    def _on_cov(property_identifier, property_value):
        prop = str(property_identifier).lower().replace('-', '').replace('_', '')
        if 'presentvalue' in prop:
            notif_q.put((time.time(), property_value))

    try:
        task_id = _cov_subscribe(addr, s_bac0, s_inst, 60, _on_cov)
        r['subscribe'] = 'PASS'
        log.info('[COV] subscribe %s:%d OK (task_id=%d)', s_short, s_inst, task_id)
    except Exception as e:
        r['subscribe'] = 'FAIL'
        r['detail'] = f'SubscribeCOV 실패: {e}'
        return r

    time.sleep(0.5)  # 구독 ACK 전파 대기

    # ── 2b. write_point COV 구독 (w_short != s_short 인 페어 한정) ──────
    # AV:13→AI:19, MO:7→MI:8, BO:1→BI:2 에서 write 측 오브젝트도 COV 확인
    w_notif_q = Queue()
    w_task_id    = None
    if w_short != s_short:
        w_bac0 = _COV_BAC0_TYPE.get(w_short, w_short.lower())
        def _on_w_cov(property_identifier, property_value,
                      _q=w_notif_q):
            prop = str(property_identifier).lower().replace('-','').replace('_','')
            if 'presentvalue' in prop:
                _q.put((time.time(), property_value))
        try:
            w_task_id = _cov_subscribe(addr, w_bac0, w_inst, 60, _on_w_cov)
            log.info('[COV] also subscribed write_point %s:%d (task_id=%d)',
                     w_short, w_inst, w_task_id)
        except Exception as _e_w:
            log.warning('[COV] write_point %s:%d subscribe 실패 (무시): %s',
                        w_short, w_inst, _e_w)
            w_task_id = None
        time.sleep(0.3)  # w_point 구독 ACK 전파 대기

    # ── 초기 Notification flush ───────────────────────────────────────
    # Subscribe 직후 BACstac이 보내는 초기 알림(현재값)을 모두 소비한 뒤 write.
    # flush 완료 후 write_time 설정 → 이후 도착 알림만 progress로 판정.
    _INIT_FLUSH_MAX = 3      # 최대 소비 건수
    _INIT_FLUSH_PER = 1.5    # 알림 1건당 최대 대기 시간 (s)
    _flushed = 0
    _last_flushed_pv = None
    while _flushed < _INIT_FLUSH_MAX:
        try:
            _ts_f, _pv_f = notif_q.get(timeout=_INIT_FLUSH_PER)
        except Empty:
            break   # 더 이상 초기 알림 없음 → flush 완료
        _flushed += 1
        _last_flushed_pv = _pv_f
        log.debug('[COV] 초기 알림 flush #%d/%d (%s:%d PV=%s)',
                  _flushed, _INIT_FLUSH_MAX, s_short, s_inst, _pv_f)
    # flush 중 마지막으로 수신된 값으로 s_pv_val 갱신
    # flush가 최대 4.5s 동안 돌아가는 사이 장치 상태가 변했을 수 있으므로
    # trigger 계산 기준을 flush 직후의 실제 상태로 맞춰야 함
    # (갱신 안 하면: BI:2=inactive로 읽었으나 flush 중 active로 변해 trigger=active write →
    #  장치 입장에서 변화 없음 → COV 미발생 → FAIL)
    if _flushed > 0 and _last_flushed_pv is not None:
        _flushed_pv = _last_flushed_pv
        if str(_flushed_pv) != str(s_pv_val):
            log.info('[COV] flush 후 PV 갱신: %s:%d %s → %s (flush %d건)',
                     s_short, s_inst, s_pv_val, _flushed_pv, _flushed)
        s_pv_val = _flushed_pv
    log.debug('[COV] flush 완료 (%d건 소비), write 시작 (최종PV=%s)', _flushed, s_pv_val)
    # w_point 초기 알림도 flush — sub_point와 동일하게 _INIT_FLUSH_PER 사용
    # timeout=0.5s 였을 때 AV:13 초기 알림이 write 후에 도착해 rcv=18.0 불일치 발생
    if w_task_id is not None:
        _wf = 0
        while _wf < _INIT_FLUSH_MAX:
            try:
                _ts_wf, _pv_wf = w_notif_q.get(timeout=_INIT_FLUSH_PER)
            except Empty:
                break   # 더 이상 초기 알림 없음
            _wf += 1
            log.debug('[COV] w_point 초기 알림 flush #%d/%d (%s:%d PV=%s)',
                      _wf, _INIT_FLUSH_MAX, w_short, w_inst, _pv_wf)
        log.debug('[COV] w_point flush 완료 (%d건 소비)', _wf)

    # ── 3. Trigger Write (write_point) ───────────────────────────────
    # sub_point 현재값과 다른 값을 write해야 COV 이벤트가 발생함
    trigger_val = None
    try:
        s_pv_f = float(str(s_pv_val))
    except (ValueError, TypeError):
        s_pv_f = 0.0

    if s_short in ('AI', 'AV'):
        # trigger = sub_point 현재값(s_pv_f) + 0.5
        # (sub_point가 이미 trigger 값이면 → COV 발생 O)
        # 단, 계산된 trigger가 write_point 현재값(w_pv_val)과 같으면
        # 반대 방향(-delta)으로 재시도 — w_point 미변경 write 방지
        delta = 0.5
        trigger_val = round(s_pv_f + delta, 2)
        _w_current = None
        if w_pv_val is not None:
            try:
                _w_current = float(str(w_pv_val))
            except (ValueError, TypeError):
                pass
        # write_point 현재값과 충돌 시 반대 방향
        if _w_current is not None and abs(trigger_val - _w_current) < 0.01:
            trigger_val = round(s_pv_f - delta, 2)
            log.info('[COV] trigger 계산: %s:%d s_PV=%s + delta → w_point 충돌, 역방향 %s',
                     s_short, s_inst, s_pv_f, trigger_val)
        else:
            log.info('[COV] trigger 계산: %s:%d s_PV=%s + delta=%.1f → %s',
                     s_short, s_inst, s_pv_f, delta, trigger_val)
        # AV:13(SetRoomTemp) 18~30℃ 구간 순차 스윕
        # AI:19(s_pv_f, 실제 실내 온도) 기준: <=18 → 18.5, >=30 → 18.0(리셋), 그 외 +0.5
        if w_short == 'AV' and w_inst == 13:
            if s_pv_f >= 30.0:
                trigger_val = 18.0
            elif s_pv_f <= 18.0:
                trigger_val = 18.5
            else:
                trigger_val = round(s_pv_f + 0.5, 2)
            log.info('[COV] AV:13 스윕: AI:19 s_PV=%s → trigger=%s', s_pv_f, trigger_val)
    elif s_short in ('BI', 'BO', 'BV'):
        # trigger = sub_point(BI:2) 현재값의 반전
        # pre-sync 단계에서 BO:1==BI:2가 보장되므로 s_pv_val 기준이 올바름
        s_pv_lower  = str(s_pv_val).lower()
        _is_active  = (('active' in s_pv_lower and 'inactive' not in s_pv_lower)
                       or s_pv_lower in ('1', 'true'))
        trigger_val = 'inactive' if _is_active else 'active'
        log.info('[COV] trigger 계산: %s:%d s_PV=%s → 반전 %s',
                 s_short, s_inst, s_pv_val, trigger_val)
    else:
        # multistate: sub_point(MI:8) 현재값과 다른 state로 순환
        # MO:7은 skip_relinquish로 BO 테스트 전까지 상태 유지되므로 s_pv_val 기준 OK
        cur_state = int(s_pv_f) if s_pv_f else 1
        if w_short == 'MO' and w_inst == 7:
            trigger_val = 1 if cur_state == 4 else 4
            log.info('[COV] trigger 계산: %s:%d(OperationMode) s_PV=%s → state %s',
                     s_short, s_inst, cur_state, trigger_val)
        else:
            trigger_val = (cur_state % 3) + 1
            if trigger_val == cur_state:
                trigger_val = (trigger_val % 3) + 1
            log.info('[COV] trigger 계산: %s:%d s_PV=%s → state %s',
                     s_short, s_inst, cur_state, trigger_val)

    # fast DUT 대응: Notification이 writeProperty 응답보다 먼저 도착할 수 있다.
    # 따라서 post-write 판정 기준은 "write 완료 시각"이 아니라 "write 요청 직전 시각"을 사용한다.
    write_time = time.time()   # post-write 경계 기준(요청 시작 시각)
    write_ack_time = None      # 참고용(응답 완료 시각)
    write_ok, write_err = bacnet_write(addr, w_long, w_inst, trigger_val)
    if write_ok:
        write_ack_time = time.time()
        r['trigger'] = f'PASS ({s_pv_val}→{trigger_val})'
        try:
            ack_dt = write_ack_time - write_time
            log.info('[COV] write %s:%d trigger=%s OK (ack %.3fs)',
                     w_short, w_inst, trigger_val, ack_dt)
        except Exception:
            log.info('[COV] write %s:%d trigger=%s OK', w_short, w_inst, trigger_val)
    elif w_short in ('AI', 'BI', 'MI'):
        r['trigger'] = 'SKIP(read-only)'
        log.info('[COV] %s:%d read-only, write skip', w_short, w_inst)
    else:
        r['trigger'] = f'FAIL ({write_err})'
        r['detail']  = f'Write 실패: {write_err}'
        _cov_cancel(task_id)
        return r

    # ── 4. COV Notification 대기 — write 이후 알림 중 trigger값 일치 기다림 ──
    # - write_time(요청 시작) 이전 notification(초기값 알림)은 무시
    #   (요청~응답 사이 초고속으로 온 notification은 post-write로 인정)
    # - 나머지는 값이 일치할 때까지 반복 수신 (최대 _NOTIF_TIMEOUT 초)
    _sub_detail = ''  # section 4 결과 (sub_point)
    _w_detail   = ''  # section 4b 결과 (write_point)
    _NOTIF_TIMEOUT = 20.0
    deadline = time.time() + _NOTIF_TIMEOUT
    match = False
    rcv_pv = None
    match_ts = None
    pre_count  = 0   # write_time 이전 notification 건수 (initial 포함)
    mismatch_count = 0  # write 이후 도착했지만 값 불일치한 건수

    while time.time() < deadline:
        remaining = deadline - time.time()
        try:
            ts, rcv_pv = notif_q.get(timeout=max(remaining, 0.1))
        except Empty:
            break

        # write 이전에 이미 큐에 쌓여 있던 알림 (initial notification 포함) 무시
        if ts <= write_time:
            pre_count += 1
            log.debug('[COV] pre-write notification 무시 #%d (%s:%d PV=%s ts=%.3f <= write=%.3f)',
                      pre_count, s_short, s_inst, rcv_pv, ts, write_time)
            continue

        notif_dt = ts - write_time
        log.info('[COV] notification %s:%d PV=%s (trigger=%s, Δt=%.3fs)',
                 s_short, s_inst, rcv_pv, trigger_val, notif_dt)
        # 값 일치 확인
        try:
            if s_short in ('AI', 'AV'):
                match = abs(float(rcv_pv) - float(trigger_val)) <= max(cov_inc, 0.5)
            elif s_short in ('BI', 'BO', 'BV'):
                def _bv_norm(v):
                    s = str(v).lower().replace('-', '')
                    return 'active' if s in ('active', '1', 'true') else 'inactive'
                match = (_bv_norm(rcv_pv) == _bv_norm(trigger_val))
            else:  # MI, MO, MV
                match = int(float(rcv_pv)) == int(trigger_val)
        except Exception:
            match = False

        if match:
            match_ts = ts
            break
        mismatch_count += 1
        log.debug('[COV] post-write notification #%d 값 불일치 (PV=%s≠%s), 계속 대기...',
                   mismatch_count, rcv_pv, trigger_val)

    # write_point 원값 복원은 4b(w_point COV 확인) 이후에 수행
    # → relinquish 직후 발생하는 COV 알림(복원값)을 4b 대기에서 잡지 않도록 순서 조정

    _detail_suffix = ''.join([
        f' (pre-write 무시={pre_count}건)' if pre_count else '',
        f' (post-write 불일치={mismatch_count}건)' if mismatch_count else '',
    ])
    if rcv_pv is None:
        # timeout 내 write 이후 notification 자체가 없음
        r['notification'] = 'FAIL'
        _sub_detail = (f'[FAIL] {s_short}:{s_inst} COV 미수신 ({_NOTIF_TIMEOUT:.0f}s timeout, trigger={trigger_val})')
        log.warning('[COV] FAIL — %s:%d write 이후 notification 없음 %.0fs (trigger=%s%s)',
                    s_short, s_inst, _NOTIF_TIMEOUT, trigger_val, _detail_suffix)
    elif match:
        r['notification'] = 'PASS'
        _notif_dt = (match_ts if match_ts is not None else write_time) - write_time
        _sub_detail = f'{s_short}:{s_inst} COV 수신: PASS (PV={rcv_pv} 일치, Δt={_notif_dt:.3f}s)'
        log.info('[COV] PASS — %s:%d PV=%s 일치 Δt=%.3fs%s', s_short, s_inst, rcv_pv, _notif_dt, _detail_suffix)
    else:
        r['notification'] = 'FAIL'
        _sub_detail = (f'[FAIL] {s_short}:{s_inst} COV 수신: FAIL (PV={rcv_pv} 불일치, 기대={trigger_val})')
        log.warning('[COV] FAIL — %s:%d 값 불일치 최종rcv=%s 기대=%s%s',
                    s_short, s_inst, rcv_pv, trigger_val, _detail_suffix)

    # ── 4b. write_point COV notification 확인 (최대 3s 추가 대기) ─────
    if w_task_id is not None:
        w_deadline = time.time() + 3.0
        w_match = False
        w_rcv_pv = None

        def _w_pv_match(_pv):
            try:
                if w_short in ('AV',):
                    return abs(float(_pv) - float(trigger_val)) <= max(cov_inc, 0.5)
                elif w_short in ('BO', 'BV'):
                    return (str(_pv).lower().replace('-', '') ==
                            str(trigger_val).lower().replace('-', ''))
                else:  # MO
                    return int(float(_pv)) == int(trigger_val)
            except Exception:
                return bool(_pv is not None)

        # 루프 진입 전 이미 큐에 누적된 알림 먼저 처리 (초고속 수신 누락 방지)
        while True:
            try:
                ts_w, pv_w = w_notif_q.get_nowait()
            except Empty:
                break
            if ts_w <= write_time:
                continue
            w_rcv_pv = pv_w
            w_match = _w_pv_match(w_rcv_pv)
            if w_match:
                break

        if not w_match:
            # 아직 도착하지 않았거나 불일치 — 대기 루프
            while time.time() < w_deadline:
                remaining_w = w_deadline - time.time()
                try:
                    ts_w, pv_w = w_notif_q.get(timeout=max(remaining_w, 0.1))
                except Empty:
                    break
                if ts_w <= write_time:
                    continue
                w_rcv_pv = pv_w
                w_match = _w_pv_match(w_rcv_pv)
                if w_match:
                    break
        if w_rcv_pv is None:
            _w_detail = f'{w_short}:{w_inst} COV 수신: 미수신'
        elif w_match:
            _w_detail = f'{w_short}:{w_inst} COV 수신: PASS (PV={w_rcv_pv} 일치)'
        else:
            _w_detail = f'{w_short}:{w_inst} COV 수신: FAIL (PV={w_rcv_pv} 불일치, 기대={trigger_val})'
            # w_point 불일치도 전체 FAIL
            r['notification'] = 'FAIL'
        log.info('[COV] write_point %s:%d COV notif: %s', w_short, w_inst, _w_detail)
    # ── detail 조합: write_point 먼저, sub_point 다음 줄 ──────────────
    if _w_detail and _sub_detail:
        r['detail'] = _w_detail + '\n' + _sub_detail
    elif _w_detail:
        r['detail'] = _w_detail
    else:
        r['detail'] = _sub_detail

    # ── 5. Unsubscribe ────────────────────────────────────────────────
    _cov_cancel(task_id)
    if w_task_id is not None:
        _cov_cancel(w_task_id)
        log.info('[COV] unsubscribe write_point %s:%d OK', w_short, w_inst)
    r['unsubscribe'] = 'PASS'
    log.info('[COV] unsubscribe %s:%d OK', s_short, s_inst)

    # ── 종합 판정 ─────────────────────────────────────────────────────
    if r['subscribe'] == 'PASS' and r['notification'] == 'PASS':
        r['overall'] = 'PASS'
        log.info('[COV] ✓ PASS — %s:%d → %s:%d', w_short, w_inst, s_short, s_inst)
    elif r['subscribe'] == 'FAIL':
        r['overall'] = 'FAIL'
        log.warning('[COV] ✗ FAIL — %s:%d subscribe 실패', s_short, s_inst)
    elif r['trigger'] == 'SKIP(read-only)':
        r['overall'] = 'SKIP'
        log.info('[COV] SKIP — %s:%d read-only', w_short, w_inst)
    else:
        r['overall'] = 'FAIL'
        log.warning('[COV] ✗ FAIL — %s:%d → %s:%d: %s',
                    w_short, w_inst, s_short, s_inst, r['detail'])

    return r


# 고정 COV 페어:  (write_obj_short, write_inst, sub_obj_short, sub_inst, 설명)
# MO를 AV보다 먼저 실행 — MO write 후 relinquish가 일어나야 AV 테스트 시
# OperationMode가 neutral 상태가 되어 AV:13 온도 설정이 정상 반영됨
_COV_FIXED_PAIRS = [
#    ('MO',  7, 'MI',  8, 'MO:7(Setting)→MI:8(Status) COV'),
#    ('AV', 13, 'AI', 19, 'AV:13(Setting)→AI:19(Status) COV'),
#    ('BO',  1, 'BI',  2, 'BO:1(Setting)→BI:2(Status) COV'),
#    ('BV',  6, 'BV',  6, 'BV:6(FilterSignReset) COV'),
#    ('AO',  40051, 'AO',  40051, 'AO:40051(Fill Press) COV'),
    ('AO',  40071, 'AO',  40071, 'AO:40071(Ctrl. Type) COV'),
]


def _run_cov_tests(points: list, settings: dict) -> list:
    """
    DS-COV-B 검증 실행:
      - Device.activeCovSubscriptions ReadProperty 확인
      - 고정 페어 (AV:13→AI:19, MO:7→MI:8) COV 검증
        write 오브젝트에 값을 쓰고, pair input 오브젝트에서 COVNotification 수신 확인
    """
    addr    = state['device_ip']
    results = []

    # ── A. 6개 COV 포인트 일괄 구독 후 activeCovSubscriptions count==6 확인 ──
    dev_id   = state.get('device_id', 1234)
    dev_long = 'device'
    import time as _time_mod

    def _count_acs(val) -> int:
        """activeCovSubscriptions 반환값에서 구독 수 추출 (타입 무관 강건 파싱)."""
        if val is None:
            return 0
        if isinstance(val, int):
            return val
        try:
            return len(val)
        except TypeError:
            try:
                return int(val)
            except Exception:
                return 0

    # 이전 실행에서 _cov_cancel이 완료되기 전에 서버가 재시작된 경우
    # 장치에 잔류 구독이 남을 수 있음.
    # state에 저장된 이전 task_id 목록으로 추가 cancel 시도 (best-effort).
    _prev_task_ids = state.pop('_last_cov_pre_task_ids', [])
    for _ptid in _prev_task_ids:
        try:
            _cov_cancel(_ptid)
        except Exception:
            pass
    if _prev_task_ids:
        _time_mod.sleep(0.5)
        log.info('[COV-pre] 직전 실행 잔류 구독 %d개 cancel 시도 완료', len(_prev_task_ids))

    # 구독 전 baseline count 측정 (이전 테스트 잔류 구독 보정용)
    _acs_baseline_val, _acs_baseline_err = bacnet_read(addr, dev_long, dev_id, 'activeCovSubscriptions')
    _acs_baseline = _count_acs(_acs_baseline_val) if _acs_baseline_err == 'OK' else 0
    log.info('[COV-pre] baseline activeCovSubscriptions count=%d (err=%s)',
             _acs_baseline, _acs_baseline_err)

    # 고유 포인트 목록 산출 (self-pair 중복 제거 — BV:6 등)
    _pre_objects: list = []
    _pre_seen: set = set()
    for _pw, _wi, _ps, _si, _ in _COV_FIXED_PAIRS:
        for _pshort, _pinst in ((_ps, _si), (_pw, _wi)):
            if (_pshort, _pinst) not in _pre_seen:
                _pre_seen.add((_pshort, _pinst))
                _pre_objects.append((_pshort, _pinst))
    _expected_cov_count = len(_pre_objects)  # 고유 포인트 수

    # 일괄 SubscribeCOV (activeCovSubscriptions 카운트 확인용)
    # 구독은 모든 테스트가 끝날 때까지 유지 → C-post에서 해제
    _pre_task_ids = []
    for _pshort, _pinst in _pre_objects:
        _pbac0 = _COV_BAC0_TYPE.get(_pshort, _pshort.lower())
        try:
            _tid = _cov_subscribe(addr, _pbac0, _pinst, 60, lambda *a, **kw: None)
            _pre_task_ids.append(_tid)
            log.info('[COV-pre] subscribed %s:%d (task_id=%d)', _pshort, _pinst, _tid)
        except Exception as _pe:
            log.warning('[COV-pre] subscribe %s:%d 실패 (무시): %s', _pshort, _pinst, _pe)
    _time_mod.sleep(1.0)  # 구독 전파 대기

    # activeCovSubscriptions 읽기 — baseline 대비 증가분이 _expected_cov_count(6)이면 PASS
    acs_val, acs_err = bacnet_read(addr, dev_long, dev_id, 'activeCovSubscriptions')
    acs_count = _count_acs(acs_val) if acs_err == 'OK' else 0
    acs_delta = acs_count - _acs_baseline
    acs_pass  = (acs_err == 'OK' and acs_delta == _expected_cov_count)
    results.append({
        'idx':          0,
        'instance':     dev_id,
        'name':         f'Device:{dev_id} activeCovSubscriptions',
        'object_type':  'DEV',
        'device_type':  '',
        'subscribe':    '–',
        'trigger':      '–',
        'notification': '–',
        'unsubscribe':  '–',
        'overall':      'PASS' if acs_pass else 'FAIL',
        'detail':       f'ReadProperty: {acs_err}' + (
                            f', count={acs_delta} (기대={_expected_cov_count})'
                            if acs_err == 'OK' else ''),
    })
    log.info('[COV] Device activeCovSubscriptions: total=%d baseline=%d delta=%d pass=%s',
             acs_count, _acs_baseline, acs_delta, acs_pass)

    # ── B. 포인트 목록에서 인스턴스로 빠른 조회 ──────────────────────
    _by_inst: dict = {}
    for p in points:
        _by_inst[(p.get('object_type', ''), p['instance'])] = p

    # ── C-pre. AV:13(SetRoomTemp) 테스트 전 온도 범위 한계 확장 ─────
    # SetRoomTemp(AV:13) 쓰기 성공을 위해 사전에 상한(AV:27)=30, 하한(AV:28)=16 으로 설정.
    # 테스트 후 원래 값으로 복원한다.
    _TEMP_LIMIT_INST = [
        ('AV', 27, 30.0, 'TempRangeUpperLimitCommand'),  # 상한 → 최대 30℃
        ('AV', 28, 16.0, 'TempRangeLowerLimitCommand'),  # 하한 → 최소 16℃
    ]
    _temp_limit_originals = {}  # inst → original_value
    for tl_short, tl_inst, tl_target, tl_name in _TEMP_LIMIT_INST:
        tl_long       = 'analogValue'
        orig_val, err = bacnet_read(addr, tl_long, tl_inst, 'presentValue')
        if err == 'OK':
            _temp_limit_originals[tl_inst] = orig_val
            log.info('[COV-pre] %s AV:%d 현재값=%s', tl_name, tl_inst, orig_val)
        else:
            log.warning('[COV-pre] %s AV:%d 읽기 실패: %s', tl_name, tl_inst, err)
        ok, werr = bacnet_write(addr, tl_long, tl_inst, tl_target)
        if ok:
            log.info('[COV-pre] %s AV:%d → %s OK', tl_name, tl_inst, tl_target)
        else:
            log.warning('[COV-pre] %s AV:%d → %s 쓰기 실패: %s', tl_name, tl_inst, tl_target, werr)

    # ── C. 고정 페어 COV 검증 ────────────────────────────────────────
    for w_short, w_inst, s_short, s_inst, label in _COV_FIXED_PAIRS:
        write_p = _by_inst.get((w_short, w_inst))
        sub_p   = _by_inst.get((s_short, s_inst))

        # 포인트 목록에 없으면 최소 dict 생성 (인스턴스 번호만 있어도 동작)
        if write_p is None:
            write_p = {'idx': 0, 'instance': w_inst, 'name': label,
                       'object_type': w_short, 'device_type': ''}
        if sub_p is None:
            sub_p = {'idx': 0, 'instance': s_inst, 'name': f'{s_short}:{s_inst}',
                     'object_type': s_short, 'device_type': ''}

        log.info('[COV] 페어 검증 시작: %s', label)
        _broadcast({'type': 'cov_progress',
                    'name': label,
                    'object_type': w_short,
                    'instance': w_inst})

        r = _cov_test_one(addr, write_p, sub_p)
        results.append(r)
        _broadcast({'type': 'cov_result', 'result': r})
        _time_mod.sleep(0.5)  # COV 이벤트 수신 후 다음 테스트 write 전 안정화 대기

    # ── C-post. 온도 범위 한계 복원 생략 ─────────────────────────────
    # AV:27(상한=30), AV:28(하한=16) 은 COV 검증 후에도 유지한다.
    log.info('[COV-post] TempRangeLimit AV:27/AV:28 복원 생략 (유지).')

    # ── C-post. 사전 구독 해제 (모든 테스트 완료 후) ─────────────────
    # _cov_cancel이 task를 await하여 SubscribeCOV(lifetime=0) cancel 패킷을 장치에 전달.
    # _pre_task_ids를 state에 저장 → 다음 실행 시작 시 잔류분 추가 cancel (서버 재시작 대비).
    state['_last_cov_pre_task_ids'] = list(_pre_task_ids)
    for _tid in _pre_task_ids:
        try:
            _cov_cancel(_tid)
        except Exception:
            pass
    log.info('[COV-post] 사전 구독 %d개 해제 완료', len(_pre_task_ids))
    _time_mod.sleep(0.5)  # 해제 전파 대기 (다음 테스트 구독 슬롯 확보)

    return results


# ═══════════════════════════════════════════════════════
#  사전설정 적용 헬퍼
# ═══════════════════════════════════════════════════════
def _format_condition(point: dict) -> str:
    """관제점의 사전설정/판정설정 조건을 사람이 읽을 수 있는 문자열로 변환.
    결과는 UI "condition" 열에 표시됨.
    """
    parts = []

    # ── 장치설정 필터 ─────────────────────────────────────────────────────
    dev_filter = point.get('device_filter', [])
    if dev_filter:
        parts.append(f'장치설정:{",".join(dev_filter)}')

    # ── AV 범위 / ctrl_ranges ──────────────────────────────────────────────
    if point.get('object_type') == 'AV':
        ctrl_ranges = point.get('ctrl_ranges', {})
        av_min = point.get('av_min')
        av_max = point.get('av_max')
        if ctrl_ranges:
            ctrl = point.get('device_ctrl_method', '')
            if av_min is not None and av_max is not None:
                if ctrl:
                    parts.append(f'범위:{av_min}~{av_max}(ctrl={ctrl})')
                else:
                    range_info = ', '.join(
                        f'{k if k != "__default__" else "else"}={v[0]}~{v[1]}'
                        for k, v in ctrl_ranges.items()
                    )
                    parts.append(f'범위선택({range_info})')
        elif av_min is not None and av_max is not None:
            parts.append(f'범위:{av_min}~{av_max}')

    pre = point.get('pre_condition')
    if pre:
        if pre.get('occupied'):
            parts.append('사전설정 occupied=on')
        for cw in pre.get('cni_writes', []):
            parts.append(f"사전설정 {cw['cni']}={cw['value']}")

    jc = point.get('judge_condition', {})
    jc_real = {k: v for k, v in jc.items() if isinstance(v, dict)}  # __post_write_state 제외
    if jc_real:
        jc_parts = []
        for cmd, conds in sorted(jc_real.items()):
            cond_str = ', '.join(f'{k}={v}' for k, v in conds.items())
            jc_parts.append(f'cmd{cmd}:{cond_str}')
        parts.append('판정설정(' + ' | '.join(jc_parts) + ')')
    else:
        # command==X 형식이 아닌 판정설정(statetext, Both 등):
        # 파싱 불가 형식은 question 원문에서 판정설정 부분을 직접 표시
        q = point.get('question', '')
        if '판정' in q:
            m = re.search(r'판정\s*설\s*정\s*:(.*)', q, re.DOTALL)
            if m:
                raw = m.group(1).strip().replace('\n', ' ')
                parts.append(f'판정설정:{raw}')

    return ' / '.join(parts)


def _apply_ctrl_ranges(point: dict, addr: str) -> dict:
    """ctrl_ranges가 있는 AV 포인트에 대해 device_ctrl_method에 따른 av_min/max를 확정한다.

    device_ctrl_method가 아직 비어 있으면 description을 읽어 자동 감지한다.
    반환: 필요 시 av_min/av_max가 수정된 새 point dict
    """
    ctrl_ranges = point.get('ctrl_ranges', {})
    if not ctrl_ranges or point.get('object_type') != 'AV':
        return point

    detected_ctrl = point.get('device_ctrl_method', '')

    # device_ctrl_method가 비어있으면 description 읽기로 직접 감지
    if not detected_ctrl and addr:
        obj_long = OBJ_TYPE_MAP.get(point.get('object_type', ''),
                                    point.get('object_type', '').lower())
        val, err = bacnet_read(addr, obj_long, point['instance'], 'description')
        if err == 'OK' and val and str(val).strip().startswith('ctrl:'):
            detected_ctrl = str(val).strip()[5:]
            point = dict(point, device_ctrl_method=detected_ctrl)
            log.info('[ctrl_ranges] inst=%d description 직접 읽기 → ctrl=%s',
                     point['instance'], detected_ctrl)

    if detected_ctrl in ctrl_ranges:
        lo, hi = ctrl_ranges[detected_ctrl]
        log.info('[ctrl_ranges] inst=%d b2=%d ctrl=%s → av_min=%.1f av_max=%.1f',
                 point['instance'], point.get('device_b2', -1), detected_ctrl, lo, hi)
    elif '__default__' in ctrl_ranges:
        lo, hi = ctrl_ranges['__default__']
        log.info('[ctrl_ranges] inst=%d b2=%d ctrl=%s (else→default) → av_min=%.1f av_max=%.1f',
                 point['instance'], point.get('device_b2', -1), detected_ctrl or '(없음)', lo, hi)
    else:
        log.warning('[ctrl_ranges] inst=%d ctrl=%s 매칭 없음, K열 기본값 av_min=%s av_max=%s',
                    point['instance'], detected_ctrl, point.get('av_min'), point.get('av_max'))
        return point

    return dict(point, av_min=lo, av_max=hi)


def _apply_pre_condition(pre_cond: dict, addr: str, point: dict) -> tuple:
    """write 검증 전 사전설정 조건 적용.
    pre_cond = {'occupied': True, 'cni_writes': [{'cni': 'mode', 'value': 'cool'}, ...]}
               {'condition': {'exists': ['acMode','acOper']}, 'cni_writes': [...]}
    point    : 현재 검증 중인 관제점 (device_b2 매칭에 사용)
    반환: (success: bool, detail: str)
      success=True  → 사전설정 반영 확인, 후속 검증 진행 가능
      success=False → 사전설정 실패/미반영, 후속 검증 SKIP 권장
    """
    if not pre_cond:
        return True, ''

    b2       = point.get('device_b2', 0)
    dev_type = point.get('device_type', '')
    details  = []

    # ── if 조건 평가 (is exist 체크) ─────────────────────────────────────
    # 지정 CNI가 장치에 없으면 사전설정 전체 스킵 → 검증은 정상 진행 (True 반환)
    cond = pre_cond.get('condition', {})
    if cond:
        for cni_name in cond.get('exists', []):
            found = any(
                p.get('cni', '').lower() == cni_name.lower()
                and p.get('device_b2', 0) == b2
                and p.get('device_type', '') == dev_type
                for p in state.get('points', [])
            )
            if not found:
                log.info('[PreCond] if조건 불충족: %s 포인트 없음 → 사전설정 스킵, 검증 진행', cni_name)
                return True, f'사전설정 스킵({cni_name} 없음)'
        log.info('[PreCond] if조건 충족: %s 모두 존재', ', '.join(cond.get('exists', [])))

    # ── occupied BO 선행 적용 ────────────────────────────────────────────
    if pre_cond.get('occupied'):
        occ_point = None
        for p in state.get('points', []):
            if (p.get('object_type') in ('BO', 'BV')
                    and p.get('cni', '').lower() == 'occupied'
                    and p.get('device_b2', 0) == b2
                    and p.get('device_type', '') == dev_type):
                occ_point = p
                break
        if occ_point is None:
            log.warning('[PreCond] occupied BO/BV 없음 (device_b2=%d, device_type=%s)', b2, dev_type)
            return False, '사전설정 실패: occupied BO 없음'

        occ_long = OBJ_TYPE_MAP.get(occ_point['object_type'], '')
        ok, w_err = bacnet_write(addr, occ_long, occ_point['instance'], 'active')
        if not ok:
            log.warning('[PreCond] occupied write 실패: %s', w_err)
            return False, f'사전설정 실패: occupied ({w_err})'

        passed, poll_err = _poll_bool_readback(
            addr, occ_long, occ_point['instance'], 1, _POLL_TIMEOUT)
        if not passed:
            log.warning('[PreCond] occupied 반영 미확인 (%s)', poll_err)
            return False, f'사전설정 미반영: occupied ({poll_err})'

        log.info('[PreCond] occupied=on 반영 확인 (inst=%s)', occ_point['instance'])
        details.append('occupied=on→OK')

    # ── 범용 cni_writes 처리 ─────────────────────────────────────────────
    # 엑셀에 "사전설정:cni이름=>값" 추가만 하면 코드 수정 없이 자동 처리
    for cw in pre_cond.get('cni_writes', []):
        cni_name = cw['cni']
        val_str  = cw['value']

        # 같은 장치에서 해당 CNI의 commandable 포인트 탐색
        tgt = None
        for p in state.get('points', []):
            if (p.get('cni', '').lower() == cni_name.lower()
                    and p.get('object_type') in ('MO', 'AV', 'AO', 'BO', 'BV')
                    and p.get('device_b2', 0) == b2
                    and p.get('device_type', '') == dev_type):
                tgt = p
                break
        if tgt is None:
            # commandable 포인트 없음 → BI/AI/MI read-only 포인트에서 값 체크
            # 예: ntfcAvail=>Enabled → BI 읽어서 active(1)이면 진행, inactive이면 SKIP
            ro_tgt = None
            for p in state.get('points', []):
                if (p.get('cni', '').lower() == cni_name.lower()
                        and p.get('object_type') in ('BI', 'AI', 'MI')
                        and p.get('device_b2', 0) == b2
                        and p.get('device_type', '') == dev_type):
                    ro_tgt = p
                    break
            if ro_tgt is None:
                log.warning('[PreCond] cni=%s 포인트 없음 (device_b2=%d, device_type=%s)',
                            cni_name, b2, dev_type)
                return False, f'사전설정 실패: {cni_name} 포인트 없음'

            ro_long = OBJ_TYPE_MAP.get(ro_tgt['object_type'], '')
            cur_val, cur_err = bacnet_read(addr, ro_long, ro_tgt['instance'], 'presentValue')
            if cur_err != 'OK':
                return False, f'사전설정 실패: {cni_name} 읽기 오류({cur_err})'

            # Enabled/active/on/1 → 1, 나머지 → 0
            expected_active = val_str.lower() in ('enabled', 'active', 'on', '1', 'true')
            cur_active = _bv_to_int(cur_val) == 1 if ro_tgt['object_type'] == 'BI' else bool(cur_val)
            if expected_active and not cur_active:
                log.info('[PreCond] %s=%s 조건 불충족 (현재값=%s) → 검증 SKIP',
                         cni_name, val_str, cur_val)
                return False, f'사전조건 불충족: {cni_name} 비활성 상태 → SKIP'
            log.info('[PreCond] %s=%s 조건 확인 OK (현재값=%s)', cni_name, val_str, cur_val)
            details.append(f'{cni_name}확인→OK')
            continue

        obj_long = OBJ_TYPE_MAP.get(tgt['object_type'], '')
        otype    = tgt['object_type']

        if otype == 'MO':
            # state_texts에서 val_str 매칭 → state 번호 추출
            # 예: val_str='cool', state_texts=['Cool(1)','Dry(2)',...]
            target_state = None
            for st in tgt.get('state_texts', []):
                if val_str.lower() in st.lower():
                    m = re.search(r'\((\d+)\)', st)
                    if m:
                        target_state = int(m.group(1))
                        break
            if target_state is None:
                log.warning('[PreCond] cni=%s state=%s 매칭 없음 (state_texts=%s)',
                            cni_name, val_str, tgt.get('state_texts'))
                return False, f'사전설정 실패: {cni_name}={val_str} (state 매칭 없음)'
            ok, w_err = bacnet_write(addr, obj_long, tgt['instance'], target_state)
            if not ok:
                log.warning('[PreCond] %s write 실패: %s', cni_name, w_err)
                return False, f'사전설정 실패: {cni_name}={val_str} ({w_err})'
            log.info('[PreCond] %s (state%d) write 완료, 반영 확인 대기 중', cni_name, target_state)
            time.sleep(_poll_initial())
            _precond_timeout = (_POLL_TIMEOUT_PARALLEL if state.get('all_running') else _POLL_TIMEOUT) - _poll_initial()
            passed, poll_err = _poll_int_readback(
                addr, obj_long, tgt['instance'], target_state,
                _precond_timeout)
            if not passed:
                log.warning('[PreCond] %s=%s 반영 미확인 (%s)', cni_name, val_str, poll_err)
                return False, f'사전설정 미반영: {cni_name}={val_str} ({poll_err})'
            details.append(f'{cni_name}={val_str}(state{target_state})→OK')

        elif otype in ('AV', 'AO'):
            # 숫자 값 write
            try:
                write_val = float(val_str)
            except ValueError:
                return False, f'사전설정 실패: {cni_name}={val_str} (숫자 변환 불가)'
            ok, w_err = bacnet_write(addr, obj_long, tgt['instance'], write_val)
            if not ok:
                log.warning('[PreCond] %s write 실패: %s', cni_name, w_err)
                return False, f'사전설정 실패: {cni_name}={write_val} ({w_err})'
            log.info('[PreCond] %s=%.1f write 완료 (inst=%s)', cni_name, write_val, tgt['instance'])
            details.append(f'{cni_name}={write_val}→OK')

        elif otype in ('BO', 'BV'):
            bac_val = 'active' if val_str.lower() in ('on', '1', 'active', 'true') else 'inactive'
            ok, w_err = bacnet_write(addr, obj_long, tgt['instance'], bac_val)
            if not ok:
                log.warning('[PreCond] %s write 실패: %s', cni_name, w_err)
                return False, f'사전설정 실패: {cni_name}={val_str} ({w_err})'
            details.append(f'{cni_name}={bac_val}→OK')

    return True, ', '.join(details)


def _check_judge_condition(judge_cond: dict, state_val: int, addr: str,
                           point: dict) -> tuple:
    """판정설정 조건으로 write 결과 검증.

    judge_cond  : {state_int: {cni_name: expected_int, ...}, ...}
    state_val   : 방금 write 한 state 번호
    addr        : 장치 IP
    point       : 현재 검증 중인 관제점 (device_b2 매칭에 사용)

    반환: (passed: bool, detail: str)
    해당 state에 대한 판정 조건이 없으면 (None, '') 반환 → 호출 측에서 기본 readback 사용.
    """
    cond = judge_cond.get(state_val)
    if not cond or not isinstance(cond, dict):  # __post_write_state 등 int 값 스킵
        return None, ''

    b2       = point.get('device_b2', 0)
    dev_type = point.get('device_type', '')
    details  = []
    all_pass = True

    for cni_name, expected in cond.items():
        # state['points']에서 해당 CNI + **같은 장치** 오브젝트 탐색
        target_point = None
        for p in state.get('points', []):
            if (p.get('cni', '').strip() == cni_name
                    and p.get('device_b2', 0) == b2
                    and p.get('device_type', '') == dev_type):
                target_point = p
                break

        if target_point is None:
            log.warning('[JudgeCond] cni=%s 오브젝트 없음 (device_b2=%d, device_type=%s)',
                        cni_name, b2, dev_type)
            details.append(f'{cni_name}=없음')
            all_pass = False
            continue

        obj_long = OBJ_TYPE_MAP.get(target_point['object_type'], '')
        rb_val, rb_err = bacnet_read(addr, obj_long, target_point['instance'], 'presentValue')

        if rb_err != 'OK' or rb_val is None:
            details.append(f'{cni_name}=읽기실패')
            all_pass = False
            continue

        try:
            actual = int(float(str(rb_val)))
        except Exception:
            # binary active/inactive 처리
            s = str(rb_val).lower()
            actual = 1 if s in ('active', '1', 'true') else 0

        passed = (actual == expected)
        if not passed:
            all_pass = False
        details.append(f'{cni_name}:{actual}(기대{expected})→{"OK" if passed else "NG"}')

    return all_pass, ', '.join(details)


# ═══════════════════════════════════════════════════════
#  단일 관제점 검증 코어 (전체/개별 검증 공통 사용)
# ═══════════════════════════════════════════════════════
def _test_one_point(point: dict, settings: dict, addr: str) -> dict:
    """관제점 1개 검증 후 result dict 반환"""
    obj_short = point['object_type']
    obj_long  = OBJ_TYPE_MAP.get(obj_short, '')
    inst      = point['instance']
    demo_mode = not BACNET_OK or state['bacnet'] is None

    is_odu     = (point.get('device_type') == 'ODU')
    is_scroll2 = (point.get('device_type') == 'Chiller')
    is_general = (point.get('device_type') == 'General')

    result = {
        'idx': point['idx'],
        'instance': inst,
        'name': point['name'],
        'object_type': obj_short,
        'device_type': point.get('device_type', ''),
        'device_b2':   point.get('device_b2', 0),
        'read_result': '–',
        'read_value': '',
        'write_result': '–',
        'write_detail': '',   # 서브 테스트 상세 (BO:2회, MO:N회, AV:3회)
        'prop_result': '–',
        'missing_props': [],
        'overall': 'SKIP',
        'error': '',
        'condition': _format_condition(point),
        # ODU 전용 기대값 비교 필드
        'expected_value': '',
        'value_match':    '',
    }

    # BACnet 미연결(demo_mode) 시 SKIP 사유 표시
    if demo_mode:
        result['error'] = 'BACnet 미연결'

    # EXPIO: 미장착 모듈 항목 → 즉시 SKIP (타임아웃 방지)
    if point.get('expio_skip'):
        result['error'] = point.get('expio_skip_reason', 'EXPIO 모듈 미장착')
        log.info('[EXPIO] SKIP inst=%s (%s)', inst, result['error'])
        return result

    # Vent 장치에서는 F열이 '-'인 DXHRV 전용 항목을 검증하지 않는다.
    # 상위 경로의 필터가 누락돼도 코어 단계에서 FAIL 대신 SKIP으로 방어한다.
    if (point.get('device_type') == 'Vent'
            and point.get('vent_skip')
            and not point.get('is_dxhrv', False)):
        result['error'] = 'Vent 미지원 항목(DXHRV 전용)'
        log.info('[Vent] SKIP inst=%s %s → %s',
                 inst, point['name'], result['error'])
        return result

    # CNI '-' 인 관제점은 검증 불가 → SKIP
    # General 장치: 가상 브로드캐스트 유닛이므로 CNI 없어도 직접 BACnet Write로 검증
    if point.get('cni', '').strip() in ('-', '') and not is_general:
        result['error'] = 'CNI 없음'
        log.info('[Skip] inst=%s %s → CNI 없음', inst, point['name'])
        return result

    # general_read_pass_only: Read를 항상 강제 수행 (settings 무관)
    if point.get('general_read_pass_only'):
        _fr = dict(settings)
        _fr['test_read'] = True
        settings = _fr

    # ① ReadProperty 검증
    if settings.get('test_read', True):
        val, err = bacnet_read(addr, obj_long, inst, 'presentValue')
        if err == 'OK':
            result['read_result'] = 'PASS'
            # ODU: float 부동소수점 노이즈 제거 (15.300000190734863 → 15.3)
            if is_odu or is_scroll2:
                try:
                    result['read_value'] = f'{float(val):g}'
                except (TypeError, ValueError):
                    result['read_value'] = str(val)
            else:
                result['read_value'] = str(val)
        elif err in ('NO_DEVICE', 'UNKNOWN_OBJECT'):
            # NO_DEVICE: BACnet 미연결, UNKNOWN_OBJECT: 장치에 해당 오브젝트 없음
            # → 테스트 불가, 이하 전 단계 SKIP 후 즉시 반환
            result['read_result']  = 'SKIP'
            result['write_result'] = 'SKIP'
            result['prop_result']  = 'SKIP'
            result['overall']      = 'SKIP'
            result['error'] = '오브젝트 없음' if err == 'UNKNOWN_OBJECT' else 'BACnet 미연결'
            log.info('[Skip] inst=%s %s → %s', inst, point['name'], result['error'])
            return result
        else:
            result['read_result'] = 'FAIL'
            result['error'] = err
            log.warning('[Read] inst=%s %s:%s → FAIL (%s)', point['name'], obj_short, inst, err)
        # (327684): TotalAccumulatedPower — Read 성공 시 항상 PASS

        # ODU: instance 번호에서 직접 master slot(1-43)을 계산해 기대값과 비교.
        # master: slot 0-42, slave1: 64-106, slave2: 128-170, slave3: 192-234
        # → (instance & 0xFF) - 1 로 0-based slot 추출 후 오프셋 제거
        if is_odu and result['read_result'] == 'PASS':
            raw_slot = (inst & 0xFF) - 1          # 0-based slot (0-234)
            if raw_slot >= 192:
                base_slot = raw_slot - 192         # slave3 → 0-42
            elif raw_slot >= 128:
                base_slot = raw_slot - 128         # slave2 → 0-42
            elif raw_slot >= 64:
                base_slot = raw_slot - 64          # slave1 → 0-42
            else:
                base_slot = raw_slot               # master → 0-42
            pno = base_slot + 1                    # 1-based (1-43)
            # 서브타입별 기대값 우선 사용, 없으면 ODU_EXPECTED 폴백
            # (api_load_excel 시 facilitySubType 기반으로 odu_expected_value 주입됨)
            exp = point.get('odu_expected_value')
            if exp is None and pno in ODU_EXPECTED:
                exp = ODU_EXPECTED[pno]
            if exp is not None:
                odu_subtype = point.get('odu_subtype', '')
                subtype_tag = f'[{odu_subtype}]' if odu_subtype else ''
                result['expected_value'] = f'{exp}{subtype_tag}'
                bac_f = _bac_val_to_float(val, obj_short)
                if bac_f is None:
                    result['value_match'] = 'FAIL(변환불가)'
                    result['read_result'] = 'FAIL'
                    result['error'] = f'값 변환 실패: {val!r}'
                elif abs(bac_f - exp) <= ODU_TOLERANCE:
                    result['value_match'] = 'MATCH'
                else:
                    result['value_match'] = 'MISMATCH'
                    result['read_result'] = 'FAIL'
                    result['error'] = f'기대={exp}{subtype_tag}, 실제={bac_f}'
                    log.warning('[ODU] 값 불일치 inst=%s name=%s subtype=%s 기대=%.3f 실제=%.3f',
                                inst, point['name'], odu_subtype or '(없음)', exp, bac_f)
            else:
                result['expected_value'] = '매핑없음'
                result['value_match'] = f'SKIP(point_no={pno})'

        # EXPIO AI(UI)/BI(DI): 기대값 체크 (ACS IO, ACU 공통)
        is_expio_expected = (point.get('device_type') == 'EXPIO'
                             and 'expected_read_value' in point)
        if is_expio_expected and result['read_result'] == 'PASS':
            _expio_f = _bac_val_to_float(val, obj_short)
            _exp_v   = point['expected_read_value']
            result['expected_value'] = str(_exp_v)
            if _expio_f is None:
                result['value_match'] = 'FAIL(변환불가)'
                result['read_result'] = 'FAIL'
                result['error'] = f'값 변환 실패: {val!r}'
                log.warning('[EXPIO] 값 변환 실패 inst=%s name=%s val=%r',
                            inst, point['name'], val)
            else:
                if obj_short == 'AI':
                    _slot = (inst & 0xFF) - 1
                    _tol = _ACU_UI_TOLERANCE if _slot in _ACU_UI_SLOTS else _EXPIO_UI_TOLERANCE
                else:
                    _tol = 0.1
                if abs(_expio_f - _exp_v) <= _tol:
                    result['value_match'] = 'MATCH'
                    log.info('[EXPIO] 기대값 일치 inst=%s name=%s 기대=%g 실제=%g',
                             inst, point['name'], _exp_v, _expio_f)
                else:
                    result['value_match'] = 'MISMATCH'
                    result['read_result'] = 'FAIL'
                    result['error'] = f'기대={_exp_v}, 실제={_expio_f}'
                    log.warning('[EXPIO] 기대값 불일치 inst=%s name=%s 기대=%g 실제=%g',
                                inst, point['name'], _exp_v, _expio_f)

        # AIR_SCROLL2: 센티넬값(-999999 등) 및 기대값 비교 (pno=inst & 0xFF)
        if is_scroll2 and result['read_result'] == 'PASS':
            # ── 센티넬값 검사: 장치 미연결/데이터 미수신 시 -999999 등 반환
            _scroll2_f = _bac_val_to_float(val, obj_short)
            _SENTINEL_THRESHOLD = -99999.0
            if _scroll2_f is None:
                result['read_result'] = 'FAIL'
                result['value_match'] = 'FAIL(변환불가)'
                result['error'] = f'값 변환 실패: {val!r}'
                log.warning('[SCROLL2] 값 변환 실패 inst=%s name=%s val=%r',
                            inst, point['name'], val)
            elif _scroll2_f <= _SENTINEL_THRESHOLD:
                result['read_result'] = 'FAIL'
                result['value_match'] = 'FAIL(센티넬값)'
                result['error'] = f'센티넬값 수신={_scroll2_f:.0f} (장치 미연결 의심)'
                log.warning('[SCROLL2] 센티넬값 수신 inst=%s name=%s val=%g',
                            inst, point['name'], _scroll2_f)
            else:
                pno = inst & 0xFF          # slot+1 (1-42)
                if pno in SCROLL2_EXPECTED:
                    exp = SCROLL2_EXPECTED[pno]
                    result['expected_value'] = str(exp)
                    if abs(_scroll2_f - exp) <= SCROLL2_TOLERANCE:
                        result['value_match'] = 'MATCH'
                    else:
                        result['value_match'] = 'MISMATCH'
                        result['read_result'] = 'FAIL'
                        result['error'] = f'기대={exp}, 실제={_scroll2_f}'
                        log.warning('[SCROLL2] 값 불일치 inst=%s name=%s 기대=%.3f 실제=%.3f',
                                    inst, point['name'], exp, _scroll2_f)

    # ② WriteProperty 검증 (Commandable 객체만)
    if settings.get('test_write', True) and point['commandable']:
        if demo_mode:
            result['write_result'] = 'SKIP'
        else:
            # 사전설정 조건 적용 (예: mode=> cool 설정 후 검증)
            pre_cond = point.get('pre_condition')
            pre_detail = ''
            pre_ok = True
            if pre_cond:
                pre_ok, pre_detail = _apply_pre_condition(pre_cond, addr, point)
                log.info('[PreCond] %s inst=%s → %s', point['name'], inst, pre_detail)

            if not pre_ok:
                # 사전설정 미반영 → write 검증 SKIP
                result['write_result'] = 'SKIP'
                result['write_detail'] = pre_detail
            else:
                # 타입별 Write 검증 함수 호출
                if obj_short == 'BO':
                    wr, wd = _write_test_bo(point, addr, obj_long)
                elif obj_short == 'MO':
                    wr, wd = _write_test_mo(point, addr, obj_long)
                elif obj_short == 'AV':
                    if point.get('write_mode') == 'simple':
                        wr, wd = _write_test_expio_av(point, addr, obj_long)
                    else:
                        wr, wd = _write_test_av(point, addr, obj_long)
                elif obj_short == 'BV':
                    wr, wd = _write_test_bv(point, addr, obj_long)
                else:   # AO
                    wr, wd = _write_test_ao(point, addr, obj_long)

                result['write_result'] = wr
                # 사전설정이 있으면 write_detail 앞에 사전설정 결과 표시
                result['write_detail'] = f'[{pre_detail}] {wd}' if pre_detail else wd
                # UNKNOWN_OBJECT: 장치에 해당 오브젝트 없음 → SKIP
                if 'UNKNOWN_OBJECT' in wd:
                    result['write_result'] = 'SKIP'
                    result['write_detail'] = '오브젝트 없음'
                elif wr == 'FAIL':
                    result['error'] = wd
                log.debug('[Write] inst=%s type=%s → %s  (%s)', inst, obj_short, wr, wd)

    # ③ BTL 필수 프로퍼티 확인
    if settings.get('test_props', True):
        required = REQUIRED_PROPS.get(obj_short, [])
        if not demo_mode:
            missing = [p for p in required
                       if bacnet_read(addr, obj_long, inst, p)[1] not in ('OK',)]
            result['missing_props'] = missing
            result['prop_result'] = 'PASS' if not missing else 'FAIL'
            if missing:
                log.warning('[Prop] inst=%s %s:%s → FAIL 누락프로퍼티=%s',
                            point['name'], obj_short, inst, missing)
        else:
            result['prop_result'] = 'SKIP'

    # ④ 종합 판정
    statuses = [result['read_result'], result['write_result'], result['prop_result']]
    if 'FAIL' in statuses:
        result['overall'] = 'FAIL'
    elif any(s == 'PASS' for s in statuses):
        result['overall'] = 'PASS'
    else:
        result['overall'] = 'SKIP'

    return result


def _broadcast(ev):
    """모든 활성 SSE 연결에 이벤트 브로드캐스트 (연결별 큐 방식)."""
    for q in state['sse_queues'][:]:
        try:
            q.put_nowait(ev)
        except Exception:
            pass   # 가득 찬 큐는 무시 (연결 끊김 상태)


def _shutdown_oper_points(addr: str, points: list):
    """전체 검증 완료 후 CNI=oper BO를 inactive(OFF)로 전환.
    검증 중 ON 유지했던 기동 명령을 원복.
    """
    obj_long = OBJ_TYPE_MAP.get('BO', 'binaryOutput')
    for p in points:
        if (p.get('object_type') == 'BO'
                and p.get('cni', '').strip().lower() == 'oper'
                and p.get('commandable')):
            ok, err = bacnet_write(addr, obj_long, p['instance'], 'inactive')
            if ok:
                log.info('[Shutdown] CNI=oper inst=%s → OFF(inactive) 완료', p['instance'])
            else:
                log.warning('[Shutdown] CNI=oper inst=%s OFF 실패: %s', p['instance'], err)


# ═══════════════════════════════════════════════════════
#  검증 실행 엔진 (백그라운드 스레드 — 전체 검증)
# ═══════════════════════════════════════════════════════
def _check_cmd4avail(points: list, addr: str) -> bool:
    """Vent/DXHRV 장치의 cmd4Avail BI를 읽어 DXHRV 여부를 반환한다.
    반환: True=DXHRV(active), False=Vent(inactive 또는 포인트 없음)
    """
    cmd4_pt = next((p for p in points if p.get('cni') == 'cmd4Avail'), None)
    if cmd4_pt is None:
        log.info('[cmd4Avail] 포인트 없음 → Vent 모드로 검증')
        return False
    val, err = bacnet_read(addr, OBJ_TYPE_MAP.get(cmd4_pt['object_type'],
                           cmd4_pt['object_type'].lower()),
                           cmd4_pt['instance'], 'presentValue')
    if err != 'OK' or val is None:
        log.warning('[cmd4Avail] 읽기 실패(%s) → Vent 모드로 검증', err)
        return False
    # BI: 'active'/'inactive' 문자열 또는 1.0/0.0
    s = str(val).lower()
    is_dxhrv = ('active' in s and 'inactive' not in s) or \
               (s not in ('inactive', '0', '0.0') and s not in ('', 'none'))
    try:
        is_dxhrv = float(val) != 0.0
    except (ValueError, TypeError):
        pass
    log.info('[cmd4Avail] inst=%d 값=%s → %s 모드로 검증',
             cmd4_pt['instance'], val, 'DXHRV' if is_dxhrv else 'Vent')
    return is_dxhrv


# ── EXPIO subtype 자동 감지 ──────────────────────────────────────────────────
def _fetch_expio_subtypes(device_ip: str) -> set:
    """SSH로 장치에 접속해 expio-module facility의 facilitySubType 집합을 반환.

    성공 시: {'pexpmb000', 'pexpmb100', ...} 형태의 set
    실패 시: 빈 set (연결 성공/검증에 영향 없음)
    """
    import subprocess, json as _json
    cmd = (
        "luna-send -n 1 "
        "luna://com.b2b.dataManager.service/facility/getList "
        "'{\"facilityType\":\"expio-module\"}'"
    )
    try:
        result = subprocess.run(
            ['ssh', '-o', 'StrictHostKeyChecking=no',
             '-o', 'ConnectTimeout=5',
             f'root@{device_ip}', cmd],
            capture_output=True, text=True, timeout=10
        )
        payload = result.stdout.strip()
        if not payload:
            log.warning('[EXPIO] SSH facility/getList 응답 없음')
            return set()
        data = _json.loads(payload)
        facilities = data.get('facilities', [])
        subtypes = {f.get('facilitySubType', '') for f in facilities
                    if f.get('facilitySubType')}
        log.info('[EXPIO] 감지된 expio-module subtype: %s', sorted(subtypes))
        return subtypes
    except Exception as e:
        log.warning('[EXPIO] subtype 감지 실패: %s', e)
        return set()


def _fetch_expio_modules(device_ip: str) -> dict:
    """SSH로 장치에서 expio-module 모듈 목록을 slaveId별로 반환.
    반환 형식: {'acs_io': [(slaveId, name)], 'acu': [(slaveId, name, subtype)]}
    실패 시 빈 dict.
    """
    import subprocess, json as _json
    _ACU_SUBTYPES = {'pexpmb100', 'pexpmb200', 'pexpmb300'}
    cmd = ("luna-send -n 1 luna://com.b2b.dataManager.service/facility/getList"
           " '{}'")
    try:
        r = subprocess.run(
            ['ssh', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=5',
             f'root@{device_ip}', cmd],
            capture_output=True, text=True, timeout=10)
        payload = r.stdout.strip()
        if not payload:
            log.warning('[EXPIO] _fetch_expio_modules: SSH 응답 없음')
            return {}
        data = _json.loads(payload)
        acs_io_list = []
        acu_list    = []
        for f in data.get('facilities', []):
            if f.get('facilityType') != 'expio-module':
                continue
            subtype  = f.get('facilitySubType', '')
            name     = f.get('name', '') or f.get('facilityName', '')
            # slaveId는 extendedInfo.modbus.slaveId 에 있음
            slave_id = (f.get('extendedInfo', {}).get('modbus', {}).get('slaveId')
                        or f.get('modbusSlaveId') or f.get('slaveId'))
            if slave_id is None:
                continue
            slave_id = int(slave_id)
            if subtype == 'pexpmb000':
                acs_io_list.append((slave_id, name))
            elif subtype in _ACU_SUBTYPES:
                acu_list.append((slave_id, name, subtype))
        result = {}
        if acs_io_list:
            result['acs_io'] = sorted(acs_io_list, key=lambda x: x[0])
        if acu_list:
            result['acu'] = sorted(acu_list, key=lambda x: x[0])
        log.info('[EXPIO] 모듈 감지: acs_io=%s acu=%s', result.get('acs_io'), result.get('acu'))
        return result
    except Exception as e:
        log.warning('[EXPIO] _fetch_expio_modules 실패: %s', e)
        return {}


def _build_expio_points(base_points: list, expio_modules: dict, idx_start: int,
                        slave_filter: set = None) -> list:
    """expio_modules 정보로 올바른 인스턴스 번호를 가진 포인트 목록을 생성.

    base_points의 instance는 엑셀에서 파싱된 베이스값(b2=0 기준).
    인스턴스 공식: (DEVICE_EXP_IO=8 << 16) | (slaveId << 8) | (slot + 1)

    ACS IO 모듈(pexpmb000): slot 0-13 포인트만, 각 모듈의 slaveId 사용
    ACU   모듈(pexpmb100/200/300): slot 14-38 포인트만, 각 모듈의 slaveId 사용

    slave_filter: 포함할 slaveId 집합. None이면 전체 모듈 대상.
                 호출 전에 api_load에서 순차 인덱스 → slaveId 변환 완료.
    """
    _DEVICE_EXP_IO_BASE = 8 << 16   # 524288
    result = []
    idx = idx_start

    acs_io_mods = [(sid, nm) for (sid, nm) in expio_modules.get('acs_io', [])
                   if slave_filter is None or sid in slave_filter]
    acu_mods    = [(sid, nm, st) for (sid, nm, st) in expio_modules.get('acu', [])
                   if slave_filter is None or sid in slave_filter]

    for p in base_points:
        s1b = p['instance'] & 0xFF
        if s1b == 0:
            continue
        slot = s1b - 1   # 0-based slot

        if slot in _EXPIO_ACS_IO_SLOTS:
            for (slave_id, unit_name) in acs_io_mods:
                entry = dict(p)
                entry['instance']         = _DEVICE_EXP_IO_BASE | (slave_id << 8) | (slot + 1)
                entry['pair_instance']    = (
                    (_DEVICE_EXP_IO_BASE | (slave_id << 8) | ((p['pair_instance'] & 0xFF)))
                    if p.get('pair_instance') else None
                )
                entry['device_type']      = 'EXPIO'
                entry['device_b2']        = slave_id
                entry['device_unit_name'] = unit_name
                entry['idx']              = idx
                entry['condition']        = _format_condition(entry)
                if entry.get('object_type') == 'AV':
                    entry['write_mode'] = 'simple'  # min/max/mid 3회 write-read
                # ACS IO AI(UI) 기대값: slot 4-7 → 5/6/7/8
                if entry.get('object_type') == 'AI' and slot in _EXPIO_UI_EXPECTED:
                    entry['expected_read_value'] = _EXPIO_UI_EXPECTED[slot]
                # ACS IO BI(DI) 기대값: slot 11-13 → 1 (Active)
                if entry.get('object_type') == 'BI' and slot in _EXPIO_DI_SLOTS:
                    entry['expected_read_value'] = _EXPIO_DI_EXPECTED
                result.append(entry)
                idx += 1
        elif slot in _EXPIO_ACU_SLOTS:
            for (slave_id, unit_name, _subtype) in acu_mods:
                entry = dict(p)
                entry['instance']         = _DEVICE_EXP_IO_BASE | (slave_id << 8) | (slot + 1)
                entry['pair_instance']    = (
                    (_DEVICE_EXP_IO_BASE | (slave_id << 8) | ((p['pair_instance'] & 0xFF)))
                    if p.get('pair_instance') else None
                )
                entry['device_type']      = 'EXPIO'
                entry['device_b2']        = slave_id
                entry['device_unit_name'] = unit_name
                entry['idx']              = idx
                entry['condition']        = _format_condition(entry)
                if entry.get('object_type') == 'AV':
                    entry['write_mode'] = 'simple'  # min/max/mid 3회 write-read
                if entry.get('object_type') == 'BV':
                    entry['write_mode'] = 'bv_toggle'  # 토글 2회 write-read
                # ACU AI(UI) 기대값: slot 33-38 → 20.0
                if entry.get('object_type') == 'AI' and slot in _ACU_UI_SLOTS:
                    entry['expected_read_value'] = _ACU_UI_EXPECTED
                # ACU BI(DI) 기대값: slot 20-28 → 1.0 (Active)
                if entry.get('object_type') == 'BI' and slot in _ACU_DI_SLOTS:
                    entry['expected_read_value'] = _ACU_DI_EXPECTED
                result.append(entry)
                idx += 1

    log.info('[EXPIO] _build_expio_points: %d포인트 생성 (acs_io모듈=%d, acu모듈=%d)',
             len(result), len(acs_io_mods), len(acu_mods))
    return result


def _detect_unit_names(points: list, addr: str) -> dict:
    """각 (device_type, device_b2) 그룹의 첫 번째 포인트 description을 읽어
    facilityName 또는 control_method를 자동 감지한다.

    bacnet_init_object.c description 인코딩 규칙:
      - control_method가 있으면: "ctrl:{method}"  (예: "ctrl:awhpWtCtrl")
      - 없으면:                  facilityName 그대로 (예: "ahu-commkit-sa")

    반환: {(device_type, device_b2): {'name': str, 'ctrl': str}}
      - name: facilityName/SubType 값 (ctrl 있을 때는 빈 문자열)
      - ctrl: control_method 값 (없으면 빈 문자열)
    """
    group_rep: dict = {}
    for p in points:
        key = (p.get('device_type', ''), p.get('device_b2', 0))
        if key not in group_rep:
            group_rep[key] = p

    result: dict = {}
    for key, rep in group_rep.items():
        obj_long = OBJ_TYPE_MAP.get(rep['object_type'],
                                    rep['object_type'].lower())
        val, err = bacnet_read(addr, obj_long, rep['instance'], 'description')
        if err == 'OK' and val and str(val).strip():
            raw_desc = str(val).strip()
            if raw_desc.startswith('ctrl:'):
                ctrl_val = raw_desc[5:].strip()
                result[key] = {'name': '', 'ctrl': ctrl_val}
                log.info('[UnitDetect] %s b2=%d → control_method="%s"',
                         key[0], key[1], ctrl_val)
            else:
                result[key] = {'name': raw_desc, 'ctrl': ''}
                log.info('[UnitDetect] %s b2=%d → facilityName="%s"',
                         key[0], key[1], raw_desc)
        else:
            result[key] = {'name': '', 'ctrl': ''}
            log.info('[UnitDetect] %s b2=%d description 읽기 실패(%s), unit_name 미설정',
                     key[0], key[1], err)
    return result


def _apply_expio_subtype_filter(all_points: list, subtypes: set) -> None:
    """EXPIO 포인트에 expio_skip 플래그를 설정한다.

    - ACS IO 슬롯(0-13, inst & 0xFF in 1-14):  pexpmb000 없으면 expio_skip=True
    - ACU 슬롯  (14-38, inst & 0xFF in 15-39): pexpmb100/200/300 없으면 expio_skip=True
    """
    has_acs_io = 'pexpmb000' in subtypes
    has_acu    = any(st in subtypes for st in ('pexpmb100', 'pexpmb200', 'pexpmb300'))

    # subtype을 아직 모를 경우(연결 전 load-excel) 필터 미적용
    if not subtypes:
        return

    skipped_acs = skipped_acu = 0
    for p in all_points:
        if p.get('device_type') != 'EXPIO':
            continue
        slot_1based = p['instance'] & 0xFF   # instance = (dev<<16)|(msid<<8)|(slot+1)
        if slot_1based == 0:
            continue  # 비정상 인스턴스
        slot = slot_1based - 1  # 0-based
        if slot <= 13:          # ACS IO 슬롯 0-13
            if not has_acs_io:
                p['expio_skip'] = True
                skipped_acs += 1
        else:                   # ACU 슬롯 14-38
            if not has_acu:
                p['expio_skip'] = True
                skipped_acu += 1

    if skipped_acs or skipped_acu:
        log.info('[EXPIO] subtype 필터: ACS IO skip=%d, ACU skip=%d (subtypes=%s)',
                 skipped_acs, skipped_acu, sorted(subtypes))


def _normalize_vent_points(points: list, addr: str,
                           log_prefix: str = 'Vent/DXHRV') -> list:
    """Vent/DXHRV 공통 시트 포인트를 b2별 실제 장치 타입에 맞게 정규화한다."""
    vent_groups = {}
    for idx, point in enumerate(points):
        if point.get('device_type') != 'Vent':
            continue
        key = (point.get('device_type', ''), point.get('device_b2', 0))
        vent_groups.setdefault(key, []).append((idx, point))

    if not vent_groups:
        return points

    normalized = {}
    skipped = set()

    for (_, b2), items in sorted(vent_groups.items(), key=lambda item: item[0][1]):
        unit_points = [point for _, point in items]
        is_dxhrv = _check_cmd4avail(unit_points, addr)

        if is_dxhrv:
            for idx, point in items:
                normalized[idx] = dict(
                    point,
                    cni=point.get('dxhrv_cni', point['cni']),
                    is_dxhrv=True,
                )
            log.info('[%s] b2=%d DXHRV 확인 → G열(dxhrv_cni) 기준, %d개 검증',
                     log_prefix, b2, len(unit_points))
            continue

        before = len(unit_points)
        kept = 0
        for idx, point in items:
            if point.get('vent_skip', False):
                skipped.add(idx)
                continue
            normalized[idx] = dict(point, cni=point.get('vent_cni', point['cni']))
            kept += 1
        log.info('[%s] b2=%d Vent 확인 → F열(vent_cni) 기준, vent_skip 제외 %d→%d개 검증',
                 log_prefix, b2, before, kept)

    result = []
    for idx, point in enumerate(points):
        if point.get('device_type') != 'Vent':
            result.append(point)
            continue
        if idx in skipped:
            continue
        result.append(normalized.get(idx, point))

    return result


def run_tests_thread(points: list, settings: dict):
    state['running'] = True
    state['results'] = []

    addr  = state['device_ip']

    # ── Vent/DXHRV 자동 판별 ────────────────────────────────────────────
    # 다중 Vent 유닛 로드 시에도 b2별로 DXHRV/Vent를 독립 판별해야 한다.
    if any(p.get('device_type') == 'Vent' for p in points):
        points = _normalize_vent_points(points, addr, 'Vent/DXHRV')

    # ── EXPIO subtype 필터 적용 ──────────────────────────────────────────
    # expio_skip=True 항목은 장치에 해당 모듈이 없으므로 검증 제외
    # ※ 엑셀 로드 후 연결한 경우에 대비해 최신 subtypes로 재필터링
    _apply_expio_subtype_filter(points, state.get('expio_subtypes', set()))
    before_expio = len(points)
    for _ep in points:
        if _ep.get('expio_skip'):
            log.info('[Skip] inst=%s %s → EXPIO 모듈 미장착 (%s)',
                     _ep['instance'], _ep['name'],
                     _ep.get('expio_skip_reason', 'EXPIO 모듈 미장착'))
    points = [p for p in points if not p.get('expio_skip', False)]
    if len(points) < before_expio:
        log.info('[EXPIO] expio_skip 항목 제외: %d → %d개 검증',
                 before_expio, len(points))

    # ── 장치 facilityName / control_method 자동 감지 ─────────────────────
    has_device_filter    = any(p.get('device_filter') for p in points)
    has_ctrl_method_skip = any(p.get('control_method_skip') for p in points)
    has_ctrl_ranges      = any(p.get('ctrl_ranges') for p in points)
    non_ac_empty = any(
        not p.get('device_unit_name', '').strip() and
        p.get('device_type', 'Aircon') != 'Aircon'
        for p in points
    )
    if has_device_filter or has_ctrl_method_skip or has_ctrl_ranges or non_ac_empty:
        log.info('[UnitDetect] 장치설정/control_method 필터 존재 → description 자동 감지 시작')
        detected = _detect_unit_names(points, addr)
        if detected:
            def _apply_detected(p):
                key = (p.get('device_type', ''), p.get('device_b2', 0))
                info = detected.get(key, {'name': '', 'ctrl': ''})
                return dict(p,
                            device_unit_name=info['name'],
                            device_ctrl_method=info['ctrl'])
            points = [_apply_detected(p) for p in points]
            log.info('[UnitDetect] 감지 완료: %s',
                     {f"{k[0]}[b2={k[1]}]": v for k, v in detected.items()})
        else:
            log.warning('[UnitDetect] description 읽기 실패 — 필터 비교 불가, 모든 포인트 검증')

    total = len(points)

    for i, point in enumerate(points):
        if not state['running']:
            break

        # ── 포인트 없음 → SKIP ──────────────────────────────────────────
        if point.get('no_point_info'):
            skip_result = {
                'idx':          point['idx'],
                'instance':     point['instance'],
                'name':         point['name'],
                'object_type':  point['object_type'],
                'read_result':  '–',
                'read_value':   '',
                'write_result': '–',
                'write_detail': '',
                'prop_result':  '–',
                'missing_props': [],
                'overall':      'SKIP',
                'error':        '포인트 없음: BACnet 오브젝트 미등록 관제점',
                'condition':    point.get('condition', ''),
                'expected_value': '',
                'value_match':    '',
            }
            log.info('[Skip] inst=%s %s → 포인트 없음', point['instance'], point['name'])
            state['results'].append(skip_result)
            _broadcast({'type': 'progress', 'current': i + 1, 'total': total,
                        'percent': int((i + 1) / total * 100), 'result': skip_result})
            continue

        # ── ODU single/multi slave → SKIP (master만 검증) ──────────────
        # master: slot 0-42 (inst & 0xFF ≤ 43), slave1: 64-106, slave2: 128-170, slave3: 192-234
        if point.get('device_type') == 'ODU':
            sub = (point.get('odu_subtype') or
                   state.get('odu_subtypes', {}).get(point.get('device_b2', -1), ''))
            raw_slot = (point['instance'] & 0xFF) - 1
            if sub in ('single', 'multi') and raw_slot >= 64:
                _slave_skip = {
                    'idx':           point['idx'],
                    'instance':      point['instance'],
                    'name':          point['name'],
                    'object_type':   point['object_type'],
                    'read_result':   '–',
                    'read_value':    '',
                    'write_result':  '–',
                    'write_detail':  '',
                    'prop_result':   '–',
                    'missing_props': [],
                    'overall':       'SKIP',
                    'error':         f'{sub} ODU slave(slot={raw_slot}) — master만 검증',
                    'condition':     point.get('condition', ''),
                    'expected_value': '',
                    'value_match':    '',
                }
                log.info('[Skip] inst=%s %s → %s slave(slot=%d)',
                         point['instance'], point['name'], sub, raw_slot)
                state['results'].append(_slave_skip)
                _broadcast({'type': 'progress', 'current': i + 1, 'total': total,
                            'percent': int((i + 1) / total * 100),
                            'result': _slave_skip})
                continue

        # ── General always-skip: 전체 테스트에서만 SKIP (단독 실행은 허용) ──
        if point.get('general_always_skip'):
            _asskip = {
                'idx':           point['idx'],
                'instance':      point['instance'],
                'name':          point['name'],
                'object_type':   point['object_type'],
                'read_result':   '–',
                'read_value':    '',
                'write_result':  '–',
                'write_detail':  '',
                'prop_result':   '–',
                'missing_props': [],
                'overall':       'SKIP',
                'error':         '항상 SKIP (재부팅 위험 — 단독 테스트만 허용)',
                'condition':     point.get('condition', ''),
                'expected_value': '',
                'value_match':    '',
            }
            log.info('[Skip] inst=%s %s → 항상SKIP (재부팅위험)', point['instance'], point['name'])
            state['results'].append(_asskip)
            _broadcast({
                'type': 'progress',
                'current': i + 1,
                'total': total,
                'percent': int((i + 1) / total * 100),
                'result': _asskip,
            })
            continue

        # ── control_method SKIP: 해당 method이면 SKIP ───────────────────
        ctrl_skip = point.get('control_method_skip', '')
        if ctrl_skip:
            detected_ctrl = point.get('device_ctrl_method', '')
            if detected_ctrl == ctrl_skip:
                skip_result = {
                    'idx':          point['idx'],
                    'instance':     point['instance'],
                    'name':         point['name'],
                    'object_type':  point['object_type'],
                    'read_result':  '–',
                    'read_value':   '',
                    'write_result': '–',
                    'write_detail': '',
                    'prop_result':  '–',
                    'missing_props': [],
                    'overall':      'SKIP',
                    'error':        f'control_method 제외: {ctrl_skip} 장치 (awhpWtCtrl 전용 제어)',
                    'condition':    point.get('condition', ''),
                    'expected_value': '',
                    'value_match':    '',
                }
                log.info('[Skip] inst=%s %s → control_method=%s', point['instance'], point['name'], ctrl_skip)
                state['results'].append(skip_result)
                _broadcast({'type': 'progress', 'current': i + 1, 'total': total,
                            'percent': int((i + 1) / total * 100), 'result': skip_result})
                continue

        # ── 장치설정 필터: device_filter에 명시된 장치가 아닌 경우 SKIP ──
        dev_filter = point.get('device_filter', [])
        if dev_filter:
            unit_name = point.get('device_unit_name', '').strip()
            if not unit_name or unit_name not in dev_filter:
                skip_result = {
                    'idx':          point['idx'],
                    'instance':     point['instance'],
                    'name':         point['name'],
                    'object_type':  point['object_type'],
                    'read_result':  '–',
                    'read_value':   '',
                    'write_result': '–',
                    'write_detail': '',
                    'prop_result':  '–',
                    'missing_props': [],
                    'overall':      'SKIP',
                    'error':        f'장치설정 제외: {unit_name or "(이름미지정)"} ∉ {dev_filter}',
                    'condition':    point.get('condition', ''),
                    'expected_value': '',
                    'value_match':    '',
                }
                log.info('[Skip] inst=%s %s → 장치설정 제외 (%s ∉ %s)', point['instance'], point['name'], unit_name or '(이름미지정)', dev_filter)
                state['results'].append(skip_result)
                _broadcast({
                    'type': 'progress',
                    'current': i + 1,
                    'total': total,
                    'percent': int((i + 1) / total * 100),
                    'result': skip_result,
                })
                continue

        # 테스트 시작 전 "진행중" 이벤트 전송
        _broadcast({
            'type': 'start',
            'current': i + 1,
            'total': total,
            'point': {
                'idx':         point['idx'],
                'instance':    point['instance'],
                'name':        point['name'],
                'object_type': point['object_type'],
                'condition':   point.get('condition', ''),
            },
        })

        # ── ctrl_ranges: control_method별 AV 범위 분기 (예: AWHP settemp) ──
        point = _apply_ctrl_ranges(point, addr)

        result = _test_one_point(point, settings, addr)
        state['results'].append(result)

        _broadcast({
            'type': 'progress',
            'current': i + 1,
            'total': total,
            'percent': int((i + 1) / total * 100),
            'result': result,
        })

        # commandable 포인트만 inter-test delay 적용
        # (read-only 포인트는 write 여파 없으므로 대기 불필요)
        # SKIP된 포인트는 write를 시도하지 않았으므로 딜레이 불필요
        if (point.get('commandable') and settings.get('test_write', True)
                and result.get('overall') != 'SKIP'):
            time.sleep(_POLL_INITIAL)

    state['running'] = False
    state['points']  = points   # device_unit_name 등 감지 결과 반영

    # 전체 검증 완료 후 oper BO → OFF(inactive) 처리
    _shutdown_oper_points(addr, points)

    pass_cnt = sum(1 for r in state['results'] if r['overall'] == 'PASS')
    fail_cnt = sum(1 for r in state['results'] if r['overall'] == 'FAIL')
    skip_cnt = sum(1 for r in state['results'] if r['overall'] == 'SKIP')

    _broadcast({
        'type': 'complete',
        'pass': pass_cnt,
        'fail': fail_cnt,
        'skip': skip_cnt,
        'total': len(state['results']),
    })
    log.info(f"검증 완료: PASS={pass_cnt}, FAIL={fail_cnt}, SKIP={skip_cnt}")


# ═══════════════════════════════════════════════════════
#  All 버튼 병렬 검증 엔진 (기존 run_tests_thread 와 완전 분리)
#  문제 발생 시 아래 섹션만 제거하면 기존 동작으로 즉시 복구 가능
# ═══════════════════════════════════════════════════════

def _make_skip_result(point: dict, error_msg: str) -> dict:
    """SKIP 결과 딕셔너리 생성 헬퍼 (All 버튼 전용)"""
    return {
        'idx':            point['idx'],
        'instance':       point['instance'],
        'name':           point['name'],
        'object_type':    point['object_type'],
        'read_result':    '–',
        'read_value':     '',
        'write_result':   '–',
        'write_detail':   '',
        'prop_result':    '–',
        'missing_props':  [],
        'overall':        'SKIP',
        'error':          error_msg,
        'condition':      point.get('condition', ''),
        'expected_value': '',
        'value_match':    '',
    }


def _run_b2_worker(dev_type: str, b2: int, points: list, settings: dict,
                   addr: str, progress_ref: dict, lock: threading.Lock):
    """b2 유닛 1개의 관제점을 순차 검증하는 워커 (All 버튼 전용).

    progress_ref: {'completed': int, 'total': int} — 공유 진행 카운터 (lock 보호)
    lock        : completed 카운터 + results 리스트 동시 접근 보호
    """
    # ── Vent/DXHRV 판별: b2별 독립 실행 ─────────────────────────────────
    if dev_type == 'Vent':
        points = _normalize_vent_points(points, addr, 'All/Vent')

    for point in points:
        # All 검증 중지 요청 확인
        if not state.get('all_running'):
            break

        # ── SKIP 조건 검사 ────────────────────────────────────────────────
        skip_res = None
        if point.get('no_point_info'):
            skip_res = _make_skip_result(
                point, '포인트 없음: BACnet 오브젝트 미등록 관제점')
        elif point.get('general_always_skip'):
            skip_res = _make_skip_result(
                point, '항상 SKIP (재부팅 위험 — 단독 테스트만 허용)')
        elif point.get('control_method_skip'):
            ctrl_skip     = point.get('control_method_skip', '')
            detected_ctrl = point.get('device_ctrl_method', '')
            if detected_ctrl == ctrl_skip:
                skip_res = _make_skip_result(
                    point, f'control_method 제외: {ctrl_skip}')
        elif point.get('device_type') == 'ODU':
            sub = (point.get('odu_subtype') or
                   state.get('odu_subtypes', {}).get(point.get('device_b2', -1), ''))
            raw_slot = (point['instance'] & 0xFF) - 1
            if sub in ('single', 'multi') and raw_slot >= 64:
                skip_res = _make_skip_result(
                    point,
                    f'{sub} ODU slave(slot={raw_slot}) — master만 검증')

        if skip_res is None:
            dev_filter = point.get('device_filter', [])
            if dev_filter:
                unit_name = point.get('device_unit_name', '').strip()
                if not unit_name or unit_name not in dev_filter:
                    skip_res = _make_skip_result(
                        point,
                        f'장치설정 제외: {unit_name or "(이름미지정)"} ∉ {dev_filter}')

        if skip_res is not None:
            result = skip_res
        else:
            # ctrl_ranges 분기 (AV 범위 확정)
            point = _apply_ctrl_ranges(point, addr)
            result = _test_one_point(point, settings, addr)

        # ── 공유 카운터 갱신 & 브로드캐스트 ─────────────────────────────
        with lock:
            state['results'].append(result)
            progress_ref['completed'] += 1
            done  = progress_ref['completed']
            total = progress_ref['total']
            pct   = int(done / total * 100) if total > 0 else 0

        _broadcast({
            'type':    'progress',
            'current': done,
            'total':   total,
            'percent': pct,
            'result':  result,
        })

        # Commandable 포인트 간 인터-테스트 지연 (write 여파 방지)
        if (skip_res is None
                and point.get('commandable')
                and settings.get('test_write', True)):
            time.sleep(_POLL_INITIAL)


def run_tests_all_thread(devices_config: list, settings: dict,
                         max_workers: int = 4):
    """All 버튼 전용 병렬 검증 엔진.

    devices_config: [{device_type, b2_from, b2_to}, ...]
    max_workers   : 동시 실행 최대 워커 수 (기본 4, 최대 16)

    각 b2 유닛이 독립 스레드에서 실행되며, 모든 스레드는
    공유 asyncio 루프를 통해 BACnet I/O를 수행한다.
    기존 run_tests_thread 와 독립적으로 동작하므로
    이 함수 삭제 시 기존 동작에 전혀 영향 없음.
    """
    state['all_running'] = True
    state['results'] = []
    addr = state['device_ip']

    # SSE 연결 수립 대기
    time.sleep(0.5)

    # ── 포인트를 (device_type, b2) 키로 그룹화 ───────────────────────────
    b2_groups: dict = {}
    for point in state['points']:
        key = (point.get('device_type', ''), point.get('device_b2', 0))
        b2_groups.setdefault(key, []).append(point)

    # ── devices_config 기준 태스크 목록 구성 ─────────────────────────────
    tasks: list = []
    for dev in devices_config:
        dev_type = dev.get('device_type', '')
        b2_from  = int(dev.get('b2_from', 0))
        b2_to    = max(b2_from, int(dev.get('b2_to', b2_from)))
        for b2 in range(b2_from, b2_to + 1):
            pts = b2_groups.get((dev_type, b2), [])
            if pts:
                tasks.append((dev_type, b2, pts))

    if not tasks:
        state['all_running'] = False
        _broadcast({'type': 'complete', 'pass': 0, 'fail': 0, 'skip': 0, 'total': 0})
        log.warning('[All] 검증할 관제점 없음 — devices_config=%s', devices_config)
        return

    total_points = sum(len(pts) for _, _, pts in tasks)
    progress_ref = {'completed': 0, 'total': total_points}
    lock         = threading.Lock()

    log.info('[All] 병렬 검증 시작: %d 유닛, 총 %d 관제점, max_workers=%d',
             len(tasks), total_points, max_workers)
    for dev_type, b2, pts in tasks:
        log.info('[All]   - %s[b2=%d]: %d 관제점', dev_type, b2, len(pts))

    with concurrent.futures.ThreadPoolExecutor(
            max_workers=max_workers,
            thread_name_prefix='bacnet-all') as executor:
        futures = {
            executor.submit(
                _run_b2_worker, dev_type, b2, pts,
                settings, addr, progress_ref, lock
            ): f'{dev_type}[b2={b2}]'
            for dev_type, b2, pts in tasks
        }
        for future in concurrent.futures.as_completed(futures):
            key_str = futures[future]
            try:
                future.result()
                log.info('[All] 완료: %s', key_str)
            except Exception as exc:
                log.error('[All] 워커 예외 %s: %s', key_str, exc)

    state['all_running'] = False

    # 완료 후 oper BO → OFF
    _shutdown_oper_points(addr, state['points'])

    pass_cnt = sum(1 for r in state['results'] if r['overall'] == 'PASS')
    fail_cnt = sum(1 for r in state['results'] if r['overall'] == 'FAIL')
    skip_cnt = sum(1 for r in state['results'] if r['overall'] == 'SKIP')

    _broadcast({
        'type':  'complete',
        'pass':  pass_cnt,
        'fail':  fail_cnt,
        'skip':  skip_cnt,
        'total': len(state['results']),
    })
    log.info('[All] 검증 완료: PASS=%d, FAIL=%d, SKIP=%d', pass_cnt, fail_cnt, skip_cnt)


# ── All 버튼 전용 섹션 끝 ─────────────────────────────────────────────────


# ═══════════════════════════════════════════════════════
#  리포트 생성
# ═══════════════════════════════════════════════════════
def generate_report() -> tuple:
    """검증 결과를 Excel 리포트로 저장; 반환: (filepath, filename)"""
    if not OPENPYXL_OK:
        raise RuntimeError("openpyxl 필요: pip install openpyxl")

    wb = openpyxl.Workbook()
    ws = wb.active
    ws.title = 'BTL 검증 결과'

    # ── 스타일 정의
    hdr_fill  = PatternFill('solid', fgColor='1B2A3B')
    hdr_font  = Font(bold=True, color='FFFFFF')
    pass_fill = PatternFill('solid', fgColor='D5F5E3')
    fail_fill = PatternFill('solid', fgColor='FADBD8')
    skip_fill = PatternFill('solid', fgColor='F2F3F4')
    center    = Alignment(horizontal='center', vertical='center')

    # ── 제목 블록
    dev_types_used = sorted({p.get('device_type', '') for p in state['points'] if p.get('device_type')})
    dev_label = ', '.join(dev_types_used) if dev_types_used else '–'

    ws.append(['BACnet BTL 검증 결과 리포트'])
    ws['A1'].font = Font(bold=True, size=14)
    ws.append([f'생성: {datetime.now().strftime("%Y-%m-%d %H:%M:%S")}  |  '
               f'대상 장치: {state["device_ip"]}  |  DeviceID: {state["device_id"]}  |  '
               f'검증 장치: {dev_label}'])
    ws.append([])

    # ── 데이터 헤더
    headers = ['No.', '장치', '장치번호', '인스턴스', '타입', '기능명', 'BACnet 객체명 패턴',
               'Read 결과', 'Read 값', 'Write 결과',
               'Property 확인', '누락 프로퍼티', 'SKIP/오류 사유', '종합 결과']
    ws.append(headers)
    hdr_row = ws.max_row
    for col in range(1, len(headers) + 1):
        cell = ws.cell(hdr_row, col)
        cell.fill = hdr_fill
        cell.font = hdr_font
        cell.alignment = center

    # ── 데이터 행 (인스턴스 번호 오름차순 정렬)
    point_map = {p['instance']: p for p in state['points']}
    for r in sorted(state['results'], key=lambda x: x['instance']):
        pt = point_map.get(r['instance'], {})
        row = [
            r['idx'],
            DEVICE_REGISTRY.get(pt.get('device_type', ''), {}).get('label', pt.get('device_type', '')).split('(')[0].strip(),
            pt.get('device_b2', 0), r['instance'], r['object_type'], r['name'],
            pt.get('obj_name_template', ''),

            r['read_result'], r['read_value'],
            r['write_result'],
            r['prop_result'], ', '.join(r['missing_props']),
            r.get('error', ''),
            r['overall'],
        ]
        ws.append(row)
        ri = ws.max_row
        fill = {'PASS': pass_fill, 'FAIL': fail_fill}.get(r['overall'], skip_fill)
        for col in range(1, len(headers) + 1):
            ws.cell(ri, col).fill = fill
            ws.cell(ri, col).alignment = center

    # ── 컬럼 너비
    for col, w in enumerate([6, 22, 6, 12, 7, 35, 38, 10, 18, 10, 13, 35, 35, 10], 1):
        ws.column_dimensions[get_column_letter(col)].width = w

    # ── 요약 시트
    ws2 = wb.create_sheet('요약')
    total = len(state['results'])
    pass_cnt = sum(1 for r in state['results'] if r['overall'] == 'PASS')
    fail_cnt = sum(1 for r in state['results'] if r['overall'] == 'FAIL')
    skip_cnt = sum(1 for r in state['results'] if r['overall'] == 'SKIP')

    ws2.append(['BTL 검증 요약'])
    ws2['A1'].font = Font(bold=True, size=13)
    ws2.append([])
    ws2.append(['항목', '건수', '비율'])
    for h in ['A3', 'B3', 'C3']:
        ws2[h].font = Font(bold=True)
        ws2[h].fill = PatternFill('solid', fgColor='1B2A3B')
        ws2[h].font = Font(bold=True, color='FFFFFF')
    ws2.append(['PASS ✅', pass_cnt, f'{pass_cnt/total*100:.1f}%' if total else '–'])
    ws2.append(['FAIL ❌', fail_cnt, f'{fail_cnt/total*100:.1f}%' if total else '–'])
    ws2.append(['SKIP ⏭', skip_cnt, f'{skip_cnt/total*100:.1f}%' if total else '–'])
    ws2.append(['합 계', total, '100%'])

    filename = f'BTL_CDU_Report_{datetime.now().strftime("%Y%m%d_%H%M%S")}.xlsx'
    filepath = str(_get_data_dir() / filename)
    wb.save(filepath)
    return filepath, filename


# ═══════════════════════════════════════════════════════
#  Flask 라우트
# ═══════════════════════════════════════════════════════
@app.route('/')
def index():
    return render_template('index.html')


@app.route('/api/logs')
def api_logs():
    """logs/ 디렉토리의 로그 파일 목록 반환"""
    files = sorted(LOG_DIR.glob('*.log'), key=lambda f: f.stat().st_mtime, reverse=True)
    return jsonify({
        'files': [
            {'name': f.name,
             'size': f.stat().st_size,
             'mtime': datetime.fromtimestamp(f.stat().st_mtime).strftime('%Y-%m-%d %H:%M:%S')}
            for f in files
        ],
        'current': state.get('log_file'),
    })


@app.route('/api/download-log/<filename>')
def api_download_log(filename):
    if '/' in filename or '..' in filename or not filename.endswith('.log'):
        return jsonify({'error': '잘못된 파일 이름'}), 400
    filepath = LOG_DIR / filename
    if not filepath.exists():
        return jsonify({'error': '파일 없음'}), 404
    return send_file(str(filepath), as_attachment=True, download_name=filename,
                     mimetype='text/plain')


@app.route('/api/status')
def api_status():
    # uploaded_excel 미등록 시 .btl_last_file 재확인 (서버 기동 후 파일이 생긴 경우 대응)
    if not state.get('uploaded_excel'):
        try:
            last = _abs_path(_LAST_FILE_PATH.read_text(encoding='utf-8').strip())
            if last and os.path.exists(last):
                state['uploaded_excel'] = last
        except Exception:
            pass

    return jsonify({
        'connected':          state['connected'],
        'device_ip':          state['device_ip'],
        'device_id':          state['device_id'],
        'own_ip':             state['own_ip'],
        'server_ip':          SERVER_IP,
        'points_loaded':      len(state['points']),
        'results_count':      len(state['results']),
        'running':            state['running'],
        'all_running':        state.get('all_running', False),
        'bacnet_available':   BACNET_OK,
        'xlrd_available':     XLRD_OK,
        'openpyxl_available': OPENPYXL_OK,
        'default_excel':      DEFAULT_EXCEL,
        'log_file':           state.get('log_file'),
        'b2_count':           state.get('b2_count', 0),
        'device_type':        state.get('device_type', 'Aircon'),
        'bacnet_port':        state.get('bacnet_port', 47808),
        'expio_subtypes':     sorted(state.get('expio_subtypes', set())),
        'expio_modules':      {
            'acs_io_count': len(state.get('expio_modules', {}).get('acs_io', [])),
            'acu_count':    len(state.get('expio_modules', {}).get('acu', [])),
        },
        'uploaded_excel':     state.get('uploaded_excel'),
        'excel_file_exists':  bool(
            state.get('uploaded_excel') and
            os.path.exists(state['uploaded_excel'])
        ),
        'devices':            [
            {'key': k, 'label': v['label']}
            for k, v in DEVICE_REGISTRY.items()
        ],
    })


@app.route('/api/upload-excel', methods=['POST'])
def api_upload_excel():
    """엑셀 파일 업로드 — multipart/form-data 수신 후 uploads/ 저장"""
    if 'file' not in request.files:
        return jsonify({'success': False, 'error': '파일이 없습니다'}), 400
    f = request.files['file']
    if not f.filename:
        return jsonify({'success': False, 'error': '파일을 선택하세요'}), 400

    ext = Path(f.filename).suffix.lower()
    if ext not in ('.xls', '.xlsx'):
        return jsonify({'success': False, 'error': '.xls / .xlsx 파일만 지원합니다'}), 400

    # 경로 탐색 공격 방지 — 파일명을 영숫자/언더스코어/점/하이픈만 허용
    safe_name = re.sub(r'[^\w.\-]', '_', Path(f.filename).name)
    save_path = UPLOAD_DIR / safe_name
    f.save(str(save_path))
    state['uploaded_excel'] = str(save_path)
    return jsonify({'success': True, 'filepath': str(save_path), 'filename': safe_name})


@app.route('/api/load-excel', methods=['POST'])
def api_load_excel():
    data     = request.json or {}
    filepath = data.get('filepath', '').strip() or state.get('uploaded_excel') or DEFAULT_EXCEL

    if not os.path.exists(filepath):
        return jsonify({'success': False, 'error': f'파일 없음: {filepath}'}), 400

    # ── 다중 장치 형식: devices=[{device_type, b2_count}, ...]
    # ── 하위 호환: 구형 단일 device_type / b2_count 도 지원
    devices_req = data.get('devices')
    if not devices_req:
        dev_key  = data.get('device_type', 'Aircon')
        b2_count = max(0, int(data.get('b2_count', 0)))
        devices_req = [{'device_type': dev_key, 'b2_count': b2_count}]

    try:
        all_points  = []
        dev_summary = []

        for req in devices_req:
            dev_key  = req.get('device_type', 'Aircon')
            b2_count = max(0, int(req.get('b2_count', 0)))

            if dev_key not in DEVICE_REGISTRY:
                return jsonify({'success': False, 'error': f'알 수 없는 장치 타입: {dev_key}'}), 400

            # b2_from/b2_to 우선 처리, 없으면 b2_count 하위 호환
            if 'b2_from' in req or 'b2_to' in req:
                b2_from = max(0, int(req.get('b2_from', 0)))
                b2_to   = max(b2_from, int(req.get('b2_to', b2_from)))
            else:
                b2_from = 0
                b2_to   = b2_count

            parse_fn    = DEVICE_REGISTRY[dev_key]['parse']
            base_points = parse_fn(filepath)

            # EXPIO: expio_modules 감지된 경우 올바른 인스턴스로 포인트 생성
            if dev_key == 'EXPIO' and state.get('expio_modules'):
                # b2_from~b2_to 를 slaveId 직접 입력값으로 사용
                # 예) b2_from=27, b2_to=30 → slaveId 27~30 모듈만 대상
                slave_filter = set(range(b2_from, b2_to + 1))
                built = _build_expio_points(base_points, state['expio_modules'],
                                            len(all_points) + 1, slave_filter)
                all_points.extend(built)
                # dev_summary: slave_filter 적용된 실제 슬레이브 ID만 표시
                label = DEVICE_REGISTRY[dev_key]['label'].split('(')[0].strip()
                mods  = state['expio_modules']
                acs_ids = [str(s) for (s, *_) in mods.get('acs_io', [])
                           if s in slave_filter]
                acu_ids = [str(s) for (s, *_) in mods.get('acu', [])
                           if s in slave_filter]
                parts = []
                if acs_ids: parts.append('ACS IO ' + ','.join(acs_ids) + '번')
                if acu_ids: parts.append('ACU ' + ','.join(acu_ids) + '번')
                dev_summary.append(f"{label} " + (', '.join(parts) if parts else f'{b2_from}번'))
            else:
                # 지정된 B2 범위 오프셋 적용 (폴백)
                for b2 in range(b2_from, b2_to + 1):
                    offset = b2 * 256
                    # ODU: b2 → facilitySubType 조회 (api_facility_info 또는 req 직접 전달)
                    odu_subtype = ''
                    if dev_key == 'ODU':
                        odu_subtype = (req.get('facility_sub_type') or '').strip()
                        if not odu_subtype:
                            odu_subtype = state.get('odu_subtypes', {}).get(b2, '')
                    for p in base_points:
                        entry             = dict(p)
                        entry['instance'] = p['instance'] + offset
                        entry['pair_instance'] = (
                            p['pair_instance'] + offset if p['pair_instance'] else None
                        )
                        entry['device_type'] = dev_key
                        entry['device_b2']   = b2
                        entry['idx']         = len(all_points) + 1
                        entry['condition']   = _format_condition(entry)
                        # ODU: 서브타입별 기대값 주입 (검증 시 ODU_EXPECTED 대신 사용)
                        if dev_key == 'ODU' and odu_subtype:
                            sv = p.get('subtype_expected', {}).get(odu_subtype)
                            if sv is not None:
                                entry['odu_expected_value'] = sv
                            entry['odu_subtype'] = odu_subtype
                        all_points.append(entry)

                label = DEVICE_REGISTRY[dev_key]['label'].split('(')[0].strip()
                count = b2_to - b2_from + 1
                if b2_from == b2_to:
                    dev_summary.append(f"{label} {b2_from}번 (1대)")
                else:
                    dev_summary.append(f"{label} {b2_from}~{b2_to}번 ({count}대)")

        # EXPIO: 장치에서 감지된 모듈 subtype 기반으로 불필요한 항목 SKIP 처리
        _apply_expio_subtype_filter(all_points, state.get('expio_subtypes', set()))

        state['points']      = all_points
        state['b2_count']    = 0        # 다중 장치 모드에서는 per-device 관리
        state['device_type'] = devices_req[0].get('device_type', 'Aircon')

        try:
            _LAST_FILE_PATH.write_text(_rel_path(filepath), encoding='utf-8')
        except Exception:
            pass

        type_counts = {}
        for p in all_points:
            type_counts[p['object_type']] = type_counts.get(p['object_type'], 0) + 1

        return jsonify({
            'success':        True,
            'total':          len(all_points),
            'device_summary': ' / '.join(dev_summary),
            'type_counts':    type_counts,
            'points':         all_points,
        })
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 400


# ── 네트워크 인터페이스 목록 조회 ────────────────────────────────────────────
@app.route('/api/network-interfaces')
def api_network_interfaces():
    """이 PC의 사용 가능한 IPv4 주소 목록 반환 (자동 감지 IP 포함)."""
    import socket
    ips = []
    try:
        # 방법 1: ifaddr 라이브러리 (BAC0 의존성으로 함께 설치됨)
        import ifaddr
        for adapter in ifaddr.get_adapters():
            for ip_info in adapter.ips:
                if isinstance(ip_info.ip, str) and not ip_info.ip.startswith('127.'):
                    ips.append({'ip': ip_info.ip, 'name': adapter.nice_name})
    except Exception:
        pass

    if not ips:
        # 방법 2: socket 기반 fallback
        try:
            hostname = socket.gethostname()
            for info in socket.getaddrinfo(hostname, None):
                if info[0] == socket.AF_INET:
                    ip = info[4][0]
                    if not ip.startswith('127.') and ip not in [x['ip'] for x in ips]:
                        ips.append({'ip': ip, 'name': hostname})
        except Exception:
            pass

    # 자동 감지 IP를 맨 앞으로
    auto_ip = SERVER_IP
    ips = sorted(ips, key=lambda x: (x['ip'] != auto_ip, x['ip']))

    return jsonify({'ips': ips, 'auto_ip': auto_ip})


@app.route('/api/connect', methods=['POST'])
def api_connect():
    data = request.json or {}
    ip = data.get('device_ip', '').strip()
    did = int(data.get('device_id', 1234))
    own = data.get('own_ip', '').strip()

    if not ip:
        return jsonify({'success': False, 'error': '장치 IP를 입력하세요'}), 400

    state['device_ip'] = ip
    state['device_id'] = did
    state['own_ip'] = own

    if not BACNET_OK:
        return jsonify({
            'success': False,
            'demo': True,
            'error': 'BAC0 미설치 — Demo 모드로 운영됩니다 (실제 통신 없음)',
        })

    async def _connect(ip_arg, device_ip, device_id):
        # ── 기존 BAC0 인스턴스 재사용 가능 여부 판단 ─────────────────────
        # 같은 own_ip·port로 이미 BAC0가 초기화된 경우 disconnect 없이 재사용.
        # (disconnect → rebind 사이에 OS가 포트를 반납하기까지 수 초 지연 발생)
        prev_bacnet   = state.get('bacnet')
        prev_ip_arg   = state.get('_last_bac0_ip_arg', '')
        prev_bac_port = state.get('_last_bac0_port', -1)
        bacnet_port   = state.get('bacnet_port', 47808)

        need_reinit = (
            prev_bacnet is None
            or prev_ip_arg != ip_arg
            or prev_bac_port != bacnet_port
        )

        if need_reinit:
            # 기존 연결 종료 후 OS 포트 반납 대기
            if prev_bacnet:
                try:
                    await prev_bacnet.disconnect()
                except Exception:
                    pass
                state['bacnet'] = None
                # OS가 CLOSE_WAIT → TIME_WAIT → 포트 반납까지 대기
                await asyncio.sleep(2.0)

            # BAC0.lite() 는 sync 생성자지만 내부에서 asyncio.create_task() 사용
            # → asyncio 컨텍스트(이 코루틴 내) 에서 호출해야 함
            bacnet = BAC0.lite(ip=ip_arg, port=bacnet_port)
            await asyncio.sleep(3)  # BAC0 내부 초기화 태스크 완료 대기
            state['_last_bac0_ip_arg']  = ip_arg
            state['_last_bac0_port']    = bacnet_port
        else:
            # 동일 own_ip·port → 기존 인스턴스 재사용 (포트 재바인딩 없음)
            bacnet = prev_bacnet
            log.info('[Connect] BAC0 인스턴스 재사용 (ip_arg=%s, port=%d)', ip_arg, bacnet_port)

        # 크로스 서브넷 환경: WhoIs 브로드캐스트가 도달하지 않아 I-Am 응답 없음
        # → device_info_cache에 직접 DeviceInfo를 등록해 bacnet.read() WhoIs 루프 우회
        # (bacnet_read는 _app.read_property() 직접 호출하므로 캐시 불필요하나 보조용으로 유지)
        try:
            from bacpypes3.app import DeviceInfo
            from bacpypes3.pdu import Address as BACAddress  # primitivedata가 아닌 pdu
            dev_addr = BACAddress(device_ip)
            dev_info = DeviceInfo(
                device_instance=device_id,
                device_address=dev_addr,
                max_apdu_length_accepted=480,  # VTS 패킷 확인: 0x03 = 480 octets
                segmentation_supported=3,      # no-segmentation
            )
            cache = bacnet.this_application.app.device_info_cache
            cache.address_cache[dev_addr] = dev_info
            cache.instance_cache[device_id] = dev_info
            log.info(f'DeviceInfo 캐시 등록 완료: {device_ip} (Device {device_id})')
        except Exception as e:
            log.warning(f'DeviceInfo 캐시 등록 실패: {e}')

        return bacnet

    try:
        ip_arg = f'{own}/24' if own else f'{SERVER_IP}/24'
        bacnet = aio_run(_connect(ip_arg, ip, did), timeout=30)
        state['bacnet'] = bacnet
        state['connected'] = True
        # BAC0.lite() 초기화 후 동적으로 추가된 handler에도 노이즈 필터 재적용
        # propagate=False 재설정: BAC0.lite()가 내부에서 propagate를 True로 되돌릴 수 있으므로
        _force_add_filter_to_all_handlers(_noise_filter)
        for _noisy_lib in ('BAC0', 'BAC0.scripts', 'BAC0.scripts.Lite',
                           'BAC0.core', 'BAC0.core.io', 'BAC0.core.io.Read',
                           'BAC0.core.io.Write', 'bacpypes3'):
            _nlg = logging.getLogger(_noisy_lib)
            _nlg.setLevel(logging.WARNING)
            _nlg.propagate = False
        # EXPIO 모듈 subtype/modules 자동 감지 (SSH, 실패해도 연결 성공으로 처리)
        state['expio_subtypes'] = _fetch_expio_subtypes(ip)
        state['expio_modules']  = _fetch_expio_modules(ip)
        log.info('[EXPIO] 모듈 정보 갱신: acs_io=%s acu=%s',
                 state['expio_modules'].get('acs_io'),
                 state['expio_modules'].get('acu'))
        return jsonify({'success': True,
                        'message': f'{ip} 연결 완료 (서버 바인딩: {ip_arg}, DeviceInfo 캐시 등록 완료)',
                        'expio_subtypes': sorted(state['expio_subtypes'])})
    except Exception as e:
        state['connected'] = False
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/facility-info', methods=['GET'])
def api_facility_info():
    """장치 facility 정보를 조회해 장치 유형별 번호 범위를 반환.

    조회 순서 (실패 시 다음으로 fallback):
      1. SCP로 facility_info.db 복사 (새 BACnet 서비스 배포 후)
      2. SSH + luna-send → dataManager/facility/getList (항상 사용 가능)

    반환:
      {
        "success": true,
        "source": "facility_info.db" | "dataManager",
        "devices": {
          "Aircon": {"min": 0, "max": 2, "count": 3, "addrs": [0,1,2]},
          ...
        }
      }
    """
    ip = state.get('device_ip', '').strip()
    if not ip:
        return jsonify({'success': False, 'error': '장치 IP가 설정되지 않았습니다'}), 400

    # facility_type → 검증 툴 device_type
    _TYPE_MAP = {
        'idu':          'Aircon',
        'cdu':          'CDU',
        'outdoor':      'ODU',
        'odu_unit':     'ODU',      # dataManager 실제 facilityType
        'vent':         'Vent',
        'dxhrv':        'Vent',     # DXHRV는 Vent 계열
        'ahu':          'AHU',
        'awhp':         'AWHP',
        'general':      'General',
        'expio-module': 'EXPIO',
        'expio':        'EXPIO',    # dataManager 실제 facilityType
        'chiller':      'Chiller',
        'isc2':         'Chiller',
        'aircare':      'Aircare',
    }

    import subprocess, json as _json, sqlite3 as _sqlite3, tempfile, os

    rows = []
    source = ''

    # ── 방법 1: SCP로 facility_info.db 복사 (새 BACnet 서비스 배포 후) ──
    DB_REMOTE = '/media/cryptofs/apps/usr/palm/services/com.acp.bacnet.service/facility_info.db'
    tmp_path = None
    try:
        with tempfile.NamedTemporaryFile(suffix='.db', delete=False) as tf:
            tmp_path = tf.name
        scp_cmd = [
            'scp', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=5',
            f'root@{ip}:{DB_REMOTE}', tmp_path,
        ]
        r = subprocess.run(scp_cmd, capture_output=True, text=True, timeout=10)
        if r.returncode == 0:
            conn = _sqlite3.connect(tmp_path)
            conn.row_factory = _sqlite3.Row
            cur = conn.execute(
                'SELECT facility_type, facility_sub_type, name, '
                '       facility_addr, modbus_slave_id '
                'FROM facility_info ORDER BY facility_type, facility_addr, modbus_slave_id'
            )
            rows = [dict(row) for row in cur.fetchall()]
            conn.close()
            source = 'facility_info.db'
        else:
            pass  # DB 미존재 → dataManager fallback
    except Exception:
        pass  # SCP 예외 → dataManager fallback
    finally:
        if tmp_path and os.path.exists(tmp_path):
            try:
                os.unlink(tmp_path)
            except Exception:
                pass

    # ── 방법 2: SSH + luna-send → dataManager/facility/getList ──────────
    if not rows:
        try:
            cmd = "luna-send -n 1 luna://com.b2b.dataManager.service/facility/getList '{}'"
            r2 = subprocess.run(
                ['ssh', '-o', 'StrictHostKeyChecking=no', '-o', 'ConnectTimeout=5',
                 f'root@{ip}', cmd],
                capture_output=True, text=True, timeout=15
            )
            if r2.returncode == 0 and r2.stdout.strip():
                data = _json.loads(r2.stdout.strip())
                for f in data.get('facilities', []):
                    ext    = f.get('extendedInfo') or {}
                    tms    = ext.get('tms') or {}
                    modbus = ext.get('modbus') or {}
                    lgap   = ext.get('lgap') or {}
                    # facility_addr / slaveId 가 0인 경우 `0 or -1 = -1` 버그 방지
                    # — None 체크로 직접 판별
                    fa  = tms.get('facility_addr')
                    sid = modbus.get('slaveId')
                    # facilitySubType 우선, 없으면 extendedInfo.lgap.sub_type 폴백
                    # (odu_unit 타입은 facilitySubType이 비어있고 lgap.sub_type에만 존재)
                    sub_type = (f.get('facilitySubType') or '').strip() \
                               or (lgap.get('sub_type') or '').strip()
                    rows.append({
                        'facility_type':     (f.get('facilityType') or '').lower(),
                        'facility_sub_type': sub_type,
                        'name':              f.get('name') or '',
                        'facility_addr':     int(fa)  if fa  is not None else -1,
                        'modbus_slave_id':   int(sid) if sid is not None else -1,
                    })
                source = 'dataManager'
            # returncode != 0 이면 rows가 비어있어 아래 오류 응답으로 처리됨
        except subprocess.TimeoutExpired:
            return jsonify({'success': False, 'error': 'SSH 연결 시간 초과'}), 500
        except Exception:
            pass

    # ── 방법 3: BACnet WhoIs + description 읽기 (SSH 불가 환경 — Windows 등) ──
    # SSH/SCP가 없는 PC에서도 BACnet 연결만 되면 장치 목록을 구성한다.
    # Device 오브젝트의 description 프로퍼티: "facilityType:addr[:slaveId]" 형식
    # (bacnet_init_object.c 에서 설정)
    # 없거나 읽기 실패 시 엑셀 파일 기반 수동 입력 안내로 graceful 처리
    if not rows and BACNET_OK and state.get('bacnet') and state.get('device_ip'):
        log.info('[facility] SSH 실패 → BACnet description fallback 시도')
        try:
            dev_id   = state.get('device_id', 4194302)
            desc_val, desc_err = bacnet_read(ip, 'device', dev_id, 'description')
            if desc_err == 'OK' and desc_val:
                # "idu:0,idu:1,...odu:0,vent:0" 형식 파싱 시도
                for token in str(desc_val).split(','):
                    token = token.strip()
                    parts = token.split(':')
                    if len(parts) >= 2:
                        ftype = parts[0].lower()
                        try:
                            addr = int(parts[1])
                            sid  = int(parts[2]) if len(parts) >= 3 else -1
                        except ValueError:
                            continue
                        rows.append({
                            'facility_type':     ftype,
                            'facility_sub_type': '',
                            'name':              '',
                            'facility_addr':     addr,
                            'modbus_slave_id':   sid,
                        })
                if rows:
                    source = 'bacnet_description'
                    log.info('[facility] BACnet description fallback: %d 장치', len(rows))
        except Exception as _e:
            log.warning('[facility] BACnet description fallback 실패: %s', _e)

    # ── 방법 4: SSH 없이 엑셀 파싱 결과로 장치 유형/번호 범위 추정 ──────────
    # 엑셀이 이미 로드된 경우 points 목록에서 (device_type, device_b2) 집합을 읽어
    # 장치 정보를 근사적으로 구성한다. 실제 facility 데이터는 아니므로 source 표시.
    if not rows and state.get('points'):
        log.info('[facility] 엑셀 points fallback 시도')
        from collections import defaultdict as _dd
        _pt_groups = _dd(list)
        for p in state['points']:
            dt = p.get('device_type', '')
            b2 = p.get('device_b2', 0)
            if dt and dt != 'EXPIO':
                _pt_groups[dt].append(b2)
            elif dt == 'EXPIO':
                sid = p.get('device_b2', -1)
                if sid >= 0:
                    _pt_groups['EXPIO'].append(sid)
        if _pt_groups:
            # _TYPE_MAP 역방향: device_type → 임시 facility_type
            _INV_MAP = {'Aircon': 'idu', 'ODU': 'outdoor', 'Vent': 'vent',
                        'AHU': 'ahu', 'AWHP': 'awhp', 'General': 'general', 'CDU': 'cdu',
                        'EXPIO': 'expio', 'Chiller': 'chiller', 'Aircare': 'aircare'}
            for dt, addrs in _pt_groups.items():
                for a in set(addrs):
                    rows.append({
                        'facility_type':     _INV_MAP.get(dt, dt.lower()),
                        'facility_sub_type': '',
                        'name':              '',
                        'facility_addr':     a if dt != 'EXPIO' else -1,
                        'modbus_slave_id':   a if dt == 'EXPIO' else -1,
                    })
            source = 'excel_points'
            log.info('[facility] 엑셀 points fallback: %d 항목', len(rows))

    if not rows:
        return jsonify({
            'success': False,
            'error':   'facility 정보를 가져올 수 없습니다\n'
                       '(SSH 없음/DB 미존재 — 장치 번호를 수동으로 입력하세요)',
        }), 500

    # ── 검증 대상 외 장치 제외 ───────────────────────────────────────
    #   - Chiller: mchiller-air2 서브타입만 검증
    #   - AHU: doas*** 서브타입 제외
    def _include_row(row):
        ftype    = (row.get('facility_type') or '').lower()
        dev_type = _TYPE_MAP.get(ftype, '')
        sub      = (row.get('facility_sub_type') or '').lower()
        if dev_type == 'Chiller' and sub != 'mchiller-air2':
            return False
        if dev_type == 'AHU' and sub.startswith('doas'):
            return False
        return True
    rows = [r for r in rows if _include_row(r)]

    # ── 장치 유형별 번호 범위 계산 ────────────────────────────────────
    from collections import defaultdict
    groups = defaultdict(list)
    for row in rows:
        ftype    = (row.get('facility_type') or '').lower()
        dev_type = _TYPE_MAP.get(ftype)
        if dev_type is None:
            continue
        if dev_type == 'EXPIO':
            sid = row.get('modbus_slave_id', -1)
            if sid >= 0:
                groups[dev_type].append(sid)
        else:
            addr = row.get('facility_addr', -1)
            if addr >= 0:
                groups[dev_type].append(addr)

    devices = {}
    for dev_type, addrs in groups.items():
        addrs_sorted = sorted(set(addrs))
        devices[dev_type] = {
            'min':   addrs_sorted[0],
            'max':   addrs_sorted[-1],
            'count': len(addrs_sorted),
            'addrs': addrs_sorted,
        }

    # 상세 목록: 검증 툴 device_type 붙여서 반환
    detail_rows = []
    for row in rows:
        ftype    = (row.get('facility_type') or '').lower()
        dev_type = _TYPE_MAP.get(ftype, '')
        detail_rows.append({
            'dev_type':         dev_type,
            'facility_type':    row.get('facility_type', ''),
            'facility_sub_type':row.get('facility_sub_type', ''),
            'name':             row.get('name', ''),
            'facility_addr':    row.get('facility_addr', -1),
            'modbus_slave_id':  row.get('modbus_slave_id', -1),
        })

    # ODU b2(facility_addr) → facilitySubType 매핑 저장 (검증 시 서브타입별 기대값 선택용)
    odu_subtypes: dict = {}
    for row in rows:
        ftype    = (row.get('facility_type') or '').lower()
        dev_type = _TYPE_MAP.get(ftype, '')
        if dev_type == 'ODU':
            addr    = row.get('facility_addr', -1)
            subtype = (row.get('facility_sub_type') or '').strip()
            if addr >= 0 and subtype:
                odu_subtypes[addr] = subtype
    if odu_subtypes:
        state['odu_subtypes'] = odu_subtypes
        log.info('[ODU] facility_sub_type 매핑 저장: %s', odu_subtypes)

    return jsonify({'success': True, 'devices': devices,
                    'total': len(rows), 'source': source,
                    'rows': detail_rows})


@app.route('/api/disconnect', methods=['POST'])
def api_disconnect():
    async def _disconnect():
        if state['bacnet']:
            try:
                await state['bacnet'].disconnect()
            except Exception:
                pass

    try:
        aio_run(_disconnect(), timeout=5)
    except Exception:
        pass
    state['bacnet'] = None
    state['connected'] = False
    state['_last_bac0_ip_arg'] = ''   # 다음 연결 시 재초기화 강제
    state['_last_bac0_port']   = -1
    return jsonify({'success': True})


@app.route('/api/run-tests', methods=['POST'])
def api_run_tests():
    if state['running'] or state.get('all_running'):
        return jsonify({'success': False, 'error': '이미 검증 중입니다'}), 400
    if not state['points']:
        return jsonify({'success': False, 'error': '관제점 목록을 먼저 불러오세요'}), 400

    data = request.json or {}
    settings = {
        'test_read':  data.get('test_read', True),
        'test_write': data.get('test_write', True),
        'test_props': data.get('test_props', True),
    }

    # 검증 시작 시 새 로그 파일 생성
    log_filename = _start_log_file('btl_full')
    log.info(f'전체 검증 시작 — 장치: {state["device_ip"]} (Device {state["device_id"]})')
    log.info(f'관제점 수: {len(state["points"])}, 로그 파일: logs/{log_filename}')

    # SSE 큐 초기화 (신규 연결이 이전 이벤트를 받지 않도록)
    state['sse_queues'] = []

    state['results'] = []
    state['log_file'] = log_filename
    t = threading.Thread(
        target=run_tests_thread,
        args=(state['points'], settings),
        daemon=True)
    t.start()
    return jsonify({'success': True, 'log_file': log_filename})


@app.route('/api/stop-tests', methods=['POST'])
def api_stop_tests():
    state['running']     = False
    state['all_running'] = False   # All 버튼 병렬 검증도 중지
    return jsonify({'success': True})


@app.route('/api/run-tests-all', methods=['POST'])
def api_run_tests_all():
    """All 버튼 전용 병렬 검증 API — 기존 /api/run-tests 와 완전 분리.

    요청 본문:
      devices     : [{device_type, b2_from, b2_to}, ...]
      test_read   : bool (기본 True)
      test_write  : bool (기본 True)
      test_props  : bool (기본 True)
      max_workers : int  (기본 4, 최소 1, 최대 16)
    """
    if state['running'] or state.get('all_running'):
        return jsonify({'success': False, 'error': '이미 검증 실행 중입니다'}), 400
    if not state['points']:
        return jsonify({'success': False, 'error': '관제점 목록을 먼저 불러오세요'}), 400

    data         = request.json or {}
    devices_conf = data.get('devices', [])
    if not devices_conf:
        return jsonify({'success': False, 'error': '장치를 선택하세요'}), 400

    max_workers = max(1, min(16, int(data.get('max_workers', 4))))
    settings = {
        'test_read':  data.get('test_read',  True),
        'test_write': data.get('test_write', True),
        'test_props': data.get('test_props', True),
    }

    total_units = sum(
        max(0, int(d.get('b2_to', d.get('b2_from', 0)))) -
        int(d.get('b2_from', 0)) + 1
        for d in devices_conf
    )
    log_filename = _start_log_file('btl_all')
    log.info('[All] 검증 시작 — 장치: %s, 동시 유닛: %d, max_workers: %d',
             [(d['device_type'], d.get('b2_from', 0), d.get('b2_to', 0))
              for d in devices_conf],
             total_units, max_workers)

    state['sse_queues'] = []
    state['results']    = []
    state['log_file']   = log_filename

    t = threading.Thread(
        target=run_tests_all_thread,
        args=(devices_conf, settings, max_workers),
        daemon=True)
    t.start()
    return jsonify({'success': True, 'log_file': log_filename})


@app.route('/api/test-single', methods=['POST'])
def api_test_single():
    """단일 관제점 즉시 검증 (동기 응답)"""
    if state['running']:
        return jsonify({'success': False, 'error': '전체 검증 실행 중에는 개별 검증 불가'}), 400

    data = request.json or {}
    instance = int(data.get('instance', -1))
    settings = {
        'test_read':  data.get('test_read', True),
        'test_write': data.get('test_write', True),
        'test_props': data.get('test_props', True),
    }

    point = next((p for p in state['points'] if p['instance'] == instance), None)
    if point is None:
        return jsonify({'success': False,
                        'error': f'인스턴스 {instance}를 관제점 목록에서 찾을 수 없습니다.'}), 404

    try:
        # 개별 검증도 로그 파일에 기록 (FileHandler 기존 없으면 새로 생성)
        if not any(isinstance(h, logging.FileHandler)
                   for h in logging.getLogger().handlers):
            lf = _start_log_file('btl_single')
            state['log_file'] = lf
        # ctrl_ranges 분기: 개별 검증 경로에도 description 직접 읽기로 av_min/max 확정
        point = _apply_ctrl_ranges(point, state['device_ip'])
        # ctrl_skip: 개별 검증에서도 control_method_skip 장치이면 SKIP 반환
        ctrl_skip = point.get('control_method_skip', '')
        if ctrl_skip:
            detected_ctrl = point.get('device_ctrl_method', '')
            if not detected_ctrl:
                obj_long = OBJ_TYPE_MAP.get(point['object_type'], point['object_type'].lower())
                val, err = bacnet_read(state['device_ip'], obj_long, instance, 'description')
                if err == 'OK' and str(val).startswith('ctrl:'):
                    detected_ctrl = str(val).strip()[5:]
                    point = dict(point, device_ctrl_method=detected_ctrl)
            if detected_ctrl == ctrl_skip:
                skip_result = {
                    'idx':           point['idx'],
                    'instance':      instance,
                    'name':          point['name'],
                    'object_type':   point['object_type'],
                    'read_result':   '–',
                    'read_value':    '',
                    'write_result':  '–',
                    'write_detail':  '',
                    'prop_result':   '–',
                    'missing_props': [],
                    'overall':       'SKIP',
                    'error':         f'control_method 제외: {ctrl_skip} 장치 (awhpWtCtrl 전용 제어)',
                    'condition':     point.get('condition', ''),
                    'expected_value': '',
                    'value_match':    '',
                }
                log.info(f"개별 검증: {point['object_type']}:{instance} {point['name']} → SKIP (ctrl_skip={ctrl_skip})")
                return jsonify({'success': True, 'result': skip_result, 'point': point})
        result = _test_one_point(point, settings, state['device_ip'])
        log.info(f"개별 검증: {point['object_type']}:{instance} {point['name']} → {result['overall']}")

        # update_result=True 이면 state['results']에 결과를 반영 (Tab4 재검증 용)
        if data.get('update_result', False) and state['results']:
            for i, r in enumerate(state['results']):
                if r.get('instance') == instance:
                    state['results'][i] = result
                    log.info(f"결과 업데이트: inst={instance} {result['overall']}")
                    break

        return jsonify({'success': True, 'result': result, 'point': point})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/events')
def api_events():
    """SSE 실시간 이벤트 스트림 — 연결별 큐로 이벤트 손실 방지"""
    my_q = Queue()
    state['sse_queues'].append(my_q)
    def generate():
        try:
            while True:
                try:
                    event = my_q.get(timeout=20)
                    try:
                        data = json.dumps(event, ensure_ascii=False)
                    except Exception as e:
                        log.warning('SSE 이벤트 직렬화 실패, 스킵: %s', e)
                        if event.get('type') == 'complete':
                            break
                        continue
                    yield f"data: {data}\n\n"
                    if event.get('type') == 'complete':
                        break
                except Empty:
                    yield f"data: {json.dumps({'type': 'heartbeat'})}\n\n"
        finally:
            try:
                state['sse_queues'].remove(my_q)
            except ValueError:
                pass

    return Response(generate(), mimetype='text/event-stream',
                    headers={'Cache-Control': 'no-cache',
                             'X-Accel-Buffering': 'no'})


@app.route('/api/results')
def api_results():
    return jsonify({'results': state['results'], 'running': state['running']})


@app.route('/api/run-cov-test', methods=['POST'])
def api_run_cov_test():
    """DS-COV-B 검증 실행 (동기 — 완료 후 결과 반환)"""
    if state['running']:
        return jsonify({'success': False, 'error': '다른 검증이 실행 중입니다'}), 409
    if not state.get('points'):
        return jsonify({'success': False, 'error': '관제점 목록이 없습니다. 먼저 엑셀을 로드하세요'}), 400
    if not state.get('connected') or state['bacnet'] is None:
        return jsonify({'success': False, 'error': 'BACnet 미연결'}), 400

    settings = request.json or {}
    try:
        cov_results = _run_cov_tests(state['points'], settings)
        pass_cnt = sum(1 for r in cov_results if r['overall'] == 'PASS')
        fail_cnt = sum(1 for r in cov_results if r['overall'] == 'FAIL')
        skip_cnt = sum(1 for r in cov_results if r['overall'] == 'SKIP')
        if fail_cnt:
            fail_names = [r['name'] for r in cov_results if r['overall'] == 'FAIL']
            log.warning('[COV] 검증 완료: PASS=%d FAIL=%d SKIP=%d  ← FAIL 항목: %s',
                        pass_cnt, fail_cnt, skip_cnt, fail_names)
        else:
            log.info('[COV] 검증 완료: PASS=%d FAIL=%d SKIP=%d', pass_cnt, fail_cnt, skip_cnt)
        return jsonify({
            'success':  True,
            'results':  cov_results,
            'pass':     pass_cnt,
            'fail':     fail_cnt,
            'skip':     skip_cnt,
        })
    except Exception as e:
        log.exception('[COV] 검증 오류')
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/generate-report', methods=['POST'])
def api_generate_report():
    if not state['results']:
        return jsonify({'success': False, 'error': '검증 결과가 없습니다'}), 400
    try:
        filepath, filename = generate_report()
        state['report_file'] = filename
        return jsonify({'success': True, 'filename': filename})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/download/<filename>')
def api_download(filename):
    # 경로 탐색 공격 방지: 파일 이름만 허용
    if '/' in filename or '..' in filename:
        return jsonify({'error': '잘못된 파일 이름'}), 400
    filepath = _get_data_dir() / filename
    if not filepath.exists():
        return jsonify({'error': '파일 없음'}), 404
    return send_file(str(filepath), as_attachment=True, download_name=filename)


# ═══════════════════════════════════════════════════════
#  FD (Foreign Device) 검증 API
# ═══════════════════════════════════════════════════════

@app.route('/api/fd-run-tests', methods=['POST'])
def api_fd_run_tests():
    """
    FD 검증 실행 (백그라운드 스레드).

    DUT는 실제 상품 설정 그대로(Foreign_Device=1, Fd_Bbmd_Ip=검증PC IP) 실행 중이어야 한다.
    DUT는 재시도 주기(기본 30초)마다 Register-FD를 자동 전송하므로 툴이 포트를 열면 자동 수신된다.

    처리 순서:
      1) BAC0 자동 disconnect (포트 47808 반납)
      2) 포트 47808 수신 대기 → DUT Register-FD 자동 수신
      3) BVLC-Result(Success) 응답 → WhoIs/I-Am 검증

    요청: { "listen_port": 47808, "wait_timeout": 60, "run_whois": true }
    """
    if state['fd_running']:
        return jsonify({'success': False, 'error': 'FD 검증이 이미 진행 중입니다'}), 409

    data         = request.get_json(silent=True) or {}
    listen_port  = int(data.get('listen_port',  47808))   # BBMD 포트 (BACnet 표준: 47808)
    wait_timeout = float(data.get('wait_timeout', 60.0))
    run_whois    = bool(data.get('run_whois', True))

    # ── FD 검증 전 BAC0 disconnect → 47808 포트 해제 ──────────────────────
    # BAC0(47808) 소켓이 완전히 해제될 때까지 대기 후 바인드.
    # listen_port == 47809인 경우에도 BAC0 혹은 이전 실행 잔류 소켓이
    # 해당 포트를 점유할 수 있으므로 disconnect를 시도한다.
    async def _do_disconnect():
        if state['bacnet']:
            try:
                await state['bacnet'].disconnect()
            except Exception:
                pass

    try:
        aio_run(_do_disconnect(), timeout=5)
    except Exception:
        pass
    state['bacnet'] = None
    state['connected'] = False
    state['_last_bac0_ip_arg'] = ''
    state['_last_bac0_port']   = -1

    # BAC0 소켓이 OS에서 완전히 해제될 때까지 대기
    import time as _time
    _time.sleep(1.5)

    def _progress(msg):
        state['fd_progress'] = msg
        log.info('[FD] %s', msg)

    def _run():
        state['fd_running']  = True
        state['fd_results']  = None
        state['fd_progress'] = 'FD 검증 시작 중...'
        try:
            _progress(f'포트 {listen_port} 수신 대기 중 — DUT가 자동으로 Register-FD를 전송합니다 (최대 {wait_timeout:.0f}초)...')
            result = run_fd_tests(
                listen_ip='0.0.0.0',
                listen_port=listen_port,     # 47809: FD 전용 (BBMD 역할)
                dut_port=47808,              # 47808: DUT BACnet 포트 (WhoIs 대상)
                wait_timeout=wait_timeout,
                run_whois=run_whois,
                progress_cb=_progress,
            )
            state['fd_results']  = result
            state['fd_progress'] = '검증 완료'
            log.info('[FD] 검증 완료: %s', result['summary'])
        except Exception as e:
            log.exception('[FD] 검증 오류')
            state['fd_results'] = {
                'error': str(e), 'tests': [], 'summary': {},
                'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
                'fd_registered': False, 'dut_ip': None,
            }
            state['fd_progress'] = f'오류: {e}'
        finally:
            state['fd_running'] = False

    t = threading.Thread(target=_run, daemon=True, name='fd-test')
    t.start()
    return jsonify({'success': True, 'message': 'FD 검증 시작됨'})


@app.route('/api/fd-status', methods=['GET'])
def api_fd_status():
    """FD 검증 진행 상태 및 결과 폴링."""
    return jsonify({
        'running':  state['fd_running'],
        'results':  state['fd_results'],
        'report':   state['fd_report'],
        'progress': state['fd_progress'],
    })


@app.route('/api/fd-generate-report', methods=['POST'])
def api_fd_generate_report():
    """FD 검증 결과 엑셀 리포트 생성."""
    if not state['fd_results']:
        return jsonify({'success': False, 'error': 'FD 검증 결과가 없습니다'}), 400
    if state['fd_results'].get('error'):
        return jsonify({'success': False,
                        'error': state['fd_results']['error']}), 400
    try:
        filepath, filename = generate_fd_report(
            state['fd_results'],
            output_dir=str(_get_data_dir()),
        )
        state['fd_report'] = filename
        return jsonify({'success': True, 'filename': filename})
    except Exception as e:
        log.exception('[FD] 리포트 생성 오류')
        return jsonify({'success': False, 'error': str(e)}), 500


# ═══════════════════════════════════════════════════════
#  BBMD 검증 API
# ═══════════════════════════════════════════════════════

@app.route('/api/bbmd-check', methods=['POST'])
def api_bbmd_check():
    """
    BBMD 기능 지원 여부 빠른 확인 (Read-BDT 단일 테스트).
    요청: { "bbmd_ip": "x.x.x.x", "bbmd_port": 47808 }
    """
    data      = request.get_json(silent=True) or {}
    bbmd_ip   = data.get('bbmd_ip', '').strip()
    bbmd_port = int(data.get('bbmd_port', 47808))

    if not bbmd_ip:
        return jsonify({'success': False, 'error': 'bbmd_ip 필수'}), 400

    try:
        import socket
        socket.inet_aton(bbmd_ip)
    except Exception:
        return jsonify({'success': False, 'error': f'유효하지 않은 IP: {bbmd_ip}'}), 400

    try:
        from bbmd_check import test_read_bdt, _get_local_ip, _pack_bvlc, BVLC_READ_BDT
        import socket as _sock

        local_ip = _get_local_ip(bbmd_ip)
        sock = _sock.socket(_sock.AF_INET, _sock.SOCK_DGRAM)
        sock.setsockopt(_sock.SOL_SOCKET, _sock.SO_REUSEADDR, 1)
        try:
            sock.bind(('', 47808))
        except OSError:
            sock.bind(('', 0))
        try:
            result = test_read_bdt(sock, bbmd_ip, bbmd_port, timeout=5.0)
        finally:
            sock.close()

        supported = (result['status'] == 'PASS')
        return jsonify({
            'success':       True,
            'bbmd_supported': supported,
            'status':        result['status'],
            'detail':        result['detail'],
            'duration_ms':   result['duration_ms'],
            'sent_hex':      result['sent_hex'],
            'received_hex':  result['received_hex'],
            'local_ip':      local_ip,
        })
    except Exception as e:
        log.exception('[BBMD-check] 오류')
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/bbmd-run-tests', methods=['POST'])
def api_bbmd_run_tests():
    """
    BBMD 전체 검증 실행 (백그라운드 스레드).
    요청: { "bbmd_ip": "x.x.x.x", "bbmd_port": 47808,
             "device_id": 4194302, "run_whois": true, "fd_ttl": 60 }
    """
    if state['bbmd_running']:
        return jsonify({'success': False, 'error': 'BBMD 검증이 이미 진행 중입니다'}), 409

    data      = request.get_json(silent=True) or {}
    bbmd_ip   = data.get('bbmd_ip', state.get('device_ip', '')).strip()
    bbmd_port = int(data.get('bbmd_port', state.get('bacnet_port', 47808)))
    device_id = int(data.get('device_id', state.get('device_id', 4194302)))
    run_whois = bool(data.get('run_whois', True))
    fd_ttl    = int(data.get('fd_ttl', 60))

    if not bbmd_ip:
        return jsonify({'success': False, 'error': 'bbmd_ip 필수'}), 400

    def _run():
        state['bbmd_running'] = True
        state['bbmd_results'] = None
        try:
            result = run_bbmd_tests(
                bbmd_ip=bbmd_ip,
                bbmd_port=bbmd_port,
                device_id=device_id,
                timeout=5.0,
                run_whois=run_whois,
                fd_ttl=fd_ttl,
            )
            state['bbmd_results'] = result
            log.info('[BBMD] 검증 완료: %s', result['summary'])
        except Exception as e:
            log.exception('[BBMD] 검증 오류')
            state['bbmd_results'] = {
                'error': str(e), 'tests': [], 'summary': {},
                'bbmd_ip': bbmd_ip, 'timestamp': datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
            }
        finally:
            state['bbmd_running'] = False

    t = threading.Thread(target=_run, daemon=True, name='bbmd-test')
    t.start()
    return jsonify({'success': True, 'message': 'BBMD 검증 시작됨'})


@app.route('/api/bbmd-status', methods=['GET'])
def api_bbmd_status():
    """BBMD 검증 진행 상태 및 결과 폴링."""
    return jsonify({
        'running': state['bbmd_running'],
        'results': state['bbmd_results'],
        'report':  state['bbmd_report'],
    })


@app.route('/api/bbmd-generate-report', methods=['POST'])
def api_bbmd_generate_report():
    """BBMD 검증 결과 엑셀 리포트 생성."""
    if not state['bbmd_results']:
        return jsonify({'success': False, 'error': 'BBMD 검증 결과가 없습니다'}), 400
    if state['bbmd_results'].get('error'):
        return jsonify({'success': False,
                        'error': state['bbmd_results']['error']}), 400
    try:
        filepath, filename = generate_bbmd_report(
            state['bbmd_results'],
            output_dir=str(_get_data_dir()),
        )
        state['bbmd_report'] = filename
        return jsonify({'success': True, 'filename': filename})
    except Exception as e:
        log.exception('[BBMD] 리포트 생성 오류')
        return jsonify({'success': False, 'error': str(e)}), 500


@app.route('/api/debug-read', methods=['POST'])
def api_debug_read():
    """임시 디버그 엔드포인트: 임의 BACnet 인스턴스 present_value 직접 읽기.
    요청: {"instances": [[inst_num, "objType"], ...]}
    응답: {"results": [{"inst": N, "val": V, "status": "OK/ERR"}]}
    """
    data = request.get_json(silent=True) or {}
    items = data.get('instances', [])
    addr = state.get('device_ip', '')
    results = []
    obj_map = {'analogInput': 'analogInput', 'binaryInput': 'binaryInput',
               'analogValue': 'analogValue', 'binaryValue': 'binaryValue',
               'multiStateInput': 'multiStateInput', 'multiStateValue': 'multiStateValue'}
    for inst_num, obj_type in items:
        obj_long = obj_map.get(obj_type, obj_type)
        val, err = bacnet_read(addr, obj_long, inst_num, 'presentValue')
        if err == 'OK':
            try:
                fval = float(str(val).split()[0])
            except Exception:
                fval = None
            results.append({'inst': inst_num, 'obj': obj_type, 'val': fval, 'raw': str(val), 'status': 'OK'})
        else:
            results.append({'inst': inst_num, 'obj': obj_type, 'val': None, 'raw': '', 'status': err})
    return jsonify({'success': True, 'results': results})


# ═══════════════════════════════════════════════════════
#  진입점
# ═══════════════════════════════════════════════════════
if __name__ == '__main__':
    import socket
    import argparse

    # ── 커맨드라인 인수 파싱 ─────────────────────────────────
    parser = argparse.ArgumentParser(description='BACnet BTL 검증 자동화 툴')
    parser.add_argument('--port', type=int, default=5000,
                        help='Flask 서버 포트 (기본값: 5000)')
    parser.add_argument('--bacnet-port', type=int, default=47808,
                        help='BACnet UDP 포트 (기본값: 47808)')
    args = parser.parse_args()

    # ── 빈 포트 자동 탐색 ────────────────────────────────────
    def _find_free_port(start: int, max_try: int = 20) -> int:
        """start 포트부터 순서대로 바인딩 가능한 포트 반환"""
        for port in range(start, start + max_try):
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                try:
                    s.bind(('0.0.0.0', port))
                    return port
                except OSError:
                    continue
        raise RuntimeError(
            f'포트 {start}~{start + max_try - 1} 범위가 모두 사용 중입니다.')

    def _find_free_udp_port(start: int, max_try: int = 20) -> int:
        """BACnet UDP 포트 자동 탐색"""
        for port in range(start, start + max_try):
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
                s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                try:
                    s.bind(('0.0.0.0', port))
                    return port
                except OSError:
                    continue
        raise RuntimeError(
            f'BACnet UDP 포트 {start}~{start + max_try - 1} 범위가 모두 사용 중입니다.')

    # ── 서버 IP 감지 ─────────────────────────────────────────
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(('8.8.8.8', 80))
        server_ip = s.getsockname()[0]
        s.close()
    except Exception:
        server_ip = '0.0.0.0'

    # ── 포트 결정 ─────────────────────────────────────────────
    try:
        run_port = _find_free_port(args.port)
    except RuntimeError as e:
        print(f'❌  {e}')
        sys.exit(1)

    # BACnet UDP 포트 결정 (충돌 시 자동 탐색)
    try:
        bacnet_run_port = _find_free_udp_port(args.bacnet_port)
    except RuntimeError as e:
        print(f'❌  {e}')
        sys.exit(1)
    state['bacnet_port'] = bacnet_run_port

    if run_port != args.port:
        print(f'  ⚠  포트 {args.port} 사용 중 → 포트 {run_port} 로 자동 전환')
    if bacnet_run_port != args.bacnet_port:
        print(f'  ⚠  BACnet UDP {args.bacnet_port} 사용 중 → {bacnet_run_port} 로 자동 전환')

    print('\n' + '=' * 60)
    print('  BACnet BTL 검증 자동화 툴  v1.0')
    print('  대상: Aircon_List (에어컨 관제점)')
    print(f'  접속 URL → http://{server_ip}:{run_port}')
    print(f'  접속 URL → http://127.0.0.1:{run_port}')
    print(f'  BACnet UDP 포트: {bacnet_run_port}')
    print(f'  (같은 네트워크의 PC 브라우저에서 위 주소로 접속)')
    print('=' * 60)

    if not XLRD_OK:
        print("  ⚠  xlrd 없음 → pip install 'xlrd==1.2.0'")
    if not BACNET_OK:
        print("  ⚠  BAC0 없음 → Demo 모드로 동작 (pip install BAC0)")
    if not OPENPYXL_OK:
        print("  ⚠  openpyxl 없음 → pip install openpyxl")
    print()

    # ── exe 단독 실행 시 브라우저 자동 오픈 (2초 후 기본 브라우저로 열기) ──
    if getattr(sys, 'frozen', False):
        import webbrowser
        def _open_browser_delayed():
            time.sleep(2.0)
            webbrowser.open(f'http://127.0.0.1:{run_port}')
        threading.Thread(target=_open_browser_delayed, daemon=True).start()
        print(f'  자동으로 브라우저가 열립니다 → http://127.0.0.1:{run_port}')
        print()

    app.run(host='0.0.0.0', port=run_port, debug=False, use_reloader=False)
