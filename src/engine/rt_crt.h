#pragma once
// ============================================================================
// src/engine/rt_crt.h -- the ported retail CRT string bodies, by name
// ============================================================================
//
// `rtcrt::StrICmp` is retail's `_stricmp` (0x18007eefc), ported in
// src/engine/crt/msvc_stricmp.cpp. Every case-insensitive compare in the tree
// calls it instead of the host CRT's `_stricmp` (Phase D.4 Q9: the ported
// retail body everywhere). The host's `_stricmp` agrees with it only while
// nothing selects a locale (msvc_stricmp.cpp's banner), and glibc has no
// `_stricmp` at all; this body is plain C and the same on every platform.
// rt_libm.h includes this header, so its existing includers still see it.

namespace rtcrt {
// ---- crt/msvc_stricmp.cpp
int StrICmp(const char* a, const char* b);  // 0x18007eefc
}  // namespace rtcrt
