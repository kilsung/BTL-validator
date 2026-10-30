#pragma once

/**
 * @file cov_manager.h
 * @brief COV(Change of Value) 구독 관리 전담 클래스
 *
 * 책임:
 *  - COV 구독자 테이블 관리 (subscribe / unsubscribe)
 *  - COV 폴링 스레드 (동적 구독자 기반 Modbus 폴링 + 변화 감지)
 *  - COVNotification 전송 (Confirmed / Unconfirmed)
 *
 * 의존성: types.h, ModbusClient
 */

#include "types.h"
#include "modbus_client.h"
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <bacsrv.h>

namespace gateway {

/**
 * @brief COV 구독 관리 및 폴링 전담 클래스.
 */
class CovManager {
public:
    /**
     * @brief 생성자.
     * @param modbus      Modbus 클라이언트 참조 (background read 제출 전용)
     * @param poll_ms     COV 폴링 최소 주기 (ms)
     * @param enable_log  진단 로그 활성화 여부
     */
    CovManager(ModbusClient& modbus, int poll_ms, bool enable_log);
    ~CovManager();

    // 복사/이동 금지
    CovManager(const CovManager&)            = delete;
    CovManager& operator=(const CovManager&) = delete;
    CovManager(CovManager&&)                 = delete;
    CovManager& operator=(CovManager&&)      = delete;

    /**
     * @brief 로컬 BACnet 디바이스 핸들을 설정합니다.
     * COV 알림 전송(BACstacConfCOVNotification 등)에 사용됩니다.
     * start() 호출 전에 설정해야 합니다.
     */
    void setLocalDevHandle(BACSTAC_HDEVICE hDev) { hLocalDev_ = hDev; }

    /**
     * @brief COV 폴링 스레드를 시작합니다.
     */
    void start();

    /**
     * @brief COV 폴링 스레드를 종료합니다.
     */
    void stop();

    static constexpr size_t kMaxCovSubscriptions = 20;

    /**
     * @brief 신규 COV 구독 등록이 가능한지 용량을 확인합니다.
     * 이미 등록된 동일 구독자(재구독/갱신)인 경우 true를 반환합니다.
     */
    bool canSubscribe(const ObjectMapping* mapping, BACSTAC_UI32 processID, BACSTAC_PROPERTY_ID propID = PROP_PRESENT_VALUE);

    /**
     * @brief COV 구독을 등록하거나 갱신합니다.
     *
     * @param mapping  구독 대상 ObjectMapping
     * @param sub      구독자 정보
     * @return true if this is a new subscription (not a re-subscription)
     */
    bool subscribe(ObjectMapping* mapping, const CovSubscriber& sub);

    /**
     * @brief COV 구독을 취소합니다.
     *
     * @param mapping    구독 대상 ObjectMapping
     * @param processID  취소할 구독자 ID
     * @param propID     취소할 프로퍼티 ID (선택, 기본값: PROP_PRESENT_VALUE)
     */
    void unsubscribe(ObjectMapping* mapping, BACSTAC_UI32 processID, BACSTAC_PROPERTY_ID propID = PROP_PRESENT_VALUE);

    /**
     * @brief 활성 구독자 목록에 지정된 ObjectMapping 이 있는지 확인합니다.
     */
    bool hasSubscribers(const ObjectMapping* mapping) const;

    /**
     * @brief 단일 포인트의 모든 구독자에게 COVNotification 을 전송합니다.
     *
     * @param mapping  변화가 감지된 포인트
     * @param new_val  새로운 값
     */
    void sendNotification(ObjectMapping* mapping, double new_val);

    /**
     * @brief 신규 구독자에게 단독으로 초기 COVNotification 을 전송합니다.
     *
     * @param mapping   구독 대상 포인트
     * @param sub       새로 등록된 구독자
     * @param init_val  현재 초기값
     */
    void sendInitialNotification(ObjectMapping* mapping, const CovSubscriber& sub, double init_val);

    /**
     * @brief 현재 활성화된 모든 COV 구독자 목록을 반환합니다.
     * Device 객체의 Active_COV_Subscriptions 프로퍼티 응답에 사용됩니다.
     */
    std::vector<ActiveCovSubscriptionInfo> getActiveSubscriptions();

private:
    /// 폴링 스레드 본체
    void pollingThread();

    /// Modbus 폴링 결과를 COV 초기화/변화 감지/Notification으로 처리합니다.
    void processPolledValue(ObjectMapping* mapping, double new_val);

    /// 만료된 구독자 제거 (lock 보유 상태에서 호출)
    void pruneExpiredSubscribers(std::vector<CovSubscriber>& subs);

    ModbusClient&   modbus_;
    int             poll_ms_;
    bool            enable_log_;
    BACSTAC_HDEVICE hLocalDev_ = nullptr;

    std::map<ObjectMapping*, std::vector<CovSubscriber>> subscribers_;
    mutable std::mutex subscribers_mutex_;

    std::thread      poll_thread_;
    std::atomic<bool> running_{false};

    // LOW Modbus 요청은 완료될 때까지 하나만 outstanding 상태로 유지합니다.
    std::shared_ptr<ModbusReadHandle> pending_poll_;
    ObjectMapping*                   pending_mapping_ = nullptr;
};

} // namespace gateway
