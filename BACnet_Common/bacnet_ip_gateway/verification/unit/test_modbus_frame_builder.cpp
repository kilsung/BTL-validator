/**
 * @file test_modbus_frame_builder.cpp
 * @brief modbus::ModbusFrameBuilder 단위 테스트
 *
 * 테스트 케이스 구성:
 *  [정상 케이스]
 *   - 각 FC 별 요청 프레임 조립 후 CRC 유효성 검증
 *   - 응답 프레임 해석 (INT16, UINT16, FLOAT32, BIT, BIT_PACKED)
 *   - validateResponse() 정상 통과
 *
 *  [경계/예외 케이스]
 *   - nullptr 버퍼 입력 시 0/false 반환
 *   - 버퍼 크기 부족 시 0 반환 (Buffer Overflow 방지)
 *   - Modbus 예외 응답(FC | 0x80) 감지
 *   - CRC 불일치 프레임 거부
 *   - 응답 길이 부족 시 false 반환
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "../src/common/modbus_frame_builder.h"
#include "../src/common/modbus_crc.h"
#include <cstring>   // std::memcpy
#include <cstdint>
#include <vector>
#include <cmath>     // std::fabs

using namespace modbus;

// ---------------------------------------------------------------------------
// 헬퍼: CRC를 포함한 완전한 읽기 응답 프레임 생성
// ---------------------------------------------------------------------------

static std::vector<uint8_t> makeReadResponse(uint8_t unit_id,
                                              uint8_t fc,
                                              const std::vector<uint8_t>& data_bytes)
{
    std::vector<uint8_t> frame;
    frame.push_back(unit_id);
    frame.push_back(fc);
    frame.push_back(static_cast<uint8_t>(data_bytes.size())); // byte count
    for (auto b : data_bytes) { frame.push_back(b); }
    const uint16_t crc = calculateCRC16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc & 0xFF));
    frame.push_back(static_cast<uint8_t>(crc >> 8));
    return frame;
}

// ---------------------------------------------------------------------------
// buildReadRequest 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: buildReadRequest produces valid 8-byte frame with correct CRC") {
    uint8_t buf[32];
    const size_t len = ModbusFrameBuilder::buildReadRequest(
        buf, sizeof(buf), 0x01, kFcReadHoldingRegs, 0x006B, 0x0003);
    ASSERT_EQ(static_cast<size_t>(8), len);

    // CRC 유효성 검사
    ASSERT_TRUE(verifyCRC16(buf, len));

    // 필드 값 검사
    ASSERT_EQ(static_cast<uint8_t>(0x01), buf[0]); // unit_id
    ASSERT_EQ(static_cast<uint8_t>(0x03), buf[1]); // FC
    ASSERT_EQ(static_cast<uint8_t>(0x00), buf[2]); // addr high
    ASSERT_EQ(static_cast<uint8_t>(0x6B), buf[3]); // addr low
    ASSERT_EQ(static_cast<uint8_t>(0x00), buf[4]); // qty high
    ASSERT_EQ(static_cast<uint8_t>(0x03), buf[5]); // qty low
}

TEST_CASE("FrameBuilder: buildReadRequest with nullptr returns 0 (security check)") {
    const size_t len = ModbusFrameBuilder::buildReadRequest(
        nullptr, 32, 0x01, kFcReadHoldingRegs, 0x0000, 0x0001);
    ASSERT_EQ(static_cast<size_t>(0), len);
}

TEST_CASE("FrameBuilder: buildReadRequest with insufficient buffer returns 0") {
    uint8_t small_buf[4]; // 최소 8바이트 필요
    const size_t len = ModbusFrameBuilder::buildReadRequest(
        small_buf, sizeof(small_buf), 0x01, kFcReadHoldingRegs, 0x0000, 0x0001);
    ASSERT_EQ(static_cast<size_t>(0), len);
}

// ---------------------------------------------------------------------------
// buildWriteCoilRequest 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: buildWriteCoilRequest ON produces 0xFF00 coil value") {
    uint8_t buf[32];
    const size_t len = ModbusFrameBuilder::buildWriteCoilRequest(
        buf, sizeof(buf), 0x01, 0x0063, true);
    ASSERT_EQ(static_cast<size_t>(8), len);
    ASSERT_TRUE(verifyCRC16(buf, len));
    ASSERT_EQ(static_cast<uint8_t>(0x05), buf[1]); // FC 05
    ASSERT_EQ(static_cast<uint8_t>(0xFF), buf[4]); // ON high byte
    ASSERT_EQ(static_cast<uint8_t>(0x00), buf[5]); // ON low byte
}

TEST_CASE("FrameBuilder: buildWriteCoilRequest OFF produces 0x0000 coil value") {
    uint8_t buf[32];
    ModbusFrameBuilder::buildWriteCoilRequest(
        buf, sizeof(buf), 0x01, 0x0063, false);
    ASSERT_EQ(static_cast<uint8_t>(0x00), buf[4]);
    ASSERT_EQ(static_cast<uint8_t>(0x00), buf[5]);
}

TEST_CASE("FrameBuilder: buildWriteCoilRequest with nullptr returns 0") {
    const size_t len = ModbusFrameBuilder::buildWriteCoilRequest(
        nullptr, 32, 0x01, 0x0000, true);
    ASSERT_EQ(static_cast<size_t>(0), len);
}

// ---------------------------------------------------------------------------
// buildWriteSingleRegRequest 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: buildWriteSingleRegRequest encodes register value correctly") {
    uint8_t buf[32];
    const uint16_t reg_val = 0x1234;
    const size_t len = ModbusFrameBuilder::buildWriteSingleRegRequest(
        buf, sizeof(buf), 0x01, 0x0010, reg_val);
    ASSERT_EQ(static_cast<size_t>(8), len);
    ASSERT_TRUE(verifyCRC16(buf, len));
    ASSERT_EQ(static_cast<uint8_t>(0x06), buf[1]); // FC 06
    ASSERT_EQ(static_cast<uint8_t>(0x12), buf[4]); // High byte
    ASSERT_EQ(static_cast<uint8_t>(0x34), buf[5]); // Low byte
}

// ---------------------------------------------------------------------------
// buildWriteFloat32Request 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: buildWriteFloat32Request produces valid 13-byte frame") {
    uint8_t buf[32];
    const float test_val = 123.456f;
    const size_t len = ModbusFrameBuilder::buildWriteFloat32Request(
        buf, sizeof(buf), 0x01, 0x0020, test_val);
    ASSERT_EQ(static_cast<size_t>(13), len);
    ASSERT_TRUE(verifyCRC16(buf, len));
    ASSERT_EQ(static_cast<uint8_t>(0x10), buf[1]); // FC 16
    ASSERT_EQ(static_cast<uint8_t>(0x02), buf[5]); // qty = 2 registers
    ASSERT_EQ(static_cast<uint8_t>(0x04), buf[6]); // byte count = 4
}

TEST_CASE("FrameBuilder: buildWriteFloat32Request round-trips float value via extractFloat32") {
    uint8_t tx_buf[32];
    const float original = -273.15f;
    const size_t tx_len = ModbusFrameBuilder::buildWriteFloat32Request(
        tx_buf, sizeof(tx_buf), 0x01, 0x0000, original);
    ASSERT_TRUE(tx_len > 0);

    // FC16 응답 형태로 재포맷하여 extractFloat32 테스트
    // (실제 슬레이브 응답은 별도 포맷이지만, 데이터 직렬화 일관성 검증)
    // 대신: 직접 응답 프레임 조립으로 검증
    std::vector<uint8_t> rx_data = {
        tx_buf[7], tx_buf[8], tx_buf[9], tx_buf[10]
    };
    // 응답 프레임: [unit][FC03][bytecount=4][D0][D1][D2][D3][CRCL][CRCH]
    auto resp = makeReadResponse(0x01, 0x03, rx_data);

    float extracted = 0.0f;
    ASSERT_TRUE(ModbusFrameBuilder::extractFloat32(resp.data(),
                static_cast<int>(resp.size()), extracted));
    ASSERT_NEAR(static_cast<double>(original), static_cast<double>(extracted), 1e-3);
}

TEST_CASE("FrameBuilder: buildWriteFloat32Request with small buffer returns 0") {
    uint8_t buf[10]; // 최소 13바이트 필요
    const size_t len = ModbusFrameBuilder::buildWriteFloat32Request(
        buf, sizeof(buf), 0x01, 0x0000, 1.0f);
    ASSERT_EQ(static_cast<size_t>(0), len);
}

// ---------------------------------------------------------------------------
// validateResponse 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: validateResponse passes for correct FC03 response") {
    auto resp = makeReadResponse(0x01, 0x03, {0x01, 0xF4}); // value=500
    ASSERT_TRUE(ModbusFrameBuilder::validateResponse(
        resp.data(), static_cast<int>(resp.size()), 0x01, 0x03));
}

TEST_CASE("FrameBuilder: validateResponse rejects nullptr buffer") {
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(nullptr, 7, 0x01, 0x03));
}

TEST_CASE("FrameBuilder: validateResponse rejects zero-length response") {
    uint8_t buf[8] = {};
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(buf, 0, 0x01, 0x03));
}

TEST_CASE("FrameBuilder: validateResponse rejects wrong unit_id") {
    auto resp = makeReadResponse(0x01, 0x03, {0x01, 0xF4});
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(
        resp.data(), static_cast<int>(resp.size()), 0x02, 0x03)); // unit_id 불일치
}

TEST_CASE("FrameBuilder: validateResponse rejects Modbus exception response (FC | 0x80)") {
    // Modbus 예외 응답 프레임: [unit][FC|0x80][exception_code][CRCL][CRCH]
    uint8_t frame[5];
    frame[0] = 0x01;
    frame[1] = 0x03 | 0x80U; // 예외 FC
    frame[2] = 0x02;          // exception code: Illegal Data Address
    const uint16_t crc = calculateCRC16(frame, 3);
    frame[3] = static_cast<uint8_t>(crc & 0xFF);
    frame[4] = static_cast<uint8_t>(crc >> 8);
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(frame, 5, 0x01, 0x03));
}

TEST_CASE("FrameBuilder: validateResponse rejects frame with corrupted CRC") {
    auto resp = makeReadResponse(0x01, 0x03, {0x01, 0xF4});
    resp.back() ^= 0xFF; // CRC 변조
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(
        resp.data(), static_cast<int>(resp.size()), 0x01, 0x03));
}

TEST_CASE("FrameBuilder: validateResponse rejects too-short frame") {
    uint8_t frame[4] = {0x01, 0x03, 0x01, 0xCB}; // 4바이트 < minimum 5
    ASSERT_FALSE(ModbusFrameBuilder::validateResponse(frame, 4, 0x01, 0x03));
}

// ---------------------------------------------------------------------------
// extractRegister16 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: extractRegister16 decodes INT16 value correctly") {
    // 응답: 슬레이브=0x01, FC=0x04, ByteCnt=0x02, Data=0x01F4 (500)
    auto resp = makeReadResponse(0x01, 0x04, {0x01, 0xF4});
    uint16_t val = 0U;
    ASSERT_TRUE(ModbusFrameBuilder::extractRegister16(
        resp.data(), static_cast<int>(resp.size()), val));
    ASSERT_EQ(static_cast<uint16_t>(500), val);
}

TEST_CASE("FrameBuilder: extractRegister16 with nullptr returns false") {
    uint16_t val = 0U;
    ASSERT_FALSE(ModbusFrameBuilder::extractRegister16(nullptr, 7, val));
}

TEST_CASE("FrameBuilder: extractRegister16 with too-short frame returns false") {
    const uint8_t short_frame[] = {0x01, 0x03, 0x02, 0x00}; // 4바이트 < 7
    uint16_t val = 0U;
    ASSERT_FALSE(ModbusFrameBuilder::extractRegister16(short_frame, 4, val));
}

// ---------------------------------------------------------------------------
// extractFloat32 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: extractFloat32 decodes IEEE 754 float correctly") {
    const float original = 3.14f;
    uint32_t raw = 0U;
    std::memcpy(&raw, &original, sizeof(raw));

    std::vector<uint8_t> data_bytes = {
        static_cast<uint8_t>(raw >> 24),
        static_cast<uint8_t>(raw >> 16),
        static_cast<uint8_t>(raw >>  8),
        static_cast<uint8_t>(raw & 0xFF)
    };
    auto resp = makeReadResponse(0x01, 0x04, data_bytes);

    float extracted = 0.0f;
    ASSERT_TRUE(ModbusFrameBuilder::extractFloat32(
        resp.data(), static_cast<int>(resp.size()), extracted));
    ASSERT_NEAR(static_cast<double>(original), static_cast<double>(extracted), 1e-5);
}

TEST_CASE("FrameBuilder: extractFloat32 with too-short frame returns false") {
    const uint8_t short_frame[8] = {};
    float val = 0.0f;
    // 9바이트 미만이어야 false
    ASSERT_FALSE(ModbusFrameBuilder::extractFloat32(short_frame, 8, val));
}

TEST_CASE("FrameBuilder: extractFloat32 with word_swap CDAB decodes swapped words correctly") {
    const float original = 123.456f;
    uint32_t raw = 0U;
    std::memcpy(&raw, &original, sizeof(raw));

    // CDAB (word_swap = true): Low Word First [Low_H, Low_L, High_H, High_L]
    std::vector<uint8_t> cdab_bytes = {
        static_cast<uint8_t>((raw >>  8) & 0xFF),
        static_cast<uint8_t>(raw & 0xFF),
        static_cast<uint8_t>((raw >> 24) & 0xFF),
        static_cast<uint8_t>((raw >> 16) & 0xFF)
    };
    auto resp = makeReadResponse(0x01, 0x04, cdab_bytes);

    float extracted = 0.0f;
    ASSERT_TRUE(ModbusFrameBuilder::extractFloat32(
        resp.data(), static_cast<int>(resp.size()), extracted, true /* word_swap */));
    ASSERT_NEAR(static_cast<double>(original), static_cast<double>(extracted), 1e-4);
}

TEST_CASE("FrameBuilder: buildWriteFloat32Request with word_swap formats CDAB frame and round-trips") {
    uint8_t tx_buf[32];
    const float original = 987.654f;
    uint32_t raw = 0U;
    std::memcpy(&raw, &original, sizeof(raw));

    // buildWriteFloat32Request with word_swap = true
    const size_t tx_len = ModbusFrameBuilder::buildWriteFloat32Request(
        tx_buf, sizeof(tx_buf), 0x01, 0x0004, original, true /* word_swap */);
    ASSERT_TRUE(tx_len > 0);

    // Verify CDAB byte order in write request payload: bytes 7..10
    ASSERT_EQ(static_cast<uint8_t>((raw >>  8) & 0xFF), tx_buf[7]);
    ASSERT_EQ(static_cast<uint8_t>(raw & 0xFF),         tx_buf[8]);
    ASSERT_EQ(static_cast<uint8_t>((raw >> 24) & 0xFF), tx_buf[9]);
    ASSERT_EQ(static_cast<uint8_t>((raw >> 16) & 0xFF), tx_buf[10]);

    // Round-trip verification with extractFloat32 (word_swap = true)
    std::vector<uint8_t> rx_data = { tx_buf[7], tx_buf[8], tx_buf[9], tx_buf[10] };
    auto resp = makeReadResponse(0x01, 0x03, rx_data);

    float extracted = 0.0f;
    ASSERT_TRUE(ModbusFrameBuilder::extractFloat32(
        resp.data(), static_cast<int>(resp.size()), extracted, true /* word_swap */));
    ASSERT_NEAR(static_cast<double>(original), static_cast<double>(extracted), 1e-4);
}

// ---------------------------------------------------------------------------
// extractBit 테스트
// ---------------------------------------------------------------------------

TEST_CASE("FrameBuilder: extractBit reads bit=1 from coil response correctly") {
    auto resp = makeReadResponse(0x01, 0x01, {0x01}); // 코일 = ON
    uint8_t bit = 0U;
    ASSERT_TRUE(ModbusFrameBuilder::extractBit(
        resp.data(), static_cast<int>(resp.size()), bit));
    ASSERT_EQ(static_cast<uint8_t>(1), bit);
}

TEST_CASE("FrameBuilder: extractBit reads bit=0 from coil response correctly") {
    auto resp = makeReadResponse(0x01, 0x01, {0x00}); // 코일 = OFF
    uint8_t bit = 0U;
    ModbusFrameBuilder::extractBit(resp.data(), static_cast<int>(resp.size()), bit);
    ASSERT_EQ(static_cast<uint8_t>(0), bit);
}

TEST_CASE("FrameBuilder: extractBit with nullptr returns false") {
    uint8_t bit = 0U;
    ASSERT_FALSE(ModbusFrameBuilder::extractBit(nullptr, 6, bit));
}

TEST_CASE("FrameBuilder: extractBit with too-short frame returns false") {
    const uint8_t short_frame[5] = {};
    uint8_t bit = 0U;
    // 6바이트 미만이어야 false
    ASSERT_FALSE(ModbusFrameBuilder::extractBit(short_frame, 5, bit));
}
