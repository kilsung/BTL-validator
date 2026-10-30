#include "vport.h"
#include <iostream>
#include <vector>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <sys/select.h>

std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

std::string make_link_path(const std::string& link_dir, const std::string& name) {
    if (link_dir.empty() || link_dir == "/") {
        return link_dir + name;
    }
    return link_dir + "/" + name;
}

void cleanup_links(int num_masters, const std::string& link_dir) {
    for (int i = 1; i <= num_masters; ++i) {
        const std::string link_path = make_link_path(
            link_dir, "ttyMASTER" + std::to_string(i));
        unlink(link_path.c_str());
    }
    const std::string slave_link = make_link_path(link_dir, "ttySLAVE");
    unlink(slave_link.c_str());
}

struct VportConfig {
    int slave_timeout_ms = DEFAULT_SLAVE_TIMEOUT_MS;
    int max_masters = 32;
    int max_queue_depth = 0; // 0 = auto (= max_masters)
};

bool load_config(const std::string& config_file_path,
                 VportConfig& cfg, bool config_explicit) {
    std::ifstream cfg_file(config_file_path);
    if (!cfg_file.is_open()) {
        if (config_explicit) {
            std::cerr << "[ERROR] 설정 파일을 찾을 수 없습니다: "
                      << config_file_path << std::endl;
            return false;
        }
        std::cout << " -> 설정 파일을 찾을 수 없습니다 (" << config_file_path
                  << "). 기본값을 사용합니다." << std::endl;
        return true;
    }

    std::string line;
    while (std::getline(cfg_file, line)) {
        if (line.empty() || line[0] == '#') continue;

        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim helper
        auto trim = [](std::string& s) {
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), s.end());
        };

        trim(key);
        trim(value);

        try {
            if (key == "DEFAULT_SLAVE_TIMEOUT_MS") {
                cfg.slave_timeout_ms = std::stoi(value);
                std::cout << " -> 슬레이브 응답 타임아웃 설정 로드 완료: "
                          << cfg.slave_timeout_ms << "ms (" << config_file_path
                          << ")" << std::endl;
            } else if (key == "MAX_MASTERS") {
                int parsed_max = std::stoi(value);
                if (parsed_max > FD_SETSIZE) {
                    std::cerr << "[ERROR] 설정 파일(" << config_file_path
                              << ")의 MAX_MASTERS 값(" << parsed_max
                              << ")이 POSIX select()의 FD_SETSIZE(" << FD_SETSIZE
                              << ")를 초과합니다." << std::endl;
                    return false;
                }
                cfg.max_masters = parsed_max;
                std::cout << " -> 마스터 수 상한 설정 로드 완료: "
                          << cfg.max_masters << " (" << config_file_path
                          << ")" << std::endl;
            } else if (key == "MAX_QUEUE_DEPTH") {
                cfg.max_queue_depth = std::stoi(value);
                std::cout << " -> 요청 큐 상한 설정 로드 완료: "
                          << cfg.max_queue_depth << " (" << config_file_path
                          << ")" << std::endl;
            }
        } catch (...) {
            std::cerr << "[Config Warning] 설정 값이 올바르지 않습니다: "
                      << key << "=" << value << std::endl;
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    int num_masters = 3; // 기본값 3
    std::string link_dir = "/dev";
    std::string config_path = "vport.cfg";
    bool config_explicit = false;
    bool master_count_set = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--link-dir") {
            if (i + 1 >= argc) {
                std::cerr << "--link-dir 옵션에는 경로가 필요합니다." << std::endl;
                return 1;
            }
            link_dir = argv[++i];
        } else if (arg == "--config") {
            if (i + 1 >= argc) {
                std::cerr << "--config 옵션에는 파일 경로가 필요합니다." << std::endl;
                return 1;
            }
            config_path = argv[++i];
            config_explicit = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "사용법: " << argv[0]
                      << " [마스터개수] [--link-dir 경로] [--config 파일]" << std::endl;
            return 0;
        } else if (!master_count_set) {
            try {
                num_masters = std::stoi(arg);
                master_count_set = true;
            } catch (...) {
                std::cerr << "알 수 없는 인자: " << arg << std::endl;
                return 1;
            }
        } else {
            std::cerr << "알 수 없는 인자: " << arg << std::endl;
            return 1;
        }
    }

    VportConfig cfg;
    if (!load_config(config_path, cfg, config_explicit)) {
        return 1;
    }

    if (num_masters < 1) {
        std::cerr << "마스터 개수는 최소 1개 이상이어야 합니다." << std::endl;
        return 1;
    }
    if (num_masters > cfg.max_masters) {
        std::cerr << "마스터 개수가 상한(" << cfg.max_masters
                  << ")을 초과합니다: " << num_masters << std::endl;
        return 1;
    }
    if (link_dir.empty()) {
        std::cerr << "링크 디렉터리는 비어 있을 수 없습니다." << std::endl;
        return 1;
    }

    // 1. PTY 가상 포트 생성 (POSIX openpt 및 symlink 설정, keeper fd 보관)
    std::vector<UniqueFd> master_fds;
    std::vector<UniqueFd> keeper_fds;
    for (int i = 1; i <= num_masters; ++i) {
        const std::string link_path = make_link_path(
            link_dir, "ttyMASTER" + std::to_string(i));
        UniqueFd k_fd;
        UniqueFd m_fd = create_pty_link(link_path, &k_fd);
        if (!m_fd.is_valid()) {
            std::cerr << "가상 마스터 포트 생성 실패: " << link_path << std::endl;
            cleanup_links(num_masters, link_dir);
            return 1;
        }
        master_fds.push_back(std::move(m_fd));
        keeper_fds.push_back(std::move(k_fd));
    }

    const std::string slave_link = make_link_path(link_dir, "ttySLAVE");
    UniqueFd slave_keeper_fd;
    UniqueFd s_fd = create_pty_link(slave_link, &slave_keeper_fd);
    if (!s_fd.is_valid()) {
        std::cerr << "가상 슬레이브 포트 생성 실패" << std::endl;
        cleanup_links(num_masters, link_dir);
        return 1;
    }

    // 2. RAII를 적용한 UniqueFd 자원 관리 적용 및 ISerialPort 인터페이스 포장
    std::vector<std::shared_ptr<ISerialPort>> masters;
    for (int i = 0; i < num_masters; ++i) {
        masters.push_back(std::make_shared<PosixSerialPort>(std::move(master_fds[i])));
    }
    std::shared_ptr<ISerialPort> s = std::make_shared<PosixSerialPort>(std::move(s_fd));

    std::cout << "[ModbusRTU Hub START] 마스터 수: " << num_masters << std::endl;
    for (int i = 1; i <= num_masters; ++i) {
        std::cout << " -> 마스터 " << i << " 장치 경로: "
                  << make_link_path(link_dir, "ttyMASTER" + std::to_string(i))
                  << std::endl;
    }
    std::cout << " -> 슬레이브 연동 경로: " << slave_link << std::endl;

    const char* env_log = std::getenv("VPORT_LOG");
    if (env_log != nullptr && std::string(env_log) == "1") {
        std::cout << " -> 패킷 로그 기능 활성화 (VPORT_LOG=1)" << std::endl;
    } else {
        std::cout << " -> 패킷 로그 기능 비활성화" << std::endl;
    }

    // Register signal handlers for graceful shutdown (Ctrl+C, kill)
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 3. ModbusHub 객체 생성 및 스레드 가동 (의존성 주입 완료)
    int queue_depth = (cfg.max_queue_depth > 0) ? cfg.max_queue_depth : num_masters;
    ModbusHub hub(std::move(masters), std::move(s), cfg.slave_timeout_ms, queue_depth);
    hub.start();

    std::cout << "\n[Running] 서비스가 실행 중입니다. (Ctrl+C 또는 SIGTERM 신호 수신 시 안전하게 종료됩니다.)" << std::endl;
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // 4. 안전한 스레드 종료 및 가상 심볼릭 링크 해제
    hub.stop();
    cleanup_links(num_masters, link_dir);
    
    std::cout << "[ModbusRTU Hub STOP]" << std::endl;
    return 0;
}
