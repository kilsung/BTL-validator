#pragma once

/**
 * @file file_logger.h
 * @brief 콘솔(stdout/stderr) 출력을 크기 제한이 있는 로그 파일에 함께 기록하는 로거
 *
 * - std::cout / std::clog / std::cerr 의 출력을 기존 콘솔에 그대로 내보내면서
 *   동일 내용을 로그 파일에도 기록한다 (tee 방식, 기존 출력 코드 변경 불필요).
 * - 파일에는 각 줄 앞에 "YYYY-MM-DD HH:MM:SS.mmm I|E " 접두사를 붙인다.
 *   (I: stdout/clog, E: stderr)
 * - 크기 제한: 현재 파일이 (max_total_bytes / 2) 를 넘으면 "<path>.1" 로 교체(rename)하고
 *   새 파일에 기록한다. 따라서 디스크 사용량은 항상 max_total_bytes 이하이며
 *   가장 최근 내용이 보존된다.
 * - execv 재시작 시에도 같은 파일에 이어서(append) 기록한다.
 *
 * 환경 변수:
 *   BACNET_LOG_FILE    로그 파일 경로 (기본값: /var/log/bacnet_ip_gateway.log,
 *                      빈 문자열 또는 "none" 이면 파일 로그 비활성화)
 *   BACNET_LOG_MAX_KB  로그 파일 총 크기 제한 KB (현재 파일 + .1 백업, 기본값: 1024, 최소 32)
 */

#include <cstddef>
#include <string>

namespace gateway {

class FileLogger {
public:
    static constexpr const char* kDefaultPath   = "/var/log/bacnet_ip_gateway.log";
    static constexpr std::size_t kDefaultMaxKb  = 1024U;
    static constexpr std::size_t kMinMaxKb      = 32U;

    /// 환경 변수(BACNET_LOG_FILE, BACNET_LOG_MAX_KB)를 반영하여 파일 로그를 설치한다.
    /// 파일을 열 수 없으면 경고를 stderr 에 출력하고 콘솔 전용으로 계속 동작한다.
    static bool installFromEnvironment();

    /// 지정 경로/크기로 파일 로그를 설치한다. 이미 설치되어 있으면 true 를 반환한다.
    static bool install(const std::string& path, std::size_t max_total_bytes);

    /// 버퍼를 비우고 원래 콘솔 스트림 버퍼로 복구한다 (execv 직전/종료 시 호출).
    static void uninstall();
};

} // namespace gateway
