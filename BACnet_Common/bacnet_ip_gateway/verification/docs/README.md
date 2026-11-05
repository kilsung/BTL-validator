# BACnet/IP Gateway 검증 문서 패키지 색인 (Documentation Index)

본 디렉터리는 `bacnet_ip_gateway` 및 `vport` 프로젝트의 V&V(Verification & Validation) 엔지니어링 문서들을 보관하며, 용도에 따라 **공식 심사/제출용 사양서(`specs/`)**와 **실무 환경/운영 가이드(`guides/`)**로 분리하여 관리합니다.

---

## 🏛 1. 공식 V&V 심사 및 제출 문서 (`specs/`)

QE/QA 품질 심사, 고객사 감사, 소프트웨어 릴리즈 승인 시 제출하는 V-모델 표준 산출물입니다.

| 구분 | 문서 파일명 | 문서 ID | 설명 및 역할 |
|---|---|---|---|
| **SRS** | [specs/bacnet_ip_gateway_srs.docx](specs/bacnet_ip_gateway_srs.docx) | `SRS-BIG-001` | **소프트웨어 요구사항 명세서** (ISO/IEC/IEEE 29148 기준 28개 원자 요구사항) |
| **HLD** | [specs/bacnet_ip_gateway_hld.docx](specs/bacnet_ip_gateway_hld.docx) | `HLD-BIG-001` | **상위/기본설계서** (아키텍처, 프로세스 격리, 상태 머신) |
| **LLD** | [specs/bacnet_ip_gateway_lld.docx](specs/bacnet_ip_gateway_lld.docx) | `LLD-BIG-001` | **하위/상세설계서** (11개 C++ 모듈, 2단계 우선순위 큐, 알고리즘) |
| **TC Spec** | [specs/bacnet_gateway_test_spec.md](specs/bacnet_gateway_test_spec.md) | `TCS-BIG-001` | **테스트 케이스 명세서** (총 94개 TC 사전조건, 절차, 기대값) |

---

## 🛠 2. 실무 환경 및 운영 가이드 (`guides/`)

개발자, DevOps 엔지니어, 실무 테스터가 테스트 환경을 구축하고 실행하기 위한 가이드입니다.

| 문서 파일명 | 설명 및 역할 |
|---|---|
| [guides/test_environment_portability_plan.md](guides/test_environment_portability_plan.md) | **테스트 환경 구축 및 포터블 실행 가이드** (Docker 환경, `test_config.json` 3단 설정, CLI 옵션) |
| [guides/README_benchmark.md](guides/README_benchmark.md) | **성능 벤치마크 및 기능 검증 기술 가이드** (Throughput, Latency 측정 방식, BTL 기능 설명) |

