#include "serial_port.h"
#include "modbus_crc.h"

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <cstring>
#include <iostream>
#include <cerrno>
#include <chrono>

namespace modbus {

SerialPort::SerialPort() : fd_(-1), deviceName_(""), isPtyMaster_(false), isMasterRole_(false) {}

SerialPort::~SerialPort() {
    closePort();
}

// ISerialPort interface wrappers
bool SerialPort::open(const std::string& device, int baudrate,
                      const std::string& parity, int stop_bits, int data_bits) {
    return openSerial(device, baudrate, parity, stop_bits, data_bits);
}

void SerialPort::close() {
    closePort();
}

bool SerialPort::openSerial(const std::string& device, int baudrate, const std::string& parity, int stop_bits, int data_bits) {
    closePort();

    // Open read/write, no controlling tty, non-blocking initial open
    fd_ = ::open(device.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ < 0) {
        if (errno != ENOENT && errno != ENODEV) {
            std::cerr << "[SerialPort] Failed to open device: " << device 
                      << " (Error: " << strerror(errno) << ")" << std::endl;
        }
        return false;
    }

    // Clear O_NDELAY to make read block/non-block based on termios/select
    fcntl(fd_, F_SETFL, 0);

    deviceName_ = device;
    isPtyMaster_ = false;

    if (!configureTermios(baudrate, parity, stop_bits, data_bits)) {
        std::cerr << "[SerialPort] Warning: termios configuration failed for " << device 
                  << " (might be a virtual port or PTY, proceeding anyway)" << std::endl;
    }

    return true;
}

bool SerialPort::openPTY(std::string& slavePath) {
    closePort();

    fd_ = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd_ < 0) {
        std::cerr << "[SerialPort] posix_openpt failed (Error: " << strerror(errno) << ")" << std::endl;
        return false;
    }

    if (grantpt(fd_) < 0) {
        std::cerr << "[SerialPort] grantpt failed (Error: " << strerror(errno) << ")" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (unlockpt(fd_) < 0) {
        std::cerr << "[SerialPort] unlockpt failed (Error: " << strerror(errno) << ")" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    char* ptsNameStr = ptsname(fd_);
    if (!ptsNameStr) {
        std::cerr << "[SerialPort] ptsname failed" << std::endl;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    slavePath = ptsNameStr;
    deviceName_ = "PTY_MASTER:" + slavePath;
    isPtyMaster_ = true;

    // Set non-blocking/raw mode on master side if possible (usually standard termios handles it)
    struct termios options;
    if (tcgetattr(fd_, &options) == 0) {
        cfmakeraw(&options);
        tcsetattr(fd_, TCSANOW, &options);
    }

    return true;
}

void SerialPort::closePort() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    deviceName_ = "";
    isPtyMaster_ = false;
}

bool SerialPort::isOpen() const {
    std::lock_guard<std::mutex> lock(port_mutex_);
    return fd_ >= 0;
}

bool SerialPort::checkHealth() {
    std::lock_guard<std::mutex> lock(port_mutex_);
    if (fd_ < 0) {
        return false;
    }

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 0); // Non-blocking check
    if (ret > 0) {
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            std::cerr << "[SerialPort] Port hangup/disconnection detected via checkHealth() on "
                      << deviceName_ << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
            return false;
        }
    } else if (ret < 0) {
        if (errno == EBADF || errno == EIO || errno == ENODEV) {
            std::cerr << "[SerialPort] Port error detected via checkHealth() on "
                      << deviceName_ << " (errno: " << strerror(errno) << ")" << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
            return false;
        }
    }
    return true;
}

int SerialPort::writeBytes(const uint8_t* buffer, size_t length) {
    std::lock_guard<std::mutex> lock(port_mutex_);
    if (fd_ < 0) return -1;
    
    ssize_t written = ::write(fd_, buffer, length);
    if (written < 0) {
        if (errno == EIO || errno == EBADF || errno == ENODEV || errno == EPIPE) {
            std::cerr << "[SerialPort] Write failed due to broken/hangup port (" 
                      << strerror(errno) << "), closing " << deviceName_ << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
        } else if (errno != ENOTTY) {
            std::cerr << "[SerialPort] Write error (Error: " << strerror(errno) << ")" << std::endl;
        }
        return -1;
    }
    return static_cast<int>(written);
}

void SerialPort::drainTxEcho(size_t echo_len) {
    std::lock_guard<std::mutex> lock(port_mutex_);
    // PTY ports do not echo TX back to RX – nothing to drain.
    if (isPtyMaster_ || fd_ < 0 || echo_len == 0) return;

    // Wait for the UART hardware to finish transmitting before we try to
    // read the echo bytes.
    tcdrain(fd_);

    uint8_t discard[256];
    size_t drained = 0;
    struct pollfd pfd;
    pfd.fd     = fd_;
    pfd.events = POLLIN;

    bool first_byte = true;
    while (drained < echo_len) {
        int timeout_ms = first_byte ? 60 : 30;
        int pr = poll(&pfd, 1, timeout_ms);
        if (pr <= 0) break;   // timeout or error – stop draining

        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
            break;
        }

        size_t want = echo_len - drained;
        if (want > sizeof(discard)) want = sizeof(discard);

        ssize_t n = ::read(fd_, discard, want);
        if (n <= 0) {
            if (n == 0 || (n < 0 && (errno == EIO || errno == EBADF || errno == ENODEV))) {
                if (fd_ >= 0) {
                    ::close(fd_);
                    fd_ = -1;
                }
            }
            break;
        }
        drained    += static_cast<size_t>(n);
        first_byte  = false;
    }
}

int SerialPort::readFrame(uint8_t* buffer, size_t max_length, int timeout_ms) {
    std::lock_guard<std::mutex> lock(port_mutex_);
    if (fd_ < 0) return -1;

    struct pollfd pfd;
    pfd.fd = fd_;
    pfd.events = POLLIN;

    // 1. Wait for the first byte
    int pollResult = poll(&pfd, 1, timeout_ms);
    if (pollResult < 0) {
        if (errno == EINTR) return 0;
        if (errno == EIO || errno == EBADF || errno == ENODEV) {
            std::cerr << "[SerialPort] Poll failed due to broken/hangup port ("
                      << strerror(errno) << "), closing " << deviceName_ << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
        } else if (errno != ENOTTY) {
            std::cerr << "[SerialPort] Poll error on first byte (Error: " << strerror(errno) << ")" << std::endl;
        }
        return -1;
    } else if (pollResult == 0) {
        return 0; // Timeout, no data
    }

    if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        std::cerr << "[SerialPort] POLLHUP/POLLERR detected on " << deviceName_
                  << ", closing port." << std::endl;
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        return -1;
    }

    size_t totalRead = 0;
    auto start_time = std::chrono::steady_clock::now();
    const int max_frame_timeout_ms = 150; // Maximum duration for receiving the entire frame
    size_t expected_length = 0;

    // 2. Loop to read the entire frame
    while (totalRead < max_length) {
        size_t want = 1;
        if (totalRead < 3) {
            want = 3 - totalRead;
        } else if (expected_length > 0) {
            want = expected_length - totalRead;
            // For FC 06 (Write Single Register), initially read up to 6 bytes to check if it's a 6-byte response
            if (expected_length == 8 && buffer[1] == 0x06 && totalRead < 6) {
                want = 6 - totalRead;
            }
        }

        ssize_t bytesRead = ::read(fd_, buffer + totalRead, want);
        if (bytesRead < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                bytesRead = 0; // No data available right now
            } else {
                if (errno == EIO || errno == EBADF || errno == ENODEV || errno == EPIPE) {
                    std::cerr << "[SerialPort] Read failed due to broken/hangup port ("
                              << strerror(errno) << "), closing " << deviceName_ << std::endl;
                    if (fd_ >= 0) {
                        ::close(fd_);
                        fd_ = -1;
                    }
                } else if (errno != ENOTTY) {
                    std::cerr << "[SerialPort] Read error (Error: " << strerror(errno) << ")" << std::endl;
                }
                return -1;
            }
        } else if (bytesRead == 0) {
            // EOF on PTY slave (Master Closed)
            std::cerr << "[SerialPort] EOF (POLLHUP/Master Closed) on " << deviceName_
                      << ", closing port." << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
            return -1;
        }
        totalRead += bytesRead;

        // Determine the expected length of the frame based on Modbus RTU specification
        if (expected_length == 0 && totalRead >= 3) {
            uint8_t fc = buffer[1];
            if (fc & 0x80u) {
                expected_length = 5; // Exception response
            } else if (fc == 0x01 || fc == 0x02 || fc == 0x03 || fc == 0x04) {
                if (isMasterRole_) {
                    uint8_t byte_cnt = buffer[2];
                    expected_length = 3 + static_cast<size_t>(byte_cnt) + 2;
                } else {
                    if (buffer[2] <= 125) {
                        uint8_t byte_cnt = buffer[2];
                        expected_length = 3 + static_cast<size_t>(byte_cnt) + 2;
                    } else {
                        expected_length = 8;
                    }
                }
            } else if (fc == 0x05) {
                expected_length = 8; // Write Single Coil request and response are both 8 bytes
            } else if (fc == 0x06) {
                expected_length = 8; // Write Single Register (default)
            } else if (fc == 0x0F || fc == 0x10) {
                if (isMasterRole_) {
                    expected_length = 8; // Response is 8 bytes
                } else {
                    if (totalRead >= 7) {
                        uint8_t byte_cnt = buffer[6];
                        expected_length = 7 + static_cast<size_t>(byte_cnt) + 2;
                    } else {
                        expected_length = 0; // Need more bytes to decide
                    }
                }
            }
        }

        // For FC 06, verify if 6-byte packet CRC matches
        if (expected_length == 8 && buffer[1] == 0x06 && totalRead >= 6) {
            if (verifyCRC16(buffer, 6)) {
                expected_length = 6;
            }
        }

        // If we reached the expected length, return immediately
        if (expected_length > 0 && totalRead >= expected_length) {
            break;
        }

        // Prevent infinite looping on corrupted/partial frames
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count();
        if (elapsed > max_frame_timeout_ms) {
            break;
        }

        // Wait for the next byte with a 25ms inter-character gap timeout
        int subPollResult = poll(&pfd, 1, 25);
        if (subPollResult < 0) {
            if (errno == EINTR) continue;
            if (errno == EIO || errno == EBADF || errno == ENODEV) {
                if (fd_ >= 0) {
                    ::close(fd_);
                    fd_ = -1;
                }
            }
            return -1;
        } else if (subPollResult == 0) {
            break;
        }
        if (pfd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
            std::cerr << "[SerialPort] POLLHUP/POLLERR detected mid-frame on " << deviceName_
                      << ", closing port." << std::endl;
            if (fd_ >= 0) {
                ::close(fd_);
                fd_ = -1;
            }
            return -1;
        }
    }

    return static_cast<int>(totalRead);
}

void SerialPort::flushInput() {
    std::lock_guard<std::mutex> lock(port_mutex_);
    if (fd_ >= 0) {
        tcflush(fd_, TCIFLUSH);
    }
}

bool SerialPort::configureTermios(int baudrate, const std::string& parity, int stop_bits, int data_bits) {
    struct termios options;
    if (tcgetattr(fd_, &options) != 0) {
        return false;
    }

    // Configure Parity
    if (parity == "even") {
        options.c_cflag |= PARENB;
        options.c_cflag &= ~PARODD;
    } else if (parity == "odd") {
        options.c_cflag |= PARENB;
        options.c_cflag |= PARODD;
    } else {
        options.c_cflag &= ~PARENB;        // No Parity
    }

    // Configure Stop Bits
    if (stop_bits == 2) {
        options.c_cflag |= CSTOPB;        // 2 Stop Bits
    } else {
        options.c_cflag &= ~CSTOPB;        // 1 Stop Bit
    }

    options.c_cflag &= ~CSIZE;         // Mask character size
    if (data_bits == 5) {
        options.c_cflag |= CS5;
    } else if (data_bits == 6) {
        options.c_cflag |= CS6;
    } else if (data_bits == 7) {
        options.c_cflag |= CS7;
    } else {
        options.c_cflag |= CS8;            // Default to 8 Data Bits
    }
    options.c_cflag &= ~CRTSCTS;       // Disable RTS/CTS flow control
    options.c_cflag |= (CLOCAL | CREAD); // Enable receiver, local line

    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    options.c_iflag &= ~(IXON | IXOFF | IXANY);         // Disable software flow control
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    options.c_oflag &= ~OPOST;                          // Raw output

    // Blocking behavior: return immediately if data is available,
    // readFrame handles timeout via poll()
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;

    // Set baudrate
    speed_t speed;
    switch (baudrate) {
        case 4800:   speed = B4800;   break;
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:
            std::cerr << "[SerialPort] Unsupported baudrate " << baudrate << ", defaulting to 19200" << std::endl;
            speed = B19200;
            break;
    }

    if (cfsetispeed(&options, speed) != 0 || cfsetospeed(&options, speed) != 0) {
        return false;
    }

    if (tcsetattr(fd_, TCSANOW, &options) != 0) {
        return false;
    }

    return true;
}

} // namespace modbus
