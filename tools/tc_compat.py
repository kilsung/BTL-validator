#!/usr/bin/env python3
"""
Windows 호스트 호환 부트스트랩 — verification/scripts 를 수정하지 않고 실행하기 위한 shim

BACnet_Common(verification/scripts)은 Linux/Docker 실행을 전제로 하므로
Windows 호스트 Python(live 모드, TC 목록, SSH 점검)에서 다음 문제가 발생한다.

  1. modbus_rtu_slave.py 가 termios/tty/fcntl 을 모듈 최상단에서 import
     → tc_environment / tc_cases import 단계에서 ModuleNotFoundError
     (시리얼 포트는 managed 모드(WSL)에서만 사용하므로 Windows 에서는 stub 으로 대체)
  2. run_gateway_tcs._restore_result_ownership 이 os.geteuid() 를 무조건 호출
     → 결과 저장 직후 AttributeError (Windows 는 소유권 복원 대상 아님 → -1 반환)
  3. tc_cases._run_remote_ssh_command 의 Windows 경로(paramiko) 제거 및 /dev/null 고정
     → Windows OpenSSH 에서 '-F /dev/null' 실패, 암호 인증 불가
     (암호 지정 시 paramiko, 키 인증 시 os.devnull 을 사용하는 구현으로 교체)

사용: python -X utf8 -u -c <BOOTSTRAP> <run_gateway_tcs.py> [args...]
     SSH 점검 등 인라인 스크립트는 SHIM + 본문 을 -c 로 실행 (sys.argv[1] = scripts 디렉터리)
PyInstaller exe 에서도 외부 Python 으로 실행되므로 파일이 아닌 소스 문자열로 전달한다.
"""

SHIM = r'''
import os as _os, sys as _sys, types as _types


def _btl_compat(scripts_dir):
    if scripts_dir and scripts_dir not in _sys.path:
        _sys.path.insert(0, scripts_dir)
    if _sys.platform != "win32":
        return

    def _stub(name):
        mod = _types.ModuleType(name)

        def __getattr__(attr):
            if attr.startswith("__"):
                raise AttributeError(attr)
            if attr.isupper() or (attr[:1] == "B" and attr[1:].isdigit()):
                return 0          # termios.B9600, fcntl.F_GETFL 등 상수

            def _unsupported(*_a, **_k):
                raise OSError(f"{name}.{attr} 는 Windows 에서 지원되지 않습니다 (managed 모드는 WSL 사용)")
            return _unsupported

        mod.__getattr__ = __getattr__
        return mod

    try:
        import termios  # noqa: F401
    except ImportError:
        # 표준 tty.py 는 'from termios import *' 에 의존하므로 termios 가 없으면 함께 stub 처리
        for name in ("termios", "tty", "fcntl"):
            _sys.modules[name] = _stub(name)
    if not hasattr(_os, "geteuid"):
        _os.geteuid = lambda: -1   # root 아님 → 소유권 복원 생략

    try:
        import tc_cases
    except Exception:
        return                     # 오류는 본 실행 경로에서 그대로 보고
    if hasattr(tc_cases, "_run_remote_ssh_command"):
        tc_cases._run_remote_ssh_command = _win_ssh


def _win_ssh(ctx, script, timeout=8.0):
    import subprocess
    opts = ctx.options
    if not getattr(opts, "ssh_enable", False):
        return None
    ip = getattr(opts, "target_ip", None) or getattr(opts, "ip", None)
    if not ip or ip in ("127.0.0.1", "localhost"):
        return None
    user = getattr(opts, "ssh_user", "root") or "root"
    port = int(getattr(opts, "ssh_port", 22) or 22)
    password = getattr(opts, "ssh_pass", None) or _os.environ.get("SSH_PASS")

    if password:
        client = None
        try:
            import paramiko
            client = paramiko.SSHClient()
            client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
            client.connect(hostname=ip, port=port, username=user, password=password,
                           look_for_keys=False, allow_agent=False, timeout=timeout,
                           auth_timeout=timeout, banner_timeout=timeout)
            stdin, stdout, stderr = client.exec_command(script, timeout=timeout)
            stdin.close()
            out = stdout.read().decode("utf-8", errors="replace")
            err = stderr.read().decode("utf-8", errors="replace")
            code = stdout.channel.recv_exit_status()
            if code != 0:
                ctx._ssh_last_error = err.strip() or f"exit code {code}"
            return code, out, err
        except ImportError:
            ctx._ssh_last_error = "Windows SSH 암호 인증에 paramiko>=3.4,<4 가 필요합니다 (pip install paramiko)"
            return None
        except Exception as error:
            ctx._ssh_last_error = str(error)
            return None
        finally:
            if client is not None:
                client.close()

    # Windows OpenSSH 는 '-F nul' 을 열지 못하므로 빈 임시 파일을 설정/known_hosts 로 사용
    import tempfile
    tmp = tempfile.mkdtemp(prefix="btl_ssh_")
    empty_cfg = _os.path.join(tmp, "config")
    known = _os.path.join(tmp, "known_hosts")
    for path in (empty_cfg, known):
        open(path, "w").close()
    cmd = ["ssh", "-F", empty_cfg, "-o", "BatchMode=yes", "-o", "ConnectTimeout=5",
           "-o", "StrictHostKeyChecking=no", "-o", f"UserKnownHostsFile={known}",
           "-o", "LogLevel=ERROR", "-o", "HostKeyAlgorithms=+ssh-rsa",
           "-o", "PubkeyAcceptedKeyTypes=+ssh-rsa",
           "-o", "KexAlgorithms=+diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1",
           "-p", str(port), f"{user}@{ip}", script]
    try:
        res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                             encoding="utf-8", errors="replace", timeout=timeout,
                             creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        if res.returncode != 0:
            ctx._ssh_last_error = res.stderr.strip() or f"exit code {res.returncode}"
        return res.returncode, res.stdout, res.stderr
    except Exception as error:
        ctx._ssh_last_error = str(error)
        return None
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)
'''

# sys.argv = ['-c', '<run_gateway_tcs.py>', args...]
BOOTSTRAP = SHIM + r'''
import runpy as _runpy
_script = _sys.argv[1]
_sys.argv = _sys.argv[1:]
_btl_compat(_os.path.dirname(_os.path.abspath(_script)))
_runpy.run_path(_script, run_name="__main__")
'''


def bootstrap_cmd(python: list, script) -> list:
    """[python..., -X utf8, -u, -c, BOOTSTRAP, script] — 뒤에 run_gateway_tcs.py 인자를 붙인다"""
    return list(python) + ['-X', 'utf8', '-u', '-c', BOOTSTRAP, str(script)]
