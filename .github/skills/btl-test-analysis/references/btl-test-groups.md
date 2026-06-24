# BTL 테스트 그룹 참조

## DS-RP-B (ReadProperty 서버) 테스트 그룹

### 9.X — ReadProperty 서비스
- **9.1.1**: 단일 프로퍼티 읽기 — 표준 필수 프로퍼티 반환 확인
- **9.1.2**: 배열 프로퍼티 인덱스 0 읽기 — 배열 크기 반환
- **9.1.3**: 배열 프로퍼티 인덱스 N 읽기 — 특정 요소 반환
- **9.1.4**: 존재하지 않는 오브젝트 읽기 → `UNKNOWN_OBJECT` 에러
- **9.1.5**: 존재하지 않는 프로퍼티 읽기 → `UNKNOWN_PROPERTY` 에러
- **9.1.6**: 잘못된 배열 인덱스 → `INVALID_ARRAY_INDEX` 에러
- **9.1.7**: Object_List 배열 읽기 (전체 및 인덱스별)
- **9.1.8**: Property_List 읽기

## DS-WP-B (WriteProperty 서버) 테스트 그룹

### 9.X — WriteProperty 서비스
- **9.2.1**: Writable 프로퍼티 쓰기 성공
- **9.2.2**: Read-Only 프로퍼티 쓰기 → `WRITE_ACCESS_DENIED`
- **9.2.3**: 잘못된 데이터 타입 쓰기 → `INVALID_DATA_TYPE`
- **9.2.4**: 범위 초과 값 쓰기 → `VALUE_OUT_OF_RANGE`
- **9.2.5**: Priority Array 기반 Present_Value 쓰기 (Commandable)
- **9.2.6**: Relinquish_Default 동작 확인
- **9.2.7**: 존재하지 않는 오브젝트 쓰기 → `UNKNOWN_OBJECT`

## DS-COV-B (COV 서버) 테스트 그룹

### 13.X — COV 서비스
- **13.1.1**: SubscribeCOV 요청 처리 (confirmed)
- **13.1.2**: SubscribeCOV 요청 처리 (unconfirmed)
- **13.1.3**: Present_Value 변경 시 COV Notification 발송
- **13.1.4**: COV_Increment 임계치 미달 시 Notification 미발송
- **13.1.5**: Status_Flags 변경 시 COV Notification 발송
- **13.1.6**: 구독 lifetime 만료 확인
- **13.1.7**: 재구독 시 기존 구독 갱신
- **13.1.8**: 구독 취소 (lifetime=0)
- **13.1.9**: Binary 오브젝트 Present_Value 변경 Notification

## DM-DDB-B (Device Discovery 서버) 테스트 그룹

### 9.X — WhoIs/I-Am 서비스
- **9.31.1**: 전역 WhoIs → I-Am 응답
- **9.31.2**: 범위 지정 WhoIs (자기 범위 내) → I-Am 응답
- **9.31.3**: 범위 지정 WhoIs (자기 범위 외) → 무응답
- **9.31.4**: I-Am 내용 검증 (device ID, maxAPDU, segmentation, vendorID)

## AE-N-B (Event Notification 서버) 테스트 그룹

### 13.X — Event Notification
- **13.X.1**: Intrinsic Reporting 오브젝트 설정 확인
- **13.X.2**: OFFNORMAL 상태 전이 → Notification 발송
- **13.X.3**: NORMAL 복귀 → Notification 발송
- **13.X.4**: Event_Enable 비트마스크 제어
- **13.X.5**: Time_Delay 대기 후 전이
- **13.X.6**: Notification_Class 참조 확인
- **13.X.7**: Event_Time_Stamps 업데이트

## 참고
- 실제 테스트 번호는 BTF 버전에 따라 다를 수 있음
- 위 번호는 BTL 테스트 가이드 기준 대표 패턴
- BTF 실행 시 출력되는 정확한 테스트 ID를 기준으로 분석할 것
