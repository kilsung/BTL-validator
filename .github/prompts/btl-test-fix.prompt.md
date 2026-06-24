---
description: >
  BTF 테스트 실패를 분석하고 코드를 수정하는 프롬프트.
  Use when: BTF 테스트 실패 수정, BTL 테스트 FAIL 해결, BACnet 표준 위반 수정.
argument-hint: >
  실패한 BTF 테스트 ID, 에러 로그, 또는 BIBB를 입력하세요.
agent: "btl-expert"
tools: ['execute', 'read', 'edit', 'search', 'todo']
---

BTF 테스트 실패를 분석하고 코드를 수정한다.

## 작업 절차

1. **테스트 실패 정보 수집**
   - 실패한 테스트 ID와 에러 메시지 확인
   - 해당 테스트가 검증하는 BIBB 및 BACnet 표준 조항 파악

2. **원인 분석**
   - `test/srvdefault/` 에서 관련 서비스 핸들러 탐색
   - BACnet 표준 요구사항과 현재 구현 비교
   - 누락된 프로퍼티, 잘못된 에러 응답, 인코딩 오류 등 분류

3. **코드 수정**
   - 최소 변경 원칙으로 표준 적합하도록 보정
   - 관련 오브젝트의 PropList, 서비스 핸들러 수정

4. **빌드 검증**
   ```bash
   cd /home/js.yang/work/NEXT_ACS/bacnet/lib.bacstac-cimetrics && ./build.sh
   ```

5. **영향 분석**
   - 수정이 다른 BIBB 테스트에 미치는 부작용 확인
   - PICS 선언과 일치 여부 확인

## 보고 형식
1. **원인**: 테스트 실패 근본 원인 (표준 조항 참조)
2. **수정 내용**: 변경한 파일, 라인, 변경 이유
3. **빌드 결과**: 성공/실패
4. **부작용**: 다른 BIBB에 미치는 영향 유무
