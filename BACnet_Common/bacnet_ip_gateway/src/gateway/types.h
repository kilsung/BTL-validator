#pragma once

/**
 * @file types.h
 * @brief Modbus RTU ↔ BACnet IP 게이트웨이 공유 데이터 타입 정의
 *
 * 모든 레이어(ConfigLoader, ModbusClient, CovManager, BacnetServer, BacnetGateway)가
 * 공통으로 참조하는 열거형, 설정 구조체, 매핑 구조체를 단일 헤더에 정의합니다.
 *
 * 의존성: BACstac 헤더 (bactypes.h, bacsrv.h) 및 C++ 표준 라이브러리
 */

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <bactypes.h>
#include <bacsrv.h>

namespace gateway {

// ---------------------------------------------------------------------------
// 열거형 정의
// ---------------------------------------------------------------------------

/// Modbus 레지스터 블록 유형
enum class ModbusBlock {
    COIL,
    DISCRETE_INPUT,
    INPUT_REGISTER,
    HOLDING_REGISTER
};

/// Modbus 레지스터 데이터 타입
enum class DataType {
    BIT,
    INT16,
    UINT16,
    FLOAT32,
    BIT_PACKED
};

// ---------------------------------------------------------------------------
// 설정 구조체
// ---------------------------------------------------------------------------

/// BACnet/IP 계층 설정
struct BACnetConfig {
    uint32_t    device_id            = 1000U;
    std::string device_name          = "BACnet-Modbus Gateway";
    std::string vendor_name          = "LG Electronics";
    uint32_t    vendor_id            = 432U;
    std::string model_name           = "BACnet-Modbus Gateway";
    std::string description          = "Modbus RTU to BACnet IP Gateway";
    std::string firmware_revision    = "1.0";
    std::string app_software_version = "1.0";
    uint32_t    protocol_revision    = 26U;
    uint32_t    database_revision    = 1U;
    uint32_t    apdu_timeout_ms      = 6000U;
    uint32_t    apdu_retries         = 3U;
    uint32_t    max_apdu_length      = 1476U;
    uint16_t    port                 = 47808U;
    std::string network_port_name    = "NetworkPort_1";
    uint32_t    network_port_instance = 1U;
    std::string config_path;                    ///< gateway.json 절대 경로 (Non-volatile 저장용)
};

/// Modbus RTU 시리얼 포트 설정
struct ModbusConfig {
    std::string serial_device    = "none";
    int         serial_baudrate  = 19200;
    int         serial_data_bits = 8;
    std::string serial_parity    = "none";
    int         serial_stop_bits = 1;
    int         rtu_timeout_ms   = 500;
    bool        hardware_echo    = false; ///< RS-485 하드웨어 루프백(에코) 활성화 여부
};

// ---------------------------------------------------------------------------
// ObjectMapping 구조체
// ---------------------------------------------------------------------------

/**
 * @brief BACnet 객체 ↔ Modbus 레지스터 매핑 정보
 *
 * v2 변경 사항:
 *  - word_order  : FLOAT32/INT32 데이터의 워드 스왑 여부 (기본값 ABCD = no swap)
 *  - read_only   : true 이면 BACnet WriteProperty 요청을 게이트웨이에서 차단
 *                  → AI/BI 로 매핑하는 것이 권장되나, 명시적 차단용 안전장치로도 사용
 *  - offset_val  : 센서 보정 오프셋. 최종값 = (Raw × scale) + offset_val
 *                  쓰기 역변환: Raw = (BACnet값 - offset_val) / scale
 *  - instance    : 0 이면 자동 할당 대상
 */
struct ObjectMapping {
    BACSTAC_OBJECT_TYPE bacnet_type = OBJ_ANALOG_INPUT;
    uint32_t            instance    = 0U;   ///< 0 = 자동 할당 대상
    bool                instance_auto = false; ///< loadConfig 시 자동 할당 여부 플래그
    std::string         name;
    std::string         description;

    uint8_t     modbus_unit_id   = 1U;  ///< Modbus 슬레이브 ID (1~247, 미지정 시 프로필 기본값 상속)
    ModbusBlock modbus_block     = ModbusBlock::HOLDING_REGISTER;
    uint32_t    raw_offset_addr  = 0U;  ///< 0-based 원시 오프셋 주소
    DataType    data_type        = DataType::INT16;
    double      scale            = 1.0;
    double      offset_val       = 0.0; ///< 보정 오프셋: BACnet값 = (Raw × scale) + offset_val
    int         bit_position     = -1;  ///< BIT_PACKED 전용. -1 = 미사용

    /// [v2 신규] 32비트(FLOAT32) 데이터 워드 순서 교환
    /// false = ABCD: Big-Endian, High Word First (기본값)
    /// true  = CDAB: Low Word First (Word Swapped)
    bool        swap             = false;

    /// [v2 신규] 읽기 전용 강제 차단 플래그
    /// true 이면 writePropHook 에서 ERR_CODE_WRITE_ACCESS_DENIED 반환
    bool        read_only        = false;

    /// [v4 신규] 엔지니어링 단위 (Units) 속성 지원
    BACSTAC_ENGINEERING_UNITS   units = UNIT_NO_UNITS;

    /// [v5 신규] 최소/최대값 제한 속성 지원 (Min_Pres_Value / Max_Pres_Value)
    bool        has_min_value    = false;
    double      min_value        = 0.0;
    bool        has_max_value    = false;
    double      max_value        = 0.0;

    /// [v6 신규] Relinquish Default 속성 값 지원
    bool        has_relinquish_default = false;
    double      relinquish_default     = 0.0;

    /// [v8 COV 신규] Change of Value 구독 지원
    /// cov_increment 미지정 시: scale 값 자동 사용 (센서 정밀도와 COV 민감도 동기화)
    /// cov_increment 명시 시: 명시 값 우선
    double  cov_increment   = 1.0;     ///< 변화 판정 기준값 (미지정 시: scale 값 사용)
    double  cov_last_value  = 0.0;     ///< COV 폴링 이전 값 (변화 감지용)
    bool    cov_initialized = false;   ///< 최초 폴링 여부
    std::chrono::steady_clock::time_point cov_last_poll_time; ///< COV 내부 1초 캐시 만료용

    /// BACstac 객체 핸들 (start() 이후 할당)
    BACSTAC_HOBJECT hObject = nullptr;
};

// ---------------------------------------------------------------------------
// ModbusJob 구조체 (범용 비동기/동기 시리얼 I/O 작업)
// ---------------------------------------------------------------------------

static constexpr size_t kPriorityArraySize = 16;

using ModbusCompletionCallback = std::function<void(bool success)>;

class ModbusReadHandle;

enum class ModbusJobPriority {
    HIGH = 0, ///< 지연에 민감한 foreground 요청
    LOW  = 1  ///< 백그라운드 요청
};

enum class ModbusJobType {
    WRITE_ASYNC,  ///< 비동기 쓰기 (완료 시 callback 호출)
    READ_SYNC     ///< 읽기 요청 (ModbusReadHandle로 완료 통지)
};

struct ModbusJob {
    ModbusJobPriority                    priority = ModbusJobPriority::HIGH;
    ModbusJobType                        job_type = ModbusJobType::WRITE_ASYNC;
    const ObjectMapping*                 mapping = nullptr;
    bool                                 apply_limits = true;
    double                               write_value = 0.0;
    ModbusCompletionCallback             callback;
    std::shared_ptr<ModbusReadHandle>    read_handle;
    std::chrono::steady_clock::time_point enqueued_time;
};

struct ModbusJobComparator {
    bool operator()(const ModbusJob& a, const ModbusJob& b) const {
        if (a.priority != b.priority) {
            return static_cast<int>(a.priority) > static_cast<int>(b.priority);
        }
        // 동일 우선순위 내에서는 FIFO 순서 유지 (오래된 작업 먼저)
        return a.enqueued_time > b.enqueued_time;
    }
};

// ---------------------------------------------------------------------------
// CovSubscriber / ActiveCovSubscriptionInfo 구조체
// ---------------------------------------------------------------------------

/// COV 구독자 정보 (SubscribeCOV 및 SubscribeCOVProperty 공용)
struct CovSubscriber {
    BACSTAC_UI32        processID = 0;
    BACSTAC_ADDRESS     destAddress;
    BACSTAC_BOOLEAN     confirmed = false;      ///< true: ConfirmedCOVNotification
    std::chrono::steady_clock::time_point subscribeTime;
    BACSTAC_UNSIGNED    lifetimeSec = 0;    ///< 0 = 영구 (BACSTAC_INFINITE_LIFETIME)
    BACSTAC_PROPERTY_ID propID = PROP_PRESENT_VALUE; ///< 구독 대상 속성 ID (기본값: Present_Value)
    BACSTAC_ARRAY_INDEX arrayIndex = BACSTAC_VOID_INDEX; ///< 속성 배열 인덱스 (기본값: VOID)
    bool                hasIncrement = false; ///< covIncrement 명시 여부
    float               covIncrement = 0.0f;  ///< SubscribeCOVProperty 지정 임계값
};

/// Active_COV_Subscriptions 프로퍼티 응답용 구조체
struct ActiveCovSubscriptionInfo {
    BACSTAC_ADDRESS     destAddress;
    BACSTAC_UI32        processID;
    BACSTAC_OBJECT_TYPE objType;
    uint32_t            instance;
    uint32_t            propID;       ///< PROP_PRESENT_VALUE 등
    BACSTAC_ARRAY_INDEX arrayIndex = BACSTAC_VOID_INDEX;
    bool                confirmed;
    uint32_t            timeRemaining;///< 남은 초 (BACSTAC_INFINITE_LIFETIME = 영구)
    float               covIncrement;
    bool                hasIncrement;
};

// ---------------------------------------------------------------------------
// DeviceProfile 구조체
// ---------------------------------------------------------------------------

/**
 * @brief 단일 JSON 파일에서 로드되는 논리 장치(Virtual Device) 프로파일.
 *
 * 현재 버전에서는 모든 프로파일의 객체들을 단일 게이트웨이 디바이스 아래에 병합합니다.
 */
struct DeviceProfile {
    std::string   config_path;          ///< 원본 JSON 파일 경로 (인스턴스 역기록용)
    std::string   device_name;          ///< 기기 표시 이름 (선택 필드)
    uint32_t      device_id = 0U;       ///< 가상 BACnet 디바이스 ID (0 = 자동 할당 대상)
    bool          device_id_auto = false; ///< 디바이스 ID 자동 할당 여부 플래그
    uint8_t       modbus_unit_id = 1U;  ///< 프로필 필수 Modbus 슬레이브 주소 (1~247)
    std::vector<ObjectMapping> mappings;
    bool          needs_writeback = false; ///< 자동 할당 발생 시 true
    BACSTAC_HDEVICE hDevice = nullptr;  ///< 생성된 가상 디바이스 핸들
};

} // namespace gateway
