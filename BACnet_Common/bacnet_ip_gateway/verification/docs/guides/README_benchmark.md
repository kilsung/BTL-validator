# BACnet/IP Gateway 검증 스크립트 테스트 항목 설명서

현재 BACnet 기능 및 성능 검증의 단일 진입점은
`verification/scripts/run_gateway_tcs.py`입니다. 이전의 분리된 단편
테스트 헬퍼들은 Runner-native packet assertion으로 흡수 통합되어
실행 의존성에서 제거되었습니다.

기능 TC는 `tc_cases.py`가 BACnet/IP request/response raw hex와 parsed field를
기록하고, 성능 TC는 동일한 Runner 내부에서 직접 ReadProperty를 송수신합니다.

---

## 1. 기능 검증 모드 (Functional Validation Mode)
`TC-BAC-*` 및 `TC-BTL-001` 실행 시 활성화되며, 게이트웨이에 설정된 각 장치
프로파일(Profile)을 대상으로 프로토콜 기능과 BTL 규격 요소를 검증합니다.

### A. 객체 Present_Value 조회 테스트 (Read Property)
* **대상**: 각 프로파일 내에 정의된 모든 BACnet 객체 (Analog Input, Analog Output, Binary Input, Binary Value)
* **방법**: 
  * BACnet `ReadProperty` 요청 패킷을 생성하여 대상 객체의 `Present_Value` (Property ID: 85) 속성 조회를 요청합니다.
  * 게이트웨이로부터 유효한 `Complex ACK` 응답이 오는지 확인합니다.
  * 반환된 값의 데이터 타입이 사양과 일치하는지 검증합니다. (Analog: `Float32 / Real`, Binary: `Enumerated / Boolean`)
* **목적**: Modbus 레지스터 값을 BACnet 객체 속성으로 올바르게 변환하고 패킷을 형성하여 응답하는지 검증합니다.

### B. 쓰기-검증-복구 테스트 (Write-Verify-Restore)
* **대상**: 쓰기 권한이 있는 Commandable 객체 (Analog Output, Binary Value)
* **과정**:
  1. **값 변경 쓰기 (Write)**: 기존 Present_Value 값과 다른 값(Binary 객체는 반대 값, Analog 객체는 기존 값 + 1.0)을 BACnet `WriteProperty` 요청을 통해 쓰기 우선순위(Priority) 16레벨로 전송합니다.
  2. **쓰기 상태 검증 (Verify)**: `ReadProperty`를 다시 보내 해당 값이 새로 지정한 값으로 변경되었는지 확인합니다.
  3. **원본 값 복구 (Restore)**: 원활한 시뮬레이션 환경 유지를 위해 기존에 읽어두었던 원래 값(Original Value)을 다시 씁니다.
  4. **최종 복구 확인 (Verify Restore)**: `ReadProperty`를 통해 값이 원래 상태로 안전하게 되돌아갔는지 최종 확인합니다.
* **목적**: BACnet 클라이언트의 제어 명령이 게이트웨이를 거쳐 Modbus Slave 기기의 물리 레지스터(Coil, Holding Register)로 정상 전파되는지 확인합니다.

### C. BTL 고급 기능 검증 (BTL Advanced Features)
BACnet BTL 인증 기준에 부합해야 하는 필수 프로토콜 기능들을 시뮬레이션합니다.

#### 1) 우선순위 배열 및 해제 검증 (Priority Array & Relinquish)
* **대상**: 프로파일의 첫 번째 쓰기 가능한 출력 객체
* **단계별 시나리오**:
  1. **Priority 8 제어**: 비교적 높은 우선순위인 8번 채널로 값 `v_high` (예: 50.0 또는 1)를 씁니다.
  2. **동작 확인**: Present_Value를 읽어 `v_high`로 바뀌었는지 확인합니다.
  3. **Priority 16 제어**: 낮은 우선순위인 16번 채널로 값 `v_low` (예: 20.0 또는 0)를 씁니다.
  4. **우선순위 대기 확인**: Present_Value를 읽어 여전히 더 높은 우선순위인 `v_high` (Priority 8)를 유지하고 있는지 확인합니다.
  5. **Priority 8 해제 (Relinquish)**: Priority 8에 `Null` 값을 써 제어권을 풉니다.
  6. **하위 우선순위 적용 확인**: Present_Value를 다시 읽었을 때 대기 중이던 Priority 16의 값(`v_low`)으로 자동 대체되었는지 확인합니다.
  7. **Priority 16 해제 (Relinquish)**: Priority 16에도 `Null` 값을 써 제어권을 풉니다.
  8. **기본값 낙하 확인 (Relinquish Default)**: Present_Value가 JSON 지정값 또는 부팅 시 Main 현재값에서 결정되고 Min/Max 클램핑된 최종 Relinquish_Default로 복구되는지 검증합니다.

---

## 2. 성능 벤치마크 모드 (Performance Benchmark Mode)
`TC-PERF-001..003` 실행 시 활성화되며, 게이트웨이에 ReadProperty/WriteProperty 트래픽을
인가하여 처리량과 반응 속도를 측정합니다. 성능 기준은 `performance_policy.json`에 기본 승인된
표준 기준치와 Main 응답 지연 설정(`expected_response_time_ms`)을 바탕으로 동적 계산되어 자동 판정됩니다.

### A. 멀티스레드 동시 처리 (Concurrency Testing)
* Runner가 선택한 객체를 대상으로 정해진 횟수의 BACnet ReadProperty를 직접
  송수신합니다. standalone helper의 출력 문자열을 판정 기준으로 사용하지 않습니다.

### B. 연속 트래픽 성능 지표 수집
* 요청별 raw packet, invoke ID, service choice, 응답 status와 latency를 기록합니다.
* **기록하는 성능 메트릭**:
  * **전체 소요 시간 (Total Elapsed Time)**: 부하 개시부터 최종 패킷 수신 완료까지의 시간 (초)
  * **성공/실패 횟수 (Success / Errors)**: 정상 수신된 패킷 개수와 타임아웃/포맷 불일치 에러 개수
  * **처리량 (Throughput)**: 초당 처리한 요청 개수 (RPS - Requests/sec)
  * **네트워크 지연 시간 (Latency)**: 요청 전송 직후 응답 수신까지 걸린 시간의 최소(Min), 평균(Avg), 최대(Max) 값 (단위: ms)
  * **성공률 통계**: 객체별 누적 테스트 통과 횟수
  * **실패 세부 통계**: 검증 오류(Header/PDU Type 등), BACnet 예외 코드(Class/Code), 소켓 타임아웃 발생 빈도 집계
* **목적**: 게이트웨이의 처리 한계(Max Throughput) 및 다중 접속 시 지연율(Avg Latency)을 프로파일링하여 운영 안정성을 증명합니다.
