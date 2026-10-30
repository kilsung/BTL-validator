/**
 * @file test_fd_link_state.cpp
 * @brief Foreign Device 연결 상태 판정 로직 단위 테스트
 *
 * 테스트 대상:
 *  - fd_link_state::next(): Reliability 기반 상태 전이, 기동 직후 대기(grace) 처리
 *  - fd_link_state::shouldReport(): 연결/끊김 메시지 출력 여부
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "common/fd_link_state.h"

namespace fls = gateway::fd_link_state;

namespace {
const uint32_t kOk = 0U;  // RELIABILITY_NO_FAULT_DETECTED
const uint32_t kGrace = 3000U;
}

TEST_CASE("FdLinkState: registered is deferred during startup grace") {
    ASSERT_TRUE(fls::next(fls::State::Unknown, true, kOk, 1000U, kGrace) == fls::State::Unknown);
    ASSERT_TRUE(fls::next(fls::State::Unknown, true, kOk, 3000U, kGrace) == fls::State::Registered);
}

TEST_CASE("FdLinkState: failure is reported immediately even during grace") {
    const fls::State next = fls::next(fls::State::Unknown, true,
                                      fls::kRenewFdRegistrationFailure, 0U, kGrace);
    ASSERT_TRUE(next == fls::State::Failed);
    ASSERT_TRUE(fls::shouldReport(fls::State::Unknown, next));
}

TEST_CASE("FdLinkState: recovery after failure is reported without grace") {
    const fls::State next = fls::next(fls::State::Failed, true, kOk, 500U, kGrace);
    ASSERT_TRUE(next == fls::State::Registered);
    ASSERT_TRUE(fls::shouldReport(fls::State::Failed, next));
}

TEST_CASE("FdLinkState: read failure keeps previous state and reports nothing") {
    ASSERT_TRUE(fls::next(fls::State::Registered, false, 0U, 10000U, kGrace) == fls::State::Registered);
    ASSERT_TRUE(fls::next(fls::State::Failed, false, 0U, 10000U, kGrace) == fls::State::Failed);
    ASSERT_FALSE(fls::shouldReport(fls::State::Registered, fls::State::Registered));
    ASSERT_FALSE(fls::shouldReport(fls::State::Unknown, fls::State::Unknown));
}

TEST_CASE("FdLinkState: renew every 5 minutes only when TTL exceeds 5 minutes") {
    ASSERT_TRUE(fls::shouldRenew(1800U, 300000U));
    ASSERT_FALSE(fls::shouldRenew(1800U, 299999U));
    ASSERT_FALSE(fls::shouldRenew(300U, 600000U));
    ASSERT_FALSE(fls::shouldRenew(60U, 600000U));
    ASSERT_TRUE(fls::shouldRenew(301U, 300000U));
}

TEST_CASE("FdLinkState: connection loss after registration is reported") {
    const fls::State next = fls::next(fls::State::Registered, true,
                                      fls::kRenewFdRegistrationFailure, 60000U, kGrace);
    ASSERT_TRUE(next == fls::State::Failed);
    ASSERT_TRUE(fls::shouldReport(fls::State::Registered, next));
}
