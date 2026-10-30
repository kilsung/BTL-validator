#pragma once

namespace gateway {
namespace relinquish_policy {

inline double clamp(double candidate,
                    bool has_min_value,
                    double min_value,
                    bool has_max_value,
                    double max_value)
{
    double result = candidate;
    if (has_min_value && result < min_value) {
        result = min_value;
    }
    if (has_max_value && result > max_value) {
        result = max_value;
    }
    return result;
}

inline bool isValidBinaryValue(bool is_binary_commandable, double value)
{
    return !is_binary_commandable || value == 0.0 || value == 1.0;
}

} // namespace relinquish_policy
} // namespace gateway
