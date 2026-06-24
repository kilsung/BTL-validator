# BACnet 표준 조항 참조 (ASHRAE 135)

## Clause 12: 오브젝트 타입 정의

### 12.1 Analog Input (Type 0)
- 필수 프로퍼티: Object_Identifier, Object_Name, Object_Type, Present_Value,
  Status_Flags, Event_State, Out_Of_Service, Units
- COV 지원 시: COV_Increment 필수
- Intrinsic Reporting 지원 시: High_Limit, Low_Limit, Deadband, Limit_Enable,
  Event_Enable, Acked_Transitions, Notify_Type, Event_Time_Stamps

### 12.2 Analog Output (Type 1)
- Analog Input 공통 + Priority_Array, Relinquish_Default
- Commandable: Priority Array 기반 Present_Value 결정

### 12.3 Analog Value (Type 2)
- 필수: Object_Identifier, Object_Name, Object_Type, Present_Value,
  Status_Flags, Event_State, Out_Of_Service, Units

### 12.5 Binary Input (Type 3)
- 필수: Object_Identifier, Object_Name, Object_Type, Present_Value,
  Status_Flags, Event_State, Out_Of_Service, Polarity

### 12.6 Binary Output (Type 4)
- Binary Input 공통 + Priority_Array, Relinquish_Default
- Commandable: Priority Array 기반 Present_Value 결정

### 12.7 Binary Value (Type 5)
- 필수: Object_Identifier, Object_Name, Object_Type, Present_Value,
  Status_Flags, Event_State, Out_Of_Service

### 12.12 Device (Type 8)
- 필수: Object_Identifier, Object_Name, Object_Type, System_Status,
  Vendor_Name, Vendor_Identifier, Model_Name, Firmware_Revision,
  Application_Software_Version, Protocol_Version, Protocol_Revision,
  Protocol_Services_Supported, Protocol_Object_Types_Supported,
  Object_List, Max_APDU_Length_Accepted, Segmentation_Supported,
  APDU_Timeout, Number_Of_APDU_Retries, Database_Revision

### 12.19 Multi-state Input (Type 13)
- 필수: Object_Identifier, Object_Name, Object_Type, Present_Value,
  Status_Flags, Event_State, Out_Of_Service, Number_Of_States
- State_Text: 선택이지만 PICS 선언 시 필수

### 12.20 Multi-state Output (Type 14)
- Multi-state Input 공통 + Priority_Array, Relinquish_Default

### 12.28 Notification Class (Type 15)
- 필수: Object_Identifier, Object_Name, Object_Type,
  Notification_Class, Priority, Ack_Required, Recipient_List

---

## Clause 15: 서비스 정의

### 15.5 ReadProperty (서버)
- 요청: objectIdentifier, propertyIdentifier, propertyArrayIndex(선택)
- 정상 응답: objectIdentifier, propertyIdentifier, propertyArrayIndex, propertyValue
- 에러: class=OBJECT, code=UNKNOWN_OBJECT / class=PROPERTY, code=UNKNOWN_PROPERTY

### 15.9 WriteProperty (서버)
- 요청: objectIdentifier, propertyIdentifier, propertyArrayIndex(선택),
  propertyValue, priority(선택)
- 정상 응답: Simple ACK
- 에러: WRITE_ACCESS_DENIED, INVALID_DATA_TYPE, VALUE_OUT_OF_RANGE

### 15.7 SubscribeCOV (서버)
- 요청: subscriberProcessIdentifier, monitoredObjectIdentifier,
  issueConfirmedNotifications(선택), lifetime(선택)
- 정상 응답: Simple ACK
- Notification: UnconfirmedCOVNotification 또는 ConfirmedCOVNotification

### 15.10 WhoIs / I-Am
- WhoIs 요청: deviceInstanceRangeLowLimit(선택), deviceInstanceRangeHighLimit(선택)
- I-Am 응답: iAmDeviceIdentifier, maxAPDULengthAccepted, segmentationSupported, vendorID

### 15.8 ConfirmedEventNotification / UnconfirmedEventNotification
- 매개변수: processIdentifier, initiatingDeviceIdentifier, eventObjectIdentifier,
  timeStamp, notificationClass, priority, eventType, messageText(선택),
  notifyType, ackRequired(선택), fromState, toState, eventValues

---

## Clause 13: COV (Change of Value)

### 13.1 COV Increment 동작
- Analog 타입: |현재값 - 마지막통보값| ≥ COV_Increment 일 때 발생
- Binary 타입: Present_Value 변경 시 즉시 발생
- Status_Flags 변경 시: 모든 타입에서 발생

### 13.2 구독 관리
- subscriberProcessIdentifier + monitoredObjectIdentifier = 구독 식별자
- 동일 구독 재요청: 기존 구독 갱신 (lifetime, confirmed/unconfirmed)
- lifetime=0: 구독 취소
- lifetime 미지정: 영구 구독 (구현에 따라 기본값 적용 가능)

---

## Clause 14: Event Notification

### 14.1 Intrinsic Reporting
- Event_Enable 비트마스크: TO_OFFNORMAL(0), TO_FAULT(1), TO_NORMAL(2)
- 상태 전이 조건 지속 시간 ≥ Time_Delay → 상태 전이 실행
- 전이 발생 → Notification_Class 참조 → Recipient 에게 Notification 전송

### 14.2 Notification Class
- Priority: 3개 값 (TO_OFFNORMAL, TO_FAULT, TO_NORMAL)
- Ack_Required: 비트마스크 (각 전이별 확인 응답 필요 여부)
- Recipient_List: 알림 대상 목록 (device + processIdentifier)
