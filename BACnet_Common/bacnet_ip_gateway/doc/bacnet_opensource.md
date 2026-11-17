# bacnet_ip_gateway 오픈소스 사용 현황

## 요약

| 프로젝트 | 구성 요소 | 구분 | 라이선스 | 링크 방식 | 소스 공개 의무 | 고지 의무 |
|---|---|---|---|---|---|---|
| vport | — | 별도 오픈소스 미사용 | — | — | 없음 | 없음 |
| bacnet_ip_gateway | JsonCpp 1.10.0 | 오픈소스 | MIT / Public Domain (선택) | 정적 링크 | 없음 | **있음** |
| bacnet_ip_gateway | BACstac 7.11.3 (Cimetrics) | 상용 SW | Cimetrics 상용 라이선스 | 정적 링크 | 해당 없음 | 계약 조건에 따름 |

## 1. JsonCpp — 오픈소스, 고지 필요

- 위치: `module/jsoncpp/` (헤더 `include/json/`, 사전 빌드 정적 라이브러리 `x86/`, `arm/libjsoncpp.a`)
- 버전: 1.10.0 (`JSONCPP_VERSION_STRING`)
- 출처: <https://github.com/open-source-parsers/jsoncpp>
- 라이선스: Public Domain 또는 MIT License 중 선택 (Copyright (c) 2007-2010 Baptiste Lepilleur and The JsonCpp Authors)
- 용도: `conf/*.json` 설정 파일 파싱 (`config_loader.cpp` 등)
- 의무: 정적 링크로 바이너리에 포함되므로 MIT 조건에 따라 **저작권 표시와 라이선스 전문을 고지문(오픈소스 고지 화면/문서 등)에 포함**해야 합니다. 소스 공개 의무는 없습니다.

## 2. BACstac (Cimetrics) — 상용 SW

- 위치: `module/bacstac-32_7.11.3-main/` (헤더, 사전 빌드 정적 라이브러리 `libbacsrv.a`, `libbacacc.a`, `libosl.a`, `exutils.a`, 설정 오브젝트 `cfg-*.o`, 호스트 툴 `bin/cbnobj`)
- 모든 모듈에 `Copyright (c) Cimetrics Inc. All rights reserved.` 표기 → **오픈소스가 아닌 상용 라이선스 제품**입니다.
- BACstac 라이브러리 내부에서 외부 오픈소스 라이브러리(OpenSSL, zlib 등)를 참조하지 않음을 확인했습니다 (외부 참조 심볼은 libc/pthread 표준 함수만 존재).
  - 참고: 라이브러리 내 `apigpl.c`는 *Get Port ID List* API 모듈로, GPL 라이선스와 무관합니다.
- 다음 파일도 BACstac 유래이며 Cimetrics 계약 범위에 해당합니다.
  - `src/gateway/dummy_device.c`: BACstac 툴(`cbnobj`)로 `dummy_device.tpi`에서 생성된 코드 (Cimetrics 저작권 표기)
  - `src/gateway/makeobj.h`: BACstac 예제 매크로를 게이트웨이용으로 수정한 파일
