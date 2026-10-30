#include "modbus_crc.h"

namespace modbus {

uint16_t calculateCRC16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool verifyCRC16(const uint8_t* data, size_t length) {
    if (length < 3) return false; // At least address, function, and CRC (or data)
    uint16_t computed = calculateCRC16(data, length - 2);
    uint16_t received = data[length - 2] | (static_cast<uint16_t>(data[length - 1]) << 8);
    return computed == received;
}

} // namespace modbus
