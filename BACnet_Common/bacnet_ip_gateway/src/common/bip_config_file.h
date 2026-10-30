#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unistd.h>

namespace gateway {
namespace bip_config_file {

/// Normal 모드 설정 파일 접미사 (bip.cfg -> bip.cfg.normal)
static const char* const kNormalSuffix = ".normal";
/// Foreign Device 모드 설정 파일 접미사 (bip.cfg -> bip.cfg.fd)
static const char* const kForeignSuffix = ".fd";

inline std::string trim(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return std::string();
    }
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1U);
}

inline std::string toLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

/**
 * @brief 한 줄에서 활성 key=value 쌍을 추출합니다. ('#' 이후는 주석)
 * @return 활성 설정 줄이면 true. key는 소문자로 반환합니다.
 */
inline bool parseLine(const std::string& line, std::string& key, std::string& value)
{
    std::string body = line;
    const std::size_t comment = body.find('#');
    if (comment != std::string::npos) {
        body.erase(comment);
    }
    const std::size_t separator = body.find('=');
    if (separator == std::string::npos) {
        return false;
    }
    key = toLower(trim(body.substr(0U, separator)));
    value = trim(body.substr(separator + 1U));
    return !key.empty();
}

/// 설정 내용에서 key 값을 찾습니다. (BACstac과 동일하게 key는 대소문자 무시)
inline bool findValue(const std::string& content, const std::string& key, std::string& value)
{
    const std::string wanted = toLower(key);
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        std::string lineKey;
        std::string lineValue;
        if (parseLine(line, lineKey, lineValue) && lineKey == wanted) {
            value = lineValue;
            return true;
        }
    }
    return false;
}

/**
 * @brief 설정 내용의 key 값을 바꿉니다. 주석과 다른 줄은 그대로 유지합니다.
 *        내용에 없는 key는 끝에 "key=value" 줄로 추가합니다.
 */
inline std::string updateContent(const std::string& content,
                                 const std::map<std::string, std::string>& values)
{
    std::map<std::string, bool> applied;
    std::ostringstream out;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line)) {
        std::string lineKey;
        std::string lineValue;
        if (parseLine(line, lineKey, lineValue)) {
            for (const auto& entry : values) {
                if (toLower(entry.first) == lineKey && !applied[lineKey]) {
                    line = entry.first + "=" + entry.second;
                    applied[lineKey] = true;
                    break;
                }
            }
        }
        out << line << '\n';
    }
    for (const auto& entry : values) {
        if (!applied[toLower(entry.first)]) {
            out << entry.first << '=' << entry.second << '\n';
        }
    }
    return out.str();
}

inline bool readFile(const std::string& path, std::string& content)
{
    std::ifstream stream(path.c_str());
    if (!stream.is_open()) {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    content = buffer.str();
    return true;
}

/// 임시 파일에 쓴 뒤 rename()으로 교체해 중간 상태의 파일이 남지 않게 합니다.
inline bool writeFileAtomic(const std::string& path, const std::string& content)
{
    const std::string tmp = path + ".tmp";
    {
        std::ofstream stream(tmp.c_str(), std::ios::out | std::ios::trunc);
        if (!stream.is_open()) {
            return false;
        }
        stream << content;
        stream.flush();
        if (!stream.good()) {
            stream.close();
            (void)std::remove(tmp.c_str());
            return false;
        }
    }
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        (void)std::remove(tmp.c_str());
        return false;
    }
    return true;
}

inline bool updateFile(const std::string& path, const std::map<std::string, std::string>& values)
{
    std::string content;
    if (!readFile(path, content)) {
        return false;
    }
    return writeFileAtomic(path, updateContent(content, values));
}

/// 링크 경로와 같은 폴더의 형제 파일 이름 (예: "/opt/gw/bip.cfg" + ".fd" -> "bip.cfg.fd")
inline std::string siblingName(const std::string& link_path, const char* suffix)
{
    const std::size_t slash = link_path.find_last_of('/');
    const std::string base = slash == std::string::npos ? link_path : link_path.substr(slash + 1U);
    return base + suffix;
}

/// 링크 경로와 같은 폴더의 형제 파일 전체 경로 (예: "/opt/gw/bip.cfg.fd")
inline std::string siblingPath(const std::string& link_path, const char* suffix)
{
    return link_path + suffix;
}

/**
 * @brief link_path를 같은 폴더의 target_name을 가리키는 상대경로 심볼릭 링크로 원자적으로 교체합니다.
 *        link_path가 일반 파일이어도 링크로 바뀝니다.
 */
inline bool replaceSymlink(const std::string& link_path, const std::string& target_name)
{
    const std::string tmp = link_path + ".tmp";
    (void)::unlink(tmp.c_str());
    if (::symlink(target_name.c_str(), tmp.c_str()) != 0) {
        return false;
    }
    if (std::rename(tmp.c_str(), link_path.c_str()) != 0) {
        (void)::unlink(tmp.c_str());
        return false;
    }
    return true;
}

/// "a.b.c.d" 형식 IPv4 문자열을 4바이트로 변환합니다.
inline bool parseIpv4(const std::string& text, uint8_t out[4])
{
    unsigned int octets[4] = { 0U, 0U, 0U, 0U };
    char tail = '\0';
    if (std::sscanf(text.c_str(), "%u.%u.%u.%u%c",
                    &octets[0], &octets[1], &octets[2], &octets[3], &tail) != 4) {
        return false;
    }
    for (std::size_t i = 0U; i < 4U; ++i) {
        if (octets[i] > 255U) {
            return false;
        }
        out[i] = static_cast<uint8_t>(octets[i]);
    }
    return true;
}

inline std::string formatIpv4(const uint8_t bytes[4])
{
    std::ostringstream out;
    out << static_cast<unsigned int>(bytes[0]) << '.' << static_cast<unsigned int>(bytes[1]) << '.'
        << static_cast<unsigned int>(bytes[2]) << '.' << static_cast<unsigned int>(bytes[3]);
    return out.str();
}

} // namespace bip_config_file
} // namespace gateway
