/**
 * @file test_bip_config_file.cpp
 * @brief bip.cfg key=value 파서/갱신 및 심볼릭 링크 교체 단위 테스트
 *
 * 테스트 대상:
 *  - bip_config_file::findValue(): 주석/공백/대소문자 처리
 *  - bip_config_file::updateContent(): 값 치환, 주석 보존, 누락 key 추가
 *  - bip_config_file::replaceSymlink(): bip.cfg 링크 교체 (임시 폴더에서 실제 파일 시스템 사용)
 *  - bip_config_file::parseIpv4() / formatIpv4()
 *
 * 하드웨어 의존성 없음. BACstac API 를 사용하지 않으므로
 * 독립 바이너리로 컴파일/실행 가능합니다.
 *
 * 표준: C++11 / 외부 라이브러리 의존 없음
 */

#include "test_runner.h"
#include "common/bip_config_file.h"

#include <cstdlib>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>

namespace bcf = gateway::bip_config_file;

namespace {

const char* const kFdSample =
    "# FD (Foreign Device) mode example\n"
    "mode=fd\n"
    "fd.ip=192.168.1.31\n"
    "#fd.bbmd_ip=10.0.0.1\n"
    "fd.bbmd_ip=192.168.1.31   # BBMD\n"
    "fd.bbmd_port=47809\n"
    "FD.TTL = 1800\n";

std::string makeTempDir()
{
    const char* base = std::getenv("TMPDIR");
    std::string pattern = std::string(base != nullptr ? base : "/tmp") + "/bipcfg-XXXXXX";
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    const char* dir = ::mkdtemp(buffer.data());
    return dir != nullptr ? std::string(dir) : std::string();
}

std::string readLink(const std::string& path)
{
    char buffer[256];
    const ssize_t len = ::readlink(path.c_str(), buffer, sizeof(buffer) - 1U);
    if (len < 0) {
        return std::string();
    }
    buffer[len] = '\0';
    return std::string(buffer);
}

} // namespace

TEST_CASE("BipConfigFile: findValue ignores comments, spaces and key case") {
    std::string value;
    ASSERT_TRUE(bcf::findValue(kFdSample, "fd.bbmd_ip", value));
    ASSERT_EQ(std::string("192.168.1.31"), value);
    ASSERT_TRUE(bcf::findValue(kFdSample, "fd.ttl", value));
    ASSERT_EQ(std::string("1800"), value);
    ASSERT_FALSE(bcf::findValue(kFdSample, "fd.reg_timeout", value));
}

TEST_CASE("BipConfigFile: updateContent replaces values and keeps comments") {
    std::map<std::string, std::string> values;
    values["fd.bbmd_ip"] = "10.1.2.3";
    values["fd.ttl"] = "600";
    const std::string updated = bcf::updateContent(kFdSample, values);

    std::string value;
    ASSERT_TRUE(bcf::findValue(updated, "fd.bbmd_ip", value));
    ASSERT_EQ(std::string("10.1.2.3"), value);
    ASSERT_TRUE(bcf::findValue(updated, "fd.ttl", value));
    ASSERT_EQ(std::string("600"), value);
    ASSERT_TRUE(bcf::findValue(updated, "fd.bbmd_port", value));
    ASSERT_EQ(std::string("47809"), value);
    ASSERT_TRUE(updated.find("# FD (Foreign Device) mode example\n") != std::string::npos);
    ASSERT_TRUE(updated.find("#fd.bbmd_ip=10.0.0.1\n") != std::string::npos);
}

TEST_CASE("BipConfigFile: updateContent appends missing keys") {
    std::map<std::string, std::string> values;
    values["fd.reg_timeout"] = "3000";
    const std::string updated = bcf::updateContent("mode=fd\n", values);
    ASSERT_EQ(std::string("mode=fd\nfd.reg_timeout=3000\n"), updated);
}

TEST_CASE("BipConfigFile: updateFile and replaceSymlink on real files") {
    const std::string dir = makeTempDir();
    ASSERT_FALSE(dir.empty());
    const std::string link = dir + "/bip.cfg";
    ASSERT_TRUE(bcf::writeFileAtomic(link + ".normal", "mode=normal\n"));
    ASSERT_TRUE(bcf::writeFileAtomic(link + ".fd", kFdSample));
    // 기존 일반 파일도 링크로 바뀌어야 한다.
    ASSERT_TRUE(bcf::writeFileAtomic(link, "mode=normal\n"));

    ASSERT_TRUE(bcf::replaceSymlink(link, bcf::siblingName(link, bcf::kForeignSuffix)));
    ASSERT_EQ(std::string("bip.cfg.fd"), readLink(link));
    ASSERT_TRUE(bcf::replaceSymlink(link, bcf::siblingName(link, bcf::kNormalSuffix)));
    ASSERT_EQ(std::string("bip.cfg.normal"), readLink(link));

    std::map<std::string, std::string> values;
    values["fd.bbmd_port"] = "47808";
    const std::string fdPath = bcf::siblingPath(link, bcf::kForeignSuffix);
    ASSERT_TRUE(bcf::updateFile(fdPath, values));
    std::string content;
    ASSERT_TRUE(bcf::readFile(fdPath, content));
    std::string value;
    ASSERT_TRUE(bcf::findValue(content, "fd.bbmd_port", value));
    ASSERT_EQ(std::string("47808"), value);

    ::unlink(link.c_str());
    ::unlink((link + ".normal").c_str());
    ::unlink(fdPath.c_str());
    ::rmdir(dir.c_str());
}

TEST_CASE("BipConfigFile: parseIpv4 accepts dotted quad only") {
    uint8_t bytes[4] = { 0U, 0U, 0U, 0U };
    ASSERT_TRUE(bcf::parseIpv4("192.168.1.31", bytes));
    ASSERT_EQ(std::string("192.168.1.31"), bcf::formatIpv4(bytes));
    ASSERT_FALSE(bcf::parseIpv4("256.1.1.1", bytes));
    ASSERT_FALSE(bcf::parseIpv4("1.2.3", bytes));
    ASSERT_FALSE(bcf::parseIpv4("1.2.3.4x", bytes));
    ASSERT_FALSE(bcf::parseIpv4("bbmd.local", bytes));
}
