#include "vport.h"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <cerrno>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>

static std::string to_hex_string(const uint8_t* data, size_t len) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]) << " ";
    }
    return ss.str();
}

#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>

// ==========================================
// UniqueFd Implementation
// ==========================================

UniqueFd::UniqueFd() : fd_(-1) {}

UniqueFd::UniqueFd(int fd) : fd_(fd) {}

UniqueFd::~UniqueFd() {
    reset();
}

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
        reset();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

int UniqueFd::get() const {
    return fd_;
}

void UniqueFd::reset(int new_fd) {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = new_fd;
}

int UniqueFd::release() {
    int temp = fd_;
    fd_ = -1;
    return temp;
}

bool UniqueFd::is_valid() const {
    return fd_ >= 0;
}

// ==========================================
// PosixSerialPort Implementation
// ==========================================

PosixSerialPort::PosixSerialPort(UniqueFd fd) : fd_(std::move(fd)) {}

ssize_t PosixSerialPort::read_port(void* buf, size_t count) {
    return ::read(fd_.get(), buf, count);
}

ssize_t PosixSerialPort::write_port(const void* buf, size_t count) {
    return ::write(fd_.get(), buf, count);
}

int PosixSerialPort::get_fd() const {
    return fd_.get();
}

bool PosixSerialPort::is_valid() const {
    return fd_.is_valid();
}

// ==========================================
// Helper Functions Implementation
// ==========================================

UniqueFd create_pty_link(const std::string& link_path, UniqueFd* keeper_out) {
    int m_fd = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (m_fd < 0 || grantpt(m_fd) < 0 || unlockpt(m_fd) < 0) return UniqueFd();
    
    char pts_name_buf[256];
    if (ptsname_r(m_fd, pts_name_buf, sizeof(pts_name_buf)) != 0) {
        ::close(m_fd);
        return UniqueFd();
    }
    
    unlink(link_path.c_str());
    if (symlink(pts_name_buf, link_path.c_str()) < 0) {
        ::close(m_fd);
        return UniqueFd();
    }
    chmod(link_path.c_str(), 0666);

    // 슬레이브 참조를 하나 붙들어 둔다. 마지막 클라이언트가 포트를 닫아도 참조 수가
    // 0 이 되지 않으므로, 마스터 fd 의 read() 가 EIO 를 반환하며 select() 루프가
    // 100% CPU 로 공회전하는 것을 막는다. 이 fd 로는 절대 read 하지 않는다.
    if (keeper_out != nullptr) {
        int k_fd = open(pts_name_buf, O_RDWR | O_NOCTTY);
        if (k_fd >= 0) {
            // 클라이언트가 붙기 전에도 에코/정규모드 처리가 일어나지 않도록 한다.
            struct termios tio;
            if (tcgetattr(k_fd, &tio) == 0) {
                cfmakeraw(&tio);
                tcsetattr(k_fd, TCSANOW, &tio);
            }
            keeper_out->reset(k_fd);
        }
    }

    return UniqueFd(m_fd);
}

// ==========================================
// ModbusFramingParser Implementation
// ==========================================

uint16_t ModbusFramingParser::calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

size_t ModbusFramingParser::find_complete_frame(const std::vector<uint8_t>& buffer, bool is_request) {
    if (buffer.size() < 4) return 0;

    uint8_t fc = buffer[1];
    size_t expected_len = 0;

    if (is_request) {
        switch (fc) {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
            case 0x05:
            case 0x06:
                expected_len = 8;
                break;
            case 0x0F:
            case 0x10:
                if (buffer.size() >= 7) {
                    expected_len = 9 + buffer[6];
                } else {
                    return 0;
                }
                break;
            default:
                // 표준 Modbus FC만 지원. 알 수 없는 FC는 프레임으로 인식하지 않음.
                return 0;
        }
    } else {
        switch (fc) {
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x04:
                if (buffer.size() >= 3) {
                    expected_len = 5 + buffer[2];
                } else {
                    return 0;
                }
                break;
            case 0x05:
            case 0x0F:
            case 0x10:
                expected_len = 8;
                break;
            case 0x06:
                if (buffer.size() >= 6) {
                    uint16_t calculated = calculate_crc16(buffer.data(), 4);
                    uint16_t received = (buffer[5] << 8) | buffer[4];
                    if (calculated == received) {
                        expected_len = 6;
                        break;
                    }
                }
                expected_len = 8;
                break;
            default:
                // Modbus Exception Response: FC의 MSB가 set (예: 0x81, 0x82, ...)
                // 형식: [SlaveID][FC|0x80][ExCode][CRC_L][CRC_H] = 5바이트
                if ((fc & 0x80U) != 0) {
                    expected_len = 5;
                } else {
                    // 표준 FC가 아닌 비표준 응답은 프레임으로 인식하지 않음
                    return 0;
                }
                break;
        }
    }

    if (expected_len > 0) {
        if (buffer.size() < expected_len) {
            return 0;
        }
        uint16_t calculated = calculate_crc16(buffer.data(), expected_len - 2);
        uint16_t received = (buffer[expected_len - 1] << 8) | buffer[expected_len - 2];
        if (calculated == received) {
            return expected_len;
        }
        return 0;
    }

    return 0;
}

size_t ModbusFramingParser::find_next_complete_frame(
    const std::vector<uint8_t>& buffer, bool is_request, size_t& frame_offset) {
    frame_offset = 0;
    for (size_t offset = 0; offset + 4 <= buffer.size(); ++offset) {
        const std::vector<uint8_t> candidate(buffer.begin() + offset, buffer.end());
        const size_t frame_len = find_complete_frame(candidate, is_request);
        if (frame_len > 0) {
            frame_offset = offset;
            return frame_len;
        }
    }
    return 0;
}

bool ModbusFramingParser::response_matches_request(
    const std::vector<uint8_t>& request, const uint8_t* response, size_t response_len) {
    if (request.size() < 2 || response == nullptr || response_len < 2) {
        return false;
    }

    const uint8_t request_fc = request[1];
    const uint8_t response_fc = response[1];
    if (response[0] != request[0] ||
        (response_fc != request_fc && response_fc != static_cast<uint8_t>(request_fc | 0x80U))) {
        return false;
    }

    if (response_fc == static_cast<uint8_t>(request_fc | 0x80U)) {
        return response_len >= 5;
    }

    if ((request_fc == 0x01U || request_fc == 0x02U ||
         request_fc == 0x03U || request_fc == 0x04U) &&
        request.size() >= 6 && response_len >= 3) {
        const uint16_t quantity = static_cast<uint16_t>(
            (static_cast<uint16_t>(request[4]) << 8U) | request[5]);
        const size_t expected_bytes = (request_fc == 0x01U || request_fc == 0x02U)
            ? static_cast<size_t>((quantity + 7U) / 8U)
            : static_cast<size_t>(quantity) * 2U;
        return static_cast<size_t>(response[2]) == expected_bytes;
    }

    if ((request_fc == 0x05U || request_fc == 0x06U) &&
        request.size() >= 6 && response_len >= 4) {
        const bool address_matches = response[2] == request[2] && response[3] == request[3];
        const bool value_matches = response_len < 8 ||
                                   (response[4] == request[4] && response[5] == request[5]);
        return address_matches && value_matches;
    }

    if ((request_fc == 0x0FU || request_fc == 0x10U) &&
        request.size() >= 6 && response_len >= 6) {
        return response[2] == request[2] && response[3] == request[3] &&
               response[4] == request[4] && response[5] == request[5];
    }

    return true;
}

// ==========================================
// ModbusHub Implementation
// ==========================================

ModbusHub::ModbusHub(std::vector<std::shared_ptr<ISerialPort>> masters,
                     std::shared_ptr<ISerialPort> slave,
                     int slave_timeout_ms,
                     int max_queue_depth)
    : masters_(std::move(masters)), s_(std::move(slave)), running_(false),
      log_packets_(false), slave_timeout_ms_(slave_timeout_ms),
      max_queue_depth_(max_queue_depth) {
    master_buffers_.resize(masters_.size());

    const char* env_log = std::getenv("VPORT_LOG");
    log_packets_ = (env_log != nullptr && std::string(env_log) == "1");
}

ModbusHub::~ModbusHub() {
    stop();
}

void ModbusHub::start() {
    running_ = true;
    listener_thread_ = std::thread(&ModbusHub::listener_loop, this);
    dispatcher_thread_ = std::thread(&ModbusHub::dispatcher_loop, this);
}

void ModbusHub::stop() {
    if (running_) {
        running_ = false;
        queue_cv_.notify_all();
        if (listener_thread_.joinable()) listener_thread_.join();
        if (dispatcher_thread_.joinable()) dispatcher_thread_.join();
    }
}

void ModbusHub::queue_request(int master_index, const std::vector<uint8_t>& packet) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);

        // [Per-Master Serialization] 현재 슬레이브 응답 대기 중이거나 큐에 대기 중인 마스터의 추가 요청 검사
        bool master_busy = (active_master_index_.load() == master_index);
        if (!master_busy && !request_queue_.empty()) {
            std::queue<ModbusRequest> copy = request_queue_;
            while (!copy.empty()) {
                if (copy.front().origin_master_index == master_index) {
                    master_busy = true;
                    break;
                }
                copy.pop();
            }
        }

        if (master_busy) {
            std::cerr << "[Hub Warning] Master " << master_index + 1
                      << " 이미 진행 중인 요청 존재 - 추가 요청 거부 (Server Device Busy 0x06 즉시 회신)" << std::endl;
            if (packet.size() >= 2 &&
                master_index >= 0 &&
                master_index < static_cast<int>(masters_.size())) {
                uint8_t exc_resp[5];
                exc_resp[0] = packet[0];  // Slave ID
                exc_resp[1] = static_cast<uint8_t>(packet[1] | 0x80U);
                exc_resp[2] = 0x06U;  // Server Device Busy
                uint16_t crc = ModbusFramingParser::calculate_crc16(exc_resp, 3);
                exc_resp[3] = static_cast<uint8_t>(crc & 0xFFU);
                exc_resp[4] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
                masters_[master_index]->write_port(exc_resp, 5);
            }
            return;
        }

        if (max_queue_depth_ > 0 &&
            static_cast<int>(request_queue_.size()) >= max_queue_depth_) {
            std::cerr << "[Hub Warning] 요청 큐 상한(" << max_queue_depth_
                      << ") 초과 - Master " << master_index + 1
                      << " 요청 폐기" << std::endl;
            // Modbus Exception Response: Server Device Busy (FC | 0x80, code 0x06)
            if (packet.size() >= 2 &&
                master_index >= 0 &&
                master_index < static_cast<int>(masters_.size())) {
                uint8_t exc_resp[5];
                exc_resp[0] = packet[0];  // Slave ID
                exc_resp[1] = static_cast<uint8_t>(packet[1] | 0x80U);
                exc_resp[2] = 0x06U;  // Server Device Busy
                uint16_t crc = ModbusFramingParser::calculate_crc16(exc_resp, 3);
                exc_resp[3] = static_cast<uint8_t>(crc & 0xFFU);
                exc_resp[4] = static_cast<uint8_t>((crc >> 8) & 0xFFU);
                masters_[master_index]->write_port(exc_resp, 5);
            }
            return;
        }
        ModbusRequest req{master_index, packet};
        request_queue_.push(req);
    }
    queue_cv_.notify_one();
}

void ModbusHub::process_master_input(ISerialPort& port, int master_index, std::vector<uint8_t>& buffer) {
    if (buffer.size() > SAFE_GUARD_BUFFER_LIMIT) {
        buffer.clear();
        return;
    }
    uint8_t read_buf[MAX_BUFFER_SIZE];
    ssize_t bytes = port.read_port(read_buf, sizeof(read_buf));
    if (bytes > 0) {
        buffer.insert(buffer.end(), read_buf, read_buf + bytes);

        // Drop an oversized partial stream before attempting an O(n^2)
        // resynchronisation scan.  Besides bounding memory, this prevents a
        // hostile/noisy sender from keeping the parser busy indefinitely.
        if (buffer.size() > SAFE_GUARD_BUFFER_LIMIT) {
            buffer.clear();
            return;
        }
        
        while (true) {
            size_t frame_offset = 0;
            const size_t frame_len = ModbusFramingParser::find_next_complete_frame(
                buffer, true, frame_offset);
            if (frame_len > 0) {
                const auto frame_begin = buffer.begin() + frame_offset;
                std::vector<uint8_t> frame(frame_begin, frame_begin + frame_len);
                buffer.erase(buffer.begin(), frame_begin + frame_len);
                queue_request(master_index, frame);
            } else {
                if (buffer.size() > SAFE_GUARD_BUFFER_LIMIT) {
                    buffer.clear();
                }
                break;
            }
        }
    }
}

void ModbusHub::listener_loop() {
    int max_fd = -1;
    std::vector<int> master_fds;
    for (const auto& master : masters_) {
        int fd = master->get_fd();
        master_fds.push_back(fd);
        if (fd > max_fd) {
            max_fd = fd;
        }
    }

    while (running_) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        for (int fd : master_fds) {
            if (fd >= 0) {
                FD_SET(fd, &read_fds);
            }
        }

        struct timeval timeout = {0, 50000};
        int activity = ::select(max_fd + 1, &read_fds, nullptr, nullptr, &timeout);

        if (activity < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (activity > 0) {
            for (size_t i = 0; i < masters_.size(); ++i) {
                int fd = master_fds[i];
                if (fd >= 0 && FD_ISSET(fd, &read_fds)) {
                    process_master_input(*masters_[i], static_cast<int>(i), master_buffers_[i]);
                }
            }
        }
    }
}

void ModbusHub::dispatcher_loop() {
    uint8_t buffer[MAX_BUFFER_SIZE];
    std::vector<uint8_t> slave_buffer;

    while (running_) {
        ModbusRequest req;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !request_queue_.empty() || !running_; });

            if (!running_) break;

            req = request_queue_.front();
            request_queue_.pop();
        }

        if (log_packets_) {
            std::cout << "[Master " << req.origin_master_index + 1 << " -> Slave] Tx: " 
                      << to_hex_string(req.packet.data(), req.packet.size()) << std::endl;
        }

        // 이전 요청이 timeout 된 뒤 도착한 응답 조각은 새 요청의 응답으로 사용하지 않습니다.
        int s_fd = s_->get_fd();
        if (s_fd < 0) continue;
        while (running_) {
            fd_set stale_fds;
            FD_ZERO(&stale_fds);
            FD_SET(s_fd, &stale_fds);
            struct timeval no_wait = {0, 0};
            const int stale_activity = ::select(s_fd + 1, &stale_fds, nullptr, nullptr, &no_wait);
            if (stale_activity <= 0 || !FD_ISSET(s_fd, &stale_fds)) {
                break;
            }
            const ssize_t stale_bytes = s_->read_port(buffer, sizeof(buffer));
            if (stale_bytes <= 0) {
                break;
            }
            if (log_packets_) {
                std::cout << "[Hub] Discarded " << stale_bytes
                          << " stale slave byte(s) before next request." << std::endl;
            }
        }

        s_->write_port(req.packet.data(), req.packet.size());

        slave_buffer.clear();
        auto start_time = std::chrono::steady_clock::now();
        int remaining_ms = slave_timeout_ms_;
        bool response_complete = false;

        while (remaining_ms > 0 && running_) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(s_fd, &read_fds);

            struct timeval timeout;
            timeout.tv_sec = remaining_ms / 1000;
            timeout.tv_usec = (remaining_ms % 1000) * 1000;

            int activity = ::select(s_fd + 1, &read_fds, nullptr, nullptr, &timeout);

            if (activity > 0 && FD_ISSET(s_fd, &read_fds)) {
                ssize_t bytes_read = s_->read_port(buffer, sizeof(buffer));
                if (bytes_read > 0) {
                    slave_buffer.insert(slave_buffer.end(), buffer, buffer + bytes_read);

                    while (!slave_buffer.empty()) {
                        size_t frame_offset = 0;
                        const size_t frame_len = ModbusFramingParser::find_next_complete_frame(
                            slave_buffer, false, frame_offset);
                        if (frame_len == 0) {
                            break;
                        }
                        if (frame_offset > 0) {
                            slave_buffer.erase(slave_buffer.begin(),
                                               slave_buffer.begin() + frame_offset);
                        }
                        if (!ModbusFramingParser::response_matches_request(
                                req.packet, slave_buffer.data(), frame_len)) {
                            if (log_packets_) {
                                std::cout << "[Hub] Discarded unmatched slave frame: "
                                          << to_hex_string(slave_buffer.data(), frame_len)
                                          << std::endl;
                            }
                            slave_buffer.erase(slave_buffer.begin(),
                                               slave_buffer.begin() + frame_len);
                            const auto unmatched_elapsed =
                                std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start_time).count();
                            remaining_ms = slave_timeout_ms_ -
                                           static_cast<int>(unmatched_elapsed);
                            continue;
                        }
                        int origin_idx = req.origin_master_index;
                        if (origin_idx >= 0 && origin_idx < static_cast<int>(masters_.size())) {
                            auto target_master = masters_[origin_idx];
                            if (target_master && target_master->is_valid()) {
                                target_master->write_port(slave_buffer.data(), frame_len);
                                if (log_packets_) {
                                    std::cout << "[Slave -> Master " << origin_idx + 1 << "] Tx: " 
                                              << to_hex_string(slave_buffer.data(), frame_len) << std::endl;
                                }
                            }
                        }
                        response_complete = true;
                        break;
                    }
                    if (response_complete) {
                        break;
                    }
                    if (slave_buffer.size() > SAFE_GUARD_BUFFER_LIMIT) {
                        slave_buffer.clear();
                    }
                } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    break;
                }
            } else if (activity < 0) {
                if (errno == EINTR) continue;
                break;
            } else {
                break;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time
            ).count();
            remaining_ms = slave_timeout_ms_ - static_cast<int>(elapsed);
        }

        if (!response_complete) {
            std::cerr << "[Hub Warning] Slave Response Timeout (" << slave_timeout_ms_ 
                      << "ms) or incomplete frame for Master Index " 
                      << req.origin_master_index << std::endl;
        }
    }
}
