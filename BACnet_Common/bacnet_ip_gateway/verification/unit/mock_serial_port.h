/**
 * @file mock_serial_port.h
 * @brief ISerialPort 모의(Mock) 구현체 — 단위 테스트 전용
 *
 * 설계 목적:
 *  - 물리적 시리얼 포트 없이 Modbus RTU 통신 로직을 단위 테스트할 수 있도록
 *    ISerialPort 의 가짜(Mock) 구현을 제공합니다.
 *  - 테스트별로 미리 정해진 RX 데이터를 큐(std::deque)에 삽입해 두면,
 *    readFrame() 호출 시 해당 데이터를 순서대로 반환합니다.
 *
 * 라이선스 준수:
 *  - 외부 라이브러리 의존 없음. C++11 STL 만 사용.
 *
 * 표준: C++11
 */

#pragma once

#include "../src/common/iserial_port.h"
#include <vector>
#include <deque>
#include <mutex>
#include <cstring>   // std::memcpy
#include <cstdint>
#include <cstddef>
#include <string>

namespace test {

/**
 * @brief ISerialPort 의 Mock 구현체.
 *
 * 사용법:
 * @code
 *   MockSerialPort mock;
 *   mock.setIsOpen(true);
 *   mock.pushRxFrame({0x01, 0x03, 0x02, 0x01, 0xF4, 0xB9, 0xD0}); // 응답 삽입
 *   // readFrame() 은 큐에서 꺼낸 데이터를 반환합니다.
 * @endcode
 */
class MockSerialPort : public modbus::ISerialPort {
public:
    MockSerialPort()
        : is_open_(false)
        , is_hardware_port_(false)
        , last_write_len_(0)
    {}

    ~MockSerialPort() override = default;

    std::mutex& getMutex() override { return mock_mutex_; }

    // -----------------------------------------------------------------------
    // 테스트 제어 메서드
    // -----------------------------------------------------------------------

    /// 포트 열림 상태를 설정합니다.
    void setIsOpen(bool open)          { is_open_ = open; }

    /// 하드웨어 포트 여부를 설정합니다 (true → TX echo 필터 활성화).
    void setIsHardwarePort(bool hw)    { is_hardware_port_ = hw; }

    /**
     * @brief 다음 readFrame() 호출에서 반환할 RX 프레임을 추가합니다.
     *
     * 여러 번 호출하면 선입선출(FIFO) 순서로 반환됩니다.
     */
    void pushRxFrame(const std::vector<uint8_t>& frame)
    {
        rx_queue_.push_back(frame);
    }

    /// 마지막으로 writeBytes()에 전달된 데이터를 반환합니다.
    const std::vector<uint8_t>& getLastWritten() const { return last_write_buf_; }

    /// writeBytes() 가 호출된 총 횟수를 반환합니다.
    int getWriteCallCount() const { return write_call_count_; }

    /// readFrame() 이 호출된 총 횟수를 반환합니다.
    int getReadCallCount() const { return read_call_count_; }

    /// flushInput() 이 호출된 총 횟수를 반환합니다.
    int getFlushCallCount() const { return flush_call_count_; }

    // -----------------------------------------------------------------------
    // ISerialPort 구현
    // -----------------------------------------------------------------------

    bool isOpen() const override { return is_open_; }

    int writeBytes(const uint8_t* buffer, size_t length) override
    {
        if (!is_open_ || buffer == nullptr) { return -1; }
        ++write_call_count_;
        last_write_buf_.assign(buffer, buffer + length);
        last_write_len_ = length;
        return static_cast<int>(length);
    }

    void drainTxEcho(size_t /*echo_len*/) override
    {
        // Mock: 아무 동작 없음 (하드웨어 에코 없음)
    }

    /**
     * @brief 미리 삽입된 RX 프레임 큐에서 꺼내어 반환합니다.
     *
     * 큐가 비어 있으면 0을 반환합니다 (타임아웃 시뮬레이션).
     */
    int readFrame(uint8_t* buffer, size_t max_length, int /*timeout_ms*/) override
    {
        if (!is_open_ || buffer == nullptr) { return -1; }
        ++read_call_count_;

        if (rx_queue_.empty()) {
            return 0; // 타임아웃 시뮬레이션
        }

        const std::vector<uint8_t>& frame = rx_queue_.front();
        const size_t copy_len = (frame.size() < max_length) ? frame.size() : max_length;
        std::memcpy(buffer, frame.data(), copy_len);
        rx_queue_.pop_front();
        return static_cast<int>(copy_len);
    }

    void flushInput() override
    {
        ++flush_call_count_;
        // Mock: 실제로 버퍼를 비울 필요 없음
    }

    bool isHardwarePort() const override { return is_hardware_port_; }

    std::string getDeviceName() const override { return "MockSerialPort"; }

    // -----------------------------------------------------------------------
    // ISerialPort open/close 구현
    // -----------------------------------------------------------------------

    bool open(const std::string& /*device*/,
              int /*baudrate*/ = 19200,
              const std::string& /*parity*/ = "none",
              int /*stop_bits*/ = 1,
              int /*data_bits*/ = 8) override
    {
        is_open_ = true;
        return true;
    }

    void close() override
    {
        is_open_ = false;
    }

    // -----------------------------------------------------------------------
    // 모의 포트 직접 open/close 지원 (하위 호환성 유지)
    // -----------------------------------------------------------------------

    bool openSerial(const std::string& device,
                    int baudrate = 19200,
                    const std::string& parity = "none",
                    int stop_bits = 1,
                    int data_bits = 8)
    {
        return open(device, baudrate, parity, stop_bits, data_bits);
    }

    void closePort()
    {
        close();
    }

private:
    std::mutex                      mock_mutex_;
    bool                            is_open_;
    bool                            is_hardware_port_;
    std::deque<std::vector<uint8_t>> rx_queue_;
    std::vector<uint8_t>            last_write_buf_;
    size_t                          last_write_len_;
    int                             write_call_count_  = 0;
    int                             read_call_count_   = 0;
    int                             flush_call_count_  = 0;
};

} // namespace test
