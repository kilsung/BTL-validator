#pragma once

#include "iserial_port.h"
#include <string>
#include <cstdint>
#include <cstddef>
#include <mutex>

namespace modbus {

/**
 * @brief Concrete POSIX serial port implementation of ISerialPort.
 *
 * Supports physical RS-232/RS-485 ports as well as pseudo-terminals
 * (PTY) for loopback testing without physical hardware.
 */
class SerialPort : public ISerialPort {
public:
    SerialPort();
    ~SerialPort() override;

    std::mutex& getMutex() override { return port_mutex_; }

    /**
     * @brief ISerialPort::open() implementation — delegates to openSerial().
     */
    bool open(const std::string& device, int baudrate = 19200,
              const std::string& parity = "none",
              int stop_bits = 1, int data_bits = 8) override;

    /**
     * @brief ISerialPort::close() implementation — delegates to closePort().
     */
    void close() override;

    /**
     * @brief Opens a physical serial port.
     * 
     * @param device Device path (e.g., "/dev/ttyS1" or "/dev/pts/3")
     * @param baudrate Baud rate (default 19200)
     * @param parity Parity configuration ("none", "even", "odd")
     * @param stop_bits Number of stop bits (1 or 2)
     * @param data_bits Number of data bits (5 to 8)
     * @return true if successful
     */
    bool openSerial(const std::string& device, int baudrate = 19200, const std::string& parity = "none", int stop_bits = 1, int data_bits = 8);

    /**
     * @brief Creates a pseudo-terminal (PTY) master.
     * 
     * The slave path (e.g. "/dev/pts/X") is returned in slavePath.
     * This is useful for testing without physical serial hardware.
     * 
     * @param slavePath Output parameter for the slave PTY device path
     * @return true if successful
     */
    bool openPTY(std::string& slavePath);

    /**
     * @brief Closes the serial port.
     */
    void closePort();

    /**
     * @brief Checks if the port is open.
     */
    bool isOpen() const override;

    /**
     * @brief Writes data to the serial port.
     *
     * @param buffer Data to write
     * @param length Length of data
     * @return int Number of bytes written, or -1 on error
     */
    int writeBytes(const uint8_t* buffer, size_t length) override;

    /**
     * @brief Drains exactly @p echo_len bytes of TX echo from the RX buffer.
     *
     * Waits up to one character-time per byte (derived from the baud rate)
     * then reads and discards the looped-back bytes.  On PTY ports this
     * is a no-op.
     *
     * @param echo_len  Number of transmitted bytes that will echo back.
     */
    void drainTxEcho(size_t echo_len) override;

    /**
     * @brief Reads a single Modbus RTU frame using timing constraints.
     *
     * Waits for the first byte up to timeout_ms.
     * Once bytes start arriving, continues reading until a silent period of
     * at least 10 ms is detected (Modbus RTU frame boundary).
     *
     * @param buffer      Output buffer
     * @param max_length  Maximum size of output buffer
     * @param timeout_ms  Milliseconds to wait for the first byte
     * @return int Number of bytes read, or -1 on error, or 0 on timeout
     */
    int readFrame(uint8_t* buffer, size_t max_length, int timeout_ms) override;

    /**
     * @brief Discards all data in the input (receive) buffer.
     */
    void flushInput() override;

    /**
     * @brief Returns true for physical hardware ports (RS-485/RS-232).
     *
     * Used internally to decide whether TX echo draining is needed.
     */
    bool isHardwarePort() const override { return !isPtyMaster_; }

    /**
     * @brief Returns the open device name or path.
     */
    std::string getDeviceName() const override { return deviceName_; }

    void setMasterRole(bool isMaster) override { isMasterRole_ = isMaster; }
    bool isMasterRole() const override { return isMasterRole_; }

    /**
     * @brief Checks whether the file descriptor is healthy and non-HUP.
     */
    bool checkHealth() override;

private:
    int fd_;
    std::string deviceName_;
    bool isPtyMaster_;
    bool isMasterRole_;
    mutable std::mutex port_mutex_;

    bool configureTermios(int baudrate, const std::string& parity, int stop_bits, int data_bits);
};

} // namespace modbus
