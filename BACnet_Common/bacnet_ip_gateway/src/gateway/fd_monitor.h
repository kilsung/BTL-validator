#pragma once

/**
 * @file fd_monitor.h
 * @brief Foreign Device(BBMD 등록) 연결 상태 모니터
 *
 * BACstac은 FD 등록 상태를 공개 API로 제공하지 않으므로, 자기 장치의
 * Network Port Reliability(RENEW_FD_REGISTRATION_FAILURE 여부)를 주기적으로 읽어
 * 연결/끊김 메시지를 출력합니다. 재접속은 BACstac이 bip.cfg.fd의
 * fd.reg_timeout 주기로 수행합니다.
 *
 * TTL이 5분을 넘으면 BACstac은 TTL 시점에만 재등록하므로, 끊김을 늦게 알게 된다.
 * 이 경우 5분마다 자기 Network Port의 Command에 RENEW_FD_REGISTRATION을 써서
 * Register-Foreign-Device를 보내고 응답(BVLC-Result)으로 연결을 확인한다.
 */

#include "../common/fd_link_state.h"

#include <bactypes.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace gateway {

class FdMonitor {
public:
    FdMonitor() = default;
    ~FdMonitor();

    FdMonitor(const FdMonitor&)            = delete;
    FdMonitor& operator=(const FdMonitor&) = delete;

    /**
     * @brief 모니터 스레드를 시작합니다.
     * @param bbmd_ip         로그에 표시할 BBMD IP
     * @param bbmd_port       로그에 표시할 BBMD UDP 포트
     * @param ttl             로그에 표시할 등록 TTL(초)
     * @param retry_interval_ms 로그에 표시할 재시도 주기(ms, bip.cfg.fd의 fd.reg_timeout)
     */
    void start(const std::string& bbmd_ip, uint16_t bbmd_port, uint16_t ttl,
               uint32_t retry_interval_ms);

    /// 모니터 스레드를 종료하고 join합니다.
    void stop();

    /**
     * @brief 다음 폴링 주기(최대 1초 이내)에 즉시 Register-Foreign-Device 를 보내도록 요청합니다.
     *        NetworkWatcher 가 링크 복구를 감지했을 때 호출하여, TTL(최대 300초 renew 주기)을
     *        기다리지 않고 BBMD 에 재등록합니다. 스레드 안전.
     */
    void requestRenew() { renew_requested_ = true; }

    /**
     * @brief WriteProperty 요청이 이 모니터가 보낸 자기 재등록 명령인지 확인합니다.
     *        (자기 재등록 진행 중 + 요청 출처가 자기 장치 주소)
     *        외부에서 온 Command 쓰기는 계속 거부해야 하므로 writePropHook에서 사용합니다.
     */
    static bool isSelfRenewRequest(const BACSTAC_ADDRESS* source);

private:
    void run();

    /// 자기 Network Port 객체의 Reliability를 읽습니다.
    static bool readReliability(uint32_t& reliability);

    /// 자기 Network Port 객체의 Command에 RENEW_FD_REGISTRATION을 씁니다.
    static bool renewRegistration(int& status_out);

    /// 자기 재등록 WriteProperty 진행 중 표시 (writePropHook 허용 판단용)
    static std::atomic<bool> self_renew_in_progress_;

    std::string       bbmd_ip_;
    uint16_t          bbmd_port_         = 47808;
    uint16_t          ttl_               = 0;
    uint32_t          retry_interval_ms_ = 0;
    std::atomic<bool> running_{false};
    std::atomic<bool> renew_requested_{false};
    std::thread       thread_;
};

} // namespace gateway
