# BTL 인증 BACnet 검증 프로젝트 가이드라인

## 프로젝트 개요
- **목적**: BACnet Testing Laboratories (BTL) 인증 획득
- **테스트 도구**: BACnet Testing Framework (BTF)
- **BACnet 스택**: Cimetrics BACstac
- **플랫폼**: aarch64 (webOS), cross-compile SDK
- **주요 소스**: `test/srvdefault/` (BACnet 서비스 구현)
- **빌드 시스템**: CMake 오케스트레이터 + make

## BTL 인증 대상 BIBB
- **DS-RP-B**: ReadProperty (서버)
- **DS-WP-B**: WriteProperty (서버)
- **DS-COV-B**: Change of Value (서버)
- **DM-DDB-B**: Dynamic Device Binding (서버)
- **AE-N-B**: Event Notification (서버)

## 빌드 명령

| 대상 | 명령 |
|------|------|
| 전체 빌드 | `./build.sh` |
| 클린 빌드 | `./build.sh rebuild` |
| 클린만 | `./build.sh clean` |
| 수동 빌드 | `cd build && cmake .. && make -j$(nproc)` |

## SDK 환경
- **SDK 경로**: `/usr/local/nano-sdk-x86_64/`
- **환경 설정**: `source /usr/local/nano-sdk-x86_64/environment-setup-aarch64-webos-linux`

## 코드 스타일
- Linux Kernel Coding Style (탭 8칸, 80컬럼 권장)
- `goto err_cleanup` 패턴으로 에러 처리 통일
- 커밋 메시지, 주석은 한국어
- SDK에 없는 BACnet 표준 상수는 로컬 `#define`으로 정의

## BACstac API 제약
- `InitandAllocPropInst()` — 프로퍼티 버퍼 할당
- `BACstacCreateDBObject()` — 오브젝트 DB 등록
- `BACstacSetContentsElementsNum()` — 배열형 프로퍼티 원소 수 설정
- `BACstacInitPriArray()` — priority array 초기화
- `FreePropListBuffers()` — 프로퍼티 버퍼 해제
- SDK API 시그니처를 변경하지 않는다

## BTL 인증 작업 흐름
1. **BTF 테스트 실행** → 실패 테스트 식별
2. **실패 원인 분석** → BACnet 표준 조항 대조
3. **코드 수정** → BACstac 구현 보정
4. **빌드 검증** → 크로스 컴파일 성공 확인
5. **재테스트** → BTF 통과 확인

## PICS (Protocol Implementation Conformance Statement)
- BTL 인증 신청 시 제출하는 구현 적합성 명세서
- 지원 오브젝트 타입, 프로퍼티, 서비스를 정확히 선언
- PICS 선언과 실제 구현이 정확히 일치해야 인증 통과
