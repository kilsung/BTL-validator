"""
BTL Validator Windows 실행파일 빌드 스크립트
실행: python build_exe.py
"""

import subprocess
import sys
import os
from pathlib import Path

HERE = Path(__file__).parent


def run(cmd, check=True, **kwargs):
    print(f"  > {' '.join(str(c) for c in cmd)}")
    return subprocess.run(cmd, check=check, **kwargs)


def main():
    print()
    print("=" * 60)
    print("  BTL Validator Windows 실행파일 빌드")
    print("=" * 60)
    print()

    os.chdir(HERE)

    # ── 필수 패키지 설치 ──────────────────────────────────────────
    print("[1/5] 필수 패키지 설치 중...")
    run([sys.executable, "-m", "pip", "install", "--quiet",
         "pyinstaller", "pillow", "flask",
         "xlrd==1.2.0", "openpyxl", "BAC0"])
    print("      완료\n")

    # ── 아이콘 생성 ───────────────────────────────────────────────
    print("[2/5] 아이콘 생성 중...")
    icon_path = HERE / "btl_icon.ico"
    if not icon_path.exists():
        result = run([sys.executable, "create_icon.py"], check=False)
        if result.returncode != 0:
            print("      [경고] 아이콘 생성 실패 - 기본 아이콘으로 진행")
            # spec 파일에서 icon 항목 None 으로 임시 교체
            spec_text = (HERE / "btl_validator.spec").read_text(encoding="utf-8")
            spec_text = spec_text.replace("icon='btl_icon.ico'", "icon=None")
            (HERE / "btl_validator.spec").write_text(spec_text, encoding="utf-8")
        else:
            print("      완료")
    else:
        print("      btl_icon.ico 이미 존재합니다.")
    print()

    # ── 이전 빌드 정리 ────────────────────────────────────────────
    print("[3/5] 이전 빌드 결과 정리 중...")
    import shutil
    import stat

    def _force_remove(path):
        """읽기 전용/잠금 파일 강제 삭제."""
        try:
            os.chmod(path, stat.S_IWRITE)
            os.unlink(path)
        except Exception as e:
            print(f"      [경고] 삭제 실패 (무시): {path} — {e}")

    def _rmtree(d):
        """Python 버전에 무관하게 강제 삭제 (3.12+ onexc / 구버전 onerror 대응)."""
        import sys as _sys
        if _sys.version_info >= (3, 12):
            def _onexc(func, path, exc):
                _force_remove(path)
            shutil.rmtree(d, onexc=_onexc)
        else:
            def _onerror(func, path, exc_info):
                _force_remove(path)
            shutil.rmtree(d, onerror=_onerror)

    for d in [HERE / "build" / "BTL_Validator", HERE / "dist" / "BTL_Validator"]:
        if d.exists():
            _rmtree(d)
            print(f"      삭제: {d}")
    print()

    # ── PyInstaller 빌드 ──────────────────────────────────────────
    print("[4/5] PyInstaller 빌드 실행 중...")
    print()
    run([sys.executable, "-m", "PyInstaller", "btl_validator.spec", "--noconfirm"])
    print()

    # ── 결과 확인 및 배포 파일 복사 ──────────────────────────────
    exe_path = HERE / "dist" / "BTL_Validator" / "BTL_Validator.exe"
    if not exe_path.exists():
        print("[오류] 실행파일이 생성되지 않았습니다.")
        sys.exit(1)

    print("[5/5] 배포 폴더 구성 중...")
    dist_dir = HERE / "dist" / "BTL_Validator"

    # 런타임 데이터 폴더 미리 생성
    (dist_dir / "uploads").mkdir(exist_ok=True)
    (dist_dir / "logs").mkdir(exist_ok=True)

    # 엑셀 파일 탐색 순서:
    #   1. tools/uploads/     ← 빌드 전 여기에 미리 넣어두면 됨 (권장)
    #   2. tools/             ← tools 폴더 바로 아래
    #   3. tools/../          ← 상위 폴더
    excel_copied = False
    search_dirs = [
        HERE / "uploads",   # 1순위: tools/uploads/
        HERE,               # 2순위: tools/
        HERE.parent,        # 3순위: 상위 폴더
    ]
    for search_dir in search_dirs:
        if not search_dir.exists():
            continue
        found = sorted(list(search_dir.glob("*.xls")) + list(search_dir.glob("*.xlsx")))
        for excel_src in found:
            dest = dist_dir / "uploads" / excel_src.name
            shutil.copy2(excel_src, dest)
            print(f"      복사: uploads/{excel_src.name}  (원본: {excel_src})")
            excel_copied = True
        if excel_copied:
            break

    if not excel_copied:
        print()
        print("  ┌─────────────────────────────────────────────────────────┐")
        print("  │  [안내] 엑셀 파일을 찾지 못했습니다.                   │")
        print("  │                                                          │")
        print("  │  아래 경로에 엑셀 파일을 복사한 뒤 재빌드하세요:       │")
        print(f"  │  {str(HERE / 'uploads')[:50]:<50}  │")
        print("  │                                                          │")
        print("  │  또는 빌드 완료 후 수동으로 복사하세요:                 │")
        print(f"  │  {str(dist_dir / 'uploads')[:50]:<50}  │")
        print("  └─────────────────────────────────────────────────────────┘")
        print()

    print()
    print("=" * 60)
    print("  빌드 완료!")
    print()
    print(f"  실행파일: dist\\BTL_Validator\\BTL_Validator.exe")
    print()
    print("  배포 시 dist\\BTL_Validator\\ 폴더 전체를 복사하세요.")
    print("=" * 60)
    print()

    # 완료 후 탐색기 열기
    try:
        os.startfile(str(dist_dir))
    except Exception:
        pass


if __name__ == "__main__":
    main()
