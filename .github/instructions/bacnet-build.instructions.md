---
description: >
  BACnet 프로젝트 빌드 규칙 및 크로스 컴파일 관련 지침.
  Use when: building, cmake, make, cross-compile, SDK, ipk 패키징.
applyTo: ["**/Makefile", "**/CMakeLists.txt", "build.sh", "make_ipk.sh"]
---

# BACnet 빌드 규칙

## 빌드 환경
- **빌드 시스템**: CMake 오케스트레이터 + make
- **크로스 컴파일러**: aarch64-webos-linux (SDK)
- **SDK 경로**: `/usr/local/nano-sdk-x86_64/`

## 빌드 명령

| 대상 | 명령 |
|------|------|
| 전체 빌드 | `./build.sh` |
| 클린 빌드 | `./build.sh rebuild` |
| 클린만 | `./build.sh clean` |
| 수동 빌드 | `cd build && cmake .. && make -j$(nproc)` |
| IPK 패키징 | `./make_ipk.sh <output_dir>` |

## 빌드 순서
1. `arch/linux/BACstac/` — BACstac 코어 라이브러리
2. `arch/linux/BACstac/test/` — 테스트 (bacnet_srv 바이너리 생성)
3. 바이너리 → `out/` 디렉토리 복사

## SDK 환경 설정
```bash
source /usr/local/nano-sdk-x86_64/environment-setup-aarch64-webos-linux
```

## 주의사항
- SDK 환경 설정 없이 빌드하면 호스트 컴파일러로 빌드됨
- `make clean` 후 빌드 시 BACstac 코어부터 재빌드 필요
