# BACnet Gateway Verification Package

`run_gateway_tcs.py`는 SRS에 정의된 94개 TC를 한 진입점에서 선택·실행하고
TC별 단계 로그(기대값/수신값/PASS·FAIL), 채널별 packet evidence, CSV, JSON, JUnit 결과를 생성한다.
상세 TC 정의 및 검증 기준은 `verification/docs/specs/bacnet_gateway_test_spec.md`를 따른다.

이 폴더는 Runner-native BACnet packet assertion, BACnet protocol helper, 설정
loader, Main simulator와 전문 검증기(BACnet 프로파일 검증기 `bacnet_profile_tester.py`,
BTL EPICS 전수 검증기 `btl_epics_verifier.py`)를 포함한다.

## 구성

- `docs/specs/`: 공식 심사 제출용 V&V 문서 (SRS, HLD, LLD, Test Spec)
- `docs/guides/`: 환경 구축 및 성능 벤치마크 가이드 (Portability Plan, Benchmark Guide)
- `scripts/`: 통합 Runner, Runner-native BACnet assertion, 객체 프로파일 검증기
  (`bacnet_profile_tester.py`), BTL EPICS 검증기(`btl_epics_verifier.py`), BACnet helper,
  Main simulator, 성능 정책과 Shell wrapper
- `unit/`: C++ host 단위시험과 자체 test framework
- `results/`, `log/`: 실행 시 생성되는 결과와 로그이며 버전 관리에서 제외

Gateway 실행 파일, `conf/`, `bip.cfg`, `model/`의 TPI와 프로젝트 외부의
`vport` 실행 파일은 제품·환경 입력이므로 이 폴더에 복제하지 않는다.

## 실행 의존성

실행 파일과 Python 파일의 역할은 `verification/scripts/dependencies.json`에
기계 판독 가능한 형태로 기록한다.

| 범위 | 필요한 항목 | 비고 |
|---|---|---|
| 공통 | `python3`, POSIX shell, UDP/PTY/Unix-domain socket을 제공하는 Linux | Python 표준 라이브러리만 사용하며 `pyserial`은 필요하지 않다. |
| managed | `../vport/vport`, `bacnet_ip_gateway/bacnet_ip_gateway`, `verification/scripts/gateway_server.sh` | `gateway_server.sh`가 `modbus_rtu_slave.py`를 Main simulator로 실행한다. 실행 파일이 기본 경로에 없으면 Runner가 이번 실행에서 사용할 경로를 묻는다. |
| 선택 TC | `../vport/vport_test` | `TC-VPORT-004..015` 실행 시 필요하다. `--vport-test-bin`으로 경로를 바꿀 수 있다. |
| 선택 TC | `verification/unit/run_tests` | `TC-MAP-004..006` 실행 시 필요하다. `--unit-test-bin`으로 host `make test` 결과물 경로를 바꿀 수 있다. |
| audit/full 선택 | `--config-dir`, 지정한 `--tpi`, `bacpypes3` | managed `TC-BTL-004/006`은 `--config-dir`의 전체 profile을 임시 `full-conf`로 복사해 사용하고, `--python-bin`의 `bacpypes3` import와 authoritative TPI를 함께 검사한다. |
| 선택 TC | BBMD (`--bbmd-auto` 또는 외부 `host:port`) | `TC-BIP-002` 실행 시 `--bbmd-auto`를 지정하면 내장 BBMD 서버가 자동 구동되며, 외부 BBMD 사용 시 `--bbmd-endpoint`로 지정한다. |
| 빌드 전용 | `make`, host `g++` | `--allow-build`와 `TC-ENV-001`에서만 필요하며, `makeIMX`는 사용하지 않는다. |

따라서 일반 managed BACnet 시험에 필요한 외부 프로그램은 `vport`,
`bacnet_ip_gateway`, `gateway_server.sh`이며, 위 선택 TC를 포함할 때만
`vport_test`, `verification/unit/run_tests`, `bacpypes3`가 추가된다.

실행 시작 시 선택된 TC에 필요한 의존성만 사전점검한다. 대화형 터미널에서는
없는 실행 파일·TPI·Python interpreter의 경로를 한 번씩 입력할 수 있으며, 빈
입력은 해당 의존성을 사용하는 TC만 `BLOCKED`로 만든다. `--non-interactive`를
사용하거나 표준 입력이 터미널이 아니면 자동 입력 없이 같은 정책을 적용한다.
입력 원본은 수정하지 않는다. 이번 실행에서 선택한 입력 경로와 full-profile 복사
결과는 현재 실행의 `preflight.json`, `results.json` metadata와 `steps.log`에만
기록한다.

## 실행 예

### 1. Docker 기반 포터블 원클릭 실행 (권장)
`verification/test_config.json`을 기반으로 대화형 또는 CLI 직접 실행을 지원합니다:
```bash
# 대화형 프롬프트 실행 (모드, 스위트, IP, BBMD 선택)
./verification/run_test.sh

# CLI 직접 실행 (Managed PC 로컬 테스트, 내장 BBMD 자동 구동)
./verification/run_test.sh --suite full --mode managed --bbmd-auto

# 원격 실행 Live 테스트
./verification/run_test.sh --suite full --mode live --ip 192.168.200.20
```

### 2. Python 직접 실행
```bash
python3 verification/scripts/run_gateway_tcs.py --suite quick --mode managed
python3 verification/scripts/run_gateway_tcs.py --suite full --mode managed --bbmd-auto
python3 verification/scripts/run_gateway_tcs.py --suite audit --mode live
python3 verification/scripts/run_gateway_tcs.py --suite all --list
python3 verification/scripts/run_gateway_tcs.py --suite all --case TC-CFG-009
python3 verification/scripts/run_gateway_tcs.py --suite quick --mode live \
  --ip 192.0.2.10 --port 47808 --config-dir ./conf --allow-live-mutation

# 전체 제품 profile을 managed BTL scan에 주입 (내장 BBMD 자동 구동)
python3 verification/scripts/run_gateway_tcs.py --suite full --mode managed \
  --config-dir ./conf \
  --tpi 'model/cdu/[BTL]cdu.tpi' \
  --bbmd-auto
```

기본 suite 수는 quick 54개, full 90개, audit 3개, soak 1개이며 all은 94개이다.
네트워크 장애 TC(`TC-NET-*`, full/all 포함)는 root·독립 네트워크 컨테이너에서만 실행되며 `run_test.sh` 가 자동 구성한다.
여러 `--case`는 OR 조건이고 끝의 `*`만 prefix wildcard로 해석한다.

managed 시험을 처음 실행하는 호스트에서 필요한 제품/host 실행 파일은 별도로
빌드한다. Runner는 누락 파일을 자동 빌드하지 않는다.

```bash
make -C ../vport
make -C ../vport vport_test
make test
# 제품 Gateway가 없을 때만 host 제품 빌드를 별도로 수행
make
```

이 빌드는 host `g++`만 사용하며, `makeIMX`는 사용하지 않는다.

## 실행 모드

- managed: 임시 설정, 임시 PTY 링크, vport, Gateway 및 CDU Main simulator를
  Runner가 소유하고 정리한다. `--config-dir`가 없으면 Runner가
  `fixture-conf`에 최소 `gateway.json`과 `profiles/fixture.json`을 생성한다.
  `--config-dir`가 있으면 원본 `gateway.json`과 `profiles/*.json`을 임시
  `full-conf`로 복사한다. 일반 fixture TC는 입력 유무와 관계없이
  `fixture-conf`를 사용하고, 전체 장치와 TPI를 대조하는 `TC-BTL-004/006`만
  `full-conf`를 사용한다. 복사본의 serial device만 임시 PTY 경로로 덮어쓴다.
  vport는 새 `--link-dir`와 `--config` 옵션을 사용하므로 시스템 `/dev`를
  건드리지 않는다. `--ip`를 생략하면 BACnet client와 Gateway가 같은
  호스트에서 통신하도록 `127.0.0.1`을 사용한다.
- live: `--ip`, `--port`로 지정한 실행 중 장치를 시험한다.
  설정 파일(`gateway.json`, `profiles/*.json`, `vport.cfg`)은 Managed/Live 모드에 상관없이 항상 로컬 파일 시스템에서 직접 참조하며, `test_config.json`의 `common` 섹션에 공통으로 기입된다.
  `--vport-config-path`는 `vport.cfg` 파일 직접 지정 또는 해당 파일이 위치한 디렉터리 경로 지정 모두 지원한다.
  로컬에 지정한 설정 파일이 존재하지 않는 경우 명확한 에러 메시지를 출력하고 즉시 `BLOCKED` 처리된다.
  원격 SSH(`--ssh-enable`)는 순수하게 원격 대상 장비의 OS 자원(프로세스 생존, 메모리/FD 누수) 실시간 모니터링 용도로만 사용된다.
  기본값은 BACnet 관찰/ReadProperty만 허용하며 외부 프로세스·설정을 종료하거나
  수정하지 않는다. WriteProperty, DCC, SubscribeCOV 같은 장치 상태 변경 TC는
  명시적으로 `--allow-live-mutation`을 지정해야 하며, 이 옵션은 BACnet packet
  mutation만 허용하고 Unix control socket을 만들거나 Main을 재기동하지 않는다.
- `TC-BIP-002`의 mode sweep은 설정 파일을 세 번 바꿔 Gateway를 재기동해야 하므로
  `--mode managed`가 필요하며, `--bbmd-auto`를 지정하면 내장 BBMD 서버(`bbmd_server.py`, UDP 47809)를
  자동으로 구동하여 테스트를 완결한다(외부 BBMD 지정 시 `--bbmd-endpoint host:port` 사용). live 모드에서는
  프로세스/설정을 바꾸지 않으므로 이 TC는 BLOCKED다.

managed 모드의 Main 대역은 다음 기존 wrapper를 사용한다.

```bash
verification/scripts/gateway_server.sh <temporary-dev-dir>/ttySLAVE 1 19200 \
  --control-socket <temporary-work-dir>/main-control.sock
```

제어 socket은 JSON Lines 형식이며 `set`, `get`, `reset`, `stats`,
`fault`, `clear_faults`를 지원한다. fault action은 `timeout`, `delay`,
`bad_crc`, `wrong_unit`, `exception`, `fragment`, `partial`이다.

## 판정 원칙

- 구현된 assertion이 기대결과를 확인한 경우에만 PASS이다.
- 네이티브 BACnet 패킷 검증 범위(TC-BAC-001/003/004/007/008/009/010/012/013/015,
  TC-BTL-001)는 Runner가 직접 packet을 송수신하고 parsed field를 assertion한다.
  프로파일/EPICS 검증기(bacnet_profile_tester / btl_epics_verifier)는 전용 로그를 보조 증적으로 남긴다.
- 장비, host-built 시험 실행 파일, BACpypes3, TPI, BBMD 같은 선행조건이 없으면
  그 의존성을 사용하는 TC만 BLOCKED이고 독립 TC는 계속 실행한다.
- `TC-AVL-001`, `TC-BIP-002`, `TC-OBS-003`, `TC-PERF-001..003`은 실제 process,
  BACnet/BBMD, log sampler, COV/timeout 부하를 실행한다. packet/metric assertion이
  실패하면 FAIL이며, 의존성 미확보일 때만 BLOCKED다.
- 성능 기준은 `verification/scripts/performance_policy.json`에 표준 고정 기준치가
  기본 승인되어 있으며, `--response-time-ms`(또는 `test_config.json`의 `expected_response_time_ms`)에
  설정된 슬레이브 응답 지연 시간에 따라 Throughput 및 Latency 상한선이 수학적으로 동적 자동 계산된다.
- 필요 시 커스텀 benchmark JSON을 승인하여 새로운 정책을 만들려면
  `python3 verification/scripts/run_gateway_tcs.py --approve-baseline <result.json> \
  --baseline-scenario TC-PERF-002`를 명시적으로 실행할 수 있다.
- `--allow-build`는 `TC-ENV-001`의 host `make test`만 허용한다.
  크로스 컴파일용 `makeIMX`는 호출하지 않는다.
