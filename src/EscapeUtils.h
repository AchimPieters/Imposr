#pragma once
// Internal utility header — not part of the public API.
// Include from .cpp files only; not shipped in include/aimp/.

#include <string>

namespace aimp {
namespace internal {

inline std::string EscapeJson(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

inline std::string EscapePdf(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
        if (ch == '\\' || ch == '(' || ch == ')') out.push_back('\\');
        out.push_back(ch);
    }
    return out;
}

} // namespace internal
} // namespace aimp
