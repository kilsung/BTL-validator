# BACstac API 에러 패턴 참조

## BACstacCreateDBObject() 실패

### 반환값 0 이하
```
BACstacCreateDBObject failed: object_type=X, instance=Y
```
- **원인**: PropList 배열 구성 오류, 중복 instance number, 메모리 부족
- **확인**: PropList 프로퍼티 ID 중복, instance number 계산 로직

### 중복 Instance Number
```
Object already exists: type=X, instance=Y
```
- **원인**: 같은 타입+인스턴스 조합으로 이중 생성
- **확인**: 장치 타입별 instance number 계산식, 유닛 인덱스 중복

## InitandAllocPropInst() 실패

### NULL 반환
- **원인**: 프로퍼티 크기 0, 메모리 할당 실패
- **확인**: PropList 배열 내 size 필드, 시스템 메모리 여유

## 프로퍼티 값 불일치 (ACS5 vs ACPi)

### Engineering Units 코드 불일치
- ACS5: 숫자 상수 직접 사용 (예: `62` = degrees_celsius)
- ACPi: 표준 매크로 또는 로컬 `#define` 사용
- **확인**: BACnet 표준 단위 코드 테이블 대조

### 슬롯 번호 Off-by-One
- ACS5: 1-based `point_no`
- ACPi: 0-based `slot`
- **확인**: `point_no = slot + 1` 보정 여부

### Multistate 오브젝트 state_text 누락
```
Property STATE_TEXT not found for MSI/MSO object
```
- **원인**: PropList에 PROP_STATE_TEXT 미포함
- **확인**: ACS5 원본에서 state_text 개수 및 문자열 확인

## Luna Bus 통신 에러

### LSCall 실패
```
LSCall failed: service not available
```
- **원인**: dataManager 서비스 미기동, 서비스명 오타
- **확인**: `luna-send -n 1 -f luna://com.b2b.dataManager.service/...` 수동 테스트

### cniUpdated 구독 미수신
- **원인**: subscribe 호출 누락, 콜백 미등록, LSHandle 유효하지 않음
- **확인**: `subscribe_cni_updated()` 호출 여부, LSHandle 초기화 시점

### JSON 파싱 에러
- **원인**: 예상과 다른 응답 구조, 필드명 오타
- **확인**: luna-send 수동 호출로 실제 응답 구조 확인 후 파싱 코드 대조
