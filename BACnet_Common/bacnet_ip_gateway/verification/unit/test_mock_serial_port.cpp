/**
 * @file test_mock_serial_port.cpp
 * @brief MockSerialPort 동작 및 ModbusFrameBuilder + MockSerialPort 통합 테스트
 *
 * 이 파일은 MockSerialPort 를 사용하여 Modbus RTU 통신 시퀀스를
 * 물리적 하드웨어 없이 검증합니다.
 *
 * 테스트 케이스:
 *  - MockSerialPort 상태 제어 (open/close/flush)
 *  - 미리 삽입된 RX 프레임 반환 순서
 *  - TX echo 프레임 필터링 시뮬레이션
 *  - 닫힌 포트에 쓰기 시 -1 반환
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "mock_serial_port.h"
#include "../src/common/modbus_frame_builder.h"
#include "../src/common/modbus_crc.h"
#include <vector>
#include <cstdint>

using namespace modbus;
using test::MockSerialPort;

// ---------------------------------------------------------------------------
// MockSerialPort 기본 동작 테스트
// ---------------------------------------------------------------------------

TEST_CASE("MockSerialPort: isOpen returns false by default") {
    MockSerialPort mock;
    ASSERT_FALSE(mock.isOpen());
}

TEST_CASE("MockSerialPort: setIsOpen(true) makes isOpen() return true") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    ASSERT_TRUE(mock.isOpen());
}

TEST_CASE("MockSerialPort: closePort sets isOpen to false") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    mock.closePort();
    ASSERT_FALSE(mock.isOpen());
}

TEST_CASE("MockSerialPort: writeBytes on closed port returns -1") {
    MockSerialPort mock; // 닫힌 상태
    const uint8_t data[] = {0x01, 0x02};
    ASSERT_EQ(-1, mock.writeBytes(data, sizeof(data)));
}

TEST_CASE("MockSerialPort: writeBytes on open port returns written length") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    const uint8_t data[] = {0xAA, 0xBB, 0xCC};
    const int result = mock.writeBytes(data, sizeof(data));
    ASSERT_EQ(3, result);
}

TEST_CASE("MockSerialPort: getLastWritten reflects most recent writeBytes call") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    const uint8_t data[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
    mock.writeBytes(data, sizeof(data));
    const auto& last = mock.getLastWritten();
    ASSERT_EQ(static_cast<size_t>(8), last.size());
    ASSERT_EQ(static_cast<uint8_t>(0x01), last[0]);
    ASSERT_EQ(static_cast<uint8_t>(0x03), last[1]);
}

TEST_CASE("MockSerialPort: writeBytes increments write call count") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    const uint8_t data[] = {0x01};
    mock.writeBytes(data, 1);
    mock.writeBytes(data, 1);
    ASSERT_EQ(2, mock.getWriteCallCount());
}

TEST_CASE("MockSerialPort: readFrame on empty queue returns 0 (timeout simulation)") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    uint8_t buf[16];
    ASSERT_EQ(0, mock.readFrame(buf, sizeof(buf), 500));
}

TEST_CASE("MockSerialPort: readFrame returns pushed frame in FIFO order") {
    MockSerialPort mock;
    mock.setIsOpen(true);

    const std::vector<uint8_t> frame1 = {0xAA, 0xBB};
    const std::vector<uint8_t> frame2 = {0xCC, 0xDD, 0xEE};
    mock.pushRxFrame(frame1);
    mock.pushRxFrame(frame2);

    uint8_t buf[16];

    // 1차: frame1
    int len = mock.readFrame(buf, sizeof(buf), 100);
    ASSERT_EQ(2, len);
    ASSERT_EQ(static_cast<uint8_t>(0xAA), buf[0]);
    ASSERT_EQ(static_cast<uint8_t>(0xBB), buf[1]);

    // 2차: frame2
    len = mock.readFrame(buf, sizeof(buf), 100);
    ASSERT_EQ(3, len);
    ASSERT_EQ(static_cast<uint8_t>(0xCC), buf[0]);
}

TEST_CASE("MockSerialPort: readFrame respects max_length truncation") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    mock.pushRxFrame({0x01, 0x02, 0x03, 0x04, 0x05});
    uint8_t buf[3]; // max 3바이트
    const int len = mock.readFrame(buf, sizeof(buf), 100);
    ASSERT_EQ(3, len); // 최대값으로 잘림
}

TEST_CASE("MockSerialPort: flushInput increments flush call count") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    mock.flushInput();
    mock.flushInput();
    ASSERT_EQ(2, mock.getFlushCallCount());
}

TEST_CASE("MockSerialPort: isHardwarePort reflects setIsHardwarePort") {
    MockSerialPort mock;
    ASSERT_FALSE(mock.isHardwarePort());
    mock.setIsHardwarePort(true);
    ASSERT_TRUE(mock.isHardwarePort());
}

TEST_CASE("MockSerialPort: getDeviceName returns MockSerialPort") {
    MockSerialPort mock;
    ASSERT_TRUE(mock.getDeviceName() == "MockSerialPort");
}

// ---------------------------------------------------------------------------
// MockSerialPort + ModbusFrameBuilder 통합 시뮬레이션 테스트
// ---------------------------------------------------------------------------

/**
 * @brief FC03 읽기 트랜잭션 전체 흐름을 Mock 으로 시뮬레이션합니다.
 *
 * 시나리오:
 *  1. buildReadRequest() 로 TX 프레임 조립
 *  2. mock.writeBytes() 로 전송 (TX 내용 기록)
 *  3. 미리 삽입된 응답 프레임으로 readFrame() 응답
 *  4. validateResponse() + extractRegister16() 로 값 추출
 */
TEST_CASE("Integration: FC03 read transaction via MockSerialPort returns correct INT16") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    mock.setIsHardwarePort(false); // PTY 모드: echo 없음

    // 예상 응답: 슬레이브=0x01, FC=0x03, ByteCnt=0x02, Data=500(0x01F4)
    uint8_t resp_data[7];
    resp_data[0] = 0x01;
    resp_data[1] = 0x03;
    resp_data[2] = 0x02;
    resp_data[3] = 0x01;
    resp_data[4] = 0xF4;
    const uint16_t crc = calculateCRC16(resp_data, 5);
    resp_data[5] = static_cast<uint8_t>(crc & 0xFF);
    resp_data[6] = static_cast<uint8_t>(crc >> 8);
    mock.pushRxFrame(std::vector<uint8_t>(resp_data, resp_data + 7));

    // TX 프레임 조립
    uint8_t tx_buf[32];
    const size_t tx_len = ModbusFrameBuilder::buildReadRequest(
        tx_buf, sizeof(tx_buf), 0x01, kFcReadHoldingRegs, 0x0064, 0x0001);
    ASSERT_EQ(static_cast<size_t>(8), tx_len);

    // 전송 및 수신
    ASSERT_EQ(static_cast<int>(tx_len), mock.writeBytes(tx_buf, tx_len));
    uint8_t rx_buf[32];
    const int rx_len = mock.readFrame(rx_buf, sizeof(rx_buf), 500);
    ASSERT_EQ(7, rx_len);

    // 유효성 검사
    ASSERT_TRUE(ModbusFrameBuilder::validateResponse(rx_buf, rx_len, 0x01, 0x03));

    // 값 추출 및 검증
    uint16_t raw_val = 0U;
    ASSERT_TRUE(ModbusFrameBuilder::extractRegister16(rx_buf, rx_len, raw_val));
    // scale=0.1 적용: 500 * 0.1 = 50.0
    const double final_val = static_cast<double>(static_cast<int16_t>(raw_val)) * 0.1;
    ASSERT_NEAR(50.0, final_val, 1e-6);
}

/**
 * @brief 타임아웃(RX 큐 비어있음) 시 읽기 실패를 검증합니다.
 */
TEST_CASE("Integration: FC03 read returns timeout (rx queue empty) when no response queued") {
    MockSerialPort mock;
    mock.setIsOpen(true);
    // RX 큐에 아무것도 삽입하지 않음

    uint8_t tx_buf[32];
    ModbusFrameBuilder::buildReadRequest(tx_buf, sizeof(tx_buf),
                                          0x01, kFcReadHoldingRegs, 0x0000, 0x0001);
    mock.writeBytes(tx_buf, 8);

    uint8_t rx_buf[32];
    const int rx_len = mock.readFrame(rx_buf, sizeof(rx_buf), 500);
    ASSERT_EQ(0, rx_len); // 타임아웃
}

/**
 * @brief Modbus 예외 응답 수신 시 validateResponse() 가 false 를 반환하는지 검증합니다.
 */
TEST_CASE("Integration: Modbus exception response rejected by validateResponse") {
    MockSerialPort mock;
    mock.setIsOpen(true);

    // 예외 응답 프레임: [0x01][0x83][0x02][CRCL][CRCH]
    uint8_t exc_frame[5];
    exc_frame[0] = 0x01;
    exc_frame[1] = 0x03 | 0x80U; // 예외 FC
    exc_frame[2] = 0x02;          // Illegal Data Address
    const uint16_t crc = calculateCRC16(exc_frame, 3);
    exc_frame[3] = static_cast<uint8_t>(crc & 0xFF);
    exc_frame[4] = static_cast<uint8_t>(crc >> 8);
    mock.pushRxFrame(std::vector<uint8_t>(exc_frame, exc_frame + 5));

    uint8_t rx_buf[32];
    const int rx_len = mock.readFrame(rx_buf, sizeof(rx_buf), 500);
    ASSERT_EQ(5, rx_len);
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(rx_buf, rx_len, 0x01, 0x03));
}

/**
 * @brief FC05 코일 쓰기 후 정상 응답 수신 시뮬레이션.
 */
TEST_CASE("Integration: FC05 write coil transaction succeeds") {
    MockSerialPort mock;
    mock.setIsOpen(true);

    // Modbus FC05 정상 응답은 요청 프레임 에코와 동일 (8바이트)
    uint8_t ack_frame[8];
    ack_frame[0] = 0x01;
    ack_frame[1] = 0x05;
    ack_frame[2] = 0x00;
    ack_frame[3] = 0x63; // coil addr = 99
    ack_frame[4] = 0xFF;
    ack_frame[5] = 0x00; // ON
    const uint16_t crc = calculateCRC16(ack_frame, 6);
    ack_frame[6] = static_cast<uint8_t>(crc & 0xFF);
    ack_frame[7] = static_cast<uint8_t>(crc >> 8);
    mock.pushRxFrame(std::vector<uint8_t>(ack_frame, ack_frame + 8));

    // TX 프레임 조립
    uint8_t tx_buf[32];
    const size_t tx_len = ModbusFrameBuilder::buildWriteCoilRequest(
        tx_buf, sizeof(tx_buf), 0x01, 0x0063, true);
    mock.writeBytes(tx_buf, tx_len);

    uint8_t rx_buf[32];
    const int rx_len = mock.readFrame(rx_buf, sizeof(rx_buf), 500);
    ASSERT_EQ(8, rx_len);
    ASSERT_TRUE(ModbusFrameBuilder::validateResponse(rx_buf, rx_len, 0x01, 0x05));
}
