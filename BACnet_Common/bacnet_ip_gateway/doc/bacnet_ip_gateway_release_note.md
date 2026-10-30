# BACnet/IP Gateway Release Note

| 항목 | 내용 |
|---|---|
| **문서 ID** | RN-BIG-001 |
| **소프트웨어 버전** | v1.0.0 (Baseline) |
| **릴리즈 일자** | 2026-08-31 |
| **운영 환경** | Linux (ARM Yocto Linux / x86 Native) |
| **프로토콜 규격** | BACnet/IP (ANSI/ASHRAE Standard 135, B-ASC 프로파일), Modbus RTU Master |
| **참조 사양서** | [`verification/docs/specs/bacnet_ip_gateway_lld.docx`](file:///verification/docs/specs/bacnet_ip_gateway_lld.docx), [`verification/docs/specs/bacnet_gateway_test_spec.md`](file:///verification/docs/specs/bacnet_gateway_test_spec.md) |

---

### 📌 핵심 요약

`bacnet_ip_gateway`는 Modbus RTU 장치(LG CDU 제품군 등)의 관제점을 BACnet/IP 네트워크에 단일 BACnet Device(B-ASC 프로파일)로 노출하는 임베디드 통신 데몬입니다.  
설정 기반의 유연한 관제점 매핑, 2단계(HIGH/LOW) 우선순위 시리얼 작업 큐, Relinquish Default 3자 일치 기동 게이트, 설정 오류 시 60초 자동 재검증 홀드(Crash Loop 방지) 및 88개 원자적 자동화 테스트(V&V) 체계를 완벽하게 갖추고 있습니다.

---

## 1. 주요 구성 모듈 및 파일

| 파일 / 경로 | 역할 및 책임 |
|---|---|
| [`src/gateway/bacnet_gateway.h/.cpp`](file:///src/gateway/bacnet_gateway.h) | 핵심 게이트웨이 코디네이터 (`BacnetGateway`). BACstac 서버 생명주기, 객체 DB 구성, ReadProperty / WriteProperty / SubscribeCOV / DCC Hook 처리 및 기동 동기화 게이트 총괄 |
| [`src/gateway/modbus_client.h/.cpp`](file:///src/gateway/modbus_client.h) | 비동기 Modbus RTU 실행기. HIGH/LOW 우선순위 스레드 안전 작업 큐, 시리얼 송수신, 타임아웃 및 PTY 재연결 관리 |
| [`src/gateway/cov_manager.h/.cpp`](file:///src/gateway/cov_manager.h) | BACnet COV 구독 관리자. 백그라운드 LOW 폴링을 통해 관제점 변화를 감지하고 Confirmed/Unconfirmed COV 알림 패킷 송출 |
| [`src/gateway/config_loader.h/.cpp`](file:///src/gateway/config_loader.h) | JSON 설정 파서. `gateway.json` 및 `profiles/*.json` 로드, 주소 분기, 타입 자동 추론, Min/Max 모순 검증, 전역 인스턴스 자동 배정 |
| [`src/gateway/tpi_exporter.h/.cpp`](file:///src/gateway/tpi_exporter.h) | 현재 설정된 관제점 정보를 바탕으로 BTL 호환 EPICS/TPI 명세서(`current_device.tpi`)를 자동 생성하는 도구 |
| [`src/common/serial_port.h/.cpp`](file:///src/common/serial_port.h) | POSIX `termios` 기반 시리얼/PTY I/O 인터페이스 |
| [`src/common/inotify_port_watcher.h`](file:///src/common/inotify_port_watcher.h) | Linux `inotify`를 이용한 가상 PTY 포트 단절 및 재생성 실시간 감지기 |
| [`src/common/relinquish_policy.h`](file:///src/common/relinquish_policy.h) | Relinquish Default 범위 클램핑 및 Commandable 바이너리 0/1 유효성 검증 공통 정책 헬퍼 |
| [`src/common/modbus_crc.h/.cpp`](file:///src/common/modbus_crc.h) | Modbus RTU CRC-16 계산 및 검증 유틸리티 |
| [`src/common/modbus_frame_builder.h`](file:///src/common/modbus_frame_builder.h) | Modbus RTU 요청 프레임 조립 및 응답 프레임 파싱 유틸리티 |
| [`src/gateway/dummy_device.tpi`](file:///src/gateway/dummy_device.tpi) | BACstac 라이브러리 연동용 기본 PICS 템플릿 |
| [`verification/`](file:///verification) | V&V 표준 사양서(SRS/HLD/LLD/Test Spec), 88개 TC 파이썬 자동화 프레임워크, C++ 호스트 단위 테스트 및 Docker 환경을 포함하는 통합 검증 패키지 |
| [`Makefile`](file:///Makefile) | 게이트웨이 바이너리 및 단위 테스트 빌드 시스템 (Host x86 및 ARM 크로스 빌드 지원) |

---

## 2. 핵심 아키텍처 및 주요 기능

### 2.1 설정 기반 관제점 매핑 (Configuration Architecture)

- **공통 설정 (`conf/gateway.json`)**: BACnet Device ID/이름, Vendor 정보(LG: 432), 모델명, 시리얼 포트 경로(`/dev/ttyMASTER2` 등), Baudrate, RTU 타임아웃, 캐시 시간, COV 폴링 주기를 정의합니다.
- **장치 프로필 (`conf/profiles/*.json`)**: 기기별 관제점 매핑 목록(`objects[]`)을 정의하며, Modbus 주소, 데이터 타입, Scale/Offset, 워드 스왑(`swap`), 단위(`units`), Min/Max 범위, Relinquish Default, COV 임계치(`cov_increment`), 강제 읽기 전용(`read_only`) 속성을 지원합니다.
- **표준 주소 체계 지원**: 5자리/6자리 표준 Modicon 표기(`full_address`, 예: `30001`, `400001`) 및 원시 오프셋(`offset_address` + `modbus_block`)을 모두 지원합니다.
- **객체 타입 자동 추론**: `bacnet_type` 생략 시 레지스터 블록 및 비트 설정에 따라 `AI`, `AO`, `BI`, `BV` 객체 타입을 일관되게 자동 추론합니다.
- **전역 인스턴스 자동 할당**: `instance`가 0이거나 생략된 객체는 충돌 없이 전역 고유 인스턴스 번호가 1부터 순차 자동 배정됩니다.

### 2.2 지원 데이터 타입 및 공학 단위

- **데이터 타입**: `BIT`, `INT16`, `UINT16`, `FLOAT32` (ABCD Big-Endian 및 CDAB Word Swapped), `BIT_PACKED` (Holding Register 내 개별 비트 제어).
- **공학 단위 (Units, 총 13종 공식 지원)**:  
  `degrees-celsius`, `degrees-fahrenheit`, `percent`, `hours`, `seconds`, `minutes`, `amperes`, `volts`, `hertz`, `revolutions-per-minute`, `kilopascals`, `bars`, **`cubic-meters-per-hour`** (미지정 시 `no-units`).

### 2.3 치명적 설정 모순 방지 및 60초 자동 재검증 홀드 (Config Hold)

- 프로필 로드 시 `min_value > max_value`와 같은 치명적 설정 모순이 감지되면, `systemd`/Supervisor의 무한 재시작 루프(Crash Loop)를 원천 차단하기 위해 비정상 종료하지 않고 **설정 오류 홀드(Config Hold)** 상태로 진입합니다.
- 홀드 중에는 외부 Modbus/BACnet 통신을 시작하지 않고 **60초 주기로 JSON 설정을 원자적(Atomic)으로 재검증**하며, 현장에서 설정이 정상 수정되면 프로세스 재기동 없이 즉시 정상 서비스로 진입합니다.
- SIGINT/SIGTERM 신호 수신 시 1초 이내에 즉시 안전하게 정상 종료됩니다.

### 2.4 Relinquish Default 3자 일치 기동 게이트 (Startup Gate)

- Commandable 객체(`AO`)의 초기화 시, JSON 지정값 또는 Main 장비의 현재값을 읽어 Min/Max 범위로 클램핑합니다. *(참고: `BV` 객체는 Non-Commandable 객체로서 Priority Command를 지원하지 않으며 단순 쓰기(Direct Write)로 동작합니다.)*
- 클램핑된 최종 안전 기본값은 Main 장비에 즉시 기록되어 **`Main 관제점 = Relinquish Default = 초기 Present Value`의 3자 정합성**을 보장합니다.
- 모든 Commandable 객체의 기본값 동기화가 100% 완료된 후에만 BACnet/IP 포트를 외부에 개방하여 불완전한 초기 상태 노출을 방지합니다.

### 2.5 2단계 우선순위 Modbus 실행 큐 (HIGH / LOW)

- **HIGH (Priority 0)**: BACnet 클라이언트의 실시간 ReadProperty / WriteProperty 요청 (지연에 민감한 Foreground 작업).
- **LOW (Priority 1)**: 백그라운드 COV 변화 감지 폴링 작업.
- 단일 시리얼 스레드가 우선순위 큐를 처리하여 COV 부하가 높더라도 상위 시스템의 제어/조회 요청이 기아(Starvation) 상태에 빠지지 않습니다.

### 2.6 vport PTY 연동 및 무중단 자동 재연결

- 다중 Modbus Master 요청의 충돌을 방지하는 `vport` 허브 데몬과 가상 PTY 포트(`/dev/ttyMASTER<n>`)로 연동합니다.
- Linux `inotify` 감시기를 내장하여 `vport` 재시작 등으로 PTY 파일 디스크립터가 끊어지더라도 자동으로 감지하여 무중단 재연결(Reopen)을 수행합니다.

### 2.7 BACnet / BTL 표준 서비스 완벽 지원

- **BACnet BIBB 지원**: DS-RP-B, DS-WP-B, DS-COV-B, DM-DDB-B, DM-DOB-B, DM-DCC-B.
- **우선순위 제어 (16 slots)**: 16단계 `Priority_Array`, Relinquish(NULL 쓰기) 시 차순위 또는 `Relinquish_Default`로의 안전한 Fallback.
- **보안 및 무결성**: 읽기 전용 관제점 및 AI/BI 객체에 대한 쓰기 시도 시 `WRITE_ACCESS_DENIED` 표준 에러로 완벽 차단.
- **Network Port 객체**: IP 주소, 서브넷 마스크, 게이트웨이, UDP 포트 실시간 노출 및 WriteProperty를 통한 런타임 통신 제어 지원 (Normal/Foreign Device 모드 전환, BBMD IP/Port 및 Subscription Lifetime TTL 동적 변경, bip.cfg 영구 반영).

---

## 3. 빌드 및 테스트 환경

### 3.1 호스트 빌드 (Host x86)

```bash
# 전체 게이트웨이 바이너리 및 단위 테스트 빌드
make clean
make
```

실행:
```bash
./bacnet_ip_gateway ./conf
```

### 3.2 C++ 호스트 단위 테스트 (BACstac 비의존)

```bash
# 단위 테스트 빌드 및 즉시 실행
make test
```

### 3.3 통합 검증 패키지 실행 (`verification/`)

88개 전체 원자적 테스트 케이스를 Docker 기반 포터블 환경 또는 Python 직접 실행으로 구동할 수 있습니다.

```bash
# 1. Docker 기반 대화형 원클릭 테스트 (권장)
./verification/run_test.sh

# 2. 로컬 완전 자동화 전체 테스트 (내장 BBMD 자동 구동)
./verification/run_test.sh --suite full --mode managed --bbmd-auto

# 3. Python 직접 실행
python3 verification/scripts/run_gateway_tcs.py --suite full --mode managed --config-dir ./conf --tpi 'model/cdu/[BTL]cdu.tpi' --bbmd-auto
```

---

## 4. 특이사항 및 제약조건 (Known Limitations)

1. **COV 감시 방식**: 본 게이트웨이는 센서 레벨의 인터럽트가 없는 Modbus RTU 특성상 `cov_polling` 주기(기본 0.5초)에 따른 고속 폴링 방식으로 관제점 변화를 감지하여 알림을 송출합니다.
2. **단일 디바이스 모델 (Single-Device Mode)**: 로드된 모든 장치 프로필의 관제점은 `gateway.json`에 선언된 1개의 BACnet Device 인스턴스 아래에 통합 등록됩니다.
3. **런타임 설정 변경**: 기동 완료 후 서비스 운용 중에 JSON 설정 파일을 수정한 경우에는 게이트웨이 프로세스 재시작이 필요합니다. (기동 전 Min/Max 모순에 의한 `ConfigHold` 상태에서는 60초 주기 무재시작 자동 반영 지원)

