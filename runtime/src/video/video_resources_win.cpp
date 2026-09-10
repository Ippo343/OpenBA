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
// The offset is a literal here rather than an include of
// runtime/types/video/Device.h, for the reason buffers_win.cpp gives: pulling
// an engine header in behind <windows.h> is the collision PLAN §4.1 is about
// (<wingdi.h> declares a function called `Rectangle`; <windef.h> #defines
// `near` and `far` away). The static_assert that binds this constant to
// `Device::lock` lives in the portable half.
//
// Retail: `LEA RCX,[RBX+0x60] / CALL qword ptr [0x18014b370]` at 0x1800f72d9.
// 0x18014b370 is Runtime-x64.dll's IAT slot for KERNEL32!DeleteCriticalSection
// (confirmed against the shipped import directory with pefile, not inferred
// from the surrounding code).
// ============================================================================

#include <windows.h>
#include <cstdint>

static const size_t kDeviceLock = 0x60;

extern "C" void RtVideoDeleteDeviceLock(void* device) {
    // Retail does not null-check `this` — nor does it need to, the caller is
    // the destructor. The cast is the whole body.
    DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION*>(
        static_cast<uint8_t*>(device) + kDeviceLock));
}
