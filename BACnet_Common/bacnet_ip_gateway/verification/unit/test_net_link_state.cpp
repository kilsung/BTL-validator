/**
 * @file test_net_link_state.cpp
 * @brief BACnet/IP 바인딩 인터페이스 링크/IPv4 상태 판정 로직 단위 테스트
 *
 * 테스트 대상:
 *  - net_link_state::classify(): 링크 다운, IPv4 변경, netmask 변경, 인터페이스 고정(pinned)
 *  - net_link_state::findUsableOn(): 지정 인터페이스 IPv4 선택
 *  - net_link_state::Debouncer: 일시 흔들림 무시, 연속 판정 확정
 *  - net_link_state::isLinkRecovery(): FD 즉시 재등록 트리거
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "common/net_link_state.h"

namespace nls = gateway::net_link_state;

namespace {

uint32_t ip(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    return (a << 24) | (b << 16) | (c << 8) | d;
}

const uint32_t kMask24 = ip(255, 255, 255, 0);
const uint32_t kMask16 = ip(255, 255, 0, 0);

nls::IfAddr entry(const char* name, uint32_t addr, uint32_t mask, bool up = true, bool running = true)
{
    nls::IfAddr a;
    a.name     = name;
    a.ip       = addr;
    a.netmask  = mask;
    a.up       = up;
    a.running  = running;
    a.loopback = (std::string(name) == "lo");
    return a;
}

std::vector<nls::IfAddr> twoNics(uint32_t eth0_ip, bool eth0_up = true)
{
    std::vector<nls::IfAddr> v;
    v.push_back(entry("lo", ip(127, 0, 0, 1), ip(255, 0, 0, 0)));
    v.push_back(entry("eth0", eth0_ip, kMask24, eth0_up, eth0_up));
    v.push_back(entry("eth1", ip(10, 0, 0, 5), kMask24));
    return v;
}

} // namespace

TEST_CASE("NetLinkState: bound IPv4 present and running is OK") {
    const uint32_t bound = ip(192, 168, 1, 3);
    const nls::Result r = nls::classify(bound, kMask24, "eth0", twoNics(bound), true);
    ASSERT_TRUE(r.state == nls::State::Ok);
    ASSERT_TRUE(r.ifname == "eth0");
}

TEST_CASE("NetLinkState: loopback binding is always OK") {
    std::vector<nls::IfAddr> none;
    ASSERT_TRUE(nls::classify(ip(127, 0, 0, 1), 0U, "", none).state == nls::State::Ok);
}

TEST_CASE("NetLinkState: interface down with address still assigned is LINK_DOWN") {
    const uint32_t bound = ip(192, 168, 1, 3);
    const nls::Result r = nls::classify(bound, kMask24, "eth0", twoNics(bound, false), true);
    ASSERT_TRUE(r.state == nls::State::LinkDown);
}

TEST_CASE("NetLinkState: carrier lost (UP but not RUNNING) is LINK_DOWN") {
    const uint32_t bound = ip(192, 168, 1, 3);
    std::vector<nls::IfAddr> v;
    v.push_back(entry("eth0", bound, kMask24, true, false));
    ASSERT_TRUE(nls::classify(bound, kMask24, "eth0", v, true).state == nls::State::LinkDown);
}

TEST_CASE("NetLinkState: pinned interface IPv4 change follows the same interface") {
    const uint32_t bound = ip(192, 168, 1, 3);
    const nls::Result r = nls::classify(bound, kMask24, "eth0", twoNics(ip(192, 168, 1, 50)), true);
    ASSERT_TRUE(r.state == nls::State::IpChanged);
    ASSERT_TRUE(r.ifname == "eth0");
    ASSERT_TRUE(r.new_ip == ip(192, 168, 1, 50));
}

TEST_CASE("NetLinkState: pinned interface without IPv4 never falls back to another NIC") {
    const uint32_t bound = ip(192, 168, 1, 3);
    std::vector<nls::IfAddr> v;
    v.push_back(entry("eth1", ip(10, 0, 0, 5), kMask24));  // eth0 에 IPv4 없음 (DHCP 해제 등)
    const nls::Result r = nls::classify(bound, kMask24, "eth0", v, true);
    ASSERT_TRUE(r.state == nls::State::LinkDown);
}

TEST_CASE("NetLinkState: bound IPv4 moved to another NIC is not OK when pinned") {
    const uint32_t bound = ip(192, 168, 1, 3);
    std::vector<nls::IfAddr> v;
    v.push_back(entry("eth1", bound, kMask24));
    ASSERT_TRUE(nls::classify(bound, kMask24, "eth0", v, true).state == nls::State::LinkDown);
}

TEST_CASE("NetLinkState: unpinned binding may follow another active NIC") {
    const uint32_t bound = ip(192, 168, 1, 3);
    std::vector<nls::IfAddr> v;
    v.push_back(entry("eth1", ip(10, 0, 0, 5), kMask24));
    const nls::Result r = nls::classify(bound, kMask24, "eth0", v, false);
    ASSERT_TRUE(r.state == nls::State::IpChanged);
    ASSERT_TRUE(r.ifname == "eth1");
}

TEST_CASE("NetLinkState: netmask change on the same IPv4 requires rebind") {
    const uint32_t bound = ip(192, 168, 1, 3);
    std::vector<nls::IfAddr> v;
    v.push_back(entry("eth0", bound, kMask16));
    const nls::Result r = nls::classify(bound, kMask24, "eth0", v, true);
    ASSERT_TRUE(r.state == nls::State::IpChanged);
    ASSERT_TRUE(r.new_ip == bound);
    // netmask 0 (FD 모드) 이면 netmask 비교 생략
    ASSERT_TRUE(nls::classify(bound, 0U, "eth0", v, true).state == nls::State::Ok);
}

TEST_CASE("NetLinkState: findUsableOn selects only the named running interface") {
    const std::vector<nls::IfAddr> v = twoNics(ip(192, 168, 1, 3));
    const nls::IfAddr* e = nls::findUsableOn("eth1", v);
    ASSERT_TRUE(e != nullptr && e->ip == ip(10, 0, 0, 5));
    ASSERT_TRUE(nls::findUsableOn("eth9", v) == nullptr);
    ASSERT_TRUE(nls::findUsableOn("eth0", twoNics(ip(192, 168, 1, 3), false)) == nullptr);
}

TEST_CASE("NetLinkState: debouncer ignores transient samples and confirms after threshold") {
    nls::Debouncer d(3U);
    ASSERT_FALSE(d.update(nls::State::LinkDown));
    ASSERT_FALSE(d.update(nls::State::Ok));         // 흔들림 → 카운트 초기화
    ASSERT_TRUE(d.confirmed() == nls::State::Ok);
    ASSERT_FALSE(d.update(nls::State::LinkDown));
    ASSERT_FALSE(d.update(nls::State::LinkDown));
    ASSERT_TRUE(d.update(nls::State::LinkDown));    // 3회 연속 → 확정
    ASSERT_TRUE(d.confirmed() == nls::State::LinkDown);
    ASSERT_FALSE(d.update(nls::State::LinkDown));   // 같은 상태 유지는 변화 아님
}

TEST_CASE("NetLinkState: link recovery is detected only from LINK_DOWN to OK") {
    ASSERT_TRUE(nls::isLinkRecovery(nls::State::LinkDown, nls::State::Ok));
    ASSERT_FALSE(nls::isLinkRecovery(nls::State::Ok, nls::State::Ok));
    ASSERT_FALSE(nls::isLinkRecovery(nls::State::IpChanged, nls::State::Ok));
    ASSERT_FALSE(nls::isLinkRecovery(nls::State::LinkDown, nls::State::IpChanged));
}
