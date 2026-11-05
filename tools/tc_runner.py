#!/usr/bin/env python3
"""
TC 기반 검증 실행기 — BACnet_Common/bacnet_ip_gateway/verification 연동

verification/scripts/run_gateway_tcs.py 를 하위 프로세스로 실행하고
진행 상황(콘솔 출력)과 결과(results.json, steps.csv)를 웹 UI에 제공한다.

  - live    모드: Windows/Linux 호스트 Python으로 직접 실행 (WSL 불필요)
  - managed 모드: PTY/vport/Gateway 바이너리가 필요하므로 Windows에서는 WSL로 자동 위임

run_test.sh(Docker/WSL 대화형 스크립트)의 입력 항목은 UI 옵션으로 대체되며,
기본값은 verification/test_config.json 에서 읽는다.
"""

from __future__ import annotations

import csv
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from collections import deque
from pathlib import Path
from typing import Dict, List, Optional

IS_WINDOWS = sys.platform == 'win32'
_NO_WINDOW = getattr(subprocess, 'CREATE_NO_WINDOW', 0)

# [03/54] TC-BAC-004 ...            → 시작
# [03/54] TC-BAC-004 => PASS - msg  → 완료
_PROGRESS_RE = re.compile(r'^\[(\d+)/(\d+)\]\s+(TC-[\w-]+)\s+(?:\.\.\.|=>\s+(\w+)(?:\s+-\s+(.*))?)\s*$')
_RESULT_DIR_RE = re.compile(r'^Result directory:\s*(.+?)\s*$')

SUITES = ('quick', 'full', 'audit', 'soak', 'all')
MAX_LOG_LINES = 5000


# ═══════════════════════════════════════════════════════
#  경로 / 인터프리터
# ═══════════════════════════════════════════════════════

def _candidate_roots(tools_dir: Path) -> List[Path]:
    env = os.environ.get('BTL_TC_ROOT', '').strip()
    roots = [Path(env)] if env else []
    for base in (tools_dir, *tools_dir.parents):
        roots.append(base / 'BACnet_Common' / 'bacnet_ip_gateway' / 'verification')
        roots.append(base.parent / 'BACnet_Common' / 'bacnet_ip_gateway' / 'verification')
    if IS_WINDOWS:
        roots += [Path(f'{d}:/BACnet_Common/bacnet_ip_gateway/verification') for d in 'CDE']
    return roots


def is_verification_root(path) -> bool:
    return bool(path) and (Path(path) / 'scripts' / 'run_gateway_tcs.py').is_file()


def find_verification_root(tools_dir: Path, saved: str = '') -> Optional[Path]:
    """저장된 경로 → 환경변수 BTL_TC_ROOT → 상위 폴더의 BACnet_Common 순으로 탐색"""
    for cand in ([Path(saved)] if saved else []) + _candidate_roots(tools_dir):
        if is_verification_root(cand):
            return cand.resolve()
    return None


def host_python() -> List[str]:
    """live 모드용 Python 명령 (PyInstaller exe 에서는 PATH 의 Python 사용)"""
    if not getattr(sys, 'frozen', False):
        return [sys.executable]
    if IS_WINDOWS and shutil.which('py'):
        return ['py', '-3']
    for name in ('python3', 'python'):
        if shutil.which(name):
            return [name]
    raise RuntimeError('Python 3 를 찾을 수 없습니다 (PATH 확인)')


def to_wsl_path(path) -> str:
    """D:\\a\\b → /mnt/d/a/b (상대경로는 / 구분자로만 변환)"""
    text = str(path).replace('\\', '/')
    m = re.match(r'^([A-Za-z]):/(.*)$', text)
    if m:
        return f'/mnt/{m.group(1).lower()}/{m.group(2)}'
    return text


def from_wsl_path(path: str) -> str:
    m = re.match(r'^/mnt/([a-z])/(.*)$', path)
    if IS_WINDOWS and m:
        return f'{m.group(1).upper()}:\\' + m.group(2).replace('/', '\\')
    return path


def _wsl_sh(script: str, cwd: Path, timeout: float = 30.0) -> subprocess.CompletedProcess:
    return subprocess.run(
        ['wsl.exe', '--cd', str(cwd), '-e', 'sh', '-c', script],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout,
        creationflags=_NO_WINDOW, check=False,
    )


def _decode(raw: bytes) -> str:
    for enc in ('utf-8', 'cp949'):
        try:
            return raw.decode(enc)
        except UnicodeDecodeError:
            pass
    return raw.decode('utf-8', errors='replace')


# ═══════════════════════════════════════════════════════
#  test_config.json 기본값
# ═══════════════════════════════════════════════════════

def load_defaults(root: Path) -> Dict:
    """verification/test_config.json → UI 기본값 (run_test.sh load_config 와 동일 규칙)"""
    cfg = {}
    try:
        cfg = json.loads((root / 'test_config.json').read_text(encoding='utf-8'))
    except (OSError, ValueError):
        pass
    common = cfg.get('common', {})
    managed = cfg.get('managed', {})
    live = cfg.get('live', {})
    binaries = managed.get('binaries', {})
    return {
        'mode': cfg.get('default_mode', 'managed'),
        'suite': common.get('suite', 'quick'),
        'device_id': common.get('device_id', 1200),
        'tpi_path': common.get('tpi_path', 'model/cdu/[BTL]cdu.tpi'),
        'pics_doc': common.get('pics_docx_path', 'model/cdu/[BTL]FUNC_CHECKLIST_CDU.docx'),
        'response_time_ms': common.get('expected_response_time_ms', 200),
        'performance_policy': common.get('performance_policy', './verification/scripts/performance_policy.json'),
        'gateway_config_dir': common.get('gateway_config_dir', './conf'),
        'vport_config_path': common.get('vport_config_path', '../vport_release/vport.cfg'),
        'gateway_bin': binaries.get('gateway_bin', './bacnet_ip_gateway'),
        'vport_bin': binaries.get('vport_bin', '../vport_release/vport'),
        'vport_test_bin': binaries.get('vport_test_bin', '../vport_release/vport_test'),
        'unit_test_bin': binaries.get('unit_test_bin', './verification/unit/run_tests'),
        'bbmd_auto': managed.get('bbmd_auto', True),
        'target_ip': live.get('target_ip', ''),
        'target_port': live.get('target_port', 47808),
        'allow_live_mutation': live.get('allow_live_mutation', False),
        'bbmd_endpoint': live.get('bbmd_endpoint', ''),
        'ssh_enabled': live.get('ssh_enabled', False),
        'ssh_user': live.get('ssh_user', 'root'),
        'ssh_port': live.get('ssh_port', 22),
    }


# ═══════════════════════════════════════════════════════
#  환경 점검 / TC 목록
# ═══════════════════════════════════════════════════════

def check_environment(root: Path, opts: Dict) -> Dict:
    """선택 모드 실행에 필요한 항목 점검 → [{name, ok, detail}]"""
    gateway_root = root.parent
    items = []

    def add(name, ok, detail=''):
        items.append({'name': name, 'ok': bool(ok), 'detail': detail})

    def rel_exists(p):
        return bool(p) and (gateway_root / p).exists()

    add('verification 폴더', is_verification_root(root), str(root))
    for key, label in (('tpi_path', 'TPI (EPICS)'), ('pics_doc', 'PICS 체크리스트'),
                       ('gateway_config_dir', 'Gateway 설정 디렉터리')):
        p = opts.get(key)
        add(label, rel_exists(p), p or '(미지정)')
    policy = opts.get('performance_policy')
    if policy:
        add('성능 정책', rel_exists(policy), policy)

    if opts.get('mode') == 'live':
        try:
            py = host_python()
            probe = subprocess.run(py + ['-c', 'import bacpypes3'], capture_output=True,
                                   timeout=15, creationflags=_NO_WINDOW)
            add('Python (호스트)', True, ' '.join(py))
            add('bacpypes3 (TC-BTL-004/006)', probe.returncode == 0,
                '설치됨' if probe.returncode == 0 else 'pip install bacpypes3 필요')
        except Exception as e:
            add('Python (호스트)', False, str(e))
        add('대상 IP', bool(opts.get('target_ip')), opts.get('target_ip') or '(미입력)')
        if opts.get('ssh_enabled') and opts.get('soak'):
            ok, detail = check_ssh(root, opts)
            add('SSH 접속 (TC-AVL-004 자원 모니터링)', ok, detail)
        return {'items': items, 'runner': 'host'}

    # managed
    opts = dict(opts)
    TcRunner._resolve_vport(root, opts)
    bins = [opts.get(k) for k in ('gateway_bin', 'vport_bin', 'vport_test_bin', 'unit_test_bin')]
    if IS_WINDOWS:
        if not shutil.which('wsl.exe'):
            add('WSL', False, 'managed 모드는 WSL(Ubuntu)이 필요합니다: wsl --install')
            return {'items': items, 'runner': 'wsl'}
        checks = ['command -v python3', 'python3 -c "import bacpypes3"',
                  'command -v make', 'command -v g++'] + [f'test -x "{to_wsl_path(b)}"' for b in bins]
        script = '; '.join(f'{c} >/dev/null 2>&1 && echo OK || echo NO' for c in checks)
        try:
            res = _wsl_sh(script, gateway_root)
            flags = _decode(res.stdout).split()
        except Exception as e:
            add('WSL', False, str(e))
            return {'items': items, 'runner': 'wsl'}
        flags += ['NO'] * (len(checks) - len(flags))
        ok = [f == 'OK' for f in flags]
        add('WSL python3', ok[0], 'sudo apt install python3' if not ok[0] else '')
        add('WSL bacpypes3 (TC-BTL-004/006)', ok[1], 'pip3 install bacpypes3' if not ok[1] else '')
        add('WSL make / g++ (자동 빌드)', ok[2] and ok[3],
            '' if ok[2] and ok[3] else 'sudo apt install build-essential')
        for b, flag in zip(bins, ok[4:]):
            add(f'바이너리 {Path(b).name}', flag, b)
        return {'items': items, 'runner': 'wsl'}

    add('python3', True, sys.executable)
    for b in bins:
        add(f'바이너리 {Path(b).name}', os.access(gateway_root / b, os.X_OK), b)
    return {'items': items, 'runner': 'host'}


_SSH_CHECK_SCRIPT = r'''
import os, sys, types
sys.path.insert(0, sys.argv[1])
import tc_cases
opts = types.SimpleNamespace(ssh_enable=True, target_ip=sys.argv[2], ssh_user=sys.argv[3],
                             ssh_port=int(sys.argv[4]), ssh_pass=os.environ.get("SSH_PASS") or None)
ctx = types.SimpleNamespace(options=opts)
res = tc_cases._run_remote_ssh_command(ctx, "echo SSH_TEST_OK", timeout=10.0)
if res is None and not getattr(ctx, "_ssh_last_error", None):
    print("NG loopback/빈 대상 IP 는 SSH 를 사용하지 않습니다")
elif res and res[0] == 0 and "SSH_TEST_OK" in res[1]:
    print("OK")
else:
    err = getattr(ctx, "_ssh_last_error", None) or (res[2].strip() if res else "connection failure")
    print("NG " + " ".join(err.split())[:200])
'''


def check_ssh(root: Path, opts: Dict):
    """TC-AVL-004 와 동일한 코드 경로(tc_cases._run_remote_ssh_command)로 SSH 접속 확인"""
    ip = (opts.get('target_ip') or '').strip()
    user = opts.get('ssh_user') or 'root'
    port = str(int(opts.get('ssh_port') or 22))
    env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONDONTWRITEBYTECODE='1')
    env.pop('SSH_PASS', None)
    if opts.get('ssh_pass'):
        env['SSH_PASS'] = opts['ssh_pass']
    try:
        res = subprocess.run(host_python() + ['-X', 'utf8', '-c', _SSH_CHECK_SCRIPT,
                                              str(root / 'scripts'), ip, user, port],
                             capture_output=True, timeout=30, env=env, creationflags=_NO_WINDOW)
    except subprocess.TimeoutExpired:
        return False, f'{user}@{ip}:{port} 응답 없음 (30초)'
    out = _decode(res.stdout).strip().splitlines()
    line = out[-1] if out else _decode(res.stderr).strip()[-200:]
    auth = '암호 인증' if opts.get('ssh_pass') else '키 인증'
    if line == 'OK':
        return True, f'{user}@{ip}:{port} 접속 성공 ({auth})'
    return False, f'{user}@{ip}:{port} ({auth}) 실패: {line[3:] if line.startswith("NG ") else line}'


def list_cases(root: Path, suite: str = 'all') -> List[Dict]:
    """run_gateway_tcs.py --list → [{tc_id, srs_id, title}]"""
    cmd = host_python() + ['-X', 'utf8', str(root / 'scripts' / 'run_gateway_tcs.py'),
                           '--suite', suite, '--list']
    env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONDONTWRITEBYTECODE='1')
    res = subprocess.run(cmd, cwd=str(root.parent), capture_output=True, timeout=60,
                         env=env, creationflags=_NO_WINDOW)
    if res.returncode != 0:
        raise RuntimeError(_decode(res.stderr or res.stdout).strip()[-600:])
    cases = []
    for line in _decode(res.stdout).splitlines():
        parts = line.split('\t')
        if len(parts) == 3 and parts[0].startswith('TC-'):
            cases.append({'tc_id': parts[0], 'srs_id': parts[1], 'title': parts[2]})
    return cases


# ═══════════════════════════════════════════════════════
#  결과 로드
# ═══════════════════════════════════════════════════════

def results_root(root: Path) -> Path:
    return root / 'results'


def list_history(root: Path, limit: int = 30) -> List[Dict]:
    out = []
    base = results_root(root)
    if not base.is_dir():
        return out
    dirs = sorted((d for d in base.iterdir() if (d / 'results.json').is_file()),
                  key=lambda d: d.name, reverse=True)
    for d in dirs[:limit]:
        try:
            payload = json.loads((d / 'results.json').read_text(encoding='utf-8'))
        except (OSError, ValueError):
            continue
        meta = payload.get('metadata', {})
        out.append({
            'run_id': d.name,
            'mode': meta.get('mode'),
            'suite': meta.get('suite'),
            'target': meta.get('target'),
            'started_at': meta.get('started_at'),
            'summary': payload.get('summary', {}),
        })
    return out


def load_result(root: Path, run_id: str) -> Dict:
    if not re.fullmatch(r'[\w.-]+', run_id or ''):
        raise ValueError('잘못된 run_id')
    d = results_root(root) / run_id
    payload = json.loads((d / 'results.json').read_text(encoding='utf-8'))
    steps: Dict[str, List[Dict]] = {}
    steps_csv = d / 'steps.csv'
    if steps_csv.is_file():
        with steps_csv.open(encoding='utf-8-sig', newline='') as f:   # BOM 유무 모두 허용
            for row in csv.DictReader(f):
                steps.setdefault(row['tc_id'], []).append({
                    'step': row.get('step'), 'status': row.get('status'),
                    'label': row.get('label'), 'expected': row.get('expected'),
                    'actual': row.get('actual'), 'message': row.get('message'),
                })
    results = [{
        'tc_id': r.get('tc_id'), 'srs_id': r.get('srs_id'), 'title': r.get('title'),
        'status': r.get('status'), 'message': r.get('message'),
        'duration_sec': r.get('duration_sec'),
        'steps': steps.get(r.get('tc_id'), []),
    } for r in payload.get('results', [])]
    return {
        'run_id': run_id, 'dir': str(d),
        'metadata': {k: payload.get('metadata', {}).get(k) for k in
                     ('suite', 'mode', 'target', 'device_id', 'started_at',
                      'selected_count', 'allow_live_mutation', 'capabilities')},
        'summary': payload.get('summary', {}),
        'results': results,
    }


def generate_excel(result: Dict, out_dir: Path):
    """TC 결과 → Excel (요약 + 단계 상세)"""
    from openpyxl import Workbook
    from openpyxl.styles import Alignment, Font, PatternFill

    fills = {s: PatternFill('solid', fgColor=c) for s, c in
             (('PASS', 'C6EFCE'), ('FAIL', 'FFC7CE'), ('BLOCKED', 'FFEB9C'), ('SKIP', 'D9D9D9'))}
    head_fill = PatternFill('solid', fgColor='1F4E79')
    head_font = Font(color='FFFFFF', bold=True)

    def header(ws, cols):
        ws.append([c for c, _ in cols])
        for i, (_, w) in enumerate(cols, 1):
            cell = ws.cell(row=1, column=i)
            cell.fill, cell.font = head_fill, head_font
            ws.column_dimensions[cell.column_letter].width = w
        ws.freeze_panes = 'A2'

    wb = Workbook()
    ws = wb.active
    ws.title = '요약'
    meta, summ = result['metadata'], result['summary']
    for k, v in (('Run ID', result['run_id']), ('Mode', meta.get('mode')), ('Suite', meta.get('suite')),
                 ('Target', meta.get('target')), ('Device ID', meta.get('device_id')),
                 ('Started (UTC)', meta.get('started_at')), ('TOTAL', summ.get('total')),
                 ('PASS', summ.get('PASS')), ('FAIL', summ.get('FAIL')),
                 ('BLOCKED', summ.get('BLOCKED')), ('SKIP', summ.get('SKIP'))):
        ws.append([k, v])
    ws.column_dimensions['A'].width = 16
    ws.column_dimensions['B'].width = 40

    ws = wb.create_sheet('TC 결과')
    header(ws, (('TC ID', 14), ('SRS ID', 18), ('제목', 50), ('판정', 10), ('소요(s)', 9), ('메시지', 80)))
    for r in result['results']:
        ws.append([r['tc_id'], r['srs_id'], r['title'], r['status'], r['duration_sec'], r['message']])
        ws.cell(row=ws.max_row, column=4).fill = fills.get(r['status'], PatternFill())

    ws = wb.create_sheet('단계 상세')
    header(ws, (('TC ID', 14), ('Step', 6), ('판정', 10), ('항목', 50), ('기대값', 40), ('수신값', 40), ('메시지', 60)))
    for r in result['results']:
        for s in r['steps']:
            ws.append([r['tc_id'], s['step'], s['status'], s['label'], s['expected'], s['actual'], s['message']])
            ws.cell(row=ws.max_row, column=3).fill = fills.get(s['status'], PatternFill())
            for c in range(4, 8):
                ws.cell(row=ws.max_row, column=c).alignment = Alignment(wrap_text=True, vertical='top')

    filename = f'BTL_TC_Report_{result["run_id"]}.xlsx'
    wb.save(str(out_dir / filename))
    return filename


# ═══════════════════════════════════════════════════════
#  실행기
# ═══════════════════════════════════════════════════════

class TcRunner:
    """run_gateway_tcs.py 단일 실행 관리 (동시 1개)"""

    def __init__(self):
        self.lock = threading.Lock()
        self.proc: Optional[subprocess.Popen] = None
        self._reset()

    def _reset(self):
        self.running = False
        self.phase = ''
        self.log = deque(maxlen=MAX_LOG_LINES)
        self.log_seq = 0
        self.cases: Dict[str, Dict] = {}
        self.order: List[str] = []
        self.total = 0
        self.done = 0
        self.current = ''
        self.result_dir = ''
        self.run_id = ''
        self.exit_code = None
        self.started_at = None
        self.finished_at = None
        self.command = ''
        self.error = ''
        self.stop_requested = False

    # ── 명령 구성 ─────────────────────────────────────────────
    @staticmethod
    def build_args(opts: Dict, path_fn) -> List[str]:
        """UI 옵션 → run_gateway_tcs.py 인자 (run_test.sh RUN_ARGS 와 동일 구성)"""
        mode = opts.get('mode', 'live')
        cases = [c.strip() for c in opts.get('cases', []) if c and c.strip()]
        suite = opts.get('suite', 'quick')
        if suite not in SUITES:
            raise ValueError(f'잘못된 suite: {suite}')
        args = ['--suite', 'all' if cases and suite == 'quick' else suite,
                '--mode', mode, '--non-interactive',
                '--device-id', str(int(opts.get('device_id') or 1200)),
                '--response-time-ms', str(int(opts.get('response_time_ms') or 0))]
        for c in cases:
            args += ['--case', c]

        for key, flag in (('gateway_config_dir', '--gateway-config-dir'),
                          ('vport_config_path', '--vport-config-path'),
                          ('performance_policy', '--performance-policy'),
                          ('tpi_path', '--tpi'), ('pics_doc', '--pics-doc')):
            if opts.get(key):
                args += [flag, path_fn(opts[key])]

        if mode == 'managed':
            args += ['--ip', opts.get('target_ip_managed') or '127.0.0.1', '--port', '47808']
            for key, flag in (('gateway_bin', '--gateway-bin'), ('vport_bin', '--vport-bin'),
                              ('vport_test_bin', '--vport-test-bin'), ('unit_test_bin', '--unit-test-bin')):
                if opts.get(key):
                    args += [flag, path_fn(opts[key])]
            if opts.get('allow_build'):
                args.append('--allow-build')
        else:
            ip = (opts.get('target_ip') or '').strip()
            if not ip:
                raise ValueError('live 모드는 대상 IP가 필요합니다')
            args += ['--ip', ip, '--port', str(int(opts.get('target_port') or 47808))]
            if opts.get('allow_live_mutation'):
                args.append('--allow-live-mutation')
            if opts.get('ssh_enabled') and opts.get('soak'):
                args += ['--ssh-enable', '--ssh-user', opts.get('ssh_user') or 'root',
                         '--ssh-port', str(int(opts.get('ssh_port') or 22))]

        endpoint = (opts.get('bbmd_endpoint') or '').strip()
        args += ['--bbmd-endpoint', endpoint] if endpoint else ['--bbmd-auto']
        if opts.get('long'):
            args.append('--long')
        if opts.get('soak'):
            args += ['--soak', '--soak-hours', str(float(opts.get('soak_hours') or 12.0)),
                     '--soak-interval', str(float(opts.get('soak_interval') or 15.0))]
        return args

    def _command(self, root: Path, opts: Dict):
        script = root / 'scripts' / 'run_gateway_tcs.py'
        gateway_root = root.parent
        env = dict(os.environ, PYTHONIOENCODING='utf-8', PYTHONDONTWRITEBYTECODE='1',
                   PYTHONUNBUFFERED='1')
        if opts.get('ssh_pass'):
            env['SSH_PASS'] = opts['ssh_pass']   # 명령행/파일에 남기지 않음

        if opts.get('mode') == 'managed' and IS_WINDOWS:
            args = self.build_args(opts, to_wsl_path)
            # WSLENV: SSH_PASS 등 환경변수를 Linux 측에 전달
            env['WSLENV'] = ':'.join(filter(None, [env.get('WSLENV'), 'SSH_PASS', 'PYTHONIOENCODING',
                                                   'PYTHONDONTWRITEBYTECODE', 'PYTHONUNBUFFERED']))
            cmd = ['wsl.exe', '--cd', str(gateway_root), '-e', 'python3', '-u',
                   to_wsl_path(script)] + args
            return cmd, env, 'wsl'
        args = self.build_args(opts, str)
        cmd = host_python() + ['-X', 'utf8', '-u', str(script)] + args
        python_bin = sys.executable if not getattr(sys, 'frozen', False) else shutil.which('python')
        if python_bin:
            cmd += ['--python-bin', python_bin]
        return cmd, env, 'host'

    @staticmethod
    def _resolve_vport(root: Path, opts: Dict):
        """test_config 의 ../vport_release 가 없으면 소스 폴더 ../vport 의 바이너리를 사용"""
        gateway_root = root.parent
        alt_dir = gateway_root.parent / 'vport'
        for key in ('vport_bin', 'vport_test_bin'):
            p = opts.get(key)
            if p and not (gateway_root / p).is_file() and (
                    (alt_dir / Path(p).name).is_file() or (alt_dir / 'Makefile').is_file()):
                opts[key] = f'../vport/{Path(p).name}'

    # ── 실행 ──────────────────────────────────────────────────
    def start(self, root: Path, opts: Dict, prepare=None) -> Dict:
        """prepare: 실행 직전 호출할 콜백 (예: BAC0 해제)"""
        opts = dict(opts)
        if opts.get('mode') == 'managed':
            self._resolve_vport(root, opts)
        with self.lock:
            if self.running:
                raise RuntimeError('TC 검증이 이미 진행 중입니다')
            cmd, _, runner = self._command(root, opts)   # 인자 오류는 여기서 즉시 반환
            self._reset()
            self.running = True
            self.started_at = time.strftime('%Y-%m-%d %H:%M:%S')
            self.command = subprocess.list2cmdline(cmd)
        threading.Thread(target=self._run, args=(root, opts, prepare),
                         daemon=True, name='tc-runner').start()
        return {'command': self.command, 'runner': runner}

    def _append(self, line: str):
        self.log.append(line)
        self.log_seq += 1

    def _build_if_needed(self, root: Path, opts: Dict):
        """managed: 바이너리 누락 시 make 자동 빌드 (run_test.sh ensure_binaries 대응)"""
        gateway_root = root.parent
        es_root = gateway_root.parent
        targets = []
        if opts.get('gateway_bin'):
            targets.append((gateway_root, '', opts['gateway_bin'], 'bacnet_ip_gateway'))
        if opts.get('vport_bin'):
            vdir = (gateway_root / opts['vport_bin']).parent
            if not (vdir / 'Makefile').is_file() and (es_root / 'vport' / 'Makefile').is_file():
                vdir = es_root / 'vport'
            targets.append((vdir, 'all', opts['vport_bin'], 'vport & vport_test'))
        if opts.get('unit_test_bin'):
            targets.append((gateway_root, opts['unit_test_bin'], opts['unit_test_bin'], 'unit tests'))
        for cwd, target, binary, desc in targets:
            if self.stop_requested:
                return
            if not (cwd / 'Makefile').is_file() or (gateway_root / binary).is_file():
                continue
            if IS_WINDOWS:
                cmd = ['wsl.exe', '--cd', str(cwd), '-e', 'make'] + ([to_wsl_path(target)] if target else [])
            else:
                cmd = ['make', '-C', str(cwd)] + ([target] if target else [])
            self.phase = f'빌드: {desc}'
            self._append(f'[Auto-Build] {desc}: {subprocess.list2cmdline(cmd)}')
            self._stream(cmd, dict(os.environ), str(cwd), parse=False)

    def _stream(self, cmd, env, cwd, parse=True) -> int:
        self.proc = subprocess.Popen(
            cmd, cwd=cwd, env=env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            creationflags=_NO_WINDOW,
        )
        for raw in iter(self.proc.stdout.readline, b''):
            line = _decode(raw).rstrip('\r\n')
            self._append(line)
            if parse:
                self._parse(line)
        return self.proc.wait()

    def _parse(self, line: str):
        m = _PROGRESS_RE.match(line.strip())
        if m:
            idx, total, tc_id, status, msg = m.groups()
            self.total = int(total)
            if tc_id not in self.cases:
                self.order.append(tc_id)
                self.cases[tc_id] = {'tc_id': tc_id, 'status': 'RUNNING', 'message': ''}
            if status:
                self.cases[tc_id].update(status=status, message=msg or '')
                self.done = int(idx)
                self.current = ''
            else:
                self.current = tc_id
            return
        m = _RESULT_DIR_RE.match(line.strip())
        if m:
            self.result_dir = from_wsl_path(m.group(1))
            self.run_id = Path(self.result_dir.replace('\\', '/')).name

    def _run(self, root, opts, prepare):
        try:
            if prepare:
                self.phase = '준비'
                prepare()
            if opts.get('mode') == 'managed' and opts.get('auto_build', True):
                self._build_if_needed(root, opts)
            if self.stop_requested:
                self.error = '사용자 중지'
                return
            cmd, env, _ = self._command(root, opts)
            self.phase = 'TC 실행'
            self._append(f'$ {self.command}')
            self.exit_code = self._stream(cmd, env, str(root.parent))
            if self.stop_requested:
                self.error = '사용자 중지'
            elif self.exit_code not in (0, 1, 2) or (self.exit_code and not self.run_id):
                self.error = f'Runner 비정상 종료 (exit={self.exit_code}) — 콘솔 로그 확인'
        except Exception as e:
            self.error = str(e)
            self._append(f'[ERROR] {e}')
        finally:
            self.proc = None
            self.running = False
            self.phase = '완료'
            self.finished_at = time.strftime('%Y-%m-%d %H:%M:%S')

    def stop(self):
        self.stop_requested = True
        proc = self.proc
        if proc is None or proc.poll() is not None:
            return
        if IS_WINDOWS:
            # 하위 프로세스(bbmd_server, 검증기 등)까지 트리 종료
            subprocess.run(['taskkill', '/PID', str(proc.pid), '/T', '/F'],
                           capture_output=True, creationflags=_NO_WINDOW)
        else:
            proc.terminate()

    def status(self, since: int = 0) -> Dict:
        start = max(0, since - (self.log_seq - len(self.log)))
        lines = list(self.log)[start:] if since < self.log_seq else []
        return {
            'running': self.running,
            'phase': self.phase,
            'total': self.total,
            'done': self.done,
            'current': self.current,
            'cases': [self.cases[t] for t in self.order],
            'run_id': self.run_id,
            'result_dir': self.result_dir,
            'exit_code': self.exit_code,
            'error': self.error,
            'started_at': self.started_at,
            'finished_at': self.finished_at,
            'command': self.command,
            'log': lines,
            'log_seq': self.log_seq,
        }


runner = TcRunner()
