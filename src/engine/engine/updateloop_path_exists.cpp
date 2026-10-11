// ============================================================================
// src/engine/engine/updateloop_path_exists.cpp -- the one OS call in
// ?LoadResources@UpdateLoop@Engine@@ (Wave 23 lane ENGINE)
// ============================================================================
// Was src/platform/win/engine_platform_win.cpp. Phase D.4 SDL-5 (SDL Block 5 D1c) put the
// call on the THREADS file primitive (Platform::PathAttributes, platform_file.h) and moved
// the file here: it has no OS header now. Its other function, RtEnginePlatformShiftHeld
// (PlatformConfigure's two GetAsyncKeyState calls), is Platform::Input::ShiftHeld
// (src/platform/win/input_win.cpp, src/platform/posix/input_posix.cpp). The launcher
// dialog notes that headed the old file are in src/engine/engine/engine_platform.cpp,
// beside PlatformConfigure.
//
// Retail (0x1800dd9c4):
//     MOV RCX,[RAX] ; CALL qword ptr [0x18014b0d0]   ; kernel32!GetFileAttributesW
//     CMP EAX,-1    ; SETNZ
// The IAT slot was read out of Runtime-x64.dll's import directory. The test is against
// INVALID_FILE_ATTRIBUTES (0xFFFFFFFF) and NOTHING else -- a directory and a read-only file
// both count as "exists", which is retail's behaviour. Platform::PathAttributes is that
// GetFileAttributesW (src/platform/win/file_win.cpp) and returns kInvalidPathAttributes
// (0xFFFFFFFF) for a missing path on every host.
// ============================================================================

#include "platform_file.h"   // Platform::PathAttributes, kInvalidPathAttributes

// Declared in updateloop_loadresources.cpp, its only caller.
bool RtEnginePlatformWidePathExists(const char16_t* path) {
    if (path == nullptr) return false;
    return Platform::PathAttributes(path) != Platform::kInvalidPathAttributes;
}
