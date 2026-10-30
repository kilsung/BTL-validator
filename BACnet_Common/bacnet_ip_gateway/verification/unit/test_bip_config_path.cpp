/**
 * @file test_bip_config_path.cpp
 * @brief bip.cfg 경로 계산 로직 단위 테스트
 *
 * 테스트 대상:
 *  - bip_config_path::resolve(): config 폴더의 상위 폴더 기준 bip.cfg 절대경로 계산
 *
 * 하드웨어 의존성 없음. BACstac API 를 사용하지 않으므로
 * 독립 바이너리로 컴파일/실행 가능합니다.
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "common/bip_config_path.h"
#include <string>

using gateway::bip_config_path::resolve;

TEST_CASE("BipConfigPath: default ./conf resolves to cwd/bip.cfg") {
    ASSERT_EQ(std::string("/opt/gw/bip.cfg"), resolve("./conf", "/opt/gw"));
}

TEST_CASE("BipConfigPath: relative conf without ./ prefix") {
    ASSERT_EQ(std::string("/opt/gw/bip.cfg"), resolve("conf", "/opt/gw"));
}

TEST_CASE("BipConfigPath: trailing slash on config_dir is ignored") {
    ASSERT_EQ(std::string("/opt/gw/bip.cfg"), resolve("./conf/", "/opt/gw"));
    ASSERT_EQ(std::string("/etc/gateway/bip.cfg"), resolve("/etc/gateway/conf//", "/opt/gw"));
}

TEST_CASE("BipConfigPath: absolute config_dir ignores cwd") {
    ASSERT_EQ(std::string("/etc/gateway/bip.cfg"), resolve("/etc/gateway/conf", "/opt/gw"));
    ASSERT_EQ(std::string("/etc/gateway/bip.cfg"), resolve("/etc/gateway/conf", ""));
}

TEST_CASE("BipConfigPath: dot and dot-dot segments are normalized") {
    ASSERT_EQ(std::string("/opt/bip.cfg"), resolve("../conf", "/opt/gw"));
    ASSERT_EQ(std::string("/opt/gw/test/bip.cfg"), resolve("./test/./x/../conf", "/opt/gw"));
}

TEST_CASE("BipConfigPath: empty config_dir falls back to cwd/bip.cfg") {
    ASSERT_EQ(std::string("/opt/gw/bip.cfg"), resolve("", "/opt/gw"));
}

TEST_CASE("BipConfigPath: config_dir at filesystem root stays at root") {
    ASSERT_EQ(std::string("/bip.cfg"), resolve("/conf", "/opt/gw"));
    ASSERT_EQ(std::string("/bip.cfg"), resolve("/", "/opt/gw"));
}
