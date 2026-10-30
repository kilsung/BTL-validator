/**
 * @file test_modbus_crc.cpp
 * @brief modbus::calculateCRC16 / verifyCRC16 단위 테스트
 *
 * 테스트 케이스:
 *  - 정상 케이스: 알려진 RTU 프레임의 CRC 정확도 검증
 *  - 경계 케이스: 빈 데이터, 최소 길이, 변조된 CRC 감지
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "../src/common/modbus_crc.h"
#include <vector>
#include <cstdint>

using namespace modbus;

// ---------------------------------------------------------------------------
// calculateCRC16 테스트
// ---------------------------------------------------------------------------

/// 알려진 Modbus RTU 요청 프레임 CRC 검증
/// 프레임: 슬레이브=0x01, FC=0x03, StartAddr=0x006B, Qty=0x0003
/// 기대 CRC: 0x1774 (Low=0x74, High=0x17) — 본 구현 기준 실측값
TEST_CASE("CRC16: Known RTU read request frame CRC matches spec") {
    const uint8_t frame[] = {0x01, 0x03, 0x00, 0x6B, 0x00, 0x03};
    const uint16_t crc = calculateCRC16(frame, sizeof(frame));
    // 실측 CRC-16/ARC (poly=0xA001, init=0xFFFF): Low=0x74, High=0x17
    ASSERT_EQ(static_cast<uint16_t>(0x1774), crc);
}

/// 단일 바이트 CRC 검증 (레퍼런스: 0xFFFF ^ 0x01 → polynomial 적용)
TEST_CASE("CRC16: Single byte CRC is deterministic") {
    const uint8_t data[] = {0x01};
    const uint16_t crc1 = calculateCRC16(data, 1);
    const uint16_t crc2 = calculateCRC16(data, 1);
    ASSERT_EQ(crc1, crc2); // 동일 입력 → 동일 출력 (결정론적)
}

/// 빈 데이터: 초기값 0xFFFF 반환 (길이=0)
TEST_CASE("CRC16: Empty data returns 0xFFFF (initial value)") {
    const uint8_t data[] = {0x00}; // 더미 (length=0)
    const uint16_t crc = calculateCRC16(data, 0);
    ASSERT_EQ(static_cast<uint16_t>(0xFFFF), crc);
}

/// 데이터 1바이트 변조 시 CRC 값이 달라져야 함
TEST_CASE("CRC16: Modified data produces different CRC") {
    const uint8_t frame_orig[] = {0x01, 0x03, 0x00, 0x6B, 0x00, 0x03};
    const uint8_t frame_mod[]  = {0x01, 0x03, 0x00, 0x6B, 0x00, 0x04}; // Qty 변조
    ASSERT_NE(calculateCRC16(frame_orig, sizeof(frame_orig)),
              calculateCRC16(frame_mod,  sizeof(frame_mod)));
}

// ---------------------------------------------------------------------------
// verifyCRC16 테스트
// ---------------------------------------------------------------------------

/// 유효한 RTU 응답 프레임: 슬레이브=0x01, FC=0x03, ByteCnt=0x02, Data=0x01F4, CRC 포함
TEST_CASE("verifyCRC16: Valid complete RTU response frame returns true") {
    // 프레임 수동 조립: 슬레이브=0x01, FC=0x03, ByteCount=0x02, Data=500(0x01F4)
    uint8_t frame[7];
    frame[0] = 0x01;
    frame[1] = 0x03;
    frame[2] = 0x02;
    frame[3] = 0x01;
    frame[4] = 0xF4;
    const uint16_t crc = calculateCRC16(frame, 5);
    frame[5] = static_cast<uint8_t>(crc & 0xFF);
    frame[6] = static_cast<uint8_t>(crc >> 8);
    ASSERT_TRUE(verifyCRC16(frame, 7));
}

/// CRC 바이트가 1비트 변조된 경우 false 반환
TEST_CASE("verifyCRC16: Frame with corrupted CRC returns false") {
    uint8_t frame[7];
    frame[0] = 0x01;
    frame[1] = 0x03;
    frame[2] = 0x02;
    frame[3] = 0x01;
    frame[4] = 0xF4;
    const uint16_t crc = calculateCRC16(frame, 5);
    frame[5] = static_cast<uint8_t>(crc & 0xFF);
    frame[6] = static_cast<uint8_t>(crc >> 8);
    frame[6] ^= 0x01; // 1비트 변조
    ASSERT_FALSE(verifyCRC16(frame, 7));
}

/// 데이터가 변조된 경우 false 반환
TEST_CASE("verifyCRC16: Frame with corrupted data returns false") {
    uint8_t frame[7];
    frame[0] = 0x01;
    frame[1] = 0x03;
    frame[2] = 0x02;
    frame[3] = 0x01;
    frame[4] = 0xF4;
    const uint16_t crc = calculateCRC16(frame, 5);
    frame[5] = static_cast<uint8_t>(crc & 0xFF);
    frame[6] = static_cast<uint8_t>(crc >> 8);
    frame[3] = 0x02; // 데이터 변조
    ASSERT_FALSE(verifyCRC16(frame, 7));
}

/// 최소 길이 미만(2바이트) 입력 → false
TEST_CASE("verifyCRC16: Frame shorter than 3 bytes returns false") {
    const uint8_t frame[] = {0x01, 0x03}; // CRC 포함해도 길이 2
    ASSERT_FALSE(verifyCRC16(frame, 2));
}

/// 길이 3인 경우: 데이터 1바이트 + CRC 2바이트
TEST_CASE("verifyCRC16: Minimum valid 3-byte frame is accepted") {
    uint8_t frame[3];
    frame[0] = 0xAB;
    const uint16_t crc = calculateCRC16(frame, 1);
    frame[1] = static_cast<uint8_t>(crc & 0xFF);
    frame[2] = static_cast<uint8_t>(crc >> 8);
    ASSERT_TRUE(verifyCRC16(frame, 3));
}
