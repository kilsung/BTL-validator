#include "net_watcher.h"
#include "modbus_client.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>

#include <chrono>
#include <iostream>

namespace gateway {

namespace {

constexpr int      kPollIntervalMs  = 1000;
constexpr int      kPollSliceMs     = 100;
constexpr uint32_t kDebounceSamples = 3U;   ///< 3회(약 3초) 연속 같은 판정이면 확정

const char* stateName(net_link_state::State s)
{
    switch (s) {
        case net_link_state::State::Ok:        return "OK";
        case net_link_state::State::LinkDown:  return "LINK_DOWN";
        case net_link_state::State::IpChanged: return "IP_CHANGED";
    }
    return "UNKNOWN";
}

} // namespace

NetworkWatcher::~NetworkWatcher()
{
    stop();
}

uint32_t NetworkWatcher::parseIpv4(const std::string& text)
{
    struct in_addr addr;
    if (text.empty() || inet_pton(AF_INET, text.c_str(), &addr) != 1) {
        return 0U;
    }
    return ntohl(addr.s_addr);
}

std::string NetworkWatcher::formatIpv4(uint32_t ip)
{
    struct in_addr addr;
    addr.s_addr = htonl(ip);
    char buf[INET_ADDRSTRLEN] = {0};
    if (inet_ntop(AF_INET, &addr, buf, sizeof(buf)) == nullptr) {
        return std::string();
    }
    return std::string(buf);
}

std::vector<net_link_state::IfAddr> NetworkWatcher::snapshot()
{
    std::vector<net_link_state::IfAddr> result;
    struct ifaddrs* list = nullptr;
    if (getifaddrs(&list) != 0) {
        return result;
    }
    for (struct ifaddrs* ifa = list; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        net_link_state::IfAddr entry;
        entry.name     = (ifa->ifa_name != nullptr) ? ifa->ifa_name : "";
        entry.ip       = ntohl(reinterpret_cast<const struct sockaddr_in*>(
                                   static_cast<const void*>(ifa->ifa_addr))->sin_addr.s_addr);
        if (ifa->ifa_netmask != nullptr) {
            entry.netmask = ntohl(reinterpret_cast<const struct sockaddr_in*>(
                                      static_cast<const void*>(ifa->ifa_netmask))->sin_addr.s_addr);
        }
        entry.up       = (ifa->ifa_flags & IFF_UP) != 0U;
        entry.running  = (ifa->ifa_flags & IFF_RUNNING) != 0U;
        entry.loopback = (ifa->ifa_flags & IFF_LOOPBACK) != 0U;
        result.push_back(entry);
    }
    freeifaddrs(list);
    return result;
}

std::string NetworkWatcher::netmaskOf(const std::string& ip)
{
    const uint32_t target = parseIpv4(ip);
    if (target == 0U) {
        return std::string();
    }
    for (const net_link_state::IfAddr& a : snapshot()) {
        if (a.ip == target && a.netmask != 0U) {
            return formatIpv4(a.netmask);
        }
    }
    return std::string();
}

void NetworkWatcher::start(const std::string& bound_ip, const std::string& bound_netmask,
                           const std::string& pinned_iface,
                           IpChangedCallback on_ip_changed, LinkRecoveredCallback on_link_recovered)
{
    if (running_.exchange(true)) {
        return;
    }
    bound_ip_          = parseIpv4(bound_ip);
    bound_netmask_     = parseIpv4(bound_netmask);
    on_ip_changed_     = std::move(on_ip_changed);
    on_link_recovered_ = std::move(on_link_recovered);
    pinned_       = !pinned_iface.empty();
    bound_ifname_ = pinned_iface;
    if (!pinned_) {
        for (const net_link_state::IfAddr& a : snapshot()) {
            if (a.ip == bound_ip_) {
                bound_ifname_ = a.name;
                break;
            }
        }
    }
    if (bound_ip_ == 0U || net_link_state::isLoopbackIp(bound_ip_)) {
        std::cout << "[Net] Network watcher idle (bound to " << bound_ip << ", loopback/unspecified)"
                  << std::endl;
    } else {
        std::cout << "[Net] Network watcher started: " << bound_ip
                  << (bound_ifname_.empty() ? std::string() : " on " + bound_ifname_)
                  << (pinned_ ? " [pinned by bip.cfg iface=]" : "")
                  << " (netmask " << (bound_netmask.empty() ? "-" : bound_netmask) << ")" << std::endl;
    }
    thread_ = std::thread(&NetworkWatcher::run, this);
}

void NetworkWatcher::stop()
{
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void NetworkWatcher::run()
{
    net_link_state::Debouncer debouncer(kDebounceSamples);
    bool restart_notified = false;

    while (running_.load()) {
        const net_link_state::Result r =
            net_link_state::classify(bound_ip_, bound_netmask_, bound_ifname_, snapshot(), pinned_);
        const net_link_state::State previous = debouncer.confirmed();

        if (debouncer.update(r.state)) {
            const net_link_state::State current = debouncer.confirmed();
            const std::string ifname = r.ifname.empty() ? std::string("?") : r.ifname;
            // 이상 상황/복구 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
            if (current == net_link_state::State::LinkDown) {
                std::cerr << "[" << ModbusClient::getMsTimestamp() << "][Net] Link down on " << ifname
                          << " (bound " << formatIpv4(bound_ip_) << "). Waiting for link recovery..."
                          << std::endl;
            } else if (current == net_link_state::State::Ok) {
                std::cerr << "[" << ModbusClient::getMsTimestamp() << "][Net] Link up on " << ifname
                          << " (" << formatIpv4(bound_ip_) << ", " << stateName(previous)
                          << " -> OK)" << std::endl;
                if (net_link_state::isLinkRecovery(previous, current) && on_link_recovered_) {
                    on_link_recovered_();
                }
            } else if (current == net_link_state::State::IpChanged && !restart_notified) {
                const std::string old_ip = formatIpv4(bound_ip_);
                const std::string new_ip = formatIpv4(r.new_ip);
                std::cerr << "[" << ModbusClient::getMsTimestamp() << "][Net] IPv4 changed on "
                          << ifname << ": " << old_ip << " -> " << new_ip
                          << ", restarting to rebind BACnet/IP" << std::endl;
                restart_notified = true;  // 재시작까지 한 번만 요청
                if (on_ip_changed_) {
                    on_ip_changed_(old_ip, new_ip);
                }
            }
        }

        for (int waited = 0; waited < kPollIntervalMs && running_.load(); waited += kPollSliceMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollSliceMs));
        }
    }
}

} // namespace gateway
