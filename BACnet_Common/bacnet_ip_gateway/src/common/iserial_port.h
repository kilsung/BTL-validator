#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <mutex>

namespace modbus {

/**
 * @brief Abstract interface for serial port operations.
 *
 * Decouples Modbus logic from concrete hardware implementation,
 * enabling dependency injection and unit-testing via mocks.
 * All implementations must be thread-safe for the methods they expose.
 */
class ISerialPort {
public:
    virtual ~ISerialPort() = default;

    /**
     * @brief Returns a reference to the mutex protecting this port.
     *
     * Used to synchronize multi-step transactions (write + read) on a shared port.
     */
    virtual std::mutex& getMutex() = 0;

    /**
     * @brief Opens the serial port with the specified parameters.
     *
     * @param device     Device path (e.g., "/dev/ttyS1").
     * @param baudrate   Baud rate (e.g., 19200).
     * @param parity     Parity setting ("none", "even", "odd").
     * @param stop_bits  Number of stop bits (1 or 2).
     * @param data_bits  Number of data bits (5–8).
     * @return true if the port was opened successfully.
     */
    virtual bool open(const std::string& device, int baudrate = 19200,
                      const std::string& parity = "none",
                      int stop_bits = 1, int data_bits = 8) = 0;

    /**
     * @brief Closes the port and releases all associated resources.
     */
    virtual void close() = 0;

    /**
     * @brief Checks whether the port is currently open.
     */
    virtual bool isOpen() const = 0;

    /**
     * @brief Writes raw bytes to the port.
     *
     * @param buffer  Pointer to data to transmit.
     * @param length  Number of bytes to transmit.
     * @return Number of bytes actually written, or -1 on error.
     */
    virtual int writeBytes(const uint8_t* buffer, size_t length) = 0;

    /**
     * @brief Drains the local TX echo from the receive buffer.
     *
     * On RS-485 half-duplex buses, the bytes transmitted by this node
     * are looped back into its own RX buffer.  Call this after every
     * writeBytes() to discard exactly @p echo_len echo bytes so the
     * next readFrame() sees only genuine slave/master traffic.
     *
     * On PTY (virtual) ports this is a no-op because the OS does not
     * generate such an echo.
     *
     * @param echo_len  Number of TX bytes to drain from the RX buffer.
     */
    virtual void drainTxEcho(size_t echo_len) = 0;

    /**
     * @brief Reads a single Modbus RTU frame.
     *
     * Blocks until the first byte arrives (within timeout_ms) then
     * continues reading until a 10 ms inter-character silence is
     * detected, which marks the Modbus RTU frame boundary.
     *
     * @param buffer      Output buffer.
     * @param max_length  Maximum bytes to read.
     * @param timeout_ms  Milliseconds to wait for the first byte.
     * @return Bytes read (>0), 0 on timeout, -1 on error.
     */
    virtual int readFrame(uint8_t* buffer, size_t max_length, int timeout_ms) = 0;

    /**
     * @brief Discards all pending data in the receive buffer.
     */
    virtual void flushInput() = 0;

    /**
     * @brief Returns true when the port is a physical RS-485/RS-232 device
     *        (not a PTY), meaning TX echo suppression is needed.
     */
    virtual bool isHardwarePort() const = 0;

    /**
     * @brief Returns a human-readable description of the port.
     */
    virtual std::string getDeviceName() const = 0;

    /**
     * @brief Configures whether this port is running as a Modbus Master or Slave.
     *        This helps readFrame optimize expected length parsing.
     */
    virtual void setMasterRole(bool isMaster) { (void)isMaster; }
    virtual bool isMasterRole() const { return false; }

    /**
     * @brief Proactively checks whether the port is healthy (handles POLLHUP/errors).
     * @return true if healthy, false if broken/closed.
     */
    virtual bool checkHealth() { return isOpen(); }
};

} // namespace modbus
