// ============================================================================
// src/engine/video/video_resources_platform.cpp — Wave 18 lane VRES
//   The single OS call in ??1Device@Video@@ (DeleteCriticalSection) and the
//   three WriteFile calls in ?SaveImageToFile@ImageBuffer@Video@@.
//   Was src/platform/win/video_resources_win.cpp. Phase D.4 THREADS E.3 put the
//   lock on Platform::MutexDestroy; SDL-5 put the WriteFile on Platform::FileWrite
//   (platform_file.h) and moved the file here: it has no OS header.
// ============================================================================
//
// PLAN §4.1 decision 1 kept src/engine/ free of <windows.h> outside a `_win`
// twin, so these lived in src/platform/win and video_resources.cpp /
// save_image_to_file.cpp stayed portable. They are portable themselves now.
//
// B.4 (R-VIDEO): this TU reads `Device::lock` by name (src/engine/video/Device.h).
//
// Retail: `LEA RCX,[RBX+0x60] / CALL qword ptr [0x18014b370]` at 0x1800f72d9.
// 0x18014b370 is Runtime-x64.dll's IAT slot for KERNEL32!DeleteCriticalSection
// (confirmed against the shipped import directory with pefile, not inferred
// from the surrounding code).
// ============================================================================

#include <cstdint>

#include "video/Device.h"   // Video::Device::lock
#include "platform_file.h"   // Platform::FileWrite, Platform::FileHandle
#include "platform_sync.h"   // Platform::MutexDestroy

void RtVideoDeleteDeviceLock(Video::Device* device) {
    // Retail does not null-check `this` — nor does it need to, the caller is
    // the destructor. `lock` is the opaque CRITICAL_SECTION bytes (RULE B5-6).
    Platform::MutexDestroy(&device->lock);   // DeleteCriticalSection
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
// writing. The handle is IO::FileInfo::winHandle, a Platform::FileHandle (the
// opener's; on Windows the HANDLE itself), so it passes straight through. Platform::FileWrite is
// WriteFile(h, buf, bytes, &n, NULL) with its `n` pre-zeroed and always stored to
// `*written` (src/platform/win/file_win.cpp), so a failed write still reads zero.
extern "C" uint32_t RtVideoWriteFileRaw(Platform::FileHandle handle, const void* buf,
                                       uint32_t bytes) {
    uint32_t written = 0;
    Platform::FileWrite(handle, buf, bytes, &written);  // WriteFile
    return written;
}
