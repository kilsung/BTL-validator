#!/usr/bin/env python3
"""
BACnet BTL 검증 도구 공용 실행기 (Linux/Windows/macOS)

사용 예:
  python run.py
  python run.py --port 5001 --bacnet-port 47809

환경변수도 지원:
  PORT=5001 BACNET_PORT=47809 python run.py
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path


HERE = Path(__file__).resolve().parent


def _print_header() -> None:
    print("=" * 49)
    print("  BACnet BTL 검증 자동화 툴 v1.0")
    print("=" * 49)


def _run(cmd: list[str], quiet: bool = False) -> int:
    kwargs = {}
    if quiet:
        kwargs["stdout"] = subprocess.DEVNULL
        kwargs["stderr"] = subprocess.DEVNULL
    return subprocess.run(cmd, **kwargs).returncode


def _install_deps() -> None:
    print()
    print("  의존성 확인 중...")
    rc = _run([
        sys.executable,
        "-m",
        "pip",
        "install",
        "-q",
        "flask",
        "xlrd==1.2.0",
        "openpyxl",
    ])
    if rc != 0:
        print("  ❌  기본 의존성 설치 실패")
        sys.exit(1)
    print("  ✅ 기본 의존성 설치 완료")

    rc = _run([sys.executable, "-m", "pip", "install", "-q", "BAC0"], quiet=True)
    if rc == 0:
        print("  ✅ BAC0 설치 완료")
    else:
        print("  ⚠  BAC0 설치 실패 — Demo 모드로 동작합니다")


def _build_args() -> list[str]:
    import argparse

    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument("--port", type=int)
    parser.add_argument("--bacnet-port", type=int)
    cli_args, _ = parser.parse_known_args()

    port = cli_args.port
    bacnet_port = cli_args.bacnet_port

    if port is None:
        val = os.environ.get("PORT", "").strip()
        if val:
            try:
                port = int(val)
            except ValueError:
                print(f"  ⚠  PORT 값이 올바르지 않습니다: {val!r} (무시)")

    if bacnet_port is None:
        val = os.environ.get("BACNET_PORT", "").strip()
        if val:
            try:
                bacnet_port = int(val)
            except ValueError:
                print(f"  ⚠  BACNET_PORT 값이 올바르지 않습니다: {val!r} (무시)")

    args: list[str] = []
    if port is not None:
        args += ["--port", str(port)]
    if bacnet_port is not None:
        args += ["--bacnet-port", str(bacnet_port)]
    return args


def main() -> int:
    os.chdir(HERE)
    _print_header()

    print(f"  Python: {sys.version.split()[0]}")
    if shutil.which("pip") is None:
        print("  ⚠  pip 실행 파일을 찾지 못했지만, python -m pip 로 계속 진행합니다")

    _install_deps()

    print()
    print("  종료: Ctrl+C")
    print("=" * 49)
    print()

    cmd = [sys.executable, "btl_validator.py", *_build_args()]
    return _run(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
