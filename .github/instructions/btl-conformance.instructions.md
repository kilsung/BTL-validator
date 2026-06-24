---
description: >
  BTL 인증 적합성 규칙. BACnet 표준(ASHRAE 135) 필수 요구사항, BIBB별 서비스 적합성,
  오브젝트/프로퍼티 표준 준수 기준.
  Use when: BTL test failure, BACnet conformance, PICS, ReadProperty, WriteProperty, COV,
  WhoIs, I-Am, Event Notification, 표준 위반 수정.
applyTo: "test/srvdefault/**"
---

# BTL 인증 적합성 규칙

## DS-RP-B (ReadProperty 서버)

### 필수 동작
- 모든 오브젝트의 표준 필수 프로퍼티는 ReadProperty로 읽을 수 있어야 함
- Object_List는 배열 인덱스 접근(index 0 = 크기, 1..N = 요소) 지원
- Property_List는 해당 오브젝트가 지원하는 프로퍼티 ID 목록 반환

### 에러 응답
- 존재하지 않는 오브젝트 ID → `ERROR(UNKNOWN_OBJECT)`
- 존재하지 않는 프로퍼티 ID → `ERROR(UNKNOWN_PROPERTY)`
- 배열 프로퍼티에 범위 초과 인덱스 → `ERROR(INVALID_ARRAY_INDEX)`

## DS-WP-B (WriteProperty 서버)

### 필수 동작
- Writable 프로퍼티에 대해 WriteProperty 처리
- Commandable 프로퍼티(AO/BO/MO의 Present_Value)는 Priority Array 기반 Write
- Priority 16(기본)으로 Write 시 Relinquish_Default와 독립적으로 동작

### 에러 응답
- Read-Only 프로퍼티 Write → `ERROR(WRITE_ACCESS_DENIED)`
- 유효하지 않은 데이터 타입 → `ERROR(INVALID_DATA_TYPE)`
- 범위 초과 값 → `ERROR(VALUE_OUT_OF_RANGE)`
- Priority 6(MinimumOnOff) Write 시도 시 적절한 에러 처리

## DS-COV-B (COV 서버)

### SubscribeCOV 처리
- subscriberProcessIdentifier + monitoredObjectIdentifier 조합으로 구독 식별
- 같은 구독자가 재구독하면 기존 구독 갱신 (lifetime, confirmed/unconfirmed)
- lifetime=0 → 구독 취소
- lifetime 미지정 → 무한 구독

### COV Notification 발생 조건
- **Analog 타입**: Present_Value 변화량 ≥ COV_Increment
- **Binary 타입**: Present_Value 변경 시 즉시
- **Status_Flags 변경** 시 모든 타입에서 발생

### Notification 내용
- monitoredObjectIdentifier, subscriberProcessIdentifier
- initiatingDeviceIdentifier, timeRemaining
- listOfValues: Present_Value, Status_Flags (최소)

## DM-DDB-B (Device Discovery 서버)

### WhoIs 응답
- WhoIs(범위 없음) → 무조건 I-Am 응답
- WhoIs(low_range, high_range) → 자기 Device Instance가 범위 내일 때만 응답
- I-Am에 포함: iAmDeviceIdentifier, maxAPDULengthAccepted, segmentationSupported, vendorID

## AE-N-B (Event Notification 서버)

### Intrinsic Reporting
- Event_Enable 비트마스크로 TO_OFFNORMAL, TO_FAULT, TO_NORMAL 제어
- Event_State 전이: NORMAL ↔ OFFNORMAL, NORMAL ↔ HIGH_LIMIT/LOW_LIMIT
- Time_Delay: 상태 전이 전 대기 시간 (초)

### Notification 전송
- Notification_Class 오브젝트의 Recipient_List 참조
- Confirmed/Unconfirmed 선택은 Notification_Class의 Ack_Required 설정 기반
- Event_Time_Stamps 업데이트 필수

## 공통 프로퍼티 규칙

### Status_Flags (4비트 BIT STRING)
- Bit 0: IN_ALARM (Event_State ≠ NORMAL)
- Bit 1: FAULT (Reliability ≠ NO_FAULT_DETECTED)
- Bit 2: OVERRIDDEN (로컬 오버라이드 활성)
- Bit 3: OUT_OF_SERVICE

### Out_Of_Service
- TRUE일 때 Present_Value는 소프트웨어 값 (물리 입력 무시)
- Reliability는 독립적으로 유지

### Event_State
- NORMAL, FAULT, OFFNORMAL, HIGH_LIMIT, LOW_LIMIT
- Intrinsic Reporting 미지원 오브젝트는 항상 NORMAL
