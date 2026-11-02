/**
 * @file modbus_client.cpp
 * @brief Modbus RTU 통신 구현 (단일 전담 시리얼 I/O 스레드, BACnet 비의존적 범용 드라이버)
 */

#include "modbus_client.h"
#include "../common/modbus_crc.h"
#include "../common/modbus_frame_builder.h"
#include "../common/serial_port.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <thread>

using namespace modbus;

namespace gateway {

// ── 상수 ──────────────────────────────────────────────────────────────────────
static constexpr int kMaxBitPosition = 15;

// ---------------------------------------------------------------------------
// ModbusReadHandle
// ---------------------------------------------------------------------------

bool ModbusReadHandle::waitFor(uint32_t timeout_ms)
{
    std::unique_lock<std::mutex> lock(mutex_);
    return condition_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return complete_;
    });
}

bool ModbusReadHandle::isComplete() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return complete_;
}

bool ModbusReadHandle::getResult(double& out_value) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!complete_) {
        return false;
    }
    if (success_) {
        out_value = value_;
    }
    return success_;
}

void ModbusReadHandle::complete(bool success, double value)
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (complete_) {
            return;
        }
        success_  = success;
        value_    = value;
        complete_ = true;
    }
    condition_.notify_all();
}

// ---------------------------------------------------------------------------
// 생성자 / 소멸자
// ---------------------------------------------------------------------------

ModbusClient::ModbusClient(std::unique_ptr<modbus::ISerialPort> port,
                           const ModbusConfig&                  cfg,
                           int                                  cache_sec,
                           bool                                 enable_log,
                           uint32_t                             request_timeout_ms)
    : port_(std::move(port))
    , cfg_(cfg)
    , cache_sec_(cache_sec)
    , enable_log_(enable_log)
    , inter_frame_delay_ms_(calculateInterFrameDelayMs(cfg.serial_baudrate))
{
    uint32_t margin = (cfg_.rtu_timeout_ms > 0) ? static_cast<uint32_t>(cfg_.rtu_timeout_ms) : 500U;
    if (request_timeout_ms > margin) {
        job_timeout_ms_ = std::max(1000U, request_timeout_ms - margin);
    } else {
        job_timeout_ms_ = 1000U;
    }
}

ModbusClient::~ModbusClient()
{
    stop();
}

// ---------------------------------------------------------------------------
// start() / stop()
// ---------------------------------------------------------------------------

bool ModbusClient::tryOpenPort()
{
    if (cfg_.serial_device == "none") {
        is_connected_ = true;
        return true;
    }

    if (!port_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(serial_mutex_);
    if (port_->isOpen()) {
        if (port_->checkHealth()) {
            is_connected_ = true;
            return true;
        }
        port_->close();
    }

    if (port_->open(cfg_.serial_device,
                    cfg_.serial_baudrate,
                    cfg_.serial_parity,
                    cfg_.serial_stop_bits,
                    cfg_.serial_data_bits))
    {
        bool was_connected = is_connected_.exchange(true);
        std::cout << "[ModbusClient] Serial port connected/reconnected successfully: "
                  << cfg_.serial_device << " (" << cfg_.serial_baudrate << " bps)" << std::endl;
        if (!was_connected) {
            OnConnectCallback cb;
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                cb = on_connect_cb_;
            }
            if (cb) {
                std::thread([cb]() {
                    try { cb(); } catch (...) {}
                }).detach();
            }
        }
        return true;
    }

    is_connected_ = false;
    return false;
}

bool ModbusClient::start()
{
    if (cfg_.serial_device != "none") {
        port_watcher_.init(cfg_.serial_device);
        if (!tryOpenPort()) {
            std::cout << "[ModbusClient] Serial port '" << cfg_.serial_device
                      << "' not yet available. Waiting for connection..." << std::endl;
        }
    } else {
        is_connected_ = true;
    }

    running_ = true;
    executor_thread_ = std::thread(&ModbusClient::executorThread, this);
    return true;
}

void ModbusClient::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }

    std::cout << "[ModbusClient] Stopping Modbus executor thread..." << std::endl;

    // 1. 큐에 대기 중인 모든 Job들을 취소하고 callback/read handle에 실패 통지
    {
        std::lock_guard<std::mutex> lock(job_queue_mutex_);
        while (!job_queue_.empty()) {
            ModbusJob job = std::move(const_cast<ModbusJob&>(job_queue_.top()));
            job_queue_.pop();
            if (job.job_type == ModbusJobType::WRITE_ASYNC && job.callback) {
                try {
                    job.callback(false);
                } catch (...) {}
            } else if (job.read_handle) {
                job.read_handle->complete(false, 0.0);
            }
        }
    }

    // 2. watcher 닫기 및 executorThread 깨우기
    port_watcher_.closeWatcher();
    job_queue_cv_.notify_all();

    // 3. 시리얼 포트 닫기
    if (port_) {
        std::lock_guard<std::mutex> lock(serial_mutex_);
        port_->close();
    }

    // 4. 스레드 종료 대기
    if (executor_thread_.joinable()) {
        executor_thread_.join();
    }
    std::cout << "[ModbusClient] Stopped." << std::endl;
}

// ---------------------------------------------------------------------------
// enqueueWrite() — 비동기 Modbus 쓰기 요청 큐 등록 (완료 시 콜백 호출)
// ---------------------------------------------------------------------------

bool ModbusClient::enqueueWrite(const ObjectMapping&      mapping,
                                double                    write_value,
                                ModbusCompletionCallback  on_complete,
                                ModbusJobPriority         priority)
{
    std::lock_guard<std::mutex> lock(job_queue_mutex_);
    if (!running_) {
        return false;
    }
    if (job_queue_.size() >= kMaxJobQueueSize) {
        // 이상 상황 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
        std::cerr << "[" << getMsTimestamp() << "][Modbus Queue Full] Write job rejected for '"
                  << mapping.name << "' (Queue=" << job_queue_.size()
                  << " >= " << kMaxJobQueueSize << ")" << std::endl;
        return false;
    }

    ModbusJob job;
    job.priority = priority;
    job.job_type = ModbusJobType::WRITE_ASYNC;
    job.mapping = &mapping;
    job.write_value = write_value;
    job.callback = std::move(on_complete);
    job.enqueued_time = std::chrono::steady_clock::now();

    job_queue_.push(std::move(job));
    job_queue_cv_.notify_one();
    return true;
}

// ---------------------------------------------------------------------------
// submitRead() — 우선순위가 지정된 Modbus 읽기 요청 큐 등록
// ---------------------------------------------------------------------------

std::shared_ptr<ModbusReadHandle> ModbusClient::submitRead(
    const ObjectMapping& mapping,
    ModbusJobPriority    priority,
    bool                 apply_limits)
{
    auto read_handle = std::make_shared<ModbusReadHandle>();

    ModbusJob job;
    job.priority    = priority;
    job.job_type    = ModbusJobType::READ_SYNC;
    job.mapping     = &mapping;
    job.apply_limits = apply_limits;
    job.read_handle = read_handle;
    job.enqueued_time = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(job_queue_mutex_);
        if (!running_) {
            return nullptr;
        }
        if (job_queue_.size() >= kMaxJobQueueSize) {
            // 이상 상황 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
            std::cerr << "[" << getMsTimestamp() << "][Modbus Queue Full] Read job rejected for '"
                      << mapping.name << "' (Queue=" << job_queue_.size()
                      << " >= " << kMaxJobQueueSize << ")" << std::endl;
            return nullptr;
        }
        job_queue_.push(std::move(job));
    }
    job_queue_cv_.notify_one();
    return read_handle;
}

// ---------------------------------------------------------------------------
// readValue() — HIGH 우선순위 값 읽기 (캐시 유효 시 캐시 우선)
// ---------------------------------------------------------------------------

bool ModbusClient::readValue(const ObjectMapping& mapping,
                             double&              out_value,
                             bool                 bypass_cache)
{
    return readValueInternal(mapping, out_value, bypass_cache, true);
}

bool ModbusClient::readRawValue(const ObjectMapping& mapping,
                                double&              out_value,
                                bool                 bypass_cache)
{
    return readValueInternal(mapping, out_value, bypass_cache, false);
}

bool ModbusClient::readValueInternal(const ObjectMapping& mapping,
                                     double&              out_value,
                                     bool                 bypass_cache,
                                     bool                 apply_limits)
{
    if (!running_) {
        return false;
    }

    if (!bypass_cache && cache_sec_ > 0) {
        bool is_cache_valid = false;
        {
            uint32_t cache_key = (static_cast<uint32_t>(mapping.modbus_unit_id) << 24) |
                                 (static_cast<uint32_t>(mapping.modbus_block)   << 16) |
                                  static_cast<uint32_t>(toRawAddr(mapping));
            std::lock_guard<std::mutex> cache_lock(cache_mutex_);
            auto it = cache_table_.find(cache_key);
            if (it != cache_table_.end() && it->second.is_initialized) {
                auto now     = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->second.last_updated).count();
                if (elapsed < cache_sec_) {
                    is_cache_valid = true;
                }
            }
        }
        if (is_cache_valid) {
            return serialRead(mapping, out_value, apply_limits);
        }
    }

    auto read_handle = submitRead(mapping, ModbusJobPriority::HIGH, apply_limits);
    if (!read_handle || !read_handle->waitFor(job_timeout_ms_)) {
        return false;
    }
    return read_handle->getResult(out_value);
}

// ---------------------------------------------------------------------------
// writeValue() — 동기 시리얼 I/O 쓰기 (초기화 동기화 전용)
// ---------------------------------------------------------------------------

bool ModbusClient::writeValue(const ObjectMapping& mapping, double in_value)
{
    if (!running_) {
        return false;
    }
    return serialWrite(mapping, in_value);
}

// ---------------------------------------------------------------------------
// executorThread() — 단일 전담 시리얼 Worker 스레드
// ---------------------------------------------------------------------------

void ModbusClient::executorThread()
{
    bool was_waiting = false;

    while (running_ || !job_queue_.empty()) {
        // -------------------------------------------------------------------
        // 1. Connection Health & Auto-Reconnection (POLLHUP + inotify + 1s fallback)
        // -------------------------------------------------------------------
        if (cfg_.serial_device != "none") {
            bool healthy = port_ && port_->checkHealth();
            if (!healthy) {
                if (is_connected_ || !was_waiting) {
                    std::cout << "[ModbusClient] Port " << cfg_.serial_device
                              << " disconnected. Waiting for connection..." << std::endl;
                    was_waiting = true;
                }
                is_connected_ = false;

                // 큐에 대기 중인 요청이 있다면 에이징 검사하여 만료된 것은 즉시 실패 통지
                {
                    std::lock_guard<std::mutex> lock(job_queue_mutex_);
                    auto now = std::chrono::steady_clock::now();
                    std::priority_queue<ModbusJob, std::vector<ModbusJob>, ModbusJobComparator> remaining_jobs;
                    while (!job_queue_.empty()) {
                        ModbusJob job = std::move(const_cast<ModbusJob&>(job_queue_.top()));
                        job_queue_.pop();
                        auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - job.enqueued_time).count();
                        if (static_cast<uint32_t>(wait_ms) > job_timeout_ms_) {
                            if (job.job_type == ModbusJobType::WRITE_ASYNC && job.callback) {
                                try { job.callback(false); } catch (...) {}
                            } else if (job.read_handle) {
                                job.read_handle->complete(false, 0.0);
                            }
                        } else {
                            remaining_jobs.push(std::move(job));
                        }
                    }
                    job_queue_ = std::move(remaining_jobs);
                }

                if (!running_) {
                    break;
                }

                // inotify 또는 1초 Fallback 타이머 대기
                port_watcher_.waitForDevice(1000);

                if (!running_) {
                    break;
                }

                tryOpenPort();
                if (!is_connected_) {
                    continue; // Reconnect not yet successful, loop again
                }
                was_waiting = false;
            } else {
                was_waiting = false;
            }
        }

        // -------------------------------------------------------------------
        // 2. Job Queue Pop & Processing
        // -------------------------------------------------------------------
        std::unique_lock<std::mutex> lock(job_queue_mutex_);
        job_queue_cv_.wait_for(lock, std::chrono::milliseconds(500), [this]{
            return !job_queue_.empty() || !running_;
        });

        if (job_queue_.empty()) {
            continue;
        }

        ModbusJob job = std::move(const_cast<ModbusJob&>(job_queue_.top()));
        job_queue_.pop();
        lock.unlock();

        // 2.1 Aging timeout check
        auto now = std::chrono::steady_clock::now();
        auto wait_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - job.enqueued_time).count();
        if (static_cast<uint32_t>(wait_ms) > job_timeout_ms_) {
            // 이상 상황 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
            std::cerr << "[" << getMsTimestamp() << "][Modbus Worker] Job Timed Out in Queue ("
                      << wait_ms << "ms > " << job_timeout_ms_ << "ms) for '"
                      << (job.mapping ? job.mapping->name : "Unknown") << "'" << std::endl;
            if (job.job_type == ModbusJobType::WRITE_ASYNC && job.callback) {
                try {
                    job.callback(false);
                } catch (...) {}
            } else if (job.read_handle) {
                job.read_handle->complete(false, 0.0);
            }
            continue;
        }

        // 2.2 Execute Serial I/O
        if (job.job_type == ModbusJobType::WRITE_ASYNC) {
            if (enable_log_ && job.mapping != nullptr) {
                std::cout << "[" << getMsTimestamp() << "][Modbus Write Target] Writing Value="
                          << job.write_value << " to " << job.mapping->name << std::endl;
            }

            bool ok = (job.mapping != nullptr) && serialWrite(*job.mapping, job.write_value);
            if (!ok) {
                // 이상 상황 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
                std::cerr << "[" << getMsTimestamp() << "][Modbus Error] Write failed for '"
                          << (job.mapping ? job.mapping->name : "Unknown") << "'" << std::endl;
                if (port_ && !port_->isOpen()) {
                    is_connected_ = false;
                }
            }
            if (job.callback) {
                try {
                    job.callback(ok);
                } catch (...) {}
            }
        } else {
            // Read jobs
            double val = 0.0;
            bool ok = (job.mapping != nullptr) &&
                      serialRead(*job.mapping, val, job.apply_limits);
            if (!ok && port_ && !port_->isOpen()) {
                is_connected_ = false;
            }
            if (job.read_handle) {
                job.read_handle->complete(ok, val);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// serialRead()
// ---------------------------------------------------------------------------

bool ModbusClient::serialRead(const ObjectMapping& mapping,
                              double&              out_value,
                              bool                 apply_limits)
{
    if (!port_) { return false; }

    uint8_t  fc       = 0U;
    uint16_t quantity = 1U;

    switch (mapping.modbus_block) {
        case ModbusBlock::COIL:
            fc = kFcReadCoils;
            break;
        case ModbusBlock::DISCRETE_INPUT:
            fc = kFcReadDiscreteInputs;
            break;
        case ModbusBlock::INPUT_REGISTER:
            fc       = kFcReadInputRegs;
            quantity = (mapping.data_type == DataType::FLOAT32) ? 2U : 1U;
            break;
        case ModbusBlock::HOLDING_REGISTER:
            fc       = kFcReadHoldingRegs;
            quantity = (mapping.data_type == DataType::FLOAT32 ||
                        mapping.data_type == DataType::BIT_PACKED) ? 2U : 1U;
            break;
        default:
            return false;
    }

    uint8_t tx_buf[kTxBufMaxLen];
    const size_t tx_len = ModbusFrameBuilder::buildReadRequest(
        tx_buf, sizeof(tx_buf),
        mapping.modbus_unit_id, fc,
        toRawAddr(mapping), quantity);

    if (tx_len == 0U) {
        std::cerr << "[ModbusClient] Frame build error in serialRead" << std::endl;
        return false;
    }

    uint8_t rx_buf[kRxBufMaxLen];
    int     rx_len    = 0;
    bool    cache_hit = false;
    uint32_t cache_key = (static_cast<uint32_t>(mapping.modbus_unit_id) << 24) |
                         (static_cast<uint32_t>(mapping.modbus_block)   << 16) |
                          static_cast<uint32_t>(toRawAddr(mapping));

    if (cache_sec_ > 0) {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        auto it = cache_table_.find(cache_key);
        if (it != cache_table_.end() && it->second.is_initialized) {
            auto now     = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_updated).count();
            if (elapsed < cache_sec_) {
                rx_len = static_cast<int>(it->second.rx_frame.size());
                std::memcpy(rx_buf, it->second.rx_frame.data(), rx_len);
                cache_hit = true;
                if (enable_log_) {
                    std::cout << "[" << getMsTimestamp() << "][Modbus Cache Hit] Key=0x"
                              << std::hex << cache_key << std::dec
                              << " (Unit=" << static_cast<int>(mapping.modbus_unit_id)
                              << ", Addr=0x" << std::hex << mapping.raw_offset_addr << std::dec
                              << " / " << mapping.raw_offset_addr
                              << "), Value served from cache (Age: " << elapsed << "s)" << std::endl;
                }
            }
        }
    }

    if (!cache_hit) {
        std::lock_guard<std::mutex> lock(serial_mutex_);
        if (!port_->isOpen()) { return false; }

        port_->flushInput();
        if (enable_log_) {
            std::cout << "[" << getMsTimestamp() << "][Modbus TX] ";
            for (size_t i = 0; i < tx_len; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2)
                          << static_cast<int>(tx_buf[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }

        if (port_->writeBytes(tx_buf, tx_len) < 0) {
            std::cerr << "[ModbusClient] Modbus serial write error" << std::endl;
            return false;
        }

        rx_len = port_->readFrame(rx_buf, sizeof(rx_buf), cfg_.rtu_timeout_ms);
        rx_len = filterTxEcho(rx_buf, rx_len, tx_buf, tx_len);
        if (rx_len > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms_));
        }

        if (enable_log_ && rx_len > 0) {
            std::cout << "[" << getMsTimestamp() << "][Modbus RX] ";
            for (int i = 0; i < rx_len; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2)
                          << static_cast<int>(rx_buf[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }

    if (rx_len <= 0) {
        std::cerr << "[ModbusClient] Modbus timeout or read error from unit "
                  << static_cast<int>(mapping.modbus_unit_id)
                  << " (Addr: " << mapping.raw_offset_addr << ")" << std::endl;
        return false;
    }

    if (!cache_hit) {
        if (!ModbusFrameBuilder::validateResponse(rx_buf, rx_len, mapping.modbus_unit_id, fc)) {
            std::cerr << "[ModbusClient] Modbus response validation failed from unit "
                      << static_cast<int>(mapping.modbus_unit_id)
                      << " (FC: " << static_cast<int>(rx_buf[1])
                      << ", code: " << static_cast<int>(rx_buf[2]) << ")" << std::endl;
            return false;
        }

        if (cache_sec_ > 0) {
            std::lock_guard<std::mutex> cache_lock(cache_mutex_);
            auto& entry = cache_table_[cache_key];
            entry.rx_frame.assign(rx_buf, rx_buf + rx_len);
            entry.is_initialized = true;
            entry.last_updated   = std::chrono::steady_clock::now();
            if (enable_log_) {
                std::cout << "[" << getMsTimestamp() << "][Modbus Cache Update] Key=0x"
                          << std::hex << cache_key << std::dec
                          << " (Unit=" << static_cast<int>(mapping.modbus_unit_id)
                          << ", Addr=0x" << std::hex << mapping.raw_offset_addr << std::dec
                          << " / " << mapping.raw_offset_addr << ")" << std::endl;
            }
        }
    }

    const uint8_t byte_count = rx_buf[2];
    if (rx_len < static_cast<int>(byte_count + 5)) {
        std::cerr << "[ModbusClient] Modbus response frame too short" << std::endl;
        return false;
    }

    // 데이터 타입에 따른 값 해석
    if (mapping.data_type == DataType::BIT ||
        mapping.modbus_block == ModbusBlock::COIL ||
        mapping.modbus_block == ModbusBlock::DISCRETE_INPUT)
    {
        uint8_t bit = 0U;
        if (!ModbusFrameBuilder::extractBit(rx_buf, rx_len, bit)) { return false; }
        out_value = (bit != 0U) ? 1.0 : 0.0;
    }
    else if (mapping.data_type == DataType::BIT_PACKED) {
        if (mapping.bit_position < 0 || mapping.bit_position > kMaxBitPosition) {
            std::cerr << "[ModbusClient] Invalid bit_position: " << mapping.bit_position << std::endl;
            return false;
        }
        uint16_t reg_val = 0U;
        if (!ModbusFrameBuilder::extractRegister16(rx_buf, rx_len, reg_val)) { return false; }
        out_value = ((reg_val >> static_cast<unsigned>(mapping.bit_position)) & 0x01U)
                    ? 1.0 : 0.0;
    }
    else {
        const double effective_scale = (mapping.scale == 0.0) ? 1.0 : mapping.scale;
        if (mapping.data_type == DataType::INT16) {
            uint16_t reg_val = 0U;
            if (!ModbusFrameBuilder::extractRegister16(rx_buf, rx_len, reg_val)) { return false; }
            const int16_t signed_val = static_cast<int16_t>(reg_val);
            out_value = static_cast<double>(signed_val) * effective_scale + mapping.offset_val;
        }
        else if (mapping.data_type == DataType::UINT16) {
            uint16_t reg_val = 0U;
            if (!ModbusFrameBuilder::extractRegister16(rx_buf, rx_len, reg_val)) { return false; }
            out_value = static_cast<double>(reg_val) * effective_scale + mapping.offset_val;
        }
        else if (mapping.data_type == DataType::FLOAT32) {
            float float_val = 0.0f;
            if (!ModbusFrameBuilder::extractFloat32(rx_buf, rx_len, float_val, mapping.swap)) {
                return false;
            }
            out_value = static_cast<double>(float_val) * effective_scale + mapping.offset_val;
        }
    }

    if (apply_limits) {
        if (mapping.has_min_value && out_value < mapping.min_value) {
            out_value = mapping.min_value;
        }
        if (mapping.has_max_value && out_value > mapping.max_value) {
            out_value = mapping.max_value;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// serialWrite()
// ---------------------------------------------------------------------------

bool ModbusClient::serialWrite(const ObjectMapping& mapping, double in_value)
{
    if (!port_) { return false; }

    std::lock_guard<std::mutex> lock(serial_mutex_);
    if (!port_->isOpen()) { return false; }

    uint8_t  tx_buf[kTxBufMaxLen];
    uint8_t  rx_buf[kRxBufMaxLen];
    size_t   tx_len  = 0U;
    uint8_t  exp_fc  = 0U;
    const uint16_t raw_addr = toRawAddr(mapping);

    if (mapping.modbus_block == ModbusBlock::COIL) {
        tx_len = ModbusFrameBuilder::buildWriteCoilRequest(
            tx_buf, sizeof(tx_buf),
            mapping.modbus_unit_id, raw_addr, (in_value != 0.0));
        exp_fc = kFcWriteSingleCoil;
    }
    else if (mapping.data_type == DataType::BIT_PACKED) {
        // Read-Modify-Write 시퀀스
        const size_t read_len = ModbusFrameBuilder::buildReadRequest(
            tx_buf, sizeof(tx_buf),
            mapping.modbus_unit_id, kFcReadHoldingRegs, raw_addr, 1U);
        if (read_len == 0U) { return false; }

        port_->flushInput();
        if (enable_log_) {
            std::cout << "[" << getMsTimestamp() << "][Modbus TX] ";
            for (size_t i = 0; i < read_len; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2)
                          << static_cast<int>(tx_buf[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }
        if (port_->writeBytes(tx_buf, read_len) < 0) { return false; }

        int rx_read_len = port_->readFrame(rx_buf, sizeof(rx_buf), cfg_.rtu_timeout_ms);
        rx_read_len = filterTxEcho(rx_buf, rx_read_len, tx_buf, read_len);
        if (rx_read_len > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms_));
        }
        if (enable_log_ && rx_read_len > 0) {
            std::cout << "[" << getMsTimestamp() << "][Modbus RX] ";
            for (int i = 0; i < rx_read_len; ++i) {
                std::cout << std::hex << std::setfill('0') << std::setw(2)
                          << static_cast<int>(rx_buf[i]) << " ";
            }
            std::cout << std::dec << std::endl;
        }

        if (!ModbusFrameBuilder::validateResponse(rx_buf, rx_read_len,
                                                   mapping.modbus_unit_id, kFcReadHoldingRegs)) {
            std::cerr << "[ModbusClient] Failed to read holding register for bit masking" << std::endl;
            return false;
        }

        uint16_t current_val = 0U;
        if (!ModbusFrameBuilder::extractRegister16(rx_buf, rx_read_len, current_val)) {
            return false;
        }

        if (mapping.bit_position < 0 || mapping.bit_position > kMaxBitPosition) {
            std::cerr << "[ModbusClient] Invalid bit_position: " << mapping.bit_position << std::endl;
            return false;
        }
        const uint16_t bit_mask = static_cast<uint16_t>(
            1U << static_cast<unsigned>(mapping.bit_position));
        if (in_value != 0.0) {
            current_val |=  bit_mask;
        } else {
            current_val &= ~bit_mask;
        }

        tx_len = ModbusFrameBuilder::buildWriteSingleRegRequest(
            tx_buf, sizeof(tx_buf),
            mapping.modbus_unit_id, raw_addr, current_val);
        exp_fc = kFcWriteSingleReg;
    }
    else {
        const double effective_scale = (mapping.scale == 0.0) ? 1.0 : mapping.scale;
        if (mapping.data_type == DataType::FLOAT32) {
            const float float_val = static_cast<float>(
                (in_value - mapping.offset_val) / effective_scale);
            tx_len = ModbusFrameBuilder::buildWriteFloat32Request(
                tx_buf, sizeof(tx_buf),
                mapping.modbus_unit_id, raw_addr, float_val, mapping.swap);
            exp_fc = kFcWriteMultipleRegs;
        }
        else {
            // INT16 / UINT16
            const int16_t reg_val = static_cast<int16_t>(
                std::round((in_value - mapping.offset_val) / effective_scale));
            tx_len = ModbusFrameBuilder::buildWriteSingleRegRequest(
                tx_buf, sizeof(tx_buf),
                mapping.modbus_unit_id, raw_addr,
                static_cast<uint16_t>(reg_val));
            exp_fc = kFcWriteSingleReg;
        }
    }

    if (tx_len == 0U) {
        std::cerr << "[ModbusClient] Frame build error in serialWrite" << std::endl;
        return false;
    }

    port_->flushInput();
    if (enable_log_) {
        std::cout << "[" << getMsTimestamp() << "][Modbus TX] ";
        for (size_t i = 0; i < tx_len; ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(tx_buf[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    if (port_->writeBytes(tx_buf, tx_len) < 0) {
        std::cerr << "[ModbusClient] Modbus serial write error during write command" << std::endl;
        return false;
    }

    int rx_len = port_->readFrame(rx_buf, sizeof(rx_buf), cfg_.rtu_timeout_ms);
    rx_len = filterTxEcho(rx_buf, rx_len, tx_buf, tx_len);
    if (rx_len > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(inter_frame_delay_ms_));
    }

    if (enable_log_ && rx_len > 0) {
        std::cout << "[" << getMsTimestamp() << "][Modbus RX] ";
        for (int i = 0; i < rx_len; ++i) {
            std::cout << std::hex << std::setfill('0') << std::setw(2)
                      << static_cast<int>(rx_buf[i]) << " ";
        }
        std::cout << std::dec << std::endl;
    }

    if (!ModbusFrameBuilder::validateResponse(rx_buf, rx_len, mapping.modbus_unit_id, exp_fc)) {
        std::cerr << "[ModbusClient] Modbus write response validation failed (FC: "
                  << static_cast<int>((rx_len > 1) ? rx_buf[1] : 0U) << ")" << std::endl;
        return false;
    }

    // 쓰기 성공 시 캐시 무효화
    if (cache_sec_ > 0) {
        uint32_t cache_key = (static_cast<uint32_t>(mapping.modbus_unit_id) << 24) |
                             (static_cast<uint32_t>(mapping.modbus_block)   << 16) |
                              static_cast<uint32_t>(raw_addr);
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        cache_table_.erase(cache_key);
        if (enable_log_) {
            std::cout << "[" << getMsTimestamp() << "][Modbus Cache Invalidate] Key=0x"
                      << std::hex << cache_key << std::dec
                      << " (Unit=" << static_cast<int>(mapping.modbus_unit_id)
                      << ", Addr=0x" << std::hex << raw_addr << std::dec
                      << " / " << raw_addr << ") due to write operation" << std::endl;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// filterTxEcho()
// ---------------------------------------------------------------------------

int ModbusClient::filterTxEcho(uint8_t*       rx_buf,
                                int            rx_len,
                                const uint8_t* tx_buf,
                                size_t         tx_len)
{
    if (!port_ || !port_->isHardwarePort()) { return rx_len; }
    if (!cfg_.hardware_echo)                { return rx_len; }

    if (rx_len == static_cast<int>(tx_len) &&
        std::memcmp(rx_buf, tx_buf, tx_len) == 0)
    {
        return port_->readFrame(rx_buf, kRxBufMaxLen, cfg_.rtu_timeout_ms);
    }
    return rx_len;
}

// ---------------------------------------------------------------------------
// 정적 유틸리티
// ---------------------------------------------------------------------------

uint16_t ModbusClient::toRawAddr(const ObjectMapping& m)
{
    return static_cast<uint16_t>(m.raw_offset_addr & 0xFFFFU);
}

std::string ModbusClient::getMsTimestamp()
{
    using namespace std::chrono;
    auto now   = system_clock::now();
    auto ms    = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto timer = system_clock::to_time_t(now);
    std::tm bt;
    localtime_r(&timer, &bt);
    std::ostringstream oss;
    oss << std::put_time(&bt, "%H:%M:%S") << '.'
        << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

uint32_t ModbusClient::calculateInterFrameDelayMs(int baudrate)
{
    if (baudrate <= 0) {
        return 2U;
    }

    // Modbus RTU 사양 (Modbus over Serial Line Specification and Implementation Guide V1.02, 2.5.1.1):
    // 19,200 bps 초과 속도에서는 t3.5 = 1.750 ms 고정값 사용 권장 -> 밀리초 단위 올림 처리로 최소 2ms 적용.
    // 19,200 bps (약 2.005 ms) 역시 2ms 간격으로 표준 충족.
    if (baudrate >= 19200) {
        return 2U;
    }

    // 19,200 bps 미만 저속 통신 (9600, 4800, 2400 bps 등):
    // 1 문자 = 11 bits (Start 1, Data 8, Parity 1, Stop 1 또는 Stop 2)
    // t3.5 = 3.5 * 11 / baudrate (초) = 38,500 / baudrate (ms)
    // 슬레이브 기기의 상태 머신(FSM) 복귀를 확실히 보장하기 위해 정수 올림(ceil) 계산 적용
    // 예: 9600 bps -> 5ms (4.01ms 올림), 4800 bps -> 9ms (8.02ms 올림)
    const uint32_t t35_ms = static_cast<uint32_t>((38500 + baudrate - 1) / baudrate);
    return std::max<uint32_t>(2U, t35_ms);
}

} // namespace gateway
