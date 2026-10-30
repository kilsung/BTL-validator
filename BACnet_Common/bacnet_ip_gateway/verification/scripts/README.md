# BACnet Gateway Automated Verification Scripts Guide

본 디렉터리(`verification/scripts/`)는 **BACnet/IP Gateway** 및 가상 시리얼 포트 브리지(**vport**)의 기능, 성능, 신뢰성, 표준 적합성(BTL EPICS)을 검증하기 위한 테스트 자동화 스위트 스크립트들을 포함합니다.

---

## 1. 스크립트 모듈 구성 및 역할 정의

| 파일명 | 주요 역할 및 기술 사양 | 실행 주체 / 연동 방식 |
|---|---|---|
| **`run_gateway_tcs.py`** | 통합 검증 러너 CLI 엔트리포인트 (옵션 파싱, 의존성 검사, TC 스위트 디스패치) | `./verification/run_test.sh` 또는 Docker 러너 |
| **`tc_cases.py`** | 88개 전체 테스트 케이스(TC) 핸들러 정의 및 상세 단계(Step) Assertion | `run_gateway_tcs.py`에서 등록 및 실행 |
| **`tc_framework.py`** | TC 등록 데코레이터, 결과 수집, `results.json`, `junit.xml`, Allure 결과 파일 생성 | 러너 프레임워크 코어 |
| **`tc_environment.py`** | 프로세스 수명주기, 가상 PTY 시리얼 링크, 가상 네트워크 및 사전 준비 관리자 | 테스트 실행 런타임 환경 |
| **`bacnet_profile_tester.py`** | BACnet/IP 클라이언트 기반 객체 Read / Write / Restore 사이클 및 프로파일 정합성 검증기 | `tc_cases.py` (`_run_profile_test`) |
| **`btl_epics_verifier.py`** | BTL EPICS/TPI 파일 AST 파싱 및 라이브 게이트웨이 전체 관제점(EPICS 선언 객체) Conformance 전수 검증기 | `tc_cases.py` (`_run_epics_conformance_test`) |
| **`bacnet_protocol.py`** | 순수 파이썬 BACnet NPDU/APDU 바이너리 패킷 빌더 및 파서 | 러너 네이티브 패킷 검증 |
| **`modbus_rtu_slave.py`** | Main Controller 에뮬레이터 (Modbus RTU 슬레이브 + Fault Injection 제어 소켓 지원) | `gateway_server.sh` / `tc_environment.py` |
| **`gateway_server.sh`** | `modbus_rtu_slave.py` 구동용 쉘 래퍼 스크립트 | 백그라운드 프로세스 |
| **`bbmd_server.py`** | BACnet/IP BBMD(Broadcast Management Device) 및 Foreign Device 라우터 에뮬레이터 | `TC-BIP-002` 등 |
| **`config_loader.py`** | 게이트웨이 `conf/` 디렉터리 json/cfg 설정 파서 및 정적 유효성 검사기 | 사전 설정 검증 |
| **`dependencies.json`** | 테스트 케이스별 실행 파일, 의존성 및 호환성 매니페스트 | 러너 사전 의존성 검사 |
| **`performance_policy.json`** | 성능 TC(처리량 RPS, P95/P99 지연시간) 기준 및 허용 한계선 정책 | `TC-PERF-001..003` |

---

## 2. 검증 아키텍처 및 데이터 흐름

```text
[ 사용자 / CI 시스템 ]
        │  ./verification/run_test.sh
        ▼
[ run_gateway_tcs.py (통합 러너) ]
   ├── tc_environment.py (환경 준비: PTY 생성, 게이트웨이 기동, Main 시뮬레이터 구동)
   ├── tc_cases.py (88개 TC 실행)
   │     ├── bacnet_protocol.py (네이티브 BACnet 패킷 송수신)
   │     ├── bacnet_profile_tester.py (BACnet 객체 R/W/Restore 검증)
   │     └── btl_epics_verifier.py (BTL EPICS 선언 전체 관제점 Conformance 전수 검증)
   └── tc_framework.py (결과 리포트 출력)
         ├── results.json (프로젝트 마스터 결과)
         ├── junit.xml (CI/CD 표준 연동용)
         └── *-result.json (Allure Report 대시보드 시각화용)
```

---

## 3. 주요 검증기 상세 안내

### ① BACnet 객체 프로파일 검증기 (`bacnet_profile_tester.py`)
* BACnet/IP 클라이언트로 게이트웨이에 접속하여 표준 fixture 및 실기기 프로파일의 8대 핵심 객체(AI, AO, BI, BV 등)에 대해 **Read $\rightarrow$ Write $\rightarrow$ 복원(Restore) 사이클**을 수행하고 Modbus RTU 반영을 검증합니다.

### ② BTL EPICS Conformance 검증기 (`btl_epics_verifier.py`)
* `*.tpi` (EPICS) 파일을 파싱하여 BTL 표준(Clause 4.4, 4.5, 4.6, 4.7)에 선언된 **전체 관제점(EPICS 선언 객체)의 속성(Max_Pres_Value, Units, Reliability 등)**이 라이브 장비와 100% 일치하는지 전수 검증합니다.

### ③ 장기 연속 운용 신뢰성 검증기 (`TC-AVL-004` Soak Test)
* 장시간(기본 12시간) 동안 전체 관제점에 대해 지속적인 BACnet ReadProperty 질의를 반복하면서 **Gateway 및 vport 두 프로세스의 무중단 생존(No Crash)**, **메모리(RSS < 5% 또는 < 2MB) 누수 억제**, **파일 디스크립터(FD 증가량 <= 0) 누수 방지** 및 통신 성공률(>= 99.9%)을 종합 검증합니다.
* Live 모드 실행 시 원격 타깃 보드에 대해 **SSH 기반 실시간 OS 자원(Gateway & vport) 정밀 모니터링**을 지원합니다.
