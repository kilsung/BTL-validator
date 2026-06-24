---
name: bacnet-debug
description: >
  BACnet/BACstac 런타임 디버깅 스킬. 오브젝트 생성 실패, 프로퍼티 값 불일치,
  BACstac API 에러, Luna bus 통신 문제를 체계적으로 디버깅.
  Use when: BACstacCreateDBObject 실패, 프로퍼티 값 불일치, BACstac API 에러,
  Luna bus 에러, 오브젝트 생성 실패, BTL 테스트 런타임 오류.
argument-hint: >
  에러 로그, 실패한 오브젝트 타입/슬롯 번호, 또는 재현 조건을 입력하세요.
---

# BACnet 런타임 디버그 스킬

## 목적
BACnet 오브젝트 초기화, BACstac API 호출, Luna bus 통신 관련 런타임 오류를
체계적으로 분석하여 근본 원인을 찾고 수정한다.

## 사용 시점
- BACstacCreateDBObject() 실패
- BTL 테스트 중 런타임 에러 발생
- 프로퍼티 값이 기대값과 불일치
- Luna bus 응답 파싱 에러
- BACnet 클라이언트에서 오브젝트/프로퍼티 읽기 실패

## 분석 절차

### Phase 1: 증상 수집
1. 로그 메시지 확인 (syslog, journalctl, 프로세스 stdout)
2. 실패한 오브젝트 타입과 인스턴스 번호 정리
3. BTL 테스트 실패 시 해당 테스트가 검증하는 서비스 확인

### Phase 2: BACstac API 호출 검증
1. PropList 배열 구성 확인 (프로퍼티 ID, 타입, 크기)
2. InitandAllocPropInst() 반환값 확인
3. BACstacCreateDBObject() 반환값 확인
4. 에러 시 FreePropListBuffers() 호출 여부 확인
5. [BACstac 에러 패턴 참조](./references/bacstac-error-patterns.md)

### Phase 3: 프로퍼티 값 검증
1. BTL 테스트 기대값 vs 실제 설정값 대조
2. BACnet 표준 필수 프로퍼티 누락 확인
3. 접근 권한(R/W/O) 설정 확인
4. 배열 프로퍼티 인덱스 구성 확인

### Phase 4: Luna bus 통신 검증 (필요 시)
1. luna-send 수동 테스트:
   ```bash
   luna-send -n 1 -f luna://com.b2b.dataManager.service/facility/getList '{}'
   ```
2. JSON 파싱 결과 확인

### Phase 5: 수정 및 검증
1. 최소 변경으로 근본 원인 해결
2. `./build.sh` 로 빌드 검증
3. 다른 오브젝트/서비스에 부작용 없는지 확인

## 참조 자료
- [BACstac API 에러 패턴](./references/bacstac-error-patterns.md)
