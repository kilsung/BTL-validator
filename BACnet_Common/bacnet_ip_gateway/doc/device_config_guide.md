# 기기별 JSON 설정 파일 작성 가이드

> **대상**: 현장 엔지니어, 통합 설치 담당자, 시스템 관리자  
> **적용 버전**: `bacnet_ip_gateway` v1.0.0 (Single-Device / BACstac 7.11.3 연동 기준)  
> **참조 사양서**: [`verification/docs/specs/bacnet_ip_gateway_lld.docx`](file:///verification/docs/specs/bacnet_ip_gateway_lld.docx)

---

## 1. 개요

이 게이트웨이는 **Modbus RTU 장치**를 **BACnet/IP 객체**로 변환하여 상위 빌딩자동화시스템(BMS/BAS)에 노출합니다.  
현재 구현은 **단일 장치(Single-Device) 모드**로 동작하며, `conf/profiles/*.json`에 정의된 모든 객체를 **하나의 BACnet Device** 아래에 등록합니다.

설정 파일은 **`conf/` 디렉터리 및 `conf/profiles/` 하위 디렉터리** 안에 다음 두 종류로 구분됩니다.

| 설정 파일 | 위치 | 필수 여부 | 주요 역할 및 설정 내용 |
|:---|:---|:---:|:---|
| `gateway.json` | `conf/` | **필수 (1개)** | 게이트웨이 공통 BACnet Device 객체 속성, Modbus RTU 시리얼 통신 파라미터, 캐시 및 COV 주기 |
| `profiles/*.json` | `conf/profiles/` | **필수 (1개 이상)** | 장치별 Modbus Slave 관제점 ↔ BACnet 객체(AI/AO/BI/BV) **1:1 매핑 정의** |

```text
bacnet_ip_gateway/
├── conf/
│   ├── gateway.json            # 공통 통신 및 장치 설정
│   └── profiles/               # 관제점 프로필 폴더
│       ├── lg_cdu_c.json       # 기기별 관제점 매핑 파일
│       └── fan_motor.json
├── bacnet_ip_gateway           # 게이트웨이 실행 바이너리
├── bip.cfg -> bip.cfg.normal   # BACstac 네트워크 설정 (현재 모드 파일을 가리키는 심볼릭 링크)
├── bip.cfg.normal              # Normal 모드 설정
└── bip.cfg.fd                  # Foreign Device 모드 설정 (BBMD 주소/포트, TTL, 재접속 주기)
```

> `bip.cfg`는 실행 위치가 아니라 **config 폴더의 상위 폴더**에서 로드합니다.
> 예: `./bacnet_ip_gateway ./conf` → `./bip.cfg`, `./bacnet_ip_gateway /etc/gateway/conf` → `/etc/gateway/bip.cfg`

### 1.1 BACnet/IP 모드와 Network Port 실시간 변경

`bip.cfg`는 현재 모드의 설정 파일(`bip.cfg.normal` 또는 `bip.cfg.fd`)을 가리키는 상대경로 심볼릭 링크입니다. 기본값은 `bip.cfg.normal`입니다. `bip.cfg`가 일반 파일이어도 그대로 동작하며, 모드를 바꾸는 쓰기 요청이 오면 링크로 바뀝니다.

Network Port 객체의 아래 속성은 BACnet WriteProperty로 바꿀 수 있습니다. Changes_Pending / ReinitializeDevice(ACTIVATE_CHANGES)를 거치지 않고 바로 반영되며, Changes_Pending은 항상 FALSE입니다.

| 속성 | 쓰기 값 | 동작 |
|:---|:---|:---|
| `BACnet_IP_Mode` | `NORMAL`(0) / `FOREIGN`(1) | `bip.cfg` 링크를 `bip.cfg.normal` / `bip.cfg.fd`로 바꾸고 게이트웨이를 재시작합니다. `BBMD`(2)와 그 밖의 값은 `VALUE_OUT_OF_RANGE`입니다. |
| `FD_BBMD_Address` | IPv4 주소 + UDP 포트 | `bip.cfg.fd`의 `fd.bbmd_ip`, `fd.bbmd_port`를 갱신합니다. FD 모드면 재시작해 새 BBMD로 접속합니다(재접속 중이어도 즉시 새 주소 사용). Normal 모드면 저장만 합니다. 호스트 이름과 `0.0.0.0`, 포트 0은 `VALUE_OUT_OF_RANGE`입니다. |
| `FD_Subscription_Lifetime` | 1 ~ 65535 (초) | `bip.cfg.fd`의 `fd.ttl`을 갱신합니다. FD 모드면 재시작합니다. |

* **재시작:** 게이트웨이는 WriteProperty에 응답한 뒤 같은 실행 인자로 자기 자신을 다시 실행합니다(`execv`). 이 과정에서 Modbus 초기 동기화가 다시 수행되고 COV 구독은 초기화됩니다. BACstac은 실행 중에 B/IP 설정을 바꾸는 API를 제공하지 않기 때문입니다.
* **FD 속성 읽기:** `FD_BBMD_Address`, `FD_Subscription_Lifetime`은 Normal 모드에서도 `bip.cfg.fd`의 설정값으로 응답합니다.
* **Property_List:** Network Port의 Property_List에는 `Command`가 포함되지 않습니다(이 제품은 네트워크 명령을 지원하지 않음).
* **BBMD 접속 상태 로그(FD 모드):**
  * 접속 성공: `[FD] Registered with BBMD <ip>:<port> (TTL=<n>s)`
  * 접속 실패/끊김: `[FD] BBMD <ip>:<port> registration failed or lost. Retrying every <n> ms...`
  * 재접속은 BACstac이 `bip.cfg.fd`의 `fd.reg_timeout`(기본 3000 ms) 주기로 무한 반복합니다(`fd.reg_attempts=0`).
* **연결 확인 주기:** BACstac은 등록에 성공하면 TTL 시점에만 재등록합니다. 그래서 TTL이 5분(300초)을 넘으면 게이트웨이가 5분마다 자기 Network Port의 Command에 `RENEW_FD_REGISTRATION`을 써서 Register-Foreign-Device를 보내고 응답으로 연결을 확인합니다. TTL이 5분 이하이면 BACstac이 TTL 주기로 재등록합니다. 따라서 BBMD가 응답 없이 끊기면 최대 5분(+ `fd.reg_timeout`) 안에 감지됩니다. 이 자기 재등록 명령 외의 Command 쓰기는 `UNKNOWN_PROPERTY`로 거부합니다.
* **Normal 전환 시 BBMD 항목:** Normal 모드로 바꿀 때 BBMD의 FDT 항목은 BACstac 종료 처리에 따라 즉시 삭제되거나, TTL 만료 후 사라집니다.
* **BTL 참고:** 135.1 7.3.2.46.1.1(Configure Network Through Network Port Object Test)은 쓰기 후 Changes_Pending=TRUE와 ACTIVATE_CHANGES 후 적용을 확인하므로, 즉시 반영 방식과 다릅니다.

---

## 2. gateway.json — 공통 시스템 및 통신 설정

`gateway.json`은 게이트웨이 프로세스의 BACnet Device 객체 속성과 하부 Modbus RTU 시리얼 연결 파라미터를 정의합니다.

```json
{
  "bacnet": {
    "device_id": 1200,
    "device_name": "CDU",
    "vendor_name": "LG Electronics",
    "vendor_id": 432,
    "model_name": "CDU600K/CDU1M/CDU2.5M/CDU5M",
    "description": "BACnet/IP implementation in LG CDU Products.",
    "database_revision": 1,
    "network_port_name": "NetworkPort_1"
  },
  "modbus": {
    "serial_device": "/dev/ttyMASTER2",
    "serial_baudrate": 19200,
    "serial_data_bits": 8,
    "serial_parity": "none",
    "serial_stop_bits": 1,
    "rtu_timeout_ms": 500,
    "hardware_echo": false,
    "cache_sec": 0,
    "cov_polling": 0.5
  }
}
```

### 2.1 `bacnet` 섹션

| 필드 | 타입 | 필수 | 기본값 | 유효 범위 / 설명 |
|:---|:---|:---:|:---|:---|
| `device_id` | Unsigned | **YES** | `1200` | 0 ~ 4,194,302 (BACnet 네트워크에 노출되는 고유 Device Instance ID) |
| `device_name` | String | **YES** | — | 최대 100자 (Device Object_Name) |
| `vendor_name` | String | **YES** | — | 최대 100자 (Vendor_Name, e.g. "LG Electronics") |
| `vendor_id` | Unsigned | **YES** | `432` | 0 ~ 65535 (ASHRAE 공인 Vendor ID, LG: 432) |
| `model_name` | String | **YES** | — | 최대 100자 (Model_Name, BTL 인증 모델명) |
| `description` | String | 선택 | `""` | 최대 80자 (Device Description) |
| `database_revision` | Unsigned | 선택 | `1` | Database_Revision (설정 변경 시 상위 시스템에 통지) |
| `apdu_timeout_ms` | Unsigned | 선택 | `6000` | 100 ~ 60,000 (BACnet Device APDU_Timeout ms, Modbus 큐 에이징 만료시간) |
| `apdu_retries` | Unsigned | 선택 | `3` | 0 ~ 10 (BACnet Device Number_Of_APDU_Retries) |
| `network_port_name` | String | 선택 | `"NetworkPort_1"` | Network Port 객체 이름 (최대 100자) |

### 2.2 `modbus` 섹션

| 필드 | 타입 | 필수 | 기본값 | 유효 범위 / 설명 |
|:---|:---|:---:|:---|:---|
| `serial_device` | String | **YES** | `"/dev/ttyMASTER2"` | Modbus RTU 포트 경로 (vport 가상 포트 e.g. `/dev/ttyMASTER2` 또는 물리 시리얼) |
| `serial_baudrate` | Integer | 선택 | `19200` | 통신 속도 (9600, 19200, 38400, 57600, 115200 등) |
| `serial_data_bits` | Integer | 선택 | `8` | 데이터 비트 (7, 8) |
| `serial_parity` | String | 선택 | `"none"` | 패리티 비트 (`"none"`, `"even"`, `"odd"`) |
| `serial_stop_bits` | Integer | 선택 | `1` | 스톱 비트 (1, 2) |
| `rtu_timeout_ms` | Integer | 선택 | `500` | Modbus Slave 응답 대기 한계 (100 ~ 10,000 ms) |
| `hardware_echo` | Boolean | 선택 | `false` | RS-485 하드웨어 루프백(Echo) 필터 사용 여부 |
| `cache_sec` | Integer | 선택 | `0` | 읽기 캐시 유효 시간 (초, `0`=캐시 비활성화 실시간 조회) |
| `cov_polling` | Float | 선택 | `0.5` | 백그라운드 COV 폴링 주기 (0.1 ~ 60.0 초) |

---

## 3. 기기별 관제점 매핑 — 프로필 파일 (`conf/profiles/*.json`)

각 현장 기기 또는 관제점 그룹마다 `conf/profiles/` 폴더에 `.json` 파일을 생성합니다.

```json
{
  "modbus_unit_id": 1,
  "objects": [
    {
      "name": "Supply Water Temp",
      "description": "Supply water temperature sensor",
      "full_address": 30001,
      "data_type": "INT16",
      "scale": 0.1,
      "units": "degrees-celsius",
      "min_value": -20.0,
      "max_value": 80.0
    },
    {
      "name": "Target Flow Rate",
      "full_address": 40005,
      "data_type": "INT16",
      "scale": 1.0,
      "units": "cubic-meters-per-hour",
      "relinquish_default": 25.0,
      "min_value": 0.0,
      "max_value": 100.0
    },
    {
      "name": "Pump Run Command",
      "full_address": 1,
      "modbus_block": "COIL",
      "relinquish_default": 0
    }
  ]
}
```

### 3.1 최상위 필드

| 필드 | 필수 | 타입 | 기본값 | 설명 |
|:---|:---:|:---|:---|:---|
| `device_name` | 선택 | 문자열 | 파일 기본명 | 장치 프로필 이름 (식별용) |
| `device_id` | 선택 | 정수 | `0` | BACnet Device ID (생략 시 `gateway.json` 공통 Device ID 사용) |
| `modbus_unit_id` | **YES** | 정수/문자열 | — | 1 ~ 247 (해당 장치 프로필의 필수 Modbus Slave Unit ID) |
| `objects` | **YES** | 배열 | — | 매핑 대상 관제점 정의 객체 배열 |

---

## 4. `objects` 섹션 — 관제점 상세 매핑 명세

### 4.1 전체 필드 목록

| 필드 | 필수 | 타입 | 기본값 | 설명 |
|:---|:---:|:---|:---|:---|
| `name` | **YES** | 문자열 | `"Object_N"` | BACnet Object_Name (최대 100자) |
| `description` | 선택 | 문자열 | `""` | BACnet Description (최대 80자) |
| `full_address` | 조건부* | 정수 | — | Modbus 표준 5/6자리 주소 (10001~49999 / 100001~499999). `offset_address`와 상호 배타적 |
| `offset_address` | 조건부* | 정수/문자열 | — | 블록 내 0-based 원시 오프셋 주소 (0 ~ 65535, `"0xA000"` 허용). 사용 시 `modbus_block` 필수 |
| `modbus_block` | 조건부* | 문자열 | — | `"COIL"`, `"DISCRETE_INPUT"`, `"INPUT_REGISTER"`, `"HOLDING_REGISTER"` |
| `modbus_unit_id` | 선택 | 정수 | 상위 프로필 값 | 개별 관제점 전용 Unit ID 지정 시 (1 ~ 247, 명시 시 해당 값 우선 적용) |
| `data_type` | 선택 | 문자열 | 자동 추론 | `"BIT"`, `"INT16"`, `"UINT16"`, `"FLOAT32"`, `"BIT_PACKED"` |
| `bit_position` | 선택 | 정수 | `-1` | 0 ~ 15 (Holding Register 비트 패킹 위치, 설정 시 `data_type`은 `BIT_PACKED`로 자동 전환) |
| `scale` | 선택 | 실수 | `1.0` | 아날로그 환산 계수 ($\text{BACnet} = \text{Raw} \times \text{scale} + \text{offset}$). INT16/UINT16 및 FLOAT32 단위 변환에 공통 적용 ($\le 0.0$ 입력 시 1.0 자동 보정) |
| `offset` | 선택 | 실수 | `0.0` | 아날로그 보정 오프셋 |
| `swap` | 선택 | 불리언 | `false` | 32비트 워드 순서 교환 (`false`=ABCD Big-Endian, `true`=CDAB Word Swapped) |
| `read_only` | 선택 | 불리언 | `false` | `true`이면 BACnet WriteProperty 요청을 `WRITE_ACCESS_DENIED`로 강제 차단 |
| `bacnet_type` | 선택 | 문자열 | 자동 추론 | 명시 강제 타입 (`"ANALOG_INPUT"`, `"ANALOG_OUTPUT"`, `"BINARY_INPUT"`, `"BINARY_VALUE"`) |
| `instance` | 선택 | 정수 | `0` (자동 할당) | BACnet Object Instance 번호. 생략 시 전역 고유 번호 자동 배정 |
| `units` | 선택 | 문자열 | `"no-units"` | BACnet 엔지니어링 단위 문자열 (4.4절 참조) |
| `min_value` | 선택 | 실수 | — | 아날로그 유효 최소값 (`Min_Pres_Value`, 범위 미만 쓰기 차단) |
| `max_value` | 선택 | 실수 | — | 아날로그 유효 최대값 (`Max_Pres_Value`, 범위 초과 쓰기 차단) |
| `relinquish_default`| 선택 | 실수 | — | Commandable 객체(AO)의 안전 기본값 (4.5절 참조, BV는 Non-Commandable) |
| `cov_increment` | 선택 | 실수 | `scale` 값 | Change of Value 알림 임계값. 미지정 시 `scale` 값 자동 사용 (FLOAT32 미세 통지 필요 시 명시 설정 권장) |

`*` 관제점 주소 지정 시 **`full_address`** 또는 **`modbus_block + offset_address`** 중 하나를 반드시 지정해야 합니다.

---

### 4.2 주소 체계 및 객체 타입 자동 추론

#### 4.2.1 `full_address` 표준 표기법
* `00001` ~ `09999`: Coil (읽기/쓰기 비트)
* `10001` ~ `19999` / `100001` ~ `199999`: Discrete Input (읽기 전용 비트)
* `30001` ~ `39999` / `300001` ~ `399999`: Input Register (읽기 전용 16비트 레지스터)
* `40001` ~ `49999` / `400001` ~ `499999`: Holding Register (읽기/쓰기 16비트 레지스터)

#### 4.2.2 객체 타입 자동 판별 규칙 (`bacnet_type` 생략 시)
| Modbus 블록 | 조건 | 자동 생성되는 BACnet 객체 |
|:---|:---|:---|
| `DISCRETE_INPUT` | — | **`BINARY_INPUT` (BI)** |
| `INPUT_REGISTER` | — | **`ANALOG_INPUT` (AI)** |
| `COIL` | `read_only=false` (기본) | **`BINARY_VALUE` (BV)** |
| `COIL` | `read_only=true` | **`BINARY_INPUT` (BI)** |
| `HOLDING_REGISTER` | `bit_position >= 0`, `read_only=false` | **`BINARY_VALUE` (BV)** |
| `HOLDING_REGISTER` | `bit_position >= 0`, `read_only=true` | **`BINARY_INPUT` (BI)** |
| `HOLDING_REGISTER` | 비트 아님, `read_only=false` (기본) | **`ANALOG_OUTPUT` (AO)** |
| `HOLDING_REGISTER` | 비트 아님, `read_only=true` | **`ANALOG_INPUT` (AI)** |

---

### 4.3 Min/Max 유효 범위 및 설정 오류 홀드 (Config Hold)

아날로그 관제점(`AI`, `AO`)에 `min_value`와 `max_value`를 선언하면 유효 범위를 엄격하게 보호합니다.

1. **상위 시스템 쓰기 제한**:
   * 상위 BMS가 선언된 `min_value` 미만 또는 `max_value`를 초과하는 값을 WriteProperty로 쓰려고 시도하면 `VALUE_OUT_OF_RANGE` 오류로 즉시 거부됩니다.
2. **치명적 설정 모순 검증 (`min_value > max_value`)**:
   * JSON 파일에 `min_value > max_value`와 같이 모순된 설정이 기입된 경우, 게이트웨이는 **Crash Loop(무한 재시작)를 방지하기 위해 비정상 종료하지 않고 '설정 오류 홀드(Config Hold)' 상태로 진입**합니다.
   * 홀드 상태에서는 외부 통신을 개방하지 않고 **60초 주기로 JSON 파일을 자동 재검증**합니다.
   * 운영자가 JSON 파일을 정상 범위로 수정하여 저장하면, 프로세스를 재시작하지 않아도 60초 후 자동으로 감지하여 정상 기동을 재개합니다.

---

### 4.4 엔지니어링 단위 (Units) 지원 목록

`"units"` 필드를 통해 BACnet 표준 엔지니어링 단위를 지정할 수 있습니다 (대소문자 무시):

| JSON 지정 문자열 | BACnet 내부 상수 (Enum 값) | 단위 설명 |
|:---|:---|:---|
| `"degrees-celsius"` | `UNIT_DEGREES_C` (62) | 섭씨 온도 ($^\circ\text{C}$) |
| `"degrees-fahrenheit"` | `UNIT_DEGREES_F` (64) | 화씨 온도 ($^\circ\text{F}$) |
| `"percent"` | `UNIT_PERCENT` (98) | 백분율 개도율 및 레벨 (%) |
| `"hours"` | `UNIT_HOURS` (60) | 적산 운전 시간 (h) |
| `"seconds"` | `UNIT_SECONDS` (61) | 초 단위 (s) |
| `"minutes"` | `UNIT_MINUTES` (72) | 분 단위 (min) |
| `"amperes"` | `UNIT_AMPERES` (3) | 전류 (A) |
| `"volts"` | `UNIT_VOLTS` (5) | 전압 (V) |
| `"hertz"` | `UNIT_HERTZ` (27) | 주파수 (Hz) |
| `"revolutions-per-minute"` | `UNIT_REVOLUTIONS_PER_MIN` (96) | 회전 속도 (RPM) |
| `"kilopascals"` | `UNIT_KILOPASCALS` (54) | 압력 단위 (kPa) |
| `"bars"` | `UNIT_BARS` (55) | 압력 단위 (bar) |
| `"cubic-meters-per-hour"` | `UNIT_CUBIC_METERS_PER_HOUR` (135) | 유량 단위 ($m^3/h$) |

> [!NOTE]
> * `"units"` 필드를 생략하거나 지원하지 않는 문자열을 기입한 경우, 기본값인 **`UNIT_NO_UNITS` (95)** (단위 없음)으로 안전하게 매핑됩니다.
> * 냉동기 연동 등에서 압력 단위($\text{kg/cm}^2$)와 같이 대응하는 BACnet 표준 단위가 없거나 스케일 오해 소지가 있는 경우, 단위를 의도적으로 생략(`no-units`) 처리하는 것을 권장합니다.

---

### 4.5 Relinquish Default (안전 기본값) 및 기동 동기화 규칙

Commandable 객체(`AO`)의 `relinquish_default`는 우선순위 제어가 모두 해제(Relinquish)되었을 때 적용되는 안전 기본값입니다.
*(참고: `BV` 객체는 Non-Commandable 객체로서 Priority Array 및 Relinquish Default를 지원하지 않으며, 단순 쓰기(Direct Write)로 동작합니다.)*

1. **JSON에 `relinquish_default`가 명시된 경우**:
   * 지정된 값을 `min_value`/`max_value`로 클램핑한 뒤, **게이트웨이 기동 시 Main 장비(Modbus Slave)에 즉시 Write하여 초기화**합니다.
   * `Main 장치 값 = BACnet Relinquish Default = 초기 Present Value`의 3자 일치를 강제합니다.
2. **JSON에 `relinquish_default`가 생략된 경우**:
   * 기동 시 **Main 장비의 현재 관제점 값을 읽어와 Relinquish Default로 채택**합니다.
   * 읽은 값이 `min_value`/`max_value` 범위를 벗어난 경우에만 정상 범위로 클램핑하여 Main에 역기록(Write-back)합니다.

---

### 4.6 Scale 및 COV (Change of Value) 임계값 동기화

게이트웨이는 센서 정밀도(`scale`)와 변화 감지 임계값(`cov_increment`)을 지능적으로 연동합니다.

* **`scale`만 지정 시 (권장)**: `cov_increment`가 `scale` 값과 동일하게 자동 설정됩니다. (예: `scale=0.01`이면 0.01°C 변화 시 즉시 COV 통지)
* **`cov_increment` 명시 시**: 네트워크 트래픽 감소를 위해 변화 통지 주기를 둔감하게 만들고 싶을 때 더 큰 값을 직접 지정할 수 있습니다. (예: 1 kPa 단위 센서이지만 `cov_increment=5.0`으로 지정 시 5 kPa 이상 변할 때만 통지)

---

## 5. 설정 검증 체크리스트

설정 파일을 현장에 적용하기 전에 다음 사항을 확인하십시오:

1. [ ] `conf/gateway.json`의 `device_id`가 현장 BACnet 네트워크 내에서 고유합니까?
2. [ ] `conf/gateway.json`의 `serial_device`가 실제 vport 포트(`/dev/ttyMASTER2` 등)와 일치합니까?
3. [ ] 모든 프로필 JSON 파일의 문법 오류(쉼표 누락, 괄호 불일치 등)가 없습니까?
4. [ ] `min_value`가 선언된 경우 `max_value`보다 작거나 같습니까 (`min_value <= max_value`)?
5. [ ] AO 객체의 `relinquish_default`가 올바르게 지정되었습니까?
6. [ ] 아날로그 관제점의 단위 문자열이 4.4절의 공식 지원 목록(`"degrees-celsius"`, `"cubic-meters-per-hour"` 등)과 일치합니까?
