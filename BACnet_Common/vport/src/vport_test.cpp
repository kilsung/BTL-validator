#include "vport.h"
#include <cassert>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <thread>
#include <vector>

// Mock implementation of ISerialPort for testing without physical PTY/serial ports
class MockSerialPort : public ISerialPort {
public:
    MockSerialPort() : is_valid_(true), read_idx_(0) {}
    
    ssize_t read_port(void* buf, size_t count) override {
        if (read_buffer_.empty()) return 0;
        size_t available = read_buffer_.size() - read_idx_;
        size_t to_copy = std::min(count, available);
        if (to_copy == 0) return 0;
        std::memcpy(buf, &read_buffer_[read_idx_], to_copy);
        read_idx_ += to_copy;
        return to_copy;
    }
    
    ssize_t write_port(const void* buf, size_t count) override {
        const uint8_t* p = static_cast<const uint8_t*>(buf);
        write_buffer_.insert(write_buffer_.end(), p, p + count);
        return count;
    }
    
    int get_fd() const override {
        return 999; // Dummy file descriptor for testing
    }
    
    bool is_valid() const override {
        return is_valid_;
    }
    
    void set_read_data(const std::vector<uint8_t>& data) {
        read_buffer_ = data;
        read_idx_ = 0;
    }
    
    const std::vector<uint8_t>& get_written_data() const {
        return write_buffer_;
    }
    
    void clear() {
        read_buffer_.clear();
        write_buffer_.clear();
        read_idx_ = 0;
    }

private:
    bool is_valid_;
    std::vector<uint8_t> read_buffer_;
    std::vector<uint8_t> write_buffer_;
    size_t read_idx_;
};

// Double-ended Port based on Linux socketpair for true asynchronous E2E integration testing
class SocketPairPort : public ISerialPort {
public:
    SocketPairPort() {
        int sv[2];
        if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0) {
            hub_fd_.reset(sv[0]);
            test_fd_.reset(sv[1]);
            // Set non-blocking to prevent tests from blocking forever
            fcntl(hub_fd_.get(), F_SETFL, O_NONBLOCK);
            fcntl(test_fd_.get(), F_SETFL, O_NONBLOCK);
        }
    }
    
    ssize_t read_port(void* buf, size_t count) override {
        return ::read(hub_fd_.get(), buf, count);
    }
    
    ssize_t write_port(const void* buf, size_t count) override {
        return ::write(hub_fd_.get(), buf, count);
    }
    
    int get_fd() const override {
        return hub_fd_.get();
    }
    
    bool is_valid() const override {
        return hub_fd_.is_valid();
    }
    
    // Testing side interfaces
    ssize_t test_read(void* buf, size_t count) {
        return ::read(test_fd_.get(), buf, count);
    }
    
    ssize_t test_write(const void* buf, size_t count) {
        return ::write(test_fd_.get(), buf, count);
    }
    
    int get_test_fd() const {
        return test_fd_.get();
    }

private:
    UniqueFd hub_fd_;
    UniqueFd test_fd_;
};

static std::vector<uint8_t> with_crc(std::vector<uint8_t> frame) {
    const uint16_t crc = ModbusFramingParser::calculate_crc16(frame.data(), frame.size());
    frame.push_back(static_cast<uint8_t>(crc & 0xFFU));
    frame.push_back(static_cast<uint8_t>((crc >> 8U) & 0xFFU));
    return frame;
}

static std::vector<uint8_t> make_read_request(uint8_t unit, uint16_t address,
                                              uint16_t quantity) {
    return with_crc({unit, 0x03U,
                     static_cast<uint8_t>((address >> 8U) & 0xFFU),
                     static_cast<uint8_t>(address & 0xFFU),
                     static_cast<uint8_t>((quantity >> 8U) & 0xFFU),
                     static_cast<uint8_t>(quantity & 0xFFU)});
}

static std::vector<uint8_t> make_read_response(uint8_t unit,
                                               const std::vector<uint16_t>& values) {
    std::vector<uint8_t> frame{unit, 0x03U,
                               static_cast<uint8_t>(values.size() * 2U)};
    for (uint16_t value : values) {
        frame.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
        frame.push_back(static_cast<uint8_t>(value & 0xFFU));
    }
    return with_crc(frame);
}

static bool wait_for_readable(int fd, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return ::select(fd + 1, &read_fds, nullptr, nullptr, &timeout) > 0 &&
           FD_ISSET(fd, &read_fds);
}

static void write_all(int fd, const std::vector<uint8_t>& data) {
    size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
        if (written > 0) {
            offset += static_cast<size_t>(written);
            continue;
        }
        if (written < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        assert(false && "socketpair write failed");
    }
}

static bool read_complete_frame(int fd, bool is_request,
                                std::vector<uint8_t>& frame, int timeout_ms) {
    std::vector<uint8_t> buffer;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    uint8_t read_buffer[128];
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (!wait_for_readable(fd, static_cast<int>(std::max<int64_t>(1, remaining)))) {
            continue;
        }
        const ssize_t bytes = ::read(fd, read_buffer, sizeof(read_buffer));
        if (bytes > 0) {
            buffer.insert(buffer.end(), read_buffer, read_buffer + bytes);
            size_t frame_offset = 0;
            const size_t frame_len = ModbusFramingParser::find_next_complete_frame(
                buffer, is_request, frame_offset);
            if (frame_len > 0) {
                frame.assign(buffer.begin() + frame_offset,
                             buffer.begin() + frame_offset + frame_len);
                return true;
            }
            if (buffer.size() > SAFE_GUARD_BUFFER_LIMIT) {
                buffer.clear();
            }
        } else if (bytes < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
            return false;
        } else if (bytes == 0) {
            return false;
        }
    }
    return false;
}

static bool has_data(int fd, int timeout_ms) {
    return wait_for_readable(fd, timeout_ms);
}

void test_crc16() {
    // 01 03 00 00 00 02 should have CRC 0x0BC4
    std::vector<uint8_t> data = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02};
    uint16_t crc = ModbusFramingParser::calculate_crc16(data.data(), data.size());
    assert(crc == 0x0BC4);
    std::cout << "[Test Success] test_crc16" << std::endl;
}

void test_framing_parser_request() {
    // 1. Valid Read Holding Registers (FC 0x03) request: 8 bytes
    // CRC for Modbus transmitted in Little-Endian: Low-byte (C4) first, High-byte (0B) last
    std::vector<uint8_t> valid_req = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    size_t frame_len = ModbusFramingParser::find_complete_frame(valid_req, true);
    assert(frame_len == 8);

    // 2. Fragmented packet: 5 bytes
    std::vector<uint8_t> frag_req(valid_req.begin(), valid_req.begin() + 5);
    frame_len = ModbusFramingParser::find_complete_frame(frag_req, true);
    assert(frame_len == 0); // Incomplete, should return 0

    // 3. CRC mismatch
    std::vector<uint8_t> corrupt_req = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0xFF};
    frame_len = ModbusFramingParser::find_complete_frame(corrupt_req, true);
    assert(frame_len == 0);

    // 4. FC 0x10 (Write Multiple Registers) Request (9 + byte_count bytes)
    // 9 + 4 = 13 bytes
    std::vector<uint8_t> write_req = {0x01, 0x10, 0x00, 0x00, 0x00, 0x02, 0x04, 0x00, 0x0A, 0x00, 0x14};
    uint16_t w_crc = ModbusFramingParser::calculate_crc16(write_req.data(), write_req.size());
    write_req.push_back(w_crc & 0xFF);
    write_req.push_back((w_crc >> 8) & 0xFF);

    frame_len = ModbusFramingParser::find_complete_frame(write_req, true);
    assert(frame_len == 13);

    std::cout << "[Test Success] test_framing_parser_request" << std::endl;
}

void test_framing_parser_response() {
    // 1. FC 0x03 Response: Slave(01) FC(03) ByteCount(04) Data(00 0A 00 14) CRC
    // 5 + 4 = 9 bytes
    std::vector<uint8_t> valid_resp = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14};
    uint16_t r_crc = ModbusFramingParser::calculate_crc16(valid_resp.data(), valid_resp.size());
    valid_resp.push_back(r_crc & 0xFF);
    valid_resp.push_back((r_crc >> 8) & 0xFF);

    size_t frame_len = ModbusFramingParser::find_complete_frame(valid_resp, false);
    assert(frame_len == 9);

    // 2. FC 0x06 Response (Panasonic non-standard 6-byte response: C8 06 A0 07 E7 87)
    std::vector<uint8_t> panasonic_resp = {0xC8, 0x06, 0xA0, 0x07};
    uint16_t p_crc = ModbusFramingParser::calculate_crc16(panasonic_resp.data(), panasonic_resp.size());
    panasonic_resp.push_back(p_crc & 0xFF);
    panasonic_resp.push_back((p_crc >> 8) & 0xFF);

    size_t frame_len_p = ModbusFramingParser::find_complete_frame(panasonic_resp, false);
    assert(frame_len_p == 6);

    // 3. FC 0x06 Response (Standard 8-byte response: C8 06 A0 07 00 00 0B 92)
    std::vector<uint8_t> std_06_resp = {0xC8, 0x06, 0xA0, 0x07, 0x00, 0x00};
    uint16_t std_crc = ModbusFramingParser::calculate_crc16(std_06_resp.data(), std_06_resp.size());
    std_06_resp.push_back(std_crc & 0xFF);
    std_06_resp.push_back((std_crc >> 8) & 0xFF);

    size_t frame_len_std = ModbusFramingParser::find_complete_frame(std_06_resp, false);
    assert(frame_len_std == 8);

    std::cout << "[Test Success] test_framing_parser_response" << std::endl;
}

void test_framing_recovery_and_response_matching() {
    const std::vector<uint8_t> request = {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B
    };
    std::vector<uint8_t> response = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14};
    const uint16_t response_crc = ModbusFramingParser::calculate_crc16(
        response.data(), response.size());
    response.push_back(response_crc & 0xFFU);
    response.push_back((response_crc >> 8U) & 0xFFU);

    std::vector<uint8_t> prefixed = {0xFF, 0x00, 0x7E};
    prefixed.insert(prefixed.end(), response.begin(), response.end());
    size_t frame_offset = 0;
    const size_t frame_len = ModbusFramingParser::find_next_complete_frame(
        prefixed, false, frame_offset);
    assert(frame_offset == 3);
    assert(frame_len == response.size());
    assert(ModbusFramingParser::response_matches_request(
        request, prefixed.data() + frame_offset, frame_len));

    std::vector<uint8_t> other_response = response;
    other_response[0] = 0x02;
    const uint16_t other_crc = ModbusFramingParser::calculate_crc16(
        other_response.data(), other_response.size() - 2);
    other_response[other_response.size() - 2] = other_crc & 0xFFU;
    other_response[other_response.size() - 1] = (other_crc >> 8U) & 0xFFU;
    assert(!ModbusFramingParser::response_matches_request(
        request, other_response.data(), other_response.size()));

    std::cout << "[Test Success] test_framing_recovery_and_response_matching" << std::endl;
}

void test_packet_reassembly() {
    auto m1 = std::make_shared<MockSerialPort>();
    auto m2 = std::make_shared<MockSerialPort>();
    auto m3 = std::make_shared<MockSerialPort>();
    auto s  = std::make_shared<MockSerialPort>();

    std::vector<std::shared_ptr<ISerialPort>> masters = { m1, m2, m3 };
    ModbusHub hub(masters, s);

    // Simulate packet arriving in two chunks:
    // Chunk 1: [01, 03, 00, 00, 00]
    // Chunk 2: [02, C4, 0B]
    std::vector<uint8_t> frag1 = {0x01, 0x03, 0x00, 0x00, 0x00};
    std::vector<uint8_t> frag2 = {0x02, 0xC4, 0x0B};

    std::vector<uint8_t> master_buffer;

    // Process chunk 1
    m1->set_read_data(frag1);
    hub.process_master_input(*m1, 0, master_buffer);
    assert(master_buffer.size() == 5); // Remainder buffer should contain chunk 1

    // Process chunk 2
    m1->set_read_data(frag2);
    hub.process_master_input(*m1, 0, master_buffer);
    assert(master_buffer.empty()); // The whole frame must be reconstructed and queue popped

    std::cout << "[Test Success] test_packet_reassembly" << std::endl;
}

// Full E2E integration test using socketpairs to test thread concurrency,
// zero sleep delay (condition_variable), and slave response fragmentation assembly.
void test_hub_e2e_communication() {
    auto m1 = std::make_shared<SocketPairPort>();
    auto m2 = std::make_shared<SocketPairPort>(); // Idle
    auto m3 = std::make_shared<SocketPairPort>(); // Idle
    auto s  = std::make_shared<SocketPairPort>();

    std::vector<std::shared_ptr<ISerialPort>> masters = { m1, m2, m3 };
    ModbusHub hub(masters, s);

    // Start Hub Threads
    hub.start();

    // 1. Send Modbus Request from Master 1
    std::vector<uint8_t> request = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
    ssize_t written = m1->test_write(request.data(), request.size());
    assert(written == static_cast<ssize_t>(request.size()));

    // 2. Read request on Slave side (wait up to 100ms)
    std::vector<uint8_t> received_request(8, 0);
    int s_test_fd = s->get_test_fd();
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(s_test_fd, &read_fds);
    struct timeval timeout = {0, 100000}; // 100ms
    int activity = select(s_test_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    assert(activity > 0);

    ssize_t bytes_read = s->test_read(received_request.data(), received_request.size());
    assert(bytes_read == 8);
    assert(received_request == request);

    // 3. Send an unmatched frame first; it must not be routed to Master 1.
    std::vector<uint8_t> unmatched_response = {0x02, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14};
    uint16_t unmatched_crc = ModbusFramingParser::calculate_crc16(
        unmatched_response.data(), unmatched_response.size());
    unmatched_response.push_back(unmatched_crc & 0xFFU);
    unmatched_response.push_back((unmatched_crc >> 8U) & 0xFFU);
    s->test_write(unmatched_response.data(), unmatched_response.size());

    // 4. Send Fragmented Slave Response to check reassembly logic
    // Expected response format: Slave(01) FC(03) ByteCount(04) Data(00 0A 00 14) CRC(Low) CRC(High)
    std::vector<uint8_t> response = {0x01, 0x03, 0x04, 0x00, 0x0A, 0x00, 0x14};
    uint16_t r_crc = ModbusFramingParser::calculate_crc16(response.data(), response.size());
    response.push_back(r_crc & 0xFF);
    response.push_back((r_crc >> 8) & 0xFF);

    // Split response into 2 fragments: [01, 03, 04] and [00, 0A, 00, 14, Low, High]
    std::vector<uint8_t> resp_frag1(response.begin(), response.begin() + 3);
    std::vector<uint8_t> resp_frag2(response.begin() + 3, response.end());

    // Write first fragment
    s->test_write(resp_frag1.data(), resp_frag1.size());
    // Sleep briefly to ensure it is processed as a separate packet chunk in dispatcher_loop
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // Write second fragment
    s->test_write(resp_frag2.data(), resp_frag2.size());

    // 5. Verify Master 1 receives only the reconstructed matching response
    std::vector<uint8_t> final_response(9, 0);
    int m1_test_fd = m1->get_test_fd();
    FD_ZERO(&read_fds);
    FD_SET(m1_test_fd, &read_fds);
    timeout = {0, 200000}; // 200ms
    activity = select(m1_test_fd + 1, &read_fds, nullptr, nullptr, &timeout);
    assert(activity > 0);

    bytes_read = m1->test_read(final_response.data(), final_response.size());
    assert(bytes_read == 9);
    assert(final_response == response);

    // Stop Hub Threads
    hub.stop();

    std::cout << "[Test Success] test_hub_e2e_communication (including fragmentation check)" << std::endl;
}

void test_crc_recovery_and_guard_limit() {
    const std::vector<uint8_t> request = make_read_request(1, 0, 2);
    std::vector<uint8_t> corrupt = request;
    corrupt[corrupt.size() - 1] ^= 0xFFU;
    std::vector<uint8_t> combined = corrupt;
    combined.insert(combined.end(), request.begin(), request.end());
    size_t frame_offset = 0;
    const size_t frame_len = ModbusFramingParser::find_next_complete_frame(
        combined, true, frame_offset);
    assert(frame_offset == corrupt.size());
    assert(frame_len == request.size());

    auto master = std::make_shared<MockSerialPort>();
    auto slave = std::make_shared<MockSerialPort>();
    ModbusHub hub({master}, slave);
    std::vector<uint8_t> buffer(SAFE_GUARD_BUFFER_LIMIT + 1U, 0xA5U);
    hub.process_master_input(*master, 0, buffer);
    assert(buffer.empty());

    std::cout << "[Test Success] test_crc_recovery_and_guard_limit" << std::endl;
}

static void run_multi_master_round(bool concurrent) {
    auto m1 = std::make_shared<SocketPairPort>();
    auto m2 = std::make_shared<SocketPairPort>();
    auto m3 = std::make_shared<SocketPairPort>();
    auto slave = std::make_shared<SocketPairPort>();
    assert(m1->is_valid() && m2->is_valid() && m3->is_valid() && slave->is_valid());
    std::vector<std::shared_ptr<ISerialPort>> masters = {m1, m2, m3};
    ModbusHub hub(masters, slave, 300);
    hub.start();

    const std::vector<std::vector<uint8_t>> requests = {
        make_read_request(1, 10, 1),
        make_read_request(1, 20, 2),
        make_read_request(1, 30, 3),
    };
    const std::vector<std::shared_ptr<SocketPairPort>> master_ports = {m1, m2, m3};
    if (concurrent) {
        for (size_t i = 0; i < master_ports.size(); ++i) {
            write_all(master_ports[i]->get_test_fd(), requests[i]);
        }
    }

    std::vector<bool> completed(requests.size(), false);
    for (size_t round = 0; round < requests.size(); ++round) {
        if (!concurrent) {
            write_all(master_ports[round]->get_test_fd(), requests[round]);
        }
        std::vector<uint8_t> received_request;
        assert(read_complete_frame(slave->get_test_fd(), true, received_request, 500));
        size_t request_index = requests.size();
        for (size_t i = 0; i < requests.size(); ++i) {
            if (!completed[i] && requests[i] == received_request) {
                request_index = i;
                break;
            }
        }
        assert(request_index < requests.size());
        completed[request_index] = true;
        const std::vector<uint16_t> values(request_index + 1U, static_cast<uint16_t>(0x100U + request_index));
        const std::vector<uint8_t> response = make_read_response(1, values);
        write_all(slave->get_test_fd(), response);
        std::vector<uint8_t> routed_response;
        assert(read_complete_frame(master_ports[request_index]->get_test_fd(), false,
                                   routed_response, 500));
        assert(routed_response == response);
        for (size_t i = 0; i < master_ports.size(); ++i) {
            if (i != request_index) {
                assert(!has_data(master_ports[i]->get_test_fd(), 20));
            }
        }
    }
    assert(std::all_of(completed.begin(), completed.end(), [](bool value) { return value; }));
    hub.stop();
}

void test_multi_master_concurrent_routing() {
    run_multi_master_round(true);
    std::cout << "[Test Success] test_multi_master_concurrent_routing" << std::endl;
}

void test_multi_master_sequential_routing() {
    run_multi_master_round(false);
    std::cout << "[Test Success] test_multi_master_sequential_routing" << std::endl;
}

void test_slave_timeout_recovery() {
    auto m1 = std::make_shared<SocketPairPort>();
    auto m2 = std::make_shared<SocketPairPort>();
    auto slave = std::make_shared<SocketPairPort>();
    assert(m1->is_valid() && m2->is_valid() && slave->is_valid());
    ModbusHub hub({m1, m2}, slave, 100);
    hub.start();

    const std::vector<uint8_t> first = make_read_request(1, 40, 1);
    const std::vector<uint8_t> second = make_read_request(1, 41, 1);
    write_all(m1->get_test_fd(), first);
    std::vector<uint8_t> received;
    assert(read_complete_frame(slave->get_test_fd(), true, received, 300));
    assert(received == first);
    assert(!has_data(m1->get_test_fd(), 180));

    write_all(m2->get_test_fd(), second);
    received.clear();
    assert(read_complete_frame(slave->get_test_fd(), true, received, 300));
    assert(received == second);
    const std::vector<uint8_t> response = make_read_response(1, {0x55});
    write_all(slave->get_test_fd(), response);
    std::vector<uint8_t> routed;
    assert(read_complete_frame(m2->get_test_fd(), false, routed, 300));
    assert(routed == response);
    assert(!has_data(m1->get_test_fd(), 20));
    hub.stop();

    std::cout << "[Test Success] test_slave_timeout_recovery" << std::endl;
}

void test_pty_slave_close_cpu_spin() {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "[TC-VPORT-010] PTY 슬레이브 닫힘 시 EIO 무한 루프 & CPU 100% 스핀 재현 및 방지 테스트" << std::endl;
    std::cout << "========================================================" << std::endl;

    const std::string link_path = "/tmp/vport_test_spin_pty";
    (void)::unlink(link_path.c_str());

    UniqueFd keeper_fd;
    UniqueFd master_fd = create_pty_link(link_path, &keeper_fd);
    assert(master_fd.is_valid());
    assert(keeper_fd.is_valid());

    // ---------------------------------------------------------
    // 1단계: PTY 커널 드라이버 격리 동작 검증 (select + read EIO 반복 횟수 실측)
    // ---------------------------------------------------------
    std::cout << "[Step 1] PTY 커널 레벨: keeper_fd 보유 상태에서 슬레이브 open -> close 후 select() 반응 측정..." << std::endl;
    int s_fd = ::open(link_path.c_str(), O_RDWR | O_NOCTTY);
    assert(s_fd >= 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ::close(s_fd); // 슬레이브 포트 닫음 (하지만 keeper_fd가 열려있어 참조 카운트 유지됨)

    // 100ms 동안 select(50ms 타임아웃) + read() 시도 반복 횟수 측정
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
    uint64_t loop_count = 0;
    uint64_t eio_count = 0;
    char read_buf[64];

    while (std::chrono::steady_clock::now() < deadline) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(master_fd.get(), &rfds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 50000; // 50ms
        int act = ::select(master_fd.get() + 1, &rfds, nullptr, nullptr, &tv);
        if (act > 0 && FD_ISSET(master_fd.get(), &rfds)) {
            loop_count++;
            ssize_t n = ::read(master_fd.get(), read_buf, sizeof(read_buf));
            if (n < 0 && errno == EIO) {
                eio_count++;
            }
        }
    }

    std::cout << "  - 100ms 동안 select 즉시 반환 횟수 : " << loop_count << " 회" << std::endl;
    std::cout << "  - read() EIO 반환 횟수               : " << eio_count << " 회" << std::endl;
    assert(eio_count == 0 && "EIO must not occur when keeper fd is held");
    std::cout << "  -> [정상 대기] 슬레이브가 닫혀도 keeper fd 덕분에 select()가 정상 대기함" << std::endl;

    // ---------------------------------------------------------
    // 2단계: ModbusHub 통합 레벨 (listener_thread CPU 사용량 실측)
    // ---------------------------------------------------------
    std::cout << "\n[Step 2] ModbusHub 실서비스 레벨: listener_thread CPU 사용량 측정..." << std::endl;
    auto master_port = std::make_shared<PosixSerialPort>(std::move(master_fd));
    auto dummy_slave = std::make_shared<SocketPairPort>();

    ModbusHub hub({master_port}, dummy_slave, 500);
    hub.start();

    // 슬레이브를 다시 열었다가 닫음
    s_fd = ::open(link_path.c_str(), O_RDWR | O_NOCTTY);
    assert(s_fd >= 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ::close(s_fd);

    // 슬레이브 닫힌 직후 200ms 동안 CPU 사용 시간(user + sys) 측정
    struct rusage ru_start, ru_end;
    int r_start = getrusage(RUSAGE_SELF, &ru_start);
    assert(r_start == 0);
    (void)r_start;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    int r_end = getrusage(RUSAGE_SELF, &ru_end);
    assert(r_end == 0);
    (void)r_end;

    hub.stop();
    (void)::unlink(link_path.c_str());

    long cpu_time_us = (ru_end.ru_utime.tv_sec - ru_start.ru_utime.tv_sec) * 1000000L +
                       (ru_end.ru_utime.tv_usec - ru_start.ru_utime.tv_usec) +
                       (ru_end.ru_stime.tv_sec - ru_start.ru_stime.tv_sec) * 1000000L +
                       (ru_end.ru_stime.tv_usec - ru_start.ru_stime.tv_usec);

    double cpu_usage_percent = (static_cast<double>(cpu_time_us) / 200000.0) * 100.0;
    std::cout << "  - 200ms sleep 중 소비된 CPU 시간    : " << cpu_time_us << " us ("
              << cpu_usage_percent << "% core usage)" << std::endl;

    assert(cpu_time_us < 50000 && "CPU spin detected when keeper fd is held");
    std::cout << "  -> [테스트 통과] CPU 스핀 없이 정상 절전/대기 상태 유지 확인" << std::endl;
    std::cout << "[Test Success] TC-VPORT-010" << std::endl;
}

// TC-VPORT-012: 요청 큐 상한 초과 시 Modbus Exception(0x06: Server Device Busy) 응답 검증
void test_queue_overflow_exception_response() {
    auto m1 = std::make_shared<SocketPairPort>();
    auto m2 = std::make_shared<SocketPairPort>();
    auto slave = std::make_shared<SocketPairPort>();
    assert(m1->is_valid() && m2->is_valid() && slave->is_valid());

    ModbusHub hub({m1, m2}, slave, 500, /*max_queue_depth=*/1);
    hub.start();

    const std::vector<uint8_t> req1 = make_read_request(1, 10, 1);
    const std::vector<uint8_t> req2 = make_read_request(1, 20, 1);
    const std::vector<uint8_t> req3 = make_read_request(1, 30, 1);

    // m1 -> req1: dispatcher가 pop하여 slave로 write (queue는 비워짐)
    write_all(m1->get_test_fd(), req1);
    std::vector<uint8_t> received_slave;
    assert(read_complete_frame(slave->get_test_fd(), true, received_slave, 500));
    assert(received_slave == req1);

    // m2 -> req2: 큐에 진입 (queue size = 1 == max_queue_depth)
    write_all(m2->get_test_fd(), req2);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));

    // m1 -> req3: 큐 크기 한계 초과 -> Exception Response 회신되어야 함
    write_all(m1->get_test_fd(), req3);

    std::vector<uint8_t> exc_resp;
    assert(read_complete_frame(m1->get_test_fd(), false, exc_resp, 500));
    assert(exc_resp.size() == 5);
    assert(exc_resp[0] == 1);       // Slave ID
    assert(exc_resp[1] == 0x83);    // FC 0x03 | 0x80
    assert(exc_resp[2] == 0x06);    // Server Device Busy
    uint16_t expected_crc = ModbusFramingParser::calculate_crc16(exc_resp.data(), 3);
    uint16_t received_crc = exc_resp[3] | (exc_resp[4] << 8);
    assert(expected_crc == received_crc);

    // 슬레이브가 req1에 응답
    const std::vector<uint8_t> resp1 = make_read_response(1, {0x1111});
    write_all(slave->get_test_fd(), resp1);
    std::vector<uint8_t> m1_resp;
    assert(read_complete_frame(m1->get_test_fd(), false, m1_resp, 500));
    assert(m1_resp == resp1);

    // req2가 이제 슬레이브에 도달함
    received_slave.clear();
    assert(read_complete_frame(slave->get_test_fd(), true, received_slave, 500));
    assert(received_slave == req2);

    const std::vector<uint8_t> resp2 = make_read_response(1, {0x2222});
    write_all(slave->get_test_fd(), resp2);
    std::vector<uint8_t> m2_resp;
    assert(read_complete_frame(m2->get_test_fd(), false, m2_resp, 500));
    assert(m2_resp == resp2);

    hub.stop();
    std::cout << "[Test Success] TC-VPORT-012" << std::endl;
}

// TC-VPORT-013: 표준 FC만 지원 및 비표준 FC 거부, Modbus Exception 정상 처리 검증
void test_standard_fc_only_and_exception_response() {
    // 1. 비표준 FC (0x44) 요청 -> find_complete_frame이 0 반환
    std::vector<uint8_t> non_std_req = {0x01, 0x44, 0x00, 0x01, 0x00, 0x01};
    uint16_t crc = ModbusFramingParser::calculate_crc16(non_std_req.data(), non_std_req.size());
    non_std_req.push_back(static_cast<uint8_t>(crc & 0xFF));
    non_std_req.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

    size_t frame_len = ModbusFramingParser::find_complete_frame(non_std_req, true);
    assert(frame_len == 0 && "Non-standard FC must not be parsed as complete frame");

    // 2. 비표준 FC 응답 -> find_complete_frame이 0 반환
    std::vector<uint8_t> non_std_resp = {0x01, 0x44, 0x02, 0xAA, 0xBB};
    crc = ModbusFramingParser::calculate_crc16(non_std_resp.data(), non_std_resp.size());
    non_std_resp.push_back(static_cast<uint8_t>(crc & 0xFF));
    non_std_resp.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

    frame_len = ModbusFramingParser::find_complete_frame(non_std_resp, false);
    assert(frame_len == 0 && "Non-standard FC response must not be parsed as complete frame");

    // 3. 표준 Modbus Exception 응답 (FC 0x83, code 0x02) -> 정상 5바이트 파싱
    std::vector<uint8_t> exc_resp = {0x01, 0x83, 0x02};
    crc = ModbusFramingParser::calculate_crc16(exc_resp.data(), exc_resp.size());
    exc_resp.push_back(static_cast<uint8_t>(crc & 0xFF));
    exc_resp.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

    frame_len = ModbusFramingParser::find_complete_frame(exc_resp, false);
    assert(frame_len == 5 && "Exception frame must be parsed as exactly 5 bytes");

    // 4. E2E Hub 검증: Exception 응답이 원 발신 마스터로 정상 라우팅되는지 확인
    auto m1 = std::make_shared<SocketPairPort>();
    auto slave = std::make_shared<SocketPairPort>();
    ModbusHub hub({m1}, slave, 500);
    hub.start();

    const std::vector<uint8_t> req = make_read_request(1, 10, 1);
    write_all(m1->get_test_fd(), req);

    std::vector<uint8_t> slave_rx;
    assert(read_complete_frame(slave->get_test_fd(), true, slave_rx, 500));
    assert(slave_rx == req);

    write_all(slave->get_test_fd(), exc_resp);
    std::vector<uint8_t> m1_rx;
    assert(read_complete_frame(m1->get_test_fd(), false, m1_rx, 500));
    assert(m1_rx == exc_resp);

    hub.stop();
    std::cout << "[Test Success] TC-VPORT-013" << std::endl;
}

// TC-VPORT-015: 타임아웃 어긋남 시 stale response drain 검증
void test_stale_response_drain_on_timeout() {
    auto m1 = std::make_shared<SocketPairPort>();
    auto m2 = std::make_shared<SocketPairPort>();
    auto slave = std::make_shared<SocketPairPort>();
    assert(m1->is_valid() && m2->is_valid() && slave->is_valid());

    // vport slave_timeout_ms = 150ms
    ModbusHub hub({m1, m2}, slave, 150);
    hub.start();

    const std::vector<uint8_t> req1 = make_read_request(1, 10, 1);
    const std::vector<uint8_t> req2 = make_read_request(1, 20, 1);

    // 1. m1이 req1 전송
    write_all(m1->get_test_fd(), req1);
    std::vector<uint8_t> slave_rx;
    assert(read_complete_frame(slave->get_test_fd(), true, slave_rx, 500));
    assert(slave_rx == req1);

    // 슬레이브가 150ms 이내에 응답하지 않음 -> vport 타임아웃 발생
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 2. vport 타임아웃 후 슬레이브가 뒤늦게(stale) req1의 응답을 slave 포트에 보냄
    const std::vector<uint8_t> stale_resp1 = make_read_response(1, {0xDEAD});
    write_all(slave->get_test_fd(), stale_resp1);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    // 3. m2가 req2 전송
    write_all(m2->get_test_fd(), req2);

    // hub는 req2를 처리하기 전에 stale slave 응답(stale_resp1)을 폐기(drain)해야 함
    slave_rx.clear();
    assert(read_complete_frame(slave->get_test_fd(), true, slave_rx, 500));
    assert(slave_rx == req2);

    // 슬레이브가 req2의 정상 응답 회신
    const std::vector<uint8_t> resp2 = make_read_response(1, {0xBEEF});
    write_all(slave->get_test_fd(), resp2);

    // m2는 resp2만을 정확하게 수신해야 함 (stale_resp1이 섞이지 않음)
    std::vector<uint8_t> m2_rx;
    assert(read_complete_frame(m2->get_test_fd(), false, m2_rx, 500));
    assert(m2_rx == resp2);

    // m1은 아무것도 받지 않았어야 함
    assert(!has_data(m1->get_test_fd(), 20));

    hub.stop();
    std::cout << "[Test Success] TC-VPORT-015" << std::endl;
}

int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--case") {
        const std::string selected = argv[2];
        if (selected == "TC-VPORT-005") {
            test_crc_recovery_and_guard_limit();
        } else if (selected == "TC-VPORT-006") {
            test_multi_master_concurrent_routing();
        } else if (selected == "TC-VPORT-007") {
            test_multi_master_sequential_routing();
        } else if (selected == "TC-VPORT-009") {
            test_slave_timeout_recovery();
        } else if (selected == "TC-VPORT-004") {
            test_packet_reassembly();
        } else if (selected == "TC-VPORT-008") {
            test_framing_parser_response();
        } else if (selected == "TC-VPORT-010") {
            test_pty_slave_close_cpu_spin();
        } else if (selected == "TC-VPORT-012") {
            test_queue_overflow_exception_response();
        } else if (selected == "TC-VPORT-013") {
            test_standard_fc_only_and_exception_response();
        } else if (selected == "TC-VPORT-015") {
            test_stale_response_drain_on_timeout();
        } else {
            std::cerr << "Unknown test case: " << selected << std::endl;
            return 2;
        }
        return 0;
    }

    std::cout << "=== 단위 테스트 시작 ===" << std::endl;
    test_crc16();
    test_framing_parser_request();
    test_framing_parser_response();
    test_framing_recovery_and_response_matching();
    test_packet_reassembly();
    test_hub_e2e_communication();
    std::cout << "========================" << std::endl;
    std::cout << "모든 단위 테스트 성공" << std::endl;
    return 0;
}
