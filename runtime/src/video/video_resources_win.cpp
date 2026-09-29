// ============================================================================
// runtime/src/video/video_resources_win.cpp — Wave 18 lane VRES
//   The single Win32 call in ??1Device@Video@@: DeleteCriticalSection.
// ============================================================================
//
// PLAN §4.1 decision 1 keeps runtime/src/ free of <windows.h> outside a `_win`
// twin, so this one line lives here and video_resources.cpp stays portable.
// Same split, and the same reason, as device_startup_win.cpp and
// buffers_win.cpp.
//
/// B.4 (R-VIDEO): this TU includes runtime/types/video/Device.h AFTER
// <windows.h> (as buffers_win.cpp does) and reads `Device::lock` by name.
//
// Retail: `LEA RCX,[RBX+0x60] / CALL qword ptr [0x18014b370]` at 0x1800f72d9.
// 0x18014b370 is Runtime-x64.dll's IAT slot for KERNEL32!DeleteCriticalSection
// (confirmed against the shipped import directory with pefile, not inferred
// from the surrounding code).
// ============================================================================

#include <windows.h>
#include <cstdint>

#include "../../types/video/Device.h"   // Video::Device::lock

void RtVideoDeleteDeviceLock(Video::Device* device) {
    // Retail does not null-check `this` — nor does it need to, the caller is
    // the destructor. `lock` is the opaque CRITICAL_SECTION bytes (RULE B5-6).
    DeleteCriticalSection(reinterpret_cast<LPCRITICAL_SECTION>(device->lock));   // CAST-OK: win32
}

// ============================================================================
// The three WriteFile calls in ?SaveImageToFile@ImageBuffer@Video@@
// ============================================================================
//
// Retail's shape at 0x1800f2a33 / 0x1800f2a5a / 0x1800f2af7 is identical three
// times: zero a DWORD, `WriteFile(handle, buf, len, &written, NULL)`, then add
// `written` to the stream's cursor. It NEVER looks at WriteFile's BOOL — a
// failed write contributes whatever the pre-zeroed out-parameter holds, which
// is zero. That is reproduced here rather than turned into an error path: a
// port that returned early on failure would stop writing rows retail keeps
// writing. (`HANDLE` is `void*`: the handle passes straight through.)
extern "C" uint32_t RtVideoWriteFileRaw(void* handle, const void* buf, uint32_t bytes) {
    DWORD written = 0;
    WriteFile(handle, buf, bytes, &written, nullptr);
    return static_cast<uint32_t>(written);
}
