# -*- mode: python ; coding: utf-8 -*-
# BTL Validator Windows 단독 실행파일 빌드 스펙
# 빌드: pyinstaller btl_validator.spec
#
# 출력: dist/BTL_Validator/BTL_Validator.exe  (--onedir 방식, 빠른 기동)
#       dist/BTL_Validator.exe                (--onefile 방식: 아래 ONEFILE 옵션 참조)

import sys
import os

def _find_bac0_data():
    """BAC0 패키지 내 데이터 파일 경로를 동적으로 찾아 datas 항목 반환."""
    try:
        import BAC0 as _bac0
        bac0_dir = os.path.dirname(_bac0.__file__)
        items = []
        for root, dirs, files in os.walk(bac0_dir):
            for f in files:
                if f.endswith(('.json', '.ini', '.conf', '.yaml', '.csv')):
                    src = os.path.join(root, f)
                    # _internal/BAC0/... 경로 유지
                    rel = os.path.relpath(root, os.path.dirname(bac0_dir))
                    items.append((src, rel))
        return items
    except ImportError:
        return []

_bac0_datas = _find_bac0_data()

block_cipher = None

a = Analysis(
    ['btl_validator.py'],
    pathex=['.'],
    binaries=[],
    datas=[
        # Flask 템플릿 포함
        ('templates', 'templates'),
        # 파서 패키지 포함
        ('parsers', 'parsers'),
        # uploads/ 폴더 (기본 엑셀 파일 포함 — 빌드 시 build_exe.py가 복사)
        ('uploads', 'uploads'),
        # BAC0 데이터 파일 (device.json 등 — 없으면 연결 시 FileNotFoundError 발생)
    ] + _bac0_datas,
    hiddenimports=[
        # ── Flask / Werkzeug ────────────────────────────────────────────
        'flask',
        'flask.templating',
        'jinja2',
        'jinja2.ext',
        'werkzeug',
        'werkzeug.serving',
        'werkzeug.routing',
        'werkzeug.exceptions',
        # ── BAC0 / bacpypes3 ────────────────────────────────────────────
        'BAC0',
        'BAC0.scripts',
        'BAC0.scripts.Lite',
        'BAC0.scripts.Base',
        'BAC0.scripts.ReadWriteScripts',
        'BAC0.scripts.Sim',
        'BAC0.core',
        'BAC0.core.io',
        'BAC0.core.io.Read',
        'BAC0.core.io.Write',
        'BAC0.core.io.Simulate',
        'BAC0.core.app',
        'BAC0.core.app.asyncApp',
        'BAC0.core.devices',
        'BAC0.core.devices.Device',
        'BAC0.core.devices.Points',
        'BAC0.core.utils',
        'BAC0.tasks',
        'bacpypes3',
        'bacpypes3.app',
        'bacpypes3.local',
        'bacpypes3.local.device',
        'bacpypes3.local.object',
        'bacpypes3.pdu',
        'bacpypes3.primitivedata',
        'bacpypes3.constructeddata',
        'bacpypes3.apdu',
        'bacpypes3.npdu',
        'bacpypes3.comm',
        'bacpypes3.netservice',
        'bacpypes3.ipv4',
        'bacpypes3.ipv4.bvll',
        'bacpypes3.ipv4.service',
        # ── 네트워크 인터페이스 감지 ──────────────────────────────────
        'ifaddr',
        'netifaces',
        # ── Excel 처리 ──────────────────────────────────────────────────
        'xlrd',
        'openpyxl',
        'openpyxl.styles',
        'openpyxl.styles.fills',
        'openpyxl.styles.fonts',
        'openpyxl.styles.alignment',
        'openpyxl.utils',
        'openpyxl.utils.cell',
        'openpyxl.writer',
        # ── 파서 패키지 ─────────────────────────────────────────────────
        'parsers',
        'parsers.aircon',
        'parsers.vent',
        'parsers.ahu',
        'parsers.odu',
        'parsers.awhp',
        'parsers.general',
        'parsers.expio',
        'parsers.chiller',
        'parsers.aircare',
        'parsers.base',
        # ── BBMD 검증 모듈 ──────────────────────────────────────────────
        'bbmd_check',
        # ── 기타 ────────────────────────────────────────────────────────
        'asyncio',
        'threading',
        'concurrent.futures',
        'queue',
        'webbrowser',
        'zeroconf',
        'zeroconf._utils',
        'zeroconf._utils.ipaddress',
        'cryptography',
        'cryptography.hazmat',
        'cryptography.hazmat.primitives',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        # 불필요한 대형 패키지 제외
        'matplotlib',
        'numpy',
        'pandas',
        'scipy',
        'PIL',           # 빌드 시에는 사용, 런타임에는 불필요
        'tkinter',
        'IPython',
        'notebook',
        'pytest',
        'sphinx',
        'docutils',
        'PyQt5',
        'PyQt6',
        'wx',
        'gi',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

# ── --onedir 방식 (기본, 빠른 기동) ─────────────────────────────────────────
exe = EXE(
    pyz,
    a.scripts,
    [],                     # onedir: 바이너리는 별도 폴더로
    exclude_binaries=True,  # onedir 필수
    name='BTL_Validator',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    console=True,           # 서버 상태/URL 표시용 콘솔 창 유지
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='btl_icon.ico',    # create_icon.py 로 생성
    version='file_version_info.txt' if os.path.exists('file_version_info.txt') else None,
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='BTL_Validator',   # dist/BTL_Validator/ 폴더 생성
)

# ── --onefile 방식 (배포 간편, 기동 느림 ~10~30초) ─────────────────────────
# 아래 주석을 해제하고 위 exe/coll 블록을 주석 처리하면 단일 .exe 생성
#
# exe = EXE(
#     pyz,
#     a.scripts,
#     a.binaries,
#     a.zipfiles,
#     a.datas,
#     [],
#     name='BTL_Validator',
#     debug=False,
#     bootloader_ignore_signals=False,
#     strip=False,
#     upx=True,
#     upx_exclude=[],
#     runtime_tmpdir=None,
#     console=True,
#     disable_windowed_traceback=False,
#     argv_emulation=False,
#     target_arch=None,
#     codesign_identity=None,
#     entitlements_file=None,
#     icon='btl_icon.ico',
# )
