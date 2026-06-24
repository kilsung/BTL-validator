---
name: btl-test-analysis
description: >
  BTF(BACnet Testing Framework) 테스트 실패 분석 스킬. 테스트 로그에서 실패 원인을 추출하고
  BACnet 표준 조항과 매핑하여 코드 수정 방향을 제시.
  Use when: BTF 테스트 실패, BTL 테스트 로그 분석, BACnet 표준 위반 추적, 테스트 결과 해석.
argument-hint: >
  BTF 테스트 로그 파일 경로, 실패한 테스트 ID, 또는 에러 메시지를 입력하세요.
---

# BTL 테스트 실패 분석 스킬

## 목적
BACnet Testing Framework(BTF) 테스트 실패 시 체계적으로 원인을 분석하여
BACnet 표준 조항과 매핑하고 코드 수정 방향을 제시한다.

## 사용 시점
- BTF 테스트 실행 후 실패 항목 발생
- 테스트 로그에서 FAIL/ERROR 추출 필요
- 실패 원인이 표준 위반인지 구현 버그인지 구분 필요
- PICS 선언과 테스트 기대값 불일치

---

## 분석 절차

### Phase 1: 테스트 결과 수집
1. BTF 테스트 로그에서 FAIL/ERROR 항목 추출
2. 테스트 ID, 테스트 그룹, 실패 메시지 정리
3. 테스트별 BIBB 분류 (DS-RP-B, DS-WP-B, DS-COV-B, DM-DDB-B, AE-N-B)

### Phase 2: 표준 조항 매핑
1. 실패 테스트가 검증하는 BACnet 표준 조항 확인
   → [BTL 테스트 그룹 참조](./references/btl-test-groups.md)
2. 표준 요구사항과 현재 구현 비교
   → [BACnet 표준 조항 참조](./references/bacnet-standard-clauses.md)
3. 표준 위반 vs 구현 버그 vs PICS 불일치 구분

### Phase 3: 코드 탐색
1. 실패한 서비스 핸들러 코드 탐색
   - ReadProperty → `test/srvdefault/` 내 RP 핸들러
   - WriteProperty → WP 핸들러
   - COV → COV 핸들러
   - WhoIs/I-Am → Device Discovery 핸들러
   - Event → Event Notification 핸들러
2. 관련 오브젝트 생성 코드 확인 (`create_xx_object()`)
3. PropList 구성에서 누락/오류 프로퍼티 확인

### Phase 4: 원인 분류 및 수정 방향

| 원인 분류 | 설명 | 수정 방향 |
|-----------|------|----------|
| **필수 프로퍼티 누락** | 표준 필수 프로퍼티가 PropList에 없음 | PropList에 프로퍼티 추가 |
| **프로퍼티 값 오류** | 초기값이 표준과 불일치 | 값 보정 |
| **접근 권한 오류** | R/W 권한이 표준과 불일치 | 접근 제어 로직 수정 |
| **에러 응답 누락** | 표준 에러 코드 미반환 | 에러 핸들링 추가 |
| **서비스 미구현** | PICS에 선언했으나 서비스 핸들러 없음 | 서비스 핸들러 구현 |
| **PICS 불일치** | PICS 선언과 실제 구현 차이 | PICS 수정 또는 구현 추가 |
| **인코딩 오류** | APDU 인코딩/디코딩 오류 | BACstac API 호출 수정 |

### Phase 5: 수정 및 검증
1. 원인별 최소 수정안 작성
2. `test/srvdefault/` 코드 직접 수정
3. `./build.sh` 빌드 검증
4. 수정이 다른 BIBB 테스트에 미치는 영향 확인

---

## 출력 형식

```
## BTF 테스트 실패 분석 보고

### 실패 요약
| 테스트 ID | BIBB | 실패 메시지 | 원인 분류 |
|-----------|------|------------|----------|
| ... | ... | ... | ... |

### 상세 분석

#### [테스트 ID]
- **표준 조항**: Clause X.Y.Z
- **기대 동작**: ...
- **현재 동작**: ...
- **원인**: ...
- **수정 위치**: [파일:라인]
- **수정 방향**: ...

### 수정 우선순위
1. 🔴 (표준 위반 — 즉시 수정)
2. 🟡 (PICS 불일치 — 인증 전 수정)
3. 🟢 (개선 — 선택)
```
