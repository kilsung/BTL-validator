#include "bacnet_gateway.h"
#include "file_logger.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <unistd.h>

static std::atomic<bool> g_running(true);

static void signalHandler(int sig) {
    (void)sig;
    g_running = false;
}

/**
 * @brief Modbus RTU ↔ BACnet IP Gateway 진입점
 *
 * v2 변경: 단일 JSON 파일 인자 방식에서 config 폴더 경로 방식으로 변경.
 *
 * 사용법:
 *   ./bacnet_ip_gateway [config_dir]
 *
 *   config_dir: 설정 파일들이 위치한 폴더 경로 (기본값: "./conf")
 *               해당 폴더 내의 gateway.json과 profiles/ 하위 장치 JSON 파일들을 자동으로 로드합니다.
 *               BACstac 네트워크 설정(bip.cfg)은 이 폴더의 상위 폴더에서 로드합니다.
 *               (예: ./conf -> ./bip.cfg, /etc/gateway/conf -> /etc/gateway/bip.cfg)
 *
 * 예시:
 *   ./bacnet_ip_gateway ./conf
 *   ./bacnet_ip_gateway /etc/gateway/conf
 */
int main(int argc, char* argv[]) {
    // 콘솔 출력을 /var/log 로그 파일(크기 제한, 최신 내용 유지)에도 기록한다.
    // 경로/크기: BACNET_LOG_FILE, BACNET_LOG_MAX_KB 환경 변수 (file_logger.h 참조)
    gateway::FileLogger::installFromEnvironment();

    std::string config_dir = "./conf";
    if (argc > 1) {
        config_dir = argv[1];
    }

    std::cout << "==================================================\n"
              << "       Modbus RTU to BACnet IP Gateway v2\n"
              << "==================================================\n"
              << "[Main] Config directory: " << config_dir << "\n";

    gateway::BacnetGateway gateway;
    gateway.setConfigDir(config_dir); // bip.cfg는 config 폴더의 상위 폴더에서 로드

    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT,  signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 설정 로드 (gateway.json 및 장치 프로필 매핑)
    const std::string gateway_cfg_path = config_dir + "/gateway.json";
    const std::string profile_dir = config_dir + "/profiles";

    while (g_running) {
        if (!gateway.loadGatewayConfig(gateway_cfg_path)) {
            if (!g_running) {
                std::cout << "[Main] Startup aborted by shutdown signal." << std::endl;
                return 0;
            }
            std::cerr << "\n=================================================================\n"
                      << "[FATAL CONFIG ERROR] Failed to load gateway config: " << gateway_cfg_path << "\n"
                      << "[FATAL CONFIG ERROR] Process is entering HOLD (sleep) state to prevent infinite restart loops by systemd/supervisors.\n"
                      << "[FATAL CONFIG ERROR] Waiting 60 seconds to reload configuration...\n"
                      << "[FATAL CONFIG ERROR] (Please correct the configuration file to resume startup)\n"
                      << "=================================================================" << std::endl;
            for (int s = 0; s < 60 && g_running; ++s) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!g_running) {
                std::cout << "[Main] Startup aborted by shutdown signal." << std::endl;
                return 0;
            }
            std::cout << "[Main] Reloading configuration to check for fixes... (Reloading device profiles)" << std::endl;
            continue;
        }

        std::string load_err;
        bool is_contradiction = false;
        if (!gateway.loadConfigDir(profile_dir, &load_err, &is_contradiction)) {
            if (!g_running) {
                std::cout << "[Main] Startup aborted by shutdown signal." << std::endl;
                return 0;
            }

            std::cerr << "\n=================================================================\n"
                      << "[FATAL CONFIG ERROR] " << (load_err.empty() ? "Failed to load device configurations from " + profile_dir : load_err) << "\n"
                      << "[FATAL CONFIG ERROR] Process is entering HOLD (sleep) state to prevent infinite restart loops by systemd/supervisors.\n"
                      << "[FATAL CONFIG ERROR] Waiting 60 seconds to reload configuration...\n"
                      << "[FATAL CONFIG ERROR] (Please correct the configuration file to resume startup)\n"
                      << "=================================================================" << std::endl;
            for (int s = 0; s < 60 && g_running; ++s) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            if (!g_running) {
                std::cout << "[Main] Startup aborted by shutdown signal." << std::endl;
                return 0;
            }
            std::cout << "[Main] Reloading configuration to check for fixes... (Reloading device profiles)" << std::endl;
            continue;
        }

        break; // gateway.json 및 장치 프로필 모두 성공적으로 로드 완료
    }

    if (!g_running) {
        return 0;
    }

#ifdef ENABLE_TPI_EXPORT
    if (gateway.exportCurrentDeviceTpi("current_device.tpi")) {
        std::cout << "[Main] TPI export mode: current_device.tpi generated successfully. Exiting." << std::endl;
        return 0;
    } else {
        std::cerr << "[Main] Failed to export current_device.tpi" << std::endl;
        std::cerr << "[Main] Gateway process terminated with exit code 1." << std::endl;
        return 1;
    }
#endif

    if (!gateway.start(&g_running)) {
        if (!g_running) {
            std::cout << "[Main] Gateway startup interrupted by shutdown signal." << std::endl;
            return 0;
        }
        std::cerr << "[Main] Failed to start gateway." << std::endl;
        std::cerr << "[Main] Gateway process terminated with exit code 1." << std::endl;
        return 1;
    }

    // 메인 스레드 감시 루프
    std::cout << "[Main] Listening for commands. Press Ctrl+C to stop..." << std::endl;
    while (g_running && !gateway.isRestartRequested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // B/IP 설정 변경 반영: Network Port 쓰기(BACnet_IP_Mode, FD_BBMD_Address, FD_Subscription_Lifetime)
    // 또는 바인딩 인터페이스의 IPv4 변경(NetworkWatcher).
    // BACstac은 실행 중 B/IP 설정을 바꿀 수 없으므로 같은 인자로 자기 자신을 재실행한다.
    if (g_running && gateway.isRestartRequested()) {
        std::cout << "[Main] BACnet/IP configuration changed. Restarting gateway..." << std::endl;
        gateway.stop();
        std::cout.flush();
        std::cerr.flush();
        gateway::FileLogger::uninstall(); // 로그 fd 는 O_CLOEXEC, 재실행 후 같은 파일에 이어서 기록
        execv("/proc/self/exe", argv);
        std::cerr << "[Main] Failed to restart gateway (execv errno=" << errno << ": "
                  << std::strerror(errno) << "). Exiting with code 1." << std::endl;
        return 1;
    }

    std::cout << "\n[Main] Shutdown signal received. Stopping gateway..." << std::endl;
    gateway.stop();
    std::cout << "[Main] gateway.stop() completed. Exiting main." << std::endl;
    gateway::FileLogger::uninstall();

    return 0;
}
