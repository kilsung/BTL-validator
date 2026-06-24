#!/usr/bin/env python3.11
"""
Peak Demand BACnet 제어 검증 테스트
=====================================
acp_simulator(BACnet)를 통해 Peak Demand 파라미터를 제어하고,
제어 결과가 com.b2b.setting.service 및 com.acp.energy.service에
올바르게 반영됐는지 Pass/Fail 로 검증한다.

■ 검증 항목
  1. PeakShiftTime   AV (inst=327687)  Write → b2b.setting.service switchTime 일치
  2. PeakTargetRatio AV (inst=327689)  Write → b2b.setting.service targetRate 일치
  3. PeakControlOp   BO (inst=327685)  Write → b2b.setting.service enable 일치
  4. PeakCurrentRunningRatio AI (inst=327691)  Read ↔ getPeakRate.currentRate 일치

■ b2b.setting.service 조회 방법 (SSH)
  luna-send -n 1 luna://com.b2b.setting.service/getAll
      '{"appId":"com.acp.energy.service"}'

■ getPeakRate 조회 방법 (SSH)
  luna-send -n 1 luna://com.acp.energy.service/getPeakRate '{}'

사용법:
    cd tools/
    python3 peak_demand_test.py
    python3 peak_demand_test.py --ip 10.175.195.114 --id 9000 --ssh-user root
"""

import sys
import os
import time
import json
import argparse
import subprocess

# ── btl_validator 디렉토리를 sys.path 에 추가 ─────────────────────
_TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if _TOOLS_DIR not in sys.path:
    sys.path.insert(0, _TOOLS_DIR)

import btl_validator as btl
from btl_validator import aio_run, bacnet_read, bacnet_write, state, _aio_loop

# ── 기본 파라미터 ─────────────────────────────────────────────────
DEV_IP    = "10.175.195.114"
DEV_ID    = 9000
SSH_USER  = "root"
WAIT_SEC  = 2.5     # BACnet write 후 서비스 반영 대기
TOLERANCE = 0.1     # 부동소수점 비교 허용 오차

# GENERAL 오브젝트 인스턴스 번호 (general.py 기준)
INST_PEAK_CTRL_OP     = 327685   # BO  slot 4  PeakControlOperation
INST_PEAK_SHIFT_TIME  = 327687   # AV  slot 6  PeakShiftTime
INST_PEAK_TARGET      = 327689   # AV  slot 8  PeakTargetRatio
INST_PEAK_CURRENT     = 327691   # AI  slot 10 PeakCurrentRunningRatio

# ── 테스트 케이스 정의 ─────────────────────────────────────────────
# (설명, 오브젝트 타입, 인스턴스, 쓸 값, 검증 방법, 검증 키)
#   verify_mode:
#     "setting"  → b2b.setting.service의 peakDemand.<key> 와 비교
#     "readback" → BACnet read-back 값과 에너지 서비스 값 비교
TEST_CASES = [
    {
        "desc"        : "PeakShiftTime AV write → b2b.setting.service switchTime",
        "obj_type"    : "analogValue",
        "instance"    : INST_PEAK_SHIFT_TIME,
        "write_val"   : 10,
        "verify_mode" : "setting",
        "setting_key" : "switchTime",
        "expected"    : 10,
    },
    {
        "desc"        : "PeakTargetRatio AV write → b2b.setting.service targetRate",
        "obj_type"    : "analogValue",
        "instance"    : INST_PEAK_TARGET,
        "write_val"   : 75.0,
        "verify_mode" : "setting",
        "setting_key" : "targetRate",
        "expected"    : 75.0,
    },
    {
        "desc"        : "PeakControlOperation BO write(1=run) → b2b.setting.service enable=true",
        "obj_type"    : "binaryOutput",
        "instance"    : INST_PEAK_CTRL_OP,
        "write_val"   : 1,   # 1=Active/run
        "verify_mode" : "setting",
        "setting_key" : "enable",
        "expected"    : True,
    },
    {
        "desc"        : "PeakCurrentRunningRatio AI read ↔ getPeakRate.currentRate 일치",
        "obj_type"    : "analogInput",
        "instance"    : INST_PEAK_CURRENT,
        "write_val"   : None,   # read-only, write 없음
        "verify_mode" : "readback",
        "setting_key" : None,
        "expected"    : None,
    },
]

# ── 컬러 출력 ─────────────────────────────────────────────────────
GREEN  = "\033[92m"
RED    = "\033[91m"
YELLOW = "\033[93m"
RESET  = "\033[0m"

def _pass(msg): print(f"  {GREEN}[PASS]{RESET} {msg}")
def _fail(msg): print(f"  {RED}[FAIL]{RESET} {msg}")
def _info(msg): print(f"  {YELLOW}[INFO]{RESET} {msg}")


# ── SSH 헬퍼 ──────────────────────────────────────────────────────
def _ssh(ssh_user: str, dev_ip: str, cmd: str) -> dict | None:
    """디바이스에 SSH로 luna-send 명령을 실행하고 JSON 응답을 반환한다."""
    full_cmd = ["ssh", f"{ssh_user}@{dev_ip}", cmd]
    try:
        result = subprocess.run(
            full_cmd, capture_output=True, text=True, timeout=10
        )
        return json.loads(result.stdout.strip())
    except Exception as e:
        print(f"  SSH 오류: {e} (cmd={cmd})")
        return None


def get_b2b_setting(ssh_user: str, dev_ip: str) -> dict | None:
    """com.b2b.setting.service 에서 peakDemand 설정값을 읽어온다."""
    resp = _ssh(
        ssh_user, dev_ip,
        "luna-send -n 1 luna://com.b2b.setting.service/getAll "
        "'{\"appId\":\"com.acp.energy.service\"}'"
    )
    if resp and resp.get("returnValue"):
        return resp.get("settings", {}).get("peakDemand")
    return None


def get_peak_rate(ssh_user: str, dev_ip: str) -> float | None:
    """com.acp.energy.service/getPeakRate 에서 currentRate 를 읽어온다."""
    resp = _ssh(
        ssh_user, dev_ip,
        "luna-send -n 1 luna://com.acp.energy.service/getPeakRate '{}'"
    )
    if resp and resp.get("returnValue"):
        return float(resp.get("currentRate", -1))
    return None


# ── BACnet 연결 초기화 (mo11_stress_test.py 동일 패턴) ────────────
def connect(dev_ip: str, dev_id: int):
    import asyncio
    import BAC0

    async def _connect():
        ip_arg = f"{btl.SERVER_IP}/24"
        bacnet_port = state.get("bacnet_port", 47808)
        bacnet = BAC0.lite(ip=ip_arg, port=bacnet_port)
        await asyncio.sleep(3)
        try:
            from bacpypes3.app import DeviceInfo
            from bacpypes3.pdu import Address as BACAddress
            dev_addr = BACAddress(dev_ip)
            dev_info = DeviceInfo(
                device_instance=dev_id,
                device_address=dev_addr,
                max_apdu_length_accepted=480,
                segmentation_supported=3,
            )
            cache = bacnet.this_application.app.device_info_cache
            cache.address_cache[dev_addr] = dev_info
            cache.instance_cache[dev_id]  = dev_info
            print(f"  [btl] DeviceInfo 등록: {dev_ip} (Device {dev_id})")
        except Exception as e:
            print(f"  [btl] DeviceInfo 등록 실패 (무시): {e}")
        return bacnet

    return aio_run(_connect())


# ── 단일 테스트 실행 ──────────────────────────────────────────────
def run_test(tc: dict, bacnet, dev_ip: str, dev_id: int,
             ssh_user: str) -> bool:
    """한 테스트 케이스를 실행하고 Pass/Fail 을 반환한다."""
    print(f"\n{'─'*60}")
    print(f"  {tc['desc']}")

    # ── 1. BACnet Write ────────────────────────────────────────────
    if tc["write_val"] is not None:
        try:
            aio_run(bacnet_write(
                bacnet, dev_ip, dev_id,
                tc["obj_type"], tc["instance"],
                tc["write_val"]
            ))
            _info(f"BACnet write: {tc['obj_type']} inst={tc['instance']} "
                  f"← {tc['write_val']}")
        except Exception as e:
            _fail(f"BACnet write 실패: {e}")
            return False

        _info(f"{WAIT_SEC}초 대기 중 (서비스 반영 대기)...")
        time.sleep(WAIT_SEC)

    # ── 2. 검증 ───────────────────────────────────────────────────
    if tc["verify_mode"] == "setting":
        setting = get_b2b_setting(ssh_user, dev_ip)
        if setting is None:
            _fail("b2b.setting.service 조회 실패")
            return False

        actual = setting.get(tc["setting_key"])
        expected = tc["expected"]
        _info(f"b2b.setting.service peakDemand.{tc['setting_key']} = {actual!r}  "
              f"(expected={expected!r})")

        # bool 비교
        if isinstance(expected, bool):
            ok = (bool(actual) == expected)
        # 숫자 비교 (허용 오차)
        else:
            try:
                ok = abs(float(actual) - float(expected)) <= TOLERANCE
            except (TypeError, ValueError):
                ok = (actual == expected)

        if ok:
            _pass(f"{tc['setting_key']} 일치 ({actual})")
        else:
            _fail(f"{tc['setting_key']} 불일치: actual={actual!r}, expected={expected!r}")
        return ok

    elif tc["verify_mode"] == "readback":
        # BACnet read
        try:
            bacnet_val = aio_run(bacnet_read(
                bacnet, dev_ip, dev_id,
                tc["obj_type"], tc["instance"]
            ))
        except Exception as e:
            _fail(f"BACnet read 실패: {e}")
            return False

        # energy service read
        energy_val = get_peak_rate(ssh_user, dev_ip)
        if energy_val is None:
            _fail("getPeakRate 조회 실패")
            return False

        _info(f"BACnet PeakCurrentRunningRatio (inst={tc['instance']}) = {bacnet_val}")
        _info(f"getPeakRate.currentRate = {energy_val}")

        try:
            ok = abs(float(bacnet_val) - float(energy_val)) <= TOLERANCE
        except (TypeError, ValueError):
            ok = (bacnet_val == energy_val)

        if ok:
            _pass(f"BACnet ({bacnet_val}) ↔ getPeakRate ({energy_val}) 일치")
        else:
            _fail(f"불일치: BACnet={bacnet_val}, getPeakRate.currentRate={energy_val}")
        return ok

    _fail(f"알 수 없는 verify_mode: {tc['verify_mode']}")
    return False


# ── 복원 헬퍼 ─────────────────────────────────────────────────────
def restore_defaults(bacnet, dev_ip: str, dev_id: int, ssh_user: str):
    """테스트 후 peak demand 파라미터를 원래 기본값으로 복원한다."""
    print(f"\n{'─'*60}")
    print("  [복원] 기본값 복원 중...")

    # 1. 운전 시작 (enable=true)
    try:
        aio_run(bacnet_write(bacnet, dev_ip, dev_id,
                             "binaryOutput", INST_PEAK_CTRL_OP, 1))
        _info("PeakControlOperation → 1 (run/enable=true)")
    except Exception as e:
        _info(f"복원 write 실패 (무시): {e}")

    time.sleep(WAIT_SEC)

    # 2. switchTime=5, targetRate=100 으로 복원
    try:
        aio_run(bacnet_write(bacnet, dev_ip, dev_id,
                             "analogValue", INST_PEAK_SHIFT_TIME, 5))
        aio_run(bacnet_write(bacnet, dev_ip, dev_id,
                             "analogValue", INST_PEAK_TARGET, 100.0))
        _info("PeakShiftTime → 5, PeakTargetRatio → 100.0 (복원)")
    except Exception as e:
        _info(f"복원 write 실패 (무시): {e}")

    time.sleep(WAIT_SEC)

    # 3. 복원 후 setting 상태 출력
    setting = get_b2b_setting(ssh_user, dev_ip)
    print(f"  복원 후 b2b.setting peakDemand = {json.dumps(setting, ensure_ascii=False)}")


# ── 메인 ──────────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="Peak Demand BACnet 검증 테스트")
    parser.add_argument("--ip",       default=DEV_IP,   help="디바이스 IP")
    parser.add_argument("--id",       default=DEV_ID,   type=int, help="BACnet Device ID")
    parser.add_argument("--ssh-user", default=SSH_USER, help="SSH 사용자명")
    parser.add_argument("--wait",     default=WAIT_SEC, type=float,
                        help="write 후 대기 시간(초)")
    parser.add_argument("--no-restore", action="store_true",
                        help="테스트 후 기본값 복원 생략")
    args = parser.parse_args()

    print("=" * 60)
    print("  Peak Demand BACnet 제어 검증 테스트")
    print(f"  Device: {args.ip}  (BACnet ID={args.id})")
    print(f"  SSH:    {args.ssh_user}@{args.ip}")
    print("=" * 60)

    # 테스트 전 현재 setting 상태 출력
    setting_before = get_b2b_setting(args.ssh_user, args.ip)
    print(f"\n[사전] b2b.setting peakDemand = {json.dumps(setting_before, ensure_ascii=False)}")

    # BACnet 연결
    print("\n  BACnet 연결 중...")
    try:
        bacnet = connect(args.ip, args.id)
    except Exception as e:
        print(f"{RED}  BACnet 연결 실패: {e}{RESET}")
        sys.exit(1)
    print("  연결 완료\n")

    # 테스트 실행
    results: list[tuple[str, bool]] = []
    for tc in TEST_CASES:
        ok = run_test(tc, bacnet, args.ip, args.id, args.ssh_user)
        results.append((tc["desc"], ok))

    # 결과 요약
    print(f"\n{'=' * 60}")
    print("  결과 요약")
    print(f"{'=' * 60}")
    passed = sum(1 for _, ok in results if ok)
    total  = len(results)
    for desc, ok in results:
        mark = f"{GREEN}PASS{RESET}" if ok else f"{RED}FAIL{RESET}"
        # 설명이 길면 줄임
        short = (desc[:52] + "...") if len(desc) > 55 else desc
        print(f"  [{mark}] {short}")
    print(f"{'─' * 60}")
    print(f"  {passed}/{total} 통과")
    print(f"{'=' * 60}")

    # 기본값 복원
    if not args.no_restore:
        restore_defaults(bacnet, args.ip, args.id, args.ssh_user)

    # 최종 종료 코드
    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
