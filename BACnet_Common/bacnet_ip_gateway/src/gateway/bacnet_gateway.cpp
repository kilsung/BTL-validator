/**
 * @file bacnet_gateway.cpp
 * @brief Modbus RTU ↔ BACnet IP 게이트웨이 구현부
 */

#include "bacnet_gateway.h"
#include "tpi_exporter.h"
#include "makeobj.h"
#include "../common/serial_port.h"
#include "../common/bip_config_path.h"
#include "../common/bip_config_file.h"

#include <baccomm.h>
#include <baccfg.h>
#include <bacprerr.h>
#include <bacacc.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <utility>
#include <cctype>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <linux/limits.h>
#include <unistd.h>
#include <cerrno>

using namespace modbus;

extern "C" {
    extern const struct DEVICE_TEMPLATE dummy_device;
    BACSTAC_CFG_STATUS BACstacCfgLoadBipConfig(void);
    extern BACSTAC_BIP_CFG_U bacBipCfgU;
}

namespace gateway {

namespace {

std::string trimConfigValue(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

std::string readConfiguredBipMode(const std::string& path)
{
    std::ifstream stream(path);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const std::string key = trimConfigValue(line.substr(0U, separator));
        if (key != "mode") {
            continue;
        }
        std::string mode = trimConfigValue(line.substr(separator + 1U));
        std::transform(mode.begin(), mode.end(), mode.begin(),
                       [](unsigned char character) {
                           return static_cast<char>(std::tolower(character));
                       });
        return mode;
    }
    return "normal";
}

/// bip.cfg 숫자 값 변환. 형식 오류나 범위 초과면 false.
bool parseConfigUnsigned(const std::string& text, unsigned long max_value, unsigned long& out)
{
    if (text.empty() || text[0] == '-') {
        return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0' || value > max_value) {
        return false;
    }
    out = value;
    return true;
}

// Network Port Property_List (BTL 테스터 EPICS 기준, Command(417) 제외).
// BACstac NL 계층은 Command를 항상 노출하지만(nl/nlnpo.c IsNlProperty), 이 제품은
// 네트워크 명령을 지원하지 않으므로 목록에서 제외한다.
static const BACSTAC_ENUM kNetworkPortPropertyList[] = {
    PROP_STATUS_FLAGS,
    PROP_RELIABILITY,
    PROP_OUT_OF_SERVICE,
    PROP_NETWORK_TYPE,
    PROP_PROTOCOL_LEVEL,
    PROP_NETWORK_NUMBER,
    PROP_NETWORK_NUMBER_QUALITY,
    PROP_CHANGES_PENDING,
    PROP_MAC_ADDRESS,
    PROP_APDU_LENGTH,
    PROP_LINK_SPEED,
    PROP_NETWORK_INTERFACE_NAME,
    PROP_BIP_MODE,
    PROP_IP_ADDRESS,
    PROP_BIP_UDP_PORT,
    PROP_IP_SUBNET_MASK,
    PROP_IP_DEFAULT_GATEWAY,
    PROP_IP_DNS_SERVER,
    PROP_IP_DHCP_ENABLE,
    PROP_IP_DHCP_LTIME,
    PROP_IP_DHCP_LTIME_REMAINING,
    PROP_IP_DHCP_SERVER,
    PROP_FD_BBMD_ADDRESS,
    PROP_FD_SUBSCRIPTION_LIFETIME,
    PROP_ROUTING_TABLE
};

} // namespace

// ── 내부 상수 ─────────────────────────────────────────────────────────────────
static constexpr int kBacstacMaxObjectsExtra = 5;

// Network Port 속성 중 게이트웨이가 직접 응답하는 속성(표준 번호, BACstac bnpropid.h).
// 그 외 속성(IP_Address, BACnet_IP_Mode 등)은 BACstac 기본 처리에 위임한다.
enum class NetworkPortReadKind {
    MACAddress,
    IPDNSServer,
    FDBBMDAddress,
    FDSubscriptionLifetime,
    ObjectName
};

struct NetworkPortProperty {
    uint32_t             id;
    NetworkPortReadKind  kind;
};

static const NetworkPortProperty kNetworkPortProperties[] = {
    { PROP_OBJECT_NAME,              NetworkPortReadKind::ObjectName },
    { PROP_MAC_ADDRESS,              NetworkPortReadKind::MACAddress },
    { PROP_IP_DNS_SERVER,            NetworkPortReadKind::IPDNSServer },
    { PROP_FD_BBMD_ADDRESS,          NetworkPortReadKind::FDBBMDAddress },
    { PROP_FD_SUBSCRIPTION_LIFETIME, NetworkPortReadKind::FDSubscriptionLifetime }
};

static constexpr uint32_t kNetworkPortCommandProperty = PROP_COMMAND; // 417
static constexpr uint32_t kNetworkPortChangesPendingProperty = PROP_CHANGES_PENDING; // 416

struct HookDefinition {
    BACSTAC_HOOK_TYPE      hookType;
    BACSTAC_SERVICES_BITS  serviceBit;
    BACSTAC_HOOK_PROC      proc;
    const char*            name;
};

// BACnet Device 객체에 외부로 광고할 서비스 목록이다.
// B-ASC 서버로서 수신 실행(Execute)하는 서비스만 광고한다.
// I-Am, I-Have, Confirmed/Unconfirmed COV Notification은 발신(Initiate)만 하므로 제외한다.
static const BACSTAC_SERVICES_BITS kAdvertisedServiceBits[] = {
    SVC_SUBSCRIBE_COV,
    SVC_READ_PROP,
    SVC_WRITE_PROP,
    SVC_DEVICE_COMM_CONTROL,
    SVC_WHO_HAS,
    SVC_WHO_IS
};

static constexpr BACSTAC_UINT kServiceBitCount =
    static_cast<BACSTAC_UINT>(SVC_LAST_KNOWN_BIT + 1);

static bool initializeServiceBits(
    BACSTAC_BIT_STRING* bitString,
    const BACSTAC_SERVICES_BITS* services,
    std::size_t serviceCount)
{
    if (bitString == nullptr || (services == nullptr && serviceCount != 0U)) {
        return false;
    }

    if (!BACstacInitBitString(bitString, kServiceBitCount)) {
        return false;
    }

    for (std::size_t i = 0U; i < serviceCount; ++i) {
        if (!BACstacSetBitStringBit(bitString, services[i], BACSTAC_TRUE)) {
            return false;
        }
    }

    return true;
}

static const HookDefinition kHookDefinitions[] = {
    { HOOK_READ_PROP,           SVC_READ_PROP,             reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::readPropHook),             "ReadProperty" },
    { HOOK_WRITE_PROP,          SVC_WRITE_PROP,            reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::writePropHook),            "WriteProperty" },
    { HOOK_READ_PROP_MULTIPLE,  SVC_READ_PROP_MULTIPLE,    reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::readPropMultipleHook),     "ReadPropertyMultiple" },
    { HOOK_WRITE_PROP_MULTIPLE, SVC_WRITE_PROP_MULTIPLE,   reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::writePropMultipleHook),    "WritePropertyMultiple" },
    { HOOK_SUBSCRIBE_COV,       SVC_SUBSCRIBE_COV,         reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::subscribeCOVHook),         "SubscribeCOV" },
    { HOOK_DCC,                 SVC_DEVICE_COMM_CONTROL,   reinterpret_cast<BACSTAC_HOOK_PROC>(BacnetGateway::dccHook),                  "DeviceCommunicationControl" }
};

// BACstacSetHook()은 해당 서비스 비트가 TRUE일 때만 Hook 등록을 허용한다.
// 따라서 외부 광고 목록에 없는 Reject Hook 서비스도 내부 비트에는 추가한다.
// TimeSynchronization과 UTCTimeSynchronization은 이 Gateway 구성에서
// 기본 실행 동작이 없으므로 Hook 없이 BACstac의 기본 Reject를 사용한다.
// SubscribeCOVProperty도 Hook 없이 처리하며, COVDefault가 FALSE인 현재 구성에서는
// 동일하게 BACstac의 기본 Reject 경로를 사용한다.
static bool initializeInternalServiceBits(BACSTAC_BIT_STRING* bitString)
{
    if (!initializeServiceBits(bitString,
                               kAdvertisedServiceBits,
                               sizeof(kAdvertisedServiceBits) / sizeof(kAdvertisedServiceBits[0]))) {
        return false;
    }

    for (std::size_t i = 0U;
         i < sizeof(kHookDefinitions) / sizeof(kHookDefinitions[0]);
         ++i) {
        if (!BACstacSetBitStringBit(bitString,
                                    kHookDefinitions[i].serviceBit,
                                    BACSTAC_TRUE)) {
            return false;
        }
    }

    return true;
}

static const NetworkPortProperty* findNetworkPortProperty(uint32_t propertyId)
{
    for (std::size_t i = 0; i < sizeof(kNetworkPortProperties) / sizeof(kNetworkPortProperties[0]); ++i) {
        if (kNetworkPortProperties[i].id == propertyId) {
            return &kNetworkPortProperties[i];
        }
    }
    return nullptr;
}

static std::string getMsTimestamp();

// ── 싱글턴 포인터 ─────────────────────────────────────────────────────────────
BacnetGateway* BacnetGateway::instance_ = nullptr;

// ---------------------------------------------------------------------------
// 생성자 / 소멸자
// ---------------------------------------------------------------------------

BacnetGateway::BacnetGateway()
    : BacnetGateway(std::unique_ptr<ISerialPort>(new SerialPort()))
{
}

BacnetGateway::BacnetGateway(std::unique_ptr<ISerialPort> port)
    : serial_port_(std::move(port))
{
    initialize();
}

void BacnetGateway::initialize()
{
    instance_ = this;

    const char* env_log = std::getenv("BACNET_LOG");
    enable_log_ = (env_log != nullptr && std::string(env_log) == "1");
}

BacnetGateway::~BacnetGateway()
{
    stop();
    if (instance_ == this) {
        instance_ = nullptr;
    }
}

// ---------------------------------------------------------------------------
// 설정 로드 (ConfigLoader 위임)
// ---------------------------------------------------------------------------

bool BacnetGateway::loadGatewayConfig(const std::string& gateway_config_path)
{
    ConfigLoader loader;
    return loader.loadGatewayConfig(gateway_config_path,
                                    bacnet_cfg_,
                                    modbus_cfg_,
                                    cache_sec_,
                                    cov_polling_ms_);
}

bool BacnetGateway::loadConfigDir(const std::string& config_dir,
                                  std::string*       out_error,
                                  bool*              out_is_contradiction)
{
    ConfigLoader loader;
    return loader.loadConfigDir(config_dir, profiles_, mappings_, out_error, out_is_contradiction);
}

// ---------------------------------------------------------------------------
// start()
// ---------------------------------------------------------------------------

bool BacnetGateway::start(const std::atomic<bool>* external_running)
{
    if (running_.exchange(true)) {
        return false;
    }
    external_running_ = external_running;

    std::cout << std::unitbuf; // Enable unbuffered logging

    // 1. Modbus Client 시작 (Executor 스레드 구동 포함)
    if (!startModbusClient()) {
        cleanupStartFailure(false);
        return false;
    }

    // 1.5. 기기 값 읽어서 Relinquish Default 초기값 먼저 설정 (객체 생성 전에 기입)
    if (!syncRelinquishDefaults()) {
        cleanupStartFailure(false);
        return false;
    }
    external_running_ = nullptr;

    // 2. BACstac 서버 초기화 (config 폴더 상위 폴더의 bip.cfg 로드 및 서비스 비트 활성화)
    BACSTAC_SRV_INIT srv_init;
    if (!initBACstacServer(srv_init)) {
        cleanupStartFailure(true);
        return false;
    }

    // 3. 디바이스 핸들 획득
    hLocalDev_ = BACstacGetLocalDeviceHandle();
    if (hLocalDev_ == nullptr) {
        std::cerr << "[Gateway] Failed to obtain local device handle" << std::endl;
        cleanupStartFailure(true);
        return false;
    }

    // 3.5. 디바이스 오브젝트 획득 및 속성 설정
    BACSTAC_HOBJECT hDevObj = BACstacGetDeviceObjectHandle(hLocalDev_);
    if (hDevObj != nullptr) {
        if (!configureDeviceObject(hDevObj)) {
            std::cerr << "[Gateway] Warning: Some device object properties could not be set." << std::endl;
        }
    } else {
        std::cerr << "[Gateway] Local Device Object Handle is nullptr." << std::endl;
    }

    // 3.6. APDU TSM 파라미터 안전 설정 (NO_SEGMENTATION)
    {
        BACSTAC_APDU_PROPERTIES apdu_props;
        if (BACstacGetDefaultAPDUProperties(&apdu_props)) {
            apdu_props.mask = APDU_SEGMENTATION_FLAG;
            apdu_props.eSegmentation = NO_SEGMENTATION;
            if (BACstacSetDefaultAPDUProperties(&apdu_props)) {
                std::cout << "[Gateway] Default APDU Properties set: eSegmentation=NO_SEGMENTATION" << std::endl;
            } else {
                std::cerr << "[Gateway] Failed to set Default APDU Properties!" << std::endl;
            }
        }
    }

    // 4. BACnet 객체 등록 (릴링귀시 디폴트가 채워진 상태로 생성)
    if (!registerAllObjects(hLocalDev_)) {
        cleanupStartFailure(true);
        return false;
    }

    // 5. Hook 등록
    if (!registerHooks()) {
        cleanupStartFailure(true);
        return false;
    }

    // 6. TSM 파라미터를 DB에 동기화 후, APDU 세그멘테이션 속성 최종 명시 지정
    BACstacSynchDeviceTSMParams(hLocalDev_);

    // TSM 기본 동기화(32) 후 Device Object DB에 NO_SEGMENTATION(3) 및 Max_Segments_Accepted(1) 명시 설정
    {
        BACSTAC_HOBJECT hDevObj = BACstacGetDeviceObjectHandle(hLocalDev_);
        if (hDevObj != nullptr) {
            // Segmentation_Supported (107) = NO_SEGMENTATION (3)
            BACSTAC_ENUM seg_supp = NO_SEGMENTATION;
            BACSTAC_PROPERTY_CONTENTS pc_seg;
            BACstacInitContentsToVar(&pc_seg, DATA_TYPE_ENUM, &seg_supp, sizeof(seg_supp));
            BACSTAC_STATUS st = BACstacStorePropertyInstance(hDevObj, PROP_SEGMENTATION_SUPPORTED, BACSTAC_VOID_INDEX, &pc_seg);
            if (st == BACSTAC_STATUS_OK) {
                std::cout << "[Gateway] Device APDU properties finalized: Segmentation_Supported=NO_SEGMENTATION(3)" << std::endl;
            } else {
                std::cerr << "[Gateway] Failed to finalize Device APDU properties. Status: " << st << std::endl;
            }

            // Max_Segments_Accepted (167) = 1
            BACSTAC_UNSIGNED max_seg = 1U;
            BACSTAC_PROPERTY_CONTENTS pc_max_seg;
            BACstacInitContentsToVar(&pc_max_seg, DATA_TYPE_UNSIGNED, &max_seg, sizeof(max_seg));
            st = BACstacStorePropertyInstance(hDevObj, PROP_MAX_SEGMENTS_ACCEPTED, BACSTAC_VOID_INDEX, &pc_max_seg);
            if (st == BACSTAC_STATUS_OK) {
                std::cout << "[Gateway] Device APDU properties finalized: Max_Segments_Accepted=1" << std::endl;
            } else {
                std::cerr << "[Gateway] Failed to finalize Max_Segments_Accepted. Status: " << st << std::endl;
            }
        }
    }

    BACstacUnlockData();

    BACSTAC_ADDRESS broadcastAddr;
    BACstacInitAddrGlobalBroadcast(&broadcastAddr);
    BACstacIAm(hLocalDev_, &broadcastAddr);

    // 7. COV Manager 시작
    startCovManager();

    // 8. FD 모드면 BBMD 연결 상태 모니터 시작 (재접속은 BACstac이 fd.reg_timeout 주기로 수행)
    if (bip_mode_str_ == "fd") {
        fd_monitor_ = std::unique_ptr<FdMonitor>(new FdMonitor());
        fd_monitor_->start(fd_bbmd_ip_, fd_bbmd_port_, fd_ttl_, fd_reg_timeout_ms_);
    }

#ifdef ENABLE_TPI_EXPORT
    exportCurrentDeviceTpi("current_device.tpi");
#endif

    std::cout << "[Gateway] BACnet IP Gateway started. Listening for commands..." << std::endl;
    return true;
}

bool BacnetGateway::shouldContinueRunning() const
{
    return running_.load() &&
           (external_running_ == nullptr || external_running_->load());
}

void BacnetGateway::cleanupStartFailure(bool bacstac_initialized)
{
    if (bacstac_initialized) {
        unregisterHooks();
    }

    if (cov_) {
        cov_->stop();
        cov_.reset();
    }

    if (modbus_) {
        modbus_->stop();
        if (modbus_->getPort() != nullptr) {
            modbus_->getPort()->close();
        }
        modbus_.reset();
    } else if (serial_port_) {
        serial_port_->close();
    }

    if (bacstac_initialized) {
        BACstacClose();
    }

    hLocalDev_ = nullptr;
    running_ = false;
    external_running_ = nullptr;
}

// ---------------------------------------------------------------------------
// startModbusClient()
// ---------------------------------------------------------------------------

bool BacnetGateway::startModbusClient()
{
    modbus_ = std::unique_ptr<ModbusClient>(
        new ModbusClient(std::move(serial_port_), modbus_cfg_, cache_sec_, enable_log_,
                         bacnet_cfg_.apdu_timeout_ms));
    return modbus_->start();
}

// ---------------------------------------------------------------------------
// syncRelinquishDefaults() — Modbus Main 기기로부터 Relinquish_Default 전수 동기화
// ---------------------------------------------------------------------------
// Main 응답이 없거나 특정 항목에서 멈추면 계속 재시도하며 대기하고,
// 모든 항목 동기화가 완료된 후에만 다음 단계(initBACstacServer)로 진행합니다.
// ---------------------------------------------------------------------------

bool BacnetGateway::syncRelinquishDefaults()
{
    if (mappings_.empty() || !modbus_) {
        return true;
    }

    std::vector<ObjectMapping*> target_mappings;
    for (auto* m : mappings_) {
        if (m != nullptr && isCommandableObject(m->bacnet_type)) {
            target_mappings.push_back(m);
        }
    }

    if (target_mappings.empty()) {
        return true;
    }

    std::cout << "[Gateway] Synchronizing initial Relinquish_Default from Modbus device for "
              << target_mappings.size() << " commandable objects..." << std::endl;

    std::vector<bool> synced(target_mappings.size(), false);
    size_t synced_count = 0;
    bool was_waiting = false;

    while (shouldContinueRunning() && synced_count < target_mappings.size()) {
        if (!modbus_->isConnected()) {
            if (!was_waiting) {
                std::cout << "[Gateway] Waiting for serial port connection..." << std::endl;
                was_waiting = true;
            }
            for (int k = 0; k < 5 && shouldContinueRunning(); ++k) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }

        for (size_t i = 0; i < target_mappings.size() && shouldContinueRunning(); ++i) {
            if (synced[i]) {
                continue;
            }
            ObjectMapping* mapping = target_mappings[i];

            if (mapping->has_relinquish_default) {
                // 방안 A-1: JSON에 명시된 경우 -> min/max 클램핑 후 해당 기본값을 Modbus 기기로 초기 쓰기(Write)
                const double write_val = ConfigLoader::clampRelinquishDefault(
                    *mapping, mapping->relinquish_default);
                if (!ConfigLoader::isValidBinaryRelinquishDefault(*mapping, write_val)) {
                    std::cerr << "[Gateway] Invalid Binary Output relinquish_default for '"
                              << mapping->name << "': " << write_val
                              << ". Only 0 or 1 is allowed." << std::endl;
                    return false;
                }
                mapping->relinquish_default = write_val;

                if (modbus_->writeValue(*mapping, mapping->relinquish_default)) {
                    synced[i] = true;
                    synced_count++;
                    was_waiting = false;
                    if (enable_log_) {
                        std::cout << "[" << ModbusClient::getMsTimestamp() << "][Gateway Sync] Explicit relinquish_default="
                                  << mapping->relinquish_default << " for '" << mapping->name
                                  << "' -> Written to Modbus device (" << synced_count << "/" << target_mappings.size() << ")" << std::endl;
                    }
                } else {
                    if (!was_waiting) {
                        std::cout << "[Gateway] Modbus device not responding or write failed for '"
                                  << mapping->name << "'. Retrying until device is ready..." << std::endl;
                        was_waiting = true;
                    }
                    break;
                }
            } else {
                // 방안 A-2: JSON에 생략된 경우 -> Modbus 기기에서 현재값을 읽어와(Read) Relinquish_Default로 설정
                double raw_read_val = 0.0;
                if (modbus_->readRawValue(*mapping, raw_read_val, true)) {
                    const double clamped_val = ConfigLoader::clampRelinquishDefault(
                        *mapping, raw_read_val);
                    const bool was_clamped = clamped_val != raw_read_val;

                    if (!ConfigLoader::isValidBinaryRelinquishDefault(*mapping, clamped_val)) {
                        if (!was_waiting) {
                            std::cout << "[Gateway] Invalid Binary Output value read for '"
                                      << mapping->name << "': " << clamped_val
                                      << ". Retrying until Main returns 0 or 1..." << std::endl;
                            was_waiting = true;
                        }
                        break;
                    }

                    if (was_clamped) {
                        // 클램핑으로 값이 변경된 경우: Main 기기에도 써서 세 값(기기, Relinquish_Default, Present_Value) 일치 보장
                        if (modbus_->writeValue(*mapping, clamped_val)) {
                            mapping->relinquish_default = clamped_val;
                            mapping->has_relinquish_default = true;
                            synced[i] = true;
                            synced_count++;
                            was_waiting = false;
                            if (enable_log_) {
                                std::cout << "[" << ModbusClient::getMsTimestamp() << "][Gateway Sync] Omitted relinquish_default for '"
                                          << mapping->name << "' -> Read: " << raw_read_val << ", Clamped & Written to Modbus device: " << clamped_val
                                          << " (" << synced_count << "/" << target_mappings.size() << ")" << std::endl;
                            }
                        } else {
                            if (!was_waiting) {
                                std::cout << "[Gateway] Modbus device not responding or clamped write failed for '"
                                          << mapping->name << "'. Retrying until device is ready..." << std::endl;
                                was_waiting = true;
                            }
                            break;
                        }
                    } else {
                        // 클램핑 없이 정상 범위인 경우: 읽은 값 그대로 사용
                        mapping->relinquish_default = clamped_val;
                        mapping->has_relinquish_default = true;
                        synced[i] = true;
                        synced_count++;
                        was_waiting = false;
                        if (enable_log_) {
                            std::cout << "[" << ModbusClient::getMsTimestamp() << "][Gateway Sync] Omitted relinquish_default for '"
                                      << mapping->name << "' -> Read from Modbus device: " << clamped_val
                                      << " (" << synced_count << "/" << target_mappings.size() << ")" << std::endl;
                        }
                    }
                } else {
                    if (!was_waiting) {
                        std::cout << "[Gateway] Modbus device not responding or read failed for '"
                                  << mapping->name << "'. Retrying until device is ready..." << std::endl;
                        was_waiting = true;
                    }
                    break;
                }
            }
        }

        if (synced_count < target_mappings.size() && shouldContinueRunning()) {
            for (int k = 0; k < 10 && shouldContinueRunning(); ++k) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }

    if (!shouldContinueRunning()) {
        std::cout << "[Gateway] Initialization interrupted by user." << std::endl;
        return false;
    }

    std::cout << "[Gateway] Successfully synchronized all " << synced_count
              << " Relinquish_Default values from Modbus device." << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// initBACstacServer()
// ---------------------------------------------------------------------------

bool BacnetGateway::initBACstacServer(BACSTAC_SRV_INIT& srv_init)
{
    database_revision_ = bacnet_cfg_.database_revision;
    std::cout << "[Gateway] Initializing BACstac Server (Device ID: " << bacnet_cfg_.device_id
              << ", Database_Revision: " << database_revision_ << ")..." << std::endl;

    if (!BACstacInitSrvInit(&srv_init,
                            BACSTAC_VERSION_MAJOR,
                            BACSTAC_VERSION_MINOR,
                            &dummy_device,
                            static_cast<BACSTAC_INST_NUMBER>(bacnet_cfg_.device_id)))
    {
        std::cerr << "[Gateway] BACstacInitSrvInit failed!" << std::endl;
        return false;
    }

    const int max_objects = static_cast<int>(mappings_.size()) + kBacstacMaxObjectsExtra;
    BACstacSetSrvInitMaxObjects(&srv_init, static_cast<BACSTAC_OBJECT_COUNT>(max_objects));
    BACstacSetSrvInitMaxCovSubs(&srv_init, 20);

    // config 폴더의 상위 폴더에 있는 B/IP 설정 파일을 로드한다.
    // 절대경로 config 폴더는 현재 작업 폴더와 무관하므로 getcwd() 실패 시에도 진행한다.
    char cwd[PATH_MAX];
    const bool has_cwd = getcwd(cwd, sizeof(cwd)) != nullptr;
    if (!has_cwd && (config_dir_.empty() || config_dir_[0] != '/')) {
        std::cerr << "[Gateway] Failed to get current working directory for bip.cfg path!" << std::endl;
        return false;
    }
    const std::string bip_config_path =
        bip_config_path::resolve(config_dir_, has_cwd ? std::string(cwd) : std::string());
    setenv("BACSTAC_BIP_CONFIG", bip_config_path.c_str(), 1);
    std::cout << "[Gateway] Setting BACSTAC_BIP_CONFIG to: " << bip_config_path << std::endl;
    bip_config_path_ = bip_config_path;

    // FD 속성은 Normal 모드에서도 읽고 쓸 수 있어야 하므로 bip.cfg.fd를 항상 로드한다.
    loadFdConfig();

    if (BACstacCfgLoadBipConfig() != BCS_OK) {
        std::cerr << "[Gateway] Failed to load BIP configuration (" << bip_config_path << ")!" << std::endl;
        return false;
    }

    // BACstac exposes foreign-device state directly, but BBMD mode is a
    // configuration-level choice.  Preserve that choice for the Network Port
    // IP_Mode property instead of reporting every non-FD mode as normal.
    const std::string configured_bip_mode = readConfiguredBipMode(bip_config_path);

    if (bacBipCfgU.foreignDevice == BACSTAC_TRUE) {
        bip_mode_str_ = "fd";
        bip_udp_port_ = static_cast<uint16_t>(bacBipCfgU.fd.udp);
        if (bacBipCfgU.fd.ip != nullptr && bacBipCfgU.fd.ip[0] != '\0') {
            ip_address_str_ = reinterpret_cast<const char*>(bacBipCfgU.fd.ip);
        }
        if (bacBipCfgU.fd.fdBbmdIp != nullptr && bacBipCfgU.fd.fdBbmdIp[0] != '\0') {
            fd_bbmd_ip_ = reinterpret_cast<const char*>(bacBipCfgU.fd.fdBbmdIp);
        }
        fd_bbmd_port_ = static_cast<uint16_t>(bacBipCfgU.fd.fdBbmdUdp);
        fd_ttl_       = static_cast<uint16_t>(bacBipCfgU.fd.fdTimeToLive);
    } else {
        bip_mode_str_ = configured_bip_mode == "bbmd" ? "bbmd" : "normal";
        bip_udp_port_ = static_cast<uint16_t>(bacBipCfgU.bip.udp);
        if (bacBipCfgU.bip.ip != nullptr && bacBipCfgU.bip.ip[0] != '\0') {
            ip_address_str_  = reinterpret_cast<const char*>(bacBipCfgU.bip.ip);
        }
        if (bacBipCfgU.bip.subnet != nullptr && bacBipCfgU.bip.subnet[0] != '\0') {
            subnet_mask_str_ = reinterpret_cast<const char*>(bacBipCfgU.bip.subnet);
        }
    }

    detectNetworkInfo();

    // 서비스 지원 비트 설정.
    // Reject Hook을 사용하는 서비스는 BACstac의 Hook 등록 조건을 만족하기
    // 위해 내부 목록에 포함하지만, Device 객체 광고 목록에는 포함하지 않는다.
    BACSTAC_BIT_STRING srv_supported_bits;
    if (!initializeInternalServiceBits(&srv_supported_bits) ||
        !BACstacSetSrvInitServicesSupported(&srv_init, &srv_supported_bits)) {
        std::cerr << "[Gateway] Failed to configure BACstac service support bits!" << std::endl;
        return false;
    }

    const BACSTAC_STATUS status = BACstacServerInit(&srv_init, nullptr);
    if (status != BACSTAC_STATUS_OK) {
        std::cerr << "[Gateway] BACstacServerInit failed! Code: " << status << std::endl;
        return false;
    }


    std::cout << "[Gateway] BACstac server initialized. device_id="
              << bacnet_cfg_.device_id
              << ", max_objects=" << max_objects << std::endl;
    return true;
}

void BacnetGateway::detectNetworkInfo()
{
    // 1. MAC Address 탐지 (ip_address_str_와 일치하는 인터페이스 우선 검색)
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        std::string target_ifname;
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET) {
                continue;
            }
            if ((ifa->ifa_flags & IFF_LOOPBACK) != 0) {
                continue;
            }
            char host[INET_ADDRSTRLEN];
            auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            inet_ntop(AF_INET, &(sa->sin_addr), host, INET_ADDRSTRLEN);
            if (ip_address_str_ == host) {
                target_ifname = ifa->ifa_name;
                break;
            }
            if (target_ifname.empty()) {
                target_ifname = ifa->ifa_name;
            }
        }
        freeifaddrs(ifaddr);

        if (!target_ifname.empty()) {
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock >= 0) {
                struct ifreq ifr;
                std::memset(&ifr, 0, sizeof(ifr));
                std::strncpy(ifr.ifr_name, target_ifname.c_str(), IFNAMSIZ - 1);
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
                    std::memcpy(mac_address_, ifr.ifr_hwaddr.sa_data, 6);
                }
                close(sock);
            }
        }
    }

    // 2. DNS 서버 목록 파싱 (/etc/resolv.conf)
    dns_servers_.clear();
    std::ifstream resolv_file("/etc/resolv.conf");
    if (resolv_file.is_open()) {
        std::string line;
        while (std::getline(resolv_file, line)) {
            size_t start = line.find_first_not_of(" \t");
            if (start == std::string::npos || line[start] == '#') {
                continue;
            }
            if (line.compare(start, 10, "nameserver") == 0) {
                size_t ip_start = line.find_first_not_of(" \t", start + 10);
                if (ip_start != std::string::npos) {
                    size_t ip_end = line.find_first_of(" \t\r\n", ip_start);
                    std::string dns_ip = line.substr(ip_start, ip_end == std::string::npos ? std::string::npos : (ip_end - ip_start));
                    int o1 = 0, o2 = 0, o3 = 0, o4 = 0;
                    if (std::sscanf(dns_ip.c_str(), "%d.%d.%d.%d", &o1, &o2, &o3, &o4) == 4) {
                        dns_servers_.push_back(dns_ip);
                    }
                }
            }
        }
        resolv_file.close();
    }
}

// ---------------------------------------------------------------------------
// configureDeviceObject()
// ---------------------------------------------------------------------------

bool BacnetGateway::configureDeviceObject(BACSTAC_HOBJECT hDevObj)
{
    BACSTAC_STATUS status = BACSTAC_STATUS_OK;

    BACSTAC_CHAR_STRING name_str;
    if (BACstacInitCharStringUTF8(&name_str, bacnet_cfg_.device_name.c_str(), BACSTAC_NULL_TERM_STRING)) {
        BACSTAC_PROPERTY_CONTENTS pc_name;
        BACstacInitContentsToVar(&pc_name, DATA_TYPE_ANY_CHAR_STRING, &name_str, sizeof(name_str));
        status = BACstacStorePropertyInstance(hDevObj, PROP_OBJECT_NAME, BACSTAC_VOID_INDEX, &pc_name);
        if (status == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Device Name set to: " << bacnet_cfg_.device_name << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Device Name. Status: " << status << std::endl;
        }
    }

    BACSTAC_CHAR_STRING model_str;
    if (BACstacInitCharStringUTF8(&model_str, bacnet_cfg_.model_name.c_str(), BACSTAC_NULL_TERM_STRING)) {
        BACSTAC_PROPERTY_CONTENTS pc_model;
        BACstacInitContentsToVar(&pc_model, DATA_TYPE_ANY_CHAR_STRING, &model_str, sizeof(model_str));
        status = BACstacStorePropertyInstance(hDevObj, PROP_MODEL_NAME, BACSTAC_VOID_INDEX, &pc_model);
        if (status == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Device Model Name set to: " << bacnet_cfg_.model_name << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Device Model Name. Status: " << status << std::endl;
        }
    }

    BACSTAC_CHAR_STRING desc_str;
    if (BACstacInitCharStringUTF8(&desc_str, bacnet_cfg_.description.c_str(), BACSTAC_NULL_TERM_STRING)) {
        BACSTAC_PROPERTY_CONTENTS pc_desc;
        BACstacInitContentsToVar(&pc_desc, DATA_TYPE_ANY_CHAR_STRING, &desc_str, sizeof(desc_str));
        status = BACstacStorePropertyInstance(hDevObj, PROP_DESCRIPTION, BACSTAC_VOID_INDEX, &pc_desc);
        if (status == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Device Description set to: " << bacnet_cfg_.description << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Device Description. Status: " << status << std::endl;
        }
    }

    // Device 객체의 Protocol_Services_Supported는 TPI의 기존 값을 재사용하지
    // 않고 제품의 실제 광고 목록으로 새로 구성한다. 이렇게 해야 TPI에 남은
    // 미지원 서비스 비트가 외부에 잘못 광고되지 않는다.
    BACSTAC_BIT_STRING myServices;
    BACSTAC_PROPERTY_CONTENTS pc_svc;
    if (!initializeServiceBits(&myServices,
                               kAdvertisedServiceBits,
                               sizeof(kAdvertisedServiceBits) / sizeof(kAdvertisedServiceBits[0]))) {
        std::cerr << "[Gateway] Failed to configure Protocol_Services_Supported bits." << std::endl;
        return false;
    }

    BACstacInitContentsToVar(&pc_svc, DATA_TYPE_BIT_STRING, &myServices, sizeof(myServices));
    status = BACstacStorePropertyInstance(hDevObj, PROP_PROT_SERVICES_SUPPORTED,
                                          BACSTAC_VOID_INDEX, &pc_svc);
    if (status == BACSTAC_STATUS_OK) {
        std::cout << "[Gateway] Protocol_Services_Supported configured. Status: 0" << std::endl;
    } else {
        std::cerr << "[Gateway] Failed to set Protocol_Services_Supported. Status: " << status << std::endl;
    }

    // Device 객체의 Protocol_Object_Types_Supported를 로드된 실제 객체 구성에 맞춰 동적 설정
    {
        bool has_ai = false;
        bool has_ao = false;
        bool has_bi = false;
#ifdef BINARY_OUTPUT_SUPPORT
        bool has_bo = false;
#endif
        bool has_bv = false;
        for (const auto& obj : mappings_) {
            if (obj != nullptr) {
                if (obj->bacnet_type == OBJ_ANALOG_INPUT)  has_ai = true;
                if (obj->bacnet_type == OBJ_ANALOG_OUTPUT) has_ao = true;
                if (obj->bacnet_type == OBJ_BINARY_INPUT)  has_bi = true;
#ifdef BINARY_OUTPUT_SUPPORT
                if (obj->bacnet_type == OBJ_BINARY_OUTPUT) has_bo = true;
#endif
                if (obj->bacnet_type == OBJ_BINARY_VALUE)  has_bv = true;
            }
        }

        BACSTAC_BIT_STRING objTypes;
        BACstacInitBitString(&objTypes, static_cast<BACSTAC_UINT>(OBJ_LAST_KNOWN_TYPE + 1));
        BACstacSetBitStringBit(&objTypes, OBJ_DEVICE, BACSTAC_TRUE);
        BACstacSetBitStringBit(&objTypes, OBJ_NETWORK_PORT, BACSTAC_TRUE);
        if (has_ai) BACstacSetBitStringBit(&objTypes, OBJ_ANALOG_INPUT, BACSTAC_TRUE);
        if (has_ao) BACstacSetBitStringBit(&objTypes, OBJ_ANALOG_OUTPUT, BACSTAC_TRUE);
        if (has_bi) BACstacSetBitStringBit(&objTypes, OBJ_BINARY_INPUT, BACSTAC_TRUE);
#ifdef BINARY_OUTPUT_SUPPORT
        if (has_bo) BACstacSetBitStringBit(&objTypes, OBJ_BINARY_OUTPUT, BACSTAC_TRUE);
#endif
        if (has_bv) BACstacSetBitStringBit(&objTypes, OBJ_BINARY_VALUE, BACSTAC_TRUE);

        BACSTAC_PROPERTY_CONTENTS pc_obj_types;
        BACstacInitContentsToVar(&pc_obj_types, DATA_TYPE_BIT_STRING, &objTypes, sizeof(objTypes));
        status = BACstacStorePropertyInstance(hDevObj, PROP_PROT_OBJ_TYPES_SUPPORTED,
                                              BACSTAC_VOID_INDEX, &pc_obj_types);
        if (status == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Protocol_Object_Types_Supported configured dynamically (AI="
                      << (has_ai ? "T" : "F") << ", AO=" << (has_ao ? "T" : "F")
                      << ", BI=" << (has_bi ? "T" : "F")
#ifdef BINARY_OUTPUT_SUPPORT
                      << ", BO=" << (has_bo ? "T" : "F")
#endif
                      << ", BV=" << (has_bv ? "T" : "F") << ")" << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Protocol_Object_Types_Supported. Status: " << status << std::endl;
        }
    }

    // Daylight_Savings_Status = False (BACSTAC_FALSE) 설정
    {
        BACSTAC_BOOLEAN dst_status = BACSTAC_FALSE;
        BACSTAC_PROPERTY_CONTENTS pc_dst;
        BACstacInitContentsToVar(&pc_dst, DATA_TYPE_BOOLEAN, &dst_status, sizeof(dst_status));
        const BACSTAC_STATUS dst_st = BACstacStorePropertyInstance(
            hDevObj, PROP_DAYLIGHT_SAVINGS_STATUS, BACSTAC_VOID_INDEX, &pc_dst);
        if (dst_st == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Daylight_Savings_Status set to: False" << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Daylight_Savings_Status. Status: " << dst_st << std::endl;
        }
    }

    // Database_Revision 설정 (gateway.json의 bacnet.database_revision 값)
    {
        BACSTAC_UNSIGNED dbrev_val = static_cast<BACSTAC_UNSIGNED>(database_revision_);
        BACSTAC_PROPERTY_CONTENTS pc_dbrev;
        BACstacInitContentsToVar(&pc_dbrev, DATA_TYPE_UNSIGNED, &dbrev_val, sizeof(dbrev_val));
        const BACSTAC_STATUS dbrev_st = BACstacStorePropertyInstance(
            hDevObj, PROP_DATABASE_REVISION, BACSTAC_VOID_INDEX, &pc_dbrev);
        if (dbrev_st == BACSTAC_STATUS_OK) {
            std::cout << "[Gateway] Database_Revision set to: " << database_revision_ << std::endl;
        } else {
            std::cerr << "[Gateway] Failed to set Database_Revision. Status: " << dbrev_st << std::endl;
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// registerAllObjects()
// ---------------------------------------------------------------------------

bool BacnetGateway::isSupportedObjectType(BACSTAC_OBJECT_TYPE objType)
{
    return objType == OBJ_ANALOG_INPUT || objType == OBJ_ANALOG_OUTPUT ||
           objType == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
           objType == OBJ_BINARY_OUTPUT ||
#endif
           objType == OBJ_BINARY_VALUE;
}

bool BacnetGateway::isAnalogObject(BACSTAC_OBJECT_TYPE objType)
{
    return objType == OBJ_ANALOG_INPUT || objType == OBJ_ANALOG_OUTPUT;
}

bool BacnetGateway::isBinaryObject(BACSTAC_OBJECT_TYPE objType)
{
    return objType == OBJ_BINARY_INPUT ||
#ifdef BINARY_OUTPUT_SUPPORT
           objType == OBJ_BINARY_OUTPUT ||
#endif
           objType == OBJ_BINARY_VALUE;
}

bool BacnetGateway::isCommandableObject(BACSTAC_OBJECT_TYPE objType)
{
    return objType == OBJ_ANALOG_OUTPUT
#ifdef BINARY_OUTPUT_SUPPORT
           || objType == OBJ_BINARY_OUTPUT
#endif
           ;
}

void BacnetGateway::initializePriorityArray(BACSTAC_PRI_ARRAY_ITEM* priorityArray)
{
    if (priorityArray == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < kPriorityArraySize; ++i) {
        BACstacInitPriArrayItem(&priorityArray[i]);
        BACstacSetPriArrayItemNull(&priorityArray[i]);
    }
}

void BacnetGateway::storeNullPriorityArray(BACSTAC_HOBJECT hObject)
{
    if (hObject == nullptr) {
        return;
    }

    BACSTAC_PRI_ARRAY_ITEM nullItem;
    BACstacInitPriArrayItem(&nullItem);
    BACstacSetPriArrayItemNull(&nullItem);

    BACSTAC_PROPERTY_CONTENTS pcNull;
    BACstacInitContentsToVar(&pcNull, DATA_TYPE_PRI_ARRAY_ITEM,
                             &nullItem, sizeof(nullItem));
    for (std::size_t priority = 1U; priority <= kPriorityArraySize; ++priority) {
        (void)BACstacStorePropertyInstance(hObject, PROP_PRIORITY_ARRAY,
                                           static_cast<BACSTAC_ARRAY_INDEX>(priority),
                                           &pcNull);
    }
}

BACSTAC_STATUS BacnetGateway::createObjectForMapping(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    switch (mapping.bacnet_type) {
        case OBJ_ANALOG_INPUT:
            return createAnalogInputObject(hDev, mapping, phObject);
        case OBJ_ANALOG_OUTPUT:
            return createAnalogOutputObject(hDev, mapping, phObject);
        case OBJ_BINARY_INPUT:
            return createBinaryInputObject(hDev, mapping, phObject);
#ifdef BINARY_OUTPUT_SUPPORT
        case OBJ_BINARY_OUTPUT:
            return createBinaryOutputObject(hDev, mapping, phObject);
#endif
        case OBJ_BINARY_VALUE:
            return createBinaryValueObject(hDev, mapping, phObject);
        default:
            return BACSTAC_STATUS_UNSUPPORTED_OBJ_TYPE;
    }
}

BACSTAC_STATUS BacnetGateway::createAnalogInputObject(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    static const BACSTAC_REAL       initReal     = 0.0f;
    static const BACSTAC_BIT_STRING statusFlags  = BACSTAC_BIT_STRING1_INIT(4, 0x00);
    static const BACSTAC_EVENT_STATE eventState  = STATE_NORMAL;
    static const BACSTAC_BOOLEAN    outOfService = BACSTAC_FALSE;
    BACSTAC_REAL covIncrement = static_cast<BACSTAC_REAL>(mapping.cov_increment);

    BACSTAC_PROPERTY_COUNT propCount = 9U;
    if (!mapping.description.empty()) { ++propCount; }
    if (mapping.has_min_value)        { ++propCount; }
    if (mapping.has_max_value)        { ++propCount; }

    BACSTAC_STATUS status = BACSTAC_STATUS_OK;
    BACSTAC_PROPERTY_COUNT firstFailed = 0U;
    CREATE_OBJ_BEGIN(mapping.bacnet_type, mapping.instance, mapping.name.c_str(), propCount);
    if (!mapping.description.empty()) { INIT_PROPS(DESCRIPTION, mapping.description.c_str()); }
    INIT_PROPV(PRESENT_VALUE,   REAL,       &initReal);
    INIT_PROPV(STATUS_FLAGS,    BIT_STRING, &statusFlags);
    INIT_PROPV(EVENT_STATE,     ENUM,       &eventState);
    INIT_PROPVF(OUT_OF_SERVICE, BOOLEAN,    &outOfService, BACSTAC_WRITABLE_FLAG);
    INIT_PROPV(UNITS,            ENUM,      &mapping.units);
    INIT_PROPVF(COV_INCREMENT,  REAL,       &covIncrement, BACSTAC_WRITABLE_FLAG);
    BACSTAC_REAL minReal = static_cast<BACSTAC_REAL>(mapping.min_value);
    BACSTAC_REAL maxReal = static_cast<BACSTAC_REAL>(mapping.max_value);
    if (mapping.has_min_value) { INIT_PROPV(MIN_PRES_VALUE, REAL, &minReal); }
    if (mapping.has_max_value) { INIT_PROPV(MAX_PRES_VALUE, REAL, &maxReal); }
    CREATE_OBJ_END(status, phObject, &firstFailed);
    return status;
}

BACSTAC_STATUS BacnetGateway::createAnalogOutputObject(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    const BACSTAC_REAL              initReal     = static_cast<BACSTAC_REAL>(mapping.relinquish_default);
    static const BACSTAC_BIT_STRING statusFlags  = BACSTAC_BIT_STRING1_INIT(4, 0x00);
    static const BACSTAC_EVENT_STATE eventState  = STATE_NORMAL;
    static const BACSTAC_BOOLEAN    outOfService = BACSTAC_FALSE;
    static const BACSTAC_OPT_CMD_UNSIGNED initPriority = BACSTAC_VOID_PRI;
    BACSTAC_REAL relinquishDefault = static_cast<BACSTAC_REAL>(mapping.relinquish_default);
    BACSTAC_REAL covIncrement = static_cast<BACSTAC_REAL>(mapping.cov_increment);
    BACSTAC_PRI_ARRAY_ITEM priorityArray[kPriorityArraySize];
    initializePriorityArray(priorityArray);

    BACSTAC_PROPERTY_COUNT propCount = 12U;
    if (!mapping.description.empty()) { ++propCount; }
    if (mapping.has_min_value)        { ++propCount; }
    if (mapping.has_max_value)        { ++propCount; }

    BACSTAC_STATUS status = BACSTAC_STATUS_OK;
    BACSTAC_PROPERTY_COUNT firstFailed = 0U;
    CREATE_OBJ_BEGIN(mapping.bacnet_type, mapping.instance, mapping.name.c_str(), propCount);
    if (!mapping.description.empty()) { INIT_PROPS(DESCRIPTION, mapping.description.c_str()); }
    INIT_PROPVF(PRESENT_VALUE, REAL, &initReal, BACSTAC_WRITABLE_FLAG);
    INIT_PROPV(STATUS_FLAGS, BIT_STRING, &statusFlags);
    INIT_PROPV(EVENT_STATE, ENUM, &eventState);
    INIT_PROPVF(OUT_OF_SERVICE, BOOLEAN, &outOfService, BACSTAC_WRITABLE_FLAG);
    INIT_PROPV(UNITS, ENUM, &mapping.units);
    INIT_PROPA(PRIORITY_ARRAY, PRI_ARRAY_ITEM, priorityArray, 16);
    INIT_PROPV(RELINQUISH_DEFAULT, REAL, &relinquishDefault);
    INIT_PROPV(CURRENT_COMMAND_PRIORITY, OPT_CMD_UNSIGNED, &initPriority);
    INIT_PROPVF(COV_INCREMENT, REAL, &covIncrement, BACSTAC_WRITABLE_FLAG);
    BACSTAC_REAL minReal = static_cast<BACSTAC_REAL>(mapping.min_value);
    BACSTAC_REAL maxReal = static_cast<BACSTAC_REAL>(mapping.max_value);
    if (mapping.has_min_value) { INIT_PROPV(MIN_PRES_VALUE, REAL, &minReal); }
    if (mapping.has_max_value) { INIT_PROPV(MAX_PRES_VALUE, REAL, &maxReal); }
    CREATE_OBJ_END(status, phObject, &firstFailed);
    return status;
}

BACSTAC_STATUS BacnetGateway::createBinaryInputObject(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    static const BACSTAC_ENUM       initEnum     = BACSTAC_BINARY_INACTIVE;
    static const BACSTAC_BIT_STRING statusFlags  = BACSTAC_BIT_STRING1_INIT(4, 0x00);
    static const BACSTAC_EVENT_STATE eventState  = STATE_NORMAL;
    static const BACSTAC_BOOLEAN    outOfService = BACSTAC_FALSE;
    static const BACSTAC_POLARITY   polarity     = POLARITY_NORMAL;

    BACSTAC_PROPERTY_COUNT propCount = 8U;
    if (!mapping.description.empty()) { ++propCount; }

    BACSTAC_STATUS status = BACSTAC_STATUS_OK;
    BACSTAC_PROPERTY_COUNT firstFailed = 0U;
    CREATE_OBJ_BEGIN(mapping.bacnet_type, mapping.instance, mapping.name.c_str(), propCount);
    if (!mapping.description.empty()) { INIT_PROPS(DESCRIPTION, mapping.description.c_str()); }
    INIT_PROPV(PRESENT_VALUE, ENUM, &initEnum);
    INIT_PROPV(STATUS_FLAGS, BIT_STRING, &statusFlags);
    INIT_PROPV(EVENT_STATE, ENUM, &eventState);
    INIT_PROPVF(OUT_OF_SERVICE, BOOLEAN, &outOfService, BACSTAC_WRITABLE_FLAG);
    INIT_PROPVF(POLARITY, ENUM, &polarity, BACSTAC_WRITABLE_FLAG);
    CREATE_OBJ_END(status, phObject, &firstFailed);
    return status;
}

#ifdef BINARY_OUTPUT_SUPPORT
BACSTAC_STATUS BacnetGateway::createBinaryOutputObject(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    const BACSTAC_ENUM              initEnum     = mapping.relinquish_default == 1.0
                                                    ? BACSTAC_BINARY_ACTIVE
                                                    : BACSTAC_BINARY_INACTIVE;
    static const BACSTAC_BIT_STRING statusFlags  = BACSTAC_BIT_STRING1_INIT(4, 0x00);
    static const BACSTAC_EVENT_STATE eventState  = STATE_NORMAL;
    static const BACSTAC_BOOLEAN    outOfService = BACSTAC_FALSE;
    static const BACSTAC_POLARITY   polarity     = POLARITY_NORMAL;
    static const BACSTAC_OPT_CMD_UNSIGNED initPriority = BACSTAC_VOID_PRI;
    BACSTAC_ENUM relinquishDefault = initEnum;
    BACSTAC_PRI_ARRAY_ITEM priorityArray[kPriorityArraySize];
    initializePriorityArray(priorityArray);

    BACSTAC_PROPERTY_COUNT propCount = 11U;
    if (!mapping.description.empty()) { ++propCount; }

    BACSTAC_STATUS status = BACSTAC_STATUS_OK;
    BACSTAC_PROPERTY_COUNT firstFailed = 0U;
    CREATE_OBJ_BEGIN(mapping.bacnet_type, mapping.instance, mapping.name.c_str(), propCount);
    if (!mapping.description.empty()) { INIT_PROPS(DESCRIPTION, mapping.description.c_str()); }
    INIT_PROPVF(PRESENT_VALUE, ENUM, &initEnum, BACSTAC_WRITABLE_FLAG);
    INIT_PROPV(STATUS_FLAGS, BIT_STRING, &statusFlags);
    INIT_PROPV(EVENT_STATE, ENUM, &eventState);
    INIT_PROPVF(OUT_OF_SERVICE, BOOLEAN, &outOfService, BACSTAC_WRITABLE_FLAG);
    INIT_PROPA(PRIORITY_ARRAY, PRI_ARRAY_ITEM, priorityArray, 16);
    INIT_PROPV(RELINQUISH_DEFAULT, ENUM, &relinquishDefault);
    INIT_PROPV(CURRENT_COMMAND_PRIORITY, OPT_CMD_UNSIGNED, &initPriority);
    INIT_PROPVF(POLARITY, ENUM, &polarity, BACSTAC_WRITABLE_FLAG);
    CREATE_OBJ_END(status, phObject, &firstFailed);
    return status;
}
#endif

BACSTAC_STATUS BacnetGateway::createBinaryValueObject(
    BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject)
{
    static const BACSTAC_ENUM       initEnum     = BACSTAC_BINARY_INACTIVE;
    static const BACSTAC_BIT_STRING statusFlags  = BACSTAC_BIT_STRING1_INIT(4, 0x00);
    static const BACSTAC_EVENT_STATE eventState  = STATE_NORMAL;
    static const BACSTAC_BOOLEAN    outOfService = BACSTAC_FALSE;

    BACSTAC_PROPERTY_COUNT propCount = 7U;
    if (!mapping.description.empty()) { ++propCount; }

    BACSTAC_STATUS status = BACSTAC_STATUS_OK;
    BACSTAC_PROPERTY_COUNT firstFailed = 0U;
    CREATE_OBJ_BEGIN(mapping.bacnet_type, mapping.instance, mapping.name.c_str(), propCount);
    if (!mapping.description.empty()) { INIT_PROPS(DESCRIPTION, mapping.description.c_str()); }
    INIT_PROPVF(PRESENT_VALUE, ENUM, &initEnum, BACSTAC_WRITABLE_FLAG);
    INIT_PROPV(STATUS_FLAGS, BIT_STRING, &statusFlags);
    INIT_PROPV(EVENT_STATE, ENUM, &eventState);
    INIT_PROPVF(OUT_OF_SERVICE, BOOLEAN, &outOfService, BACSTAC_WRITABLE_FLAG);
    CREATE_OBJ_END(status, phObject, &firstFailed);
    return status;
}

bool BacnetGateway::registerAllObjects(BACSTAC_HDEVICE hDev)
{
    std::cout << "[Gateway] Registering all objects under local device (Single-Device Mode)" << std::endl;

    for (auto* mapping_ptr : mappings_) {
        if (mapping_ptr == nullptr || !isSupportedObjectType(mapping_ptr->bacnet_type)) {
            std::cerr << "[Gateway] Failed to register an unsupported or null object mapping." << std::endl;
            return false;
        }

        ObjectMapping& mapping = *mapping_ptr;
        BACSTAC_HOBJECT hObj = nullptr;

        std::cout << "[Gateway] Registering object: " << mapping.name
                  << " (Type: " << mapping.bacnet_type
                  << ", Instance: " << mapping.instance << ")" << std::endl;

        const BACSTAC_STATUS status = createObjectForMapping(hDev, mapping, &hObj);

        if (status != BACSTAC_STATUS_OK) {
            std::cerr << "[Gateway] Failed to create object " << mapping.name
                      << "! Code: " << status << std::endl;
            return false;
        }

        if (isCommandableObject(mapping.bacnet_type)) {
            storeNullPriorityArray(hObj);
        }

        mapping.hObject = hObj;
    }

    return true;
}

// ---------------------------------------------------------------------------
// registerHooks()
// ---------------------------------------------------------------------------

bool BacnetGateway::registerHooks()
{
    for (std::size_t i = 0; i < sizeof(kHookDefinitions) / sizeof(kHookDefinitions[0]); ++i) {
        const HookDefinition& hook = kHookDefinitions[i];
        const BACSTAC_STATUS status = BACstacSetHook(hook.hookType, hook.proc);
        if (status != BACSTAC_STATUS_OK) {
            std::cerr << "[Gateway] Failed to set " << hook.name
                      << " hook! Code: " << status << std::endl;
            return false;
        }
    }
    return true;
}

void BacnetGateway::unregisterHooks()
{
    for (std::size_t i = 0; i < sizeof(kHookDefinitions) / sizeof(kHookDefinitions[0]); ++i) {
        (void)BACstacSetHook(kHookDefinitions[i].hookType, nullptr);
    }
}

// ---------------------------------------------------------------------------
// startCovManager()
// ---------------------------------------------------------------------------

void BacnetGateway::startCovManager()
{
    cov_ = std::unique_ptr<CovManager>(
        new CovManager(*modbus_, cov_polling_ms_, enable_log_));
    cov_->setLocalDevHandle(hLocalDev_);
    cov_->start();
}

// ---------------------------------------------------------------------------
// stop()
// ---------------------------------------------------------------------------

void BacnetGateway::stop()
{
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        external_running_ = nullptr;
        return;
    }

    std::cout << "[Gateway] Stopping BACnet IP Gateway..." << std::endl;

    unregisterHooks();

    if (fd_monitor_) {
        fd_monitor_->stop();
        fd_monitor_.reset();
    }
    if (cov_) {
        cov_->stop();
    }
    if (modbus_) {
        modbus_->stop();
    }

    BACstacClose();

    if (serial_port_) {
        serial_port_->close();
    }
    hLocalDev_ = nullptr;
    external_running_ = nullptr;
    std::cout << "[Gateway] Stopped." << std::endl;
}

// ---------------------------------------------------------------------------
// onAsyncWriteCompleted() / onAsyncWriteFailed()
// (단일 전담 시리얼 Worker 스레드에서 I/O 완료 후 호출)
// ---------------------------------------------------------------------------

void BacnetGateway::onAsyncWriteCompleted(
    BACSTAC_HTSM                   hTsm,
    const ObjectMapping*           mapping,
    bool                           is_commandable,
    const BACSTAC_PRI_ARRAY_ITEM*  priorityArray,
    double                         write_val)
{
    if (is_commandable && mapping != nullptr && priorityArray != nullptr) {
        storePriorityArray(const_cast<ObjectMapping*>(mapping),
                           const_cast<BACSTAC_PRI_ARRAY_ITEM*>(priorityArray));
        notifyCov(const_cast<ObjectMapping*>(mapping), write_val, false);
    } else if (mapping != nullptr) {
        storePresentValue(const_cast<ObjectMapping*>(mapping), mapping->bacnet_type, write_val);
        notifyCov(const_cast<ObjectMapping*>(mapping), write_val, false);
    }
    sendResponse(hTsm);
}

void BacnetGateway::onAsyncWriteFailed(
    BACSTAC_HTSM         hTsm,
    const ObjectMapping* mapping)
{
    (void)mapping;
    sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
}

// ---------------------------------------------------------------------------
// findMapping()
// ---------------------------------------------------------------------------

ObjectMapping* BacnetGateway::findMapping(BACSTAC_OBJECT_TYPE type, uint32_t instance)
{
    for (auto* m : mappings_) {
        if (m != nullptr && m->bacnet_type == type && m->instance == instance) {
            return m;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// ms 타임스탬프 (로깅용)
// ---------------------------------------------------------------------------

static std::string getMsTimestamp()
{
    return ModbusClient::getMsTimestamp();
}

bool BacnetGateway::validateReadHookInput(const BACSTAC_READ_INFO* pInfo)
{
    return instance_ != nullptr && pInfo != nullptr;
}

BACSTAC_HOOK_STATUS BacnetGateway::sendServiceError(
    BACSTAC_HTSM hTsm, BACSTAC_ERROR_CLASS errClass, BACSTAC_ERROR_CODE errCode)
{
    if (hTsm == BACSTAC_INVALID_HANDLE) {
        return HOOK_STATUS_OK;
    }

    const BACSTAC_STATUS status = BACstacSrvcError(hTsm, errClass, errCode);
    if (status != BACSTAC_STATUS_OK) {
        (void)BACstacDestroyTSM(hTsm);
    }
    return HOOK_STATUS_OK;
}

BACSTAC_HOOK_STATUS BacnetGateway::sendReject(
    BACSTAC_HTSM hTsm, BACSTAC_REJECT_REASON reason, const char* serviceName)
{
    if (instance_ != nullptr && instance_->enable_log_) {
        std::cout << "[" << getMsTimestamp() << "][BACnet RX] Rejecting "
                  << (serviceName != nullptr ? serviceName : "unsupported service")
                  << " (UNRECOGNIZED_SERVICE)" << std::endl;
    }

    if (hTsm == BACSTAC_INVALID_HANDLE) {
        return HOOK_STATUS_OK;
    }

    const BACSTAC_STATUS status = BACstacSrvcReject(hTsm, reason);
    if (status != BACSTAC_STATUS_OK) {
        (void)BACstacDestroyTSM(hTsm);
    }
    return HOOK_STATUS_OK;
}

BACSTAC_HOOK_STATUS BacnetGateway::sendAbort(BACSTAC_HTSM hTsm,
                                              BACSTAC_ABORT_REASON reason)
{
    if (hTsm == BACSTAC_INVALID_HANDLE) {
        return HOOK_STATUS_OK;
    }

    const BACSTAC_STATUS status = BACstacSrvcAbort(hTsm, reason);
    if (status != BACSTAC_STATUS_OK) {
        (void)BACstacDestroyTSM(hTsm);
    }
    return HOOK_STATUS_OK;
}

BACSTAC_HOOK_STATUS BacnetGateway::sendResponse(BACSTAC_HTSM hTsm)
{
    if (hTsm == BACSTAC_INVALID_HANDLE) {
        return HOOK_STATUS_OK;
    }

    const BACSTAC_STATUS status = BACstacSrvcResponse(hTsm);
    if (status != BACSTAC_STATUS_OK) {
        return sendAbort(hTsm, ABORT_OTHER);
    }
    return HOOK_STATUS_OK;
}

BACSTAC_HOOK_STATUS BacnetGateway::sendReadResponse(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo,
    const BACSTAC_PROPERTY_CONTENTS* pValue)
{
    if (hTsm == BACSTAC_INVALID_HANDLE || pInfo == nullptr || pValue == nullptr) {
        return HOOK_STATUS_OK;
    }

    const BACSTAC_STATUS status = BACstacReadPropResponse(hTsm, pInfo, pValue);
    if (status != BACSTAC_STATUS_OK) {
        return sendAbort(hTsm, ABORT_OTHER);
    }
    return HOOK_STATUS_OK;
}

BACSTAC_HOOK_STATUS BacnetGateway::rejectUnsupportedService(
    BACSTAC_HTSM hTsm, const char* serviceName)
{
    return sendReject(hTsm, REJECT_UNRECOGNIZED_SERVICE, serviceName);
}

// ---------------------------------------------------------------------------
// readPropHook()
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::readPropHook(
    BACSTAC_HTSM             hTransaction,
    BACSTAC_ADDRESS*         sourceAddress,
    BACSTAC_ADDRESS*         destinationAddress,
    const BACSTAC_READ_INFO* pServiceInfo)
{
    (void)sourceAddress;
    (void)destinationAddress;

    if (!validateReadHookInput(pServiceInfo)) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_OBJECT_TYPE objType =
        static_cast<BACSTAC_OBJECT_TYPE>(pServiceInfo->objectID.type);
    const uint32_t inst    = pServiceInfo->objectID.instNumber;
    const uint32_t prop_id = pServiceInfo->ePropertyID;

    if (objType == OBJ_DEVICE) {
        if (prop_id == PROP_VENDOR_NAME) {
            const std::string& str = instance_->bacnet_cfg_.vendor_name;
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UTF_8_STRING, str.c_str(), static_cast<BACSTAC_VALUE_SIZE>(str.size() + 1));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_VENDOR_IDENTIFIER) {
            BACSTAC_UNSIGNED vid = static_cast<BACSTAC_UNSIGNED>(instance_->bacnet_cfg_.vendor_id);
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &vid, sizeof(vid));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_MODEL_NAME) {
            const std::string& str = instance_->bacnet_cfg_.model_name;
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UTF_8_STRING, str.c_str(), static_cast<BACSTAC_VALUE_SIZE>(str.size() + 1));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_OBJECT_NAME) {
            const std::string& str = instance_->bacnet_cfg_.device_name;
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UTF_8_STRING, str.c_str(), static_cast<BACSTAC_VALUE_SIZE>(str.size() + 1));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_OBJECT_IDENTIFIER) {
            BACSTAC_OBJECT_ID objId;
            BACstacInitObjectID(&objId, OBJ_DEVICE, static_cast<BACSTAC_INST_NUMBER>(instance_->bacnet_cfg_.device_id));
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_OBJECT_ID, &objId, sizeof(objId));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_OBJECT_TYPE) {
            BACSTAC_ENUM otype = static_cast<BACSTAC_ENUM>(OBJ_DEVICE);
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_ENUM, &otype, sizeof(otype));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_APDU_TIMEOUT) {
            BACSTAC_UNSIGNED val = static_cast<BACSTAC_UNSIGNED>(instance_->bacnet_cfg_.apdu_timeout_ms);
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_NUMBER_OF_APDU_RETRIES) {
            BACSTAC_UNSIGNED val = static_cast<BACSTAC_UNSIGNED>(instance_->bacnet_cfg_.apdu_retries);
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_MAX_APDU_LENGTH_ACCEPTED) {
            BACSTAC_UNSIGNED val = static_cast<BACSTAC_UNSIGNED>(instance_->bacnet_cfg_.max_apdu_length);
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_SEGMENTATION_SUPPORTED) {
            BACSTAC_ENUM val = 3U; // NO_SEGMENTATION (3)
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_ENUM, &val, sizeof(val));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_MAX_SEGMENTS_ACCEPTED) {
            BACSTAC_UNSIGNED val = 1U; // 1 (Segmentation not supported)
            BACSTAC_PROPERTY_CONTENTS pc;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            return sendReadResponse(hTransaction, pServiceInfo, &pc);
        }
        if (prop_id == PROP_DATABASE_REVISION) {
            BACSTAC_UNSIGNED dbrev = static_cast<BACSTAC_UNSIGNED>(instance_->database_revision_);
            BACSTAC_PROPERTY_CONTENTS pc_dbrev;
            BACstacInitContentsToVar(&pc_dbrev, DATA_TYPE_UNSIGNED, &dbrev, sizeof(dbrev));
            return sendReadResponse(hTransaction, pServiceInfo, &pc_dbrev);
        }
        if (prop_id == PROP_ACTIVE_COV_SUBSCRIPTIONS) {
            return handleReadActiveCOVSubscriptions(hTransaction, pServiceInfo);
        }
        if (prop_id == PROP_LOCAL_TIME || prop_id == PROP_LOCAL_DATE) {
            // 시스템 로컬 시간 취득 (localtime_r 사용, PROP_UTC_OFFSET 연동 안 함)
            time_t now = time(nullptr);
            struct tm tm_local;
            localtime_r(&now, &tm_local);

            if (prop_id == PROP_LOCAL_TIME) {
                BACSTAC_TIME bTime;
                BACstacInitTime(&bTime,
                                static_cast<BACSTAC_HOUR>(tm_local.tm_hour),
                                static_cast<BACSTAC_MINUTE>(tm_local.tm_min),
                                static_cast<BACSTAC_SECOND>(tm_local.tm_sec),
                                0);
                BACSTAC_PROPERTY_CONTENTS pcTime;
                BACstacInitContentsToVar(&pcTime, DATA_TYPE_TIME, &bTime, sizeof(bTime));
                return sendReadResponse(hTransaction, pServiceInfo, &pcTime);
            } else {
                BACSTAC_DATE bDate;
                BACstacInitDate(&bDate,
                                static_cast<BACSTAC_YEAR>(tm_local.tm_year + 1900),
                                static_cast<BACSTAC_MONTH>(tm_local.tm_mon + 1),
                                static_cast<BACSTAC_DAY_OF_MONTH>(tm_local.tm_mday));
                BACSTAC_DAY_OF_WEEK wday = static_cast<BACSTAC_DAY_OF_WEEK>(tm_local.tm_wday == 0 ? 7 : tm_local.tm_wday);
                BACstacSetDateDayOfWeek(&bDate, wday);

                BACSTAC_PROPERTY_CONTENTS pcDate;
                BACstacInitContentsToVar(&pcDate, DATA_TYPE_DATE, &bDate, sizeof(bDate));
                return sendReadResponse(hTransaction, pServiceInfo, &pcDate);
            }
        }
        return HOOK_STATUS_DEFAULT;
    }

    if (objType == OBJ_NETWORK_PORT) {
        if (prop_id == kNetworkPortCommandProperty) {
            return rejectNetworkPortBlockingProperty(hTransaction, prop_id);
        }
        if (prop_id == PROP_PROPERTY_LIST) {
            return handleNetworkPortPropertyList(hTransaction, pServiceInfo);
        }
        // Changes_Pending은 FALSE로 고정한다. Network Port 쓰기는 pending 없이 즉시 반영(재시작)되며
        // 스택의 pending 경로(ACTIVATE_CHANGES)를 사용하지 않는다.
        if (prop_id == kNetworkPortChangesPendingProperty) {
            BACSTAC_BOOLEAN bFalse = BACSTAC_FALSE;
            BACSTAC_PROPERTY_CONTENTS pc_cp;
            BACstacInitContentsToVar(&pc_cp, DATA_TYPE_BOOLEAN, &bFalse, sizeof(bFalse));
            return sendReadResponse(hTransaction, pServiceInfo, &pc_cp);
        }
        return handleNetworkPortRead(hTransaction, pServiceInfo);
    }

    if (prop_id == PROP_ALL || prop_id == PROP_REQUIRED || prop_id == PROP_OPTIONAL) {
        return rejectSpecialPropertyQuery(hTransaction, prop_id, static_cast<uint32_t>(objType));
    }

    ObjectMapping* mapping = instance_->findMapping(objType, inst);
    if (mapping == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    if (prop_id == PROP_CURRENT_COMMAND_PRIORITY) {
        return handleReadCommandPriority(hTransaction, pServiceInfo, mapping);
    }

    if (prop_id != PROP_PRESENT_VALUE && prop_id != PROP_RELINQUISH_DEFAULT) {
        return HOOK_STATUS_DEFAULT;
    }

    return handleReadPresentValue(hTransaction, pServiceInfo, mapping);
}

// ── readPropHook 세분화 핸들러 ───────────────────────────────────────────────

BACSTAC_HOOK_STATUS BacnetGateway::rejectSpecialPropertyQuery(
    BACSTAC_HTSM hTsm, uint32_t prop_id, uint32_t obj_type)
{
    (void)prop_id;
    (void)obj_type;
    return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_PROPERTY_IS_NOT_AN_ARRAY);
}

BACSTAC_HOOK_STATUS BacnetGateway::rejectNetworkPortBlockingProperty(
    BACSTAC_HTSM hTsm, uint32_t prop_id)
{
    if (instance_ != nullptr && instance_->enable_log_) {
        std::cout << "[" << getMsTimestamp() << "][BACnet RX] Rejecting Network Port property "
                  << prop_id << " (UNKNOWN_PROPERTY)" << std::endl;
    }
    return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_UNKNOWN_PROPERTY);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleNetworkPortRead(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo)
{
    if (pInfo == nullptr || instance_ == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const NetworkPortProperty* property = findNetworkPortProperty(pInfo->ePropertyID);
    if (property == nullptr) {
        // 위에서 처리되지 않은 Network Port 속성은 BACstac 기본 처리에 위임
        return HOOK_STATUS_DEFAULT;
    }

    BACSTAC_PROPERTY_CONTENTS pc = {};
    switch (property->kind) {
        case NetworkPortReadKind::ObjectName: {
            const std::string& str = instance_->bacnet_cfg_.network_port_name;
            BACstacInitContentsToVar(&pc, DATA_TYPE_UTF_8_STRING, str.c_str(), static_cast<BACSTAC_VALUE_SIZE>(str.size() + 1));
            break;
        }
        case NetworkPortReadKind::MACAddress: {
            BACSTAC_OCTET_STRING octetString;
            BACstacInitOctetString(&octetString, instance_->mac_address_, sizeof(instance_->mac_address_));
            BACstacInitContentsToVar(&pc, DATA_TYPE_OCTET_STRING,
                                     &octetString, sizeof(octetString));
            break;
        }
        case NetworkPortReadKind::IPDNSServer: {
            const BACSTAC_ARRAY_INDEX arrayIndex = pInfo->nIndex;
            const size_t count = instance_->dns_servers_.size();

            if (arrayIndex == 0U) {
                BACSTAC_UNSIGNED val = static_cast<BACSTAC_UNSIGNED>(count);
                BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            } else if (arrayIndex == BACSTAC_VOID_INDEX) {
                if (count == 0U) {
                    return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_UNKNOWN_PROPERTY);
                }
                int oct0 = 0, oct1 = 0, oct2 = 0, oct3 = 0;
                uint8_t ipBytes[4] = { 0U, 0U, 0U, 0U };
                if (std::sscanf(instance_->dns_servers_[0].c_str(), "%d.%d.%d.%d",
                                &oct0, &oct1, &oct2, &oct3) == 4) {
                    ipBytes[0] = static_cast<uint8_t>(oct0);
                    ipBytes[1] = static_cast<uint8_t>(oct1);
                    ipBytes[2] = static_cast<uint8_t>(oct2);
                    ipBytes[3] = static_cast<uint8_t>(oct3);
                }
                BACSTAC_OCTET_STRING octetString;
                BACstacInitOctetString(&octetString, ipBytes, sizeof(ipBytes));
                BACstacInitContentsToVar(&pc, DATA_TYPE_OCTET_STRING,
                                         &octetString, sizeof(octetString));
            } else if (arrayIndex <= static_cast<BACSTAC_ARRAY_INDEX>(count)) {
                int oct0 = 0, oct1 = 0, oct2 = 0, oct3 = 0;
                uint8_t ipBytes[4] = { 0U, 0U, 0U, 0U };
                if (std::sscanf(instance_->dns_servers_[arrayIndex - 1U].c_str(), "%d.%d.%d.%d",
                                &oct0, &oct1, &oct2, &oct3) == 4) {
                    ipBytes[0] = static_cast<uint8_t>(oct0);
                    ipBytes[1] = static_cast<uint8_t>(oct1);
                    ipBytes[2] = static_cast<uint8_t>(oct2);
                    ipBytes[3] = static_cast<uint8_t>(oct3);
                }
                BACSTAC_OCTET_STRING octetString;
                BACstacInitOctetString(&octetString, ipBytes, sizeof(ipBytes));
                BACstacInitContentsToVar(&pc, DATA_TYPE_OCTET_STRING,
                                         &octetString, sizeof(octetString));
            } else {
                return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_ARRAY_INDEX);
            }
            break;
        }
        case NetworkPortReadKind::FDBBMDAddress: {
            // 모드와 무관하게 bip.cfg.fd의 설정값으로 응답한다(Normal 모드에서 쓴 값도 그대로 읽힘).
            int oct0 = 0, oct1 = 0, oct2 = 0, oct3 = 0;
            uint8_t ipBytes[4] = { 0U, 0U, 0U, 0U };
            if (std::sscanf(instance_->fd_bbmd_ip_.c_str(), "%d.%d.%d.%d",
                            &oct0, &oct1, &oct2, &oct3) == 4) {
                ipBytes[0] = static_cast<uint8_t>(oct0);
                ipBytes[1] = static_cast<uint8_t>(oct1);
                ipBytes[2] = static_cast<uint8_t>(oct2);
                ipBytes[3] = static_cast<uint8_t>(oct3);
            }
            BACSTAC_HOST_ADDRESS hostAddr;
            BACSTAC_HOST_NPORT hostNPort;
            BACstacInitHostAddressIpAddress(&hostAddr, ipBytes, sizeof(ipBytes));
            BACstacInitHostNPort(&hostNPort, &hostAddr, instance_->fd_bbmd_port_);
            BACstacInitContentsToVar(&pc, DATA_TYPE_HOST_NPORT, &hostNPort, sizeof(hostNPort));
            break;
        }
        case NetworkPortReadKind::FDSubscriptionLifetime: {
            BACSTAC_UNSIGNED val = static_cast<BACSTAC_UNSIGNED>(instance_->fd_ttl_);
            BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &val, sizeof(val));
            break;
        }
    }

    return sendReadResponse(hTsm, pInfo, &pc);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleNetworkPortPropertyList(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo)
{
    static const BACSTAC_ARRAY_INDEX kCount = static_cast<BACSTAC_ARRAY_INDEX>(
        sizeof(kNetworkPortPropertyList) / sizeof(kNetworkPortPropertyList[0]));

    BACSTAC_PROPERTY_CONTENTS pc = {};
    const BACSTAC_ARRAY_INDEX arrayIndex = pInfo->nIndex;
    if (arrayIndex == BACSTAC_VOID_INDEX) {
        BACstacInitContentsToArray(&pc, DATA_TYPE_ENUM, kNetworkPortPropertyList,
                                   static_cast<BACSTAC_VALUE_SIZE>(sizeof(kNetworkPortPropertyList)),
                                   static_cast<BACSTAC_ELEMENT_COUNT>(kCount));
        return sendReadResponse(hTsm, pInfo, &pc);
    }
    if (arrayIndex == BACSTAC_ARRAY_COUNT) {
        BACSTAC_UNSIGNED count = static_cast<BACSTAC_UNSIGNED>(kCount);
        BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &count, sizeof(count));
        return sendReadResponse(hTsm, pInfo, &pc);
    }
    if (arrayIndex > kCount) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_ARRAY_INDEX);
    }
    BACSTAC_ENUM element = kNetworkPortPropertyList[arrayIndex - 1U];
    BACstacInitContentsToVar(&pc, DATA_TYPE_ENUM, &element, sizeof(element));
    return sendReadResponse(hTsm, pInfo, &pc);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleReadCommandPriority(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo, ObjectMapping* mapping)
{
    if (mapping == nullptr || !isCommandableObject(mapping->bacnet_type)) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_UNKNOWN_PROPERTY);
    }

    BACSTAC_OPT_CMD_UNSIGNED optCmd = BACSTAC_VOID_PRI;

    // Check all priority slots 1-16 to find the first non-null slot
    for (uint16_t pri_idx = 1; pri_idx <= 16; ++pri_idx) {
        BACSTAC_PRI_ARRAY_ITEM priItem;
        BACstacInitPriArrayItem(&priItem);
        BACSTAC_PROPERTY_CONTENTS pc_slot;
        BACstacInitContentsToVar(&pc_slot, DATA_TYPE_PRI_ARRAY_ITEM, &priItem, sizeof(priItem));

        BACSTAC_STATUS slotStatus = BACstacRetrievePropertyInstance2(
            mapping->hObject, PROP_PRIORITY_ARRAY, pri_idx, &pc_slot, nullptr);

        if (slotStatus == BACSTAC_STATUS_OK && !BACstacIsPriArrayItemNull(&priItem)) {
            optCmd = static_cast<BACSTAC_OPT_CMD_UNSIGNED>(pri_idx);
            break;
        }
    }

    if (instance_ != nullptr && instance_->enable_log_) {
        std::cout << "[" << ModbusClient::getMsTimestamp() << "][BACnet TX] ReadProperty Response: '"
                  << mapping->name << "' Current_Command_Priority=";
        if (optCmd == BACSTAC_VOID_PRI) {
            std::cout << "NULL" << std::endl;
        } else {
            std::cout << static_cast<unsigned int>(optCmd) << std::endl;
        }
    }

    BACSTAC_PROPERTY_CONTENTS pc_out;
    BACstacInitContentsToVar(&pc_out, DATA_TYPE_OPT_CMD_UNSIGNED, &optCmd, sizeof(optCmd));
    return sendReadResponse(hTsm, pInfo, &pc_out);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleReadPresentValue(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo, ObjectMapping* mapping)
{
    if (mapping == nullptr || pInfo == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const uint32_t prop_id = pInfo->ePropertyID;

    if (readOutOfService(mapping) == BACSTAC_TRUE || prop_id == PROP_RELINQUISH_DEFAULT) {
        return HOOK_STATUS_DEFAULT;
    }

    double raw_val = 0.0;
    if (instance_->modbus_ == nullptr) {
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }
    bool read_ok = instance_->modbus_->readValue(*mapping, raw_val);
    if (!read_ok) {
        // 이상 상황 로그: BACNET_LOG 설정과 무관하게 항상 stderr 로 출력
        std::cerr << "[" << ModbusClient::getMsTimestamp() << "][BACnet TX] ReadProperty Error: Modbus read failed for '"
                  << mapping->name << "' (Instance=" << mapping->instance << ")" << std::endl;
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }

    if (instance_->enable_log_) {
        std::cout << "[" << ModbusClient::getMsTimestamp() << "][BACnet TX] ReadProperty Response: '"
                  << mapping->name << "' val=" << raw_val << std::endl;
    }

    BACSTAC_PROPERTY_CONTENTS  pc_out;
    const bool is_binary = isBinaryObject(mapping->bacnet_type);

    if (is_binary) {
        const BACSTAC_POLARITY polarity = readPolarity(mapping);
        const double logical_value = toLogicalBinaryValue(raw_val, polarity);
        BACSTAC_ENUM enum_val = (logical_value != 0.0)
                                  ? BACSTAC_BINARY_ACTIVE
                                  : BACSTAC_BINARY_INACTIVE;

        BACstacInitContentsToVar(&pc_out, DATA_TYPE_ENUM, &enum_val, sizeof(enum_val));
    } else {
        BACSTAC_REAL real_val = static_cast<BACSTAC_REAL>(raw_val);
        BACstacInitContentsToVar(&pc_out, DATA_TYPE_REAL, &real_val, sizeof(real_val));
    }

    return sendReadResponse(hTsm, pInfo, &pc_out);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleReadActiveCOVSubscriptions(
    BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo)
{
    if (instance_ == nullptr || pInfo == nullptr || instance_->cov_ == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const auto subs = instance_->cov_->getActiveSubscriptions();

    std::cout << "[" << getMsTimestamp() << "][BACnet HOOK] handleReadActiveCOVSubscriptions called. Subs count: "
              << subs.size() << std::endl;

    std::vector<BACSTAC_COV_SUBSCRIPTION> cov_subs(subs.size());
    for (size_t i = 0; i < subs.size(); ++i) {
        const auto& s = subs[i];
        BACSTAC_COV_SUBSCRIPTION& cs = cov_subs[i];

        BACSTAC_RECIPIENT recipient;
        BACstacInitRecipientAddress(&recipient, &s.destAddress);

        BACSTAC_RECIPIENT_PROCESS recipProc;
        recipProc.recipient = recipient;
        recipProc.processID = s.processID;

        BACSTAC_OBJ_PROP_REF objPropRef;
        objPropRef.objectID.type = static_cast<BACSTAC_ENUM>(s.objType);
        objPropRef.objectID.instNumber = s.instance;
        objPropRef.propID = s.propID;
        objPropRef.index = s.arrayIndex;

        BACstacInitCOVSubs(
            &cs,
            &recipProc,
            &objPropRef,
            s.confirmed ? BACSTAC_TRUE : BACSTAC_FALSE,
            static_cast<BACSTAC_UNSIGNED>(s.timeRemaining));

        if (s.hasIncrement) {
            BACstacSetCOVSubsIncrement(&cs, static_cast<BACSTAC_REAL>(s.covIncrement));
        }

        std::cout << "  - Sub[" << i << "]: ProcessID=" << s.processID
                  << ", Obj=(" << s.objType << "," << s.instance << ")"
                  << ", Confirmed=" << (s.confirmed ? "Y" : "N")
                  << ", TimeRemaining=" << s.timeRemaining
                  << ", AddrLen=" << static_cast<int>(s.destAddress.addressLength)
                  << ", NetNum=" << s.destAddress.netNumber << std::endl;
    }

    BACSTAC_PROPERTY_CONTENTS pc;
    if (cov_subs.empty()) {
        BACstacInitContentsToArray(
            &pc, DATA_TYPE_COV_SUBSCRIPTION, nullptr, 0, 0);
    } else {
        BACstacInitContentsToArray(
            &pc,
            DATA_TYPE_COV_SUBSCRIPTION,
            cov_subs.data(),
            static_cast<BACSTAC_VALUE_SIZE>(cov_subs.size() * sizeof(BACSTAC_COV_SUBSCRIPTION)),
            static_cast<BACSTAC_ELEMENT_COUNT>(cov_subs.size())
        );
    }

    return sendReadResponse(hTsm, pInfo, &pc);
}

// ---------------------------------------------------------------------------
// writePropHook()
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::writePropHook(
    BACSTAC_HTSM              hTransaction,
    BACSTAC_ADDRESS*          sourceAddress,
    BACSTAC_ADDRESS*          destinationAddress,
    const BACSTAC_WRITE_INFO* pServiceInfo)
{
    (void)destinationAddress;

    if (!validateWriteHookInput(pServiceInfo)) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_OBJECT_TYPE objType =
        static_cast<BACSTAC_OBJECT_TYPE>(pServiceInfo->objectID.type);
    const uint32_t inst    = pServiceInfo->objectID.instNumber;
    const uint32_t prop_id = pServiceInfo->ePropertyID;

    // ── Device 객체 쓰기 처리 (BTL 7.3.2.10.5/6/8/9/10) ─────────────────────
    if (objType == OBJ_DEVICE) {
        return handleWriteDeviceProperty(hTransaction, pServiceInfo);
    }

    // ── Network Port 객체: IP 모드/FD 설정만 쓰기 허용, 나머지 거부 (BTL 7.3.2.46.1.5) ──
    if (objType == OBJ_NETWORK_PORT) {
        return handleWriteNetworkPort(hTransaction, sourceAddress, pServiceInfo);
    }

    ObjectMapping* mapping = instance_->findMapping(objType, inst);
    if (mapping == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    if (prop_id == PROP_POLARITY) {
        if (mapping->bacnet_type == OBJ_BINARY_VALUE) {
            return sendServiceError(hTransaction, ERR_CLASS_PROPERTY, ERR_CODE_UNKNOWN_PROPERTY);
        }
        return handleWritePolarity(hTransaction, pServiceInfo, mapping);
    }
    if (prop_id == PROP_COV_INCREMENT) {
        return handleWriteCovIncrement(hTransaction, pServiceInfo, mapping);
    }
    if (prop_id == PROP_OUT_OF_SERVICE) {
#ifdef OUT_OF_SERVICE
        return handleWriteOutOfService(hTransaction, pServiceInfo, mapping, objType);
#else
        return sendServiceError(hTransaction, ERR_CLASS_PROPERTY, ERR_CODE_WRITE_ACCESS_DENIED);
#endif
    }
    if (prop_id == PROP_PRESENT_VALUE) {
        return handleWritePresentValue(hTransaction, pServiceInfo, mapping, objType);
    }

    return HOOK_STATUS_DEFAULT;
}

// ── writePropHook 세분화 핸들러 ──────────────────────────────────────────────

// ---------------------------------------------------------------------------
// handleWriteDeviceProperty() — Device 객체 속성 쓰기
// ---------------------------------------------------------------------------
// 설계 방침:
//  - Device 객체 속성(Object_Name, Object_Identifier 등)은 Read-Only 정책
//  - 네트워크를 통한 변경 요청 시 WRITE_ACCESS_DENIED 에러 반환
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteDeviceProperty(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo)
{
    const uint32_t prop_id = pInfo->ePropertyID;

    // Device 객체의 속성 쓰기 요청은 Read-Only 정책에 따라 WRITE_ACCESS_DENIED 반환
    if (prop_id == PROP_OBJECT_NAME || prop_id == PROP_OBJECT_IDENTIFIER || prop_id == PROP_UTC_OFFSET) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_WRITE_ACCESS_DENIED);
    }

    // 그 외 Device 속성은 BACstac 기본 처리에 위임
    return HOOK_STATUS_DEFAULT;
}

// ---------------------------------------------------------------------------
// handleWriteNetworkPort() — Network Port 쓰기 처리
// ---------------------------------------------------------------------------
// 설계 방침:
//  - BACnet_IP_Mode / FD_BBMD_Address / FD_Subscription_Lifetime 만 쓰기를 허용하고,
//    Changes_Pending / ReinitializeDevice(ACTIVATE_CHANGES) 없이 즉시 반영한다.
//    BACstac은 실행 중 B/IP 설정 변경 API가 없으므로(7.10_doc pg 1405행)
//    응답 후 게이트웨이를 재실행해 반영한다.
//  - 그 외 속성은 Read-Only 정책에 따라 WRITE_ACCESS_DENIED (BTL 7.3.2.46.1.5)
//  - Changes_Pending 은 readPropHook 에서 항상 FALSE 로 응답한다.
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteNetworkPort(
    BACSTAC_HTSM hTsm, const BACSTAC_ADDRESS* source, const BACSTAC_WRITE_INFO* pInfo)
{
    const uint32_t prop_id = pInfo->ePropertyID;
    if (prop_id == kNetworkPortCommandProperty) {
        // FD 모니터가 TTL 5분 초과 시 보내는 자기 재등록(RENEW_FD_REGISTRATION)만 BACstac에 넘긴다.
        // 외부에서 온 Command 쓰기는 Property_List에 없는 속성이므로 UNKNOWN_PROPERTY.
        if (FdMonitor::isSelfRenewRequest(source) && instance_ != nullptr &&
            instance_->bip_mode_str_ == "fd") {
            return HOOK_STATUS_DEFAULT;
        }
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_UNKNOWN_PROPERTY);
    }
    if (prop_id != PROP_BIP_MODE && prop_id != PROP_FD_BBMD_ADDRESS &&
        prop_id != PROP_FD_SUBSCRIPTION_LIFETIME) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_WRITE_ACCESS_DENIED);
    }
    if (pInfo->nIndex != BACSTAC_VOID_INDEX) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_PROPERTY_IS_NOT_AN_ARRAY);
    }
    if (instance_ == nullptr || instance_->bip_config_path_.empty()) {
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }

    if (prop_id == PROP_BIP_MODE) {
        return handleWriteBipMode(hTsm, pInfo);
    }
    if (prop_id == PROP_FD_BBMD_ADDRESS) {
        return handleWriteFdBbmdAddress(hTsm, pInfo);
    }
    return handleWriteFdSubscriptionLifetime(hTsm, pInfo);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteBipMode(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo)
{
    if (BACstacGetContentsType(pInfo->pValue) != DATA_TYPE_ENUM) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
    }
    const BACSTAC_ENUM mode = BACstacGetContentsEnum(pInfo->pValue);
    // 이 제품은 BBMD 기능을 제공하지 않으므로 NORMAL / FOREIGN 만 허용한다.
    if (mode != BIP_MODE_NORMAL && mode != BIP_MODE_FOREIGN) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
    }

    const std::string requested = mode == BIP_MODE_FOREIGN ? "fd" : "normal";
    if (requested == instance_->bip_mode_str_) {
        return sendResponse(hTsm);
    }

    const char* suffix = mode == BIP_MODE_FOREIGN ? bip_config_file::kForeignSuffix
                                                  : bip_config_file::kNormalSuffix;
    const std::string link_path = instance_->bip_config_path_;
    const std::string target_path = bip_config_file::siblingPath(link_path, suffix);
    if (::access(target_path.c_str(), R_OK) != 0) {
        std::cerr << "[Gateway] Cannot switch BACnet/IP mode: " << target_path
                  << " is not readable." << std::endl;
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }
    if (!bip_config_file::replaceSymlink(link_path, bip_config_file::siblingName(link_path, suffix))) {
        std::cerr << "[Gateway] Failed to relink " << link_path << " to " << target_path
                  << " (errno=" << errno << ")" << std::endl;
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }

    std::cout << "[Gateway] BACnet_IP_Mode written: " << instance_->bip_mode_str_ << " -> "
              << requested << " (" << link_path << " -> " << target_path << ")" << std::endl;
    const BACSTAC_HOOK_STATUS hookStatus = sendResponse(hTsm);
    requestRestart("BACnet_IP_Mode changed");
    return hookStatus;
}

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteFdBbmdAddress(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo)
{
    if (BACstacGetContentsType(pInfo->pValue) != DATA_TYPE_HOST_NPORT) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
    }
    const BACSTAC_HOST_NPORT* hostNPort =
        static_cast<const BACSTAC_HOST_NPORT*>(BACstacGetContentsBufferPtr(pInfo->pValue));
    const BACSTAC_HOST_ADDRESS* host =
        hostNPort != nullptr ? BACstacGetHostNPortHostPtr(hostNPort) : nullptr;
    // 호스트 이름은 지원하지 않으며 IPv4 주소만 허용한다.
    if (host == nullptr || BACstacGetHostAddressType(host) != HOST_ADDR_ADDRESS ||
        BACstacGetHostAddressIpAddressLength(host) != 4U) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
    }
    const BACSTAC_BYTE* ip = BACstacGetHostAddressIpAddressPtr(host);
    const BACSTAC_UI16 port = BACstacGetHostNPortPort(hostNPort);
    if (ip == nullptr || port == 0U || (ip[0] == 0U && ip[1] == 0U && ip[2] == 0U && ip[3] == 0U)) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
    }

    const uint8_t ipBytes[4] = { ip[0], ip[1], ip[2], ip[3] };
    const std::string ipString = bip_config_file::formatIpv4(ipBytes);
    std::map<std::string, std::string> values;
    values["fd.bbmd_ip"]   = ipString;
    values["fd.bbmd_port"] = std::to_string(static_cast<unsigned int>(port));
    const std::string fd_path = bip_config_file::siblingPath(instance_->bip_config_path_,
                                                             bip_config_file::kForeignSuffix);
    if (!bip_config_file::updateFile(fd_path, values)) {
        std::cerr << "[Gateway] Failed to update " << fd_path << " for FD_BBMD_Address." << std::endl;
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }

    const bool changed = ipString != instance_->fd_bbmd_ip_ || port != instance_->fd_bbmd_port_;
    instance_->fd_bbmd_ip_   = ipString;
    instance_->fd_bbmd_port_ = static_cast<uint16_t>(port);
    std::cout << "[Gateway] FD_BBMD_Address written: " << ipString << ':' << port
              << " (saved to " << fd_path << ")" << std::endl;

    const BACSTAC_HOOK_STATUS hookStatus = sendResponse(hTsm);
    if (changed && instance_->bip_mode_str_ == "fd") {
        requestRestart("FD_BBMD_Address changed");
    }
    return hookStatus;
}

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteFdSubscriptionLifetime(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo)
{
    if (BACstacGetContentsType(pInfo->pValue) != DATA_TYPE_UNSIGNED) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
    }
    const BACSTAC_UNSIGNED ttl = BACstacGetContentsUnsigned(pInfo->pValue);
    // Register-Foreign-Device의 Time-to-Live는 Unsigned16 이며, 0(만료 없음)은 허용하지 않는다.
    if (ttl == 0U || ttl > 65535U) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
    }

    std::map<std::string, std::string> values;
    values["fd.ttl"] = std::to_string(static_cast<unsigned long>(ttl));
    const std::string fd_path = bip_config_file::siblingPath(instance_->bip_config_path_,
                                                             bip_config_file::kForeignSuffix);
    if (!bip_config_file::updateFile(fd_path, values)) {
        std::cerr << "[Gateway] Failed to update " << fd_path << " for FD_Subscription_Lifetime." << std::endl;
        return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
    }

    const bool changed = ttl != instance_->fd_ttl_;
    instance_->fd_ttl_ = static_cast<uint16_t>(ttl);
    std::cout << "[Gateway] FD_Subscription_Lifetime written: " << ttl
              << "s (saved to " << fd_path << ")" << std::endl;

    const BACSTAC_HOOK_STATUS hookStatus = sendResponse(hTsm);
    if (changed && instance_->bip_mode_str_ == "fd") {
        requestRestart("FD_Subscription_Lifetime changed");
    }
    return hookStatus;
}

void BacnetGateway::requestRestart(const char* reason)
{
    if (instance_ == nullptr) {
        return;
    }
    if (!instance_->restart_requested_.exchange(true)) {
        std::cout << "[Gateway] " << reason << ". Gateway restart requested to apply BACnet/IP settings."
                  << std::endl;
    }
}

// ---------------------------------------------------------------------------
// loadFdConfig() — bip.cfg.fd 의 FD 설정 로드 (모드와 무관)
// ---------------------------------------------------------------------------

void BacnetGateway::loadFdConfig()
{
    const std::string fd_path = bip_config_file::siblingPath(bip_config_path_,
                                                             bip_config_file::kForeignSuffix);
    std::string content;
    if (!bip_config_file::readFile(fd_path, content)) {
        std::cout << "[Gateway] FD config not found: " << fd_path
                  << " (FD_BBMD_Address/FD_Subscription_Lifetime use defaults)" << std::endl;
        return;
    }

    std::string value;
    unsigned long number = 0UL;
    uint8_t ipBytes[4] = { 0U, 0U, 0U, 0U };
    if (bip_config_file::findValue(content, "fd.bbmd_ip", value) &&
        bip_config_file::parseIpv4(value, ipBytes)) {
        fd_bbmd_ip_ = bip_config_file::formatIpv4(ipBytes);
    }
    if (bip_config_file::findValue(content, "fd.bbmd_port", value) &&
        parseConfigUnsigned(value, 65535UL, number)) {
        fd_bbmd_port_ = static_cast<uint16_t>(number);
    }
    if (bip_config_file::findValue(content, "fd.ttl", value) &&
        parseConfigUnsigned(value, 65535UL, number)) {
        fd_ttl_ = static_cast<uint16_t>(number);
    }
    if (bip_config_file::findValue(content, "fd.reg_timeout", value) &&
        parseConfigUnsigned(value, 0xFFFFFFFFUL, number)) {
        fd_reg_timeout_ms_ = static_cast<uint32_t>(number);
    }
    std::cout << "[Gateway] FD config loaded from " << fd_path << ": BBMD=" << fd_bbmd_ip_ << ':'
              << fd_bbmd_port_ << ", TTL=" << fd_ttl_ << "s, reg_timeout=" << fd_reg_timeout_ms_
              << "ms" << std::endl;
}

// ---------------------------------------------------------------------------
// incrementDatabaseRevision() — Device Database_Revision +1 (BTL 7.3.2.10.5/6)
// ---------------------------------------------------------------------------

void BacnetGateway::incrementDatabaseRevision()
{
    if (instance_ == nullptr || instance_->hLocalDev_ == nullptr) { return; }
    BACSTAC_HOBJECT hDevObj = BACstacGetDeviceObjectHandle(instance_->hLocalDev_);
    if (hDevObj == nullptr) { return; }

    BACSTAC_UNSIGNED cur_rev = 0U;
    BACSTAC_PROPERTY_CONTENTS pc;
    BACstacInitContentsToVar(&pc, DATA_TYPE_UNSIGNED, &cur_rev, sizeof(cur_rev));
    if (BACstacRetrievePropertyInstance2(hDevObj, PROP_DATABASE_REVISION,
                                          BACSTAC_VOID_INDEX, &pc,
                                          nullptr) == BACSTAC_STATUS_OK) {
        cur_rev = BACstacGetContentsUnsigned(&pc);
    }
    ++cur_rev; // 32-bit 오버플로우 시 wrapping (표준 허용)
    instance_->database_revision_ = cur_rev;
    BACSTAC_PROPERTY_CONTENTS pc_new;
    BACstacInitContentsToVar(&pc_new, DATA_TYPE_UNSIGNED, &cur_rev, sizeof(cur_rev));
    BACstacStorePropertyInstance(hDevObj, PROP_DATABASE_REVISION,
                                  BACSTAC_VOID_INDEX, &pc_new);
    if (instance_->enable_log_) {
        std::cout << "[Gateway] Database_Revision incremented to " << cur_rev << std::endl;
    }
}

bool BacnetGateway::validateWriteHookInput(const BACSTAC_WRITE_INFO* pInfo)
{
    return instance_ != nullptr && pInfo != nullptr && pInfo->pValue != nullptr;
}

bool BacnetGateway::decodeWriteValue(
    BACSTAC_HTSM hTsm, const BACSTAC_PROPERTY_CONTENTS* pValue,
    double& value, bool& isNull)
{
    if (pValue == nullptr) {
        (void)sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
        return false;
    }

    value = 0.0;
    isNull = false;
    switch (BACstacGetContentsType(pValue)) {
        case DATA_TYPE_NULL:
            isNull = true;
            return true;
        case DATA_TYPE_REAL:
            value = static_cast<double>(BACstacGetContentsReal(pValue));
            return true;
        case DATA_TYPE_UNSIGNED:
            value = static_cast<double>(BACstacGetContentsUnsigned(pValue));
            return true;
        case DATA_TYPE_INTEGER:
            value = static_cast<double>(BACstacGetContentsInteger(pValue));
            return true;
        case DATA_TYPE_ENUM:
            value = static_cast<double>(BACstacGetContentsEnum(pValue));
            return true;
        case DATA_TYPE_BOOLEAN:
            value = BACstacGetContentsBool(pValue) ? 1.0 : 0.0;
            return true;
        default:
            (void)sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
            return false;
    }
}

bool BacnetGateway::validateWriteRange(
    BACSTAC_HTSM hTsm, const ObjectMapping* mapping,
    BACSTAC_OBJECT_TYPE objType, double value, bool isNull)
{
    if (mapping == nullptr || isNull || !isAnalogObject(objType)) {
        return true;
    }

    if ((mapping->has_min_value && value < mapping->min_value) ||
        (mapping->has_max_value && value > mapping->max_value)) {
        (void)sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
        return false;
    }
    return true;
}

BACSTAC_BOOLEAN BacnetGateway::readOutOfService(const ObjectMapping* mapping)
{
    if (mapping == nullptr || mapping->hObject == nullptr) {
        return BACSTAC_FALSE;
    }

    BACSTAC_BOOLEAN value = BACSTAC_FALSE;
    BACSTAC_PROPERTY_CONTENTS contents;
    BACstacInitContentsToVar(&contents, DATA_TYPE_BOOLEAN, &value, sizeof(value));
    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_OUT_OF_SERVICE,
                                         BACSTAC_VOID_INDEX, &contents, nullptr) == BACSTAC_STATUS_OK) {
        value = BACstacGetContentsBool(&contents) ? BACSTAC_TRUE : BACSTAC_FALSE;
    }
    return value;
}

BACSTAC_POLARITY BacnetGateway::readPolarity(const ObjectMapping* mapping)
{
    if (mapping == nullptr || mapping->hObject == nullptr) {
        return POLARITY_NORMAL;
    }

    BACSTAC_ENUM value = static_cast<BACSTAC_ENUM>(POLARITY_NORMAL);
    BACSTAC_PROPERTY_CONTENTS contents;
    BACstacInitContentsToVar(&contents, DATA_TYPE_ENUM, &value, sizeof(value));
    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_POLARITY,
                                         BACSTAC_VOID_INDEX, &contents, nullptr) == BACSTAC_STATUS_OK) {
        value = BACstacGetContentsEnum(&contents);
    }
    return static_cast<BACSTAC_POLARITY>(value);
}

double BacnetGateway::toLogicalBinaryValue(double rawValue, BACSTAC_POLARITY polarity)
{
    const bool active = rawValue != 0.0;
    const bool logicalActive = active ^ (polarity == POLARITY_REVERSE);
    return logicalActive ? 1.0 : 0.0;
}

double BacnetGateway::coerceOutputValue(
    const ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType, double value)
{
    // FLOAT32는 16비트 정수 레지스터 반올림(int16_t round) 양자화가 불필요하므로 원값을 유지하며,
    // scale == 0.0인 경우 0으로 나누기 방지를 위해 원값을 그대로 반환합니다.
    if (mapping == nullptr || objType != OBJ_ANALOG_OUTPUT ||
        mapping->data_type == DataType::FLOAT32 || mapping->scale == 0.0) {
        return value;
    }

    const int16_t registerValue = static_cast<int16_t>(
        std::round((value - mapping->offset_val) / mapping->scale));
    return static_cast<double>(registerValue) * mapping->scale + mapping->offset_val;
}

void BacnetGateway::storePresentValue(
    ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType, double value)
{
    if (mapping == nullptr || mapping->hObject == nullptr) {
        return;
    }

    BACSTAC_PROPERTY_CONTENTS contents;
    if (isBinaryObject(objType)) {
        BACSTAC_ENUM enumValue = (value != 0.0)
                                   ? BACSTAC_BINARY_ACTIVE
                                   : BACSTAC_BINARY_INACTIVE;
        BACstacInitContentsToVar(&contents, DATA_TYPE_ENUM,
                                 &enumValue, sizeof(enumValue));
    } else {
        BACSTAC_REAL realValue = static_cast<BACSTAC_REAL>(value);
        BACstacInitContentsToVar(&contents, DATA_TYPE_REAL,
                                 &realValue, sizeof(realValue));
    }
    (void)BACstacStorePropertyInstance(mapping->hObject, PROP_PRESENT_VALUE,
                                       BACSTAC_VOID_INDEX, &contents);
}

double BacnetGateway::readRelinquishDefault(
    const ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType)
{
    if (mapping == nullptr || mapping->hObject == nullptr) {
        return 0.0;
    }

    BACSTAC_PROPERTY_CONTENTS contents;
    if (objType == OBJ_ANALOG_OUTPUT) {
        BACSTAC_REAL value = 0.0f;
        BACstacInitContentsToVar(&contents, DATA_TYPE_REAL, &value, sizeof(value));
        if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_RELINQUISH_DEFAULT,
                                             BACSTAC_VOID_INDEX, &contents, nullptr) == BACSTAC_STATUS_OK) {
            return static_cast<double>(BACstacGetContentsReal(&contents));
        }
    } else {
        BACSTAC_ENUM value = BACSTAC_BINARY_INACTIVE;
        BACstacInitContentsToVar(&contents, DATA_TYPE_ENUM, &value, sizeof(value));
        if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_RELINQUISH_DEFAULT,
                                             BACSTAC_VOID_INDEX, &contents, nullptr) == BACSTAC_STATUS_OK) {
            return static_cast<double>(BACstacGetContentsEnum(&contents));
        }
    }
    return 0.0;
}

void BacnetGateway::updatePriorityArray(
    ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType,
    uint8_t effectivePriority, double requestedValue, bool isNull,
    BACSTAC_PRI_ARRAY_ITEM* priorityArray, double& effectiveValue,
    bool& activeWrite)
{
    effectiveValue = requestedValue;
    activeWrite = true;
    initializePriorityArray(priorityArray);
    if (mapping == nullptr || mapping->hObject == nullptr || priorityArray == nullptr) {
        return;
    }
    if (effectivePriority == 0U ||
        effectivePriority > static_cast<uint8_t>(kPriorityArraySize)) {
        return;
    }

    BACSTAC_PROPERTY_CONTENTS contents;
    BACstacInitContentsToArray(&contents, DATA_TYPE_PRI_ARRAY_ITEM,
                               priorityArray, sizeof(BACSTAC_PRI_ARRAY_ITEM) * kPriorityArraySize,
                               static_cast<BACSTAC_ELEMENT_COUNT>(kPriorityArraySize));
    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRIORITY_ARRAY,
                                         BACSTAC_ENTIRE_ARRAY, &contents, nullptr) != BACSTAC_STATUS_OK) {
        return;
    }

    const std::size_t priorityIndex = static_cast<std::size_t>(effectivePriority - 1U);
    if (isNull) {
        BACstacSetPriArrayItemNull(&priorityArray[priorityIndex]);
    } else if (objType == OBJ_ANALOG_OUTPUT) {
        const double coercedValue = coerceOutputValue(mapping, objType, requestedValue);
        (void)BACstacSetPriArrayItemReal(&priorityArray[priorityIndex],
                                         static_cast<BACSTAC_REAL>(coercedValue));
    } else {
        const BACSTAC_ENUM enumValue = (requestedValue != 0.0)
                                        ? BACSTAC_BINARY_ACTIVE
                                        : BACSTAC_BINARY_INACTIVE;
        (void)BACstacSetPriArrayItemEnum(&priorityArray[priorityIndex], enumValue);
    }

    bool foundActive = false;
    int activePriority = static_cast<int>(kPriorityArraySize) + 1;
    for (std::size_t i = 0; i < kPriorityArraySize; ++i) {
        if (BACstacIsPriArrayItemNull(&priorityArray[i])) {
            continue;
        }

        if (objType == OBJ_ANALOG_OUTPUT) {
            effectiveValue = static_cast<double>(BACstacGetPriArrayItemReal(&priorityArray[i]));
        } else {
            effectiveValue = static_cast<double>(BACstacGetPriArrayItemEnum(&priorityArray[i]));
        }
        activePriority = static_cast<int>(i) + 1;
        foundActive = true;
        break;
    }

    if (!foundActive) {
        effectiveValue = readRelinquishDefault(mapping, objType);
    }
    activeWrite = !foundActive || effectivePriority <= static_cast<uint8_t>(activePriority);
}

void BacnetGateway::storePriorityArray(
    ObjectMapping* mapping, BACSTAC_PRI_ARRAY_ITEM* priorityArray)
{
    if (mapping == nullptr || mapping->hObject == nullptr || priorityArray == nullptr) {
        return;
    }

    for (std::size_t i = 0; i < kPriorityArraySize; ++i) {
        BACSTAC_PROPERTY_CONTENTS contents;
        BACstacInitContentsToVar(&contents, DATA_TYPE_PRI_ARRAY_ITEM,
                                 &priorityArray[i], sizeof(priorityArray[i]));
        (void)BACstacStorePropertyInstance(
            mapping->hObject, PROP_PRIORITY_ARRAY,
            static_cast<BACSTAC_ARRAY_INDEX>(i + 1U), &contents);
    }
}

bool BacnetGateway::writeMappedValue(
    ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType, double logicalValue)
{
    (void)objType;
    if (instance_ == nullptr || instance_->modbus_ == nullptr || mapping == nullptr) {
        return false;
    }

    double physicalValue = logicalValue;
#ifdef BINARY_OUTPUT_SUPPORT
    if (objType == OBJ_BINARY_OUTPUT) {
        physicalValue = toLogicalBinaryValue(logicalValue, readPolarity(mapping));
    }
#endif
    return instance_->modbus_->writeValue(*mapping, physicalValue);
}

void BacnetGateway::notifyCov(ObjectMapping* mapping, double value, bool useIncrement)
{
    if (mapping == nullptr) {
        return;
    }

    const bool changed = useIncrement
                       ? std::abs(value - mapping->cov_last_value) >= mapping->cov_increment
                       : value != mapping->cov_last_value;
    if (!changed) {
        return;
    }

    mapping->cov_last_value = value;
    if (instance_ != nullptr && instance_->cov_ != nullptr) {
        instance_->cov_->sendNotification(mapping, value);
    }
}

BACSTAC_HOOK_STATUS BacnetGateway::handleWritePolarity(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo, ObjectMapping* mapping)
{
    if (pInfo == nullptr || mapping == nullptr || instance_ == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_DATA_TYPE dataType = BACstacGetContentsType(pInfo->pValue);
    if (dataType != DATA_TYPE_ENUM) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_ENUM new_pol = BACstacGetContentsEnum(pInfo->pValue);
    const BACSTAC_POLARITY old_pol = readPolarity(mapping);

    if (new_pol != static_cast<BACSTAC_ENUM>(old_pol)) {
        BACSTAC_PROPERTY_CONTENTS pcPol;
        BACstacInitContentsToVar(&pcPol, DATA_TYPE_ENUM, &new_pol, sizeof(new_pol));
        (void)BACstacStorePropertyInstance(mapping->hObject, PROP_POLARITY,
                                           BACSTAC_VOID_INDEX, &pcPol);

#ifdef BINARY_OUTPUT_SUPPORT
        if (mapping->bacnet_type == OBJ_BINARY_OUTPUT) {
            // [BO 처리] 논리적 Present_Value는 유지되므로, 새 Polarity를 반영한 물리값을 Modbus에 Write
            BACSTAC_ENUM enum_pv = BACSTAC_BINARY_INACTIVE;
            BACSTAC_PROPERTY_CONTENTS pcPv;
            BACstacInitContentsToVar(&pcPv, DATA_TYPE_ENUM, &enum_pv, sizeof(enum_pv));
            if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRESENT_VALUE,
                                                 BACSTAC_VOID_INDEX, &pcPv, nullptr) == BACSTAC_STATUS_OK) {
                enum_pv = BACstacGetContentsEnum(&pcPv);
            }
            const double logical_val = (enum_pv == BACSTAC_BINARY_ACTIVE) ? 1.0 : 0.0;

            const double new_physical = toLogicalBinaryValue(
                logical_val, static_cast<BACSTAC_POLARITY>(new_pol));
            if (instance_->modbus_ != nullptr) {
                instance_->modbus_->writeValue(*mapping, new_physical);
            }
        } else
#endif
        {
            // [BI 처리] 물리값은 고정되어 있으므로 새 Polarity에 따른 논리값 변화 감지 후 COV 통지
            double raw_val = 0.0;
            if (instance_->modbus_ != nullptr &&
                instance_->modbus_->readValue(*mapping, raw_val)) {
                const double old_logical = toLogicalBinaryValue(raw_val, old_pol);
                const double new_logical = toLogicalBinaryValue(
                    raw_val, static_cast<BACSTAC_POLARITY>(new_pol));

                if (old_logical != new_logical) {
                    notifyCov(mapping, new_logical, false);
                }
            }
        }
    }

    return sendResponse(hTsm);
}

BACSTAC_HOOK_STATUS BacnetGateway::handleWriteCovIncrement(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo, ObjectMapping* mapping)
{
    const BACSTAC_DATA_TYPE dataType = BACstacGetContentsType(pInfo->pValue);
    if (dataType != DATA_TYPE_REAL) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_REAL cov_val = BACstacGetContentsReal(pInfo->pValue);
    if (cov_val <= 0.0f) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_VALUE_OUT_OF_RANGE);
    }

    mapping->cov_increment = static_cast<double>(cov_val);
    BACSTAC_PROPERTY_CONTENTS pcCov;
    BACstacInitContentsToVar(&pcCov, DATA_TYPE_REAL, &cov_val, sizeof(cov_val));
    (void)BACstacStorePropertyInstance(mapping->hObject, PROP_COV_INCREMENT,
                                       BACSTAC_VOID_INDEX, &pcCov);

    return sendResponse(hTsm);
}

#ifdef OUT_OF_SERVICE
BACSTAC_HOOK_STATUS BacnetGateway::handleWriteOutOfService(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo,
    ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType)
{
    if (pInfo == nullptr || mapping == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_DATA_TYPE dataType = BACstacGetContentsType(pInfo->pValue);
    BACSTAC_BOOLEAN new_oos = BACSTAC_FALSE;

    if (dataType == DATA_TYPE_BOOLEAN) {
        new_oos = BACstacGetContentsBool(pInfo->pValue) ? BACSTAC_TRUE : BACSTAC_FALSE;
    } else if (dataType == DATA_TYPE_ENUM) {
        new_oos = (BACstacGetContentsEnum(pInfo->pValue) != 0) ? BACSTAC_TRUE : BACSTAC_FALSE;
    } else {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_BOOLEAN old_oos = readOutOfService(mapping);

    if (new_oos != old_oos) {
        BACSTAC_PROPERTY_CONTENTS pcOOS;
        BACstacInitContentsToVar(&pcOOS, DATA_TYPE_BOOLEAN, &new_oos, sizeof(new_oos));
        (void)BACstacStorePropertyInstance(mapping->hObject, PROP_OUT_OF_SERVICE,
                                           BACSTAC_VOID_INDEX, &pcOOS);

        if (new_oos == BACSTAC_FALSE) {
            if (!isCommandableObject(objType)) {
                double raw_val = 0.0;
                if (instance_->modbus_ != nullptr &&
                    instance_->modbus_->readValue(*mapping, raw_val)) {
                    const double logicalValue = isBinaryObject(objType)
                                               ? toLogicalBinaryValue(raw_val, readPolarity(mapping))
                                               : raw_val;
                    storePresentValue(mapping, objType, logicalValue);
                    notifyCov(mapping, logicalValue, !isBinaryObject(objType));
                }
            } else {
                double presentValue = 0.0;
                BACSTAC_PROPERTY_CONTENTS pcPV;
#ifdef BINARY_OUTPUT_SUPPORT
                if (objType == OBJ_BINARY_OUTPUT) {
                    BACSTAC_ENUM pvValue = BACSTAC_BINARY_INACTIVE;
                    BACstacInitContentsToVar(&pcPV, DATA_TYPE_ENUM, &pvValue, sizeof(pvValue));
                    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRESENT_VALUE,
                                                         BACSTAC_VOID_INDEX, &pcPV, nullptr) == BACSTAC_STATUS_OK) {
                        presentValue = BACstacGetContentsEnum(&pcPV) != BACSTAC_BINARY_INACTIVE
                                     ? 1.0 : 0.0;
                    }

                    (void)writeMappedValue(mapping, objType, presentValue);
                } else
#endif
                {
                    BACSTAC_REAL pvValue = 0.0f;
                    BACstacInitContentsToVar(&pcPV, DATA_TYPE_REAL, &pvValue, sizeof(pvValue));
                    if (BACstacRetrievePropertyInstance2(mapping->hObject, PROP_PRESENT_VALUE,
                                                         BACSTAC_VOID_INDEX, &pcPV, nullptr) == BACSTAC_STATUS_OK) {
                        presentValue = static_cast<double>(BACstacGetContentsReal(&pcPV));
                    }
                    (void)writeMappedValue(mapping, objType, presentValue);
                }
            }
        }
    }

    return sendResponse(hTsm);
}
#endif

BACSTAC_HOOK_STATUS BacnetGateway::handleWritePresentValue(
    BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo,
    ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType)
{
    if (pInfo == nullptr || mapping == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    const BACSTAC_BOOLEAN out_of_service = readOutOfService(mapping);
    const bool is_commandable = isCommandableObject(objType);
    const bool is_input_obj = (objType == OBJ_ANALOG_INPUT ||
                               objType == OBJ_BINARY_INPUT ||
                               objType == OBJ_MULTISTATE_INPUT);

    // Input 객체 (AI/BI/MSI) 및 Read-Only 매핑은 Out_Of_Service == FALSE 일 때 쓰기 금지 (WRITE_ACCESS_DENIED)
    if (out_of_service == BACSTAC_FALSE && (is_input_obj || mapping->read_only)) {
        return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_WRITE_ACCESS_DENIED);
    }

    if (out_of_service == BACSTAC_TRUE && !is_commandable) {
        double value = 0.0;
        bool isNull = false;
        if (!decodeWriteValue(hTsm, pInfo->pValue, value, isNull)) {
            return HOOK_STATUS_OK;
        }
        if (isNull) {
            return sendServiceError(hTsm, ERR_CLASS_PROPERTY, ERR_CODE_INVALID_DATA_TYPE);
        }
        if (!validateWriteRange(hTsm, mapping, objType, value, false)) {
            return HOOK_STATUS_OK;
        }

        storePresentValue(mapping, objType, value);
        notifyCov(mapping, value, !isBinaryObject(objType));
        return sendResponse(hTsm);
    }

    double write_val = 0.0;
    bool is_null = false;
    if (!decodeWriteValue(hTsm, pInfo->pValue, write_val, is_null)) {
        return HOOK_STATUS_OK;
    }

    if (!validateWriteRange(hTsm, mapping, objType, write_val, is_null)) {
        return HOOK_STATUS_OK;
    }

    uint8_t effective_priority = 16U;
    bool has_write_priority = (pInfo->nWritePriority >= 1 && pInfo->nWritePriority <= 16);
    if (is_commandable && has_write_priority) {
        effective_priority = static_cast<uint8_t>(pInfo->nWritePriority);
    }

    if (out_of_service == BACSTAC_TRUE && is_commandable && !has_write_priority && !is_null) {
        const double coercedValue = coerceOutputValue(mapping, objType, write_val);
        storePresentValue(mapping, objType, coercedValue);
        notifyCov(mapping, write_val, true);
        return sendResponse(hTsm);
    }

    if (instance_->enable_log_) {
        std::cout << "[" << ModbusClient::getMsTimestamp() << "][BACnet RX] WriteProperty: Object Type="
                  << objType << ", Instance=" << mapping->instance
                  << ", PropertyID=" << pInfo->ePropertyID
                  << " (" << mapping->name << ")";
        if (is_commandable) {
            std::cout << ", Priority=" << static_cast<int>(effective_priority);
        }
        if (is_null) { std::cout << ", Value=NULL (Relinquish)"; }
        else         { std::cout << ", Value=" << write_val; }
        std::cout << std::endl;
    }

    BACSTAC_PRI_ARRAY_ITEM priorityArray[kPriorityArraySize];
    bool is_active_write = true;
    double effectiveValue = write_val;

    if (is_commandable) {
        updatePriorityArray(mapping, objType, effective_priority, write_val, is_null,
                             priorityArray, effectiveValue, is_active_write);
        write_val = effectiveValue;
    }

    if (is_active_write && out_of_service == BACSTAC_FALSE) {
        if (instance_->modbus_ == nullptr) {
            return sendServiceError(hTsm, ERR_CLASS_DEVICE, ERR_CODE_OPERATIONAL_PROBLEM);
        }

        double adjusted_write_val = write_val;
#ifdef BINARY_OUTPUT_SUPPORT
        if (objType == OBJ_BINARY_OUTPUT) {
            adjusted_write_val = toLogicalBinaryValue(write_val, readPolarity(mapping));
        }
#endif

        // Priority array 복사본 캡처용 구조체
        struct PriArrayHolder {
            BACSTAC_PRI_ARRAY_ITEM items[kPriorityArraySize];
        } holder;
        if (is_commandable) {
            std::memcpy(holder.items, priorityArray, sizeof(holder.items));
        }

        bool queued = instance_->modbus_->enqueueWrite(
            *mapping, adjusted_write_val,
            [hTsm, mapping, is_commandable, holder, write_val](bool success) {
                if (success) {
                    onAsyncWriteCompleted(hTsm, mapping, is_commandable,
                                          is_commandable ? holder.items : nullptr, write_val);
                } else {
                    onAsyncWriteFailed(hTsm, mapping);
                }
            },
            ModbusJobPriority::HIGH);

        if (!queued) {
            return sendServiceError(hTsm, ERR_CLASS_RESOURCES, ERR_CODE_NO_SPACE_TO_WRITE_PROP);
        }

        // 비동기 완료를 위해 Hook은 즉시 OK 리턴 (BACstac 블로킹 방지)
        return HOOK_STATUS_OK;
    }

    if (is_commandable) {
        storePriorityArray(mapping, priorityArray);
        notifyCov(mapping, write_val, false);
        return sendResponse(hTsm);
    }

    // 비-commandable 객체: 기본 DB 처리를 유도하여 안전하게 완료되도록 함
    return HOOK_STATUS_DEFAULT;
}

// ---------------------------------------------------------------------------
// readPropMultipleHook() / writePropMultipleHook()
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::readPropMultipleHook(
    BACSTAC_HTSM hTransaction, BACSTAC_ADDRESS* sourceAddress,
    BACSTAC_ADDRESS* destinationAddress, const void* pServiceInfo)
{
    (void)sourceAddress;
    (void)destinationAddress;
    (void)pServiceInfo;
    return rejectUnsupportedService(hTransaction, "ReadPropertyMultiple");
}

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::writePropMultipleHook(
    BACSTAC_HTSM hTransaction, BACSTAC_ADDRESS* sourceAddress,
    BACSTAC_ADDRESS* destinationAddress, const void* pServiceInfo)
{
    (void)sourceAddress;
    (void)destinationAddress;
    (void)pServiceInfo;
    return rejectUnsupportedService(hTransaction, "WritePropertyMultiple");
}

// ---------------------------------------------------------------------------
// subscribeCOVHook()
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::subscribeCOVHook(
    BACSTAC_HTSM                      hTransaction,
    BACSTAC_ADDRESS*                  sourceAddress,
    BACSTAC_ADDRESS*                  /*destinationAddress*/,
    const BACSTAC_SUBSCRIBE_COV_INFO* pServiceInfo)
{
    if (instance_ == nullptr || pServiceInfo == nullptr || sourceAddress == nullptr ||
        instance_->cov_ == nullptr) {
        if (hTransaction != BACSTAC_INVALID_HANDLE) {
            (void)BACstacDestroyTSM(hTransaction);
        }
        return HOOK_STATUS_OK;
    }

    const BACSTAC_OBJECT_TYPE objType =
        static_cast<BACSTAC_OBJECT_TYPE>(pServiceInfo->monitoredObjectID.type);
    const uint32_t inst = pServiceInfo->monitoredObjectID.instNumber;

    ObjectMapping* mapping = instance_->findMapping(objType, inst);
    if (mapping == nullptr) {
        if (instance_->enable_log_) {
            std::cout << "[" << getMsTimestamp()
                      << "][COV] SubscribeCOV rejected: Unknown Object ObjType=" << objType
                      << " Inst=" << inst << std::endl;
        }
        return sendServiceError(hTransaction, ERR_CLASS_OBJECT, ERR_CODE_UNKNOWN_OBJECT);
    }

    bool is_new_subscription = false;
    CovSubscriber sub;
    double initial_value = 0.0;
    bool send_initial = false;

    if (!pServiceInfo->fSubscribe) {
        instance_->cov_->unsubscribe(mapping, pServiceInfo->processID);
        if (instance_->enable_log_) {
            std::cout << "[" << getMsTimestamp() << "][COV] Unsubscribed: '"
                      << mapping->name << "' processID=" << pServiceInfo->processID << std::endl;
        }
    } else {
        sub.processID     = pServiceInfo->processID;
        sub.destAddress   = *sourceAddress;
        sub.confirmed     = pServiceInfo->issueConfirmedNotifications;
        sub.lifetimeSec   = pServiceInfo->lifetime;
        sub.subscribeTime = std::chrono::steady_clock::now();
        sub.propID        = PROP_PRESENT_VALUE;
        sub.arrayIndex    = BACSTAC_VOID_INDEX;
        if (!instance_->cov_->canSubscribe(mapping, pServiceInfo->processID, PROP_PRESENT_VALUE)) {
            if (instance_->enable_log_) {
                std::cout << "[" << getMsTimestamp()
                          << "][COV] SubscribeCOV rejected: Max capacity ("
                          << CovManager::kMaxCovSubscriptions << ") reached." << std::endl;
            }
            return sendServiceError(hTransaction, ERR_CLASS_RESOURCES, ERR_CODE_NO_SPACE_FOR_ELEMENT);
        }

        is_new_subscription = instance_->cov_->subscribe(mapping, sub);

        if (instance_->enable_log_) {
            std::cout << "[" << getMsTimestamp() << "][COV] "
                      << (is_new_subscription ? "Subscribed" : "Re-subscribed")
                      << ": '" << mapping->name
                      << "' processID=" << pServiceInfo->processID
                      << " confirmed=" << (pServiceInfo->issueConfirmedNotifications ? "Y" : "N")
                      << " lifetime=" << pServiceInfo->lifetime << "s"
                      << " (increment=" << mapping->cov_increment << ")" << std::endl;
        }
    }

    if (is_new_subscription && pServiceInfo->fSubscribe) {
        initial_value = mapping->cov_initialized ? mapping->cov_last_value : 0.0;
        if (!mapping->cov_initialized) {
            if (instance_->modbus_ != nullptr &&
                instance_->modbus_->readValue(*mapping, initial_value, true)) {
                mapping->cov_last_value  = initial_value;
                mapping->cov_initialized = true;
            }
        }
        send_initial = true;
    }

    const BACSTAC_HOOK_STATUS response = sendResponse(hTransaction);
    if (send_initial) {
        instance_->cov_->sendInitialNotification(mapping, sub, initial_value);
    }
    return response;
}

// ---------------------------------------------------------------------------
// dccHook()
// ---------------------------------------------------------------------------

BACSTAC_HOOK_STATUS BACstac_hook BacnetGateway::dccHook(
    BACSTAC_HTSM             hTransaction,
    BACSTAC_ADDRESS*         sourceAddress,
    BACSTAC_ADDRESS*         destinationAddress,
    const BACSTAC_DCC_INFO*  pServiceInfo)
{
    (void)sourceAddress;
    (void)destinationAddress;

    if (pServiceInfo == nullptr || instance_ == nullptr) {
        return HOOK_STATUS_DEFAULT;
    }

    if (instance_->enable_log_) {
        std::cout << "[" << getMsTimestamp() << "][BACnet RX] DeviceCommunicationControl: "
                  << "dccValue=" << static_cast<int>(pServiceInfo->dccValue)
                  << ", duration=" << pServiceInfo->nTimeDuration << std::endl;
    }

    BACstacSetDCCValue(instance_->hLocalDev_, pServiceInfo->dccValue);

    if (pServiceInfo->dccValue != DCC_ENABLE && pServiceInfo->nTimeDuration > 0) {
        const uint16_t duration_min = pServiceInfo->nTimeDuration;
        std::thread([duration_min]() {
            std::this_thread::sleep_for(std::chrono::minutes(duration_min));
            if (instance_ != nullptr && instance_->hLocalDev_ != nullptr) {
                BACstacSetDCCValue(instance_->hLocalDev_, DCC_ENABLE);
                if (instance_->enable_log_) {
                    std::cout << "[BACnet] DCC timer expired (" << duration_min
                              << " min). Communication automatically restored to DCC_ENABLE." << std::endl;
                }
            }
        }).detach();
    }

    if (instance_->enable_log_) {
        std::cout << "[" << getMsTimestamp() << "][BACnet TX] Sent DCC SimpleACK (Status OK)" << std::endl;
    }
    return sendResponse(hTransaction);
}

#ifdef ENABLE_TPI_EXPORT
bool BacnetGateway::exportCurrentDeviceTpi(const std::string& filepath) const
{
    return TpiExporter::exportCurrentDeviceTpi(filepath, bacnet_cfg_, mappings_, bip_mode_str_);
}
#endif

} // namespace gateway
