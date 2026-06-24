---
description: >
  BACnet 프로젝트를 빌드하고 결과를 보고하는 프롬프트.
  Use when: 빌드, cmake, make, 빌드 에러 확인.
argument-hint: >
  빌드 대상을 입력하세요 (full, clean, rebuild).
agent: "btl-expert"
tools: ['execute', 'read', 'search']
---

지정된 대상에 대해 빌드를 실행하고 결과를 보고한다.

## 대상별 빌드

### full (기본)
```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/lib.bacstac-cimetrics && ./build.sh
```

### clean
```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/lib.bacstac-cimetrics && ./build.sh clean
```

### rebuild
```bash
cd /home/js.yang/work/NEXT_ACS/bacnet/lib.bacstac-cimetrics && ./build.sh rebuild
```

## 보고 형식
1. 빌드 성공/실패 여부
2. 실패 시: 에러 메시지 첫 번째 발생 지점과 원인 분석
3. 경고 목록 (중요한 것만)
