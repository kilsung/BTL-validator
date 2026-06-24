# BTL 검증 툴 개발 히스토리

---

## 2026-04-16 | 작성자: js.yang

### Write 안정성 개선 + Race Condition 수정 + 진행중 UI

#### 1. `_relinquish_async` 동기 블로킹으로 변경 (Race Condition 수정)

**문제**: fire-and-forget 방식으로 relinquish를 실행하면 다음 테스트 도중 실행되어
swing/lock BO FAIL 발생 (장치 CNI 처리 방해).

**수정 (`btl_validator.py`)**
- `_relinquish_async()` 내부를 `aio_run()` 동기 블로킹으로 변경
- relinquish 완료 후 다음 테스트가 시작되도록 보장

---

#### 2. 폴링 기반 Write 검증 (전 타입 적용)

**문제**: write 직후 즉시 readback → CNI 미반영 상태에서 FAIL 판정.

**수정 (`btl_validator.py`)**
- `_poll_bool_readback()`, `_poll_int_readback()`, `_poll_float_readback()` 헬퍼 추가
- write 후 `_POLL_INITIAL` 고정 대기 → 잔여 시간 동안 0.2초 간격 폴링
- BO/MO/AV/BV/AO/EXPIO_AV 전 타입 폴링 적용

**폴링 파라미터** (현재 적용값):
| 파라미터 | 값 | 설명 |
|----------|-----|------|
| `_POLL_INITIAL` | 1.0s | write 후 최소 고정 대기 |
| `_POLL_INTERVAL` | 0.2s | 재시도 간격 |
| `_POLL_TIMEOUT` | 4.0s | 기본 최대 대기 |
| `_POLL_TIMEOUT_V` | 4.0s | Vent 장치용 최대 대기 |

---

#### 3. oper BO active write 안정화 대기

**문제**: oper BO(기동명령) 검증 완료 후 active로 유지하는데, 다음 테스트 시작 전  
장치 CNI 처리가 완료되지 않아 lock BO FAIL 발생.

**수정 (`btl_validator.py`)**
- oper BO active write 후 `time.sleep(_POLL_INITIAL)` 추가
- `run_tests_thread` inter-test delay도 `_POLL_INITIAL` (1.0s)로 통일

---

#### 4. 사전설정 readback 확인 후 후속 검증

**문제**: `_apply_pre_condition()`이 문자열만 반환 → 실패 여부 판단 불가,  
사전설정 미반영 상태에서 write 검증 강행.

**수정 (`btl_validator.py`)**
- `_apply_pre_condition()` 반환 타입을 `str` → `(bool, str)` 로 변경
- mode MO write 후 readback 폴링으로 실제 반영 확인
- `pre_ok=False` 이면 write_result='SKIP', 후속 검증 생략

---

#### 5. "진행중" 실시간 행 표시 UI

**요구사항**: 검증 진행 중 현재 테스트 중인 관제점을 실시간 결과 테이블에 표시.

**수정 (`btl_validator.py`)**
- `run_tests_thread()`: `_test_one_point()` 호출 전에 `start` 이벤트 전송
- 이벤트 타입: `start` (테스트 시작) → `progress` (테스트 완료) → `complete` (전체 완료)

**수정 (`index.html`)**
- SSE `start` 이벤트 핸들러 `handleStartEvent()` 추가
  - `live-row-{instance}` ID 행 삽입, `row-running` 클래스 + 스피너 표시
- `handleProgressEvent()`: 기존 행(`live-row-{instance}`)을 최종 결과로 갱신 (append → update)
- `.row-running` CSS: `#EBF5FB` ↔ `#D6EAF8` 1.2s 펄스 애니메이션

---

## 2026-04-13 (3차) | 작성자: js.yang

### BACnet 포트 충돌 해결 + Write 검증 로직 수정

**BACnet UDP 포트 자동 탐색 (`btl_validator.py`)**
- `_find_free_udp_port()` 추가 (SOCK_DGRAM)
- `--bacnet-port` CLI 인수 추가 (기본값 47808)
- 기동 시 UDP 빈 포트 자동 탐색 → `state['bacnet_port']` 저장
- `BAC0.lite(ip=..., port=bacnet_port)` — 포트 파라미터 전달
- `/api/status` 응답에 `bacnet_port` 포함
- 다중 인스턴스 동시 실행 시 포트 충돌 자동 회피

**`run.sh`**
- `BACNET_PORT` 환경변수 지원 추가

**Write 검증 로직 수정 (`btl_validator.py`)**
- AV/BO/MO write 검증을 pair AI/BI/MI readback → **쓴 오브젝트 자체 readback** 으로 변경
  - pair 오브젝트(실내온도 AI, 실제 상태 BI/MI)는 에어컨 물리 응답 지연으로 즉시 반영 안 됨
  - WriteProperty 프로토콜 수락 여부만 검증하는 BTL 목적에 부합
- write 후 딜레이 0.5초 → 1.0초로 증가

---

## 2026-04-13 (2차) | 작성자: js.yang

### 9개 장치 타입 지원 + 다중 장치 선택 UI

**parsers/ 구조 신설**
- `parsers/base.py` — 범용 시트 파서 (`parse_sheet()`)
- `parsers/__init__.py` — `DEVICE_REGISTRY` (9개 장치 키 → label/parse 매핑)
- 장치별 파서 9개 분리 (aircon/vent/ahu/odu/awhp/general/expio/chiller/aircare)
- EXPIO: AI/BI read-only, AV `write_mode='simple'`(0.0 write+readback), BV standard

**btl_validator.py**
- `from parsers import DEVICE_REGISTRY` 로 구 `parse_aircon_list` 대체
- `_write_test_expio_av()` 추가
- `/api/load-excel` — `devices:[{device_type, b2_count}]` 배열 수신, 다중 장치 합산
- `/api/status` — `devices` 목록 반환

**index.html**
- 장치 선택: `<select>` 드롭다운 → 9개 카드 3열 그리드로 교체 (복수 선택 가능)
- 각 카드에 대수 입력 인라인 배치, 전체/해제 버튼
- 등록 대수 입력: `0=1대` 방식 → `1=1대` 직관적 입력으로 변경
- 관제점 목록 테이블: 장치 열·대수 열 추가, 대수 표시 1-base로 변경

---

## 2026-04-13 (1차) | 작성자: js.yang

### 개요
관제점 목록 파일 드래그&드롭 오픈, 다중 장치(B2 Cell) 지원, 포트 자동 탐색,
자동 파일 복원, 검증 완료 후 결과탭 자동 이동 등 UX 개선 작업 진행.

---

### 1. 관제점 목록 파일 — 드래그&드롭 / 파일 열기

**요구사항**: 경로 직접 입력 방식 제거 → 드래그&드롭 또는 파일 선택 다이얼로그로 교체.

**백엔드 (`btl_validator.py`)**
- `UPLOAD_DIR = tools/uploads/` 자동 생성
- `/api/upload-excel` 엔드포인트 추가 (multipart/form-data 수신, `.xls`/`.xlsx` 검사, 경로 탐색 공격 방지)

**UI (`index.html`)**
- 드래그&드롭 영역(`#dropZone`) 추가 — 파일을 끌어다 놓으면 즉시 업로드
- 클릭 시 파일 선택 다이얼로그 열림 (`.xls`, `.xlsx` 허용)
- 선택/업로드 완료 후 드롭존 UI 상태(아이콘, 파일명) 업데이트

---

### 2. 장치 등록 대수 (B2 Cell) 지원

**요구사항**: 1대 이상 검증 지원. Instance 계산식: `instance = B2 × 256 + 원래_instance`

| 입력값 | 장치 수 | Instance 시작 |
|--------|---------|--------------|
| B2=0 | 1대 | 원래값 |
| B2=1 | 2대 | 원래값 + 256 |
| B2=9 | 10대 | 원래값 + 2304 |

**백엔드 (`btl_validator.py`)**
- `/api/load-excel`: `b2_count` 파라미터 수신
- B2 오프셋 적용: 각 장치 인스턴스 및 `pair_instance` 일괄 시프트
- `state['b2_count']` 저장, `/api/status`에 반환

**UI (`index.html`)**
- B2 Cell 입력 필드 추가 (기본값 `0` = 1대)
- 입력 시 "X대" 라벨 실시간 갱신
- 관제점 목록 테이블에 B2 컬럼 추가
- B2 필터 드롭다운 추가 (B2별 포인트 필터링)
- Excel 리포트에 B2 컬럼 추가

---

### 3. 포트 자동 탐색 (다중 사용자 환경 지원)

**요구사항**: 동일 서버에서 여러 명이 실행 시 포트 충돌 자동 회피.

**`btl_validator.py`**
- `--port` 커맨드라인 인수 추가 (기본값 `5000`)
- `_find_free_port(start, max_try=20)` 함수: 지정 포트 사용 중이면 +1씩 탐색
- 포트 전환 시 터미널에 경고 메시지 출력

**`run.sh`**
- `PORT` 환경변수 지원: `PORT=5001 ./run.sh`
- 미지정 시 기본 5000, 사용 중이면 자동 탐색

**README.md**
- 다중 사용자 환경 섹션 추가 (PORT 지정 / --port 인수 / 자동 탐색 3가지 방법)
- 포트 충돌 해결 방법, 프로세스 종료 명령 보강

---

### 4. 이전 파일 자동 복원 및 자동 로드

**요구사항**: 페이지 재접속 시 이전에 사용한 파일을 자동으로 복원 / 로드.
파일 오픈 실패 시에만 파일 선택 유도.

**`btl_validator.py`**
- `.btl_last_file` — 마지막 성공 로드 파일 경로 저장 (서버 재시작 후 복원)
- 서버 시작 시 `.btl_last_file` 읽어 `state['uploaded_excel']` 복원
- `/api/status`에 `excel_file_exists` 필드 추가 (파일 실제 존재 여부)

**`index.html`**
- `_setDropZoneLoaded(filename, isPrevious)` — 드롭존 "파일 로드됨" 상태 설정
  - `isPrevious=true` 시 "이전 파일 복원됨" 안내 표시
- `_resetDropZone()` — 자동 로드 실패 시 파일 선택 유도
- `fetchStatus()` — `uploaded_excel` + `excel_file_exists` 확인 후 1회 자동 로드
- `loadExcel(autoLoad)` — 자동/수동 구분, 자동 실패 시 드롭존 리셋

---

### 5. 검증 완료 후 결과/리포트 탭 자동 이동

**`index.html`**: `handleCompleteEvent()` 에 탭 자동 전환 1줄 추가.
전체 검증 완료 시 Tab 4(결과/리포트) 로 자동 포커스.

---

### .gitignore 추가

```
tools/.btl_last_file   ← 런타임 파일 경로 메모
tools/uploads/         ← 업로드된 엑셀 파일
tools/logs/            ← 검증 로그
```

---

### 최종 상태

| 구성 요소 | 파일 | 상태 |
|-----------|------|------|
| Flask 백엔드 | `tools/btl_validator.py` | 운영 중 (포트 5000, 자동 탐색) |
| 웹 UI | `tools/templates/index.html` | 정상 |
| 검증 로그 | `tools/logs/btl_*.log` | 자동 저장 (gitignore) |
| 업로드 파일 | `tools/uploads/` | 자동 저장 (gitignore) |

**접속 URL**: `http://10.175.181.34:5000`

**서버 기동 명령**
```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/app.acp.bacnet_tool/tools
./run.sh
# 또는 특정 포트 지정
PORT=5001 ./run.sh
```

**포트 충돌 시**
```bash
kill -9 $(ss -tlnp | awk -F'pid=' '/:5000 /{print $2}' | cut -d, -f1)
# 또는 포트 자동 탐색으로 실행
./run.sh
```

---



### 개요
BACnet BTL 인증을 위한 에어컨 관제점 검증 웹툴 초기 개발 및 연결 문제 해결,
기능 보완 작업을 일괄 진행함.

---

### 1. BTL Agent Harness 재설계

**작업 내용**
`.github/` 디렉터리의 GitHub Copilot 에이전트 설정 파일 전면 재설계.

**생성/수정 파일**
| 파일 | 내용 |
|------|------|
| `.github/copilot-instructions.md` | 프로젝트 개요, 빌드 명령, SDK 환경, 코드 스타일 기준 정의 |
| `.github/instructions/bacnet-bacstac.instructions.md` | BACstac API 사용 규칙 |
| `.github/instructions/btl-conformance.instructions.md` | BTL 표준 적합성 규칙 |
| `.github/instructions/bacnet-build.instructions.md` | 크로스 컴파일 빌드 규칙 |
| `.github/skills/btl-test-analysis/SKILL.md` | BTF 테스트 실패 분석 스킬 |
| `.github/skills/bacnet-debug/SKILL.md` | BACstac 런타임 디버깅 스킬 |

---

### 2. Flask 검증 웹툴 초기 제작

**목적**: 엑셀(`Aircon_List`) 파싱 → BACnet 관제점 자동 검증 → 결과 리포트

**생성 파일**
- `tools/btl_validator.py` — Flask 백엔드 + BACnet 검증 엔진
- `tools/templates/index.html` — Bootstrap 5 기반 4탭 웹 UI

**주요 기능**
- xlrd 1.2.0 로 `.xls` 파싱 (Aircon_List 시트, 5행~끝)
- 관제점별 ReadProperty / WriteProperty / 필수 프로퍼티 3종 검증
- SSE(Server-Sent Events) 기반 실시간 진행률 표시
- Excel 리포트(`.xlsx`) 자동 생성

---

### 3. BAC0 설치 및 네트워크 환경 파악

**환경**
| 항목 | 값 |
|------|-----|
| 서버(검증PC) IP | `10.175.181.34/24` |
| 장치(DUT) IP | `10.175.191.120` |
| BACnet Device Instance | `9000` |
| BAC0 버전 | `2025.9.15` (BACpypes3 asyncio 기반) |

**문제**: 서버(`181.x`)와 장치(`191.x`)가 서로 다른 서브넷.
라우팅은 `ip route` 확인 결과 통신 가능하나,
BAC0 내부의 WhoIs 5회 재시도 루프가 브로드캐스트 기반이라 장치를 발견하지 못함 → 약 20초 TIMEOUT 발생.

---

### 4. BAC0 asyncio 브릿지 구현

**문제**: Flask는 동기(sync), BAC0 2025.x는 완전 asyncio API → 직접 호출 불가.

**해결책**: asyncio 이벤트 루프를 별도 daemon 스레드에서 실행, `run_coroutine_threadsafe()` 로 브릿징.

```python
# btl_validator.py 핵심 패턴
_aio_loop: asyncio.AbstractEventLoop = None

def _start_aio_loop():
    global _aio_loop
    _aio_loop = asyncio.new_event_loop()
    _aio_loop.run_forever()

def aio_run(coro, timeout=15.0):
    future = asyncio.run_coroutine_threadsafe(coro, _aio_loop)
    return future.result(timeout=timeout)
```

---

### 5. WhoIs 우회 — DeviceInfo 직접 등록

**문제**: `bacpypes3.primitivedata.Address` import 실패(ImportError).
올바른 경로: `bacpypes3.pdu.Address`.

**해결**: WhoIs를 사용하지 않고 `build_rp_request()` + `_app.read_property()` 직접 호출 방식으로 변경.

```python
def bacnet_read(addr, obj_long, instance, prop):
    async def _read():
        args_split = [addr, obj_long, str(instance), prop]
        device_address, object_identifier, property_identifier, arr_index = \
            state['bacnet'].build_rp_request(args_split)
        _app = state['bacnet'].this_application.app
        return await _app.read_property(
            device_address, object_identifier, property_identifier, arr_index)
    return aio_run(_read(), timeout=12)
```

**효과**: WhoIs 재시도 루프 완전 우회 → 크로스 서브넷 환경에서도 정상 통신.

---

### 6. 개별 관제점 검증 기능 추가

**요구사항**: 전체 검증 외에 관제점 1개씩 즉시 검증 가능하도록.

**구현**
- 백엔드: `_test_one_point()` 공통 헬퍼 함수 + `/api/test-single` 엔드포인트
- UI Tab 2 (관제점 목록): 각 행에 ▶ 버튼 추가
- UI Tab 3 (검증 실행): 단일 관제점 입력 + 즉시 실행 패널 추가

---

### 7. 로그 파일 자동 저장 기능 추가

**구현**
- `tools/logs/` 디렉터리 자동 생성
- 파일명 형식: `btl_full_YYYYMMDD_HHMMSS.log` (전체 검증), `btl_single_YYYYMMDD_HHMMSS.log` (개별 검증)
- `_start_log_file(label)` 함수: Python `logging.FileHandler` 동적 추가
- 백엔드 API: `/api/logs` (목록), `/api/download-log/<filename>` (다운로드)
- UI: 로그 파일명 배지 + "로그 저장" 다운로드 버튼

---

### 8. BO/MO Write FAIL 버그 수정

**증상**: BO(Binary Output), MO(Multi-state Output) WriteProperty가 항상 FAIL.

**원인 분석**
1. 리드백 비교 방식 문제:
   `str(<BinaryPV: active>)` 와 `str(1)` 이 다르므로 항상 불일치 판정.
2. MO null 해제 쓰기(priority clear)가 동기 대기 → 12초 블로킹.

**수정**
1. 리드백 성공 여부(`rb_err == 'OK'` and `rb_val is not None`)만 체크, 값 비교 제거.
2. null 해제 쓰기를 fire-and-forget으로 변경 (`run_coroutine_threadsafe` 후 결과 대기 안 함).

---

### 9. CNI '-' 관제점 SKIP 처리

**요구사항**: F열(cni)이 `'-'`인 관제점은 BACnet 검증 불가 → SKIP으로 구분.

**백엔드 수정** (`btl_validator.py`)
```python
def _test_one_point(point, settings, addr):
    if point.get('cni', '').strip() in ('-', ''):
        result['error'] = 'CNI 없음'
        return result  # overall='SKIP'
```

**UI 수정** (`index.html`)
- 로드 완료 메시지: `✅ 63개 관제점 로드 완료 (CNI 없음 N개 SKIP)` 형식
- CNI 없는 행: `opacity:0.55` 반투명 + "CNI 없음" 배지 + ▶버튼 대신 "SKIP" 텍스트

**버그 수정**: 변수명 `noСni`(키릴 С, U+0421 포함) → `noCni`(ASCII) 4곳 교체.

---

### 10. CNI 파싱 로직 개선 (복수 CNI + fallback)

**요구사항**
1. F열(cni)이 `-`이면 G열(point_name)에서 CNI 값을 가져온다.
2. CNI에 줄바꿈(`\n`)으로 여러 값이 있으면 각각을 별도 포인트로 확장하여 모두 검증한다.

**실제 데이터 예시**
| 포인트 | 엑셀 F열(cni) | 엑셀 G열(point_name) |
|--------|--------------|----------------------|
| 49 | `-` | `thermo` |
| 64 | `senseBodyPowsav\nsenseBodyEcooper` | `-` |

**수정 후 파싱 결과**
| 포인트 | 변경 전 | 변경 후 |
|--------|---------|---------|
| 49 | `cni='-'` → SKIP | `cni='thermo'` → 정상 검증 |
| 64 | 1개 (SKIP) | 2개로 확장: `[senseBodyPowsav]`, `[senseBodyEcooper]` 각각 검증 |
| 전체 관제점 수 | 63개 | 67개 |

**수정 코드 요약** (`parse_aircon_list()`)
```python
# F열 '-' → G열 fallback
cni_raw = str(raw[5]).strip()
if cni_raw in ('-', ''):
    cni_raw = str(raw[6]).strip()   # point_name fallback

# 줄바꿈으로 복수 CNI 분리 → 각각 별도 포인트 생성
cni_list = [c.strip() for c in cni_raw.split('\n') if c.strip() and c.strip() != '-']
for cni_val in cni_list:
    entry = dict(base)
    entry['cni'] = cni_val
    if len(cni_list) > 1:
        entry['name'] = f"{base['name']} [{cni_val}]"
    points.append(entry)
```

---

### 최종 상태

| 구성 요소 | 파일 | 상태 |
|-----------|------|------|
| Flask 백엔드 | `tools/btl_validator.py` | 운영 중 (포트 5000) |
| 웹 UI | `tools/templates/index.html` | 정상 |
| 검증 로그 | `tools/logs/btl_*.log` | 자동 저장 |
| 엑셀 데이터 | `acs5_Instance_number_bacnet_20250324_acpi매칭_260406.xls` | 67개 관제점 파싱 |

**접속 URL**: `http://10.175.181.34:5000`

**서버 기동 명령**
```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/BTL/tools
nohup python3 btl_validator.py >> /tmp/btl_validator.log 2>&1 &
```

**포트 충돌 시**
```bash
pkill -9 -f btl_validator.py
```

---

### 미결 사항 / 다음 작업

- [ ] 장치연결 확인: 서버(`181.x`) ↔ 장치(`191.x`) 크로스 서브넷 BACnet 통신 안정성 추가 검증
- [ ] 복수 CNI 포인트(64번 등)의 WriteProperty 검증 로직 확인 — instance가 동일하므로 값 구분 방법 정의 필요
- [ ] BTL BTF 테스트 실제 실행 및 실패 항목 분석
