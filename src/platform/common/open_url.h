#pragma once
// ============================================================================
// src/platform/common/open_url.h -- what Platform::Process::Open hands SDL_OpenURL
// ============================================================================
// Phase D.4 SDL step W7 (plan §A.6). Retail's Application::LaunchProcess "open" arm is
// ShellExecuteW(NULL, L"open", target, ...): `target` may be a URL or a file / folder
// path. SDL_OpenURL takes a URL ("file:///" URLs where the platform supports them), so
// an absolute path becomes a file URL here. Pure: no OS header, no SDL header
// (tests/unit/open_url_test.cpp).
//
//   "https://x/y", "mailto:a@b"   -> unchanged (a scheme of two or more characters)
//   "C:\Games\a b.txt"            -> "file:///C:/Games/a%20b.txt"
//   "\\server\share\x"            -> "file://server/share/x"
//   "/home/u/x"                   -> "file:///home/u/x"
//   "readme.txt" (relative)       -> unchanged: a file URL cannot be relative; on
//                                    Windows SDL passes it to ShellExecuteW as retail
//                                    did, which resolves it against the current folder.
// In a converted path, '\' becomes '/' and ' ', '%', '#' and '?' are percent-encoded;
// every other byte (UTF-8 included) is kept.
// ============================================================================

#include <string>

namespace Platform {
namespace Process {

std::string ToOpenUrl(const std::string& target);

}  // namespace Process
}  // namespace Platform
