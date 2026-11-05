#pragma once

/**
 * @file bacnet_gateway.h
 * @brief Modbus RTU ↔ BACnet IP 게이트웨이 메인 조율 클래스 (Facade)
 *
 * 리팩토링 후 역할:
 *  - 레이어 조율: ConfigLoader → ModbusClient → CovManager → BACstac 초기화 순서 관리
 *  - BACnet Hook 핸들러 등록 및 응답 처리
 *  - BACnet 객체 등록 (start 내 단계별 private 메서드로 분리)
 *  - Network Port 속성 수동 처리
 *
 * 의존성 방향 (단방향):
 *   BacnetGateway → CovManager → ModbusClient → ISerialPort
 *                ↘ ConfigLoader (설정 로드 시)
 */

#include "types.h"
#include "config_loader.h"
#include "modbus_client.h"
#include "cov_manager.h"
#include "fd_monitor.h"
#include "net_watcher.h"
#include "../common/iserial_port.h"

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <bactypes.h>
#include <bacsrv.h>
#include <bacgtw.h>

namespace gateway {

/**
 * @brief Modbus RTU ↔ BACnet IP 게이트웨이 메인 클래스 (조율자 역할).
 *
 * 스레드 안전성:
 *  - readValue / writeValue 는 ModbusClient 내부에서 serial_mutex_ 로 보호됩니다.
 *  - running_ 은 std::atomic<bool> 을 사용합니다.
 */
class BacnetGateway {
public:
    /**
     * @brief 기본 생성자 (SerialPort 내부 소유).
     */
    BacnetGateway();

    /**
     * @brief 의존성 주입 생성자 (단위 테스트 / MockSerialPort 주입용).
     * @param port 외부에서 생성된 ISerialPort 구현체. 소유권을 이전받습니다.
     */
    explicit BacnetGateway(std::unique_ptr<modbus::ISerialPort> port);

    /// 소멸자: stop() 을 호출하여 BACstac 및 시리얼 포트를 안전하게 해제합니다.
    ~BacnetGateway();

    // 복사/이동 금지 (단일 인스턴스 전제)
    BacnetGateway(const BacnetGateway&)            = delete;
    BacnetGateway& operator=(const BacnetGateway&) = delete;
    BacnetGateway(BacnetGateway&&)                 = delete;
    BacnetGateway& operator=(BacnetGateway&&)      = delete;

    /**
     * @brief [v2.1] 공통 게이트웨이 설정 파일(gateway.json)을 파싱합니다.
     * @param gateway_config_path gateway.json 파일 경로
     * @return true: 성공
     */
    bool loadGatewayConfig(const std::string& gateway_config_path);

    /**
     * @brief bip.cfg 탐색 기준이 될 config 폴더를 지정합니다.
     * @param config_dir config 폴더 경로. bip.cfg는 이 폴더의 상위 폴더에서 로드합니다.
     *                   지정하지 않으면 현재 작업 폴더의 bip.cfg를 로드합니다.
     */
    void setConfigDir(const std::string& config_dir) { config_dir_ = config_dir; }

    /**
     * @brief Network Port 설정(BACnet_IP_Mode, FD_BBMD_Address, FD_Subscription_Lifetime)
     *        변경으로 게이트웨이 재시작이 필요한지 확인합니다. main 감시 루프가 확인 후 재실행합니다.
     */
    bool isRestartRequested() const { return restart_requested_.load(); }

    /**
     * @brief [v2] config 폴더 내의 기기별 .json 파일을 순차적으로 파싱합니다.
     */
    bool loadConfigDir(const std::string& config_dir,
                       std::string*       out_error = nullptr,
                       bool*              out_is_contradiction = nullptr);

    /**
     * @brief BACstac 서버 초기화, BACnet 객체 등록, 훅 설정을 수행합니다.
     * @param external_running 기동 동기화 중 종료 요청을 확인할 외부 플래그. nullptr이면 내부 플래그만 사용합니다.
     * @return true: 성공
     */
    bool start(const std::atomic<bool>* external_running = nullptr);

    /**
     * @brief BACstac 서버를 종료하고 시리얼 포트를 닫습니다.
     */
    void stop();

#ifdef ENABLE_TPI_EXPORT
    /**
     * @brief 현재 메모리에 로드 및 생성된 모든 디바이스/객체 속성을 TPI (PICS) 파일로 내보냅니다.
     * @param filepath 생성할 TPI 파일 경로 (기본값: "current_device.tpi")
     * @return true: 성공
     */
    bool exportCurrentDeviceTpi(const std::string& filepath = "current_device.tpi") const;
#endif

    // ── BACstac 훅 함수 (static) ─────────────────────────────────────────────

    /// ReadProperty 훅
    static BACSTAC_HOOK_STATUS BACstac_hook readPropHook(
        BACSTAC_HTSM             hTransaction,
        BACSTAC_ADDRESS*         sourceAddress,
        BACSTAC_ADDRESS*         destinationAddress,
        const BACSTAC_READ_INFO* pServiceInfo);

    /// WriteProperty 훅
    static BACSTAC_HOOK_STATUS BACstac_hook writePropHook(
        BACSTAC_HTSM              hTransaction,
        BACSTAC_ADDRESS*          sourceAddress,
        BACSTAC_ADDRESS*          destinationAddress,
        const BACSTAC_WRITE_INFO* pServiceInfo);

    /// ReadPropertyMultiple 훅 (B-ASC 인증: 미지원 → 즉시 Reject)
    static BACSTAC_HOOK_STATUS BACstac_hook readPropMultipleHook(
        BACSTAC_HTSM      hTransaction,
        BACSTAC_ADDRESS*  sourceAddress,
        BACSTAC_ADDRESS*  destinationAddress,
        const void*       pServiceInfo);

    /// WritePropertyMultiple 훅 (B-ASC 인증: 미지원 → 즉시 Reject)
    static BACSTAC_HOOK_STATUS BACstac_hook writePropMultipleHook(
        BACSTAC_HTSM      hTransaction,
        BACSTAC_ADDRESS*  sourceAddress,
        BACSTAC_ADDRESS*  destinationAddress,
        const void*       pServiceInfo);

    /// SubscribeCOV 훅
    static BACSTAC_HOOK_STATUS BACstac_hook subscribeCOVHook(
        BACSTAC_HTSM                      hTransaction,
        BACSTAC_ADDRESS*                  sourceAddress,
        BACSTAC_ADDRESS*                  destinationAddress,
        const BACSTAC_SUBSCRIBE_COV_INFO* pServiceInfo);

    /// DeviceCommunicationControl 훅 (DM-DCC-B)
    static BACSTAC_HOOK_STATUS BACstac_hook dccHook(
        BACSTAC_HTSM             hTransaction,
        BACSTAC_ADDRESS*         sourceAddress,
        BACSTAC_ADDRESS*         destinationAddress,
        const BACSTAC_DCC_INFO*  pServiceInfo);

private:
    // ── 공통 수명주기 처리 ────────────────────────────────────────────────────

    /// 두 생성자가 공유하는 인스턴스 및 환경 초기화
    void initialize();

    /// start() 중간 실패 시 이미 생성된 계층을 정리합니다.
    void cleanupStartFailure(bool bacstac_initialized);

    /// 등록된 Hook을 모두 해제합니다.
    static void unregisterHooks();

    // ── start() 분해 private 메서드 ─────────────────────────────────────────

    /// 시리얼 포트 열기 및 Modbus Executor 스레드 시작
    bool startModbusClient();

    /// 기동 시 AO/BO 객체의 Relinquish Default 초기 동기화
    bool syncRelinquishDefaults();

    /// 내부 실행 상태와 main의 종료 요청을 함께 확인합니다.
    bool shouldContinueRunning() const;

    /// BACstac 서버 초기화 (BIP 설정, APDU 설정 포함)
    bool initBACstacServer(BACSTAC_SRV_INIT& srv_init);

    /**
     * @brief BACstacCfgLoadBipConfig() 직후 실제 바인딩할 IPv4/netmask 를 확정합니다.
     *        - bip.cfg 에 iface=<이름> 이 있으면 그 인터페이스의 현재 IPv4 를 사용 (다중 NIC 고정)
     *        - 없으면 기존 ip= (BACstac 자동 대체 포함)를 사용하고 실제 netmask 로 subnet 보정
     *        사용 가능한 IPv4 가 없으면(링크 다운, DHCP 지연) 생길 때까지 대기(HOLD)합니다.
     * @return 종료 신호로 대기가 중단되면 false
     */
    bool resolveBipAddress();

    /// NetworkWatcher 시작 (B/IP 바인딩 인터페이스 링크/IPv4 감시)
    void startNetworkWatcher();

    /// 디바이스 객체 속성 설정 (name, model, description, services supported)
    bool configureDeviceObject(BACSTAC_HOBJECT hDevObj);

    /// 전체 BACnet 객체 등록 (AI/AO/BI/BO)
    bool registerAllObjects(BACSTAC_HDEVICE hDev);

    /// 매핑의 BACnet 유형에 맞는 객체 생성 helper
    static BACSTAC_STATUS createObjectForMapping(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);

    static BACSTAC_STATUS createAnalogInputObject(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);
    static BACSTAC_STATUS createAnalogOutputObject(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);
    static BACSTAC_STATUS createBinaryInputObject(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);
#ifdef BINARY_OUTPUT_SUPPORT
    static BACSTAC_STATUS createBinaryOutputObject(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);
#endif
    static BACSTAC_STATUS createBinaryValueObject(
        BACSTAC_HDEVICE hDev, ObjectMapping& mapping, BACSTAC_HOBJECT* phObject);

    /// 객체 등록에 공통으로 사용되는 유형/priority-array 처리
    static bool isSupportedObjectType(BACSTAC_OBJECT_TYPE objType);
    static bool isAnalogObject(BACSTAC_OBJECT_TYPE objType);
    static bool isBinaryObject(BACSTAC_OBJECT_TYPE objType);
    static bool isCommandableObject(BACSTAC_OBJECT_TYPE objType);
    static void initializePriorityArray(BACSTAC_PRI_ARRAY_ITEM* priorityArray);
    static void storeNullPriorityArray(BACSTAC_HOBJECT hObject);

    /// BACstac Hook 등록
    bool registerHooks();

    /// COV 폴링 스레드 시작
    void startCovManager();

    // ── readPropHook 세분화 private static 메서드 ──────────────────────────

    /// 특수 속성(ALL/REQUIRED/OPTIONAL/RESOLUTION) 거부 처리
    static BACSTAC_HOOK_STATUS rejectSpecialPropertyQuery(
        BACSTAC_HTSM hTsm, uint32_t prop_id, uint32_t obj_type);

    /// NetworkPort 특수 속성 거부 처리
    static BACSTAC_HOOK_STATUS rejectNetworkPortBlockingProperty(
        BACSTAC_HTSM hTsm, uint32_t prop_id);

    /// NetworkPort 필수 속성 수동 응답
    static BACSTAC_HOOK_STATUS handleNetworkPortRead(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo);

    /// NetworkPort Property_List 응답 (Command 제외)
    static BACSTAC_HOOK_STATUS handleNetworkPortPropertyList(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo);

    /// CURRENT_COMMAND_PRIORITY 읽기 처리
    static BACSTAC_HOOK_STATUS handleReadCommandPriority(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo, ObjectMapping* mapping);

    /// PRESENT_VALUE / RELINQUISH_DEFAULT 읽기 처리
    static BACSTAC_HOOK_STATUS handleReadPresentValue(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo, ObjectMapping* mapping);

    /// Device 객체의 ACTIVE_COV_SUBSCRIPTIONS 읽기 처리
    static BACSTAC_HOOK_STATUS handleReadActiveCOVSubscriptions(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo);

    /// ReadProperty Hook 공통 입력 검증
    static bool validateReadHookInput(const BACSTAC_READ_INFO* pInfo);

    // ── BACstac 응답 공통 처리 ───────────────────────────────────────────────

    static BACSTAC_HOOK_STATUS sendServiceError(
        BACSTAC_HTSM hTsm, BACSTAC_ERROR_CLASS errClass, BACSTAC_ERROR_CODE errCode);
    static BACSTAC_HOOK_STATUS sendReject(
        BACSTAC_HTSM hTsm, BACSTAC_REJECT_REASON reason, const char* serviceName);
    static BACSTAC_HOOK_STATUS sendAbort(BACSTAC_HTSM hTsm, BACSTAC_ABORT_REASON reason);
    static BACSTAC_HOOK_STATUS sendResponse(BACSTAC_HTSM hTsm);
    static BACSTAC_HOOK_STATUS sendReadResponse(
        BACSTAC_HTSM hTsm, const BACSTAC_READ_INFO* pInfo,
        const BACSTAC_PROPERTY_CONTENTS* pValue);
    static BACSTAC_HOOK_STATUS rejectUnsupportedService(
        BACSTAC_HTSM hTsm, const char* serviceName);

    // ── writePropHook 세분화 private static 메서드 ─────────────────────────

    /// PROP_POLARITY 쓰기 처리
    static BACSTAC_HOOK_STATUS handleWritePolarity(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo, ObjectMapping* mapping);

    /// PROP_COV_INCREMENT 쓰기 처리
    static BACSTAC_HOOK_STATUS handleWriteCovIncrement(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo, ObjectMapping* mapping);

#ifdef OUT_OF_SERVICE
    /// PROP_OUT_OF_SERVICE 쓰기 처리
    static BACSTAC_HOOK_STATUS handleWriteOutOfService(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo,
        ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType);
#endif

    /// PROP_PRESENT_VALUE 쓰기 처리 (Out_Of_Service=TRUE 모드 포함)
    static BACSTAC_HOOK_STATUS handleWritePresentValue(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo,
        ObjectMapping* mapping, BACSTAC_OBJECT_TYPE objType);

    /// Device 객체 속성 쓰기 처리 (UTC_Offset / Object_Name / Object_Identifier)
    /// BTL 7.3.2.10.5, 7.3.2.10.6, 7.3.2.10.8, 7.3.2.10.9, 7.3.2.10.10
    static BACSTAC_HOOK_STATUS handleWriteDeviceProperty(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo);

    /// Network Port 객체 쓰기 처리
    /// BACnet_IP_Mode / FD_BBMD_Address / FD_Subscription_Lifetime만 실시간 반영(재시작),
    /// 그 외 속성은 WRITE_ACCESS_DENIED (BTL 7.3.2.46.1.5)
    static BACSTAC_HOOK_STATUS handleWriteNetworkPort(
        BACSTAC_HTSM hTsm, const BACSTAC_ADDRESS* source, const BACSTAC_WRITE_INFO* pInfo);
    static BACSTAC_HOOK_STATUS handleWriteBipMode(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo);
    static BACSTAC_HOOK_STATUS handleWriteFdBbmdAddress(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo);
    static BACSTAC_HOOK_STATUS handleWriteFdSubscriptionLifetime(
        BACSTAC_HTSM hTsm, const BACSTAC_WRITE_INFO* pInfo);

    /// bip.cfg.fd의 FD 설정(BBMD 주소/포트, TTL, 재시도 주기)을 모드와 무관하게 로드
    void loadFdConfig();

    /// 쓰기 응답 후 재시작을 요청합니다. (실제 재실행은 main 감시 루프)
    static void requestRestart(const char* reason);

    /// Device 객체의 Database_Revision 을 +1 증가 (BTL 7.3.2.10.5, 7.3.2.10.6)
    static void incrementDatabaseRevision();

    /// WriteProperty Hook 공통 입력/값 처리
    static bool validateWriteHookInput(const BACSTAC_WRITE_INFO* pInfo);
    static bool decodeWriteValue(BACSTAC_HTSM hTsm,
                                 const BACSTAC_PROPERTY_CONTENTS* pValue,
                                 double& value, bool& isNull);
    static bool validateWriteRange(BACSTAC_HTSM hTsm,
                                   const ObjectMapping* mapping,
                                   BACSTAC_OBJECT_TYPE objType,
                                   double value, bool isNull);

    /// Present_Value, Polarity, OOS 및 COV 공통 변환/갱신
    static BACSTAC_BOOLEAN readOutOfService(const ObjectMapping* mapping);
    static double coerceOutputValue(const ObjectMapping* mapping,
                                    BACSTAC_OBJECT_TYPE objType, double value);
    static void storePresentValue(ObjectMapping* mapping,
                                  BACSTAC_OBJECT_TYPE objType, double value);
    static double readRelinquishDefault(const ObjectMapping* mapping,
                                        BACSTAC_OBJECT_TYPE objType);
    static void updatePriorityArray(ObjectMapping* mapping,
                                    BACSTAC_OBJECT_TYPE objType,
                                    uint8_t effectivePriority,
                                    double requestedValue, bool isNull,
                                    BACSTAC_PRI_ARRAY_ITEM* priorityArray,
                                    double& effectiveValue, bool& activeWrite);
    static void storePriorityArray(ObjectMapping* mapping,
                                   BACSTAC_PRI_ARRAY_ITEM* priorityArray);
    static bool writeMappedValue(ObjectMapping* mapping,
                                 BACSTAC_OBJECT_TYPE objType, double logicalValue);
    static void notifyCov(ObjectMapping* mapping, double value, bool useIncrement);

public:
    static BACSTAC_POLARITY readPolarity(const ObjectMapping* mapping);
    static double toLogicalBinaryValue(double rawValue, BACSTAC_POLARITY polarity);

    /// 비동기 쓰기 완료 / 실패 콜백 (ModbusClient 단일 Worker 스레드에서 호출)
    static void onAsyncWriteCompleted(BACSTAC_HTSM hTsm, const ObjectMapping* mapping,
                                      bool is_commandable,
                                      const BACSTAC_PRI_ARRAY_ITEM* priorityArray,
                                      double write_val);
    static void onAsyncWriteFailed(BACSTAC_HTSM hTsm, const ObjectMapping* mapping);

private:
    // ── 유틸리티 ─────────────────────────────────────────────────────────────

    /// 타입과 인스턴스 번호로 ObjectMapping 을 검색합니다.
    ObjectMapping* findMapping(BACSTAC_OBJECT_TYPE type, uint32_t instance);

private:
    // ── 멤버 변수 ────────────────────────────────────────────────────────────

    std::vector<DeviceProfile>  profiles_;  ///< 파일별 장치 프로파일 목록
    std::vector<ObjectMapping*> mappings_;  ///< 전체 매핑 포인터 (profiles_ 에서 집계)

    BACnetConfig bacnet_cfg_;  ///< gateway.json BACnet 설정
    ModbusConfig modbus_cfg_;  ///< gateway.json Modbus 설정
    int          cache_sec_      = 0;     ///< 캐시 유효 시간 (초)
    int          cov_polling_ms_ = 1000;  ///< COV 폴링 주기 (ms)
    bool         enable_log_     = false; ///< 진단 로그 활성화
    uint32_t     database_revision_ = 1U; ///< Device 객체의 Database_Revision 값
    std::string  config_dir_;              ///< bip.cfg 탐색 기준 config 폴더 (비어 있으면 현재 작업 폴더 기준)
    std::string  bip_config_path_;         ///< 로드한 bip.cfg 절대경로 (모드 전환 시 링크 대상)

    // 레이어 클래스 인스턴스
    std::unique_ptr<modbus::ISerialPort> serial_port_; ///< 생성자에서 소유, start() 시 ModbusClient 로 이전
    std::unique_ptr<ModbusClient>        modbus_;
    std::unique_ptr<CovManager>          cov_;

    BACSTAC_HDEVICE hLocalDev_ = nullptr; ///< 로컬 BACnet 디바이스 핸들 (start() 이후)

    // Network Port 속성 (BIP cfg 로드 후 설정)
    void detectNetworkInfo();

    std::string ip_address_str_      = "192.168.1.31";
    std::string subnet_mask_str_     = "255.255.255.0";
    std::string default_gateway_str_ = "192.168.1.1";
    uint8_t     mac_address_[6]      = {0, 0, 0, 0, 0, 0};
    std::vector<std::string> dns_servers_;
    std::string bip_mode_str_        = "normal";
    std::string fd_bbmd_ip_          = "";
    uint16_t    bip_udp_port_        = 47808;
    uint16_t    fd_bbmd_port_        = 47808;
    uint16_t    fd_ttl_              = 1800;
    uint32_t    fd_reg_timeout_ms_   = 3000;  ///< bip.cfg.fd fd.reg_timeout (재접속 주기)

    std::unique_ptr<FdMonitor> fd_monitor_;   ///< FD 모드에서 BBMD 연결 상태 로그 출력
    std::unique_ptr<NetworkWatcher> net_watcher_; ///< B/IP 인터페이스 링크/IPv4 변경 감시
    std::string bip_iface_;                   ///< bip.cfg iface= (비어 있으면 ip= 방식)
    const std::atomic<bool>* startup_running_ = nullptr; ///< 기동 중 네트워크 대기 중단 신호
    std::atomic<bool> restart_requested_{false};

    std::atomic<bool> running_{false};
    const std::atomic<bool>* external_running_ = nullptr;

    /// static 훅에서 인스턴스에 접근하기 위한 싱글턴 포인터 (단일 인스턴스 가정)
    static BacnetGateway* instance_;
};

} // namespace gateway
