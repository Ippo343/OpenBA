// ============================================================================
// src/platform/common/platform_settings.h — the settings seam (Phase D.4 SETTINGS E.2)
// ============================================================================
// NOT RETAIL. The four OS services the settings file and the save directory
// need (docs/runtime_port/phase_d/plans/SETTINGS.md §C, header spelling per
// D4_RULES C1). Every path and string is UTF-8; no OS header is included here.
//
// Implementations:
//   Windows  src/platform/win/settings_win.cpp
//   POSIX    (later) src/platform/posix/settings_posix.cpp — DocumentsDir is
//            the per-user data dir WITHOUT getenv (getpwuid, or SDL's pref
//            path once SDL3 lands); ReadLegacySettings returns false.
//
// Callers today: IO::FileSystem::GetSaveDirectory (DocumentsDir). The file
// functions and the legacy import get their caller in SETTINGS E.3
// (persistence_settings.cpp). Per D4_RULES C3 the two file functions are
// implemented directly in settings_win.cpp until THREADS' file primitives
// land, then re-based on them in one refactor commit.
// ============================================================================
#pragma once

#include <cstddef>
#include <string>

namespace System { class String; }

namespace Platform {

// The user's documents folder (Windows: CSIDL_PERSONAL), UTF-8, no trailing
// separator. false when the OS cannot say; `out` is then untouched.
bool DocumentsDir(System::String& out);

// The whole file as bytes. false when it cannot be opened (absent included);
// `out` is then untouched.
bool ReadWholeFile(const char* pathUtf8, std::string& out);

// Write `n` bytes to `<path>.tmp`, flush, then replace `path` with it. The
// parent directory chain is created first if absent. false on any failure
// (the tmp file is removed; `path` keeps its old content).
bool WriteFileAtomic(const char* pathUtf8, const char* data, size_t n);

// The read-only import of a legacy settings store (Windows: retail's
// HKCU\Software\Shining Rock Software LLC\Banished, opened KEY_READ, never
// created). Calls `emit` once per string value with UTF-8 key and value.
// false when there is no legacy store (POSIX: always).
bool ReadLegacySettings(void (*emit)(void* ctx, const char* key, const char* value),
                        void* ctx);

}  // namespace Platform
