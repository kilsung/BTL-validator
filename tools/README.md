# BACnet BTL 검증 자동화 툴 — 실행 및 연결 가이드

## 네트워크 환경

| 호스트 | IP | 역할 |
|--------|-----|------|
| 검증 서버 (Linux) | `10.175.181.34` | 툴 실행 서버 (`enp4s0f2`) |
| BACnet 장치 (DUT) | `10.175.191.120` | 테스트 대상 에어컨 컨트롤러 |
| 접속 PC (Windows) | `10.175.191.133` | 웹 브라우저로 툴 접근 |
| 게이트웨이 | `10.175.181.254` | 서브넷 연결 |

> **크로스 서브넷 주의**: 서버(`181.x`)와 장치(`191.x`)는 다른 서브넷입니다.
> BACnet WhoIs 브로드캐스트는 서브넷을 넘지 않으므로, 유니캐스트 직접 통신 방식을 사용합니다.

---

## 사전 조건

### 1. 의존성 설치

```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/BTL/tools
pip install -r requirements.txt
```

의존성 목록:

```
flask>=2.3.0
xlrd==1.2.0        ← 반드시 1.2.0 (.xls 파일 지원)
openpyxl>=3.1.0
BAC0>=22.9.21      ← 실제 설치: 2025.9.15 (BACpypes3 기반 async)
```

### 2. 네트워크 확인

서버에서 BACnet 장치 도달 가능 여부 확인:

```bash
ping 10.175.191.120          # ICMP 연결 확인
nc -zvu 10.175.191.120 47808 # UDP 포트 47808 확인
ip route get 10.175.191.120  # 라우팅 경로 확인
```

기대 결과:
```
10.175.191.120 via 10.175.181.254 dev enp4s0f2 src 10.175.181.34
```

---

## 툴 실행

```bash
cd app.acp.bacnet_tool/tools
python3 run.py
```

또는 자동 실행 스크립트:

Linux/macOS

```bash
./run.sh
```

Windows CMD

```bat
run.bat
```

Windows PowerShell

```powershell
.\run.ps1
```

### 다중 사용자 환경 — 포트 지정 실행

동일 서버에서 여러 명이 동시에 실행하는 경우 각자 다른 포트를 사용합니다.

```bash
# 방법 1: PORT 환경변수 지정
PORT=5001 ./run.sh
PORT=5002 ./run.sh

# 방법 2: --port 인수 직접 지정
python3 run.py --port 5001
python3 run.py --port 5002

# 방법 3: 포트 지정 없이 실행 (5000부터 자동 탐색)
./run.sh   # 5000이 사용 중이면 5001, 5002 … 순서로 자동 전환
```

```bat
REM Windows CMD 예시
set PORT=5001
run.bat

REM 또는 인수 전달
run.bat --port 5001 --bacnet-port 47809
```

> **자동 탐색**: 기본 포트(5000)가 사용 중이면 도구가 자동으로 5001~5019 범위에서 빈 포트를 찾아서 실행합니다.
> 실행 후 터미널 출력에서 실제 접속 URL을 확인하세요.

실행 성공 시 출력 예시:
```
============================================================
  BACnet BTL 검증 자동화 툴  v1.0
  대상: Aircon_List (에어컨 관제점)
  ⚠  포트 5000 사용 중 → 포트 5001 로 자동 전환
  접속 URL → http://10.175.181.34:5001
  (같은 네트워크의 PC 브라우저에서 위 주소로 접속)
============================================================
```

---

## 웹 브라우저 접속

Windows PC에서 Chrome/Edge를 열고:

```
http://10.175.181.34:5000
```

---

## BACnet 연결 방법

### 탭 1: 연결/설정

웹 UI의 **연결/설정** 탭에서 다음 값을 입력합니다:

| 항목 | 값 | 설명 |
|------|----|------|
| 장치 IP | `10.175.191.120` | BACnet 장치(DUT) IP |
| Device Instance | `9000` | BACnet Device Object Identifier |
| 서버 IP | `10.175.181.34` | 자동 입력됨 (수동 수정 가능) |

**연결** 버튼 클릭 → 약 3~5초 후 "연결 완료" 메시지.

> **내부 동작**: BAC0.lite()가 서버 IP(10.175.181.34)의 UDP 47808 포트를 바인딩합니다.
> 연결 자체는 논리적 초기화 단계이며, 실제 BACnet 통신은 검증 실행 시 ReadProperty/WriteProperty로 이루어집니다.

### 크로스 서브넷 연결 시 주의사항

- WhoIs 브로드캐스트는 서브넷을 넘지 않으므로 자동 장치 탐색은 동작하지 않음
- 장치 IP를 수동으로 정확히 입력해야 함
- Device Instance도 정확히 입력해야 함 (기본값 9000)
- ReadProperty 요청은 유니캐스트로 직접 장치 IP에 전송됨 → 정상 동작

---

## 검증 실행 방법

### 탭 2: 관제점 목록

**엑셀 불러오기** 버튼 클릭 → 63개 관제점 로드 확인.

기본 엑셀 경로:
```
/home/js.yang/work/NEXT_ACS/bacnet/BTL/acs5_Instance_number_bacnet_20250324_acpi매칭_260406.xls
```

### 탭 3: 검증 실행

검증 항목 선택:
- **ReadProperty 검증**: 각 관제점 `presentValue` 읽기 (기본 ON)
- **WriteProperty 검증**: Commandable 객체(AO/BO/MO) 쓰기 후 리드백 (기본 ON)
- **BTL 필수 프로퍼티 확인**: ASHRAE 135 필수 프로퍼티 전체 확인 (기본 ON)

**검증 시작** 버튼 → 실시간 진행률 및 결과 표시.

### 탭 4: 결과/리포트

검증 완료 후 **Excel 리포트 생성** 버튼 → PASS/FAIL/SKIP 통계 + 상세 결과 Excel 다운로드.

### 기능 설정 파일 (`tools/btl_config.json`)

탭 표시/기능 활성화를 설정한다. 수정 후 브라우저 새로고침으로 반영되며 서버 재시작은 필요 없다.
파일이 없거나 형식 오류 시 기본값(모두 `false`)을 사용한다. exe 실행 시에는 exe 옆에 둔다.

| 키 | 기본값 | 설명 |
|---|---|---|
| `features.fd_tab` | `false` | FD 검증 탭 표시 + `/api/fd-*`, `/api/bbmd-*` API 활성화 (false 이면 403) |

### 탭 6: TC 검증

`BACnet_Common/bacnet_ip_gateway/verification/scripts/run_gateway_tcs.py`(SRS TC 카탈로그)를
웹 UI에서 실행한다. 기존 `verification/run_test.sh`(WSL/Docker 대화형)의 입력 항목을 UI 옵션으로 대체하며,
기본값은 `verification/test_config.json`, 대상 IP/Device ID는 탭 1 연결 정보, 이후에는 마지막 실행 옵션(`tools/.tc_runner.json`)을 사용한다.

| 모드 | 실행 환경 | 비고 |
|---|---|---|
| Live (실장비) | Windows/Linux 호스트 Python 직접 실행 | WSL·Docker 불필요. 장비 구동/시뮬레이터가 필요한 TC는 BLOCKED |
| Managed (시뮬레이션) | Windows: WSL(Ubuntu) 자동 위임 / Linux: 직접 | vport·Gateway 바이너리 누락 시 `make` 자동 빌드(옵션) |

- verification 폴더는 `BTL_TC_ROOT` 환경변수 → 이 저장소 또는 상위 폴더의 `BACnet_Common` 순으로 자동 탐색 (UI에서 변경 가능)
- **환경 점검**: 선택 모드에 필요한 TPI/PICS/설정 디렉터리, bacpypes3, WSL 툴체인, 바이너리 확인
- 실행 중 TC별 진행 상황·콘솔 로그 실시간 표시, 완료 후 TC/단계별 결과, 실행 이력, 엑셀 리포트(`BTL_TC_Report_*.xlsx`)
- 결과 원본은 `verification/results/<UTC 시각>/` (results.json, steps.csv, junit.xml, packet evidence)
- SSH 암호는 파일에 저장하지 않고 환경변수 `SSH_PASS`로만 전달. Windows에서는 키 기반 인증 권장

---

## BAC0 2025.x asyncio 아키텍처

BAC0 2025.9.15는 BACpypes3(asyncio 기반)으로 완전 재작성되었습니다. 이 툴은 다음 패턴으로 Flask(동기)와 BAC0(비동기)를 브릿지합니다:

```
Flask HTTP 스레드 (동기)
    │
    │── aio_run(coroutine, timeout)
    │       └── asyncio.run_coroutine_threadsafe(coro, _aio_loop)
    │                          │
    ▼                          ▼
  .result(timeout)    bacnet-aio 스레드 (asyncio 루프)
                          │── BAC0.lite() 초기화
                          │── bacnet.read() await
                          └── bacnet.write() await
```

---

## 문제 해결

### 포트 5000 충돌

```bash
# 포트 점유 프로세스 확인
ss -tlnp | grep 5000

# 해당 프로세스 강제 종료
kill -9 <PID>

# 또는 — 다른 포트로 실행하여 충돌 자체를 회피
PORT=5001 ./run.sh
```

> **자동 회피**: `./run.sh`를 포트 지정 없이 실행하면 5000이 사용 중일 때 자동으로 빈 포트(5001, 5002 …)를 찾아 실행합니다.

### 연결 실패 — "no running event loop"

BAC0 2025.x는 asyncio 컨텍스트에서만 초기화됩니다.
툴이 자동으로 `bacnet-aio` 스레드를 띄워 처리하므로, 직접 `python3` 인터프리터에서 `BAC0.lite()`를 호출하면 이 오류가 발생할 수 있습니다.
반드시 **웹 UI의 연결 버튼**을 사용하세요.

### ReadProperty TIMEOUT

크로스 서브넷 환경에서 장치(10.175.191.120)가 서버(10.175.181.34)의 응답 IP를 라우팅할 수 없는 경우 발생합니다.

장치 측 라우팅 테이블에 `10.175.181.0/24 → 게이트웨이`가 등록되어 있는지 장치 관리자에게 확인하세요.

### Demo 모드로 강제 실행

BAC0 연결 없이 UI/리포트 기능만 테스트하려면, 연결하지 않고 검증을 실행합니다.
장치 연결 없이도 SKIP 결과로 리포트 생성이 가능합니다.

---

## 프로세스 종료

```bash
# 서버 터미널에서 Ctrl+C
# 또는 — 모든 btl_validator 프로세스 종료
pkill -f btl_validator.py

# 특정 포트만 종료 (다중 실행 환경)
kill -9 $(ss -tlnp | awk -F'pid=' '/:<포트>/{print $2}' | cut -d, -f1)
# 예) 포트 5001 종료:
kill -9 $(ss -tlnp | awk -F'pid=' '/:5001 /{print $2}' | cut -d, -f1)

# 현재 실행 중인 인스턴스 전체 확인
pgrep -a -f btl_validator.py
```
