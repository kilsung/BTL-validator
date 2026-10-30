#pragma once

#include "modbus_crc.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>

namespace modbus {

// ---------------------------------------------------------------------------
// Modbus RTU 프로토콜 상수 (Magic Number 제거)
// ---------------------------------------------------------------------------

/// RTU 프레임 최소 길이: [Addr(1)] [FC(1)] [Data(1+)] [CRC(2)]
static constexpr size_t kModbusMinFrameLen      = 5U;
/// 표준 Modbus RTU 8바이트 요청 프레임 길이
static constexpr size_t kModbusRequestFrameLen  = 8U;
/// FC16 Float32 요청 프레임 길이
static constexpr size_t kModbusFloat32FrameLen  = 13U;

/// Modbus Function Codes
static constexpr uint8_t kFcReadCoils          = 0x01U;
static constexpr uint8_t kFcReadDiscreteInputs = 0x02U;
static constexpr uint8_t kFcReadHoldingRegs    = 0x03U;
static constexpr uint8_t kFcReadInputRegs      = 0x04U;
static constexpr uint8_t kFcWriteSingleCoil    = 0x05U;
static constexpr uint8_t kFcWriteSingleReg     = 0x06U;
static constexpr uint8_t kFcWriteMultipleRegs  = 0x10U;

/// Write Single Coil ON/OFF 값
static constexpr uint16_t kCoilOn  = 0xFF00U;
static constexpr uint16_t kCoilOff = 0x0000U;

/// 수신/송신 버퍼 최대 크기
static constexpr size_t kRxBufMaxLen = 256U;
static constexpr size_t kTxBufMaxLen = 256U;

/// 응답 프레임 데이터 시작 오프셋 [Addr][FC][ByteCnt][Data...]
static constexpr size_t kRspDataStartIdx = 3U;

// ---------------------------------------------------------------------------
// ModbusFrameBuilder 클래스
// ---------------------------------------------------------------------------

/**
 * @brief Modbus RTU 요청 프레임을 조립하고 응답을 해석하는 유틸리티 클래스.
 *
 * 설계 결정:
 *  - 모든 메서드를 static으로 선언하여 상태 없는 순수 기능 모듈로 구성합니다.
 *  - 버퍼 포인터와 크기를 명시적으로 전달하여 Buffer Overflow 위험을 제거합니다.
 *  - verifyCRC16 / calculateCRC16 은 modbus_crc.h 를 재사용합니다.
 */
class ModbusFrameBuilder {
public:
    // 상태 없는 유틸리티 클래스 — 인스턴스화 금지
    ModbusFrameBuilder()  = delete;
    ~ModbusFrameBuilder() = delete;

    // -----------------------------------------------------------------------
    // 프레임 조립 (TX)
    // -----------------------------------------------------------------------

    /**
     * @brief 단일 레지스터/코일 읽기 요청 프레임(FC01~FC04)을 조립합니다.
     *
     * 보안 개선: 출력 버퍼 크기를 매개변수로 받아 오버플로를 방지합니다.
     *
     * @param[out] buf       송신 버퍼 (최소 kModbusRequestFrameLen 바이트)
     * @param      buf_size  송신 버퍼 크기
     * @param      unit_id   Modbus 슬레이브 주소 (1–247)
     * @param      fc        Function Code
     * @param      reg_addr  0-based 레지스터 주소
     * @param      quantity  읽을 레지스터/코일 수
     * @return 조립된 바이트 수, 버퍼 부족 시 0
     */
    static size_t buildReadRequest(uint8_t*  buf,
                                   size_t    buf_size,
                                   uint8_t   unit_id,
                                   uint8_t   fc,
                                   uint16_t  reg_addr,
                                   uint16_t  quantity)
    {
        if (buf == nullptr || buf_size < kModbusRequestFrameLen) {
            return 0U;
        }
        buf[0] = unit_id;
        buf[1] = fc;
        buf[2] = static_cast<uint8_t>(reg_addr >> 8U);
        buf[3] = static_cast<uint8_t>(reg_addr & 0xFFU);
        buf[4] = static_cast<uint8_t>(quantity >> 8U);
        buf[5] = static_cast<uint8_t>(quantity & 0xFFU);
        const uint16_t crc = calculateCRC16(buf, 6U);
        buf[6] = static_cast<uint8_t>(crc & 0xFFU);   // CRC Low byte first (Modbus spec)
        buf[7] = static_cast<uint8_t>(crc >> 8U);
        return kModbusRequestFrameLen;
    }

    /**
     * @brief Write Single Coil (FC 0x05) 요청 프레임을 조립합니다.
     */
    static size_t buildWriteCoilRequest(uint8_t* buf,
                                        size_t   buf_size,
                                        uint8_t  unit_id,
                                        uint16_t reg_addr,
                                        bool     on_off)
    {
        if (buf == nullptr || buf_size < kModbusRequestFrameLen) {
            return 0U;
        }
        const uint16_t coil_val = on_off ? kCoilOn : kCoilOff;
        buf[0] = unit_id;
        buf[1] = kFcWriteSingleCoil;
        buf[2] = static_cast<uint8_t>(reg_addr >> 8U);
        buf[3] = static_cast<uint8_t>(reg_addr & 0xFFU);
        buf[4] = static_cast<uint8_t>(coil_val >> 8U);
        buf[5] = static_cast<uint8_t>(coil_val & 0xFFU);
        const uint16_t crc = calculateCRC16(buf, 6U);
        buf[6] = static_cast<uint8_t>(crc & 0xFFU);
        buf[7] = static_cast<uint8_t>(crc >> 8U);
        return kModbusRequestFrameLen;
    }

    /**
     * @brief Write Single Register (FC 0x06) 요청 프레임을 조립합니다.
     */
    static size_t buildWriteSingleRegRequest(uint8_t*  buf,
                                             size_t    buf_size,
                                             uint8_t   unit_id,
                                             uint16_t  reg_addr,
                                             uint16_t  reg_value)
    {
        if (buf == nullptr || buf_size < kModbusRequestFrameLen) {
            return 0U;
        }
        buf[0] = unit_id;
        buf[1] = kFcWriteSingleReg;
        buf[2] = static_cast<uint8_t>(reg_addr >> 8U);
        buf[3] = static_cast<uint8_t>(reg_addr & 0xFFU);
        buf[4] = static_cast<uint8_t>(reg_value >> 8U);
        buf[5] = static_cast<uint8_t>(reg_value & 0xFFU);
        const uint16_t crc = calculateCRC16(buf, 6U);
        buf[6] = static_cast<uint8_t>(crc & 0xFFU);
        buf[7] = static_cast<uint8_t>(crc >> 8U);
        return kModbusRequestFrameLen;
    }

    /**
     * @brief Write Multiple Registers (FC 0x10) 으로 32-bit float 를 기록합니다.
     *
     * 안전한 type-pun: std::memcpy 를 사용하여 UB 없이 bit-cast 합니다.
     *
     * @param word_swap false = Big-Endian/ABCD (High word first, 기본값)
     *                  true  = Word-Swapped/CDAB (Low word first)
     */
    static size_t buildWriteFloat32Request(uint8_t* buf,
                                           size_t   buf_size,
                                           uint8_t  unit_id,
                                           uint16_t reg_addr,
                                           float    value,
                                           bool     word_swap = false)
    {
        if (buf == nullptr || buf_size < kModbusFloat32FrameLen) {
            return 0U;
        }
        uint32_t raw = 0U;
        std::memcpy(&raw, &value, sizeof(raw));  // 안전한 type-pun (UB 없음)
        buf[0]  = unit_id;
        buf[1]  = kFcWriteMultipleRegs;
        buf[2]  = static_cast<uint8_t>(reg_addr >> 8U);
        buf[3]  = static_cast<uint8_t>(reg_addr & 0xFFU);
        buf[4]  = 0x00U;
        buf[5]  = 0x02U;  // Quantity: 2 registers
        buf[6]  = 0x04U;  // Byte count: 4
        if (!word_swap) {
            // ABCD: Big-Endian, High Word First (표준)
            buf[7]  = static_cast<uint8_t>(raw >> 24U);
            buf[8]  = static_cast<uint8_t>(raw >> 16U);
            buf[9]  = static_cast<uint8_t>(raw >>  8U);
            buf[10] = static_cast<uint8_t>(raw & 0xFFU);
        } else {
            // CDAB: Low Word First (Word Swapped)
            buf[7]  = static_cast<uint8_t>(raw >>  8U);
            buf[8]  = static_cast<uint8_t>(raw & 0xFFU);
            buf[9]  = static_cast<uint8_t>(raw >> 24U);
            buf[10] = static_cast<uint8_t>(raw >> 16U);
        }
        const uint16_t crc = calculateCRC16(buf, 11U);
        buf[11] = static_cast<uint8_t>(crc & 0xFFU);
        buf[12] = static_cast<uint8_t>(crc >> 8U);
        return kModbusFloat32FrameLen;
    }

    // -----------------------------------------------------------------------
    // 응답 해석 (RX)
    // -----------------------------------------------------------------------

    /**
     * @brief 응답 프레임의 기본 유효성을 검사합니다.
     *
     * 검사 항목:
     *  1. nullptr 및 길이 체크
     *  2. CRC 일치 여부
     *  3. 슬레이브 주소 일치 여부
     *  4. Modbus 예외 응답(FC | 0x80) 감지
     *  5. Function Code 일치 여부
     */
    static bool validateResponse(const uint8_t* rx_buf,
                                 int            rx_len,
                                 uint8_t        unit_id,
                                 uint8_t        exp_fc)
    {
        if (rx_buf == nullptr || rx_len <= 0) {
            return false;
        }
        const size_t len = static_cast<size_t>(rx_len);
        if (len < kModbusMinFrameLen) {
            return false;
        }
        if (!verifyCRC16(rx_buf, len)) {
            return false;
        }
        if (rx_buf[0] != unit_id) {
            return false;
        }
        // Modbus 예외 응답: FC | 0x80
        if ((rx_buf[1] & 0x80U) != 0U) {
            return false;
        }
        if (rx_buf[1] != exp_fc) {
            return false;
        }
        return true;
    }

    /**
     * @brief 읽기 응답에서 단일 16-bit 레지스터 값을 추출합니다.
     */
    static bool extractRegister16(const uint8_t* rx_buf,
                                   int            rx_len,
                                   uint16_t&      out_val)
    {
        // [Addr][FC][ByteCnt][DataH][DataL][CRCL][CRCH] = 최소 7바이트
        if (rx_buf == nullptr || rx_len < 7) {
            return false;
        }
        out_val = static_cast<uint16_t>(
            (static_cast<uint16_t>(rx_buf[kRspDataStartIdx]) << 8U) |
             static_cast<uint16_t>(rx_buf[kRspDataStartIdx + 1U])
        );
        return true;
    }

    /**
     * @brief 읽기 응답에서 32-bit float 값을 추출합니다.
     *
     * @param word_swap false = ABCD: Big-Endian, High Word First (기본값)
     *                  true  = CDAB: Low Word First (Word Swapped)
     *
     * 바이트 배치:
     *  ABCD (no swap): D0=MSB...D3=LSB  →  raw = [D0<<24 | D1<<16 | D2<<8 | D3]
     *  CDAB (swap)   : D0=Word1_H D1=Word1_L D2=Word0_H D3=Word0_L
     *                  →  raw = [D2<<24 | D3<<16 | D0<<8 | D1]
     */
    static bool extractFloat32(const uint8_t* rx_buf,
                                int            rx_len,
                                float&         out_val,
                                bool           word_swap = false)
    {
        // [Addr][FC][ByteCnt(4)][D0][D1][D2][D3][CRCL][CRCH] = 최소 9바이트
        if (rx_buf == nullptr || rx_len < 9) {
            return false;
        }
        uint32_t raw = 0U;
        if (!word_swap) {
            // ABCD: Big-Endian, High Word First (표준)
            raw = (static_cast<uint32_t>(rx_buf[3]) << 24U) |
                  (static_cast<uint32_t>(rx_buf[4]) << 16U) |
                  (static_cast<uint32_t>(rx_buf[5]) <<  8U) |
                   static_cast<uint32_t>(rx_buf[6]);
        } else {
            // CDAB: Low Word First (Word Swapped)
            // 수신 순서: [Low_H][Low_L][High_H][High_L]
            raw = (static_cast<uint32_t>(rx_buf[5]) << 24U) |
                  (static_cast<uint32_t>(rx_buf[6]) << 16U) |
                  (static_cast<uint32_t>(rx_buf[3]) <<  8U) |
                   static_cast<uint32_t>(rx_buf[4]);
        }
        std::memcpy(&out_val, &raw, sizeof(out_val));  // 안전한 type-pun
        return true;
    }

    /**
     * @brief 읽기 응답에서 코일/이산 입력 비트 상태를 추출합니다.
     */
    static bool extractBit(const uint8_t* rx_buf,
                            int            rx_len,
                            uint8_t&       out_bit)
    {
        // [Addr][FC][ByteCnt][Data][CRCL][CRCH] = 최소 6바이트
        if (rx_buf == nullptr || rx_len < 6) {
            return false;
        }
        out_bit = rx_buf[kRspDataStartIdx] & 0x01U;
        return true;
    }
};

} // namespace modbus
