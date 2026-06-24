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
