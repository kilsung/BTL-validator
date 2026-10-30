#pragma once

/**
 * @file modbus_client.h
 * @brief Modbus RTU 통신 전담 클래스 (단일 전담 시리얼 I/O 스레드)
 *
 * 책임:
 *  - 단일 전담 시리얼 Worker 스레드(executorThread)를 통한 RS-485 독점 제어
 *  - 통합 우선순위 큐(Job Queue): foreground (HIGH) 최우선 선처리, background (LOW) 후처리
 *  - 큐 크기 제한 (최대 32개) 및 2초 만료 타임아웃(Aging) 처리
 *  - Modbus RTU Read/Write 직렬화 / 역직렬화 및 캐시 관리 (cache_sec)
 *  - TX Echo 필터 (RS-485 하드웨어 루프백)
 *
 * 의존성: types.h, ISerialPort, ModbusFrameBuilder
 */

#include "types.h"
#include "../common/iserial_port.h"
#include "../common/inotify_port_watcher.h"
#include <map>
#include <mutex>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <thread>

namespace gateway {

/**
 * @brief 큐에 제출된 Modbus 읽기 요청의 완료 상태.
 *
 * 호출자가 waitFor()에서 timeout 되어도 Worker가 요청을 완료할 때까지
 * Handle은 유효합니다. 이를 통해 background 요청의 중복 제출을 방지할 수
 * 있습니다.
 */
class ModbusReadHandle {
public:
    /// 요청 완료를 기다립니다. timeout이면 false, 완료되면 true를 반환합니다.
    bool waitFor(uint32_t timeout_ms);

    /// Worker가 요청을 완료했는지 확인합니다.
    bool isComplete() const;

    /// 완료된 결과를 회수합니다. 성공 시 true와 읽은 값을 반환합니다.
    bool getResult(double& out_value) const;

private:
    friend class ModbusClient;

    void complete(bool success, double value);

    mutable std::mutex       mutex_;
    std::condition_variable  condition_;
    bool                     complete_ = false;
    bool                     success_  = false;
    double                   value_    = 0.0;
};

/**
 * @brief Modbus RTU 통신 전담 클래스.
 */
class ModbusClient {
public:
    static constexpr size_t   kMaxJobQueueSize = 32;   ///< 최대 대기 Job 큐 크기
    static constexpr uint32_t kJobTimeoutMs    = 2000; ///< 큐 대기 및 I/O 타임아웃 (2초)

    /**
     * @brief 생성자.
     * @param port               시리얼 포트 구현체 (소유권 이전)
     * @param cfg                Modbus RTU 설정
     * @param cache_sec          캐시 유효 시간 (초, 0=비활성)
     * @param enable_log         진단 로그 활성화 여부
     * @param request_timeout_ms 요청 만료 타임아웃 (ms, 큐 대기 만료시간)
     */
    ModbusClient(std::unique_ptr<modbus::ISerialPort> port,
                 const ModbusConfig&                  cfg,
                 int                                  cache_sec,
                 bool                                 enable_log,
                 uint32_t                             request_timeout_ms = 6000U);

    ~ModbusClient();

    uint32_t getJobTimeoutMs() const { return job_timeout_ms_; }

    // 복사/이동 금지
    ModbusClient(const ModbusClient&)            = delete;
    ModbusClient& operator=(const ModbusClient&) = delete;
    ModbusClient(ModbusClient&&)                 = delete;
    ModbusClient& operator=(ModbusClient&&)      = delete;

    /**
     * @brief 시리얼 포트 열기 및 단일 전담 Executor 스레드 시작.
     * @return true: 성공 (또는 serial_device="none" 이면 시리얼 포트 없이 시작)
     */
    bool start();

    /**
     * @brief Executor 스레드 종료 및 시리얼 포트 닫기.
     */
    void stop();

    /**
     * @brief 포트가 현재 정상 연결되어 있는지 여부.
     */
    bool isConnected() const { return is_connected_.load(); }

    using OnConnectCallback = std::function<void()>;
    void setOnConnectCallback(OnConnectCallback cb) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        on_connect_cb_ = std::move(cb);
    }

    /**
     * @brief 포트 열기/재연결 시도 (스레드 안전).
     */
    bool tryOpenPort();

    /**
     * @brief 비동기 Modbus Write 요청 큐 등록 (완료 시 콜백 호출).
     * @return true: 큐 등록 성공, false: 큐 Full (32개 초과)
     */
    bool enqueueWrite(const ObjectMapping&      mapping,
                      double                    write_value,
                      ModbusCompletionCallback  on_complete,
                      ModbusJobPriority         priority = ModbusJobPriority::HIGH);

    /**
     * @brief Modbus RTU 값 읽기 (HIGH 우선순위, 캐시 유효 시 캐시 우선).
     * @param bypass_cache true 이면 캐시 무시하고 항상 Modbus 폴링
     * @return true: 성공, false: 실패
     */
    bool readValue(const ObjectMapping& mapping,
                   double&              out_value,
                   bool                 bypass_cache = false);

    /**
     * @brief Modbus RTU 원시값 읽기 (min/max 제한을 적용하지 않음).
     *
     * 초기 Relinquish_Default 동기화처럼 장치의 실제 raw 값을 확인한 뒤
     * 보정값을 다시 써야 하는 경로에서만 사용합니다. 일반 BACnet Present_Value
     * 경로는 readValue()를 사용하여 기존의 min/max 제한을 유지합니다.
     */
    bool readRawValue(const ObjectMapping& mapping,
                      double&              out_value,
                      bool                 bypass_cache = false);

    /**
     * @brief 우선순위가 지정된 비동기 읽기 요청을 큐에 등록합니다.
     *
     * 반환된 Handle은 요청이 큐에서 제거되거나 Worker가 I/O를 완료할 때까지
     * 유효합니다. 호출자가 timeout을 만나도 Handle을 폐기하지 않고 보유하면
     * 같은 background 요청의 중복 제출을 막을 수 있습니다.
     *
     * @return 큐 등록 성공 시 Handle, 실행 중이 아니거나 큐가 가득 차면 nullptr
     */
    std::shared_ptr<ModbusReadHandle> submitRead(const ObjectMapping& mapping,
                                                 ModbusJobPriority priority,
                                                 bool              apply_limits = true);

    /**
     * @brief 동기 Modbus RTU 값 쓰기 (초기화 동기화 전용).
     */
    bool writeValue(const ObjectMapping& mapping, double in_value);

    /**
     * @brief 시리얼 포트 객체에 대한 접근
     */
    modbus::ISerialPort* getPort() const { return port_.get(); }

    /// ms 타임스탬프 획득 (로깅용)
    static std::string getMsTimestamp();

    /**
     * @brief Modbus RTU 표준 3.5 Character Time (t3.5) 기반 프레임 간 지연 시간(ms) 계산
     * @param baudrate 통신 속도 (bps)
     * @return 프레임 간 최소 지연 시간 (ms, 최소 2ms 보장)
     */
    static uint32_t calculateInterFrameDelayMs(int baudrate);

private:
    // ── 실제 시리얼 I/O (단일 Executor Thread에서만 호출) ────────────────
    bool serialRead(const ObjectMapping& mapping,
                    double&              out_value,
                    bool                 apply_limits = true);
    bool serialWrite(const ObjectMapping& mapping, double in_value);

    bool readValueInternal(const ObjectMapping& mapping,
                           double&              out_value,
                           bool                 bypass_cache,
                           bool                 apply_limits);

    /// 단일 전담 시리얼 Worker 스레드 본체
    void executorThread();

    /// TX echo 필터 (RS-485 하드웨어 루프백)
    int filterTxEcho(uint8_t*       rx_buf,
                     int            rx_len,
                     const uint8_t* tx_buf,
                     size_t         tx_len);

    /// ObjectMapping.raw_offset_addr 를 uint16_t 로 안전하게 변환
    static uint16_t toRawAddr(const ObjectMapping& m);

    // ── 멤버 변수 ────────────────────────────────────────────────────────────
    std::unique_ptr<modbus::ISerialPort> port_;
    ModbusConfig                         cfg_;
    int                                  cache_sec_;
    bool                                 enable_log_;
    uint32_t                             inter_frame_delay_ms_ = 2U;

    // inotify 기반 포트 모니터 및 연결 상태
    modbus::InotifyPortWatcher port_watcher_;
    std::atomic<bool>          is_connected_{false};

    // 캐시
    struct CacheEntry {
        std::vector<uint8_t>                  rx_frame;
        bool                                  is_initialized = false;
        std::chrono::steady_clock::time_point last_updated;
    };
    std::map<uint32_t, CacheEntry> cache_table_;
    std::mutex                     cache_mutex_;

    // 통합 우선순위 Job Queue
    std::priority_queue<ModbusJob, std::vector<ModbusJob>, ModbusJobComparator> job_queue_;
    std::mutex              job_queue_mutex_;
    std::condition_variable job_queue_cv_;
    std::thread             executor_thread_;
    // 시리얼 포트 물리 락
    std::mutex serial_mutex_;

    // 실행 상태
    std::atomic<bool> running_{false};
    uint32_t          job_timeout_ms_ = 2000U;

    OnConnectCallback on_connect_cb_;
    std::mutex        callback_mutex_;
};

} // namespace gateway
