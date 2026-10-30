#pragma once

/**
 * @file config_loader.h
 * @brief JSON 설정 파일 파싱 전담 클래스
 *
 * BacnetGateway 의 JSON 파싱 로직을 완전히 분리하여 단일 책임 원칙(SRP) 적용.
 * BACstac API 를 사용하지 않으므로 단위 테스트가 가능합니다.
 *
 * 의존성: types.h, jsoncpp
 */

#include "types.h"
#include <string>
#include <vector>
#include <atomic>

namespace gateway {

/**
 * @brief JSON 설정 파일 파싱 전담 클래스.
 *
 * 책임:
 *  - gateway.json (공통 설정) 파싱
 *  - profiles 디렉토리 내의 기기별 JSON 파싱
 *  - 인스턴스 번호 중복 검증 및 자동 할당
 */
class ConfigLoader {
public:
    ConfigLoader() = default;

    /**
     * @brief gateway.json 에서 BACnet/IP 및 Modbus 공통 설정을 로드합니다.
     *
     * @param path         gateway.json 파일 경로
     * @param[out] bacnet  로드된 BACnet 설정
     * @param[out] modbus  로드된 Modbus 설정
     * @param[out] cache_sec     캐시 유효 시간 (초, 0=비활성)
     * @param[out] cov_polling_ms COV 폴링 주기 (ms)
     * @return true: 성공
     */
    bool loadGatewayConfig(const std::string& path,
                           BACnetConfig&      bacnet,
                           ModbusConfig&      modbus,
                           int&               cache_sec,
                           int&               cov_polling_ms);

    /**
     * @brief gateway.json 에 현재 BACnet 설정을 원자적으로 저장합니다.
     *
     * 전원 차단 등을 대비해 tmp → bak → rename 방식으로 원자적 쓰기를 보장합니다.
     *   1. bacnet.config_path + ".tmp" 에 새 내용을 완전히 기록
     *   2. 기존 파일을 bacnet.config_path + ".bak" 으로 이름 변경 (백업)
     *   3. ".tmp" 를 원본 경로로 이름 변경 (POSIX rename = 원자 연산)
     *
     * @param bacnet  저장할 BACnet 설정 (config_path 포함)
     * @param modbus  저장할 Modbus 설정 (modbus 섹션 보존용)
     * @return true: 성공
     */
    static bool saveGatewayConfig(const BACnetConfig& bacnet,
                                  const ModbusConfig& modbus);

    /**
     * @brief config 폴더 내 기기별 .json 파일을 순차 파싱합니다.
     *
     * - gateway.json 을 제외한 .json 파일을 DeviceProfile 로 변환
     * - 전체 인스턴스 번호 중복 검증 및 자동 할당
     *
     * @param dir           profiles 폴더 경로
     * @param[out] profiles 파싱된 프로파일 목록
     * @param[out] mappings 전체 ObjectMapping 포인터 (profiles 에서 집계)
     * @param[out] out_error 실패 시 상세 오류 메시지 반환용 포인터 (선택)
     * @param[out] out_is_contradiction 치명적 설정 모순(min > max 등) 여부 반환용 포인터 (선택)
     * @return true: 최소 1개 이상 성공
     */
    bool loadConfigDir(const std::string&         dir,
                       std::vector<DeviceProfile>& profiles,
                       std::vector<ObjectMapping*>& mappings,
                       std::string*               out_error = nullptr,
                       bool*                      out_is_contradiction = nullptr);

    // ── JSON 파싱 헬퍼 (공개: 단위 테스트 및 test_config_parser.cpp 에서 접근 가능) ──

    /// Modbus 블록 문자열 파싱
    static ModbusBlock parseModbusBlock(const std::string& s);
    /// 데이터 타입 문자열 파싱
    static DataType    parseDataType(const std::string& s);
    /// BACnet 엔지니어링 단위 문자열 파싱
    static BACSTAC_ENGINEERING_UNITS parseUnits(const std::string& s);

    /**
     * @brief full_address 로부터 ModbusBlock 과 0-based raw offset 을 도출합니다.
     *
     * 표준 Modbus 5/6자리 주소 표기법:
     *  - Coil          :     1 –  9999 (또는    1 – 99999)
     *  - Discrete Input: 10001 – 19999 (또는 100001 – 199999)
     *  - Input Register: 30001 – 39999 (또는 300001 – 399999)
     *  - Holding Reg   : 40001 – 49999 (또는 400001 – 499999)
     */
    static void resolveFullAddress(uint32_t    full_addr,
                                   ModbusBlock& block,
                                   uint32_t&   raw_addr);

    /**
     * @brief modbus_block + offset_address 조합의 범위 유효성을 검사합니다.
     *
     * @return true: 유효 (0x0000–0xFFFF 범위 이내)
     */
    static bool validateOffsetAddress(ModbusBlock block, uint32_t raw_addr);

    /**
     * @brief BACnet 객체 타입을 Modbus 블록 + read_only 에서 자동 추론합니다.
     */
    static BACSTAC_OBJECT_TYPE deduceBacnetType(const ObjectMapping& m);

    /// Relinquish Default 후보값에 mapping의 Min/Max 범위를 동일한 순서로 적용합니다.
    static double clampRelinquishDefault(const ObjectMapping& mapping, double candidate);

    /// Binary Output의 Relinquish Default가 INACTIVE(0) 또는 ACTIVE(1)인지 검사합니다.
    static bool isValidBinaryRelinquishDefault(const ObjectMapping& mapping, double value);

private:
    /**
     * @brief 단일 기기 JSON 파일을 파싱하여 DeviceProfile 을 반환합니다.
     */
    bool loadProfile(const std::string& path,
                     DeviceProfile&     profile,
                     std::string*       out_error = nullptr,
                     bool*              out_is_contradiction = nullptr);

    /**
     * @brief 전체 profiles 에 대해 인스턴스 번호 중복 검증 및 자동 할당을 수행합니다.
     */
    static bool assignInstances(std::vector<DeviceProfile>& profiles);

    /// BACnet 타입 문자열 파싱 (내부용)
    static BACSTAC_OBJECT_TYPE parseBacnetType(const std::string& s);
};

} // namespace gateway
