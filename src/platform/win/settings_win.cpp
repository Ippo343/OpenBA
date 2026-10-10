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
//   ReadWholeFile / WriteFileAtomic  CreateFileW / ReadFile / WriteFile /
//                                    MoveFileExW / CreateDirectoryW over the
//                                    engine's own UTF-8 -> UTF-16 conversion
//                                    (WideString's const char* ctor,
//                                    0x1800cb110). Per D4_RULES C3 these stay
//                                    here until THREADS' file primitives land.
//   ReadLegacySettings               the one-time, read-only import of
//                                    retail's registry store: KEY_READ, never
//                                    created, REG_SZ values only.
// ============================================================================

#include <windows.h>
#include <shlobj.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform_settings.h"
#include "system/String.h"
#include "system/WideString.h"

namespace {

// The retail store IO::Registry's ctor opened (0x1800b9500 builds it with
// Format("Software\\%s\\%s", "Shining Rock Software LLC", "Banished")).
const wchar_t kLegacyKey[] = L"Software\\Shining Rock Software LLC\\Banished";

// `len` UTF-16 units -> UTF-8, through the engine's AsUTF8.
std::string WideToUtf8(const wchar_t* w, size_t len) {
    if (len == 0)
        return std::string();
    std::wstring copy(w, len);              // NUL-terminate a counted run
    System::WideString ws(copy.c_str());
    System::String utf8 = ws.AsUTF8();
    return std::string(utf8._data ? utf8._data : "");
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

bool ReadWholeFile(const char* pathUtf8, std::string& out) {
    if (pathUtf8 == nullptr || pathUtf8[0] == 0)
        return false;
    const System::WideString wpath(pathUtf8);

    HANDLE h = CreateFileW(wpath._data, GENERIC_READ,
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
    const System::WideString wpathW(pathUtf8);
    const std::wstring wpath(wpathW._data);
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
