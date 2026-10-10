// ============================================================================
// src/platform/win/settings_win.cpp — the settings seam, Win32 (Phase D.4 SETTINGS E.2)
// ============================================================================
// src/platform/common/platform_settings.h declares the four functions; this
// file is their Windows body and the only place the seam touches wchar_t.
//
// DocumentsDir is the OS half of retail's FileSystem::GetSaveDirectory
// (0x1800acfd0), moved here unchanged from the deleted
// src/platform/win/filesystem_win.cpp: SHGetFolderPathW(CSIDL_PERSONAL) into
// wchar_t[264], then System::WideString's ctor (0x18000a6e0) and AsUTF8
// (0x1800cacd0). The Format / Filename / Error::SetText half stays in
// src/engine/io/filesystem.cpp.
//
// The rest is NOT retail (plans/SETTINGS.md §B, §C):
//   DocumentsDir(std::string&)       the same SHGetFolderPathW, converted
//                                    without the engine allocator.
//   ReadWholeFile / WriteFileAtomic  CreateFileW / ReadFile / WriteFile /
//                                    MoveFileExW / CreateDirectoryW over
//                                    MultiByteToWideChar(CP_UTF8) for the
//                                    path. Per D4_RULES C3 these stay here
//                                    until THREADS' file primitives land.
//   ReadLegacySettings               the one-time, read-only import of
//                                    retail's registry store: KEY_READ, never
//                                    created, REG_SZ values only.
//
// ALLOCATOR-FREE (SETTINGS E.3). Everything but the System::String
// DocumentsDir uses std::string / std::wstring only, because
// src/app/main.cpp's harness guard runs before Engine::Startup constructs
// System::MemoryAllocator. The UTF-16 -> UTF-8 conversion below is
// WideString::AsUTF8's rule (0x1800cacd0: a valid surrogate pair is one
// 4-byte sequence, a lone surrogate encodes to ZERO bytes), so an imported
// key or value is byte-identical to what retail's ReadSettings put in the map.
// ============================================================================

#include <windows.h>
#include <shlobj.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "platform_settings.h"
#include "system/String.h"
#include "system/WideString.h"

namespace {

// The retail store IO::Registry's ctor opened (0x1800b9500 builds it with
// Format("Software\\%s\\%s", "Shining Rock Software LLC", "Banished")).
const wchar_t kLegacyKey[] = L"Software\\Shining Rock Software LLC\\Banished";

// `len` UTF-16 units -> UTF-8 by WideString::AsUTF8's rule (the banner):
// a high surrogate followed by a low one is one supplementary code point; a
// lone high or low surrogate is dropped. No engine allocation.
std::string WideToUtf8(const wchar_t* w, size_t len) {
    std::string out;
    out.reserve(len);
    for (size_t i = 0; i < len; ++i) {
        uint32_t c = static_cast<uint16_t>(w[i]);
        if (c >= 0xD800 && c < 0xDC00) {
            if (i + 1 >= len) continue;                       // lone high
            const uint32_t c2 = static_cast<uint16_t>(w[i + 1]);
            if (c2 < 0xDC00 || c2 >= 0xE000) continue;        // lone high
            c = 0x10000u + (((c - 0xD800u) << 10) | (c2 - 0xDC00u));
            ++i;
        } else if (c >= 0xDC00 && c < 0xE000) {
            continue;                                         // lone low
        }
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (c >> 12)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (c >> 18)));
            out.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

// A UTF-8 path -> UTF-16 for the W file APIs. Empty on a conversion failure,
// which every caller treats as "cannot open".
std::wstring Utf8ToWide(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 0)
        return std::wstring();
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, &out[0], n);
    out.resize(static_cast<size_t>(n - 1));                   // the NUL
    return out;
}

// The directory part of a wide path (up to the last separator), or empty.
std::wstring ParentOf(const std::wstring& path) {
    const size_t cut = path.find_last_of(L"\\/");
    if (cut == std::wstring::npos)
        return std::wstring();
    return path.substr(0, cut);
}

// CreateDirectoryW on every prefix of `dir`, ignoring failures: an existing
// directory, a drive root or a UNC host fail harmlessly, and a real failure
// surfaces at the CreateFileW that follows.
void CreateDirectoryChain(const std::wstring& dir) {
    for (size_t i = 1; i <= dir.size(); ++i) {
        if (i == dir.size() || dir[i] == L'\\' || dir[i] == L'/') {
            const std::wstring prefix = dir.substr(0, i);
            if (!prefix.empty() && prefix.back() != L':')
                CreateDirectoryW(prefix.c_str(), nullptr);
        }
    }
}

bool WriteAll(HANDLE h, const char* data, size_t n) {
    while (n > 0) {
        const DWORD chunk = n > 0x40000000u ? 0x40000000u : static_cast<DWORD>(n);
        DWORD written = 0;
        if (!WriteFile(h, data, chunk, &written, nullptr) || written == 0)
            return false;
        data += written;
        n -= written;
    }
    return true;
}

}  // namespace

namespace Platform {

bool DocumentsDir(System::String& out) {
    wchar_t buf[264];
    const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, buf);
    if (hr != S_OK)
        return false;

    // 0x1800ad034: the WideString slot is zeroed, then Set (0x1800caf40) --
    // the `const wchar_t*` ctor is exactly that pair. AsUTF8 returns the
    // String by value.
    System::WideString ws(buf);
    out = ws.AsUTF8();
    return true;
}

bool DocumentsDir(std::string& out) {
    wchar_t buf[264];
    const HRESULT hr = SHGetFolderPathW(nullptr, CSIDL_PERSONAL, nullptr, 0, buf);
    if (hr != S_OK)
        return false;
    out = WideToUtf8(buf, wcslen(buf));
    return true;
}

bool ReadWholeFile(const char* pathUtf8, std::string& out) {
    if (pathUtf8 == nullptr || pathUtf8[0] == 0)
        return false;
    const std::wstring wpath = Utf8ToWide(pathUtf8);
    if (wpath.empty())
        return false;

    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    std::string data;
    char chunk[16384];
    bool ok = true;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, chunk, sizeof(chunk), &got, nullptr)) {
            ok = false;
            break;
        }
        if (got == 0)
            break;
        data.append(chunk, got);
    }
    CloseHandle(h);
    if (!ok)
        return false;
    out.swap(data);
    return true;
}

bool WriteFileAtomic(const char* pathUtf8, const char* data, size_t n) {
    if (pathUtf8 == nullptr || pathUtf8[0] == 0)
        return false;
    const std::wstring wpath = Utf8ToWide(pathUtf8);
    if (wpath.empty())
        return false;
    const std::wstring wtmp = wpath + L".tmp";

    const std::wstring parent = ParentOf(wpath);
    if (!parent.empty())
        CreateDirectoryChain(parent);

    HANDLE h = CreateFileW(wtmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    bool ok = WriteAll(h, data, n);
    if (ok && !FlushFileBuffers(h))
        ok = false;
    CloseHandle(h);

    if (ok && !MoveFileExW(wtmp.c_str(), wpath.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        ok = false;
    if (!ok)
        DeleteFileW(wtmp.c_str());
    return ok;
}

bool ReadLegacySettings(void (*emit)(void* ctx, const char* key, const char* value),
                        void* ctx) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kLegacyKey, 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    DWORD valueCount = 0, maxNameLen = 0, maxDataBytes = 0;
    if (RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                         &valueCount, &maxNameLen, &maxDataBytes, nullptr,
                         nullptr) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }

    std::vector<wchar_t> name(static_cast<size_t>(maxNameLen) + 1);
    // +1 unit so a REG_SZ stored without its terminator still ends in one.
    std::vector<wchar_t> value(static_cast<size_t>(maxDataBytes) / sizeof(wchar_t) + 1);

    for (DWORD i = 0; i < valueCount; ++i) {
        DWORD nameLen = static_cast<DWORD>(name.size());
        DWORD type = 0;
        DWORD bytes = static_cast<DWORD>((value.size() - 1) * sizeof(wchar_t));
        const LONG rc = RegEnumValueW(key, i, name.data(), &nameLen, nullptr, &type,
                                      reinterpret_cast<BYTE*>(value.data()),  // CAST-OK: win32
                                      &bytes);
        if (rc == ERROR_NO_MORE_ITEMS)
            break;
        if (rc != ERROR_SUCCESS)
            continue;

        const std::string keyUtf8 = WideToUtf8(name.data(), nameLen);
        if (type != REG_SZ) {
            std::fprintf(stderr,
                         "[Settings] legacy import: skipped non-REG_SZ value '%s' (type %lu)\n",
                         keyUtf8.c_str(), static_cast<unsigned long>(type));
            continue;
        }

        size_t units = bytes / sizeof(wchar_t);
        while (units > 0 && value[units - 1] == L'\0')
            --units;                        // the stored terminator(s)
        const std::string valueUtf8 = WideToUtf8(value.data(), units);
        emit(ctx, keyUtf8.c_str(), valueUtf8.c_str());
    }

    RegCloseKey(key);
    return true;
}

}  // namespace Platform
