#pragma once

#include <string>
#include <vector>

namespace gateway {
namespace bip_config_path {

static const char* const kBipConfigFileName = "bip.cfg";

/**
 * @brief config 폴더의 상위 폴더에 위치한 bip.cfg 절대경로를 계산합니다.
 *
 * 경로는 파일 시스템 조회 없이 문자열만으로 정규화합니다("."/".."/중복 "/" 처리).
 * 따라서 config_dir가 심볼릭 링크여도 링크가 놓인 위치의 상위 폴더를 기준으로 합니다.
 *
 * @param config_dir 실행 인자로 전달된 config 폴더 경로 (상대/절대)
 * @param cwd        상대경로 해석 기준이 되는 현재 작업 폴더 (절대경로)
 * @return 예) config_dir="./conf", cwd="/opt/gw" -> "/opt/gw/bip.cfg"
 *         config_dir가 비어 있으면 cwd 기준 bip.cfg를 반환합니다.
 */
inline std::string resolve(const std::string& config_dir, const std::string& cwd)
{
    const std::string base = config_dir.empty() ? cwd + "/" + kBipConfigFileName
                           : (config_dir[0] == '/' ? config_dir : cwd + "/" + config_dir);

    std::vector<std::string> parts;
    std::string::size_type pos = 0U;
    while (pos <= base.size()) {
        std::string::size_type next = base.find('/', pos);
        if (next == std::string::npos) {
            next = base.size();
        }
        const std::string part = base.substr(pos, next - pos);
        if (part == "..") {
            if (!parts.empty()) {
                parts.pop_back();
            }
        } else if (!part.empty() && part != ".") {
            parts.push_back(part);
        }
        pos = next + 1U;
    }

    // config_dir가 비어 있으면 base가 이미 bip.cfg 파일 경로이고,
    // 아니면 마지막 요소(config 폴더)를 제거해 상위 폴더로 이동한다.
    if (!parts.empty()) {
        parts.pop_back();
    }

    std::string result;
    for (const std::string& part : parts) {
        result += "/" + part;
    }
    return result + "/" + kBipConfigFileName;
}

} // namespace bip_config_path
} // namespace gateway
