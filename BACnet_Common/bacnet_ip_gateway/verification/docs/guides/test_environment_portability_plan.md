# BACnet Gateway 및 vport Docker 기반 포터블 테스트 환경 구성 문서

## 1. 개요

본 문서는 `bacnet_ip_gateway` 및 `vport` 프로젝트를 대상으로, **다른 개발 PC 및 실제 IMX 임베디드 타깃 환경**에서도 완전하게 동작하는 **Docker 기반 포터블 테스트 환경**의 구성과 사용법을 정의합니다.

---

## 2. 테스트 참여 프로그램별 역할 및 배치

| 프로그램 / 컴포넌트 | 핵심 역할 및 기능 | 배치 및 실행 위치 |
|---|---|---|
| **`bacnet_ip_gateway`** | **검증 대상 본체 (SUT)**<br>- Modbus RTU 마스터로 동작하며 슬레이브 데이터를 수집하여 BACnet/IP 객체(UDP 47808)로 변환/서비스 | **Host 파일시스템에 위치**<br>`test_config.json`에 경로 지정 후 Docker로 마운트하여 실행 |
| **`vport`**<br>(`vport_test`) | **Modbus RTU Multi-Master Hub 데몬**<br>- 가상 PTY 포트(`ttyMASTER1`, `ttyMASTER2`, `ttySLAVE`) 생성 및 요청 큐잉/응답 라우팅 | **Host 파일시스템에 위치**<br>`test_config.json`에 경로 지정 후 Docker로 마운트하여 실행 |
| **`modbus_rtu_slave.py`** | **Main CDU 시뮬레이터 (PC 검증용)**<br>- CDU Modbus Slave(Unit ID 1) 역할을 에뮬레이션하며 동적 레지스터 제어 및 결함 주입 수신 | Docker 컨테이너 내부에 내장 |
| **`run_gateway_tcs.py`** | **통합 테스트 오케스트레이터**<br>- 88개 TC 선택, PTY/프로세스 수명주기 제어, 패킷 단언 검증, 리포트 생성 총괄 | Docker 컨테이너 내부에 내장 |
| **`btl_epics_verifier.py`** | **BTL TPI 1:1 AST 검증기**<br>- `bacpypes3` 기반으로 TPI 명세와 실제 게이트웨이 객체/속성을 1:1 전수 비교 | Docker 컨테이너 내부에 내장 |
| **`bbmd_server.py`** | **내장 BBMD 서버**<br>- `TC-BIP-002` 검증을 위한 Foreign Device 등록 및 FDT 관리 데몬 | Docker 컨테이너 내부에 내장 (외부 BBMD 미지정 시 자동 구동) |

---

## 3. 사용법 (Usage)

### 3.1 호스트 사전 준비 (바이너리 빌드)
Managed 모드 로컬 테스트를 실행하기 전 호스트에서 바이너리를 빌드합니다:
```bash
# vport 빌드 (vport 및 vport_test 동시 빌드)
make -C ../vport

# bacnet_ip_gateway 빌드 (bacnet_ip_gateway 및 unit/run_tests 동시 빌드)
make
```

### 3.2 설정 파일 (`verification/test_config.json`)
설정 파일은 공통 설정(`common`), 로컬 시뮬레이션 설정(`managed`), 실제 보드 설정(`live`)으로 분리되어 있습니다:
```json
{
  "default_mode": "managed",

  "common": {
    "suite": "full",
    "device_id": 1200,
    "tpi_path": "./model/cdu/[BTL]cdu.tpi",
    "expected_response_time_ms": 200
  },

  "managed": {
    "gateway_config_dir": "./conf",
    "vport_config_path": "../vport_release/vport.cfg",
    "binaries": {
      "gateway_bin": "./bacnet_ip_gateway",
      "vport_bin": "../vport/vport",
      "vport_test_bin": "../vport/vport_test",
      "unit_test_bin": "./verification/unit/run_tests"
    },
    "bbmd_auto": true
  },

  "live": {
    "target_ip": "192.168.1.100",
    "target_port": 47808,
    "gateway_config_dir": "./conf",
    "vport_config_path": "../vport_release/vport.cfg",
    "allow_live_mutation": false,
    "bbmd_endpoint": "",
    "ssh_enabled": false,
    "ssh_user": "root",
    "ssh_port": 22
  }
}
```

### 3.3 대화형 테스트 실행 (Interactive Mode)
```bash
./verification/run_test.sh
```
프롬프트에 따라:
1. **모드 선택**: Managed (1번, 로컬 완전 자동 검증) 또는 Live (2번, 실제 IMX 보드 원격 검증)
2. **스위트 선택**: full (1번), quick (2번), case 지정 (3번)
3. **타깃 IP 및 포트 (Live 모드 시)**: 원격 대상 장비 IP/UDP 포트 입력 (기본값 제공)
4. **BBMD 옵션**: 엔터 키 입력 시 테스트 패키지 내장 BBMD 서버(`bbmd_server.py`, UDP 47809)가 자동 구동됩니다.
5. **장비 상태 변경 허용 여부 (Live 모드 시)**: WriteProperty/DCC/COV 등 실제 장비의 값을 변경하는 TC 허용 여부 선택 (기본: N 안전 모드)
6. **장시간 소요 스트레스 테스트 (--long)**: 60초 대기 및 10분 포트 무응답 시험 포함 여부 선택 (기본: N)
7. **원격 SSH 접속 및 OS 모니터링 옵션 (Live 모드 시)**: 원격 장비 설정 파일 참조 및 메모리/FD/프로세스 감시를 위한 SSH 접속 정보 입력 (기본: N)
8. **설정 경로 확인 (Live 모드 시)**:
   - **SSH 활성화 시**: 원격 보드 내 실제 설정 디렉터리(`gateway.json`, profiles) 및 `vport.cfg` 경로를 지정 (SSH를 통해 원격 보드에서 직접 참조)
   - **SSH 비활성화 시**: 원격 보드와 동일한 설정 파일이 위치한 로컬 PC 경로를 지정

### 3.4 CLI 직접 실행 (Non-Interactive / CI/CD)
```bash
# Quick 모드 로컬 테스트
./verification/run_test.sh --suite quick --mode managed

# Full 모드 로컬 테스트 (내장 BBMD 자동 구동)
./verification/run_test.sh --suite full --mode managed --bbmd-auto

# Full 모드 10분 장기 스트레스 테스트 포함 전체 100% 실행
./verification/run_test.sh --suite full --mode managed --bbmd-auto --long

# 원격 IMX 보드 Live 테스트 (안전 조회 모드)
./verification/run_test.sh --suite full --mode live --ip 192.168.1.100

# 원격 IMX 보드 Live 테스트 (제어/쓰기 및 60초 장기 시험 포함)
./verification/run_test.sh --suite full --mode live --ip 192.168.1.100 --allow-live-mutation --long
```

---

## 4. 결과 확인
테스트 완료 후 생성되는 보고서와 로그는 호스트의 `verification/results/` 디렉터리에 타임스탬프별로 저장됩니다:
- `summary.log`: 전체 결과 요약
- `steps.log`: 각 TC별 단계별 기대값/수신값 로그
- `results.json`: 기계 판독용 전체 메트릭 JSON
- `junit.xml`: CI 연동용 JUnit 테스트 결과
