/**
 * @file config_loader.cpp
 * @brief JSON 설정 파일 파싱 구현
 */

#include "config_loader.h"
#include "../common/relinquish_policy.h"
#include <json/json.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>   // strerror
#include <cerrno>
#include <cstdio>    // rename(), remove()
#include <set>
#include <map>
#include <dirent.h>  // opendir, readdir, closedir
#include <thread>
#include <chrono>

namespace gateway {

// ---------------------------------------------------------------------------
// 익명 네임스페이스: JSON 값 변환 유틸리티 (이 파일 내부에서만 사용)
// ---------------------------------------------------------------------------
namespace {

uint32_t parseJsonUInt(const Json::Value& val, uint32_t default_val)
{
    if (val.isNull())    { return default_val; }
    if (val.isNumeric()) { return val.asUInt(); }
    if (val.isString()) {
        const std::string str = val.asString();
        if (str.empty()) { return default_val; }
        try {
            if (str.size() >= 2U && str[0] == '0' &&
                (str[1] == 'x' || str[1] == 'X'))
            {
                return static_cast<uint32_t>(std::stoul(str, nullptr, 16));
            }
            return static_cast<uint32_t>(std::stoul(str, nullptr, 10));
        } catch (...) { return default_val; }
    }
    return default_val;
}

int parseJsonInt(const Json::Value& val, int default_val)
{
    if (val.isNull())    { return default_val; }
    if (val.isNumeric()) { return val.asInt(); }
    if (val.isString()) {
        const std::string str = val.asString();
        if (str.empty()) { return default_val; }
        try {
            if (str.size() >= 2U && str[0] == '0' &&
                (str[1] == 'x' || str[1] == 'X'))
            {
                return static_cast<int>(std::stol(str, nullptr, 16));
            }
            return static_cast<int>(std::stol(str, nullptr, 10));
        } catch (...) { return default_val; }
    }
    return default_val;
}

double parseJsonDouble(const Json::Value& val, double default_val)
{
    if (val.isNull())    { return default_val; }
    if (val.isNumeric()) { return val.asDouble(); }
    if (val.isString()) {
        const std::string str = val.asString();
        if (str.empty()) { return default_val; }
        try { return std::stod(str); } catch (...) { return default_val; }
    }
    return default_val;
}

bool parseJsonBool(const Json::Value& val, bool default_val)
{
    if (val.isNull())    { return default_val; }
    if (val.isBool())    { return val.asBool(); }
    if (val.isNumeric()) { return (val.asInt() != 0); }
    if (val.isString()) {
        const std::string s = val.asString();
        if (s == "true"  || s == "1") { return true; }
        if (s == "false" || s == "0") { return false; }
    }
    return default_val;
}

bool hasJsonExtension(const std::string& filename)
{
    if (filename.size() < 5U) { return false; }
    return (filename.compare(filename.size() - 5U, 5U, ".json") == 0);
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 상수
// ---------------------------------------------------------------------------
static constexpr uint32_t kHoldingReg6DigitMin =  400001U;
static constexpr uint32_t kHoldingReg6DigitMax =  499999U;
static constexpr uint32_t kHoldingReg5DigitMin =   40001U;
static constexpr uint32_t kHoldingReg5DigitMax =   49999U;
static constexpr uint32_t kInputReg6DigitMin   =  300001U;
static constexpr uint32_t kInputReg6DigitMax   =  399999U;
static constexpr uint32_t kInputReg5DigitMin   =   30001U;
static constexpr uint32_t kInputReg5DigitMax   =   39999U;
static constexpr uint32_t kDiscreteIn6DigitMin =  100001U;
static constexpr uint32_t kDiscreteIn6DigitMax =  199999U;
static constexpr uint32_t kDiscreteIn5DigitMin =   10001U;
static constexpr uint32_t kDiscreteIn5DigitMax =   19999U;
static constexpr uint32_t kModbusMaxOffset     = 0xFFFFU;
static constexpr int      kMaxBitPosition      = 15;

// ---------------------------------------------------------------------------
// 정적 헬퍼 구현
// ---------------------------------------------------------------------------

ModbusBlock ConfigLoader::parseModbusBlock(const std::string& s)
{
    if (s == "COIL")           return ModbusBlock::COIL;
    if (s == "DISCRETE_INPUT") return ModbusBlock::DISCRETE_INPUT;
    if (s == "INPUT_REGISTER") return ModbusBlock::INPUT_REGISTER;
    return ModbusBlock::HOLDING_REGISTER;
}

DataType ConfigLoader::parseDataType(const std::string& s)
{
    if (s == "BIT")        return DataType::BIT;
    if (s == "INT16")      return DataType::INT16;
    if (s == "UINT16")     return DataType::UINT16;
    if (s == "FLOAT32")    return DataType::FLOAT32;
    if (s == "BIT_PACKED") return DataType::BIT_PACKED;
    return DataType::INT16;
}

BACSTAC_OBJECT_TYPE ConfigLoader::parseBacnetType(const std::string& s)
{
    if (s == "ANALOG_INPUT")  return OBJ_ANALOG_INPUT;
    if (s == "ANALOG_OUTPUT") return OBJ_ANALOG_OUTPUT;
    if (s == "BINARY_INPUT")  return OBJ_BINARY_INPUT;
#ifdef BINARY_OUTPUT_SUPPORT
    if (s == "BINARY_OUTPUT") return OBJ_BINARY_OUTPUT;
#endif
    if (s == "BINARY_VALUE")  return OBJ_BINARY_VALUE;
    return OBJ_ANALOG_INPUT;
}

double ConfigLoader::clampRelinquishDefault(const ObjectMapping& mapping, double candidate)
{
    return relinquish_policy::clamp(candidate,
                                    mapping.has_min_value,
                                    mapping.min_value,
                                    mapping.has_max_value,
                                    mapping.max_value);
}

bool ConfigLoader::isValidBinaryRelinquishDefault(
    const ObjectMapping& mapping, double value)
{
#ifdef BINARY_OUTPUT_SUPPORT
    return relinquish_policy::isValidBinaryValue(
        mapping.bacnet_type == OBJ_BINARY_OUTPUT, value);
#else
    (void)mapping;
    (void)value;
    return true;
#endif
}

BACSTAC_ENGINEERING_UNITS ConfigLoader::parseUnits(const std::string& units_str)
{
    std::string lower_str = units_str;
    for (char& c : lower_str) {
        if (c >= 'A' && c <= 'Z') { c = c - 'A' + 'a'; }
    }
    if (lower_str == "degrees-celsius")        return UNIT_DEGREES_C;
    if (lower_str == "degrees-fahrenheit")     return UNIT_DEGREES_F;
    if (lower_str == "percent")                return UNIT_PERCENT;
    if (lower_str == "hours")                  return UNIT_HOURS;
    if (lower_str == "seconds")                return UNIT_SECONDS;
    if (lower_str == "minutes")                return UNIT_MINUTES;
    if (lower_str == "amperes")                return UNIT_AMPERES;
    if (lower_str == "hertz")                  return UNIT_HERTZ;
    if (lower_str == "volts")                  return UNIT_VOLTS;
    if (lower_str == "revolutions-per-minute") return UNIT_REVOLUTIONS_PER_MIN;
    if (lower_str == "kilopascals")            return UNIT_KILOPASCALS;
    if (lower_str == "bars")                   return UNIT_BARS;
    if (lower_str == "cubic-meters-per-hour")  return UNIT_CUBIC_METERS_PER_HOUR;
    return UNIT_NO_UNITS;
}

void ConfigLoader::resolveFullAddress(uint32_t     full_addr,
                                      ModbusBlock& block,
                                      uint32_t&    raw_addr)
{
    if (full_addr >= kHoldingReg6DigitMin && full_addr <= kHoldingReg6DigitMax) {
        block    = ModbusBlock::HOLDING_REGISTER;
        raw_addr = full_addr - kHoldingReg6DigitMin;
    } else if (full_addr >= kHoldingReg5DigitMin && full_addr <= kHoldingReg5DigitMax) {
        block    = ModbusBlock::HOLDING_REGISTER;
        raw_addr = full_addr - kHoldingReg5DigitMin;
    } else if (full_addr >= kInputReg6DigitMin && full_addr <= kInputReg6DigitMax) {
        block    = ModbusBlock::INPUT_REGISTER;
        raw_addr = full_addr - kInputReg6DigitMin;
    } else if (full_addr >= kInputReg5DigitMin && full_addr <= kInputReg5DigitMax) {
        block    = ModbusBlock::INPUT_REGISTER;
        raw_addr = full_addr - kInputReg5DigitMin;
    } else if (full_addr >= kDiscreteIn6DigitMin && full_addr <= kDiscreteIn6DigitMax) {
        block    = ModbusBlock::DISCRETE_INPUT;
        raw_addr = full_addr - kDiscreteIn6DigitMin;
    } else if (full_addr >= kDiscreteIn5DigitMin && full_addr <= kDiscreteIn5DigitMax) {
        block    = ModbusBlock::DISCRETE_INPUT;
        raw_addr = full_addr - kDiscreteIn5DigitMin;
    } else {
        block    = ModbusBlock::COIL;
        raw_addr = (full_addr >= 1U) ? (full_addr - 1U) : 0U;
    }
}

bool ConfigLoader::validateOffsetAddress(ModbusBlock block, uint32_t raw_addr)
{
    (void)block;
    return (raw_addr <= kModbusMaxOffset);
}

BACSTAC_OBJECT_TYPE ConfigLoader::deduceBacnetType(const ObjectMapping& m)
{
    switch (m.modbus_block) {
        case ModbusBlock::DISCRETE_INPUT:
            return OBJ_BINARY_INPUT;
        case ModbusBlock::INPUT_REGISTER:
            return OBJ_ANALOG_INPUT;
        case ModbusBlock::COIL:
            return m.read_only ? OBJ_BINARY_INPUT : OBJ_BINARY_VALUE;
        case ModbusBlock::HOLDING_REGISTER:
            if (m.read_only) {
                return (m.bit_position >= 0) ? OBJ_BINARY_INPUT : OBJ_ANALOG_INPUT;
            } else {
                return (m.bit_position >= 0) ? OBJ_BINARY_VALUE : OBJ_ANALOG_OUTPUT;
            }
        default:
            return OBJ_ANALOG_INPUT;
    }
}

// ---------------------------------------------------------------------------
// loadGatewayConfig()
// ---------------------------------------------------------------------------

bool ConfigLoader::loadGatewayConfig(const std::string& path,
                                     BACnetConfig&      bacnet,
                                     ModbusConfig&      modbus,
                                     int&               cache_sec,
                                     int&               cov_polling_ms)
{
    if (path.empty()) {
        std::cerr << "[GatewayConfig] Error: path is empty." << std::endl;
        return false;
    }

    std::ifstream cfg_file(path);
    if (!cfg_file.is_open()) {
        std::cerr << "[GatewayConfig] Error: Cannot open '" << path << "'." << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, cfg_file, &root, &errs)) {
        std::cerr << "[GatewayConfig] Parse error in '" << path
                  << "': " << errs << std::endl;
        return false;
    }

    if (!root.isMember("bacnet") || !root["bacnet"].isObject()) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing required 'bacnet' section.\n";
        return false;
    }

    const Json::Value& b_sec = root["bacnet"];
    if (!b_sec.isMember("device_id")) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing required field 'bacnet.device_id'.\n";
        return false;
    }
    if (!b_sec.isMember("device_name") || b_sec["device_name"].asString().empty()) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing or empty required field 'bacnet.device_name'.\n";
        return false;
    }
    if (!b_sec.isMember("vendor_name") || b_sec["vendor_name"].asString().empty()) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing or empty required field 'bacnet.vendor_name'.\n";
        return false;
    }
    if (!b_sec.isMember("vendor_id")) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing required field 'bacnet.vendor_id'.\n";
        return false;
    }
    if (!b_sec.isMember("model_name") || b_sec["model_name"].asString().empty()) {
        std::cerr << "[GatewayConfig] Error in '" << path << "': Missing or empty required field 'bacnet.model_name'.\n";
        return false;
    }

    bacnet.device_id            = parseJsonUInt(b_sec["device_id"], 1200U);
    bacnet.device_name          = b_sec["device_name"].asString();
    bacnet.vendor_name          = b_sec["vendor_name"].asString();
    bacnet.vendor_id            = parseJsonUInt(b_sec["vendor_id"], 432U);
    bacnet.model_name           = b_sec["model_name"].asString();
    bacnet.description          = b_sec.get("description", "BACnet/IP implementation in LG CDU Products.").asString();
    bacnet.firmware_revision    = b_sec.get("firmware_revision", "1.0").asString();
    bacnet.app_software_version = b_sec.get("application_software_version", "1.0").asString();
    bacnet.protocol_revision    = parseJsonUInt(b_sec.get("protocol_revision", 26U), 26U);
    bacnet.database_revision    = parseJsonUInt(b_sec.get("database_revision", 1U), 1U);
    bacnet.apdu_timeout_ms      = parseJsonUInt(b_sec.get("apdu_timeout_ms", 6000U), 6000U);
    bacnet.apdu_retries         = parseJsonUInt(b_sec.get("apdu_retries", 3U), 3U);
    bacnet.max_apdu_length       = parseJsonUInt(b_sec.get("max_apdu_length", 1476U), 1476U);
    bacnet.port                  = static_cast<uint16_t>(parseJsonUInt(b_sec.get("port", 47808U), 47808U));
    bacnet.network_port_name     = b_sec.get("network_port_name", "NetworkPort_1").asString();
    bacnet.network_port_instance = parseJsonUInt(b_sec.get("network_port_instance", 1U), 1U);
    bacnet.config_path           = path; // Non-volatile 저장 경로 기억

    modbus.serial_device    = root["modbus"].get("serial_device",    "none").asString();
    modbus.serial_baudrate  = parseJsonInt(root["modbus"].get("serial_baudrate",  19200), 19200);
    modbus.serial_data_bits = parseJsonInt(root["modbus"].get("serial_data_bits", 8),     8);
    modbus.serial_parity    = root["modbus"].get("serial_parity",    "none").asString();
    modbus.serial_stop_bits = parseJsonInt(root["modbus"].get("serial_stop_bits", 1),     1);
    modbus.rtu_timeout_ms   = parseJsonInt(root["modbus"].get("rtu_timeout_ms",   500),   500);
    modbus.hardware_echo    = parseJsonBool(root["modbus"].get("hardware_echo", false), false);

    cache_sec = parseJsonInt(root["modbus"].get("cache_sec", 0), 0);

    double cov_poll_sec = 1.0;
    if (root["modbus"].isMember("cov_polling")) {
        cov_poll_sec = parseJsonDouble(root["modbus"]["cov_polling"], 1.0);
    }
    if (cov_poll_sec < 0.1) { cov_poll_sec = 0.1; }
    cov_polling_ms = static_cast<int>(std::round(cov_poll_sec * 1000.0));

    std::cout << "[GatewayConfig] Loaded '" << path << "'."
              << " BACnet device_id=" << bacnet.device_id
              << " database_revision=" << bacnet.database_revision
              << ", serial=" << modbus.serial_device
              << " @" << modbus.serial_baudrate << "bps"
              << ", hardware_echo=" << (modbus.hardware_echo ? "true" : "false")
              << ", cache_sec=" << cache_sec << "s"
              << ", cov_polling=" << cov_polling_ms << "ms" << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// saveGatewayConfig() — Atomic 덮어쓰기: .tmp → .bak → rename
// ---------------------------------------------------------------------------

bool ConfigLoader::saveGatewayConfig(const BACnetConfig& bacnet,
                                     const ModbusConfig& modbus)
{
    if (bacnet.config_path.empty()) {
        std::cerr << "[GatewayConfig] saveGatewayConfig: config_path is empty." << std::endl;
        return false;
    }

    const std::string path_tmp = bacnet.config_path + ".tmp";
    const std::string path_bak = bacnet.config_path + ".bak";

    // 1단계: .tmp 파일에 새 내용을 완전히 기록 (기존 파일을 건드리지 않음)
    {
        Json::Value root;
        // 기존 파일이 있으면 먼저 읽어서 기존 필드(cache_sec, cov_polling 등)를 보존
        std::ifstream ifs(bacnet.config_path);
        if (ifs.is_open()) {
            Json::CharReaderBuilder rb;
            std::string errs;
            (void)Json::parseFromStream(rb, ifs, &root, &errs);
            ifs.close();
        }

        root["bacnet"]["device_id"]   = bacnet.device_id;
        root["bacnet"]["device_name"] = bacnet.device_name;
        root["bacnet"]["model_name"]  = bacnet.model_name;
        root["bacnet"]["description"] = bacnet.description;

        root["modbus"]["serial_device"]    = modbus.serial_device;
        root["modbus"]["serial_baudrate"]  = modbus.serial_baudrate;
        root["modbus"]["serial_data_bits"] = modbus.serial_data_bits;
        root["modbus"]["serial_parity"]    = modbus.serial_parity;
        root["modbus"]["serial_stop_bits"] = modbus.serial_stop_bits;
        root["modbus"]["rtu_timeout_ms"]   = modbus.rtu_timeout_ms;
        root["modbus"]["hardware_echo"]    = modbus.hardware_echo;

        std::ofstream ofs(path_tmp);
        if (!ofs.is_open()) {
            std::cerr << "[GatewayConfig] Failed to open '" << path_tmp << "' for writing." << std::endl;
            return false;
        }
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "  ";
        wb["commentStyle"] = "None";

        std::unique_ptr<Json::StreamWriter> writer(wb.newStreamWriter());
        writer->write(root, &ofs);
        ofs << std::endl;
        ofs.flush();
        if (!ofs.good()) {
            std::cerr << "[GatewayConfig] Write error on '" << path_tmp << "'." << std::endl;
            std::remove(path_tmp.c_str());
            return false;
        }
    } // ofs close (flush & sync via destructor)

    // 2단계: 기존 파일을 .bak 으로 이름 변경 (이전 .bak 있으면 덮어씀)
    // rename() 은 POSIX 상 원자 연산: 같은 파일시스템 내 rename 은 atomic
    if (std::rename(bacnet.config_path.c_str(), path_bak.c_str()) != 0) {
        // 기존 파일 없는 경우(최초 생성)는 정상: ENOENT 는 무시
        if (errno != ENOENT) {
            std::cerr << "[GatewayConfig] Failed to rename '" << bacnet.config_path
                      << "' to '" << path_bak << "': " << strerror(errno) << std::endl;
            std::remove(path_tmp.c_str());
            return false;
        }
    }

    // 3단계: .tmp 를 원본 경로로 이름 변경 (원자 연산)
    if (std::rename(path_tmp.c_str(), bacnet.config_path.c_str()) != 0) {
        std::cerr << "[GatewayConfig] Failed to rename '" << path_tmp
                  << "' to '" << bacnet.config_path << "': " << strerror(errno) << std::endl;
        // 복구 시도: .bak 을 원본으로 되돌림
        std::rename(path_bak.c_str(), bacnet.config_path.c_str());
        return false;
    }

    std::cout << "[GatewayConfig] Saved '" << bacnet.config_path
              << "' (bak: '" << path_bak << "')." << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// loadProfile() — 단일 JSON 파일 파싱 (private 헬퍼)
// ---------------------------------------------------------------------------

bool ConfigLoader::loadProfile(const std::string& config_path,
                               DeviceProfile&     profile,
                               std::string*       out_error,
                               bool*              out_is_contradiction)
{
    if (out_is_contradiction != nullptr) {
        *out_is_contradiction = false;
    }
    if (config_path.empty()) {
        std::cerr << "[Config] Error: config_path is empty." << std::endl;
        if (out_error != nullptr) {
            *out_error = "config_path is empty";
        }
        return false;
    }

    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "[Config] Error: Could not open config file: "
                  << config_path << std::endl;
        return false;
    }

    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    if (!Json::parseFromStream(builder, config_file, &root, &errs)) {
        std::cerr << "[Config] Parse error in '" << config_path
                  << "': " << errs << std::endl;
        return false;
    }

    profile.config_path = config_path;

    // 파일명에서 확장자 제거하여 접두사 추출
    const std::string::size_type slash = config_path.rfind('/');
    const std::string basename = (slash == std::string::npos)
        ? config_path
        : config_path.substr(slash + 1U);
    const std::string::size_type dot = basename.rfind('.');
    const std::string file_basename = (dot == std::string::npos) ? basename : basename.substr(0, dot);

    // 1. device_name
    if (root.isMember("device_name")) {
        profile.device_name = root["device_name"].asString();
    } else {
        profile.device_name = file_basename;
    }

    // 2. device_id
    const Json::Value dev_id_val = root.get("device_id", Json::Value::null);
    if (dev_id_val.isNull()) {
        profile.device_id      = 0U;
        profile.device_id_auto = true;
    } else {
        profile.device_id      = parseJsonUInt(dev_id_val, 0U);
        profile.device_id_auto = (profile.device_id == 0U);
    }

    // 3. modbus_unit_id (기기 필수값: 1 ~ 247)
    if (!root.isMember("modbus_unit_id")) {
        std::cerr << "[Config] Error: Profile missing required field 'modbus_unit_id': "
                  << config_path << std::endl;
        return false;
    }
    const int unit_id = parseJsonInt(root["modbus_unit_id"], -1);
    if (unit_id < 1 || unit_id > 247) {
        std::cerr << "[Config] Error: Profile 'modbus_unit_id' out of range [1..247]: "
                  << unit_id << " in " << config_path << std::endl;
        return false;
    }
    profile.modbus_unit_id = static_cast<uint8_t>(unit_id);

    profile.mappings.clear();
    const Json::Value objects = root["objects"];
    for (unsigned int i = 0U; i < objects.size(); ++i) {
        ObjectMapping mapping;

        // instance
        const Json::Value inst_val = objects[i].get("instance", Json::Value::null);
        if (inst_val.isNull()) {
            mapping.instance      = 0U;
            mapping.instance_auto = true;
        } else {
            mapping.instance      = parseJsonUInt(inst_val, 0U);
            mapping.instance_auto = (mapping.instance == 0U);
        }

        mapping.name        = objects[i].get("name", "Object_" + std::to_string(i + 1U)).asString();
        mapping.description = objects[i].get("description", "").asString();

        // Object_Name (최대 100자) 및 Description (최대 80자) 안전 길이 제한
        if (mapping.name.length() > 100) {
            mapping.name = mapping.name.substr(0, 100);
        }
        if (mapping.description.length() > 80) {
            mapping.description = mapping.description.substr(0, 80);
        }

        // modbus_unit_id (선택: 명시 시 해당 값 우선 적용, 미지정 시 프로필 기본값 상속)
        if (objects[i].isMember("modbus_unit_id")) {
            const int obj_unit_id = parseJsonInt(objects[i]["modbus_unit_id"], -1);
            if (obj_unit_id < 1 || obj_unit_id > 247) {
                std::cerr << "[Config] Error: Object '" << mapping.name
                          << "' 'modbus_unit_id' out of range [1..247]: "
                          << obj_unit_id << " in " << config_path << std::endl;
                return false;
            }
            mapping.modbus_unit_id = static_cast<uint8_t>(obj_unit_id);
        } else {
            mapping.modbus_unit_id = profile.modbus_unit_id;
        }

        // read_only
        const Json::Value ro_val = objects[i].get("read_only", Json::Value::null);
        if (!ro_val.isNull()) {
            mapping.read_only = parseJsonBool(ro_val, false);
        }

        // swap
        if (objects[i].isMember("swap")) {
            mapping.swap = parseJsonBool(objects[i]["swap"], false);
        }

        // BACnet 객체 타입 (명시 시)
        if (objects[i].isMember("bacnet_type")) {
            mapping.bacnet_type = parseBacnetType(objects[i]["bacnet_type"].asString());
        }

        {
            // 주소 해석 및 검증
            ModbusBlock block    = ModbusBlock::HOLDING_REGISTER;
            uint32_t    raw_addr = 0U;

            if (objects[i].isMember("offset_address")) {
                if (!objects[i].isMember("modbus_block")) {
                    std::cerr << "[Config] Error: Object '" << mapping.name
                              << "' uses 'offset_address' but 'modbus_block' is missing!" << std::endl;
                    return false;
                }
                raw_addr = parseJsonUInt(objects[i]["offset_address"], 0U);
                block    = parseModbusBlock(objects[i]["modbus_block"].asString());

                if (!validateOffsetAddress(block, raw_addr)) {
                    std::cerr << "[Config] Error: Object '" << mapping.name
                              << "' offset_address " << raw_addr
                              << " exceeds Modbus address range (0x0000-0xFFFF)." << std::endl;
                    return false;
                }
            } else if (objects[i].isMember("full_address")) {
                const uint32_t full_addr = parseJsonUInt(objects[i]["full_address"], 0U);
                resolveFullAddress(full_addr, block, raw_addr);

                if (objects[i].isMember("modbus_block")) {
                    const ModbusBlock explicit_block =
                        parseModbusBlock(objects[i]["modbus_block"].asString());
                    if (explicit_block != block) {
                        std::cerr << "[Config] Error: Object '" << mapping.name
                                  << "' full_address " << full_addr
                                  << " maps to a different block than 'modbus_block'."
                                  << " Please correct one of them or omit 'modbus_block'."
                                  << std::endl;
                        return false;
                    }
                }
            } else {
                std::cerr << "[Config] Error: Object '" << mapping.name
                          << "' is missing both 'offset_address' and 'full_address'!"
                          << std::endl;
                return false;
            }

            mapping.modbus_block    = block;
            mapping.raw_offset_addr = raw_addr;

            // 데이터 타입
            const std::string type_str = objects[i].get("data_type", "").asString();
            if (type_str.empty()) {
                if (mapping.modbus_block == ModbusBlock::COIL ||
                    mapping.modbus_block == ModbusBlock::DISCRETE_INPUT)
                {
                    mapping.data_type = DataType::BIT;
                } else {
                    mapping.data_type = DataType::INT16;
                }
            } else {
                mapping.data_type = parseDataType(type_str);
            }

            mapping.scale        = parseJsonDouble(objects[i].get("scale",  1.0),  1.0);
            if (mapping.scale <= 0.0) {
                std::cerr << "[Config] Warning: Object '" << mapping.name
                          << "' invalid scale (" << mapping.scale
                          << "). Auto-clamping to 1.0 to prevent division by zero."
                          << std::endl;
                mapping.scale = 1.0;
            }
            mapping.offset_val   = parseJsonDouble(objects[i].get("offset", 0.0),  0.0);
            mapping.bit_position = parseJsonInt(objects[i].get("bit_position", -1), -1);

            if (mapping.bit_position >= 0) {
                mapping.data_type = DataType::BIT_PACKED;
            }

            if (mapping.bit_position > kMaxBitPosition) {
                std::cerr << "[Config] Warning: Object '" << mapping.name
                          << "' bit_position " << mapping.bit_position
                          << " exceeds valid range (0-15), clamping to 15." << std::endl;
                mapping.bit_position = kMaxBitPosition;
            }

            // units
            if (objects[i].isMember("units")) {
                mapping.units = parseUnits(objects[i]["units"].asString());
            } else {
                mapping.units = UNIT_NO_UNITS;
            }

            // min_value / max_value
            if (objects[i].isMember("min_value")) {
                mapping.min_value     = parseJsonDouble(objects[i]["min_value"], 0.0);
                mapping.has_min_value = true;
            }
            if (objects[i].isMember("max_value")) {
                mapping.max_value     = parseJsonDouble(objects[i]["max_value"], 0.0);
                mapping.has_max_value = true;
            }

            // 모순된 설정 검증: min_value > max_value
            if (mapping.has_min_value && mapping.has_max_value && mapping.min_value > mapping.max_value) {
                std::string err = "Object '" + mapping.name + "' in profile '" + config_path +
                                  "' has contradictory configuration: min_value (" +
                                  std::to_string(mapping.min_value) + ") > max_value (" +
                                  std::to_string(mapping.max_value) + ")!";
                std::cerr << "[Config] " << err << std::endl;
                if (out_error != nullptr) {
                    *out_error = err;
                }
                if (out_is_contradiction != nullptr) {
                    *out_is_contradiction = true;
                }
                return false;
            }

            // relinquish_default (명시 시 지정값 클램핑, 생략 시 has_relinquish_default = false)
            if (objects[i].isMember("relinquish_default")) {
                const double candidate_default = clampRelinquishDefault(
                    mapping, parseJsonDouble(objects[i]["relinquish_default"], 0.0));
                mapping.relinquish_default     = candidate_default;
                mapping.has_relinquish_default = true;
            } else {
                mapping.relinquish_default     = 0.0;
                mapping.has_relinquish_default = false;
            }

            // cov_increment (미지정 시 scale 값 사용)
            if (objects[i].isMember("cov_increment")) {
                mapping.cov_increment = parseJsonDouble(objects[i]["cov_increment"], mapping.scale);
            } else {
                mapping.cov_increment = mapping.scale;
            }
            if (mapping.cov_increment <= 0.0) {
                mapping.cov_increment = 1.0;
            }

            // BACnet 타입 자동 추론 (명시되지 않은 경우)
            if (!objects[i].isMember("bacnet_type")) {
                mapping.bacnet_type = deduceBacnetType(mapping);
            }

            if (mapping.has_relinquish_default &&
                !isValidBinaryRelinquishDefault(mapping, mapping.relinquish_default))
            {
                const std::string err = "Object '" + mapping.name + "' in profile '" +
                                        config_path +
                                        "' has invalid Binary Output relinquish_default (" +
                                        std::to_string(mapping.relinquish_default) +
                                        "); only 0 or 1 is allowed.";
                std::cerr << "[Config] " << err << std::endl;
                if (out_error != nullptr) {
                    *out_error = err;
                }
                return false;
            }
            // BTL 규격: Input 객체는 항상 read_only 강제
            if (mapping.bacnet_type == OBJ_ANALOG_INPUT || mapping.bacnet_type == OBJ_BINARY_INPUT) {
                mapping.read_only = true;
            }
        }

        mapping.hObject = nullptr;
        profile.mappings.push_back(mapping);
    }

    std::cout << "[Config] Loaded '" << config_path
              << "': " << profile.mappings.size() << " object(s)." << std::endl;
    return true;
}

// ---------------------------------------------------------------------------
// assignInstances() (private static)
// ---------------------------------------------------------------------------

bool ConfigLoader::assignInstances(std::vector<DeviceProfile>& profiles)
{
    std::map<uint32_t, std::string> instance_to_file;
    bool has_duplicate = false;

    for (auto& profile : profiles) {
        for (auto& m : profile.mappings) {
            if (!m.instance_auto && m.instance != 0U) {
                auto it = instance_to_file.find(m.instance);
                if (it != instance_to_file.end()) {
                    std::cerr << "[Instance] Error: Duplicate instance number "
                              << m.instance << " for object '" << m.name
                              << "' in '" << profile.config_path
                              << "' conflicts with file '" << it->second
                              << "' (Single-Device mode)." << std::endl;
                    has_duplicate = true;
                } else {
                    instance_to_file[m.instance] = profile.config_path;
                }
            }
        }
    }

    if (has_duplicate) {
        std::cerr << "[Instance] Critical: Duplicate instance numbers detected. Aborting start." << std::endl;
        return false;
    }

    std::set<uint32_t> global_used;
    for (const auto& pair : instance_to_file) {
        global_used.insert(pair.first);
    }

    uint32_t next_candidate = 1U;
    for (auto& profile : profiles) {
        for (auto& m : profile.mappings) {
            if (m.instance_auto) {
                while (global_used.count(next_candidate)) { ++next_candidate; }
                m.instance      = next_candidate;
                m.instance_auto = false;
                global_used.insert(next_candidate);
                std::cout << "[Instance] Auto-assigned instance " << m.instance
                          << " to object '" << m.name
                          << "' in single-device config '" << profile.config_path << "'." << std::endl;
                ++next_candidate;
            }
        }
    }

    return true;
}

// ---------------------------------------------------------------------------
// loadConfigDir()
// ---------------------------------------------------------------------------

bool ConfigLoader::loadConfigDir(const std::string&          dir,
                                 std::vector<DeviceProfile>& profiles,
                                 std::vector<ObjectMapping*>& mappings,
                                 std::string*               out_error,
                                 bool*                      out_is_contradiction)
{
    if (out_error != nullptr) {
        out_error->clear();
    }
    if (out_is_contradiction != nullptr) {
        *out_is_contradiction = false;
    }
    if (dir.empty()) {
        std::cerr << "[Config] Error: config_dir is empty." << std::endl;
        if (out_error != nullptr) {
            *out_error = "config_dir is empty";
        }
        return false;
    }

    DIR* d = ::opendir(dir.c_str());
    if (d == nullptr) {
        std::cerr << "[Config] Error: Cannot open config directory '"
                  << dir << "': " << strerror(errno) << std::endl;
        if (out_error != nullptr) {
            *out_error = "Cannot open config directory '" + dir + "': " + strerror(errno);
        }
        return false;
    }

    std::vector<std::string> json_files;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(d)) != nullptr) {
        const std::string filename(entry->d_name);

        // 임시 파일 정리
        if (filename.size() >= 4U &&
            filename.compare(filename.size() - 4U, 4U, ".tmp") == 0)
        {
            const std::string full_tmp_path = dir + "/" + filename;
            if (::remove(full_tmp_path.c_str()) == 0) {
                std::cout << "[Config] Cleaned up leftover temp file: '" << filename << "'" << std::endl;
            }
            continue;
        }

        if (hasJsonExtension(filename)) {
            if (filename.find(".bak") == std::string::npos) {
                if (filename != "gateway.json") {
                    json_files.push_back(dir + "/" + filename);
                }
            }
        }
    }
    ::closedir(d);

    if (json_files.empty()) {
        std::cerr << "[Config] Warning: No .json files found in '"
                  << dir << "'." << std::endl;
        if (out_error != nullptr) {
            *out_error = "No .json profile files found in '" + dir + "'";
        }
        return false;
    }

    std::sort(json_files.begin(), json_files.end());

    std::vector<DeviceProfile> loaded_profiles;

    for (const auto& path : json_files) {
        DeviceProfile profile;
        std::string item_err;
        bool contradiction = false;
        if (loadProfile(path, profile, &item_err, &contradiction)) {
            loaded_profiles.push_back(std::move(profile));
        } else {
            if (contradiction) {
                if (out_is_contradiction != nullptr) {
                    *out_is_contradiction = true;
                }
                if (out_error != nullptr) {
                    *out_error = item_err;
                }
                return false;
            }
            const std::string err = item_err.empty()
                ? "Failed to load profile '" + path + "'"
                : item_err;
            std::cerr << "[Config] Aborting profile load: " << err << std::endl;
            if (out_error != nullptr) {
                *out_error = err;
            }
            return false;
        }
    }

    if (loaded_profiles.empty()) {
        std::cerr << "[Config] Error: All config files failed to load." << std::endl;
        if (out_error != nullptr && out_error->empty()) {
            *out_error = "All config files in '" + dir + "' failed to load";
        }
        return false;
    }

    if (!assignInstances(loaded_profiles)) {
        std::cerr << "[Config] Error: Instance assignment failed due to conflicts." << std::endl;
        if (out_error != nullptr) {
            *out_error = "Instance assignment failed due to conflicts across profiles";
        }
        return false;
    }

    profiles.swap(loaded_profiles);

    std::vector<ObjectMapping*> loaded_mappings;
    for (auto& profile : profiles) {
        for (auto& m : profile.mappings) {
            loaded_mappings.push_back(&m);
        }
    }
    mappings.swap(loaded_mappings);

    std::cout << "[Config] Total " << mappings.size()
              << " object(s) loaded from " << profiles.size()
              << " file(s) in '" << dir << "'." << std::endl;
    return true;
}

} // namespace gateway
