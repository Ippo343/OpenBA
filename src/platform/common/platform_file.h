#pragma once
// ============================================================================
// src/platform/common/platform_file.h -- files (Phase D.4, THREADS seam, E.5)
// ============================================================================
// D4_RULES C1: namespace `Platform`, free functions, NO OS header. Windows bodies:
// src/platform/win/file_win.cpp, one Win32 call (or retail's fixed sequence) each.
//
// PATHS are `const Platform::PathChar*` (D4_RULES C2): `wchar_t` today, the unit of
// System::WideString::_data, so a caller passes `ws._data` unchanged. ABI step C.5
// flips the typedef to `char16_t` in this one line; no primitive signature moves.
//
// RESULTS keep Win32's shapes, because the ported callers test them the way
// retail's code does:
//   * a "BOOL" result is an int32_t, nonzero on success. A POSIX impl returns 1 /
//     0: PersistenceWorker::DeleteJob computes `(ok != 1) + 1`, retail's own
//     arithmetic.
//   * the byte counts are uint32_t (a DWORD), written on every call.
//   * FileSeek32 / FileSize return the 32-bit value the callers compare
//     (SetFilePointer / GetFileSize with no high word: offsets are 32-bit).
//
// THE HANDLE. `FileHandle` is a pointer to an INCOMPLETE type, 8 bytes, the type of
// IO::FileInfo::winHandle (+0x18). On Windows the value IS the HANDLE. Two values
// mean "no file": kInvalidFileHandle (INVALID_HANDLE_VALUE, what FileInfo's ctor
// stores) and null; every opener's caller rejects both (`LEA RAX,[h-1] ; CMP
// RAX,-3`), so a POSIX impl hands out fd + 1 and never 0 or -1 (plans/THREADS.md A.5).
// ============================================================================

#include <cstdint>

namespace Platform {

using PathChar = wchar_t;               // D4_RULES C2: the one line ABI C.5 flips

struct FileObject;                      // never defined on Windows: the pointer is the HANDLE
using FileHandle = FileObject*;
static_assert(sizeof(FileHandle) == 8, "a FileHandle fills IO::FileInfo's 8-byte +0x18 slot");

// INVALID_HANDLE_VALUE: all bits set.
inline FileHandle const kInvalidFileHandle = reinterpret_cast<FileHandle>(~uintptr_t{0});  // CAST-OK: win32

// MAX_PATH: the size, in PathChars, of FileCreateTemp's output buffer.
constexpr uint32_t kMaxPath = 260;

// CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
// FILE_ATTRIBUTE_NORMAL, NULL): FileSystem::PlatformOpenFile 0x1800addb3 and
// GetFileTimeStamp 0x1800ae2c0's argument block.
FileHandle FileOpenRead(const PathChar* path);

// FileSystem::PlatformCreateFile 0x1800ade91..0x1800adee5, in retail's order:
// GetTempPathW(MAX_PATH, dir); GetTempFileNameW(dir, prefix, 0, outPath);
// CreateFileW(outPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
// NULL). `outPath` holds kMaxPath PathChars. A POSIX impl: mkstemp in the temp dir.
FileHandle FileCreateTemp(const PathChar* prefix, PathChar* outPath);

int32_t  FileRead(FileHandle h, void* dst, uint32_t bytes, uint32_t* got);           // ReadFile
int32_t  FileWrite(FileHandle h, const void* src, uint32_t bytes, uint32_t* written); // WriteFile
uint32_t FileSeek32(FileHandle h, int32_t pos);    // SetFilePointer(h, pos, NULL, FILE_BEGIN)
uint32_t FileSize(FileHandle h);                   // GetFileSize(h, NULL)
int32_t  FileClose(FileHandle h);                  // CloseHandle
// GetFileTime(h, NULL, NULL, &lastWrite): FILETIME units (100 ns since 1601 UTC),
// low dword in the low half. `*fileTime` is written only on success.
int32_t  FileLastWrite(FileHandle h, uint64_t* fileTime);

int32_t  FileDelete(const PathChar* path);                        // DeleteFileW
// MoveFileW: it MOVES A FILE ACROSS VOLUMES (copy + delete), so a POSIX impl copies
// and unlinks on EXDEV (plans/THREADS.md F.1).
int32_t  FileMove(const PathChar* from, const PathChar* to);
int32_t  DirCreate(const PathChar* path);                         // CreateDirectoryW(path, NULL)

}  // namespace Platform
