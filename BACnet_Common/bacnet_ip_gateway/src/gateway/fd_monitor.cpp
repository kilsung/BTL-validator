#include "fd_monitor.h"
#include "modbus_client.h"

#include <bactypes.h>
#include <bacacc.h>
#include <baccli.h>
#include <bacdb.h>

#include <chrono>
#include <cstring>
#include <iostream>

namespace gateway {

namespace {

static constexpr BACSTAC_INST_NUMBER kNetworkPortInstance = 1U;
static constexpr int kPollIntervalMs = 1000;
static constexpr int kPollSliceMs    = 100;

bool sameAddress(const BACSTAC_ADDRESS* lhs, const BACSTAC_ADDRESS* rhs)
{
    const BACSTAC_SIZE_T lhsLen = BACstacGetAddrMACLength(lhs);
    if (lhsLen != BACstacGetAddrMACLength(rhs) ||
        BACstacGetAddrNetworkNumber(lhs) != BACstacGetAddrNetworkNumber(rhs)) {
        return false;
    }
    const BACSTAC_BYTE* lhsMac = BACstacGetAddrMACPtr(lhs);
    const BACSTAC_BYTE* rhsMac = BACstacGetAddrMACPtr(rhs);
    return lhsMac != nullptr && rhsMac != nullptr && std::memcmp(lhsMac, rhsMac, lhsLen) == 0;
}

} // namespace

std::atomic<bool> FdMonitor::self_renew_in_progress_{false};

bool FdMonitor::isSelfRenewRequest(const BACSTAC_ADDRESS* source)
{
    if (!self_renew_in_progress_.load()) {
        return false;
    }
    // 로컬 요청은 출처 주소가 비어 있거나 로컬 주소로 전달된다. 그 외 출처는 외부 요청이다.
    if (source == nullptr || BACstacIsAddrLocal(source)) {
        return true;
    }
    BACSTAC_ADDRESS localAddress;
    return BACstacGetLocalBACnetAddress(&localAddress) && sameAddress(source, &localAddress);
}

FdMonitor::~FdMonitor()
{
    stop();
}

void FdMonitor::start(const std::string& bbmd_ip, uint16_t bbmd_port, uint16_t ttl,
                      uint32_t retry_interval_ms)
{
    if (running_.exchange(true)) {
        return;
    }
    bbmd_ip_           = bbmd_ip;
    bbmd_port_         = bbmd_port;
    ttl_               = ttl;
    retry_interval_ms_ = retry_interval_ms;
    std::cout << "[FD] Foreign Device mode. Connecting to BBMD " << bbmd_ip_ << ':' << bbmd_port_
              << " (TTL=" << ttl_ << "s)..." << std::endl;
    thread_ = std::thread(&FdMonitor::run, this);
}

void FdMonitor::stop()
{
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

bool FdMonitor::readReliability(uint32_t& reliability)
{
    BACSTAC_ADDRESS localAddress;
    if (!BACstacGetLocalBACnetAddress(&localAddress)) {
        return false;
    }

    BACSTAC_OBJECT_ID objectId;
    BACSTAC_READ_INFO readInfo;
    if (!BACstacInitObjectID(&objectId, OBJ_NETWORK_PORT, kNetworkPortInstance) ||
        !BACstacInitReadInfo(&readInfo, &objectId, PROP_RELIABILITY)) {
        return false;
    }

    BACSTAC_ENUM value = 0U;
    BACSTAC_PROPERTY_CONTENTS contents;
    BACstacInitContentsToVar(&contents, DATA_TYPE_ENUM, &value, sizeof(value));
    BACSTAC_ERROR error;
    // 자기 장치로의 요청은 BACstac TSM이 허용한다(tsml/tl0.c "allow requests to itself").
    const BACSTAC_STATUS status = BACstacReadPropertyEx2(
        nullptr, &localAddress, &readInfo, nullptr, nullptr, nullptr,
        &contents, &error, nullptr);
    if (status != BACSTAC_STATUS_OK) {
        return false;
    }
    reliability = static_cast<uint32_t>(BACstacGetContentsEnum(&contents));
    return true;
}

bool FdMonitor::renewRegistration(int& status_out)
{
    status_out = -1;
    BACSTAC_ADDRESS localAddress;
    if (!BACstacGetLocalBACnetAddress(&localAddress)) {
        return false;
    }

    BACSTAC_OBJECT_ID objectId;
    BACSTAC_ENUM command = static_cast<BACSTAC_ENUM>(BNPC_RENEW_FD_REGISTRATION);
    BACSTAC_PROPERTY_CONTENTS contents;
    BACstacInitContentsToVar(&contents, DATA_TYPE_ENUM, &command, sizeof(command));
    BACSTAC_WRITE_INFO writeInfo;
    if (!BACstacInitObjectID(&objectId, OBJ_NETWORK_PORT, kNetworkPortInstance) ||
        !BACstacInitWriteInfo(&writeInfo, &objectId, PROP_COMMAND, &contents)) {
        return false;
    }

    // BACstac NL이 Command를 처리해 BipFdRenewRegistration()으로 Register-Foreign-Device를 보낸다.
    self_renew_in_progress_ = true;
    BACSTAC_ERROR error;
    const BACSTAC_STATUS status = BACstacWritePropertyEx(
        nullptr, &localAddress, &writeInfo, nullptr, nullptr, nullptr, &error);
    self_renew_in_progress_ = false;
    status_out = static_cast<int>(status);
    return status == BACSTAC_STATUS_OK;
}

void FdMonitor::run()
{
    fd_link_state::State state = fd_link_state::State::Unknown;
    bool read_error_reported = false;
    const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_renew = started;
    bool renew_error_reported = false;

    while (running_.load()) {
        const uint32_t elapsed_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
        uint32_t reliability = 0U;
        const bool read_ok = readReliability(reliability);
        if (!read_ok && !read_error_reported) {
            std::cerr << "[" << ModbusClient::getMsTimestamp()
                      << "][FD] Failed to read Network Port Reliability. FD status is unknown." << std::endl;
            read_error_reported = true;
        } else if (read_ok) {
            read_error_reported = false;
        }

        const fd_link_state::State next = fd_link_state::next(
            state, read_ok, reliability, elapsed_ms, retry_interval_ms_);
        if (fd_link_state::shouldReport(state, next)) {
            if (next == fd_link_state::State::Registered) {
                std::cout << "[" << ModbusClient::getMsTimestamp() << "][FD] Registered with BBMD "
                          << bbmd_ip_ << ':' << bbmd_port_ << " (TTL=" << ttl_ << "s)" << std::endl;
            } else {
                std::cerr << "[" << ModbusClient::getMsTimestamp() << "][FD] BBMD "
                          << bbmd_ip_ << ':' << bbmd_port_
                          << " registration failed or lost. Retrying every "
                          << retry_interval_ms_ << " ms..." << std::endl;
            }
        }
        state = next;

        // TTL이 5분을 넘으면 5분마다 직접 재등록해 BBMD 연결을 확인한다.
        const uint32_t since_renew_ms = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - last_renew).count());
        // 링크 복구 직후(NetworkWatcher 요청)에는 TTL 과 무관하게 즉시 재등록한다.
        const bool link_recovered = renew_requested_.exchange(false);
        if (link_recovered) {
            std::cerr << "[" << ModbusClient::getMsTimestamp()
                      << "][FD] Network link recovered. Re-registering with BBMD "
                      << bbmd_ip_ << ':' << bbmd_port_ << " immediately." << std::endl;
        }
        if (link_recovered || fd_link_state::shouldRenew(ttl_, since_renew_ms)) {
            last_renew = std::chrono::steady_clock::now();
            int renew_status = 0;
            if (!renewRegistration(renew_status)) {
                if (!renew_error_reported) {
                    std::cerr << "[" << ModbusClient::getMsTimestamp()
                              << "][FD] Failed to send periodic Register-Foreign-Device (RENEW_FD_REGISTRATION), status="
                              << renew_status << ". Relying on TTL re-registration." << std::endl;
                    renew_error_reported = true;
                }
            } else {
                renew_error_reported = false;
            }
        }

        for (int waited = 0; waited < kPollIntervalMs && running_.load(); waited += kPollSliceMs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(kPollSliceMs));
        }
    }
}

} // namespace gateway
