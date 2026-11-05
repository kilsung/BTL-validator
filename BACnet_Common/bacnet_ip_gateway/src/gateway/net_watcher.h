#pragma once

/**
 * @file net_watcher.h
 * @brief BACnet/IP 바인딩 인터페이스 링크/IPv4 변경 감시
 *
 * 1초 주기로 getifaddrs 스냅샷을 net_link_state::classify() 로 판정하고, debounce 후
 * 다음을 수행합니다 (모든 상태 변화는 BACNET_LOG 와 무관하게 stderr 로 상시 출력).
 *  - 링크 다운: 로그만 출력 (BACstac 이 같은 IP 로 수신을 자동 재개)
 *  - 링크 복구: 로그 출력 + on_link_recovered 콜백 (FD 모드 즉시 재등록)
 *  - IPv4 변경: 로그 출력 + on_ip_changed 콜백 (게이트웨이 재시작 → 새 IPv4 로 재바인딩)
 */

#include "../common/net_link_state.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace gateway {

class NetworkWatcher {
public:
    using IpChangedCallback     = std::function<void(const std::string& old_ip,
                                                     const std::string& new_ip)>;
    using LinkRecoveredCallback = std::function<void()>;

    NetworkWatcher() = default;
    ~NetworkWatcher();

    NetworkWatcher(const NetworkWatcher&)            = delete;
    NetworkWatcher& operator=(const NetworkWatcher&) = delete;

    /**
     * @param bound_ip      BACstac 이 바인딩한 IPv4 문자열
     * @param bound_netmask 바인딩 서브넷 마스크 문자열 (빈 문자열이면 netmask 변경 감시 생략)
     * @param pinned_iface  bip.cfg iface= 로 지정한 인터페이스 이름.
     *                      비어 있지 않으면 이 인터페이스만 감시/추종한다 (다중 NIC 환경).
     *                      비어 있으면 bound_ip 를 가진 인터페이스를 기준으로 하되 다른 인터페이스로도 추종한다.
     */
    void start(const std::string& bound_ip, const std::string& bound_netmask,
               const std::string& pinned_iface,
               IpChangedCallback on_ip_changed, LinkRecoveredCallback on_link_recovered);

    void stop();

    /// 현재 시스템의 AF_INET 인터페이스 스냅샷 (getifaddrs)
    static std::vector<net_link_state::IfAddr> snapshot();

    /// IPv4 문자열 → host order (실패 시 0)
    static uint32_t parseIpv4(const std::string& text);

    /// host order → IPv4 문자열
    static std::string formatIpv4(uint32_t ip);

    /// ip 에 해당하는 인터페이스의 netmask 문자열 (없으면 빈 문자열)
    static std::string netmaskOf(const std::string& ip);

private:
    void run();

    uint32_t              bound_ip_      = 0U;
    uint32_t              bound_netmask_ = 0U;
    std::string           bound_ifname_;
    bool                  pinned_        = false;
    IpChangedCallback     on_ip_changed_;
    LinkRecoveredCallback on_link_recovered_;
    std::atomic<bool>     running_{false};
    std::thread           thread_;
};

} // namespace gateway
