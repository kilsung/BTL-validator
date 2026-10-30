/**
 * @file cov_manager.cpp
 * @brief COV 구독 관리 및 폴링 구현
 */

#include "cov_manager.h"
#include "modbus_client.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <thread>
#include <bacsrv.h>
#include <baccomm.h>

namespace gateway {

// ---------------------------------------------------------------------------
// 생성자 / 소멸자
// ---------------------------------------------------------------------------

CovManager::CovManager(ModbusClient& modbus, int poll_ms, bool enable_log)
    : modbus_(modbus)
    , poll_ms_(poll_ms)
    , enable_log_(enable_log)
{
}

CovManager::~CovManager()
{
    stop();
}

// ---------------------------------------------------------------------------
// start() / stop()
// ---------------------------------------------------------------------------

void CovManager::start()
{
    running_    = true;
    poll_thread_ = std::thread(&CovManager::pollingThread, this);
}

void CovManager::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    if (poll_thread_.joinable()) {
        poll_thread_.join();
    }
    pending_poll_.reset();
    pending_mapping_ = nullptr;
}

// ---------------------------------------------------------------------------
// subscribe() / unsubscribe()
// ---------------------------------------------------------------------------

bool CovManager::canSubscribe(const ObjectMapping* mapping, BACSTAC_UI32 processID, BACSTAC_PROPERTY_ID propID)
{
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto itMap = subscribers_.find(const_cast<ObjectMapping*>(mapping));
    if (itMap != subscribers_.end()) {
        auto& subs = itMap->second;
        pruneExpiredSubscribers(subs);
        auto it = std::find_if(subs.begin(), subs.end(),
            [&](const CovSubscriber& s) {
                return s.processID == processID && s.propID == propID;
            });
        if (it != subs.end()) {
            return true; // 재구독(갱신)은 슬롯을 새로 차지하지 않으므로 허용
        }
    }

    size_t total_active = 0;
    for (auto& kv : subscribers_) {
        pruneExpiredSubscribers(kv.second);
        total_active += kv.second.size();
    }
    return total_active < kMaxCovSubscriptions;
}

bool CovManager::subscribe(ObjectMapping* mapping, const CovSubscriber& sub)
{
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto& subs = subscribers_[mapping];
    pruneExpiredSubscribers(subs);

    auto it = std::find_if(subs.begin(), subs.end(),
        [&](const CovSubscriber& s) {
            return s.processID == sub.processID && s.propID == sub.propID;
        });

    if (it != subs.end()) {
        *it = sub; // 재구독: 갱신
        return false;
    }

    size_t total_active = 0;
    for (auto& kv : subscribers_) {
        if (kv.first != mapping) {
            pruneExpiredSubscribers(kv.second);
        }
        total_active += kv.second.size();
    }
    if (total_active >= kMaxCovSubscriptions) {
        return false;
    }

    subs.push_back(sub);
    return true; // 신규 구독
}

void CovManager::unsubscribe(ObjectMapping* mapping, BACSTAC_UI32 processID, BACSTAC_PROPERTY_ID propID)
{
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto& subs = subscribers_[mapping];
    subs.erase(
        std::remove_if(subs.begin(), subs.end(),
            [processID, propID](const CovSubscriber& s) {
                return s.processID == processID && (propID == PROP_PRESENT_VALUE || s.propID == propID);
            }),
        subs.end());
}

bool CovManager::hasSubscribers(const ObjectMapping* mapping) const
{
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto it = subscribers_.find(const_cast<ObjectMapping*>(mapping));
    return (it != subscribers_.end() && !it->second.empty());
}

std::vector<ActiveCovSubscriptionInfo> CovManager::getActiveSubscriptions()
{
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<ActiveCovSubscriptionInfo> result;

    for (auto& kv : subscribers_) {
        ObjectMapping* mapping = kv.first;
        auto& subs = kv.second;
        pruneExpiredSubscribers(subs);

        for (const auto& sub : subs) {
            ActiveCovSubscriptionInfo info;
            info.destAddress = sub.destAddress;
            info.processID   = sub.processID;
            info.objType     = static_cast<BACSTAC_OBJECT_TYPE>(mapping->bacnet_type);
            info.instance    = mapping->instance;
            info.propID      = sub.propID;
            info.arrayIndex  = sub.arrayIndex;
            info.confirmed   = (sub.confirmed != 0);

            if (sub.lifetimeSec == 0U) {
                info.timeRemaining = BACSTAC_INFINITE_LIFETIME;
            } else {
                auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    now - sub.subscribeTime).count();
                long remaining = static_cast<long>(sub.lifetimeSec) - elapsed_sec;
                info.timeRemaining = (remaining > 0) ? static_cast<uint32_t>(remaining) : 0U;
            }

            const bool is_numeric = (mapping->bacnet_type == OBJ_ANALOG_INPUT ||
                                     mapping->bacnet_type == OBJ_ANALOG_OUTPUT ||
                                     mapping->bacnet_type == OBJ_ANALOG_VALUE);
            if (sub.hasIncrement) {
                info.covIncrement = sub.covIncrement;
                info.hasIncrement = true;
            } else if (is_numeric && sub.propID == PROP_PRESENT_VALUE) {
                info.covIncrement = static_cast<float>(mapping->cov_increment);
                info.hasIncrement = true;
            } else {
                info.covIncrement = 0.0f;
                info.hasIncrement = false;
            }

            result.push_back(info);
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// pruneExpiredSubscribers() (lock 보유 상태에서 호출)
// ---------------------------------------------------------------------------

void CovManager::pruneExpiredSubscribers(std::vector<CovSubscriber>& subs)
{
    auto now = std::chrono::steady_clock::now();
    subs.erase(
        std::remove_if(subs.begin(), subs.end(),
            [&now](const CovSubscriber& s) {
                if (s.lifetimeSec == 0U) { return false; }
                auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                    now - s.subscribeTime).count();
                return elapsed_sec >= static_cast<long>(s.lifetimeSec);
            }),
        subs.end());
}

// ---------------------------------------------------------------------------
// sendNotification()
// ---------------------------------------------------------------------------

static void BACstac_callback onConfCovComplete(
    BACSTAC_HTSM           hTransaction,
    const BACSTAC_ADDRESS* pSourceAddress,
    const BACSTAC_ADDRESS* pDestinationAddress,
    BACSTAC_STATUS         status,
    BACSTAC_ERROR*         pError)
{
    (void)hTransaction;
    (void)pSourceAddress;
    (void)pDestinationAddress;
    (void)status;
    (void)pError;
}

void CovManager::sendNotification(ObjectMapping* mapping, double new_val)
{
    if (hLocalDev_ == nullptr || mapping->hObject == nullptr) { return; }

    // DCC 상태 확인: DISABLE 또는 DISABLE_INITIATION 시 전송 금지
    BACSTAC_DCC_VALUE dcc_val = DCC_ENABLE;
    if (BACstacGetDCCValue(hLocalDev_, &dcc_val) && dcc_val != DCC_ENABLE) {
        if (enable_log_) {
            std::cout << "[" << ModbusClient::getMsTimestamp()
                      << "][COV] Suppressed COV notification for '"
                      << mapping->name << "' due to active DCC state ("
                      << static_cast<int>(dcc_val) << ")" << std::endl;
        }
        return;
    }

    const bool is_binary = (mapping->bacnet_type == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
                            mapping->bacnet_type == OBJ_BINARY_OUTPUT ||
#endif
                            mapping->bacnet_type == OBJ_BINARY_VALUE);

    // listOfValues 조립: PRESENT_VALUE(85) 하나만 포함
    BACSTAC_PROPERTY_VALUE pv;
    BACSTAC_REAL  analog_val = static_cast<BACSTAC_REAL>(new_val);
    BACSTAC_ENUM  binary_val = (new_val != 0.0) ? BACSTAC_BINARY_ACTIVE : BACSTAC_BINARY_INACTIVE;

    pv.propID   = PROP_PRESENT_VALUE;
    pv.index    = BACSTAC_VOID_INDEX;
    pv.priority = BACSTAC_VOID_PRI;

    if (is_binary) {
        // Polarity 적용
        BACSTAC_POLARITY polarity = POLARITY_NORMAL;
        BACSTAC_ENUM pol_val = 0;
        BACSTAC_PROPERTY_CONTENTS pcPol;
        BACstacInitContentsToVar(&pcPol, DATA_TYPE_ENUM, &pol_val, sizeof(pol_val));
        if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_POLARITY,
                                             BACSTAC_VOID_INDEX, &pcPol, nullptr) == BACSTAC_STATUS_OK) {
            polarity = static_cast<BACSTAC_POLARITY>(pol_val);
        }
        bool is_active = (new_val != 0.0);
        if (polarity == POLARITY_REVERSE) { is_active = !is_active; }
        binary_val = is_active ? BACSTAC_BINARY_ACTIVE : BACSTAC_BINARY_INACTIVE;
        BACstacInitContentsToVar(&pv.value, DATA_TYPE_ENUM, &binary_val, sizeof(binary_val));
    } else {
        BACstacInitContentsToVar(&pv.value, DATA_TYPE_REAL, &analog_val, sizeof(analog_val));
    }

    std::vector<CovSubscriber> targets;
    auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        auto& subs = subscribers_[mapping];
        pruneExpiredSubscribers(subs);
        targets = subs;
    }

    for (const auto& sub : targets) {
        BACSTAC_UNSIGNED time_remaining = 0U;
        if (sub.lifetimeSec != 0U) {
            auto elapsed_sec = std::chrono::duration_cast<std::chrono::seconds>(
                now - sub.subscribeTime).count();
            long remaining = static_cast<long>(sub.lifetimeSec) - elapsed_sec;
            time_remaining = (remaining > 0) ? static_cast<BACSTAC_UNSIGNED>(remaining) : 0U;
        }

        if (sub.confirmed) {
            BACSTAC_COV_NOTIF_INFO covInfo;
            covInfo.processID = sub.processID;
            covInfo.initDeviceID.type = OBJ_DEVICE;
            BACSTAC_INST_NUMBER devInst = 0;
            (void)BACstacGetDeviceInstNum(hLocalDev_, &devInst);
            covInfo.initDeviceID.instNumber = devInst;
            (void)BACstacGetObjectID(mapping->hObject, &covInfo.monitoredObjectID);
            covInfo.timeRemaining = time_remaining;
            covInfo.listOfValues = &pv;
            covInfo.nValueCount = 1;

            const BACSTAC_STATUS st = BACstacConfCOVNotificationEx(
                nullptr, &sub.destAddress, &covInfo, onConfCovComplete, nullptr, nullptr, nullptr);
            if (enable_log_) {
                std::cout << "[" << ModbusClient::getMsTimestamp()
                          << "][COV TX] ConfCOVNotif '" << mapping->name
                          << "' val=" << new_val << " status=" << st << std::endl;
            }
        } else {
            BACSTAC_COV_NOTIF_INFO covInfo;
            covInfo.processID = sub.processID;
            covInfo.initDeviceID.type = OBJ_DEVICE;
            BACSTAC_INST_NUMBER devInst = 0;
            (void)BACstacGetDeviceInstNum(hLocalDev_, &devInst);
            covInfo.initDeviceID.instNumber = devInst;
            (void)BACstacGetObjectID(mapping->hObject, &covInfo.monitoredObjectID);
            covInfo.timeRemaining = time_remaining;
            covInfo.listOfValues = &pv;
            covInfo.nValueCount = 1;

            const BACSTAC_STATUS st = BACstacUnconfCOVNotificationEx(
                nullptr, &sub.destAddress, &covInfo, nullptr);
            if (enable_log_) {
                std::cout << "[" << ModbusClient::getMsTimestamp()
                          << "][COV TX] UnconfCOVNotif '" << mapping->name
                          << "' val=" << new_val << " status=" << st << std::endl;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// sendInitialNotification() (신규 구독자 1명에게만 초기값 발송)
// ---------------------------------------------------------------------------

void CovManager::sendInitialNotification(ObjectMapping* mapping, const CovSubscriber& sub, double init_val)
{
    if (hLocalDev_ == nullptr || mapping == nullptr || mapping->hObject == nullptr) {
        return;
    }

    const bool is_binary = (mapping->bacnet_type == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
                            mapping->bacnet_type == OBJ_BINARY_OUTPUT ||
#endif
                            mapping->bacnet_type == OBJ_BINARY_VALUE);

    BACSTAC_PROPERTY_VALUE pv;
    BACSTAC_REAL  analog_val = static_cast<BACSTAC_REAL>(init_val);
    BACSTAC_ENUM  binary_val = (init_val != 0.0) ? BACSTAC_BINARY_ACTIVE : BACSTAC_BINARY_INACTIVE;

    pv.propID   = sub.propID;
    pv.index    = sub.arrayIndex;
    pv.priority = BACSTAC_VOID_PRI;

    if (is_binary) {
        BACSTAC_POLARITY polarity = POLARITY_NORMAL;
        BACSTAC_ENUM pol_val = 0;
        BACSTAC_PROPERTY_CONTENTS pcPol;
        BACstacInitContentsToVar(&pcPol, DATA_TYPE_ENUM, &pol_val, sizeof(pol_val));
        if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_POLARITY,
                                             BACSTAC_VOID_INDEX, &pcPol, nullptr) == BACSTAC_STATUS_OK) {
            polarity = static_cast<BACSTAC_POLARITY>(pol_val);
        }
        bool is_active = (init_val != 0.0);
        if (polarity == POLARITY_REVERSE) { is_active = !is_active; }
        binary_val = is_active ? BACSTAC_BINARY_ACTIVE : BACSTAC_BINARY_INACTIVE;
        BACstacInitContentsToVar(&pv.value, DATA_TYPE_ENUM, &binary_val, sizeof(binary_val));
    } else {
        BACstacInitContentsToVar(&pv.value, DATA_TYPE_REAL, &analog_val, sizeof(analog_val));
    }

    BACSTAC_COV_NOTIF_INFO covInfo;
    covInfo.processID = sub.processID;
    covInfo.initDeviceID.type = OBJ_DEVICE;
    BACSTAC_INST_NUMBER devInst = 0;
    (void)BACstacGetDeviceInstNum(hLocalDev_, &devInst);
    covInfo.initDeviceID.instNumber = devInst;
    (void)BACstacGetObjectID(mapping->hObject, &covInfo.monitoredObjectID);
    covInfo.timeRemaining = sub.lifetimeSec;
    covInfo.listOfValues = &pv;
    covInfo.nValueCount = 1;

    if (sub.confirmed) {
        const BACSTAC_STATUS st = BACstacConfCOVNotificationEx(
            nullptr, &sub.destAddress, &covInfo, onConfCovComplete, nullptr, nullptr, nullptr);
        if (enable_log_) {
            std::cout << "[" << ModbusClient::getMsTimestamp()
                      << "][COV TX] Initial ConfCOVNotif '" << mapping->name
                      << "' val=" << init_val << " status=" << st << std::endl;
        }
    } else {
        const BACSTAC_STATUS st = BACstacUnconfCOVNotificationEx(
            nullptr, &sub.destAddress, &covInfo, nullptr);
        if (enable_log_) {
            std::cout << "[" << ModbusClient::getMsTimestamp()
                      << "][COV TX] Initial UnconfCOVNotif '" << mapping->name
                      << "' val=" << init_val << " status=" << st << std::endl;
        }
    }
}

// ---------------------------------------------------------------------------
// pollingThread()
// ---------------------------------------------------------------------------

void CovManager::processPolledValue(ObjectMapping* mapping, double new_val)
{
    if (mapping == nullptr) {
        return;
    }

    // 최초 폴링: 값 저장만 하고 알림 미전송 (구독 시점에 이미 전송됨)
    if (!mapping->cov_initialized) {
        mapping->cov_last_value  = new_val;
        mapping->cov_initialized = true;
        if (enable_log_) {
            std::cout << "[" << ModbusClient::getMsTimestamp()
                      << "][COV] Initialized '" << mapping->name
                      << "' init_val=" << new_val << std::endl;
        }
        return;
    }

    // 변화 감지
    bool changed = false;
    const bool is_binary = (mapping->bacnet_type == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
                            mapping->bacnet_type == OBJ_BINARY_OUTPUT ||
#endif
                            mapping->bacnet_type == OBJ_BINARY_VALUE);
    if (is_binary) {
        changed = (new_val != mapping->cov_last_value);
    } else {
        changed = (std::abs(new_val - mapping->cov_last_value) >= mapping->cov_increment);
    }

    if (!changed) {
        return;
    }

    if (enable_log_) {
        std::cout << "[" << ModbusClient::getMsTimestamp()
                  << "][COV] Change detected '" << mapping->name
                  << "': " << mapping->cov_last_value << " -> " << new_val
                  << " (threshold=" << mapping->cov_increment << ")" << std::endl;
    }

    mapping->cov_last_value = new_val;
    sendNotification(mapping, new_val);
}

void CovManager::pollingThread()
{
    std::cout << "[COV] Dynamic polling thread started." << std::endl;

    while (running_) {
        // 이전 LOW 요청이 완료되기 전에는 새 요청을 제출하지 않습니다.
        if (pending_poll_) {
            if (!pending_poll_->isComplete()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            ObjectMapping* mapping = pending_mapping_;
            double new_val = 0.0;
            const bool ok = pending_poll_->getResult(new_val);
            pending_poll_.reset();
            pending_mapping_ = nullptr;

            if (ok) {
                processPolledValue(mapping, new_val);
            }
            continue;
        }

        // DCC 상태 확인: DISABLE 활성 시 폴링 일시 정지
        BACSTAC_DCC_VALUE dcc_val = DCC_ENABLE;
        if (hLocalDev_ != nullptr &&
            BACstacGetDCCValue(hLocalDev_, &dcc_val) &&
            dcc_val != DCC_ENABLE)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // 활성 구독자가 있는 포인트 목록 수집
        std::vector<ObjectMapping*> active_targets;
        {
            std::lock_guard<std::mutex> lock(subscribers_mutex_);
            for (auto& kv : subscribers_) {
                ObjectMapping* mapping = kv.first;
                auto& subs = kv.second;
                pruneExpiredSubscribers(subs);
                if (!subs.empty()) {
                    active_targets.push_back(mapping);
                }
            }
        }

        if (active_targets.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        for (auto* mapping : active_targets) {
            if (!running_) { break; }

            auto now = std::chrono::steady_clock::now();

            // COV 내부 스킵 캐시: 설정된 poll_ms_ 미만이면 스킵
            if (mapping->cov_initialized) {
                auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - mapping->cov_last_poll_time).count();
                if (elapsed_ms < poll_ms_) { continue; }
            }

            // Out_Of_Service 확인
            BACSTAC_BOOLEAN out_of_service = BACSTAC_FALSE;
            {
                BACSTAC_ENUM oos_val = 0;
                BACSTAC_PROPERTY_CONTENTS pcOOS;
                BACstacInitContentsToVar(&pcOOS, DATA_TYPE_BOOLEAN, &oos_val, sizeof(oos_val));
                if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_OUT_OF_SERVICE,
                                                     BACSTAC_VOID_INDEX, &pcOOS, nullptr)
                    == BACSTAC_STATUS_OK)
                {
                    out_of_service = static_cast<BACSTAC_BOOLEAN>(oos_val);
                }
            }

            double new_val = 0.0;
            if (out_of_service == BACSTAC_TRUE) {
                // Out_Of_Service 모드: DB 에서 Present_Value 읽기
                BACSTAC_PROPERTY_CONTENTS pcPV;
                if (mapping->bacnet_type == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
                    mapping->bacnet_type == OBJ_BINARY_OUTPUT ||
#endif
                    mapping->bacnet_type == OBJ_BINARY_VALUE)
                {
                    BACSTAC_ENUM pv_val = 0;
                    BACstacInitContentsToVar(&pcPV, DATA_TYPE_ENUM, &pv_val, sizeof(pv_val));
                    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRESENT_VALUE,
                                                         BACSTAC_VOID_INDEX, &pcPV, nullptr)
                        == BACSTAC_STATUS_OK)
                    {
                        new_val = (pv_val != 0) ? 1.0 : 0.0;
                    }
                } else {
                    BACSTAC_REAL pv_val = 0.0f;
                    BACstacInitContentsToVar(&pcPV, DATA_TYPE_REAL, &pv_val, sizeof(pv_val));
                    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRESENT_VALUE,
                                                         BACSTAC_VOID_INDEX, &pcPV, nullptr)
                        == BACSTAC_STATUS_OK)
                    {
                        new_val = static_cast<double>(pv_val);
                    }
                }
                mapping->cov_last_poll_time = std::chrono::steady_clock::now();
            } else {
                // 정상 모드: LOW 우선순위 Job 을 통해 Modbus 폴링
                mapping->cov_last_poll_time = std::chrono::steady_clock::now();

                if (!running_) { break; }

                // 완료 전까지 Handle을 보유하여 LOW Job 중복 제출을 방지합니다.
                pending_poll_ = modbus_.submitRead(*mapping, ModbusJobPriority::LOW);
                if (!pending_poll_) {
                    continue;
                }
                pending_mapping_ = mapping;
                break;
            }

            processPolledValue(mapping, new_val);
        }

        // CPU 점유 방지 및 스레드 락 경합 완화
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "[COV] Dynamic polling thread stopped." << std::endl;
}

} // namespace gateway
