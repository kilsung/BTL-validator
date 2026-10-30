#ifndef VPORT_H
#define VPORT_H

#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <memory>
#include <string>
#include <cstdint>
#include <sys/select.h>

static constexpr int DEFAULT_SLAVE_TIMEOUT_MS = 500;
static constexpr size_t MAX_BUFFER_SIZE = 1024;
static constexpr size_t SAFE_GUARD_BUFFER_LIMIT = 2048;

// RAII Wrapper for POSIX file descriptors to prevent resource/FD leaks
class UniqueFd {
public:
    UniqueFd();
    explicit UniqueFd(int fd);
    ~UniqueFd();

    // Disable copy constructors/assignment to maintain unique ownership
    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    // Enable move semantics for transfer of ownership
    UniqueFd(UniqueFd&& other) noexcept;
    UniqueFd& operator=(UniqueFd&& other) noexcept;

    int get() const;
    void reset(int new_fd = -1);
    int release();
    bool is_valid() const;

private:
    int fd_;
};

// Serial Port abstraction interface for unit testing (Dependency Injection)
class ISerialPort {
public:
    virtual ~ISerialPort() = default;
    virtual ssize_t read_port(void* buf, size_t count) = 0;
    virtual ssize_t write_port(const void* buf, size_t count) = 0;
    virtual int get_fd() const = 0;
    virtual bool is_valid() const = 0;
};

// Concrete POSIX PTY serial port implementation
class PosixSerialPort : public ISerialPort {
public:
    explicit PosixSerialPort(UniqueFd fd);
    
    ssize_t read_port(void* buf, size_t count) override;
    ssize_t write_port(const void* buf, size_t count) override;
    int get_fd() const override;
    bool is_valid() const override;

private:
    UniqueFd fd_;
};

// Creates virtual PTY and creates link path.
// If keeper_out is non-null, opens the slave side and stores the fd there
// to prevent EIO when all clients close the slave (§7.6 CPU spin fix).
UniqueFd create_pty_link(const std::string& link_path,
                         UniqueFd* keeper_out = nullptr);

// Modbus RTU Framing Parser
class ModbusFramingParser {
public:
    static uint16_t calculate_crc16(const uint8_t* data, size_t length);
    static size_t find_complete_frame(const std::vector<uint8_t>& buffer, bool is_request);
    static size_t find_next_complete_frame(const std::vector<uint8_t>& buffer,
                                           bool is_request,
                                           size_t& frame_offset);
    static bool response_matches_request(const std::vector<uint8_t>& request,
                                         const uint8_t* response,
                                         size_t response_len);
};

// Represents a request popped from a master port
struct ModbusRequest {
    int origin_master_index; // 0-based index of master
    std::vector<uint8_t> packet;
};

// Multi-master to Single-slave Modbus Hub
class ModbusHub {
public:
    ModbusHub(std::vector<std::shared_ptr<ISerialPort>> masters,
              std::shared_ptr<ISerialPort> slave,
              int slave_timeout_ms = DEFAULT_SLAVE_TIMEOUT_MS,
              int max_queue_depth = 0);
    ~ModbusHub();

    void start();
    void stop();
    void queue_request(int master_index, const std::vector<uint8_t>& packet);
    void process_master_input(ISerialPort& port, int master_index, std::vector<uint8_t>& buffer);

private:
    void listener_loop();
    void dispatcher_loop();

    std::vector<std::shared_ptr<ISerialPort>> masters_;
    std::shared_ptr<ISerialPort> s_;

    std::atomic<bool> running_;
    bool log_packets_;
    std::queue<ModbusRequest> request_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    std::vector<std::vector<uint8_t>> master_buffers_;

    std::thread listener_thread_;
    std::thread dispatcher_thread_;

    int slave_timeout_ms_;
    int max_queue_depth_;  // 0 = unlimited (기존 동작 호환)
    std::atomic<int> active_master_index_{-1}; // 현재 슬레이브 응답 대기 중인 마스터 인덱스 (-1 = 없음)
};

#endif // VPORT_H
