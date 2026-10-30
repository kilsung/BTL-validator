#pragma once

#include <string>
#include <vector>
#include "types.h"

namespace gateway {

/**
 * @brief 게이트웨이 설정 및 객체 목록을 기반으로 BACnet TPI (PICS/EPICS) 파일을 생성하는 클래스
 */
class TpiExporter {
public:
    /**
     * @brief 현재 디바이스 및 객체 매핑 목록을 TPI 파일(current_device.tpi)로 출력
     * @param filepath 저장할 파일 경로
     * @param bacnet_cfg BACnet 디바이스 설정 정보
     * @param mappings 게이트웨이에 등록된 객체 매핑 목록
     * @return 성공 시 true, 실패 시 false
     */
    static bool exportCurrentDeviceTpi(
        const std::string& filepath,
        const BACnetConfig& bacnet_cfg,
        const std::vector<ObjectMapping*>& mappings,
        const std::string& bip_mode = "normal");

    /**
     * @brief BACnet 엔지니어링 단위를 TPI 표준 문자열로 변환
     * @param units 엔지니어링 단위 enum
     * @return TPI 문자열 식별자
     */
    static const char* unitsToTpiString(BACSTAC_ENGINEERING_UNITS units);
};

} // namespace gateway
