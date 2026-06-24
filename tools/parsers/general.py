import xlrd
from .base import parse_sheet

# ── GENERAL 오브젝트 메타데이터 ─────────────────────────────────────────
# instance = (DEVICE_GENERAL=5 << 16) | (unit=0 << 8) | (slot+1)
#
# │ slot │ instance │ 유형 │ 비고
# ├──────┼──────────┼──────┼──────────────────────────────────────────
# │  0   │ 327681   │ BO   │ AllStartStop            ← 구현, 테스트 대상
# │  1   │ 327682   │ AV   │ AllSetRoomTemp           ← 구현, 테스트 대상
# │  2   │ 327683   │ BO   │ AllTempLock              ← 구현, 테스트 대상
# │  3   │ 327684   │ AI   │ TotalAccumulatedPower    ← 구현, read-only (Read 성공 시 항상 PASS)
# │  4   │ 327685   │ BO   │ PeakControlOperation     ← 구현, pair→BI(327686)
# │  5   │ 327686   │ BI   │ PeakControlStatus        ← read-only (BO pair)
# │  6   │ 327687   │ AV   │ PeakShiftTime            ← 구현 (1~999분)
# │  7   │ 327688   │ AI   │ PeakShiftTimeStatus      ← read-only
# │  8   │ 327689   │ AV   │ PeakTargetRatio          ← 구현 (0~100%)
# │  9   │ 327690   │ AI   │ PeakTargetRatioStatus    ← read-only
# │ 10   │ 327691   │ AI   │ PeakCurrentRunningRatio  ← read-only
# │ 11   │ 327692   │ BO   │ RemoteShutdown           ← 항상 SKIP (재부팅 위험)
# │ 12   │ 327693   │ BO   │ TempUnitSetting          ← pair BI=327694
# │ 13   │ 327694   │ BI   │ TempUnitStatus           ← read-only (BO pair)

# AV 범위 주입 (엑셀 미기재 대비)
_GENERAL_AV_RANGES: dict = {
    327682: (16.0, 30.0),   # slot 1: AllSetRoomTemp (°C)
    327687: (5.0, 15.0),   # slot 6: PeakShiftTime (분, 5~15)
    327689: (0.0, 100.0),   # slot 8: PeakTargetRatio (%, 0~100)
}

# 합산 검증 대상 (TotalAccumulatedPower — 모든 IDU+Vent slot-20 합과 비교)
_GENERAL_TOTAL_POWER_INSTANCE = 327684

# 미구현 슬롯 (테스트 불필요) — 모든 슬롯 구현 완료
_GENERAL_SKIP_INSTANCES = frozenset()

# 항상 SKIP (전체 테스트에서만 건너뜀, 단독 실행은 허용)
_GENERAL_ALWAYS_SKIP_INSTANCES = frozenset({327692})

# TempUnitSetting BO → TempUnitStatus BI pair
# Peak BO (slot 4) → PeakControlStatus BI (slot 5) pair
_GENERAL_PAIR: dict = {
    327685: 327686,   # PeakControlOperation BO → PeakControlStatus BI
    327693: 327694,   # TempUnitSetting BO → TempUnitStatus BI
}

# Read 결과만 표시하고, 읽기 성공 시 항상 PASS (Write 검증 불필요)
# 327684: TotalAccumulatedPower — 합산 검증 없이 Read 성공이면 PASS
_GENERAL_READ_PASS_ONLY = frozenset({
    327684,   # TotalAccumulatedPower AI — Read 성공 시 항상 PASS (합산 검증 불필요)
})


def parse_general_list(filepath: str) -> list:
    wb = xlrd.open_workbook(filepath)
    points = parse_sheet(wb, 'General_List')

    for p in points:
        inst = p['instance']

        # AV min/max 주입
        if p['object_type'] == 'AV' and inst in _GENERAL_AV_RANGES:
            if p.get('av_min') is None:
                p['av_min'], p['av_max'] = _GENERAL_AV_RANGES[inst]

        # 미구현 슬롯 SKIP 플래그
        if inst in _GENERAL_SKIP_INSTANCES:
            p['general_skip'] = True

        # 항상 SKIP 플래그 (전체 테스트 한정)
        # commandable은 엑셀 원래값 유지 → 단일 검증 시 Write 허용
        if inst in _GENERAL_ALWAYS_SKIP_INSTANCES:
            p['general_always_skip'] = True

        # pair_instance 설정 (BO→BI 상태 확인)
        if inst in _GENERAL_PAIR:
            p['pair_instance'] = _GENERAL_PAIR[inst]

        # Read 결과만으로 PASS 판정 (Write 스킵)
        if inst in _GENERAL_READ_PASS_ONLY:
            p['general_read_pass_only'] = True

        # 합산 검증 플래그: IDU+Vent 각 slot-20 값의 합과 비교
        if inst == _GENERAL_TOTAL_POWER_INSTANCE:
            p['total_power_check'] = True

    return points
