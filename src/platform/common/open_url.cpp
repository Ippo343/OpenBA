// ============================================================================
// src/platform/common/open_url.cpp -- what Platform::Process::Open hands SDL_OpenURL
// ============================================================================
// Phase D.4 SDL step W7. The rules are in open_url.h.
// ============================================================================

#include "open_url.h"

namespace Platform {
namespace Process {

namespace {

bool IsAlpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

// RFC 3986 scheme ":" with at least two scheme characters, so a drive letter ("C:")
// is not taken for one.
bool HasScheme(const std::string& s) {
    if (s.size() < 3 || !IsAlpha(s[0])) return false;
    for (size_t i = 1; i < s.size(); ++i) {
        const char c = s[i];
        if (c == ':') return i >= 2;
        if (!(IsAlpha(c) || IsDigit(c) || c == '+' || c == '-' || c == '.')) return false;
    }
    return false;
}

std::string EncodePath(const std::string& path) {
    std::string out;
    for (const char c : path) {
        switch (c) {
            case '\\': out += '/';   break;
            case ' ':  out += "%20"; break;
            case '%':  out += "%25"; break;
            case '#':  out += "%23"; break;
            case '?':  out += "%3F"; break;
            default:   out += c;     break;
        }
    }
    return out;
}

}  // namespace

std::string ToOpenUrl(const std::string& target) {
    if (HasScheme(target)) return target;
    const bool drive = target.size() >= 3 && IsAlpha(target[0]) && target[1] == ':' &&
                       (target[2] == '\\' || target[2] == '/');
    const bool unc   = target.size() >= 2 && (target[0] == '\\' || target[0] == '/') &&
                       (target[1] == '\\' || target[1] == '/');
    if (drive) return "file:///" + EncodePath(target);
    if (unc)   return "file:" + EncodePath(target);            // "//server/share..."
    if (!target.empty() && target[0] == '/') return "file://" + EncodePath(target);
    return target;                                             // relative: unchanged
}

}  // namespace Process
}  // namespace Platform
