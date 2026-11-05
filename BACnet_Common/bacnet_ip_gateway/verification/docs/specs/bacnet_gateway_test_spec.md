# BACnet/IP Gateway Test Case Specification

| 항목 | 내용 |
|---|---|
| 문서 버전 | v1.1.0 |
| 개정일 | 2026-09-02 |
| 적용 대상 | BACnet/IP Gateway, vport, Main simulator 및 verification Runner |
| 요구사항 원문 | `verification/docs/specs/bacnet_ip_gateway_srs.docs` |
| 실행 기준 문서 | `verification/docs/specs/bacnet_gateway_test_spec.md` |
| 배포 문서 | `verification/docs/specs/bacnet_gateway_test_spec.md` |

이 문서는 QE/QA가 시험 목적, 입력값, 수행단계, 자동 판정 방법과
검증 공백을 한 번에 확인할 수 있도록 정리한 시험 사양서다. Runner는 10장 앞의
요약표에서 TC ID, SRS ID, 제목을 읽으므로 요약표는 실행 카탈로그이기도 하다.
요약표에는 정확히 94개의 TC가 있어야 하며, 10장 이후의 반복된 TC ID는 절차와
증적 설명으로만 사용한다.

이 문서는 제품 코드나 Runner의 동작을 바꾸지 않는다. 카드의 `bacnet_gateway_test_spec.md 기대 결과`는 SRS가
요구하는 결과이고, `현재 자동 판정`과 `직접 검증하지 않는 항목/검증 공백`은 현재
`verification/scripts` 구현이 실제로 확인하는 범위를 나타낸다. 두 내용이 다르면
현재 실행 결과를 PASS로 해석하지 말고 공백을 별도 이슈로 기록한다.

## 1. 목적과 읽는 방법

### 1.1 문서의 구분

- **요구사항 기대**: SRS에 정의된 제품 동작이다.
- **실행 절차**: 시험자가 준비해야 할 환경과 입력, 순서다.
- **자동 판정**: 자동 검증 프레임워크 (`tc_runner.py`)가 실제로 비교하는 expected/actual assertion이다.
- **검증 공백**: 요약 기대에는 포함되지만 현재 자동 검증 프레임워크 (`tc_runner.py`)가 직접 assertion하지 않는 부분이다.

따라서 adapter가 `SUCCESS`를 출력했다는 사실과 BACnet APDU의 각 필드가 표준에
맞다는 사실은 같은 의미가 아니다. 카드에서 두 범위를 명시적으로 구분한다.

### 1.2 결과 상태

| 상태 | 의미 | QA 해석 |
|---|---|---|
| `PASS` | 카드의 assertion과 정리가 모두 통과 | 현재 자동 범위 안에서 합격 |
| `FAIL` | expected/actual 불일치, 예외 또는 정리 실패 | 결함 또는 시험 환경 문제를 증적과 함께 분석 |
| `BLOCKED` | 필요한 실행 파일, 옵션, 장비 또는 승인 조건이 없음 | PASS가 아님. 누락된 선행조건을 기록 |
| `SKIP` | 명시적으로 제외한 실행 | 커버리지 PASS로 계산하지 않음 |

## 2. 공통 환경과 실행 규칙

### 2.1 실행 진입점과 모드

모든 카드는 다음 Runner를 기준으로 한다.

```bash
python3 verification/scripts/run_gateway_tcs.py
```

| 모드 | 환경 | 허용되는 변경 |
|---|---|---|
| `managed` | 자동 검증 프레임워크 (`tc_runner.py`)가 임시 workspace, fixture 설정, PTY, vport, Main simulator와 결과 디렉터리를 생성 | 자동 검증 프레임워크 (`tc_runner.py`)가 만든 프로세스·파일만 변경 및 정리 |
| `live` | 지정한 장비/프로세스/설정을 대상으로 실행 | 기본적으로 외부 프로세스와 설정을 재기동·변경하지 않음 |

`managed`에서 Gateway를 사용하는 TC는 `gateway_bin`, `vport_bin`,
`verification/scripts/gateway_server.sh`가 필요하다. 프레임 단위 vport TC는
별도의 `vport_test` 실행 파일을, mapping 단위 TC는 host unit-test 실행 파일을
필요로 한다. 필요한 의존성이 없으면 해당 TC만 `BLOCKED`가 된다.

### 2.1.1 설정 디렉터리 선택

| 실행 조건 | 실제로 사용하는 설정 |
|---|---|
| `live`, `--config-dir` 없음 | 프로젝트 기본 `conf/` |
| `live`, `--config-dir <dir>` 지정 | 지정한 디렉터리 |
| `managed`, 일반 TC | 자동 검증 프레임워크 (`tc_runner.py`)가 생성한 `fixture-conf/` |
| `managed`, `TC-BTL-004` 또는 `TC-BTL-006` | `--config-dir`의 `gateway.json`과 `profiles/*.json`을 복사한 `full-conf/`; 유효한 `--config-dir`가 없으면 `BLOCKED` |

`managed`에서 `--config-dir`를 지정했다고 해서 일반 TC가 그 전체 profile을
사용하는 것은 아니다. 전체 profile을 실제로 사용하는 TC는 BTL-004와 BTL-006이다.
선택 결과는 각 `results.json`의 `details.config_selection`에 기록된다.

### 2.1.2 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)의 기본 입력

자동 검증 프레임워크 (`tc_runner.py`)가 생성하는 fixture는 8개 object다. `--device-id`의 기본값은 `1200`,
BACnet/IP 포트의 기본값은 `47808`, Modbus unit ID는 `1`이다.

| 이름 | BACnet type/instance | `full_address` | 주요 입력 |
|---|---|---:|---|
| `TC_AI` | AI / 1 | 30001 | `INPUT_REGISTER`, `UINT16`, 범위 0~100 |
| `TC_AO_JSON_IN` | AO / 2 | 40001 | `HOLDING_REGISTER`, 범위 10~90, `Relinquish_Default=50` |
| `TC_AO_JSON_LOW` | AO / 3 | 40002 | 범위 10~90, `Relinquish_Default=-1` |
| `TC_AO_JSON_HIGH` | AO / 4 | 40003 | 범위 10~90, `Relinquish_Default=101` |
| `TC_AO_MAIN` | AO / 5 | 40004 | 범위 10~90, JSON 기본값 생략 |
| `TC_AO_MAIN_HIGH` | AO / 8 | 40005 | 범위 10~90, JSON 기본값 생략 |
| `TC_BI` | BI / 6 | 10001 | `DISCRETE_INPUT` |
| `TC_BV` | BV / 7 | 1 | `COIL`, `Relinquish_Default=0` |

기본 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)은 vport 3 Master, Main simulator, holding address 3=`40`,
address 4=`110`으로 시작한다. Gateway는 초기 동기화가 끝난 뒤
`BACnet IP Gateway started` 로그를 출력한다.

### 2.1.3 주요 옵션

| 옵션 | 사용하는 카드 | 의미 |
|---|---|---|
| `--long` | `TC-CFG-006/008`, `TC-OBS-001/003`, `TC-BAC-012/015`, `TC-PERF-*`의 장시간 변형 | 60초 대기 또는 600초 관찰과 같은 장시간 경로를 활성화 |
| `--allow-live-mutation` | live의 WriteProperty, DCC, SubscribeCOV TC | 외부 장비 상태 변경을 명시적으로 승인 |
| `--bbmd-endpoint HOST:PORT` | `TC-BIP-002` | BBMD endpoint를 지정 |
| `--bbmd-auto` | `TC-BIP-002` | 자동 검증 프레임워크 (`tc_runner.py`)가 `127.0.0.1:47809`의 embedded BBMD server를 시작 |
| `--config-dir <dir>` | full profile, live 입력 | 설정 디렉터리 지정 |
| `--tpi <file>` | `TC-BTL-004/006` | authoritative EPICS/TPI 입력 지정 |
| `--pics-doc <file>` | `TC-BTL-003` | checklist/PICS 문서 후보를 우선 지정 |
| `--python-bin <file>` | adapter 및 EPICS TC | 검증기에 사용할 Python interpreter 지정 |
| `--unit-test-bin <file>` | `TC-MAP-004/005/006` | host unit-test 실행 파일 지정 |
| `--vport-test-bin <file>` | `TC-VPORT-004..009` | 독립 vport 시험 실행 파일 지정 |
| `--allow-build` | `TC-ENV-001` | host `make test` 실행을 명시적으로 승인. `makeIMX`는 항상 금지 |

### 2.2 공통 단계와 visible step

Runner의 실제 TC lifecycle은 다음 순서다.

1. `TC 시작`을 기록한다.
2. TC별 선행조건과 실행 파일을 점검한다. 부족하면 해당 TC를 `BLOCKED`로 기록한다.
3. managed/live에 맞는 설정을 선택하고, 필요한 경우 fixture/fault를 준비한다.
4. 프로세스와 통신 종단을 시작한 뒤 실제 시험 요청을 전송한다.
5. assertion마다 expected/actual을 기록하고, 실패하면 해당 TC의 후속 assertion을 중단한다.
6. `clear_faults`, 프로세스 종료, fixture 복원 등 정리를 시도한다.
7. `TC 판정`을 기록한다.

`steps.log`의 단계 수는 위 lifecycle의 환경 단계, 실제 assertion, 정리를 합한
결과다. 문서에 적힌 정상 단계 수를 합격 조건으로 사용하지 않는다. 한 assertion이
실패하면 후속 단계가 기록되지 않으므로 실패 실행의 step 수가 정상 실행보다 작을
수도 있다. 실제 step 수와 expected/actual은 실행 결과의 `steps.log`, `steps.csv`,
`results.json`을 기준으로 한다.

### 2.3 채널 구분

| 채널 | 사용 목적 | 해석 방법 |
|---|---|---|
| `Unix control socket` | Main simulator memory/fault 설정·조회, ping, cleanup | JSON `ok`/`count`는 simulator 제어 결과다. Modbus ACK나 BACnet SimpleACK가 아니다. |
| `Modbus RTU` | Gateway↔vport↔Main simulator | function code, unit, 주소, payload, CRC, timeout과 transaction을 판정 |
| `BACnet/IP` | client↔Gateway UDP | Who-Is/I-Am, ReadProperty, WriteProperty, COV, DCC 및 Error/ACK APDU를 판정 |
| `Gateway process/log` | 기동, HOLD, reconnect, 종료 상태 | 기능 packet과 혼동하지 않고 process/log assertion으로 판정 |
| `host unit/adapter` | 독립 테스트 또는 외부 검증기 | 반환코드와 출력 marker만 현재 자동 검증 프레임워크 (`tc_runner.py`)가 직접 판정하는지 확인 |

### 2.4 공통 증적과 판정

기본 결과 디렉터리는 `verification/results/<UTC timestamp>/`다. 실행 명령, 선택된
설정, dependency 결과, 프로세스 로그, packet trace와 TC별 결과를 함께 보관한다.
카드의 `증적` 항목은 최소 보관 목록이며, 실패 시 관련 raw log와 packet을 추가한다.

## 3. 설정 및 기동 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-CFG-001 | SRS-FUN-CFG-001 | 유효한 최소 프로필 | 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`) loader로 object/instance 검사 | 전체 로드 성공, 정확한 mapping 생성 |
| TC-CFG-002 | SRS-FUN-CFG-001 | JSON 문법 오류/필수 주소 누락 | 두 오류 profile로 Gateway를 각각 기동 | ConfigHold 진입 및 생존, 오류 진단 로깅, 서비스 미시작 |
| TC-CFG-003 | SRS-FUN-CFG-001 | 중복 instance와 범위 밖 주소 | 잘못된 profile로 Gateway 기동 | ConfigHold 진입 및 생존, 충돌/범위 진단 로깅, 서비스 미시작 |
| TC-CFG-004 | SRS-FUN-CFG-002 | `min < max` | AO #2 loader 검증 | 유효 설정과 default 유지 |
| TC-CFG-005 | SRS-FUN-CFG-002 | `min == max` | AO #2 clamp 검증 | 경계값으로 보정 |
| TC-CFG-006 | SRS-FUN-CFG-002 | `min > max`, 이후 파일 수정 | `--long` HOLD 후 복구 | 오류 문맥, reload, 한 번의 정상 기동 |
| TC-CFG-007 | SRS-FUN-CFG-003 | 모순 설정 상태 | HOLD process 관찰 | process 생존, BACnet 미시작 |
| TC-CFG-008 | SRS-FUN-CFG-003 | 2회 실패 후 복구 | `--long` 반복 HOLD/reload | 중복 없이 한 번만 활성화 |
| TC-CFG-009 | SRS-FUN-CFG-003 | 홀드 시작 직후 SIGTERM | 종료시간 측정 | 1초 이내 종료, 자원 잔류 없음 |
| TC-START-001 | SRS-FUN-START-001 | vport→Main→Gateway 순차 기동 | 가상 시리얼 포트, Main 시뮬레이터, Gateway 순서 기동 | 프로세스 기동 성공 및 BACnet 서비스 초기화 완료 |
| TC-START-002 | SRS-FUN-START-001 | Gateway 선기동 | Gateway 기동 후 0.3초 대기, 이후 vport/Main 기동 | 조기 종료 없이 정상 기동 진행 |
| TC-START-003 | SRS-FUN-START-001 | Main 지연 기동 | vport/Gateway 기동 후 0.8초 지연, 이후 Main 기동 | Main 기동 전 BACnet 미공개, 이후 동기화 완료 |
| TC-START-004 | SRS-FUN-START-002 | JSON 기본값 적용 및 Min/Max 제한 | 설정 파일 적용된 Modbus 초기값 설정 후 AO 2/3/4 속성 검증 | 제한된 범위(50/10/90)가 Main 장비 및 RD/PV에 적용됨 |
| TC-START-005 | SRS-FUN-START-002 | JSON 기본값 생략, Main 값(40) 범위 내 | 기본값 생략 조건에서 AO 5 동기화 | Main 값이 유지되며 불필요한 쓰기 없이 객체에 동기화됨 |
| TC-START-006 | SRS-FUN-START-002 | JSON 기본값 생략, Main 값(0/110) 경계 밖 | 기본값 생략 조건에서 AO 5/8 동기화 | 경계값(10/90)으로 보정되어 Main 및 객체에 적용됨 |
| TC-START-007 | SRS-FUN-START-003 | 첫 객체 Write(FC06) 타임아웃 | 초기 동기화 중 FC06 응답 타임아웃 2회 주입 | 재시도 중 BACnet 서비스 미공개, 성공 후 단 1회 기동 |
| TC-START-008 | SRS-FUN-START-003 | 중간 객체 동기화 타임아웃 후 재진행 | 성공 객체 이후 타임아웃 1회 주입 | 실패 객체부터 재시도하며 기존 성공 객체 중복 쓰기 없음 |
| TC-START-009 | SRS-FUN-START-003 | 첫 객체 Write(FC05) 타임아웃 | 초기 동기화 중 FC05 응답 타임아웃 2회 주입 | 재시도 중 BACnet 서비스 미공개, 성공 후 단 1회 기동 |

## 4. vport TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-VPORT-001 | SRS-FUN-VPORT-001 | Master 1/3/5 설정 기동 | 가상 시리얼 포트 에뮬레이터(`vport`) 실행 후 PTY 링크 파일 확인 | 지정한 수의 Master 링크(`ttyMASTER*`) 및 단일 Slave 링크(`ttySLAVE`) 생성 확인 |
| TC-VPORT-002 | SRS-FUN-VPORT-001 | vport 정상 SIGTERM 수신 | SIGTERM 전송 후 링크(PTY) 파일 정리 확인 | 스레드 종료 및 모든 PTY 심볼릭 링크 정상 삭제 확인 |
| TC-VPORT-003 | SRS-FUN-VPORT-001 | 강제 종료(SIGKILL) 후 재기동 | 강제 종료 후 PTY 링크 상태 확인 및 새 프로세스의 링크 할당 | 기존 stale 링크 정리 및 새로운 `ttySLAVE` 종단 정상 제공 |
| TC-VPORT-004 | SRS-FUN-VPORT-002 | 1개의 요청 프레임을 2~3조각으로 분할 수신 | `vport_test` C++ 단위 테스트 바이너리 실행 | 버퍼에 조합된 완성 프레임만 Main 방향으로 1회 전달 |
| TC-VPORT-005 | SRS-FUN-VPORT-002 | CRC 오류 프레임 수신 및 Guard 초과 버퍼 발생 | `vport_test` C++ 단위 테스트 바이너리 실행 | 오류 선행 프레임 폐기 및 후속 정상 프레임만 분리 전달 |
| TC-VPORT-006 | SRS-FUN-VPORT-002 | 3개의 Master 포트에서 동시 요청 전송 | `vport_test` C++ 단위 테스트 바이너리 실행 | 바이트 인터리빙(Interleave) 방지 및 프레임 직렬 처리 큐잉 |
| TC-VPORT-007 | SRS-FUN-VPORT-003 | Master별 다른 모드버스 주소 요청 | `vport_test` C++ 단위 테스트 바이너리 실행 | Slave 응답이 원발신 Master 포트로만 정확하게 라우팅 |
| TC-VPORT-008 | SRS-FUN-VPORT-003 | 표준 FC06 응답 및 6바이트 비표준 FC06 응답 발생 | `vport_test` C++ 단위 테스트 바이너리 실행 | 길이에 무관하게 두 응답 형태 모두 해당 Master에 완벽히 라우팅 |
| TC-VPORT-009 | SRS-FUN-VPORT-003 | Slave 타임아웃, 불완전 응답 및 불일치 응답 직후 정상 요청 | `vport_test` C++ 단위 테스트 바이너리 실행 | 오류/타임아웃 프레임은 폐기되고, 직후 다른 Master의 정상 응답은 안전하게 전달 |
| TC-VPORT-010 | SRS-FUN-VPORT-001 | PTY 슬레이브 open→close 후 CPU 점유율 측정 | `vport_test` C++ 단위 테스트 바이너리 실행 | 슬레이브 참조 유지로 EIO 스핀 방지 및 CPU ≤ 5% 정상 대기 유지 |
| TC-VPORT-011 | SRS-IF-CFG-001 | 마스터 상한 초과 기동 거부 | `vport.cfg`의 MAX_MASTERS 초과 인수로 vport 실행 | 에러 메시지 출력 및 비정상 종료(exit code ≠ 0) 확인 |
| TC-VPORT-012 | SRS-FUN-VPORT-002 | 요청 큐 상한 초과 시 Modbus Exception 응답 | `vport_test` C++ 단위 테스트 바이너리 실행 | 초과분에 대해 Server Device Busy(0x06) Exception Response 회신 및 후속 요청 정상 처리 |
| TC-VPORT-013 | SRS-FUN-VPORT-002 | 비표준 FC 프레임 거부 및 Exception 프레임 정상 라우팅 | `vport_test` C++ 단위 테스트 바이너리 실행 | 비표준 FC 무시 및 Exception 응답의 원발신 마스터 정상 라우팅 확인 |
| TC-VPORT-014 | SRS-IF-CFG-001 | 명시적 --config 경로의 설정 파일 미발견 시 에러 종료 | 존재하지 않는 경로를 `--config`에 지정하여 vport 실행 | 에러 메시지 출력 및 즉시 비정상 종료(exit code ≠ 0) 확인 |
| TC-VPORT-015 | SRS-FUN-VPORT-003 | vport vs Gateway 타임아웃 계층 정합성 검증 | verification 러너 설정 파일 파싱 및 비교 | Gateway rtu_timeout_ms >= vport DEFAULT_SLAVE_TIMEOUT_MS 관계 검증 (역전 시 FAIL) |
## 5. Mapping 및 Modbus 인터페이스 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-MAP-001 | SRS-FUN-MAP-001 | AI/AO/BI/BV 명시 타입 선언 검사 | 설정 파서(`config_loader.py`)로 Object Type 집합 확인 | 4가지 필수 Type 모두 파싱됨 |
| TC-MAP-002 | SRS-FUN-MAP-001 | 속성에 따른 `bacnet_type` 자동 추론 | 5가지 Modbus 속성 조합으로 `infer_bacnet_type` 함수 단위 테스트 | 규칙에 맞는 Type으로 각각 정확히 추론됨 |
| TC-MAP-003 | SRS-FUN-MAP-001 | 런타임 자동 할당된 인스턴스의 재현성 | 동일 설정 파일을 2회 반복 파싱 후 인스턴스 배열 비교 | 할당된 인스턴스 번호가 고유하고 멱등성을 유지함 |
| TC-MAP-004 | SRS-FUN-MAP-002 | INT16/UINT16/FLOAT32 타입 및 Scale/Offset 변환 | 호스트 C++ 단위 테스트(`run_tests`) 실행 및 마커 확인 | C++ 변환 로직 및 프로세스 정상 종료 통과 |
| TC-MAP-005 | SRS-FUN-MAP-002 | Byte/Word Swap 및 Min/Max 범위 Clamp 적용 | 호스트 C++ 단위 테스트(`run_tests`) 실행 및 마커 확인 | C++ 스왑 및 경계값 제한 로직 정상 종료 통과 |
| TC-MAP-006 | SRS-FUN-MAP-002 | 특정 비트(BIT) 추출, 패킹 및 BV `Relinquish_Default` | 호스트 C++ 단위 테스트(`run_tests`) 실행 및 마커 확인 | 비트 처리 로직 및 기본값 복귀 로직 정상 종료 통과 |
| TC-MAP-007 | SRS-FUN-MAP-003 | 입력 객체(AI/BI) 대상 쓰기 차단 | 읽기 전용 객체에 BACnet WriteProperty 요청 전송 | `BACNET_ERROR_PDU` 거부 및 하위 Modbus 프레임 미발생 |
| TC-MAP-008 | SRS-FUN-MAP-003 | AO 객체 설정 Min 미만/Max 초과 쓰기 차단 | `bacnet_profile_tester.py` 실행하여 범위를 벗어난 쓰기 요청 | `VALUE_OUT_OF_RANGE` 오류 반환 및 하위 Modbus 보호 확인 |
| TC-MAP-009 | SRS-FUN-MAP-003 | 수동 기입된 중복 인스턴스 번호 및 주소 오류 거부 | 비정상 설정 파일로 Gateway 프로세스 기동 시도 | ConfigHold 진입 및 생존, 오류 감지 및 BACnet 미시작 |
| TC-MOD-001 | SRS-IF-MOD-001 | FC01/02/03/04 표준 읽기 응답 무결성 | vport 및 Main 시뮬레이터 연동 스택 기동 후 4대 블록 읽기 | BACnet 값 일치, FC01~04 프레임 및 vport CRC16 무결성 검증 |
| TC-MOD-002 | SRS-IF-MOD-001 | FC05/06/16 단일/다중 쓰기 적용 | BACnet 쓰기를 통한 FC05/06/16 프레임 발행 및 vport 중계 | vport 에코 응답 CRC16 무결성 및 시뮬레이터 메모리 반영 |
| TC-MOD-003 | SRS-IF-MOD-001 | CRC 오류 폐기, Exception 및 One-shot Timeout 주입 | 실시간 Fault 주입(Timeout, Exception 0x83, Bad CRC) | 오류 감지/폐기, BACnet 차단 및 후속 요청 정상 복구 |

## 6. BACnet/IP 기능 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-BAC-001 | SRS-FUN-BAC-001 | 범위 없는 Who-Is/I-Am | native UDP Who-Is/I-Am | Device ID, APDU, segmentation, vendor와 sender 일치 |
| TC-BAC-002 | SRS-FUN-BAC-001 | 범위 내/밖 Who-Is | 두 bounded Who-Is | 범위 내만 I-Am |
| TC-BAC-003 | SRS-FUN-BAC-001 | Who-Has 이름/ID 및 Object List | Who-Is와 첫 object Who-Has | I-Have binding 일치 |
| TC-BAC-004 | SRS-FUN-BAC-002 | AI/BI/AO/BO Present Value | 각 object ReadProperty 85(Present_Value) | status/invoke/type/value 일치 |
| TC-BAC-005 | SRS-FUN-BAC-002 | 정적/배열 속성과 Object List index | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | count/요소/APDU 정책 일치 |
| TC-BAC-006 | SRS-FUN-BAC-002 | unknown object/property/index | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 표준 Error/Abort와 transaction 종료 |
| TC-BAC-007 | SRS-FUN-BAC-003 | Priority 8 우선 및 하위 쓰기 방어 | native PV/CCP read/write | Priority 8 적용 및 16 쓰기 후에도 8 유지 |
| TC-BAC-008 | SRS-FUN-BAC-003 | Priority 8 해제 후 Priority 16 인계 | native priority mutation | Priority 16 fallback, CCP=16 |
| TC-BAC-009 | SRS-FUN-BAC-003 | 모든 priority slot NULL 시 Relinquish Default 복귀 | native priority mutation | Relinquish_Default 복귀, CCP=NULL |
| TC-BAC-010 | SRS-FUN-BAC-004 | COV_Increment 유효/무효 WriteProperty | property 22(COV_Increment), SubscribeCOV, source 변화 | 유효 ACK, 0 Error, 경계별 COV |
| TC-BAC-011 | SRS-FUN-BAC-004 | Binary 상태 변경/무변경 | BO 7 COV 관찰 | 변경 시만 알림 |
| TC-BAC-012 | SRS-FUN-BAC-004 | 구독 생성/갱신/취소 및 `--long` lifetime 만료 | Active_COV_Subscriptions + `--long` | active count와 lifetime 만료 일치 |
| TC-BAC-013 | SRS-FUN-BAC-005 | DCC Disable | Disable→ReadProperty→Enable | Disable 중 성공 응답 억제, Enable ACK |
| TC-BAC-014 | SRS-FUN-BAC-005 | DCC Disable Initiation | COV 억제와 ReadProperty 관찰 | 신규 COV 억제, 수신 요청은 허용 |
| TC-BAC-015 | SRS-FUN-BAC-005 | timeout 만료 및 명시 해제 | `--long`, 61초 후 ReadProperty | timer 만료 후 응답 복구 |
| TC-BAC-016 | SRS-FUN-BAC-006 | Device/Network Port 필수 속성 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 선언 속성과 실제 값 일치 |
| TC-BAC-017 | SRS-FUN-BAC-006 | Network Port read-only 속성 Write | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 오류, 상태 불변, block 없음 |
| TC-BAC-018 | SRS-FUN-BAC-006 | 미지원 서비스 3종 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 선언된 표준 오류 |
| TC-BAC-019 | SRS-FUN-BAC-004 | COV 최대 동시 구독 용량(20개) 및 한계 초과 거부 | native SubscribeCOV capacity | 20개 성공, 21번째 오류 거부, 취소 후 슬롯 복원 |
| TC-BAC-020 | SRS-FUN-BAC-006 | Network Port FD 설정 런타임 변경 및 에러 응답 | 런타임 WriteProperty(408, 418, 419) 및 외부 Command(417) 쓰기 | 유효값 자동 재시작 및 FDT 반영, 잘못된 값(BBMD모드, TTL=0, IP 0.0.0.0) VALUE_OUT_OF_RANGE, Command UNKNOWN_PROPERTY 거부 |

## 7. 실행 및 BACnet 인터페이스 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-IFCFG-001 | SRS-IF-CFG-001 | 설정 디렉터리 명시 및 기본 경로 | `main.cpp` 소스 계약 검사 및 커스텀/미존재 디렉터리 기동 | 기본 경로 `./conf` 지정 및 `argv[1]` 인식, 미존재 경로 ConfigHold 진입 |
| TC-IFCFG-002 | SRS-IF-CFG-001 | 통신/캐시(serial, COV, cache) 설정 경계 적용 | 6종 설정 변형(variant)에 따른 통합 환경 재기동 | 각 단위 값이 시작 로그에 정상 반영되는지 확인 |
| TC-IFCFG-003 | SRS-IF-CFG-001 | 가상 포트(vport) 종단 수 및 타임아웃 로깅 | 1/3 Master 링크 모드 재기동 및 타임아웃 출력 확인 | 명시된 종단 수 심볼릭 링크(ttyMASTER) 및 타임아웃 설정 일치 |
| TC-BIP-001 | SRS-IF-BIP-001 | BACnet/IP 기본 UDP 포트(47808) | 타겟 제한 없는 Who-Is 탐색 요청 | 설정된 주소 및 포트에서 정상 I-Am 응답 확인 |
| TC-BIP-002 | SRS-IF-BIP-001 | 독립 네트워크 모드(Normal / FD) 기동 및 BBMD 등록 유지 | bip.cfg 설정에 따른 Normal/FD 독립 기동, BBMD FDT 등록, BBMD 장애 감지/복구 로그 및 300초 상한 주기 재등록 검증 | 설정 모드 일치(IP_Mode=0/1), FDT 엔트리 확인, Normal 모드 등록 없음, BBMD 단절/복구 로그 확인, 5분 주기 재등록 확인 |
| TC-BIP-003 | SRS-IF-BIP-001 | 지원 서비스 및 객체(service/object bit) 검증 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 활용 | 광고된 기능 비트(bit)와 실제 수행 동작 간의 일치 여부 확인 |

## 8. 비기능 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-AVL-001 | SRS-NFR-AVL-001 | 런타임 vport 재시작 | 강제종료→재기동→ReadProperty 상태 폴링 | 자동 reopen과 복구 응답성 확인 |
| TC-AVL-002 | SRS-NFR-AVL-001 | Gateway 강제 중지 | Main 시뮬레이터/vport 프로세스 생존 확인 | Gateway 의존성 없는 타 프로세스 독립 실행 유지 |
| TC-AVL-003 | SRS-NFR-AVL-001 | 정상 종료 소요 시간 확인 | SIGTERM 후 Gateway 종료(stop) 시간 측정 | 3.0초 이내 완전 종료 |
| TC-AVL-004 | SRS-NFR-AVL-001 | 12시간 전체 관제점 장기 연속 운용 신뢰성 | 전체 관제점 주기적 Read/Write, 무중단 생존성 및 메모리/FD 누수 감시 | 12시간 무중단(No Crash), 통신 성공률 >= 99.9%, 메모리 누수 억제(< 5%), FD 누수 0 |
| TC-PERF-001 | SRS-NFR-PERF-001 | 순차 ReadProperty(100회) 벤치마크 | 승인 정책(performance_policy)과 결과 지표 대조 | 지연시간(P95/P99) 및 처리량(throughput) 기준 충족 |
| TC-PERF-002 | SRS-NFR-PERF-001 | COV 백그라운드 부하 중 R/W 포그라운드 처리 | 100ms 주기 값 변경 상태에서 10초간 R/W 교차 전송 | 포그라운드 사용자 요청의 기아(starvation) 방지 및 정상 응답 |
| TC-PERF-003 | SRS-NFR-PERF-001 | Main 타임아웃 주입 및 메시지 큐 포화 | 병렬 워커(16개) 집중 R/W를 통한 큐 포화 및 타임아웃 대기(최대 20초) 관찰 | NO_SPACE_TO_WRITE_PROPERTY 에러 출력 및 결함 해제 후 복구 확인 |
| TC-OBS-001 | SRS-NFR-OBS-001 | Min/Max 범위 모순 시 HOLD 진단 로그 | 설정 오류 시 HOLD 로그(60초 대기) 출력 확인 | 에러 문맥 및 60초 진단 로그 확인 |
| TC-OBS-002 | SRS-NFR-OBS-001 | 시작(startup) 트레이스 로그 식별 | 통합 환경 기동 후 초기화 단계별 토큰(Relinquish/clamp/Synchronized) 검사 | 초기화 데이터 흐름 식별 |
| TC-OBS-003 | SRS-NFR-OBS-001 | 장기(600초) Main 무응답 안정성 관찰 | 1초 주기 PID/로그 샘플링 및 2초 주기 BACnet 상태 프로브 송신 | Gateway 생존 유지, 로그 폭주 억제(<=20 lines/s), 3초 내 정상 종료 |
| TC-NET-001 | SRS-NFR-NET-001 | 링크 DOWN→UP(같은 IPv4) 복구 | netns 게이트웨이 인터페이스(vgw0) 10초 DOWN 후 UP, 0.5초 간격 단발 ReadProperty 폴링 | 재시작 없이 UP 후 10초 이내 응답 재개, Reliability=NO_FAULT, Link down/up 로그 |
| TC-NET-002 | SRS-NFR-NET-001 | 운용 중 IPv4 변경 추종 | vgw0 IPv4 를 10.77.0.2 → 10.77.0.3 으로 교체 | IPv4 changed 로그, 재시작 1회로 새 IPv4/실제 netmask 재바인딩, 새 IPv4 응답·IP_Address·서브넷 broadcast I-Am 일치, 옛 IPv4 무응답 |
| TC-NET-003 | SRS-NFR-NET-001 | 기동 시 네트워크 미준비(IPv4 미할당) | IPv4 없이 기동 후 10초 이상 대기, 이후 IPv4 할당 | exit 하지 않고 Waiting for network 로그로 대기, IPv4 할당 후 15초 이내 기동 및 응답 |
| TC-NET-004 | SRS-NFR-NET-001 | FD 모드 링크 복구 후 BBMD 재등록 | TTL 600초 FD 등록 상태에서 vgw0 15초 DOWN 후 UP | UP 후 10초 이내 Register-Foreign-Device 재등록(300초 renew 대기 없음), FD 즉시 재등록 로그 |

## 9. 표준 및 환경 TC 요약

| TC ID | SRS ID | 시나리오/입력 | 핵심 실행 | 요구사항 기대 |
|---|---|---|---|---|
| TC-BTL-001 | SRS-CON-BTL-001 | 선언 BIBB 기본 서비스 | `tc_cases.py` 스크립트를 통한 BACnet IP Who-Is/ReadProperty 패킷 결과 대조 | 선언 BIBB 결과 PASS, 선행조건 부족은 BLOCKED |
| TC-BTL-002 | SRS-CON-BTL-001 | BTL Device/Network Port 조항 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 반환 코드 및 SUCCESS 마커 확인 |
| TC-BTL-003 | SRS-CON-BTL-001 | PICS/TPI/JSON 3자 전수 대조 감사 | PICS checklist, TPI 전자명세서, Gateway 설정(JSON) 전수 교차 감사 | BIBB 6종, 지원 객체 6종(BV포함, BO제외), 미지원 59종 전수 배제, COV 4종, JSON 프로파일 수용성 일치 |
| TC-BTL-004 | SRS-CON-BTL-002 | EPICS 전체 객체/속성 | `btl_epics_verifier.py` 기반 EPICS 스캔 및 검증 | 모든 선언값과 live 값 일치 |
| TC-BTL-005 | SRS-CON-BTL-002 | Max APDU/Object List 경계 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | 1476/segmentation 정책 일치 |
| TC-BTL-006 | SRS-CON-BTL-002 | service/object bit와 protocol revision | `btl_epics_verifier.py` 기반 EPICS 스캔 및 검증 | 세 출처 완전 일치 |
| TC-ENV-001 | SRS-CON-ENV-001 | 호스트 compiler build | 승인된 경우에만 `make test` | 오류/경고 없음 |
| TC-ENV-002 | SRS-CON-ENV-001 | 지원 Linux에서 Gateway/vport smoke | 통합 테스트 환경 기동/종료 및 PTY 링크 정리 검사 | 플랫폼 API와 PTY 정상 |
| TC-ENV-003 | SRS-CON-ENV-001 | BACstac 공개 API/문서 감사 | 문서 우선 정적 API 감사 | 문서화된 공개 API, 상용 source 무단 수정 없음 |

## 10. TC별 상세 실행 카드

### 10.0 카드 읽는 법과 공통 절차

각 카드는 다음 순서로 읽는다.

1. **실행 명령**에서 공통 Runner 명령과 TC별 옵션을 확인한다.
2. **입력/사전조건**에서 fixture 값, fault, 외부 파일과 선행조건을 확인한다.
3. **수행단계**를 순서대로 실행한다. 채널 이름은 해당 단계의 실제 통신 수단이다.
4. **자동 판정** 표의 expected와 actual을 결과 파일에서 대조한다.
5. **검증 범위와 공백**을 보고 SRS 기대 전체를 현재 PASS로 확대 해석하지 않는다.
6. **정리/증적**에 따라 결과 디렉터리와 외부 장비 상태를 확인한다.

아래 카드에서 `기본 명령`은 다음과 같다.

```bash
python3 verification/scripts/run_gateway_tcs.py --suite all --case <TC-ID> --mode managed
```

live 관찰 TC는 `--mode live`를 사용한다. live에서 WriteProperty, DCC 또는
SubscribeCOV를 보내는 TC는 `--allow-live-mutation`이 없으면 `BLOCKED`다. 장시간
카드는 기본 명령에 `--long`, BIP-002는 `--bbmd-endpoint HOST:PORT` 또는
`--bbmd-auto`를 추가한다.

공통 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 단계는 다음과 같다.

- vport 3 Master를 시작하고 `ttyMASTER1..3`, `ttySLAVE` link를 확인한다.
- Main simulator를 시작하고 Unix control socket ping의 `ok=true`를 확인한다.
- 별도 입력이 없는 경우 holding address 3=`40`, address 4=`110`을 설정한다.
- Gateway를 시작하고 `BACnet IP Gateway started`를 확인한다.
- 종료 시 자동 검증 프레임워크 (`tc_runner.py`)가 만든 fault를 지우고 Gateway/Main/vport와 PTY를 정리한다.

위 ping/set은 시험용 환경 설정이고 Modbus/BACnet ACK가 아니다. startup 중의
Modbus RTU transaction은 simulator transaction log와 packet evidence에서 확인한다.

### 10.1 실제 단계별 실행 카드

#### TC-CFG-001 — 유효한 최소 프로필

- 실행 명령: 기본 명령. Gateway/vport/Main 기동은 하지 않는다.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)의 object 8개와 자동 생성 설정을 사용한다.
- 수행단계:
  1. 자동 검증 프레임워크 (`tc_runner.py`)가 fixture 설정을 만든다.
  2. 설정 loader를 실행해 `devices[0].objects`를 읽는다.
  3. object 수와 모든 `instance`의 유일성을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| object 수 | 설정 loader | `8` | 반환된 object 수가 8인지 assertion |
| instance 유일성 | 설정 loader | `true` | set 크기와 object 수 비교 |

- 현재 자동 검증 범위: 설정 파일 구조와 object/instance 수만 검사한다.
- 직접 검증하지 않는 항목/검증 공백: Gateway가 설정을 실제로 읽고 Modbus/BACnet 서비스를 시작하는 경로, 각 field의 모든 허용값은 직접 검사하지 않는다.
- 정리/증적: `results.json`, loader 출력, fixture checksum. 송수신 packet은 없다.
- BLOCKED 조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)를 만들 수 없는 경우.

#### TC-CFG-002 — JSON 문법 오류와 필수 주소 누락

- 실행 명령: 기본 명령.
- 입력/사전조건: fixture profile을 두 번 변형한다. 첫 번째는 `{ invalid json`, 두 번째는 첫 object의 `full_address` 삭제다.
- 수행단계:
  1. 잘못된 JSON으로 Gateway를 시작하고 ConfigHold 상태 진입(`entering HOLD`) 및 프로세스 생존, `Parse error` 로그를 확인한다.
  2. fixture를 복원한 뒤 필수 주소를 삭제하고 다시 Gateway를 시작하여 ConfigHold 진입 및 필수 주소 누락 진단 로그를 확인한다.
  3. 두 경우 모두 BACnet 서비스가 미기동됨을 확인하고 정상 종료한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| JSON 문법 오류 HOLD 생존 | Gateway process | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| JSON 문법 오류 진단 로그 | Gateway log | `Parse error` | 로그 내 문법 오류 진단 메시지 확인 |
| 문법 오류 중 BACnet 미기동 | Gateway log | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |
| 필수 주소 누락 HOLD 생존 | Gateway process | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| 필수 주소 누락 진단 로그 | Gateway log | `missing both 'offset_address' and 'full_address'` | 로그 내 필수 주소 누락 진단 확인 |
| 주소 누락 중 BACnet 미기동 | Gateway log | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |

- 현재 자동 검증 범위: 문법 오류 및 필수 주소 누락 시 Crash Loop 방지를 위해 ConfigHold 상태로 진입하고 프로세스가 생존하며 진단 로그를 남기는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 오류 메시지의 표준 문구, port bind 여부, Modbus/BACnet packet 미발생은 별도 assertion이 아니다.
- 정리/증적: 두 Gateway log, 변형 전후 fixture.
- BLOCKED 조건: managed Gateway 실행 파일 또는 Main wrapper가 없는 경우.

#### TC-CFG-003 — 중복 instance와 주소 범위 오류

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 내 객체 정보 조작 (객체2의 `instance`를 객체1과 동일하게 변경, 객체3의 `full_address`를 `999999`로 변경).
- 수행단계:
  1. 동일한 파일에 인스턴스 중복 오류와 주소 범위 초과 오류를 동시에 주입한다.
  2. 해당 설정 파일로 Gateway 프로세스를 시작한다.
  3. Gateway 프로세스가 즉시 종료되지 않고 ConfigHold 상태로 생존하며, 오류 진단 로그를 남기고 BACnet 서비스가 기동되지 않음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 중복 instance/범위 오류 HOLD 생존 | Gateway 프로세스 | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| 중복 instance/범위 진단 로그 | Gateway 로그 | `different block` / `exceeds` / `Duplicate instance` | 로그 내 주소/인스턴스 오류 진단 메시지 확인 |
| 오류 상태 중 BACnet 미기동 | Gateway 로그 | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |

- 현재 자동 검증 범위: 인스턴스 중복 및 Modbus 주소 범위 이탈이 포함된 설정이 파싱 단계에서 거부되어 Crash Loop 방지용 ConfigHold 상태로 진입하고 BACnet 서비스가 기동되지 않음을 검증.
- 직접 검증하지 않는 항목/검증 공백: 두 오류가 개별적으로 명시적 검출되는지, 오류 메시지 내에 문제가 발생한 객체 이름이 정확히 출력되는지는 검증하지 않음.
- 정리/증적: 조작된 설정 파일 기록, Gateway 실행 로그.
- BLOCKED 조건: Gateway 실행 파일을 찾을 수 없는 경우.

#### TC-CFG-004 — `min < max`와 Relinquish_Default

- 실행 명령: 기본 명령. (프로세스 기동 없음)
- 입력/사전조건: AO 객체 #2의 설정값을 `min_value=10`, `max_value=90`, `relinquish_default=20`으로 파일에 반영.
- 수행단계:
  1. 설정 파일에 위 조건의 값을 기록한다.
  2. 설정 파서 (`config_loader.py`)를 통해 파일을 로드한다.
  3. 파싱된 AO 객체 #2의 `relinquish_default` 속성값을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Relinquish_Default 값 유지 | 설정 파서 반환값 | `20.0` | 반환된 설정값이 변경 없이 20.0을 유지하는지 확인 |

- 현재 자동 검증 범위: `Relinquish_Default` 값이 정의된 `min_value`와 `max_value` 범위 내에 있을 경우, 값이 보정되지 않고 원본 그대로 유지되는 파싱 동작 검증.
- 직접 검증하지 않는 항목/검증 공백: 파싱 단계의 논리만 검증하며, Gateway 프로세스 실제 기동 후 BACnet Present_Value 동기화나 Modbus Write 동작은 검증하지 않음.
- 정리/증적: 설정 파싱 결과 데이터.
- BLOCKED 조건: 테스트용 설정 파서를 실행할 수 없는 경우.

#### TC-CFG-005 — `min == max` clamp

- 실행 명령: 기본 명령. (프로세스 기동 없음)
- 입력/사전조건: AO 객체 #2의 설정값을 `min_value=10`, `max_value=10`, `relinquish_default=20`으로 파일에 반영.
- 수행단계:
  1. 최소/최대값이 동일한 경계(10)를 설정 파일에 기록한다.
  2. 설정 파서 (`config_loader.py`)를 통해 파일을 로드한다.
  3. 파싱된 `relinquish_default` 속성값이 경계값 내로 Clamp(보정)되었는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Relinquish_Default Clamp 보정 | 설정 파서 반환값 | `10.0` | 반환된 설정값이 10.0으로 자동 보정되었는지 확인 |

- 현재 자동 검증 범위: `min == max` 조건이 성립할 때 범위 밖의 `Relinquish_Default`가 최솟값으로 강제 보정되는 파서 수준의 로직 검증.
- 직접 검증하지 않는 항목/검증 공백: 파싱 단계만 검증하며, 실제 BACnet 객체 생성 시의 속성 보정 동작은 다루지 않음.
- 정리/증적: 설정 파싱 결과 데이터.
- BLOCKED 조건: 테스트용 설정 파서를 실행할 수 없는 경우.

#### TC-CFG-006 — 모순 설정 후 파일 수정 및 reload

- 실행 명령: `--long` (자동 복구 대기를 위한 긴 테스트 옵션 포함).
- 입력/사전조건: AO 객체 #2의 설정값을 모순되게 `min_value=100`, `max_value=10`으로 설정. (`--long` 옵션이 없으면 `BLOCKED` 처리됨)
- 수행단계:
  1. 모순된 설정 파일을 저장한다.
  2. vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동한다.
  3. Gateway 프로세스가 에러로 종료되지 않고 `HOLD` (대기) 상태에 머무르는지 확인한다.
  4. 설정 파일을 정상 범위(`min_value=10`, `max_value=100`)로 덮어쓰기 복원한다.
  5. Gateway가 파일 갱신을 감지하고 설정을 다시 로드하여 정상적으로 기동하는지 대기 후 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 복구 후 정상 기동 | Gateway 실행 로그 | HOLD 상태에서 복구 및 기동 | `BACnet IP Gateway started` 로그 출력 대기 확인 |
| 에러 발생 로그 확인 | Gateway 실행 로그 | `FATAL CONFIG ERROR` 발생 | 해당 에러 로그 문자열 존재 확인 |
| 정상 기동 횟수 중복 검사 | Gateway 실행 로그 | `BACnet IP Gateway started` 정확히 1회 | 기동 성공 로그 문자열 개수가 1개인지 확인 |

- 현재 자동 검증 범위: 치명적인 설정 오류 시 프로세스가 죽지 않고 파일 수정 이벤트를 대기(HOLD)하며, 수정 즉시 정상 서비스로 복귀하는 전체 워크플로우 검증.
- 직접 검증하지 않는 항목/검증 공백: 파일 수정 후 리로드 시 변경된 객체 프로파일이 BACnet 네트워크에 정확히 갱신 반영되었는지는 별도 TC에서 검증.
- 정리/증적: Gateway 실행 및 복구 로그, 프로세스 상태 확인 정보.
- BLOCKED 조건: `--long` 옵션 누락 시 스킵됨. 테스트 환경(vport, Main, Gateway) 구성 요소 누락 시.

#### TC-CFG-007 — 모순 설정 HOLD 상태

- 실행 명령: 기본 명령.
- 입력/사전조건: 설정 파일 내 객체의 `min_value=100`, `max_value=10`으로 모순 설정 주입.
- 수행단계:
  1. 모순된 설정 파일을 사용하여 Gateway 프로세스만 단독 기동한다.
  2. Gateway가 `entering HOLD` 로그를 출력하고 강제 종료되지 않고 살아있는지 확인한다.
  3. HOLD 상태에서는 BACnet 서비스가 절대로 시작되지 않았음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| HOLD 대기 시 프로세스 생존 | Gateway 프로세스 | `true` | 기동 직후 프로세스 PID `poll()` 결과가 생존(None)인지 확인 |
| BACnet 미기동 안정성 | Gateway 실행 로그 | started 문자열 없음 | `BACnet IP Gateway started` 로그가 절대 출력되지 않았는지 확인 |

- 현재 자동 검증 범위: 치명적인 설정 오류 시 즉시 런타임 에러로 죽지 않고(Fault tolerance), 백그라운드에서 파일 수정을 무한 대기하는 기본 동작 검증.
- 직접 검증하지 않는 항목/검증 공백: HOLD 대기 상태에서 네트워크 포트(UDP 47808)가 점유되었는지 등 OS 수준의 소켓 상태는 확인하지 않음.
- 정리/증적: Gateway 실행 로그, 프로세스 PID 생존 상태.
- BLOCKED 조건: Gateway 실행 파일을 찾을 수 없는 경우.

#### TC-CFG-008 — HOLD 61초 유지 후 복구

- 실행 명령: `--long` (자동 복구 대기를 위한 긴 테스트 옵션 포함).
- 입력/사전조건: 설정 파일 내 객체의 `min_value=100`, `max_value=10`으로 모순 설정 주입. (`--long` 옵션 필수)
- 수행단계:
  1. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)을 모순된 설정으로 기동한다.
  2. Gateway가 HOLD 상태에 진입한 것을 확인한 뒤 61초 동안 명시적으로 대기한다. (파일 감시자 Timeout 검증)
  3. 61초 대기 후에도 프로세스가 크래시 없이 살아있는지 검사한다.
  4. 설정 파일을 정상으로 복원하고 파일 갱신 감지를 통해 Gateway가 정상 상태로 진입하는지 확인한다.
  5. 로그를 분석하여 기동 완료 로그가 1회만 발생했는지 검증한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 장기 대기 후 프로세스 생존 | Gateway 프로세스 | `true` | 61초 대기 후 프로세스 PID 상태 검사 (크래시 방지 확인) |
| 설정 오류 감지 문맥 | Gateway 실행 로그 | `FATAL CONFIG ERROR` 포함 | 대기 진입 전 치명적 에러 로그 발생 여부 확인 |
| 복구 후 정상 기동 여부 | Gateway 실행 로그 | `BACnet IP Gateway started` 정확히 1회 | 복구 이후 단 1회만 BACnet 서비스 기동이 일어나는지 검증 |

- 현재 자동 검증 범위: 1분 이상 장기간 방치되더라도 inotify 등 파일 감시 리소스가 만료되거나 예기치 않게 종료되지 않고, 복구 즉시 정상 궤도에 오르는 안정성 검증.
- 직접 검증하지 않는 항목/검증 공백: 61초 동안 로그 파일에 무의미한 스팸 로그가 반복 출력되는지는 검사하지 않음.
- 정리/증적: 61초 전/후 PID 상태 기록, 복구 성공 로그, 프로세스 상태 정보.
- BLOCKED 조건: `--long` 옵션 누락 시 스킵됨. 테스트 환경 구성 요소 누락 시.

#### TC-CFG-009 — HOLD 직후 SIGTERM

- 실행 명령: 기본 명령.
- 입력/사전조건: 설정 파일 내 객체의 `min_value=100`, `max_value=10`으로 모순 설정 주입.
- 수행단계:
  1. 모순된 설정 파일을 사용하여 Gateway 프로세스 단독 기동한다.
  2. 프로세스가 HOLD(대기) 상태에 돌입하는 즉시 SIGTERM 시스템 시그널을 전송하여 강제 종료시킨다.
  3. SIGTERM 발송 시점부터 실제 프로세스 종료까지 소요된 시간을 초 단위로 정밀 측정한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 시그널 즉각 반응성 | Gateway 프로세스 | `<= 1.0s` | 테스트 러너 (`tc_runner.py`)가 시그널 전송 후 프로세스가 1.0초 이하의 시간 내에 종료되었는지 비교 검증 |

- 현재 자동 검증 범위: 대기 모드 중에도 OS 시그널 핸들러가 정상적으로 동작하여 좀비 프로세스가 되지 않고, 데몬 종료 명령을 신속히 이행하는지 검증.
- 직접 검증하지 않는 항목/검증 공백: 비정상 종료 흐름이므로 생성된 소켓이나 파일 디스크립터(FD) 등 OS 자원 해제가 완벽한지는 직접 추적하지 않음.
- 정리/증적: 종료 소요 시간 측정값(ms 수준), Gateway 로그, SIGTERM 전송 기록.
- BLOCKED 조건: Gateway 실행 파일을 찾을 수 없는 경우.

#### TC-START-001 — 정상 순서 기동

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일(`gateway.json`, `profiles/*.json`), vport 가상 시리얼 포트 3 Master 설정, Main 시뮬레이터 Modbus 유지 레지스터 초기값(주소 3=`40`, 주소 4=`110`).
- 수행단계:
  1. vport 가상 시리얼 포트를 시작하고 5초 내 PTY 링크 생성을 확인한다.
  2. Main 시뮬레이터(`modbus_rtu_slave.py`)를 시작하고 소켓 통신을 확인한다.
  3. 시뮬레이터의 Modbus 유지 레지스터(Holding Register) 초기값을 설정한다.
  4. Gateway 프로세스를 시작하고 `BACnet IP Gateway started` 로그 출력을 확인한다(타임아웃 15초).
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 통신 채널 준비 | PTY 링크 / 유닉스 소켓 | 링크 생성 및 Ping 응답 성공 | 테스트 러너(`tc_runner.py`) 내부 어서션 통과 |
| Gateway 기동 | Gateway 로그 | `BACnet IP Gateway started` 로그 출력 | 타임아웃 내 해당 로그 문자열 확인 |

- 현재 자동 검증 범위: 정상적인 순서로 프로세스가 기동되고 BACnet 초기화가 성공적으로 완료되는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: BACnet 탐색(Discovery) 및 개별 객체의 Present_Value 속성 조회는 이 TC에서 다루지 않는다(BAC-001 등에서 검증).
- 정리/증적: 프로세스 실행 로그, PTY 링크 생성 기록, 유닉스 컨트롤 소켓 통신 이력, Gateway 시작 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-002 — Gateway 선기동

- 실행 명령: 기본 명령.
- 입력/사전조건: 프로세스가 실행되지 않은 초기 테스트 환경.
- 수행단계:
  1. 통신 채널(vport, Main)이 없는 상태에서 Gateway 프로세스를 먼저 시작한다.
  2. 0.3초 대기 후 Gateway 프로세스가 종료되지 않고 실행 중인지 확인한다.
  3. vport 가상 시리얼 포트(3 Master)와 Main 시뮬레이터를 순차적으로 시작한다.
  4. Gateway가 대기 상태를 벗어나 정상적으로 `BACnet IP Gateway started` 상태가 되는지 대기한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 사전 실행 생존 유지 | Gateway 프로세스 상태 | 프로세스 비정상 종료 없음 | `poll() is None` 조건 확인(별도 UI 출력 없음) |
| 후속 기동 및 동기화 | PTY / 소켓 / Gateway 로그 | vport, Main 기동 후 최종 기동 성공 | 통신 링크 확인, Ping 및 시작 로그 확인 통과 |

- 현재 자동 검증 범위: 통신 채널(vport) 및 대상 장비(Main)보다 Gateway가 먼저 실행되더라도 오류 종료 없이 대기 후 정상적으로 초기화를 완료하는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 대기 상태 중 실제 BACnet 요청에 대한 응답 동작이나 재시도 횟수 등은 측정하지 않는다.
- 정리/증적: Gateway 로그 파일, 프로세스 PID 유지 기록, PTY/소켓 통신 준비 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-003 — Main 지연 기동

- 실행 명령: 기본 명령.
- 입력/사전조건: vport 가상 시리얼 포트만 시작되어 PTY 링크가 준비된 상태.
- 수행단계:
  1. vport를 시작하여 통신 링크를 확인한다.
  2. Gateway 프로세스를 시작하고 0.8초 동안 대기한다.
  3. Main 시뮬레이터가 아직 실행되지 않은 시점에서, Gateway 로그에 `BACnet IP Gateway started` 문자열이 출력되지 않았음을 확인한다.
  4. Main 시뮬레이터를 기동하고 최종적으로 Gateway가 정상 기동하는지 대기한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Main 연결 전 서비스 비공개 | Gateway 로그 | `BACnet IP Gateway started` 없음 | 0.8초 시점에 해당 로그 문자열 미존재 확인 |
| Main 연결 후 기동 | 유닉스 소켓 / Gateway 로그 | 동기화 성공 및 로그 출력 | 시뮬레이터 통신 성공 및 로그 대기 조건 통과 |

- 현재 자동 검증 범위: Main 장비의 응답 지연이나 기동 지연 시 Gateway가 불완전한 상태로 BACnet 서비스를 외부에 노출하지 않고 안전하게 동기화를 완료하는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: Main 장비를 대상으로 한 Modbus 프레임 단위의 구체적인 재시도 횟수나 정책은 직접 검증하지 않는다.
- 정리/증적: 지연 대기 구간의 Gateway 로그, Main 소켓 Ping 응답 기록, 기동 완료 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-004 — JSON default와 Min/Max clamp 동기화

- 실행 명령: 기본 명령.
- 입력/사전조건: 초기 Modbus 유지 레지스터 값(주소 3=`40`, 주소 4=`110`), 테스트 설정 파일(gateway.json 및 프로파일)에 AO 인스턴스 2, 3, 4의 `default_value`가 각각 `50`, `-1`, `101`로, 범위(`min_value` ~ `max_value`)가 `10 ~ 90`으로 설정됨.
- 수행단계:
  1. 통합 테스트 환경(vport, Main 시뮬레이터)을 시작하고 초기 유지 레지스터 값을 설정한다.
  2. Gateway를 시작하고 동기화 완료 대기 후, Main 시뮬레이터의 유지 레지스터 주소 0번부터 연속 3개의 값을 읽는다(결과 배열 검증).
  3. BACnet 스크립트를 사용하여 AO 인스턴스 2, 3, 4 객체의 속성 ID 104(`Relinquish_Default`)와 ID 85(`Present_Value`) 값을 각각 조회한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Main 동기화 보정 | 유닉스 컨트롤 소켓 | 레지스터 값 `[50, 10, 90]` | 읽어온 레지스터 배열과 기댓값 정확히 일치 |
| AO 2,3,4 기본값 속성 | BACnet Property 104 (`Relinquish_Default`) | `50`, `10`, `90` | 각 인스턴스별 ReadProperty 응답값 정확히 일치 |
| AO 2,3,4 현재값 속성 | BACnet Property 85 (`Present_Value`) | `50`, `10`, `90` | 각 인스턴스별 ReadProperty 응답값 정확히 일치 |

- 현재 자동 검증 범위: 초기 기동 시 JSON 설정의 기본값이 제한된 범위(min/max)에 맞춰(Clamp) Main Modbus 레지스터에 쓰이고(FC06), 동일하게 BACnet 객체의 RD/PV 속성에 반영되는 로직을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 모든 객체에 대한 동기화 프레임 페이로드나 CRC 패킷 단위 분석은 수행하지 않는다.
- 정리/증적: Main 레지스터 조회 결과 로그, Modbus 트랜잭션 기록, BACnet 패킷 트레이스, Gateway 구동 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-005 — JSON default 생략, Main 값 범위 내

- 실행 명령: 기본 명령.
- 입력/사전조건: 초기 Modbus 유지 레지스터 값(주소 3=`40`, 주소 4=`110`), 테스트 설정에 AO 인스턴스 5 객체는 `default_value`가 생략되어 있고 범위는 `10 ~ 90`임.
- 수행단계:
  1. 통합 테스트 환경(vport, Main 시뮬레이터)을 시작하고 초기 유지 레지스터 값을 설정한다.
  2. Gateway 기동 및 동기화 완료를 대기한다.
  3. Main 시뮬레이터의 주소 3 유지 레지스터 값을 확인하고, 통신 통계에서 주소 3번 쓰기(FC06)가 발생했는지 검사한다.
  4. AO 인스턴스 5 객체의 속성 ID 104(`Relinquish_Default`)와 ID 85(`Present_Value`)를 BACnet으로 조회한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Main 값 유지 | 유닉스 컨트롤 소켓 | 레지스터 값 `40` | 조회 결과 값 확인 일치 |
| 범위 내 Write-back 유무 | Modbus 트랜잭션 기록 | 쓰기 기록(FC06) 없음 | 주소 3에 대한 쓰기 이벤트 미발생 어서션 |
| 기본/현재값 속성 | BACnet Property 104/85 | 각각 `40` | ReadProperty 응답값과 기댓값 정확히 일치 |

- 현재 자동 검증 범위: `default_value`가 설정되지 않았을 때 Main 장비에서 읽어온 초기값이 설정된 허용 범위 내에 있으면, 불필요한 Write-back을 발생시키지 않고 그대로 BACnet RD/PV로 설정되는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 다른 객체 설정과 쓰기 프레임의 바이너리 바이트 필드는 검사하지 않는다.
- 정리/증적: Main 시뮬레이터 통계 정보 로그, AO 인스턴스 5 BACnet 응답 내용, Gateway 트랜잭션 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-006 — JSON default 생략, 양쪽 경계 밖

- 실행 명령: 기본 명령.
- 입력/사전조건: 초기 Modbus 유지 레지스터 값(주소 3=`0`, 주소 4=`110`), 테스트 설정에 AO 인스턴스 5 및 8은 `default_value`가 생략되어 있고 범위는 `10 ~ 90`임.
- 수행단계:
  1. 통합 테스트 환경을 시작하고 두 레지스터 값을 설정한다.
  2. Gateway 기동 완료를 대기한다.
  3. Main 시뮬레이터 주소 3부터 연속 2개의 레지스터 값을 조회하여, 값이 `[10, 90]`으로 범위 경계 보정(Write-back)이 적용되었는지 확인한다.
  4. BACnet 스크립트로 AO 인스턴스 5와 8의 속성 104(`Relinquish_Default`)와 85(`Present_Value`)를 각각 조회한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Main 경계 보정 | 유닉스 컨트롤 소켓 | 레지스터 값 배열 `[10, 90]` | 조회 결과 배열 일치 검증 |
| AO 인스턴스 5 속성 | BACnet Property 104/85 | 각각 `10` | ReadProperty 응답값 일치 |
| AO 인스턴스 8 속성 | BACnet Property 104/85 | 각각 `90` | ReadProperty 응답값 일치 |

- 현재 자동 검증 범위: 초기 기본값 설정이 없는 객체가 Main 장비에서 유효 범위를 벗어난 값을 반환할 때, 설정된 Min/Max 경계로 자동 보정하여 장비에 쓰기(FC06) 및 BACnet 속성에 반영하는 로직을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 보정을 위한 Modbus 프레임 CRC 값 검증이나 프레임 단위의 모든 재시도 여부는 직접 어서션하지 않는다.
- 정리/증적: Main 레지스터 변경 결과, Modbus FC03/FC06 통신 트레이스, BACnet 속성 응답값 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-007 — 첫 FC06 동기화 timeout

- 실행 명령: 기본 명령.
- 입력/사전조건: Main 시뮬레이터 컨트롤 소켓에 초기 Modbus 유지 레지스터 쓰기(FC 06, 주소 0) 2회에 대해 타임아웃 장애 주입(Fault Injection).
- 수행단계:
  1. 가상 포트와 Main 시뮬레이터를 준비하고 타임아웃 장애 주입을 설정한다.
  2. Gateway 프로세스를 시작한다.
  3. 타임아웃 발생 기간 중 약 0.7초 시점에 Gateway 로그를 확인하여 외부 서비스 공개(`BACnet IP Gateway started`)가 발생하지 않았음을 확인한다.
  4. 재시도 완료 후 서비스가 성공적으로 기동되는지 확인하고, 발생한 타임아웃 트랜잭션 수와 로그 출력 횟수를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 초기 서비스 비공개 확인 | Gateway 로그 | 0.7초 시점 `started` 로그 없음 | 조건 시간 내 해당 문자열 미발생 확인 |
| 타임아웃 누적 발생 | Main 시뮬레이터 트랜잭션 기록 | 횟수 `2` | 장애 주입 카운트 일치 비교 |
| 최종 기동 확인 | Gateway 로그 | 서비스 시작 `1`회 | `BACnet IP Gateway started` 정확히 1회 확인 |

- 현재 자동 검증 범위: 초기 동기화 과정에서 단일 레지스터 쓰기(FC06) 타임아웃이 발생해도, 재시도 프로세스 중 외부 서비스를 노출하지 않고 정상적으로 복구하여 1회만 서비스를 기동하는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 실패한 각 Modbus 프레임의 바이트 페이로드 단위 분석 및 재시도 상태 머신의 세부 진입 로그까지는 어서션하지 않는다.
- 정리/증적: 장애 주입 JSON 설정 기록, Modbus 통신 타임아웃 트랜잭션 기록, Gateway 실행 로그 및 패킷 트레이스.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-008 — 중간 객체 timeout 후 진행

- 실행 명령: 기본 명령.
- 입력/사전조건: Main 시뮬레이터 컨트롤 소켓에 두 번째 Modbus 유지 레지스터 쓰기(FC 06, 주소 1) 1회에 대해 타임아웃 장애 주입.
- 수행단계:
  1. 환경 준비 및 장애(주소 1 타임아웃)를 주입한다.
  2. Gateway 프로세스를 시작하고 전체 초기화 기동 완료까지 대기한다.
  3. Main 시뮬레이터 트랜잭션 통계를 조회하여, 타임아웃 이전에 성공했던 첫 번째 유지 레지스터(주소 0)의 쓰기(FC 06) 카운트를 센다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 완료 객체 중복 쓰기 방지 | Main 트랜잭션 기록 | 주소 0의 쓰기(FC 06) `1`회 | 특정 주소의 트랜잭션 수 정확히 비교 확인 |

- 현재 자동 검증 범위: 여러 객체를 연속해서 동기화하는 도중 특정 객체에서 타임아웃이 발생하여 재시도될 때, 이전에 이미 완료된 객체는 다시 쓰기 동작을 수행하지 않고 중단된 지점부터 안전하게 이어서 처리하는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 전체 동기화 시퀀스의 진행 퍼센티지 갱신 내역 및 주소 1 객체 이후의 복구 과정은 직접 검증하지 않는다.
- 정리/증적: 타임아웃 장애 주입 기록, Main 장비 트랜잭션 로그 목록, Gateway 실행 로그.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-START-009 — 첫 FC05 동기화 timeout

- 실행 명령: 기본 명령.
- 입력/사전조건: Main 시뮬레이터 컨트롤 소켓에 초기 Modbus 코일 쓰기(FC 05, 주소 0) 2회에 대해 타임아웃 장애 주입(Fault Injection).
- 수행단계:
  1. 가상 포트와 Main 시뮬레이터를 준비하고 FC05 관련 타임아웃을 2회 주입한다.
  2. Gateway 프로세스를 시작한다.
  3. 타임아웃 진행 시점(약 0.7초)에 Gateway 외부 서비스 미공개(`started` 문자열 없음)를 확인한다.
  4. 재시도 완료 후 통계 정보에서 타임아웃 발생 수와 로그 내 기동 완료 횟수를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 서비스 비공개 유지 | Gateway 로그 | 0.7초 시점 기동 완료 로그 없음 | 지연 시점 내 해당 문자열 부재 어서션 |
| 타임아웃 누적 발생 | Main 시뮬레이터 통계 | 횟수 `2` | 지정된 타임아웃 주입 횟수 통과 |
| 최종 기동 확인 | Gateway 로그 | 서비스 시작 `1`회 | `BACnet IP Gateway started` 단 1회 확인 |

- 현재 자동 검증 범위: 초기 동기화 과정에서 코일 제어 쓰기(FC05) 타임아웃 2회가 연속 발생하더라도, 서비스가 조기 노출되지 않고 안전하게 재시도 후 성공적으로 기동 완료되는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 코일 쓰기의 구체적인 패킷 바이너리(FF00) 및 개별 재시도 프레임 바이트 검증은 수행하지 않는다.
- 정리/증적: 코일 타임아웃 장애 설정 파일, 트랜잭션 통계 요약, Gateway 재시도 로그 및 패킷 이력.
- BLOCKED 조건: Gateway, vport, Main 시뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-VPORT-001 — Master 1/3/5 link 생성과 정리

- 실행 명령: 자동 검증 프레임워크(`tc_runner.py`) 내부 실행.
- 입력/사전조건: 통합 테스트 환경 내 테스트용 PTY 링크 생성 경로(`dev/` 디렉터리).
- 수행단계:
  1. 가상 시리얼 포트 에뮬레이터(`vport`)를 Master 1 모드로 시작하고 PTY 링크(`dev/ttyMASTER1`, `dev/ttySLAVE`)를 확인한 뒤 종료한다.
  2. 같은 방식으로 Master 3과 Master 5 모드를 각각 실행한다.
  3. 각 실행에서 요청 수량의 `dev/ttyMASTER1..N`과 단일 `dev/ttySLAVE` 생성을 확인하고, 에뮬레이터 종료 후 디렉터리에 남은 링크가 없는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 생성 링크 수 | 파일 시스템(`dev/`) | Master 요청 수 + 1 | 각 모드별 생성된 심볼릭 링크 수량 비교 |
| 종료 후 잔여 링크 | 파일 시스템(`dev/`) | 0 (비어있음) | 각 모드 종료 후 디렉터리 empty 어서션 |

- 현재 자동 검증 범위: 에뮬레이터 기동 시 지정된 수량의 마스터/슬레이브 심볼릭 링크가 정확히 생성되고 종료 시 삭제되는 생명주기를 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 프로세스의 스레드 조인(Thread join), 실제 파일 디스크립터(FD) 메모리 누수 여부는 직접 검증하지 않는다.
- 정리/증적: 각 모드별 PTY 링크 목록 스냅샷, 에뮬레이터 로그, 프로세스 리턴 상태 정보.
- BLOCKED 조건: `vport` 에뮬레이터 실행 파일 부재 또는 실행 권한 없음.

#### TC-VPORT-002 — vport 정상 SIGTERM

- 실행 명령: 자동 검증 프레임워크(`tc_runner.py`) 내부 실행.
- 입력/사전조건: 3개의 Master가 설정된 가상 시리얼 포트 에뮬레이터(`vport`) 실행 상태.
- 수행단계:
  1. 3개의 Master 링크가 활성화된 `vport` 프로세스 그룹에 SIGTERM 시그널을 보낸다.
  2. 프로세스 종료 대기 후, `dev/` 디렉터리에 남은 PTY 심볼릭 링크(`ttyMASTER*`, `ttySLAVE`)가 모두 삭제되었는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 종료 후 잔여 링크 | 파일 시스템(`dev/`) | 0 (비어있음) | 디렉터리 empty 어서션 |

- 현재 자동 검증 범위: SIGTERM 수신 후 `vport`가 정상적으로 종료 루틴을 수행하며, 생성했던 PTY 심볼릭 링크 자원을 깔끔하게 정리하는지 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 커널 레벨의 자원 누수, 파일 디스크립터 반환 여부는 자동 프레임워크가 직접 확인하지 않는다.
- 정리/증적: 프로세스 종료 직후 링크 스냅샷, 에뮬레이터 로그 파일, 프로세스 리턴 상태.
- BLOCKED 조건: `vport` 에뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-VPORT-003 — 강제 종료 후 vport 재기동

- 실행 명령: 자동 검증 프레임워크(`tc_runner.py`) 내부 실행.
- 입력/사전조건: 3개의 Master가 설정된 가상 시리얼 포트 에뮬레이터(`vport`) 정상 실행 상태.
- 수행단계:
  1. `vport` 프로세스 그룹에 SIGKILL 시그널을 보내 비정상 종료(stale) 상태를 만든다.
  2. 강제 종료로 인해 이전 PTY 링크(`ttyMASTER*`, `ttySLAVE`)가 `dev/` 경로에 그대로 남아있는 상태에서 같은 `vport`를 재기동한다.
  3. 에뮬레이터가 이전 stale 링크들을 감지하여 처리하고 새로운 `ttySLAVE` 종단 링크를 정상적으로 제공하는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 강제 종료 후 상태 | 파일 시스템(`dev/`) | 프로세스 종료 및 stale 링크 잔류 | 비정상 종료 후 링크 존재 어서션 |
| 재기동 후 종단 | 파일 시스템(`dev/`) | 새로운 `ttySLAVE` 제공 | 링크 대기 5초 내 성공 여부 어서션 |

- 현재 자동 검증 범위: 이전 구동 인스턴스가 비정상 종료되어 시스템 자원(PTY 링크)이 꼬인 상태라도, 새 프로세스가 이를 우회/정리하고 슬레이브 포트를 다시 안정적으로 제공하는지 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 재기동 직후 실제 모드버스 패킷 송수신 루프의 정상 동작이나, 기존 Master 포트들에 대한 통신 복구까지는 확인하지 않는다.
- 정리/증적: 강제 종료 전후의 PID 비교 로그, 잔류/신규 링크 목록 스냅샷.
- BLOCKED 조건: `vport` 에뮬레이터 실행 파일 부재 또는 권한 오류.

#### TC-VPORT-004 — frame 조각 수신

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-004`).
- 입력/사전조건: 하나의 모드버스 요청 프레임을 TCP 윈도우 단편화처럼 2~3조각으로 분할하여 전송하도록 작성된 독립 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-004 시나리오를 주입한다.
  2. 분할된 프레임 조각들이 내부 버퍼에서 온전히 조립된 후 완성된 프레임 형태로만 Main 시뮬레이터 측으로 1회 전달되는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 프레임 단편화 수신 및 조합(reassembly) 알고리즘 검증을 `vport_test` C++ 바이너리에 위임하고, 자동 프레임워크는 해당 바이너리의 통과 유무와 종료 코드를 판정한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 프레임워크가 프레임 경계와 슬레이브 측 수신 바이트 배열을 직접 디코딩하여 검증하지 않는다. (Gateway/Main 사용 안 함)
- 정리/증적: `vport_test` 콘솔 출력(stdout/stderr), 분할 전송된 프레임 패킷 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 실행 권한 없음.

#### TC-VPORT-005 — CRC 오류와 guard 초과 buffer

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-005`).
- 입력/사전조건: CRC 필드 오류를 포함한 프레임과 가드 바이트(Guard byte) 타임아웃 초과 버퍼 발생 직후, 연달아 정상 프레임을 전송하는 독립 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-005 시나리오를 주입한다.
  2. 에뮬레이터가 CRC 에러 및 가드 바이트 초과를 감지하여 비정상 선행 프레임들은 즉각 폐기하고, 뒤이어 도착한 정상 프레임만 정확히 분리해 내는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: CRC 및 가드 타임아웃 예외 처리에 대한 버퍼 정리 메커니즘을 `vport_test` C++ 바이너리에 위임하고, 프레임워크는 결과 코드와 출력 마커를 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 프레임워크는 수신 큐(Queue)의 입출력 바이트 배열이나 가드 바이트 경계 타이밍을 직접 계산하여 어서션하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 내부 프레임 처리 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-006 — 3 Master 동시 요청

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-006`).
- 입력/사전조건: 3개의 Master 스레드가 동일한 타겟에 거의 동시에 모드버스 프레임을 전송하여 경합을 발생시키는 독립 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-006 동시 다발 트래픽 시나리오를 주입한다.
  2. 에뮬레이터 멀티플렉싱 처리 과정에서 각 Master의 프레임 바이트가 섞이지(Interleave) 않고, 큐잉(Queueing)을 통해 직렬화된 온전한 프레임들로 차례대로 처리되는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 멀티 마스터 환경에서 발생 가능한 패킷 섞임 방지(Mutually Exclusive Queueing) 메커니즘 검증을 `vport_test` 바이너리에 위임하여 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 프레임워크 수준에서는 스레드 스케줄링 시점 및 원시 바이트 레벨의 인터리빙 상태를 직접 모니터링하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 큐 처리 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-007 — Master별 응답 라우팅

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-007`).
- 입력/사전조건: 복수의 Master가 각각 상이한 모드버스 요청(예: 다른 주소)을 전송하고, 이에 매칭되는 Slave 응답을 에뮬레이션하는 독립 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-007 다중 응답 라우팅 시나리오를 주입한다.
  2. 가상 시리얼 포트 라우터가 Slave로부터 수신한 각각의 응답 프레임을 최초 요청을 발송했던 원발신(Origin) Master 포트로만 정확히 분배하는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: Slave 응답 프레임에 대한 트랜잭션 매칭 및 역방향(Reverse) 라우팅 정확도 검증을 `vport_test` 바이너리에 위임하여 결과 코드 및 출력 마커로 판정한다.
- 직접 검증하지 않는 항목/검증 공백: 프레임워크는 각 Master PTY 채널의 수신 버퍼 헥사(Hex) 덤프를 직접 캡처하여 비교하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 라우팅 디스패치 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-008 — FC06 응답 형식 두 가지

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-008`).
- 입력/사전조건: 표준 모드버스 FC06 응답(요청 복사본)과 특정 비표준 장비에서 발생하는 6바이트 길이의 축약된 FC06 응답을 각각 시뮬레이션하는 독립 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-008 이기종 FC06 응답 시나리오를 주입한다.
  2. 라우터가 길이나 포맷 차이에 구애받지 않고, 두 가지 응답 형태 모두 해당 요청을 보냈던 Master에게 완전한 응답 프레임으로 누락 없이 라우팅하는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 변칙적인 응답 프레임 길이에 대한 라우터의 유연성 및 호환성 검증 로직을 `vport_test` 바이너리에 위임하여 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 프레임워크는 응답 프레임의 바이트 길이, CRC 정합성 및 내부 라우팅 식별자 바이트를 스크립트 레벨에서 직접 파싱하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 양방향 프레임 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-009 — timeout·불완전·불일치 후 정상 요청

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-009`).
- 입력/사전조건: Slave 타임아웃, 길이가 잘린 불완전(Truncated) 응답, 요청과 주소가 맞지 않는 불일치 응답 등을 유발한 직후, 다른 Master에서 정상 요청을 시도하는 복합 에러 주입 테스트 시나리오.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-009 장애 복구 시나리오를 주입한다.
  2. 가상 라우터가 이전의 불량/타임아웃 트랜잭션 프레임을 큐에서 안전하게 폐기(Drop)하고 상태를 초기화한 뒤, 다음 번 정상 Master의 통신을 정상적으로 라우팅하는지 확인한다.
  3. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 통신 장애 상황 발생 시 트랜잭션 상태 머신 초기화 및 후속 정상 통신 복구 능력을 `vport_test` 바이너리의 어서션으로 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 프레임워크는 타임아웃 임계점(Boundary) 계산 방식과 두 번째 Master가 수신한 원시 바이트 버퍼 자체를 직접 비교하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 트랜잭션 리셋 트레이스.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-010 — PTY 슬레이브 open→close 후 CPU 점유율 측정

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-010`).
- 입력/사전조건: PTY 슬레이브 링크를 열었다가 닫아 슬레이브 클라이언트가 모두 연결 해제된 상태 유발. keeper fd가 열려 있어 슬레이브 참조가 0이 되지 않도록 보장된 환경.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-010 시나리오를 주입한다.
  2. 슬레이브 종단을 open한 뒤 close하여 클라이언트 연결 종료 상황을 만든다.
  3. 100ms 동안 마스터 fd의 select()가 블로킹 없이 EIO를 무한 반환하지 않고 정상 대기하는지 확인한다.
  4. ModbusHub 가동 상태에서 200ms 동안 소비된 CPU 시간(user + sys)이 25% 미만(정상 절전/대기)임을 확인한다.
  5. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: keeper fd 보유를 통한 PTY 슬레이브 닫힘 시 100% CPU 스핀 방지 기능을 `vport_test` 바이너리 어서션으로 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 커널 버전에 따른 select() 및 PTY 드라이버 내부 구현 차이는 직접 검사하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력 및 rusage 실측 지표.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-011 — 마스터 상한 초과 기동 거부

- 실행 명령: `vport 5 --link-dir <dir> --config <config>` 실행.
- 입력/사전조건: `vport.cfg`에 `MAX_MASTERS=2`가 설정된 임시 설정 파일을 준비하고, 마스터 수 5를 인수로 전달하여 기동 시도.
- 수행단계:
  1. `MAX_MASTERS=2` 설정 파일과 함께 마스터 개수를 5로 지정하여 `vport` 프로세스를 실행한다.
  2. 프로세스가 상한 초과 에러 메시지를 출력하고 0이 아닌 종료 코드로 즉시 비정상 종료하는지 확인한다.
  3. `MAX_MASTERS=2048` 설정 파일과 함께 `vport` 프로세스를 실행하여, POSIX `select()`의 `FD_SETSIZE`(1024) 초과 설정 시 에러 메시지 출력 후 즉시 종료하는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로세스 종료 코드 | `vport` 프로세스 | 종료 코드 `!= 0` | 프로세스 반환 코드가 0이 아님을 확인 |
| 상한 초과 에러 메시지 | `vport` 표준 에러 | "상한" 또는 "초과" 포함 | 표준 에러 문자열 내 인수 상한 검사 메시지 존재 확인 |
| FD_SETSIZE 초과 에러 메시지 | `vport` 표준 에러 | "FD_SETSIZE" 또는 "초과" 포함 | 표준 에러 문자열 내 FD_SETSIZE 초과 메시지 존재 확인 |

- 현재 자동 검증 범위: 마스터 개수 인수가 MAX_MASTERS 상한을 초과할 때 및 설정 파일의 MAX_MASTERS가 POSIX FD_SETSIZE(1024)를 초과할 때의 기동 차단 및 에러 종료 동작을 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 런타임 중 마스터 fd가 동적으로 FD_SETSIZE를 넘는 파일 디스크립터 고갈 상황은 직접 에뮬레이트하지 않는다.
- 정리/증적: `vport` 표준 에러 출력 로그.
- BLOCKED 조건: `vport` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-012 — 요청 큐 상한 초과 시 Modbus Exception 응답

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-012`).
- 입력/사전조건: `max_queue_depth=1`로 설정된 ModbusHub 인스턴스에 슬레이브 응답 지연 상태에서 3건의 연속 요청을 주입.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-012 시나리오를 주입한다.
  2. 첫 번째 요청이 슬레이브로 전달되어 응답 대기 중인 상태에서 두 번째 요청으로 큐를 가득 채운다.
  3. 세 번째 요청 주입 시 큐 상한 초과가 발생하여 Modbus Exception Response(Server Device Busy, FC|0x80, code 0x06, 유효 CRC)가 원발신 마스터로 회신되는지 확인한다.
  4. 슬레이브가 응답한 후 대기 중이던 두 번째 요청이 정상 처리되는지 확인한다.
  5. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 큐 포화 시 백프레셔 적용 및 Server Device Busy 예외 프레임 회신, 후속 요청 정상 복구를 `vport_test` 바이너리 어서션으로 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 대규모 마스터(10개 이상) 동시 폭주 상태에서의 큐 메모리 상한 실측은 포함하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-013 — 비표준 FC 프레임 거부 및 Exception 프레임 정상 라우팅

- 실행 명령: `vport_test` C++ 단위 테스트 바이너리 실행 (`--case TC-VPORT-013`).
- 입력/사전조건: 유효한 CRC를 포함하지만 지원되지 않는 비표준 Function Code(0x44)를 가진 프레임 및 유효한 Modbus Exception 응답(FC 0x83, code 0x02) 프레임 준비.
- 수행단계:
  1. 호스트 환경에서 `vport_test` C++ 바이너리를 실행하여 TC-VPORT-013 시나리오를 주입한다.
  2. 비표준 FC(0x44) 요청 및 응답 프레임이 CRC 브루트포스 폴백 없이 무시(0 반환)되는지 검증한다.
  3. Modbus Exception 응답 프레임(5바이트)이 명시적 분기를 통해 정확한 길이로 파싱되는지 검증한다.
  4. ModbusHub를 통해 Exception 응답이 원발신 마스터로 정상 라우팅되는지 확인한다.
  5. 테스트 바이너리 콘솔 출력에서 검증 성공 플래그(`[Test Success]`)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 반환 | `vport_test` 프로세스 | 종료 코드 `0` | 프로세스 리턴 코드 비교 어서션 |
| 독립 테스트 어서션 | `vport_test` 표준 출력 | `[Test Success]` 문자열 | 출력 로그 내 마커 존재 여부 검사 |

- 현재 자동 검증 범위: 표준 FC 외 비표준 프레임의 CRC 오탐 차단 및 표준 Exception 응답의 정확한 수신/라우팅을 `vport_test` 바이너리 어서션으로 확인한다.
- 직접 검증하지 않는 항목/검증 공백: Modbus 사양에 정의된 모든 비표준 벤더 확장 FC에 대한 전수 검사는 포함하지 않는다.
- 정리/증적: `vport_test` 콘솔 출력.
- BLOCKED 조건: 호스트 빌드된 `vport_test` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-014 — 명시적 --config 경로의 설정 파일 미발견 시 에러 종료

- 실행 명령: `vport 3 --link-dir <dir> --config /nonexistent/vport.cfg` 실행.
- 입력/사전조건: 존재하지 않는 가상의 설정 파일 경로(`/nonexistent/vport.cfg`)를 `--config` 옵션으로 명시 전달하여 기동 시도.
- 수행단계:
  1. 존재하지 않는 경로를 `--config`에 지정하여 `vport` 프로세스를 실행한다.
  2. 기본값을 조용히 사용하지 않고, 에러 메시지를 출력하며 0이 아닌 종료 코드로 즉시 종료하는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로세스 종료 코드 | `vport` 프로세스 | 종료 코드 `!= 0` | 프로세스 반환 코드가 0이 아님을 확인 |
| 에러 메시지 검출 | `vport` 표준 에러 | "설정 파일을 찾을 수 없습니다" 포함 | 표준 에러 문자열 내 설정 파일 부재 메시지 존재 확인 |

- 현재 자동 검증 범위: 명시적인 설정 파일 지정 시 파일이 없을 때 조용히 기본값으로 넘어가지 않고 오류로 즉시 실패하는 계약을 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 파일 권한 부족(EACCES) 상황에 대한 별도 분기 테스트는 포함하지 않는다.
- 정리/증적: `vport` 표준 에러 출력 로그.
- BLOCKED 조건: `vport` 바이너리 부재 또는 권한 오류.

#### TC-VPORT-015 — vport vs Gateway 타임아웃 계층 정합성 검증

- 실행 명령: verification 러너 (`run_test.sh --case TC-VPORT-015`).
- 모드: Managed / Live 공용 지원 (`test_config.json`의 `common`에 정의된 로컬 PC 설정 파일 직접 참조).
- 입력/사전조건: vport 설정 파일(`vport.cfg`) 및 게이트웨이 설정 파일(`gateway.json`).
- 수행단계:
  1. `vport.cfg`에서 `DEFAULT_SLAVE_TIMEOUT_MS` 값을 파싱한다 (`common.vport_config_path`에 지정된 로컬 파일 또는 폴더 내 `vport.cfg`).
  2. `gateway.json`에서 `rtu_timeout_ms` 값을 파싱한다 (`common.gateway_config_dir`에 지정된 로컬 디렉터리의 `gateway.json`).
  3. 상위 클라이언트 계층(Gateway)의 타임아웃이 하위 중계 계층(vport)의 타임아웃보다 크거나 같은지(T_gateway ≥ T_vport) 계층 정합성 규칙을 검증한다.
  4. 게이트웨이 타임아웃이 vport 타임아웃보다 크거나 같으면 `PASS`, 타임아웃 역전(T_gateway < T_vport) 발생 시 Head-of-Line 블로킹 및 연쇄 타임아웃 방지를 위해 `FAIL`로 자동 판정한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 타임아웃 계층 규칙 (T_gateway >= T_vport) | `gateway.json` vs `vport.cfg` 비교 (로컬 설정 파일 직접 참조) | `T_gateway >= T_vport` | 비교 불일치(역전) 시 즉시 FAIL 판정 |

- 현재 자동 검증 범위: vport와 Gateway의 설정 파일을 직접 참조(로컬 파일 시스템 참조)하여 타임아웃 역전으로 인한 연쇄 타임아웃 및 큐 블로킹 결함을 정적으로 원천 차단함을 자동 프레임워크가 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 런타임 중 동적으로 설정 파일이 교체되는 시나리오는 정적 검증 범위에 포함하지 않는다.
- 정리/증적: verification runner step 로그.
- BLOCKED 조건: 로컬 설정 파일 부재 (`gateway.json` 또는 `vport.cfg` 미발견).

#### TC-MAP-001 — 명시된 AI/AO/BI/BV mapping

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일(`gateway.json` + `profiles/*.json`) 또는 `--config-dir`로 지정된 디렉터리를 준비한다 (live 모드 시 필요한 Object Type이 없으면 `BLOCKED` 처리된다).
- 수행단계:
  1. 설정 파서(`config_loader.py`)로 전체 Device 및 Object 목록을 파싱한다.
  2. 파싱된 Object Type 집합에 AI(0), AO(1), BI(3), BV(5)가 모두 포함되어 있는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Object Type 집합 | 설정 파서 (`config_loader.py`) | `{0, 1, 3, 5}` 모두 포함 | 집합 포함 여부 (subset) 비교 |

- 현재 자동 검증 범위: 테스트 설정 파일에 4가지 필수 BACnet Object Type(AI, AO, BI, BV)이 모두 선언되어 있는지 파싱 레벨에서 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 BACnet Object List 응답이나, 각 인스턴스의 Present_Value와 Modbus 레지스터 간의 실시간 데이터 매핑 동작은 이 TC에서 검증하지 않는다.
- 정리/증적: 로드된 설정 파일 디렉터리 경로, 파싱된 Type 목록.
- BLOCKED 조건: Live 모드 실행 시 설정 파일에 필요한 4가지 Object Type이 누락되었거나 설정 파일을 읽을 수 없는 경우.

#### TC-MAP-002 — `bacnet_type` 자동 추론

- 실행 명령: 기본 명령.
- 입력/사전조건: 메모리 상의 파이썬 딕셔너리 매핑 객체 5가지를 준비한다.
- 수행단계:
  1. `DISCRETE_INPUT`, `INPUT_REGISTER`, `COIL`, 읽기 전용(`read_only=True`) `HOLDING_REGISTER`, 특정 비트 추출(`bit_position=2`) 조건이 설정된 `HOLDING_REGISTER` 딕셔너리를 각각 준비한다.
  2. 각 딕셔너리를 `infer_bacnet_type` 함수에 전달하여 반환되는 BACnet Object Type 코드를 확인한다.
- 자동 판정:

| 입력 객체 (Modbus Block) | 기대 BACnet Type | 현재 자동 판정 |
|---|---:|---|
| `DISCRETE_INPUT` | `3` (BI) | 일치 여부 비교 (`==`) |
| `INPUT_REGISTER` | `0` (AI) | 일치 여부 비교 (`==`) |
| `COIL` | `5` (BV) | 일치 여부 비교 (`==`) |
| read-only `HOLDING_REGISTER` | `0` (AI) | 일치 여부 비교 (`==`) |
| `HOLDING_REGISTER` + `bit_position` | `5` (BV) | 일치 여부 비교 (`==`) |

- 현재 자동 검증 범위: `infer_bacnet_type` 함수가 설정된 Modbus Block 속성 및 부가 옵션(`read_only`, `bit_position`)을 바탕으로 올바른 BACnet Object Type을 추론하는지 단위 테스트 형태로 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 `profiles/*.json` 파일 로드 시의 전체 유효성 검사 및 Gateway 프로세스가 생성하는 실제 BACnet Object 인스턴스 동작은 확인하지 않는다.
- 정리/증적: 테스트 딕셔너리 입력값 및 반환된 추론 결과 로그.
- BLOCKED 조건: 실행 환경 내 `infer_bacnet_type` 함수 의존성 누락 시.

#### TC-MAP-003 — 자동 instance 재현성

- 실행 명령: 기본 명령. (Managed 모드 전용)
- 입력/사전조건: 테스트용 설정 파일(`profiles/*.json`)에 `"name": "TC_AUTO_INSTANCE", "full_address": 30002, "modbus_block": "INPUT_REGISTER"` 객체를 동적으로 추가한다.
- 수행단계:
  1. 동적으로 수정된 테스트용 프로파일을 저장한다.
  2. 설정 파서(`config_loader.py`)를 통해 전체 설정을 2회 반복해서 로드한다.
  3. 첫 번째 파싱 결과와 두 번째 파싱 결과에서 생성된 `instance` 번호 배열을 추출하여, 두 배열이 완벽히 동일한지(재현성)와 배열 내 인스턴스 번호에 중복이 없는지(유일성) 검사한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 자동 인스턴스 재현성 및 유일성 | 설정 파서 반환 배열 | `same and unique` | 두 배열의 동일 여부(`==`) 및 중복 제거(Set) 후 길이 비교 |

- 현재 자동 검증 범위: 설정 파일 파싱 시 명시되지 않은 `instance` 속성을 자동 할당하는 로직이 멱등성(동일 파일 로드 시 동일 인스턴스 번호 할당)을 유지하고 충돌 없이 유일한 번호를 부여하는지 검증한다.
- 직접 검증하지 않는 항목/검증 공백: Gateway 프로세스가 실제 구동된 후 네트워크 상에서 Object List를 통해 해당 인스턴스 번호를 올바르게 노출하는지는 이 TC에서 검증하지 않는다.
- 정리/증적: 1/2회차 파싱 결과 인스턴스 배열, 설정 파일 체크섬.
- BLOCKED 조건: Live 모드로 실행하거나 디스크의 테스트용 설정 파일을 쓰기 모드로 수정할 수 없는 환경인 경우.

#### TC-MAP-004 — 숫자 변환 C++ unit-test

- 실행 명령: 기본 명령에 `--unit-test-bin <file>` 추가.
- 입력/사전조건: 빌드된 호스트 C++ 단위 테스트 바이너리(`run_tests`)가 경로에 존재해야 한다.
- 수행단계:
  1. `run_tests` C++ 단위 테스트 바이너리를 실행한다.
  2. 프로세스가 종료 코드 `0`으로 성공했는지 확인한다.
  3. 프로세스 표준 출력(stdout) 문자열에 INT16/UINT16/FLOAT32 변환 및 스케일/오프셋 검증을 증명하는 특정 마커(`extractRegister16`, `extractFloat32`, `Scale`, `offset`)가 모두 포함되어 있는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 실행 결과 | C++ 바이너리 프로세스 | 종료 코드 `0` | 프로세스 종료 코드 확인 |
| 변환 검증 마커 확인 | C++ 바이너리 stdout | `extractRegister16`, `extractFloat32`, `Scale`, `offset` | 모든 마커 문자열 포함 여부 검사 |

- 현재 자동 검증 범위: 게이트웨이 내부의 데이터 타입 및 단위 변환 로직(Scale/Offset)이 C++ 단위 테스트 수준에서 정상 통과함을 마커 기반으로 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크는 C++ 내부의 원시 레지스터 값이나 엔디안(Byte Order) 변환 과정을 개별적으로 캡처하고 단언(Assert)하지 않는다.
- 정리/증적: 단위 테스트 stdout 출력 로그, 단위 테스트 바이너리 경로 및 체크섬.
- BLOCKED 조건: C++ 단위 테스트 바이너리가 존재하지 않거나 실행 권한이 없는 경우.

#### TC-MAP-005 — swap 및 Min/Max 경계 C++ unit-test

- 실행 명령: 기본 명령에 `--unit-test-bin <file>` 추가.
- 입력/사전조건: 빌드된 호스트 C++ 단위 테스트 바이너리(`run_tests`)가 존재해야 한다.
- 수행단계:
  1. `run_tests` C++ 단위 테스트 바이너리를 실행한다.
  2. 프로세스가 정상 종료되는지 확인하고, stdout 출력에서 워드 스왑 및 Min/Max Clamp 변환 검증 마커를 스캔한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 실행 결과 | C++ 바이너리 프로세스 | 종료 코드 `0` | 프로세스 종료 코드 확인 |
| 변환 검증 마커 확인 | C++ 바이너리 stdout | `BIT_PACKED`, `min_value`, `max_value`, `word_swap` | 모든 마커 문자열 포함 여부 검사 |

- 현재 자동 검증 범위: 바이트/워드 스왑(`word_swap`) 및 설정값 경계(`min_value`, `max_value`) 강제 변환(Clamp) 로직이 C++ 단위 테스트를 통과했음을 stdout 마커를 통해 간접 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 네트워크를 통한 실제 Modbus 프레임 바이트 오더 변화나 초과 값 입력 시 실제 차단 여부를 이 TC가 엔드투엔드로 확인하지는 않는다.
- 정리/증적: 단위 테스트 stdout 출력 로그, 단위 테스트 바이너리 경로 및 체크섬.
- BLOCKED 조건: C++ 단위 테스트 바이너리가 존재하지 않거나 실행 실패 시.

#### TC-MAP-006 — bit/BV 변환 C++ unit-test

- 실행 명령: 기본 명령에 `--unit-test-bin <file>` 추가.
- 입력/사전조건: 빌드된 호스트 C++ 단위 테스트 바이너리(`run_tests`)가 존재해야 한다.
- 수행단계:
  1. `run_tests` C++ 단위 테스트 바이너리를 실행한다.
  2. 비트 추출(`extractBit`), 비트 패킹(`BIT_PACKED`), Binary Value 객체의 기본값 복귀(`Relinquish_Default`) 처리를 단언하는 stdout 마커들을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단위 테스트 실행 결과 | C++ 바이너리 프로세스 | 종료 코드 `0` | 프로세스 종료 코드 확인 |
| 변환 검증 마커 확인 | C++ 바이너리 stdout | `extractBit`, `BIT_PACKED`, `Binary Value Relinquish_Default` | 모든 마커 문자열 포함 여부 검사 |

- 현재 자동 검증 범위: 레지스터에서 특정 비트 인덱스를 읽고 쓰는 로직, 그리고 BV 객체의 Priority Array 해제 시 적용되는 `Relinquish_Default` 동작을 C++ 단위 테스트가 검증 완료했음을 판정한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크는 Modbus 레지스터에 기록 시 인접한 다른 비트들이 파괴되지 않고 보존(Read-Modify-Write)되는 현상을 실시간으로 캡처하지 않는다.
- 정리/증적: 단위 테스트 stdout 출력 로그, 단위 테스트 바이너리 경로 및 체크섬.
- BLOCKED 조건: C++ 단위 테스트 바이너리 부재 또는 비정상 종료 시.

#### TC-MAP-007 — 입력 객체 WriteProperty 차단

- 실행 명령: 기본 명령 (Live 네트워크 모드는 `--allow-live-mutation` 옵션 필수).
- 입력/사전조건: 통합 테스트 환경(vport + Main 시뮬레이터 + Gateway) 구동. 탐색된 첫 번째 입력 객체(AI 또는 BI)의 Instance 번호와 Type을 대상으로 지정한다.
- 수행단계:
  1. Main 시뮬레이터(`modbus_rtu_slave.py`)의 트랜잭션 통계 API를 호출하여 Write 기능(FC05, 06, 15, 16) 호출 횟수를 기록한다.
  2. 탐색된 AI 또는 BI 객체의 `Present_Value`(Property 85) 속성에 임의의 값으로 BACnet WriteProperty 요청(Invoke ID 250)을 전송한다.
  3. 게이트웨이가 `BACNET_ERROR_PDU`를 응답하는지 검사한다.
  4. WriteProperty 요청 처리 후 Main 시뮬레이터의 트랜잭션 통계를 재조회하여 Write 기능 호출 횟수가 증가하지 않았음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| BACnet WriteProperty 응답 | BACnet APDU | `BACNET_ERROR_PDU` (Class 2: PROPERTY, Code 40: WRITE_ACCESS_DENIED) | 응답 패킷의 Error Type 확인 |
| 백엔드 Modbus Write 미발생 | Main 시뮬레이터 stats API | FC05/06/15/16 누적 호출 횟수 변동 없음 (`Before == After`) | 전송 전/후 통계 카운트 비교 |

- 현재 자동 검증 범위: 읽기 전용 입력 객체(AI, BI)에 대한 네트워크상의 쓰기 시도가 안전하게 차단(Write Access Denied)되며, 하위 Modbus 네트워크로 악성 쓰기 프레임이 유출되지 않음을 단언한다.
- 직접 검증하지 않는 항목/검증 공백: 다른 모든 인스턴스 번호에 대해 순회하며 검사하지는 않으며, 대표 입력 객체 1개에 대해서만 샘플링 검증을 수행한다.
- 정리/증적: BACnet WriteProperty 요청 및 Error 응답 패킷 로그, 시뮬레이터 stats 호출 전/후 횟수 스냅샷.
- BLOCKED 조건: Live 네트워크 환경에서 쓰기(Mutation) 옵션이 미승인되었거나 통합 테스트 환경(Gateway 프로세스 등) 구동이 불가능한 경우.

#### TC-MAP-008 — AO Min 미만/Max 초과 WriteProperty 차단

- 실행 명령: 기본 명령 (Managed 모드 또는 Live 네트워크 모드).
- 입력/사전조건:
  - Managed 스택 또는 Live 타깃 게이트웨이 구동.
  - 설정 파일(`gateway.json` / 프로파일)에 `min_value`와 `max_value`가 정의된 Analog Output(AO, Type 1) 객체 (예: `TC_AO_JSON_IN`, Min: 0.0, Max: 100.0).
  - 프로파일 검증기 실행을 위한 Python 런타임.
- 수행단계:
  1. 게이트웨이 스택을 준비하고 설정 파일에서 `min_value`, `max_value`가 정의된 대상 AO 객체를 탐색한다.
  2. **Min 미만 쓰기 시험**: `min_value - 10.0` (예: `-10.0`) 값을 대상 AO의 `Present_Value`(Property 85)에 `WriteProperty` APDU로 전송한다.
  3. **Max 초과 쓰기 시험**: `max_value + 10.0` (예: `110.0`) 값을 대상 AO의 `Present_Value`(Property 85)에 `WriteProperty` APDU로 전송한다.
  4. 게이트웨이가 두 요청에 대해 즉시 쓰기를 거부하고 BACnet Error APDU (`Class 2: PROPERTY`, `Code 37: VALUE_OUT_OF_RANGE`)를 응답하는지 검증한다.
  5. 프로파일 테스터(`bacnet_profile_tester.py`)의 전체 실행 결과(`SUCCESS`) 및 실패 항목(`FAILED`) 부재를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Min 미만 / Max 초과 쓰기 차단 | BACnet/IP APDU | `BACNET_ERROR_PDU` (`VALUE_OUT_OF_RANGE`) | `bacnet_profile_tester.py` 내부 assertion |
| 테스터 프로세스 정상 종료 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | return code `0` | 프로세스 반환 코드가 0인지 확인 |
| 프로파일 검증 실패 항목 부재 | `bacnet_profile_tester.py` 실행 결과(output) | `FAILED` line 없음 | FAILED line 검사 |
| 범위 검증 성공 증적 | `bacnet_profile_tester.py` 실행 결과(output) | `SUCCESS` (`Both rejected with VALUE_OUT_OF_RANGE`) | marker 존재 비교 |

- 현재 자동 검증 범위: AO 객체에 대한 Min 미만/Max 초과 쓰기 요청이 `VALUE_OUT_OF_RANGE` 에러로 차단되고 Modbus 레지스터가 보호됨을 프로파일 테스터를 통해 검증한다.
- 직접 검증하지 않는 항목/검증 공백: Priority-Array의 각 인덱스(1~16)별 세부 우선순위 범위 검사는 본 TC에서 직접 분리하지 않고 프로파일 테스터에 위임한다.
- 정리/증적: `tc-map-008_bacnet_profile_tester.log`, `tc-map-008_gateway.log`, `tc-map-008_main_simulator_transactions.log`.
- BLOCKED 조건: Python runtime 또는 managed/network 의존성 부족.

#### TC-MAP-009 — 중복 명시 instance 및 주소 범위 초과 거부

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트 환경에 기본 정상 프로파일(`profiles/fixture.json`)이 준비되어 있다.
- 수행단계:
  1. [1차 검증: 인스턴스 중복] 첫 번째 객체의 `instance` 번호를 두 번째 객체에 동일하게 덮어쓴 프로파일로 Gateway 프로세스 기동을 시도한다.
  2. Gateway가 인스턴스 중복 충돌을 감지하여 Crash Loop 방지용 ConfigHold 상태로 진입하고 `Duplicate instance` 진단 로그를 남기며 BACnet 서비스가 시작되지 않음을 확인한다.
  3. [2차 검증: 주소 범위 초과] 프로파일을 복원한 후 객체의 `full_address`를 비정상 범위(`999999`)로 설정한 프로파일로 Gateway 프로세스 기동을 시도한다.
  4. Gateway가 주소 범위 초과 오류를 감지하여 ConfigHold 상태로 진입하고 주소 오류 진단 로그를 남기며 BACnet 서비스가 시작되지 않음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 1차 중복 instance HOLD 생존 | Gateway 프로세스 | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| 1차 진단 로그 (중복 instance) | Gateway stderr/stdout | `Duplicate instance` | 로그 텍스트 내 중복 오류 메시지 포함 검증 |
| 1차 HOLD 중 BACnet 미기동 | Gateway 로그 | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |
| 2차 주소 초과 HOLD 생존 | Gateway 프로세스 | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| 2차 진단 로그 (주소 범위 초과) | Gateway stderr/stdout | `full_address`/`exceeds` 오류 | 로그 텍스트 내 주소 범위 초과 메시지 포함 검증 |
| 2차 HOLD 중 BACnet 미기동 | Gateway 로그 | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |

- 현재 자동 검증 범위: 동일 객체 인스턴스 번호 중복과 Modbus 레지스터 주소 범위 초과 오류가 상호 마스킹되지 않도록 2단계 순차 주입을 통해 각각 Gateway 초기화 단계에서 Crash Loop 방지용 ConfigHold 상태로 안전하게 전환되고 BACnet 서비스가 격리됨을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 복수 프로파일 파일 간의 교차 중복 인스턴스 검사는 본 단일 프로파일 시나리오에서 직접 다루지 않는다.
- 정리/증적: 조작된 설정 파일(JSON), `tc-map-009_map_duplicate_instance.log`, `tc-map-009_map_address_range_error.log`.
- BLOCKED 조건: Gateway 실행 파일이 존재하지 않아 프로세스 기동 테스트 자체가 불가능한 경우.

#### TC-MOD-001 — FC01/02/03/04 정상 응답

- 실행 명령: 기본 명령. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)을 기동하여 가상 시리얼 포트 상의 Modbus RTU 읽기 트래픽 및 BACnet 반영을 검증한다. (Managed 모드 전용)
- 입력/사전조건: Managed 모드(`--mode managed`). Unit ID는 1로 설정하며, Main 시뮬레이터의 Coil 0 (`True`), Discrete Input 0 (`True`), Holding Register 0 (`50`), Input Register 0 (`78`) 위치에 초기값을 적재한다.
- 수행단계:
  1. Main 시뮬레이터 제어 소켓을 통해 4대 블록(AI 1 / Input Reg 0, BI 1 / Discrete Input 0, AO 1 / Holding Reg 0, BO 1 / Coil 0)에 초기값을 적재한다.
  2. vport 가상 시리얼 포트 허브, Main 시뮬레이터, Gateway 프로세스를 기동하여 Modbus RTU 폴링을 시작한다.
  3. BACnet ReadProperty(Present_Value) 요청으로 AI 1, BI 1, AO 1, BO 1의 현재값을 읽어 시뮬레이터 값과의 일치 여부를 검증한다.
  4. vport 트래픽 로그(`vport.log`) 및 Main 시뮬레이터 트랜잭션 이력을 분석하여 FC01, FC02, FC03, FC04 요청 프레임이 가상 시리얼 라인을 통해 정상 전송되었는지 확인한다.
  5. 송수신된 모든 Modbus RTU 프레임의 Unit ID(1) 및 CRC16 무결성을 검증한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| BACnet 관제점 읽기 일치 | BACnet ReadProperty | AI=78.0, BI=active, AO=50.0, BO=active | 4대 관제점 Present_Value 수신값 비교 |
| 4대 블록 FC 검출 | vport / Simulator 트랜잭션 | FC01, FC02, FC03, FC04 모두 검출 | 트랜잭션 내 요청 Function Code 목록 검증 |
| 프레임 CRC16 무결성 | vport 수집 프레임 | 모든 요청/응답 Valid CRC16 | Modbus RTU CRC16 무결성 알고리즘 단언 |
| vport 중계 증적 | `vport.log` 및 시뮬레이터 로그 | vport 중계 로그 및 트랜잭션 보존 | 로그 파일 내 트랜잭션 레코드 존재 확인 |

- 현재 자동 검증 범위: Gateway 프로세스가 실제 vport 가상 시리얼 라인을 통해 Main 시뮬레이터와 Modbus RTU 통신을 수행하며 4대 기본 기능 코드(FC01/02/03/04)를 모두 정상 발행하고 응답 CRC16을 검증한 뒤 BACnet 객체 속성에 무결하게 반영하는 End-to-End 연동을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 RS485 물리 계층의 전기적 노이즈나 케이블 단선은 소프트웨어 가상 포트(vport) 환경이므로 검증하지 않는다.
- 정리/증적: `vport.log`, 시뮬레이터 트랜잭션 로그(`transactions`), BACnet tx/rx 패킷, Gateway 로그.
- BLOCKED 조건: Live 모드로 실행 시 차단됨 (Gateway, Main 시뮬레이터 실동작 및 vport 트래픽 증적이 필요하므로 `--mode managed` 전용) 또는 vport/Gateway 바이너리 부재 시.

#### TC-MOD-002 — FC05/06/16 정상 Write

- 실행 명령: 기본 명령. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)을 기동하여 BACnet WriteProperty에 의한 Modbus 쓰기 프레임 발행과 시뮬레이터 메모리 반영을 검증한다. (Managed 모드 전용)
- 입력/사전조건: Managed 모드(`--mode managed`). Unit ID 1, vport 및 Main 시뮬레이터, Gateway가 정상 연동 기동됨.
- 수행단계:
  1. BACnet WriteProperty를 통해 BO 1에 `active(1)` 쓰기를 요청하여 Modbus FC05(단일 코일 쓰기)를 유도한다.
  2. BACnet WriteProperty를 통해 AO 1(UINT16)에 `60.0` 쓰기를 요청하여 Modbus FC06(단일 레지스터 쓰기)을 유도한다.
  3. BACnet WriteProperty를 통해 AO 9(FLOAT32)에 `75.5` 쓰기를 요청하여 Modbus FC16(다중 레지스터 쓰기)을 유도한다.
  4. Main 시뮬레이터 트랜잭션 로그 및 `vport.log`에서 FC05, FC06, FC16 쓰기 요청 및 에코 응답 프레임을 추출하고 CRC16 무결성을 검증한다.
  5. 제어 소켓을 통해 Main 시뮬레이터의 내부 메모리 상태를 조회하여 Coil 0 = `True`, Holding Reg 0 = `60`, Holding Reg 9~10 = `[17047, 0]` (Float 75.5)로 정확히 반영되었는지 단언한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| BACnet WriteProperty 응답 | BACnet APDU | SimpleACK | 각 관제점 쓰기 요청의 정상 성공 응답 확인 |
| FC05/06/16 쓰기 프레임 검출 | vport / Simulator 트랜잭션 | FC05, FC06, FC16 검출 | 트랜잭션 내 쓰기 Function Code 목록 검증 |
| Write 및 에코 CRC16 무결성 | vport 수집 프레임 | Valid CRC16 | 요청 프레임 및 에코 응답 프레임 CRC16 검증 |
| 시뮬레이터 메모리 반영 | Simulator 제어 소켓 | Coil=True, Reg 0=60, Reg 9=75.5 | 슬레이브 메모리 실제 적재값 비교 단언 |
| vport 중계 증적 | `vport.log` | vport 쓰기 및 에코 중계 보존 | vport 로그 내 프레임 전달 증적 확인 |

- 현재 자동 검증 범위: Gateway가 BACnet 쓰기 명령을 수신하여 관제점 데이터 타입에 맞게 Modbus RTU FC05(단일 코일), FC06(단일 레지스터), FC16(다중 레지스터) 프레임으로 변환/송신하고 에코 응답을 검증하며, Main 시뮬레이터의 메모리에 정확히 반영되는 End-to-End 쓰기 계약을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 비표준 Modbus 쓰기 명령(FC15 다중 코일 쓰기 등)은 게이트웨이 사양상 미사용이므로 검증 대상이 아니다.
- 정리/증적: `vport.log`, 시뮬레이터 트랜잭션 및 메모리 덤프, BACnet tx/rx 패킷.
- BLOCKED 조건: Live 모드로 실행 시 차단됨 (Gateway, Main 시뮬레이터 실동작 및 vport 트래픽 증적이 필요하므로 `--mode managed` 전용).

#### TC-MOD-003 — Modbus 오류와 one-shot timeout

- 실행 명령: 기본 명령. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 상에서 Main 시뮬레이터에 동적 통신 장애를 주입하고 Gateway의 오류 처리 및 자동 복구를 검증한다. (Managed 모드 전용)
- 입력/사전조건: Managed 모드(`--mode managed`). Unit ID 1. Main 시뮬레이터 제어 소켓(`control_socket`) 연결 준비.
- 수행단계:
  1. Main 시뮬레이터에 Holding Register 3=40, Holding Register 4=110 초기값을 설정하고 스택을 기동한다.
  2. 제어 소켓을 통해 FC03 요청 대상 1회성(One-shot) Timeout Fault(`action: timeout, count: 1`)를 주입한다.
  3. Gateway가 Modbus 타임아웃을 감지하여 Gateway 로그에 기록하고, 해당 시점 BACnet 요청이 차단/실패함을 확인한다.
  4. 1회성 타임아웃 소진 후 다음 요청 시 정상 응답으로 통신이 자동 복구됨을 검증한다.
  5. 제어 소켓을 통해 Modbus Exception(FC `0x83`, Exception Code `3`: Illegal Data Value) Fault를 주입한다.
  6. Gateway가 Exception 응답을 수신하여 BACnet 처리를 차단하고, 후속 요청 시 정상 통신으로 복구됨을 확인한다.
  7. 제어 소켓을 통해 손상된 CRC(Bad CRC) 패킷 반환 Fault를 주입한다.
  8. Gateway가 CRC 불일치 패킷을 감지/폐기하고, 후속 요청 시 정상 통신으로 복구됨을 확인한다.
  9. vport 로그 및 시뮬레이터 통계(`stats`)를 통해 장애 및 복구 트랜잭션 증적 보존을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Timeout 감지 및 차단 | BACnet 응답 & Gateway stdout | 에러/차단 & `Modbus ... timeout` 로그 | 타임아웃 감지 로그 및 에러 상태 단언 |
| Timeout 후속 자동 복구 | BACnet ReadProperty | `SUCCESS` 정상 복구 | 타임아웃 소진 후 정상 값 응답 확인 |
| Modbus Exception(0x83) 차단 | BACnet 응답 | 에러/차단 처리 | Exception 수신 시 BACnet 거부 단언 |
| Exception 후속 자동 복구 | BACnet ReadProperty | `SUCCESS` 정상 복구 | 후속 요청의 정상 성공 응답 확인 |
| 손상 CRC 폐기 및 오류 감지 | BACnet 응답 & 시뮬레이터 로그 | 비정상 프레임 폐기/차단 | 손상 응답 수신 시 처리 거부 단언 |
| 손상 CRC 후속 자동 복구 | BACnet ReadProperty | `SUCCESS` 정상 복구 | 후속 요청의 정상 통신 복구 확인 |
| 트랜잭션 증적 보존 | `vport.log` 및 시뮬레이터 `stats` | 장애 및 복구 트랜잭션 전수 기록 | 통계 및 로그 내 트랜잭션 보존 확인 |

- 현재 자동 검증 범위: 시뮬레이터-vport-Gateway 간 End-to-End 통신에서 발생할 수 있는 3대 통신 오류(무응답 타임아웃, Modbus Exception 프로토콜 에러, CRC 데이터 변조 손상)에 대해 Gateway가 비정상 프레임을 올바르게 감지/폐기하고 일시적 장애 복구 후 정상 통신을 재개하는 내결함성(Fault Tolerance)과 복원력을 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 영구적인 하드웨어 단선에 의한 장기 통신 장애 시 프로세스 재기동 정책 등은 장기 안정성 시험(TC-STB)에서 다룬다.
- 정리/증적: `vport.log`, 시뮬레이터 트랜잭션 통계 및 fault 내역, Gateway 실행 로그.
- BLOCKED 조건: Live 모드로 실행 시 차단됨 (Gateway, Main 시뮬레이터 실동작 및 vport 트래픽 증적이 필요하므로 `--mode managed` 전용).

#### TC-BAC-001 — 범위 없는 Who-Is와 I-Am

- 실행 명령: 기본 명령. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)을 자동 기동하고 live는 `--mode live`로 관찰한다.
- 입력/사전조건: 대상 IP/port 기본 `127.0.0.1:47808`, device ID 기본 `1200`.
- 수행단계:
  1. managed에서는 vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동한다.
  2. low/high 없는 Who-Is를 전송한다.
  3. 1초 동안 datagram을 수집하고 I-Am을 해석한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 응답 수/parse | BACnet/IP | I-Am 1개, parse `SUCCESS` | count/status 비교 |
| Device 정보 | I-Am APDU | device ID, max APDU, segmentation, vendor ID 일치 | field별 비교 |
| sender | UDP metadata | 대상 Gateway의 IP/port | sender 비교 |

- 현재 자동 검증 범위: native Who-Is/I-Am packet field와 sender를 직접 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 모든 선언 service/object bit와 I-Am의 모든 optional field는 이 카드에서 검사하지 않는다.
- 정리/증적: BACnet tx/rx packet, parsed field, sender, startup log.
- BLOCKED 조건: live에서 대상 장비에 접근할 수 없거나 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 의존성이 없는 경우.

#### TC-BAC-002 — 범위 내/범위 밖 Who-Is

- 실행 명령: 기본 명령.
- 입력/사전조건: device ID `D`는 `--device-id` 값(기본 `1200`).
- 수행단계:
  1. vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동한다.
  2. `D..D` bounded Who-Is를 보내고 응답을 수집한다.
  3. `D+1..D+2` bounded Who-Is를 보내고 응답을 수집한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 범위 내 | BACnet/IP | I-Am 1개와 기본 field 일치 | count/field 비교 |
| 범위 밖 | BACnet/IP | I-Am 0개 | response count 비교 |

- 현재 자동 검증 범위: 대상 device ID 경계의 응답 포함/제외를 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 다른 device가 동시에 응답하는 다중 장치 환경과 broadcast routing은 검사하지 않는다.
- 정리/증적: 두 Who-Is tx/rx와 parsed response list.
- BLOCKED 조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 또는 live BACnet 관찰 대상이 없음.

#### TC-BAC-003 — Who-Has와 I-Have object binding

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)의 첫 active object 이름/instance.
- 수행단계:
  1. vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동하고 unbounded Who-Is를 전송한다.
  2. 첫 active object의 이름으로 Who-Has를 전송한다.
  3. I-Have 응답에서 device/object type/instance/name을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| I-Am | BACnet/IP | 1개 및 기본 field 일치 | native Who-Is assertion |
| I-Have | BACnet/IP | 1개, 요청 object binding 일치 | count/fields exact 비교 |

- 현재 자동 검증 범위: 첫 active object 하나에 대한 이름 기반 Who-Has/I-Have를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: Object List 전체를 ReadProperty로 조회하거나 모든 object name을 검색하지 않는다.
- 정리/증적: Who-Is/Who-Has APDU와 parsed binding.
- BLOCKED 조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 또는 active object가 없음.

#### TC-BAC-004 — 8개 object Present_Value

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`) 8개 object; Main 기본 holding 3=`40`, 4=`110`.
- 수행단계:
  1. vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동한다.
  2. active object 8개에 property 85(`Present_Value`, 현재값) ReadProperty를 각각 전송한다.
  3. managed에서는 각 object의 Modbus source raw 값을 Unix control socket으로 읽는다.
  4. type과 `raw × scale + offset` 후 Min/Max clamp 결과를 BACnet PV와 비교한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| response/type | BACnet/IP property 85(`Present_Value`) | `SUCCESS`, numeric 또는 binary type | object별 parse/type assertion |
| source 변환 | Unix + BACnet/IP | scale/offset/clamp 결과 | object별 expected value 비교 |
| AO #8 예 | Main/BACnet | raw 110 → PV 90 | clamp 결과 비교 |

- 현재 자동 검증 범위: 8개 object의 PV type과 managed source 대비 변환값을 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 각 property의 모든 optional metadata, Object List 응답과 packet byte 전체는 검사하지 않는다.
- 정리/증적: object별 raw/PV matrix, BACnet packet, Modbus transaction.
- BLOCKED 조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 의존성 부족 또는 live source를 해석할 수 없음.

#### TC-BAC-005 — 정적/배열 속성과 Object List index

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), Python runtime, `bacnet_profile_tester.py`.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 target IP/port 및 config와 함께 실행한다.
  2. index 0, 유효 index, ALL 요청 결과가 포함되었는지 `bacnet_profile_tester.py` 실행 결과(output)을 확인한다.
  3. return code, FAILED 목록, 완료 marker를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로파일 검증 스크립트 실행 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| mismatch | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음 | FAILED line 검사 |
| 완료 | `bacnet_profile_tester.py` 실행 결과(output) | SUCCESS/PASSED | 'SUCCESS' 또는 'PASSED' 키워드 존재 확인 |

- 현재 자동 검증 범위: 프로파일 검증기(`bacnet_profile_tester.py`)의 종합 결과 세 가지를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)가 index별 response count/element/APDU를 직접 decode하지 않는다. 이 카드의 SRS 기대 전체는 `bacnet_profile_tester.py` 내부 판정 결과에 의존한다.
- 정리/증적: `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 log/output, config selection.
- BLOCKED 조건: Python runtime, managed Gateway 또는 tester 의존성 부족.

#### TC-BAC-006 — unknown object/property/index 오류

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 실행한다.
  2. unknown object/property/index 요청의 결과가 `bacnet_profile_tester.py` 실행 결과(output)에 반영되었는지 확인한다.
  3. 종료 code와 FAILED/SUCCESS marker를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로파일 검증 스크립트 실행 | `bacnet_profile_tester.py` 프로세스 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 실패 목록/완료 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 문자열 기반 assertion |

- 현재 자동 검증 범위: `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 결과 상태만 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 표준 Error/Abort code (예: Class 2 PROPERTY, Code 31 UNKNOWN_PROPERTY), invoke ID와 transaction termination APDU를 자동 검증 프레임워크 (`tc_runner.py`)가 직접 검사하지 않는다.
- 정리/증적: `bacnet_profile_tester.py` 실행 결과(output)/log와 필요 시 별도 packet capture.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BAC-007 — Priority 8 우선 및 하위 쓰기 방어

- 실행 명령: 기본 명령; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: 첫 writable AO/BO. 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)에서는 AO #2가 선택되며 analog 값은 high=`70`, low=`30`(범위에 맞게 clamp)다.
- 수행단계:
  1. priority 8/16 slot을 NULL로 초기화하고 RD(property 104, `Relinquish_Default`)를 읽는다.
  2. priority 8에 high 값을 쓰고 PV(property 85, `Present_Value`)/CCP(property 431, `Current_Command_Priority`)를 읽어 8이 적용되었는지 확인한다.
  3. priority 16에 low 값을 쓰고 PV/CCP가 상위인 priority 8을 그대로 유지하는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| priority 8 적용 | BACnet/IP | PV=high, CCP=8 | ReadProperty 비교 |
| priority 8 우선 방어 | BACnet/IP | priority 16 write 후에도 PV=high, CCP=8 | 비교 |

- 현재 자동 검증 범위: 선택된 writable object에 대해 priority 8 적용 및 하위 slot(16) 쓰기 시 상위 우선순위(8)가 방어/유지됨을 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: priority 8 해제 후 fallback은 TC-BAC-008에서 검사하며, 모든 16개 slot 조합은 검사하지 않는다.
- 정리/증적: WriteProperty/ReadProperty packet, PV/CCP/RD matrix, cleanup 결과.
- BLOCKED 조건: live mutation 미승인, writable object 없음, managed 의존성 부족.

#### TC-BAC-008 — Priority 8 해제 후 Priority 16 인계

- 실행 명령: 기본 명령; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: TC-BAC-007과 같은 첫 writable AO/BO; analog high/low는 `70/30` 기준이다.
- 수행단계:
  1. priority 8/16에 high/low를 순차 기입하여 priority 8 활성 및 priority 16 대기 상태를 구성한다.
  2. priority 8을 BACnet NULL로 해제한다.
  3. PV와 CCP가 차순위인 priority 16으로 정상 인계(fallback)되는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| priority 8 해제 후 PV 인계 | BACnet/IP | low (priority 16 값) | ReadProperty 비교 |
| priority 8 해제 후 CCP 인계 | BACnet/IP | `16` | property 431(`Current_Command_Priority`) 비교 |

- 현재 자동 검증 범위: priority 8 해제 시 차순위 대기 중인 priority 16으로의 정상 fallback(PV 및 CCP)을 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: priority 16 해제 후 Relinquish_Default 복귀는 TC-BAC-009에서 검사한다.
- 정리/증적: priority packet trace, PV/CCP, cleanup write 결과.
- BLOCKED 조건: live mutation 미승인, writable object 없음, managed 의존성 부족.

#### TC-BAC-009 — 모든 priority slot NULL 시 Relinquish Default 복귀

- 실행 명령: 기본 명령; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: 첫 writable AO/BO와 범위 내 high/low 값.
- 수행단계:
  1. priority 8/16에 값을 순차 기입하여 priority slot이 활성화된 상태를 구성한다.
  2. priority 8과 16을 모두 BACnet NULL로 해제한다.
  3. PV와 Current_Command_Priority를 읽어 default fallback(Relinquish_Default 복귀 및 CCP 해제)을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| PV default 복귀 | BACnet/IP | RD(property 104, `Relinquish_Default`)와 동일 | final PV/RD 비교 |
| CCP 해제 | BACnet/IP | `NULL` (None) | property 431(`Current_Command_Priority`) 비교 |

- 현재 자동 검증 범위: 모든 설정된 priority 슬롯 해제 후 Relinquish_Default로의 정상 복귀 및 CCP 해제를 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: priority 1~7/9~15의 개별 NULL 순서와 각 slot의 error response는 검사하지 않는다.
- 정리/증적: priority writes, RD/PV/CCP packet, cleanup log.
- BLOCKED 조건: live mutation 미승인, writable object 없음, managed 의존성 부족.

#### TC-BAC-010 — COV_Increment 유효/무효와 변화 경계

- 실행 명령: 기본 명령; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: 첫 analog object(AI), Main source와 범위 폭이 최소 5 이상인 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`). property 22(`COV_Increment`) 원래 값을 보관한다.
- 수행단계:
  1. property 22(`COV_Increment`)를 읽고 `5.0`을 WriteProperty한다.
  2. property 22(`COV_Increment`)를 다시 읽어 `5.0`인지 확인한다.
  3. `0.0`을 WriteProperty하고 BACnet Error를 확인한다.
  4. process 610, lifetime 60으로 SubscribeCOV한다.
  5. baseline을 버린 뒤 source를 increment 미만(약 2) 변화시키고 COV가 없는지 확인한다.
  6. source를 increment 이상(약 7) 변화시키고 COV가 하나 이상인지 확인한다.
  7. 구독과 source/property 22(`COV_Increment`)를 원래 값으로 복원한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 유효 increment | BACnet/IP property 22(`COV_Increment`) | write ACK, readback `5.0` | status/value 비교 |
| 0 increment | BACnet/IP property 22(`COV_Increment`) | `BACNET_ERROR_PDU` | expected status 비교 |
| 미만 변화 | BACnet COV observer | `0` notification | confirmed+unconfirmed count |
| 이상 변화 | BACnet COV observer | `>=1` notification | count 비교 |

- 현재 자동 검증 범위: property 22(`COV_Increment`), COV subscribe와 managed source 변화에 따른 notification count를 직접 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 음수 increment, 여러 analog object와 notification payload의 모든 property field는 검사하지 않는다.
- 정리/증적: property read/write, COV packet, source before/after, 원복 결과.
- BLOCKED 조건: live mutation 미승인, Main control 없음, fixture 범위가 경계 시험에 부족.

#### TC-BAC-011 — Binary 상태 변경/무변경 COV

- 실행 명령: 기본 명령; live mutation은 지원하지 않으며 managed 실행이 필요하다.
- 입력/사전조건: managed BO `4:7`, process ID `611`, lifetime 60.
- 수행단계:
  1. vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway 프로세스를 순서대로 기동하고 BO 4:7에 SubscribeCOV한다.
  2. initial notification을 drain한다.
  3. coil 0을 `0`으로 설정한 뒤 notification이 없는지 관찰한다.
  4. coil 0을 `1`로 바꾸고 notification을 관찰한다.
  5. `1`을 다시 설정해 notification이 없는지 확인한 뒤 구독을 취소한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 상태 변경 | BACnet COV observer | `>=1` notification | confirmed+unconfirmed count |
| 상태 무변경 | BACnet COV observer | `0` notification | 두 무변경 구간 count 비교 |

- 현재 자동 검증 범위: BO 4:7의 값 변경과 동일값 재설정에 대한 COV 개수만 검사한다.
- 직접 검증하지 않는 항목/검증 공백: notification payload의 값/시간/순서와 다른 binary object는 직접 검사하지 않는다.
- 정리/증적: Subscribe/Cancel packet, COV count, coil set control trace.
- BLOCKED 조건: managed mode, Main control, managed 실행 파일이 없는 경우.

#### TC-BAC-012 — SubscribeCOV 생성·갱신·취소·lifetime 만료

- 실행 명령: 기본 명령; lifetime 만료 branch는 `--long` 추가.
- 입력/사전조건: 첫 active object, process `701/702/703`, lifetime `60/60/1`.
- 수행단계:
  1. Active_COV_Subscriptions(property 152)의 초기 목록을 읽는다.
  2. process 701 confirmed, process 702 unconfirmed 구독을 순서대로 만들고 목록을 다시 읽는다.
  3. process 701을 refresh하고 active 수가 변하지 않는지 확인한다.
  4. `--long`이면 process 703 lifetime 1을 만들고 2초 뒤 만료를 확인한다.
  5. 701과 702를 취소하고 초기 active 수로 돌아왔는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 생성/refresh | BACnet/IP property 152 | 초기 수에서 +1/+2, refresh 후 +1 | 목록 length 비교 |
| lifetime 만료 | BACnet/IP property 152 | `--long`에서 process 703만 2초 뒤 제거 | before/after length 비교 |
| 취소 | BACnet/IP property 152 | 초기 수로 복귀 | final 목록 비교 |

- 현재 자동 검증 범위: active subscription list의 수와 생성/갱신/취소/1초 lifetime 만료를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 각 subscription의 모든 optional field, notification payload와 60초 subscription의 실제 만료 시점은 직접 검사하지 않는다.
- 정리/증적: SubscribeCOV/CancelCOV, property 152 parsed list, `--long` 대기 시각.
- BLOCKED 조건: `--long`이 없으면 짧은 만료 branch는 실행하지 않으며 PASS가 아니라 정상 branch만 수행한다. live mutation 미승인 또는 managed 의존성 부족도 BLOCKED.

#### TC-BAC-013 — DCC Disable 중 ReadProperty

- 실행 명령: 기본 명령; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: 첫 active object, DCC Enable state `0`, Disable state `1`, duration `1`을 사용한다.
- 수행단계:
  1. DCC Disable을 전송하고 ACK를 확인한다.
  2. 대상 object에 ReadProperty를 전송한다.
  3. 응답이 성공이 아닌 timeout 또는 BACnet error인지 확인한다.
  4. DCC Enable을 전송하고 ACK를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Disable ACK | BACnet/IP | `SUCCESS` | DCC response parse |
| Disable 중 ReadProperty | BACnet/IP | timeout 또는 non-success | status 비교 |
| Enable ACK | BACnet/IP | `SUCCESS` | DCC response parse |

- 현재 자동 검증 범위: Disable 중 한 번의 ReadProperty가 성공하지 않고 Enable ACK가 오는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: Enable 후 별도 ReadProperty로 PV 복구를 확인하지 않는다. duration 만료 경로는 TC-BAC-015가 담당한다.
- 정리/증적: DCC/ReadProperty APDU와 status, cleanup log.
- BLOCKED 조건: live mutation 미승인 또는 managed/live 대상 접근 불가.

#### TC-BAC-014 — DCC Disable Initiation

- 실행 명령: 기본 명령. managed 전용.
- 입력/사전조건: BO 4:7, process 614, DCC state `2`와 Enable state `0`.
- 수행단계:
  1. DCC Disable Initiation을 전송한다.
  2. BO에 SubscribeCOV를 요청한다.
  3. baseline과 coil 0→1 변경 중 COV가 발생하지 않는지 확인한다.
  4. 같은 상태에서 ReadProperty가 성공하는지 확인한다.
  5. DCC Enable 후 coil을 변경해 COV가 복구되는지 확인하고 취소한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Disable Initiation ACK | BACnet/IP | `SUCCESS` | DCC response 비교 |
| 신규 COV 억제 | BACnet observer | `0` notification | baseline/change count 비교 |
| 수신 ReadProperty | BACnet/IP | `SUCCESS` | response status 비교 |
| Enable 후 복구 | BACnet observer | `>=1` notification | after-change count 비교 |

- 현재 자동 검증 범위: COV initiation은 억제하고 incoming ReadProperty는 허용하는지, Enable 후 COV가 복구되는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 다른 서비스의 initiation 정책, notification payload와 모든 object type은 검사하지 않는다.
- 정리/증적: DCC/COV/ReadProperty packet, COV count, coil control trace.
- BLOCKED 조건: managed mode 또는 Main control/실행 파일 부족.

#### TC-BAC-015 — DCC timed Disable 만료

- 실행 명령: 기본 명령 + `--long`; live는 `--allow-live-mutation` 추가.
- 입력/사전조건: `--long` 필수. DCC Disable state `1`, duration `1`을 전송하고 61초를 기다린다.
- 수행단계:
  1. timed Disable ACK를 확인한다.
  2. 61초 동안 기다린다.
  3. 대상 object에 ReadProperty를 보내 `SUCCESS`인지 확인한다.
  4. 마지막으로 명시 Enable ACK를 보내 정리한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| timed Disable | BACnet/IP | `SUCCESS` | DCC response parse |
| timer 대기 | Runner | `61s` | 실제 sleep 후 다음 assertion |
| 만료 후 ReadProperty | BACnet/IP | `SUCCESS` | status 비교 |
| 명시 Enable | BACnet/IP | `SUCCESS` | cleanup response 비교 |

- 현재 자동 검증 범위: 1 단위 timed Disable 후 61초 뒤 통신이 복구되는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 만료 순간의 정확한 경계와 명시 Enable 전후의 별도 PV 비교는 검사하지 않는다.
- 정리/증적: DCC packet, 61초 timestamp, recovery ReadProperty, Enable ACK.
- BLOCKED 조건: `--long` 없음, live mutation 미승인 또는 대상 접근 불가.

#### TC-BAC-016 — Device/Network Port 필수 속성

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 Python runtime, `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 실행한다.
  2. Device/Network Port 필수 속성 검사 결과를 output에서 확인한다.
  3. 종료 code/FAILED/SUCCESS marker를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | `bacnet_profile_tester.py` 프로세스 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| mismatch/completion | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: 프로파일 검증기(`bacnet_profile_tester.py`)의 종합 결과를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 각 Device/Network Port property의 value/type와 설정·실제 network 비교를 자동 검증 프레임워크 (`tc_runner.py`)가 직접 decode하지 않는다.
- 정리/증적: `bacnet_profile_tester.py` 실행 결과(output)/log, config selection, 필요 시 packet capture.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BAC-017 — Network Port read-only 속성 Write

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 실행한다.
  2. read-only property Write와 Changes Pending 결과를 output에서 확인한다.
  3. 종료 code/FAILED/SUCCESS marker를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로파일 검증 스크립트 실행 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 오류/완료 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: read-only write 시나리오를 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 결과로만 판정한다.
- 직접 검증하지 않는 항목/검증 공백: 개별 표준 Error/Abort code (예: Class 2 PROPERTY, Code 31 UNKNOWN_PROPERTY), property 불변, DCC/Block 상태와 Changes Pending 값을 자동 검증 프레임워크 (`tc_runner.py`)가 직접 assertion하지 않는다.
- 정리/증적: adapter log/output, 별도 packet capture가 있으면 추가.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BAC-018 — 미지원 service 오류

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. 지원하지 않는 세 service 요청을 포함한 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 실행한다.
  2. 선언된 표준 오류와 완료 marker를 output에서 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로파일 검증 스크립트 실행 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 실패/완료 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: adapter가 미지원 service 검사를 성공적으로 끝냈다고 보고하는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 세 요청 각각의 `UNRECOGNIZED_SERVICE` 등 오류 code, invoke ID와 transaction 종료를 자동 검증 프레임워크 (`tc_runner.py`)가 직접 parse하지 않는다.
- 정리/증적: `bacnet_profile_tester.py` 실행 결과(output)/log, service별 packet capture(별도 수집 시).
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BAC-019 — COV 최대 동시 구독 용량(20개) 및 한계 초과 거부

- 실행 명령: 기본 명령.
- 입력/사전조건: 첫 active object, 20개의 process ID(`801`~`820`), 초과 process ID `821`.
- 수행단계:
  1. Active_COV_Subscriptions(property 152)의 초기 목록을 읽는다.
  2. 20개의 서로 다른 process ID로 SubscribeCOV를 순차 전송하여 모두 SUCCESS ACK를 받는지 확인한다.
  3. Active_COV_Subscriptions 목록을 읽어 활성 구독 수가 초기값 + 20인지 확인한다.
  4. 21번째 process ID(821)로 SubscribeCOV를 전송하고, 성공이 아닌 오류/Reject로 거부되는지 확인한다.
  5. 첫 번째 구독(801)을 취소(CancelCOV)하여 활성 구독 수가 1 감소함을 확인한다.
  6. 거절되었던 21번째 process ID(821)로 다시 SubscribeCOV를 전송하여 빈 슬롯에 정상 등록(SUCCESS ACK)되는지 확인한다.
  7. 등록된 모든 시험용 구독을 취소(CancelCOV)하여 초기 활성 구독 수로 원상 복구한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 최대 20개 등록 | BACnet/IP property 152 | 20개 모두 `SUCCESS`, 목록 +20 | SubscribeCOV ACK 및 목록 length 비교 |
| 21번째 초과 거부 | BACnet/IP APDU | `NON_SUCCESS` (오류/Reject) | status != SUCCESS 확인 |
| 슬롯 확보 후 등록 | BACnet/IP APDU | `SUCCESS` | 재시도 SubscribeCOV ACK 확인 |
| 정리 복구 | BACnet/IP property 152 | 초기 수로 복귀 | final 목록 비교 |

- 현재 자동 검증 범위: 스택의 최대 동시 COV 구독 한계치(20개), 한계 초과 거절 및 슬롯 반환 후 재구독 정상 동작을 전수 검사한다.
- 정리/증적: 각 SubscribeCOV/CancelCOV APDU 결과 및 property 152 목록 변화.
- BLOCKED 조건: live mutation 미승인 또는 managed 의존성 부족.

#### TC-BAC-020 — Network Port FD 설정 런타임 변경 및 에러 응답

- 실행 명령: 기본 명령 + `--bbmd-endpoint HOST:PORT` 또는 `--bbmd-auto`.
- 입력/사전조건: managed 전용. 외부 BBMD endpoint 또는 `--bbmd-auto` (embedded BBMD server `127.0.0.1:47809`), Gateway 실행 바이너리.
- 수행단계:
  1. Network Port 객체(56, 1)의 Property_List(371)를 읽어 Command(417) 속성이 목록에서 제외되어 있는지 확인하고, Changes_Pending(416)이 FALSE(b"\x10")인지 확인한다.
  2. 잘못된 값 쓰기 및 외부 Command 쓰기 시도시 정확한 BACnet 표준 에러 응답 및 재시작 방지를 검증한다:
     - `BACnet_IP_Mode`(408)에 미지원 모드 `BBMD(2)` 쓰기 시도 → `Class 2 (PROPERTY), Code 37 (VALUE_OUT_OF_RANGE)` 확인.
     - `FD_Subscription_Lifetime`(419)에 0 쓰기 시도 → `Class 2 (PROPERTY), Code 37 (VALUE_OUT_OF_RANGE)` 확인.
     - `FD_BBMD_Address`(418)에 `0.0.0.0:port` 쓰기 시도 → `Class 2 (PROPERTY), Code 37 (VALUE_OUT_OF_RANGE)` 확인.
     - `Command`(417) 외부 쓰기 시도 → `Class 2 (PROPERTY), Code 31 (UNKNOWN_PROPERTY)` 확인.
     - 잘못된 쓰기 후 Gateway 프로세스 재시작이 발생하지 않았음을 확인한다.
  3. `FD_Subscription_Lifetime`을 10초로 WriteProperty 변경하고 SimpleACK를 확인한다. Gateway가 자동 재시작된 후 property 419 값이 10으로 갱신되고, BBMD FDT에 등록된 엔트리의 TTL이 10초로 반영되었음을 확인한다.
  4. 두 번째 embedded BBMD 서버를 기동하고, `FD_BBMD_Address`를 두 번째 BBMD 주소로 WriteProperty 변경한다. Gateway 자동 재시작 후 두 번째 BBMD의 FDT에 정상 등록되었음을 확인한다.
  5. `FD_BBMD_Address`를 원래 첫 번째 BBMD 주소로 다시 WriteProperty 변경한다. Gateway 자동 재시작 후 원래 BBMD의 FDT에 다시 정상 등록되었음을 확인한다.
  6. `BACnet_IP_Mode`를 `NORMAL(0)`으로 WriteProperty 변경한다. Gateway 자동 재시작 후 `bip.cfg` 심볼릭 링크가 `bip.cfg.normal`로 원자적 교체되고, property 408이 0으로 읽히며, Normal 모드에서도 property 418(`FD_BBMD_Address`) 읽기가 성공함을 확인한다. 또한 BBMD FDT의 등록 갱신이 중단되고 등록 연결 로그가 추가 출력되지 않음을 확인한다.
  7. `BACnet_IP_Mode`를 `FOREIGN(1)`으로 WriteProperty 변경한다. Gateway 자동 재시작 후 `bip.cfg` 심볼릭 링크가 `bip.cfg.fd`로 교체되고, BBMD FDT에 다시 정상 등록되었음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Command 제외 / Changes_Pending | property 371 / 416 | 417 미포함, Changes_Pending=FALSE | 목록 및 ASN.1 boolean 비교 |
| 잘못된 쓰기 표준 에러 | WriteProperty APDU 응답 | `VALUE_OUT_OF_RANGE` (408=2, 419=0, 418=0.0.0.0) | Error Class 2, Code 37 assertion |
| Command 외부 쓰기 거부 | WriteProperty APDU 응답 | `UNKNOWN_PROPERTY` (417) | Error Class 2, Code 31 assertion |
| 에러 시 재시작 방지 | Gateway 로그 카운트 | 시작 로그 증가 없음 | started 카운트 전후 일치 |
| TTL 런타임 변경 | WriteProperty 419 + FDT | TTL=10 반영 및 FDT 일치 | property 419 값 및 BBMD FDT TTL 비교 |
| BBMD 주소 변경/복구 | WriteProperty 418 + FDT | 새 BBMD 등록 및 원복 등록 | BBMD1/BBMD2 FDT 엔트리 검증 |
| Normal / Foreign 모드 전환 | WriteProperty 408 + 심볼릭 링크 | `bip.cfg` 링크 전환, FDT 갱신 중단/재개 | readlink 및 FDT 잔여시간 감시 |

- 현재 자동 검증 범위: Network Port 객체의 런타임 설정 변경(Mode, BBMD 주소, TTL), 유효하지 않은 값 및 Command 외부 쓰기에 대한 표준 에러 응답 코드, 설정 파일 갱신 및 안전 자동 재시작 메커니즘을 전수 검사한다.
- 직접 검증하지 않는 항목/검증 공백: BACstac 라이브러리 내부의 원자적 파일 I/O 실패(OS 디스크 풀 등) 시 롤백은 모의하지 않는다.
- 정리/증적: 각 WriteProperty 요청/응답 APDU, `bip.cfg` 심볼릭 링크 타겟, BBMD1/BBMD2 FDT 덤프 및 Gateway 프로세스 로그.
- BLOCKED 조건: managed 의존성 부족 또는 BBMD 준비 실패.

#### TC-IFCFG-001 — 기본/명시 config directory 계약

- 실행 명령: 기본 명령.
- 입력/사전조건: `src/gateway/main.cpp` 소스 파일 및 커스텀/미존재 디렉터리 경로.
- 수행단계:
  1. 실제 파일 내용(`src/gateway/main.cpp`)에서 기본 설정 문자열과 argv 처리 부분을 확인한다.
  2. 기본 경로가 `./conf`로 설정되고, 명시적 인자가 `argv[1]`을 통해 파싱(`config_dir = argv[1]`)되는지 확인한다.
  3. 존재하지 않는 디렉터리 경로 전달 시 즉시 비정상 종료되는 대신 Crash Loop 방지용 ConfigHold 상태로 안전하게 진입함을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 기본 경로 | source text | `std::string config_dir = "./conf"` | exact substring 검사 |
| 명시 경로 계약 | source text | `config_dir = argv[1]` | exact substring 검사 |
| 미존재 디렉터리 HOLD 생존 | Gateway 프로세스 | True | 프로세스가 즉시 종료되지 않고 HOLD 상태로 생존하는지 확인 |
| 디렉터리 오류 진단 로그 | Gateway 에러 로그 | `Failed to load gateway config` | 오류 로그 검출 확인 |
| 디렉터리 오류 중 BACnet 미기동 | Gateway 프로세스 로그 | False | HOLD 상태 동안 BACnet 서비스가 시작되지 않음을 확인 |

- 현재 자동 검증 범위: 소스 계약뿐만 아니라 실제 Gateway 바이너리를 실행하여 커스텀 인자 수용 및 비정상 디렉터리/설정 시 Crash Loop 방지용 ConfigHold 진입 및 진단 출력을 실동작으로 검증한다.
- 직접 검증하지 않는 항목/검증 공백: OS 권한 부족(Permission Denied)에 따른 디렉터리 접근 거부는 별도 보안 테스트 범위에서 다룬다.
- 정리/증적: source checksum, matched line, 실행 로그, 결과 JSON.
- BLOCKED 조건: 소스 파일이 없는 경우에는 FAIL/예외로 기록될 수 있다.

#### TC-IFCFG-002 — serial/COV/cache 경계 6종

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일(`gateway.json` 및 `profiles/*.json`)의 `gateway.json`을 수정하여 다음 6가지 설정을 순서대로 적용한다.
- 수행단계:
  1. `serial-low`: 4800bps, 5 data bits, none, 1 stop bit를 저장한다.
  2. `serial-high`: 115200bps, 8 data bits, odd, 2 stop bits를 저장한다.
  3. `cov-low/high`: `0.1/10.0`을 저장한다.
  4. `cache-off/on`: `0/3600`을 저장한다.
  5. 각 설정마다 통합 테스트 환경(vport + Main 시뮬레이터 + Gateway)을 재기동하고, 기대하는 로그 메시지와 `BACnet IP Gateway started` 로그가 정상 출력되는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| serial | Gateway log | `@4800bps`, `@115200bps` | variant별 문자열 비교 |
| COV polling | Gateway log | `cov_polling=100ms`, `10000ms` | 문자열 비교 |
| cache | Gateway log | `cache_sec=0s`, `3600s` | 문자열 비교 |
| 기동 | Gateway log | started 존재 | variant별 started assertion |

- 현재 자동 검증 범위: 여섯 설정 variant의 로그 표현과 startup 성공을 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 serial baud/data bit가 kernel device에 적용되었는지와 COV/cache runtime 성능은 직접 측정하지 않는다.
- 정리/증적: variant별 gateway.json, Gateway/vport/Main log, process 종료 상태.
- BLOCKED 조건: managed Gateway/vport/Main 실행 파일 부족.

#### TC-IFCFG-003 — vport Master 수와 timeout log

- 실행 명령: 기본 명령.
- 입력/사전조건: managed vport 설정과 `DEFAULT_SLAVE_TIMEOUT_MS=500`.
- 수행단계:
  1. vport 가상 시리얼 포트 에뮬레이터를 1 Master(종단 1개) 모드로 기동한다.
  2. 시스템에 `ttyMASTER1` 심볼릭 링크가 존재하고 `ttyMASTER2`가 없음을 확인하며, vport 로그 파일에 `500ms` 타임아웃 값이 기록되었는지 확인한다.
  3. vport 프로세스를 종료한 뒤 3 Master 모드로 다시 기동한다.
  4. 시스템에 `ttyMASTER3` 심볼릭 링크가 생성되었는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| single Master | filesystem | MASTER1 존재, MASTER2 없음 | link 비교 |
| timeout | vport log | `500ms` | log substring 대기 |
| three Master | filesystem | MASTER3 존재 | link assertion |

- 현재 자동 검증 범위: 1/3 Master link와 timeout log를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: timeout이 실제 Slave transaction에 적용되는지, MASTER1/2 모든 link 권한은 직접 검사하지 않는다.
- 정리/증적: vport log, link snapshots, config.
- BLOCKED 조건: managed vport 실행 파일 부족.

#### TC-BIP-001 — 기본 UDP port와 I-Am

- 실행 명령: 기본 명령.
- 입력/사전조건: 기본 target IP/port `127.0.0.1:47808` 또는 명시한 `--ip/--port`.
- 수행단계:
  1. 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 또는 live Gateway 프로세스를 준비한다.
  2. 설정된 기본 포트(47808)로 타겟 제한 없는(unbounded) BACnet Who-Is 패킷을 전송한다.
  3. 수신된 I-Am 응답 개수(1개 기대)와 송신자 IP/Port 및 Device ID 필드를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| I-Am | BACnet/IP | 1개, `SUCCESS` | native Who-Is assertion |
| endpoint/field | UDP/APDU | target port, device ID, APDU/segmentation/vendor 일치 | parsed field 비교 |

- 현재 자동 검증 범위: 기본 BACnet/IP endpoint와 I-Am packet field를 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: port bind 자체를 socket inspection으로 분리 검사하지 않으며 service/object bit는 BIP-003/adapter 범위다.
- 정리/증적: tx/rx packet, sender, parsed I-Am, startup log.
- BLOCKED 조건: managed 의존성 부족 또는 live target 접근 불가.

#### TC-BIP-002 — 독립 네트워크 모드(Normal / FD) 기동 및 BBMD 등록 유지

- 실행 명령: 기본 명령 + `--bbmd-endpoint HOST:PORT` 또는 `--bbmd-auto`.
- 입력/사전조건: managed 전용. 외부 BBMD endpoint 또는 `--bbmd-auto`가 필요하다. auto는 자동 검증 프레임워크 (`tc_runner.py`)가 `127.0.0.1:47809` embedded BBMD server를 시작한다.
- 수행단계:
  1. BBMD probe와 endpoint 응답을 확인한다.
  2. `normal` 모드와 `fd` 모드를 각각 설정 파일(`write_bip_config`)에 적용한 뒤, 통합 테스트 환경(vport + Main 시뮬레이터 + Gateway)을 독립적으로 순서대로 기동한다.
  3. BACnet Network Port 객체의 property 408(`BACnet_IP_Mode`), 371(`Property_List`), 416(`Changes_Pending`, FALSE) 및 표준 필수 속성들(Network_Type 427, Protocol_Level 482, IP_Address 400 등)을 ReadProperty로 읽어 정합성을 확인한다.
  4. 두 모드 모두에서 property 418/419(`FD_BBMD_Address`/`FD_Subscription_Lifetime`) 읽기가 성공해야 하며, `fd` 모드에서는 외부 BBMD 장비의 FDT 내 Gateway 등록 정보를 검증한다.
  5. BBMD 서버를 강제 중지하여 통신 단절을 유도하고, 게이트웨이 로그에 `registration failed or lost` 단절 진단 메시지가 출력되는지 확인한다.
  6. BBMD 서버를 재기동하여 복구하고, 게이트웨이 로그에 `Registered with BBMD` 연결 메시지와 함께 BBMD FDT에 자동 재등록됨을 확인한다.
  7. (`--long`) `FD_Subscription_Lifetime`을 600초(10분)로 설정하고 기동한 환경에서, TTL 만료(600초) 시점이 아닌 5분(약 300초) 주기로 Gateway가 직접 `Register-Foreign-Device`를 보내 BBMD FDT의 잔여시간(`remaining`)이 약 300초 경과 시점에 다시 600초로 갱신되는지 확인한다 (약 6분 소요, TC 제한시간 600초).
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| mode 기동 일치 | BACnet/IP property 408(`BACnet_IP_Mode`) | normal=0, fd=1 | mode별 exact 비교 |
| 필수 속성 일관성 | property 371/416/418/419/표준속성 | Changes_Pending=FALSE, FD 속성 읽기 성공 | 값/상태 assertion |
| fd 등록 확인 | property 418/419 + BBMD FDT | endpoint/lifetime>0/Gateway entry 존재 | field/entry 비교 |
| BBMD 단절/복구 로그 | Gateway 로그 + BBMD FDT | 단절 로그 출력, 재기동 후 `Registered with BBMD` 및 FDT 재등록 | 로그 문자열 및 FDT entry 비교 |
| 5분 주기 재등록 (`--long`) | BBMD FDT 잔여 시간 갱신 | ~300초 경과 시점에 remaining 갱신 | FDT remaining 변화 시간(270~330s) 검증 |

- 현재 자동 검증 범위: 설정 파일 기반 Normal/FD 모드 기동, Network Port 필수/FD 속성 조회, BBMD FDT 등록, BBMD 통신 장애 감지 및 복구 시 재등록, 5분(300초) 상한 주기 재등록 메커니즘을 native로 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 다중 서브넷 라우터 환경에서의 broadcast forwarding 및 다중 FD 동시 등록 부하는 직접 검사하지 않는다.
- 정리/증적: BBMD probe/FDT 덤프, mode별 Network Port 응답 APDU, generated `bip.cfg`, Gateway 로그.
- BLOCKED 조건: BBMD endpoint/auto 없음, managed 실행 파일 부족.

#### TC-BIP-003 — 지원 service/object bit

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 Python runtime, `bacnet_profile_tester.py`.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 실행한다.
  2. Device의 service/object bit와 실제 요청 결과 비교가 `bacnet_profile_tester.py` 실행 결과(output)에 포함되었는지 확인한다.
  3. return code/FAILED/SUCCESS marker를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로파일 검증 스크립트 실행 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 정합성/완료 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: bit 비교를 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트에 위임하고 종합 출력 세 가지를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: Runner는 광고 bit별 실제 APDU와의 1:1 관계를 직접 decode하지 않는다.
- 정리/증적: `bacnet_profile_tester.py` 실행 결과(output)/log, Device response가 있으면 추가 capture.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-AVL-001 — runtime vport 재시작 후 복구

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), 대상 object AO 5, vport 3 Master.
- 수행단계:
  1. 사전 확인(baseline)을 위해 대상 객체(AO 5)의 property 85(Present_Value)를 ReadProperty로 읽는다.
  2. vport 가상 시리얼 프로세스를 강제 종료(SIGKILL)하고 기존 가상 포트(stale link) 단절 상태를 확인한다.
  3. vport 프로세스를 3 Master 모드로 다시 기동하여, 변경된 PID와 `ttySLAVE` 심볼릭 링크 생성 여부를 확인한다.
  4. 최대 12초 동안 BACnet ReadProperty 요청을 반복 송신하며, 첫 번째 `SUCCESS` 응답을 수신한 시각(복구 시간)을 측정한다.
  5. Gateway log의 serial reconnect 문구를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 단절/재기동 | process/filesystem | old dead, stale link, new PID | 상태 비교 |
| 요청 복구 | BACnet/IP | 12초 안에 `SUCCESS` | attempt loop 비교 |
| reconnect | Gateway log | reconnect 문구 존재 | substring assertion |

- 현재 자동 검증 범위: vport 강제 종료, 새 PID/link, BACnet 요청 복구와 reconnect log를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 reopen 내부의 정확한 syscall/재시도 횟수와 모든 object 복구는 검사하지 않는다.
- 정리/증적: recovery attempts/latency, PID/link snapshot, BACnet/Modbus packet, Gateway log.
- BLOCKED 조건: managed Gateway/vport/Main 실행 파일 부족.

#### TC-AVL-002 — Gateway 중지 후 다른 process 생존

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway).
- 수행단계:
  1. 통합 테스트 환경(vport + Main 시뮬레이터 + Gateway)을 정상 기동한다.
  2. 정상적으로 서비스 중인 Gateway 프로세스에 SIGTERM 시그널을 전송하여 graceful shutdown(정상 종료)을 유도한다.
  3. 자동 검증 프레임워크(`tc_runner.py`)를 통해 생성된 Main 시뮬레이터 프로세스와 vport 프로세스가 여전히 실행 상태(alive)를 유지하는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Main 생존 | process poll | alive | `require` 비교; 통과 시 visible assertion 없음 |
| vport 생존 | process poll | alive | `require` 비교 |

- 현재 자동 검증 범위: Gateway 중지 뒤 자동 검증 프레임워크 (`tc_runner.py`)가 시작한 Main/vport process가 살아 있는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: Gateway crash 시 실제 BACnet 서비스 상태, Main의 새 Modbus transaction과 기존 protocol simulator 기능은 검사하지 않는다.
- 정리/증적: process PID/status, shutdown log.
- BLOCKED 조건: managed 실행 파일 부족.

#### TC-AVL-003 — Gateway 종료 시간

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway).
- 수행단계:
  1. 통합 테스트 환경(vport + Main 시뮬레이터 + Gateway)을 정상 기동한다.
  2. Gateway 프로세스에 SIGTERM 시그널을 보내고, 프로세스가 완전히 종료될 때까지의 소요 시간(3.0초 이내 기대)을 측정한다.
  3. 테스트 러너(`tc_runner.py`)의 정리(cleanup) 작업 완료 후, 잔류 프로세스나 가상 포트 링크가 없는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| Gateway shutdown | process | `<3.0s` | elapsed 비교 |

- 현재 자동 검증 범위: 정상 startup 후 Gateway 종료가 3초 이내인지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 초기화 중 SIGTERM, FD/thread leak와 모든 PTY link 정리는 별도 assertion으로 확인하지 않는다.
- 정리/증적: 종료 timestamp, Gateway log, process/link snapshot.
- BLOCKED 조건: managed 실행 파일 부족.

#### TC-AVL-004 — 12시간 전체 관제점 장기 연속 운용 신뢰성

- 실행 명령: `python3 verification/scripts/run_gateway_tcs.py --case TC-AVL-004 --soak --soak-hours 12.0`
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 또는 Live 대상 장비, 로드된 장치 프로필 (`gateway.json`, `profiles/*.json`). `--soak` 또는 `--long` 옵션 필수 (미지정 시 BLOCKED).
- 수행단계:
  1. 통합 테스트 환경 또는 라이브 런타임을 준비하고 전체 관제점(Managed 픽스처 또는 대상 제품 프로필의 전체 객체) 목록을 로드한다.
  2. 30초 웜업 후 Gateway 및 vport 프로세스의 기준 메모리(RSS) 및 열린 파일 디스크립터(FD) 수를 측정한다. (Live 모드 시 SSH 원격 수집 지원)
  3. 설정된 테스트 시간(기본 12시간, 43,200초) 동안 설정된 주기(기본 15초)마다 전체 관제점에 대해 BACnet ReadProperty 질의를 반복 수행한다.
  4. 매 주기마다 Gateway 및 vport 프로세스의 생존 상태(비정상 종료 여부)를 확인하고, 각각의 메모리(RSS) 및 FD 수를 샘플링하여 시계열 메트릭을 기록한다.
  5. 12시간 경과 후 전체 통신 성공률(>=99.9%), 프로세스 무중단 생존(Gateway & vport), 메모리 누수율(Gateway/vport 모두 < 5% 성장 또는 < 2MB), 파일 디스크립터 누수량(Gateway/vport 모두 <= 0)을 단언 검증한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 프로세스 무중단 생존 | Gateway & vport process | `No Crash/Zombie` | 매 주기 Gateway 및 vport 프로세스 생존 검증 |
| 전체 관제점 통신 성공률 | BACnet ReadProperty 응답 | `>= 99.9%` | 12시간 전체 요청 중 성공률 계산 |
| 메모리(RSS) 누수 억제 | `/proc/{pid}/status` (Gateway & vport VmRSS) | `< 5.0%` 성장 또는 `< 2MB` | Gateway 및 vport 각각 기준치 대비 최종 RSS 증가율 비교 |
| 파일 디스크립터(FD) 누수 | `/proc/{pid}/fd` (Gateway & vport FD) | `<= 0` 증가 | Gateway 및 vport 각각 기준치 대비 최종 FD 증감 비교 |

- 현재 자동 검증 범위: 12시간 연속 운용 동안 전체 관제점에 대한 통신 무결성, Gateway 및 vport 프로세스 동시 생존성, 두 프로세스의 메모리 누수 및 FD 누수를 E2E로 완전 검증한다. (Live 모드 시 SSH 원격 OS 자원 모니터링 연동)
- 직접 검증하지 않는 항목/검증 공백: 커널 패닉 및 물리 전원 차단 외란은 H/W 신뢰성 시험 범위로 위임한다.
- 정리/증적: 시계열 자원 샘플 메트릭, 통신 성공/실패 통계, Gateway 로그 스냅샷.
- BLOCKED 조건: `--soak` 또는 `--long` 옵션 미지정.

#### TC-NET-001 — 링크 DOWN→UP(같은 IPv4) 복구

- 실행 명령: `run_test.sh` managed, `--case TC-NET-001` (root·독립 네트워크 컨테이너 자동 구성).
- 입력/사전조건: 컨테이너 내부 netns `bacgw` 의 `vgw0`(10.77.0.2/24) ↔ 러너 `vrun0`(10.77.0.1/24) veth 구성, `bip.cfg` 에 `iface=vgw0`, vport + Main 시뮬레이터는 러너 쪽, Gateway 는 `ip netns exec bacgw` 로 기동.
- 수행단계:
  1. Device Object_Name ReadProperty 로 기준 응답을 확인한다.
  2. `vgw0` 를 DOWN 하고 `[Net] Link down` 로그(10초 이내)와 BACnet 무응답을 확인한다.
  3. 10초 유지 후 프로세스 생존을 확인하고 `vgw0` 를 UP 한다.
  4. 0.5초 간격 단발 ReadProperty 로 응답 재개 시간을 측정한다.
  5. `[Net] Link up` 로그, 재시작 횟수 불변, Network Port Reliability(103)=0, Who-Is/I-Am 을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 감지 | Gateway log | Link down/up 로그 | substring assertion |
| 복구 | BACnet/IP | UP 후 10초 이내 SUCCESS | elapsed 비교 |
| 재시작 없음 | Gateway log | started 횟수 불변 | count 비교 |
| Reliability | BACnet/IP | NO_FAULT_DETECTED(0) | enum 비교 |

- 현재 자동 검증 범위: 같은 IPv4 링크 단절/복구 시 재시작 없는 자동 복구와 감지 로그.
- 직접 검증하지 않는 항목/검증 공백: 실제 PHY 캐리어 손실(케이블 분리)과 장시간 단절 중 COV 구독 유지.
- 정리/증적: `network_recovery`(감지/복구 시간), BACnet 패킷, Gateway log.
- BLOCKED 조건: managed 가 아니거나 root/NET_ADMIN 컨테이너가 아님(`net_admin` capability 없음).

#### TC-NET-002 — 운용 중 IPv4 변경 추종

- 실행 명령: `run_test.sh` managed, `--case TC-NET-002`.
- 입력/사전조건: TC-NET-001 과 같은 netns 구성, 초기 IPv4 10.77.0.2/24, `iface=vgw0`.
- 수행단계:
  1. 기준 응답을 확인한다.
  2. `vgw0` 의 IPv4 를 10.77.0.3/24 로 교체한다.
  3. `[Net] IPv4 changed` 로그(10초 이내)와 Gateway 재시작 1회, `BACnet/IP bind address: 10.77.0.3/255.255.255.0 (iface=vgw0)` 로그를 확인한다.
  4. 새 IPv4 로 ReadProperty 응답(변경 후 30초 이내), Network Port IP_Address=10.77.0.3, `10.77.0.255` directed broadcast Who-Is 에 새 IPv4 의 I-Am 을 확인한다.
  5. 옛 IPv4(10.77.0.2) 요청이 응답하지 않음을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 감지·재바인딩 | Gateway log | IPv4 changed, started +1, bind address 로그 | substring/count |
| 새 주소 응답 | BACnet/IP | 30초 이내 SUCCESS | elapsed 비교 |
| 속성/broadcast | BACnet/IP | IP_Address 일치, I-Am 송신 주소 = 새 IPv4 | 값 비교 |
| 옛 주소 | BACnet/IP | 무응답 | status 비교 |

- 현재 자동 검증 범위: 지정 인터페이스 IPv4 교체 시 재바인딩과 서브넷 broadcast 갱신.
- 직접 검증하지 않는 항목/검증 공백: DHCP 클라이언트 연동, FD 모드의 IPv4 변경 후 BBMD 재등록.
- 정리/증적: `network_recovery`(감지/재바인딩 시간), BACnet 패킷, Gateway log.
- BLOCKED 조건: TC-NET-001 과 같음.

#### TC-NET-003 — 기동 시 네트워크 미준비(IPv4 미할당)

- 실행 명령: `run_test.sh` managed, `--case TC-NET-003`.
- 입력/사전조건: netns 구성 후 `vgw0` 의 IPv4 를 제거한 상태(링크 UP), `iface=vgw0`.
- 수행단계:
  1. Gateway 를 기동하고 8초 이내 `Waiting for network` 로그를 확인한다.
  2. 10초 대기 후 프로세스가 살아 있고 BACnet 서버가 기동하지 않았음을 확인한다.
  3. `vgw0` 에 10.77.0.2/24 를 할당하고 15초 이내 `BACnet IP Gateway started` 와 BACnet 응답을 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 대기 | Gateway log / process | Waiting 로그, 생존, 미기동 | substring/poll/count |
| 기동 | Gateway log / BACnet/IP | 할당 후 15초 이내 started, SUCCESS | elapsed 비교 |

- 현재 자동 검증 범위: 기동 시 지정 인터페이스 IPv4 미할당 대기와 할당 후 기동.
- 직접 검증하지 않는 항목/검증 공백: 인터페이스 자체가 존재하지 않는 경우(USB NIC 미장착)와 systemd 재시작 정책.
- 정리/증적: `network_recovery`(할당 후 기동 시간), Gateway log.
- BLOCKED 조건: TC-NET-001 과 같음.

#### TC-NET-004 — FD 모드 링크 복구 후 BBMD 재등록

- 실행 명령: `run_test.sh` managed, `--case TC-NET-004`.
- 입력/사전조건: 러너 쪽 BBMDServer(10.77.0.1:47809, netns/veth 구성 직후·게이트웨이 기동 전에 bind), `bip.cfg.fd` FD 모드 TTL=600초, `iface=vgw0`.
- 수행단계:
  1. 초기 BBMD FDT 등록(15초 이내)을 확인한다.
  2. `vgw0` 를 15초 DOWN 후 UP 한다.
  3. UP 이후 BBMD 가 새 Register-Foreign-Device 를 수신한 시각(FDT 등록 시각 갱신)을 측정한다.
  4. `[FD] Network link recovered. Re-registering` 로그를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 재등록 | BBMD FDT | UP 후 10초 이내 등록 시각 갱신 | timestamp 비교 |
| 로그 | Gateway log | 즉시 재등록 로그 | substring assertion |

- 현재 자동 검증 범위: 링크 복구 시 TTL/300초 주기를 기다리지 않는 즉시 재등록.
- 직접 검증하지 않는 항목/검증 공백: BBMD 측 FDT 만료 이후 broadcast 분배 복구.
- 정리/증적: `network_recovery`(재등록 시간), BBMD/Gateway log.
- BLOCKED 조건: TC-NET-001 과 같음.

#### TC-PERF-001 — 100회 순차 ReadProperty benchmark

- 실행 명령: 기본 명령. 지연 입력을 시험하면 `--response-time-ms <ms>`를 추가한다.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), 승인된 `verification/scripts/performance_policy.json`.
- 수행단계:
  1. [100회 연속 조회] 첫 번째 활성 관제점을 대상으로 순차적(sequential) BACnet ReadProperty 요청을 100회 연속 전송한다.
  2. [메트릭 수집] 각 요청의 응답 상태(SUCCESS/FAIL)와 왕복 지연시간(latency, ms)을 정밀 측정하여 기록한다.
  3. [통계 지표 계산] 성공률(success rate), 초당 처리량(throughput, rps), P95 지연시간, P99 지연시간을 계산한다.
  4. [정책 기준선 대조] 성능 정책(`performance_policy.json`) 기준과 대조하여 합격 여부를 판정한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 성공률 (success rate) | BACnet/IP metric | `>= 95%` (100회 중 95회 이상 정상 응답) | metric 비교 |
| 처리량 (throughput) | BACnet/IP metric | `>= 10 rps` (초당 10건 이상 순차 처리) | metric 비교 |
| 지연시간 (P95 / P99) | latency metric | P95 `<= 100ms`, P99 `<= 250ms` | percentile 비교 |

- 💡 **지표 용어 안내**: **P95**는 전체 요청 중 빠른 순서로 95%가 완료된 응답 시간(대다수 사용자의 체감 성능), **P99**는 최악 1%를 제외한 99%의 요청이 완료된 상한 시간(간헐적 튐을 감시하는 SLA 기준선)입니다.
- 현재 자동 검증 범위: 평상시 정상 트래픽 환경에서 100회 순차 요청의 지연시간 및 처리량이 기준선을 만족하는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 동시성 멀티스레드 부하 및 COV 이벤트 경합 상태는 이 카드에서 검사하지 않는다.
- 정리/증적: request별 latency/status, `performance_metrics`, policy snapshot, packet trace.
- BLOCKED 조건: 승인된 performance policy 또는 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)이 없음.

#### TC-PERF-002 — COV 부하 중 foreground Read/Write (제어 명령 기아 방지)

- 실행 명령: 기본 명령. 필요 시 `--response-time-ms <ms>` 추가.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), 승인된 PERF-002 policy, 제어 가능한 Modbus source와 쓰기 가능한 AO/BV 객체.
- 수행단계:
  1. [COV 구독 등록] 최대 4개의 활성 객체를 대상으로 Confirmed 및 Unconfirmed SubscribeCOV 요청을 전송하여 구독을 개설한다.
  2. [백그라운드 부하 유발] 별도 워커 스레드가 100ms 주기마다 Modbus 레지스터 값을 강제 변경하여 대량의 COV 알림 트래픽을 지속 발생시킨다.
  3. [포그라운드 R/W 전송] COV 부하가 유지되는 10초 동안 포그라운드에서 실시간 조회(ReadProperty)와 긴급 제어(WriteProperty Priority=1)를 10ms 간격으로 교차 전송한다.
  4. [기아 방지 및 성능 대조] COV 발생 중에도 포그라운드 명령이 지연/누락되지 않고 정해진 성공률/처리량/지연시간(P95/P99)을 만족하는지 검증한다.
  5. [구독 정리] 개설된 모든 SubscribeCOV 구독을 정상 취소(lifetime=0)한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| COV 부하 유발 확인 | BACnet observer | `>= 1` 건 (COV 알림 패킷 수신 확인) | count 비교 |
| 포그라운드 성공률 / 처리량 | BACnet/IP metric | 성공률 `>= 90%`, 처리량 `>= 3 rps` (명령 기아 방지) | policy 비교 |
| 포그라운드 지연시간 | foreground metric | P95 `<= 500ms`, P99 `<= 1000ms` (COV 경합 시 상한선) | percentile 비교 |

- 💡 **지표 용어 안내**: **P95 <= 500ms**는 COV 폭주 중에도 95%의 제어/조회 명령이 0.5초 내에 처리됨을 뜻하며, **P99 <= 1000ms**는 극심한 통신 경합 시에도 99%의 요청이 1초를 넘지 않음을 보장합니다.
- 현재 자동 검증 범위: 대량의 COV 알림 트래픽 속에서 포그라운드 고우선순위 제어 명령이 시리얼 버스 기아(starvation) 없이 정상 응답하는지 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 10초를 초과하는 장시간 COV 스트레스나 모든 구독 패킷 필드의 전수 비트 비교는 검사하지 않는다.
- 정리/증적: subscription 목록, COV counts, foreground metrics, worker errors, policy snapshot.
- BLOCKED 조건: 승인 policy, Main control, mutation capability 또는 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 부족.

#### TC-PERF-003 — Main 타임아웃 큐 포화와 복구 (장애 회복력)

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), 승인된 PERF-003 policy, Main 시뮬레이터 timeout 결함 주입 기능.
- 수행단계:
  1. [고의 결함 주입] Main 시뮬레이터(`modbus_rtu_slave.py`)에 타임아웃(timeout) 결함을 주입하여 Modbus 트랜잭션이 전혀 응답하지 않는 먹통 상태를 만든다.
  2. [집중 부하 인가 / 큐 포화] 16개의 병렬 워커를 사용해 BACnet ReadProperty 64개를 동시에 집중 전송하여 게이트웨이 내부 큐(queue)를 가득 채운다.
  3. [무한 대기 방지] 모든 비동기 요청이 데드락(무한 루프)에 빠지지 않고 최대 20초 이내에 완료(타임아웃 에러 반환)되는지 확인한다.
  4. [결함 에러 검출] 하위 기기 장애 상황이므로 실패(Timeout 에러) 응답이 1건 이상(사실상 전원) 정상 발생했는지 증적을 확인한다.
  5. [자동 정상 복구] Main 시뮬레이터 결함을 해제한 즉시 단일 ReadProperty를 전송하여 'SUCCESS'로 즉각 복구되는지 확인한다.
  6. [지연 한도 대조] 의도된 장애 테스트이므로 성공률/처리량 기준은 0.0이며, 에러 반환 지연시간이 정책 한도(P95<=3초, P99<=5초) 내에 빠져나왔는지 검증한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 무한 대기 방지 (bounded completion) | BACnet/IP/process | 64개 요청 모두 20초 이내 완료 (무한 대기/데드락 없음) | count/time 비교 |
| 장애 감지 증적 (failure evidence) | BACnet/IP | 1건 이상 실패 응답 (장애 상태 정상 감지) | failed count 비교 |
| 정상 상태 복구 (recovery) | BACnet/IP | 결함 해제 즉시 정상 `SUCCESS` 응답으로 복원 | ReadProperty 비교 |
| 지연 한도 대조 (metric limits) | performance policy | P95 `<= 3000ms`, P99 `<= 5000ms` (의도된 장애 상태이므로 성공률/처리량 하한은 0.0) | `_assert_performance_metrics`로 policy limits 대조 |

- 💡 **지표 용어 및 기준 안내**: 결함 주입 상태이므로 요청 실패가 정상이어서 성공률 기준은 0%입니다. **P95 <= 3초, P99 <= 5초**는 하위 장치가 죽었더라도 게이트웨이가 클라이언트 요청을 영구 방치하지 않고 소켓 타임아웃 내에 안전하게 에러 응답을 매듭짓고 빠져나왔음을 보증합니다.
- 현재 자동 검증 범위: 하위 기기 무응답 상태에서 64개 큐 포화 시의 20초 내 안전 종료, 에러 반환, 결함 해제 후 즉각적인 자가 치유(self-healing) 복구 능력을 종합 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 내부 큐 세부 락(lock) 경합 시간이나 OS 수준의 소켓 버퍼 점유율은 직접 측정하지 않는다.
- 정리/증적: request별 status/latency, timeout transaction, recovery packet, `performance_metrics`.
- BLOCKED 조건: 승인 policy, Main control 또는 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 부족.

#### TC-OBS-001 — Min/Max HOLD 진단 log

- 실행 명령: 기본 명령 + `--long`.
- 입력/사전조건: CFG-006과 같은 `min=100`, `max=10` profile. `--long` 없으면 CFG-006 호출 자체가 `BLOCKED`다.
- 수행단계:
  1. 최소/최대값이 모순(min=100, max=10)되도록 설정된 프로파일을 사용해 CFG-006 테스트 경로를 실행한다.
  2. 기동된 Gateway의 로그에서 `Waiting 60 seconds`, `min_value`, `max_value` 문자열이 포함된 진단 메시지를 확인한다.
  3. 모순 설정에 따른 HOLD 상태 진입 및 리로드 보류 판정 결과(CFG-006 검증 로직 위임)를 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 재검증 안내 | Gateway log | `Waiting 60 seconds` | substring 존재 |
| 오류 문맥 | Gateway log | `min_value`, `max_value` 포함 | substring 존재 |
| CFG 복구 | CFG-006 assertion | 해당 카드 기준 충족 | delegated result |

- 현재 자동 검증 범위: CFG-006 결과와 HOLD log의 필수 문구를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 반복 log의 정확한 line rate, 각 retry의 1회 출력 여부와 모든 진단 field는 직접 세지 않는다.
- 정리/증적: HOLD log, timestamp, CFG-006 결과.
- BLOCKED 조건: `--long` 없음 또는 managed 실행 파일 부족.

#### TC-OBS-002 — JSON/Main/clamp startup trace

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`); `BACNET_LOG=1`이 적용된 startup log와 Main 값 3=`40`, 4=`110`.
- 수행단계:
  1. 통합 테스트 환경 (vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터, Gateway 프로세스)을 순서대로 기동하고 프로세스별 정상 동작 로그를 확인한다.
  2. Gateway 프로세스의 초기화(startup) 로그를 수집한다.
  3. 수집된 초기화 로그 내에서 초기화 단계별 트레이스 문자열(`Relinquish`, `clamp`, `Synchronized`)이 정상 출력되었는지 검사한다.
  4. (해당 항목은 자동 검증 대상이 아니며, 필요한 경우) 사람이 Modbus 트랜잭션 로그와 함께 source->raw->final 변환 흐름을 수동 대조한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| default/clamp trace | Gateway log | `Relinquish`, `clamp` 포함 | token 존재 assertion |
| 완료 trace | Gateway log | `Synchronized` 포함 | token 존재 assertion |

- 현재 자동 검증 범위: startup log 세 token을 검사한다.
- 직접 검증하지 않는 항목/검증 공백: source→raw→final→write/progress의 각 값과 Modbus FC03/FC06 대응 관계를 자동 검증 프레임워크 (`tc_runner.py`)가 직접 비교하지 않는다. 별도 BACnet 요청도 보내지 않는다.
- 정리/증적: Gateway startup log, Main/Modbus transaction, packet evidence, 결과 JSON.
- BLOCKED 조건: managed Gateway/vport/Main 실행 파일 부족.

#### TC-OBS-003 — 600초 Main 무응답 관찰

- 실행 명령: 기본 명령 + `--long`.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway), Main timeout fault 주입 가능. `--long`이 없으면 `BLOCKED`다.
- 수행단계:
  1. 통합 테스트 환경(vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터, Gateway 프로세스)을 순서대로 기동하고, 초기화 완료 시점의 로그 상태(baseline)를 저장한다.
  2. Main 시뮬레이터에 `{"op":"fault","action":"timeout","count":100000}` 명령을 전송하여 장기간 무응답 결함을 유도한다.
  3. 자동 검증 프레임워크(`tc_runner.py`)가 600초 동안 1초 주기로 Gateway PID 생존 여부 및 로그 출력률을 측정하며, 2초 주기로 BACnet ReadProperty(Object 1:5) 프로브(probe)를 송신한다.
  4. 결함 주입 기간 내 모든 송신 프로브가 non-success 응답인지 확인하고, Gateway 프로세스 종료 없이 초당 로그 출력량이 20 라인 이하로 억제되는지 검증한다.
  5. Main 시뮬레이터 무응답 결함을 제거한 후, Gateway에 SIGTERM을 전송하여 3초 이내에 메모리 누수 없이 정상 종료되는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| timeout probe | BACnet/IP | 모든 probe non-success, probe exception 없음 | status/error list 비교 |
| process 생존 | Gateway process | 모든 sample alive | sample list 비교 |
| log rate | Gateway log sampler | `<=20 lines/s` | max rate 비교 |
| 종료 | Gateway process | `<=3s` | shutdown elapsed 비교 |

- 현재 자동 검증 범위: 600초 동안 실제 BACnet probe와 Main timeout을 결합해 생존, log rate, 종료를 검사한다. startup burst는 baseline에서 제외한다.
- 직접 검증하지 않는 항목/검증 공백: 내부 CPU/queue memory, 로그 내용의 모든 의미와 개별 Modbus timeout frame의 payload는 직접 assertion하지 않는다.
- 정리/증적: 1초 sampler JSON, probe result, Modbus timeout trace, final log size, 종료 timestamp.
- BLOCKED 조건: `--long` 없음, Main control 또는 managed 실행 파일 부족.

#### TC-BTL-001 — 선언 BIBB 기본 서비스

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`), 첫 active object (예: Analog Input 1), 승인된 BIBB/PICS mapping.
- 수행단계:
  1. `_native_who_is_who_has_case` 함수를 호출하여 unbounded Who-Is 요청을 브로드캐스트하고, Gateway의 I-Am 응답을 수신한다.
  2. 첫 번째 active object(예: Type 0, Instance 1)를 지정하여 Who-Has 요청을 전송하고 I-Have 응답을 확인한다.
  3. `_native_read_values_case` 함수를 호출하여 최대 8개의 active object에 대해 Property ID 85 (Present_Value)의 ReadProperty 요청을 전송한다.
  4. 응답 패킷의 discovery/read 결과를 선언된 BIBB matrix 내용과 정적으로 대조한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| discovery | BACnet/IP | I-Am/I-Have 카운트(1개 이상) 및 Device Instance 등 주요 필드 일치 확인 | BACnet IP 패킷 검사(assert) |
| object read | BACnet/IP + Main | 8개 Object의 Property ID 85 (Present_Value) ReadProperty 응답값 및 데이터 타입 변환 일치 | BACnet IP 패킷 내 응답값 타입(Real 등) 및 수치 일치 확인 |

- 현재 자동 검증 범위: `_native_who_is_who_has_case`와 `_native_read_values_case` 스크립트를 통한 Who-Is/Who-Has 및 8개 Object의 Present_Value ReadProperty 응답 성공 여부를 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 선언된 모든 BIBB의 WriteProperty/COV/DCC 상세 동작을 이 단일 TC에서 모두 호출하지는 않으며, 타 TC(mutation) 수행 결과를 자동으로 취합하여 전체 BIBB matrix 달성도를 평가하는 기능은 포함하지 않는다.
- 정리/증적: BIBB mapping, discovery/read packet, object value matrix.
- BLOCKED 조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway) 기동 실패, active object가 존재하지 않거나 승인된 PICS/BIBB 문서가 없는 경우.

#### TC-BTL-002 — BTL Device/Network Port 조항

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 Python runtime, `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 경계 조건 프로파일과 함께 실행한다.
  2. 프로파일 검증기 출력(output)에서 Device 및 Network Port 조항의 PASS/FAIL 결과를 확인한다.
  3. 검증기 반환 코드(return code), 'FAILED' 항목 목록, 그리고 'SUCCESS' 또는 'PASSED' 마커를 수집하여 테스트 결과를 기록한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 실행 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 결과 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 실행 후 출력되는 전체 로그를 기반으로 반환 코드, FAILED 없음, SUCCESS/PASSED 마커의 3가지 조건을 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)가 개별 BACnet APDU 패킷, Property 필드, 그리고 에러 코드(예: Error Class 2 PROPERTY, Code 40 WRITE_ACCESS_DENIED)를 직접 검사(assertion)하지 않고 검증 스크립트의 최종 출력에 의존한다.
- 정리/증적: 프로파일 검증기 log/output, 조항 mapping, 별도 packet capture.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BTL-003 — PICS/TPI/JSON 3자 전수 대조 감사

- 실행 명령: 기본 명령 + `--tpi <file> --pics-doc <file> --gateway-config-dir <dir>` (미지정 시 모델 기본 경로 사용).
- 입력/사전조건: 
  1. TPI 전자명세서: 명시 `--tpi` 경로 최우선 사용 (기본값: `model/cdu/[BTL]cdu.tpi`). 자동 모델 추정 배제.
  2. BTL 기능 체크리스트(PICS) 문서: 명시 `--pics-doc` 경로 최우선 사용 (기본값: `model/cdu/[BTL]FUNC_CHECKLIST_CDU.docx`).
  3. Gateway 설정: 명시 `--gateway-config-dir` 또는 `conf/` 내 `gateway.json` 및 `profiles/*.json`만 엄격히 사용.
- 수행단계 (총 16개 단계 Soft Assertion 전수 검증):
  1. [STEP-01] TC 시작 및 대상 파일들(PICS docx, TPI, Gateway 설정 JSON) 로드.
  2. [STEP-02~06] PICS 문서 내 5대 필수 서비스(`ReadProperty`, `WriteProperty`, `Who-Is`, `Who-Has`, `COV`) 선언 확인.
  3. [STEP-07~08] PICS ↔ TPI 전자명세서 식별 정보(제조사: LG Electronics, 모델명) 일치 대조.
  4. [STEP-09~10] TPI ↔ gateway.json 장치 설정 식별 정보(제조사, 모델명) 일치 대조.
  5. [STEP-11] PICS 체크/미체크 ↔ TPI 지원 BIBB 19종 전수 일치 대조 (선언 6종: DS-RP-B, DS-WP-B, DS-COV-B, DM-DDB-B, DM-DOB-B, DM-DCC-B 일치 및 미지원 13종 배제).
  6. [STEP-12] PICS 체크(지원) 표준 객체 타입 ↔ TPI 전수 일치 대조 (Device, Network Port, Analog Input, Analog Output, Binary Input, Binary Value 6종 일치).
  7. [STEP-13] PICS 미체크(미지원 59종, **Binary Output 포함**) ↔ TPI/JSON 전수 배제 대조 (미지원 객체의 TPI 및 프로파일 JSON 누출 전무 확인).
  8. [STEP-14] PICS COV 구독 지원(4종: AI, AO, BI, BV)/미지원(28종, **BO 포함**) 전수 대조.
  9. [STEP-15] PICS & TPI ↔ 프로파일 JSON 객체 타입 수용성 대조 (JSON 매핑된 객체 타입이 PICS/TPI 승인 범위 내에 완전 수용됨을 검증).
  10. [STEP-16] 최종 전수 감사 결과 판정 (개별 스텝 불일치가 발생하더라도 중단 없이 15단계까지 전수 완주 후 최종 실패 집계 평가).
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 3자 식별 정보 | PICS, TPI, gateway.json | Vendor, Model 완전 일치 | PICS ↔ TPI 및 TPI ↔ gateway.json 제조사/모델명 일치 대조 |
| 필수 서비스 선언 | PICS 텍스트/XML | 5대 서비스 선언 존재 | `ReadProperty`, `WriteProperty`, `Who-Is`, `Who-Has`, `COV` 존재 확인 |
| BIBB 전수 대조 | PICS 체크/미체크 vs TPI | 6종 지원, 13종 미지원 배제 | 선언 6종 일치 및 미선언 BIBB 완전 배제(Disjoint) 판정 |
| 객체 타입 전수 대조 | PICS 체크/미체크 vs TPI | 6종 지원(BV포함), 59종 배제(BO포함) | 지원 6종 전수 일치 및 미지원 59종의 TPI/JSON 누출 0건 판정 |
| COV 구독 객체 대조 | PICS COV 테이블 vs TPI | 4종 지원(BV포함), 28종 미지원(BO포함) | AI, AO, BI, BV 지원 및 BO 배제 상태 일치 판정 |
| 프로파일 JSON 수용성 | 프로파일 JSON vs PICS/TPI | 미지원 타입 0건 | JSON 내 사용 객체 타입의 PICS/TPI 적합성 판정 |

- 현재 자동 검증 범위: PICS 워드 문서(XML 파싱), TPI 전자명세서, Gateway 설정 JSON 간의 정적/구조적 선언 일치성을 16개 스텝으로 전수 대조하며, 불일치 발생 시에도 끝까지 완주하여 전체 합격/불합격 항목을 리포트에 보존한다.
- 직접 검증하지 않는 항목/검증 공백: 실제 라이브 네트워크 상에서의 APDU 패킷 레벨 송수신은 본 정적 감사 TC가 아닌 `TC-BTL-001`, `TC-BTL-004` 등에서 검증한다.
- 정리/증적: `evidence.details`에 기록된 3자 소스 파일 경로, 체크/미체크 BIBB 및 객체 목록, 스텝별 PASS/FAIL 로그(`steps.log`, `steps.csv`).
- BLOCKED 조건: 대상 TPI, PICS 문서 또는 gateway.json/프로파일 JSON 파일을 찾을 수 없는 경우.

#### TC-BTL-004 — EPICS 전체 object/property scan

- 실행 명령: 기본 명령 + `--config-dir <full-config> --tpi <file> --python-bin <file>`.
- 입력/사전조건: managed에서는 valid `--config-dir`가 `full-conf`로 복사되어야 하며, authoritative TPI와 `bacpypes3`를 import할 수 있는 Python이 필요하다.
- 수행단계:
  1. 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`), TPI 문서 파일, 그리고 `bacpypes3`를 포함한 Python 실행 환경 의존성을 검증한다.
  2. 통합 테스트 환경 (vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터(`modbus_rtu_slave.py`), Gateway)을 full-conf 설정으로 순서대로 기동하고 `BACnet IP Gateway started` 로그 출력을 확인(타임아웃 15초)한다.
  3. `btl_epics_verifier.py` 스크립트를 실행하여 TPI에 선언된 전체 BACnet Object 및 Property를 대상으로 전수 스캔 및 조회를 수행한다.
  4. 검증기 프로세스의 반환 코드와 출력 로그 내의 mismatch(불일치) 항목 유무, 그리고 'SUCCESS' 마커를 분석한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| EPICS verifier | `bacnet_profile_tester.py` 프로세스 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| mismatch | verifier output | 로그 내 'FAILED' 라인 미검출 | 정규식 기반 출력 로그 스캔 |
| 완료 | verifier output | SUCCESS/PASSED | 'SUCCESS' 또는 'PASSED' 키워드 존재 확인 |

- 현재 자동 검증 범위: `btl_epics_verifier.py` 스크립트 실행 후 반환 코드 정상(0) 여부, 'FAILED' 포함 에러 라인 부재, 'SUCCESS' 또는 'PASSED' 성공 마커 존재 여부를 확인한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)는 네트워크 상의 개별 BACnet APDU 패킷을 캡처 및 디코딩하여 직접 비교하지 않으며, TPI 선언값과 실제 장비 응답값 간의 구체적 비교 결과는 전적으로 `btl_epics_verifier.py`의 출력 로그에 의존한다.
- 정리/증적: TPI checksum, full-conf 선택 결과, EPICS verifier log, mismatch output, managed startup evidence.
- BLOCKED 조건: valid `--config-dir`, `--tpi`, Python/bacpypes3 또는 managed 실행 파일 부족. TPI가 다른 장치를 설명하는 경우는 BLOCKED가 아니라 FAIL 대상이다.

#### TC-BTL-005 — Max APDU/Object List 경계

- 실행 명령: 기본 명령.
- 입력/사전조건: 통합 테스트 환경 (vport + Main 시뮬레이터 + Gateway)과 Python runtime, `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트.
- 수행단계:
  1. Max APDU Length(예: 1476 bytes) 및 대규모 Object List 경계 조건(Segmentation 유발)을 테스트하기 위한 프로파일 데이터를 준비한다.
  2. `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트를 경계 조건 프로파일과 함께 실행한다.
  3. 스크립트의 프로세스 반환 코드, 출력 로그 내 'FAILED' 에러 항목, 'SUCCESS' 마커를 검사한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 스크립트 실행 결과 | `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| 완료 | `bacnet_profile_tester.py` 실행 결과(output) | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트 실행 결과로 출력되는 종합 성공 마커 및 프로세스 반환 코드를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)는 Max APDU 1476 바이트 초과 여부, Object List의 정확한 index 및 개수, Segmentation 관련 세부 BACnet 응답 패킷을 직접 캡처하여 검증(assertion)하지 않는다.
- 정리/증적: `bacnet_profile_tester.py` 실행 결과(output), 입력 profile, 필요 시 APDU capture.
- BLOCKED 조건: Python runtime 또는 managed 의존성 부족.

#### TC-BTL-006 — service/object bit와 protocol revision

- 실행 명령: 기본 명령 + `--config-dir <full-config> --tpi <file> --python-bin <file>`.
- 입력/사전조건: BTL-004와 동일하게 full-conf, TPI와 `bacpypes3` Python이 필요하다.
- 수행단계:
  1. 통합 테스트 환경 기동을 위한 full profile, Device 및 Network Port 설정, 그리고 TPI/EPICS 문서 입력을 준비한다.
  2. `btl_epics_verifier.py` BTL EPICS 검증 스크립트를 실행한다.
  3. 장비로부터 수신한 Protocol_Revision, Services_Supported, Object_Types_Supported 속성값과 TPI 문서 상의 기대값을 비교하여 불일치(mismatch) 항목이 있는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| EPICS verifier | `bacnet_profile_tester.py` 프로세스 | code `0` | 프로세스 반환 코드가 0인지 확인 |
| mismatch/completion | verifier output | FAILED 없음, SUCCESS/PASSED | 출력 로그에서 해당 문자열 정규식/포함 여부 확인 |

- 현재 자동 검증 범위: Full profile 환경에서 `btl_epics_verifier.py` 스크립트를 실행하고, 스크립트가 반환하는 종합 검증 로그를 분석하여 성공 여부를 판별한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)는 Services/Object 지원 비트 및 Protocol Revision 정보가 담긴 raw BACnet APDU 패킷을 직접 디코딩하지 않으며, PICS 선언, TPI 문서, 실제 장비 응답 간의 3-way 정합성 비교는 `btl_epics_verifier.py`의 출력 결과에 전적으로 의존한다.
- 정리/증적: TPI/EPICS/PICS diff, verifier log, full-conf 선택과 startup evidence.
- BLOCKED 조건: full-conf/TPI/Python/bacpypes3 또는 managed 실행 파일 부족. TPI와 live 장치 불일치는 FAIL 대상이다.

#### TC-ENV-001 — host build와 warning

- 실행 명령: 기본 명령 + `--mode live --allow-build`.
- 입력/사전조건: host build를 실행할 명시적 승인. 프로젝트 규칙상 `makeIMX`는 실행하지 않는다.
- 수행단계:
  1. 테스트 실행 옵션에 `--allow-build` 플래그가 설정되어 있는지 확인한다.
  2. 플래그가 확인된 경우에 한해, 호스트 환경에서 `make test` 명령을 실행하여 빌드 및 단위 테스트를 수행한다.
  3. 빌드 프로세스의 반환 코드(return code)를 검사하고, 컴파일러 출력 로그에서 `warning:` 패턴을 검색하여 발생한 경고 라인을 기록한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| build/test | host process | code `0` | `make test` 명령의 프로세스 반환 코드가 0인지 검사 |
| warning | compiler output | 컴파일러 출력 내 'warning:' 문자열 부재 | 정규식 `\bwarning:`을 이용한 로그 스캔 |

- 현재 자동 검증 범위: `--allow-build` 옵션이 부여된 호스트 빌드의 반환 코드 성공(0) 및 컴파일러 경고(warning) 미발생을 자동 검사한다. 옵션 누락 시에는 빌드를 생략하고 `BLOCKED` 상태로 전환한다.
- 직접 검증하지 않는 항목/검증 공백: 타겟 보드를 위한 크로스 컴파일(cross build), `makeIMX` 명령 실행, 타겟 장치 전용 컴파일러 검증 및 SonarQube 등 외부 도구를 이용한 전체 소스코드 정적 분석은 본 TC의 검증 범위를 벗어난다.
- 정리/증적: build command/output, warning 목록, 승인 옵션. 이 작업에서는 build를 실행하지 않았다.
- BLOCKED 조건: `--allow-build` 없음 또는 host build toolchain 없음.

#### TC-ENV-002 — Linux Gateway/vport smoke

- 실행 명령: 기본 명령.
- 입력/사전조건: 테스트용 설정 파일 (`gateway.json` + `profiles/*.json`)와 Gateway/vport/Main 실행 파일.
- 수행단계:
  1. 통합 테스트 환경 (vport 가상 시리얼 포트 에뮬레이터, Main 시뮬레이터, Gateway 프로세스)을 순서대로 기동하고 프로세스별 정상 동작 로그를 확인한다.
  2. 정상적으로 서비스 중인 Gateway 프로세스에 SIGTERM 시그널을 전송하여 graceful shutdown(정상 종료)을 유도한다.
  3. 연이어 Main 시뮬레이터와 vport 프로세스에도 종료 시그널을 보내 중지시킨다.
  4. PTY `dev` 디렉터리에 link가 남지 않았는지 확인한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| smoke startup | PTY/Unix/Gateway | 통합 테스트 환경 기동 성공 | 각 프로세스의 로그 내 준비 완료(ready) 패턴 검출 |
| cleanup | process/filesystem | PTY link 없음 | `dev` empty 비교 |

- 현재 자동 검증 범위: 통합 테스트 환경 기동 및 정상 종료(SIGTERM) 시퀀스 수행 후, 리눅스 파일시스템 레벨에서 잔여 PTY 링크가 완전히 정리되었는지 자동 검증한다.
- 직접 검증하지 않는 항목/검증 공백: 이 TC에서는 BACnet 트래픽(Who-Is, ReadProperty 등) 송수신 정상 여부, 다양한 리눅스 배포판 환경에서의 호환성(matrix) 및 OS 플랫폼 API 전체 동작 보장은 직접 검증(assertion)하지 않는다.
- 정리/증적: process/link/socket snapshot, startup/shutdown log.
- BLOCKED 조건: managed Gateway/vport/Main 실행 파일 부족.

#### TC-ENV-003 — BACstac 공개 API/문서 감사

- 실행 명령: 기본 명령 + `--mode live`.
- 입력/사전조건: BACstac API 참조 문서(`verification/docs/references/*.md` 또는 BACstac 설명서)를 사용한다.
- 수행단계:
  1. 제품 `src/**/*.[ch]pp`에서 `BACstac...()` 호출 이름을 추출한다.
  2. 확보한 API 문서(`*.md`) 내용과 추출된 API 명칭 목록을 대조하여 문서화되지 않은(undocumented) API 사용 내역을 식별한다.
  3. 상용 BACstac 모듈 소스 코드의 수정 시간(mtime)을 제품 소스 디렉토리와 비교하여 임의 변경 여부를 감사(audit)한다.
- 자동 판정:

| 항목 | 채널/근거 | 기대 | 현재 자동 판정 |
|---|---|---|---|
| 공개 API 문서화 | source text + Markdown docs | 소스코드에서 추출된 API 명칭이 문서 내에 모두 존재 | 추출된 Set과 문서 텍스트 간 문자열 매칭 검사 |
| 문서 입력 | filesystem | 참조 디렉토리 내 `.md` 파일 최소 1개 이상 존재 | 없으면 BLOCKED |
| 상용 module 감사 | file metadata | 수정 시간(mtime) 위반 파일 개수가 0개인지 확인 후 결과 기록 | 모듈 파일과 제품 소스 간 mtime 비교 검사 |

- 현재 자동 검증 범위: 제품 source의 `BACstac` 호출 이름과 API Markdown의 문자열 일치, module mtime 정보를 검사한다.
- 직접 검증하지 않는 항목/검증 공백: 자동 검증 프레임워크 (`tc_runner.py`)는 API 함수 시그니처나 ABI 레벨의 컴파일 정합성, Deprecated(사용 중단 예정) API의 의미론적 분석 및 상용 소스코드 내부 로직의 세부 변경 내역까지는 직접 파싱하거나 비교하지 않는다.
- 정리/증적: 호출 목록, 문서 match/missing 목록, 문서 directory, module metadata 결과.
- BLOCKED 조건: 우선순위 문서와 fallback 문서가 모두 없는 경우. undocumented 호출은 FAIL 대상이다.

### 10.2 카드별 상태 해석 시 주의사항

- 프로파일 검증 카드(`TC-BAC-005/006/016/017/018`, `TC-BIP-003`, `TC-BTL-002/005`)는 현재 자동 검증 프레임워크 (`tc_runner.py`)가 `bacnet_profile_tester.py` BACnet 프로파일 검증 스크립트의 **return code, FAILED line, SUCCESS/PASSED marker** 세 가지를 판정한다. 이를 개별 BACnet APDU의 직접 PASS로 확대하지 않는다.
- EPICS 카드(`TC-BTL-004/006`)는 EPICS verifier subprocess의 output을 판정한다. 자동 검증 프레임워크 (`tc_runner.py`) 외부에서 verifier가 송신한 각 APDU가 별도 capture되지 않으면 packet-level 증거는 공백으로 기록한다.
- host unit-test 카드(`TC-MAP-004/005/006`)와 vport_test 카드(`TC-VPORT-004..009`)는 독립 실행 파일의 결과를 판정한다. Gateway/Main/BACnet 통합 결과가 아니다.
- `TC-BTL-003`, `TC-IFCFG-001`, `TC-ENV-003`은 정적 문서/source 감사다. 실제 네트워크 기능의 PASS와 혼합하지 않는다.

### 10.3 현재 자동 검증 범위와 검증 공백 요약

| 범주 | 현재 직접 판정 | 주요 공백 |
|---|---|---|
| Native BACnet | discovery, PV, priority, COV, DCC와 일부 availability를 APDU/status/value로 판정 | 모든 property/서비스/optional field 전수 검증은 아님 |
| Profile adapter | 검증 스크립트 반환 코드, FAILED 목록, 완료 marker | index별 APDU, invoke/error code, payload 직접 판정 없음 |
| EPICS 검증기 | TPI/EPICS verifier 종료 code, mismatch/완료 marker | subprocess 밖의 raw APDU capture 없음 |
| Modbus in-memory | CRC, FC, exception, memory와 fault model | 실제 serial/vport/Gateway 경로 아님 |
| vport 독립 시험 | vport_test 종료 code와 success marker | 자동 검증 프레임워크 (`tc_runner.py`)가 frame byte/queue를 직접 판정하지 않음 |
| 정적 감사 | source/document 문자열과 file metadata | 실제 실행/ABI/표준 의미의 전체 검증 아님 |

## 10.8 공통 판정·증적 참고

10.1 카드는 현재 Runner의 실행 경로를 기준으로 한다. 아래 규칙은 카드 간 판정을
일관되게 하기 위한 참고다.

1. 결과의 최종 상태는 `results.json`의 `status`와 `TC 판정` visible step을 사용한다.
2. `steps.log`/`steps.csv`는 expected/actual과 channel을 확인하는 1차 증적이다.
3. BACnet APDU를 직접 판정한 카드는 packet trace의 invoke ID, sender, status, parsed value를 함께 확인한다.
4. Unix control socket의 `ok=true`, `count`와 adapter `SUCCESS`는 각각 환경 설정 결과와 adapter 완료 결과일 뿐, 다른 protocol의 ACK가 아니다.
5. 카드의 검증 공백이 남아 있는 상태에서 SRS 전체 요구사항을 `Verified`로 변경하지 않는다.

## 11. 증적과 결과 기록

실행 결과 디렉터리에는 가능한 한 다음 파일을 보관한다.

| 증적 | 용도 |
|---|---|
| `summary.log` | 실행한 TC별 최종 상태와 전체 요약 |
| `steps.log` | 사람이 읽는 visible step, expected/actual, channel |
| `steps.csv` | assertion별 정렬·필터링 |
| `results.json` | TC 상태, duration, details, config/policy/metric 구조화 결과 |
| `junit.xml` | CI 결과 연동 |
| `preflight.json` | TC별 dependency와 BLOCKED 사유 |
| `packet_exchanges.jsonl` | 자동 검증 프레임워크 (`tc_runner.py`)가 기록한 BACnet/Modbus/control packet 및 parsed field |
| `*_gateway.log`, `*_vport.log`, `*_main_simulator.log` | process lifecycle, retry, HOLD, reconnect와 simulator transaction |
| `bacnet_profile_tester.log`, `btl_epics_verifier.log` | adapter/EPICS subprocess 원문 |

최소한 실행 명령, 대상 IP/port, device ID, config 선택, binary/Python/TPI/PICS
checksum, 시작·종료 시각과 결과 상태를 남긴다. 실패 시에는 `TC 예외` message와
해당 expected/actual을 함께 보관한다. TC 정의가 문서에 있다는 사실만으로 PASS로
간주하지 않는다.

## 12. 자동화 실행 예시

Runner는 이 문서 10장 앞의 요약표에서 90개 카탈로그를 구성한다. 카탈로그 수가
90개가 아니거나 SRS ID가 맞지 않으면 실행 전에 중단한다.

```bash
# 빠른 회귀: 54 TC
python3 verification/scripts/run_gateway_tcs.py --suite quick --mode managed

# 장시간 카드를 제외한 통합: 86 TC
python3 verification/scripts/run_gateway_tcs.py --suite full --mode managed

# 문서/빌드/API 감사: 3 TC
python3 verification/scripts/run_gateway_tcs.py --suite audit --mode live

# 전체 카탈로그: 90 TC
python3 verification/scripts/run_gateway_tcs.py --suite all --mode managed

# 특정 TC 또는 prefix
python3 verification/scripts/run_gateway_tcs.py --suite all --case 'TC-MOD-*' --mode managed

# BBMD mode 카드
python3 verification/scripts/run_gateway_tcs.py --suite all --case TC-BIP-002 --mode managed --bbmd-auto
python3 verification/scripts/run_gateway_tcs.py --suite all --case TC-BAC-020 --mode managed --bbmd-auto

# 600초/60초 branch
python3 verification/scripts/run_gateway_tcs.py --suite all --case TC-OBS-003 --mode managed --long
```

- `managed`는 자동 검증 프레임워크 (`tc_runner.py`)가 만든 임시 fixture/PTY/process만 관리한다. `live`는 외부 장비를 기본적으로 변경하지 않으며, mutation TC는 `--allow-live-mutation`이 필요하다.
- `TC-BTL-004/006`은 managed에서 valid `--config-dir`, `--tpi`, `--python-bin`과 `bacpypes3`가 필요하다. `TC-BTL-003`은 `--pics-doc`로 후보를 명시할 수 있다.
- `TC-BIP-002` 및 `TC-BAC-020`은 `--bbmd-endpoint HOST:PORT` 또는 `--bbmd-auto`가 필요하다. `--bbmd-auto`를 사용하면 자동 검증 프레임워크 (`tc_runner.py`)가 embedded `verification/scripts/bbmd_server.py`를 `127.0.0.1:47809`에서 관리한다.
- `TC-ENV-001`은 `--allow-build` 없이는 `BLOCKED`이며 host `make test`만 대상이다. 프로젝트 규칙상 `makeIMX`는 실행하지 않는다.
- 장시간 카드의 `--long`을 생략하면 해당 long branch는 PASS가 아니라 `BLOCKED` 또는 일반 branch 미실행으로 기록된다.
- 성능 기준은 `verification/scripts/performance_policy.json`의 승인된 scenario를 사용한다. PERF-001은 success rate 0.95, throughput 10 rps, P95 100ms, P99 250ms 기준이고, PERF-002/003은 각 scenario limits를 사용한다. `--response-time-ms`를 주면 자동 검증 프레임워크 (`tc_runner.py`)가 해당 지연에 맞춰 일부 기준을 조정한다.
- 종료 code는 전부 PASS/SKIP이면 `0`, FAIL이 있으면 `1`, BLOCKED가 있으면 `2`다. `SKIP`과 `BLOCKED`를 PASS로 계산하지 않는다.

자세한 옵션과 Docker 기반 실행은 [`verification/README.md`](file:///verification/README.md),
환경 이식성 참고는
[`verification/docs/guides/test_environment_portability_plan.md`](file:///verification/docs/guides/test_environment_portability_plan.md)를
참조한다.
