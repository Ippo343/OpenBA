// ============================================================================
// src/platform/win/file_win.cpp -- Platform:: files over Win32
// ============================================================================
// The Windows half of src/platform/common/platform_file.h. Each body is the Win32
// call (or the fixed sequence) the ported engine code made itself until Phase D.4
// THREADS E.5, with the same arguments.
// ============================================================================

#include "platform_file.h"

#include <windows.h>

static_assert(sizeof(HANDLE) == sizeof(Platform::FileHandle), "a file handle is a HANDLE");
static_assert(Platform::kMaxPath == MAX_PATH, "kMaxPath is MAX_PATH");

namespace {

// D4_RULES C2: the one place a PathChar becomes a Win32 wide string.
inline LPCWSTR Wide(const Platform::PathChar* p) { return reinterpret_cast<LPCWSTR>(p); }  // CAST-OK: win32
inline LPWSTR  Wide(Platform::PathChar* p)       { return reinterpret_cast<LPWSTR>(p); }   // CAST-OK: win32
inline Platform::FileHandle FromWin(HANDLE h)    { return static_cast<Platform::FileHandle>(h); }  // CAST-OK: win32

}  // namespace

namespace Platform {

FileHandle FileOpenRead(const PathChar* path) {
    return FromWin(CreateFileW(Wide(path), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
}

FileHandle FileCreateTemp(const PathChar* prefix, PathChar* outPath) {
    wchar_t tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempDir);
    GetTempFileNameW(tempDir, Wide(prefix), 0, Wide(outPath));
    return FromWin(CreateFileW(Wide(outPath), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
}

int32_t FileRead(FileHandle h, void* dst, uint32_t bytes, uint32_t* got) {
    DWORD n = 0;
    const BOOL ok = ReadFile(h, dst, bytes, &n, nullptr);
    *got = n;
    return ok;
}

int32_t FileWrite(FileHandle h, const void* src, uint32_t bytes, uint32_t* written) {
    DWORD n = 0;
    const BOOL ok = WriteFile(h, src, bytes, &n, nullptr);
    *written = n;
    return ok;
}

uint32_t FileSeek32(FileHandle h, int32_t pos) { return SetFilePointer(h, pos, nullptr, FILE_BEGIN); }
uint32_t FileSize(FileHandle h)                { return GetFileSize(h, nullptr); }
int32_t  FileClose(FileHandle h)               { return CloseHandle(h); }

int32_t FileLastWrite(FileHandle h, uint64_t* fileTime) {
    FILETIME lastWrite;
    const BOOL ok = GetFileTime(h, nullptr, nullptr, &lastWrite);
    if (ok) *fileTime = (static_cast<uint64_t>(lastWrite.dwHighDateTime) << 32) | lastWrite.dwLowDateTime;
    return ok;
}

int32_t FileDelete(const PathChar* path)                   { return DeleteFileW(Wide(path)); }
int32_t FileMove(const PathChar* from, const PathChar* to) { return MoveFileW(Wide(from), Wide(to)); }
int32_t DirCreate(const PathChar* path)                    { return CreateDirectoryW(Wide(path), nullptr); }

}  // namespace Platform
