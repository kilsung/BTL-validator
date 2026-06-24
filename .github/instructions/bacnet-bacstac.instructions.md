---
description: >
  BACstac API를 사용한 BACnet 오브젝트/프로퍼티 구현 시 적용되는 규칙.
  BTL 인증 적합성 관점에서 BACstac API 올바른 사용법 포함.
  Use when: editing bacnet_init_object.c, create_xx_object, init_analog_db.c, init_binary_db.c,
  init_multistate_db.c, BACstac API, BTL property setup.
applyTo: "test/srvdefault/**"
---

# BACstac 오브젝트 초기화 규칙

## PropList 구성 패턴
- `BACSTAC_DB_PROPERTY PropList[]` 배열로 프로퍼티 정의
- `InitandAllocPropInst()` 호출 후 값 설정
- `BACstacCreateDBObject()` 로 DB 등록
- 실패 시 `FreePropListBuffers()` 로 정리

## 슬롯 번호
- ACPi: **0-based** `slot`
- ACS5: **1-based** `point_no` (ACS5 `point_no = slot + 1`)
- 이식 시 반드시 offset 보정 확인

## BTL 필수 프로퍼티 설정

### 모든 오브젝트 공통
- `Object_Identifier`: 타입 + 인스턴스 번호 정확히 설정
- `Object_Name`: 고유한 문자열 (중복 불허 — BTL 테스트 검증 항목)
- `Object_Type`: 오브젝트 타입 코드 정확히 일치
- `Status_Flags`: 4비트 BIT STRING 초기화 (모두 FALSE)
- `Event_State`: 초기값 NORMAL
- `Out_Of_Service`: 초기값 FALSE
- `Property_List`: 해당 오브젝트의 프로퍼티 ID 목록 (BTL 필수)

### Commandable 오브젝트 (AO/BO/MO)
- `Priority_Array`: `BACstacInitPriArray()`로 16슬롯 초기화
- `Relinquish_Default`: 우선순위 없을 때 기본값
- WriteProperty 시 priority level 파라미터 처리 필수

### COV 지원 오브젝트
- `COV_Increment`: Analog 타입 필수 (값 변화 임계값)
- Binary 타입은 COV_Increment 없이 Present_Value 변경 시 자동 Notification

### Event Notification 지원 오브젝트
- `Event_Enable`: 비트마스크 (TO_OFFNORMAL, TO_FAULT, TO_NORMAL)
- `Notification_Class`: 연결된 Notification_Class 오브젝트 인스턴스
- `Event_Time_Stamps`: 배열, 이벤트 발생 시각 기록

## 프로퍼티 값 설정 주의점
- `units`: BACnet 표준 Engineering Units 코드 사용
- `high_limit` / `low_limit`: ACS5 원본 값과 정확히 일치 확인
- `cov_increment`: 오브젝트 타입별 ACS5 기본값 대조
- `relinquish_default`: commandable 오브젝트에만 설정
- `state_text`: Multistate 오브젝트에 필수

## SDK 상수 제약
- SDK에 없는 BACnet 표준 상수는 로컬 `#define` 정의
- 예: `#define UNIT_MICROGRAMS_PER_CUBIC_METER 219`
- SDK API 시그니처 변경 금지
