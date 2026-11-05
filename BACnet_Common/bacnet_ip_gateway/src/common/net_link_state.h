#pragma once

/**
 * @file net_link_state.h
 * @brief BACnet/IP 바인딩 인터페이스의 링크/IPv4 상태 판정 (순수 함수, 단위 테스트 대상)
 *
 * BACstac B/IP 소켓은 기동 시점의 IPv4 주소에 바인딩되며 주소 변경을 스스로 따라가지 못한다
 * (arch/linux/dl/bip/bipsys.c). 게이트웨이(NetworkWatcher)는 인터페이스 스냅샷을 주기적으로
 * 이 판정 함수에 넣어 링크 다운/복구와 IPv4 변경을 구분하고, IPv4 변경 시 재시작(execv)으로
 * 새 주소에 다시 바인딩한다.
 *
 * 인터페이스 고정(pinned, bip.cfg 의 iface=<이름>): 지정 인터페이스의 IPv4 만 사용하며
 * 다른 인터페이스로 넘어가지 않는다 (다중 NIC 환경).
 * 비고정(기존 ip= 방식): 바인딩 IPv4 가 사라지면 같은 인터페이스를 우선, 없으면 다른 활성
 * 인터페이스의 IPv4 를 후보로 삼는다 (재시작 시 BACstacCfgLoadBipConfig 의 자동 대체 동작과 동일).
 */

#include <cstdint>
#include <string>
#include <vector>

namespace gateway {
namespace net_link_state {

/// 인터페이스 IPv4 주소 1개에 대한 스냅샷 (getifaddrs 1 항목)
struct IfAddr {
    std::string name;
    uint32_t    ip       = 0U;   ///< host byte order
    uint32_t    netmask  = 0U;   ///< host byte order
    bool        up       = false; ///< IFF_UP
    bool        running  = false; ///< IFF_RUNNING (캐리어/링크)
    bool        loopback = false; ///< IFF_LOOPBACK
};

enum class State {
    Ok,         ///< 바인딩 IPv4 가 활성 인터페이스에 그대로 존재
    LinkDown,   ///< 바인딩 IPv4 의 인터페이스가 down 이거나, 사용 가능한 IPv4 가 없음
    IpChanged   ///< 바인딩 IPv4 가 사라지고 다른 활성 IPv4 가 있음, 또는 netmask 변경
};

struct Result {
    State       state = State::Ok;
    std::string ifname;            ///< 판정 대상 인터페이스 이름 (알 수 없으면 빈 문자열)
    uint32_t    new_ip = 0U;       ///< IpChanged 일 때 새로 바인딩할 IPv4
};

inline bool isLoopbackIp(uint32_t ip)
{
    return (ip >> 24) == 127U;
}

inline bool usable(const IfAddr& a)
{
    return a.up && a.running && !a.loopback && a.ip != 0U;
}

/**
 * @brief 지정 인터페이스의 사용 가능한(UP+RUNNING) 첫 IPv4 항목을 찾습니다.
 * @return 없으면 nullptr (인터페이스 없음, down, IPv4 미할당)
 */
inline const IfAddr* findUsableOn(const std::string& ifname, const std::vector<IfAddr>& snapshot)
{
    for (const IfAddr& a : snapshot) {
        if (a.name == ifname && usable(a)) {
            return &a;
        }
    }
    return nullptr;
}

/**
 * @brief 현재 인터페이스 스냅샷으로 바인딩 상태를 판정합니다.
 * @param bound_ip      BACstac 이 바인딩한 IPv4 (host order)
 * @param bound_netmask 바인딩 시점 netmask (0 이면 netmask 비교 생략: FD 모드 등)
 * @param bound_ifname  바인딩 인터페이스 이름 (빈 문자열 허용). 새 IPv4 후보 선택 시 우선
 * @param snapshot      getifaddrs 결과 (AF_INET 항목만)
 * @param pinned        true 면 bound_ifname 인터페이스만 판정 대상 (다른 인터페이스로 넘어가지 않음)
 */
inline Result classify(uint32_t bound_ip, uint32_t bound_netmask, const std::string& bound_ifname,
                       const std::vector<IfAddr>& snapshot, bool pinned = false)
{
    Result result;
    result.ifname = bound_ifname;

    // loopback 바인딩(검증 환경 127.0.0.1)은 링크 개념이 없으므로 항상 정상으로 본다.
    if (bound_ip == 0U || isLoopbackIp(bound_ip)) {
        return result;
    }

    for (const IfAddr& a : snapshot) {
        if (a.ip != bound_ip || a.loopback) {
            continue;
        }
        if (pinned && a.name != bound_ifname) {
            continue;  // 같은 IP 가 다른 인터페이스로 옮겨간 경우는 지정 인터페이스 기준으로 판정
        }
        result.ifname = a.name;
        if (!a.up || !a.running) {
            result.state = State::LinkDown;
            return result;
        }
        if (bound_netmask != 0U && a.netmask != bound_netmask) {
            result.state  = State::IpChanged;   // 같은 IP, 서브넷 변경 → broadcast 주소 재계산 필요
            result.new_ip = a.ip;
            return result;
        }
        return result;  // Ok
    }

    // 바인딩 IPv4 가 사라짐: 같은 인터페이스의 활성 IPv4 를 우선, 없으면(비고정일 때만) 다른 활성 인터페이스
    const IfAddr* candidate = nullptr;
    for (const IfAddr& a : snapshot) {
        if (!usable(a)) {
            continue;
        }
        if (pinned && a.name != bound_ifname) {
            continue;
        }
        if (!bound_ifname.empty() && a.name == bound_ifname) {
            candidate = &a;
            break;
        }
        if (candidate == nullptr) {
            candidate = &a;
        }
    }
    if (candidate == nullptr) {
        result.state = State::LinkDown;
        return result;
    }
    result.state  = State::IpChanged;
    result.ifname = candidate->name;
    result.new_ip = candidate->ip;
    return result;
}

/**
 * @brief 일시적인 판정 흔들림(DHCP 갱신, 링크 순간 단절)을 거르는 debounce.
 *        같은 판정이 threshold 회 연속되면 확정 상태로 바꿉니다.
 */
class Debouncer {
public:
    explicit Debouncer(uint32_t threshold = 3U) : threshold_(threshold == 0U ? 1U : threshold) {}

    /// @return 확정 상태가 바뀌었으면 true
    bool update(State observed)
    {
        if (observed == confirmed_) {
            pending_count_ = 0U;
            return false;
        }
        if (observed != pending_) {
            pending_       = observed;
            pending_count_ = 0U;
        }
        ++pending_count_;
        if (pending_count_ >= threshold_) {
            confirmed_     = observed;
            pending_count_ = 0U;
            return true;
        }
        return false;
    }

    State confirmed() const { return confirmed_; }

private:
    uint32_t threshold_;
    State    confirmed_     = State::Ok;
    State    pending_       = State::Ok;
    uint32_t pending_count_ = 0U;
};

/// 링크 다운 확정 상태에서 정상으로 돌아왔는지 (FD 즉시 재등록 트리거)
inline bool isLinkRecovery(State previous, State current)
{
    return previous == State::LinkDown && current == State::Ok;
}

} // namespace net_link_state
} // namespace gateway
