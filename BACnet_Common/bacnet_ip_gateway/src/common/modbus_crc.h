#pragma once

#include <cstdint>
#include <cstddef>

namespace modbus {

/**
 * @brief Calculates the standard Modbus RTU CRC-16 checksum.
 * 
 * Polynomial: 0xA001 (x16 + x15 + x2 + 1, reversed)
 * Initial value: 0xFFFF
 * 
 * @param data Pointer to the data buffer
 * @param length Length of the data in bytes
 * @return uint16_t The 16-bit CRC value
 */
uint16_t calculateCRC16(const uint8_t* data, size_t length);

/**
 * @brief Verifies if the data ends with a valid Modbus RTU CRC-16.
 * 
 * Assumes the last two bytes of the data buffer contain the CRC (Low byte first, High byte second).
 * 
 * @param data Pointer to the data buffer
 * @param length Total length of the buffer (including CRC bytes)
 * @return true If the CRC is valid
 * @return false If the CRC is invalid
 */
bool verifyCRC16(const uint8_t* data, size_t length);

} // namespace modbus
