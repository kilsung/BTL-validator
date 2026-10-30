#pragma once

#include <cstdint>

namespace gateway {
namespace fd_link_state {

/// BBMD Foreign Device 등록 상태 (Network Port Reliability 기반 판정)
enum class State {
    Unknown,     ///< 아직 판정 전 (기동 직후 또는 Reliability 읽기 실패)
    Registered,  ///< 등록 성공 (Reliability != RENEW_FD_REGISTRATION_FAILURE)
    Failed       ///< 등록 실패/끊김 (Reliability == RENEW_FD_REGISTRATION_FAILURE)
};

/// BACnetReliability RENEW_FD_REGISTRATION_FAILURE (BACstac RELIABILITY_RENEW_FD_REG_FAILURE)
static constexpr uint32_t kRenewFdRegistrationFailure = 19U;

/**
 * @brief Reliability 읽기 결과로 다음 상태를 정합니다.
 *        읽기에 실패하면 판단 근거가 없으므로 이전 상태를 유지합니다.
 *
 * BACstac은 첫 Register-Foreign-Device 전송 직후(CONNECTING)에도 Reliability를
 * 정상으로 보고하므로(dl/bip/subdlbip.h FD_CAN_SEND), 기동 후 첫 등록 응답 대기
 * 시간(grace_ms) 동안은 Registered 판정을 보류합니다.
 *
 * @param elapsed_ms 모니터 시작 후 경과 시간(ms)
 * @param grace_ms   첫 등록 응답 대기 시간(ms, fd.reg_timeout)
 */
inline State next(State previous, bool read_ok, uint32_t reliability,
                  uint32_t elapsed_ms, uint32_t grace_ms)
{
    if (!read_ok) {
        return previous;
    }
    if (reliability == kRenewFdRegistrationFailure) {
        return State::Failed;
    }
    if (previous == State::Unknown && elapsed_ms < grace_ms) {
        return State::Unknown;
    }
    return State::Registered;
}

/// TTL이 이 값(초)을 넘으면 이 주기로 재등록해 BBMD 연결을 확인한다.
static constexpr uint32_t kMaxRenewIntervalSec = 300U;

/**
 * @brief 게이트웨이가 직접 재등록(RENEW_FD_REGISTRATION)해야 하는지 판단합니다.
 *        TTL이 5분 이하이면 BACstac이 TTL 주기로 재등록하므로 직접 보내지 않습니다.
 * @param ttl_sec           FD_Subscription_Lifetime (초)
 * @param since_renew_ms    마지막 등록(또는 모니터 시작) 후 경과 시간(ms)
 */
inline bool shouldRenew(uint32_t ttl_sec, uint32_t since_renew_ms)
{
    return ttl_sec > kMaxRenewIntervalSec && since_renew_ms >= kMaxRenewIntervalSec * 1000U;
}

/// 상태가 바뀌어 연결/끊김 메시지를 출력해야 하는지 판단합니다.
inline bool shouldReport(State previous, State current)
{
    return current != State::Unknown && current != previous;
}

} // namespace fd_link_state
} // namespace gateway
