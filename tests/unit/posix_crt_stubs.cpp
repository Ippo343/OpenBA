// ============================================================================
// tests/unit/posix_crt_stubs.cpp -- the MSVC-CRT names the unit link needs off Windows
// ============================================================================
// Phase D.4 LINUX-1. Empty on Windows, where the MinGW CRT provides them.
//
// _purecall: src/engine/video/CompanionData.h's CompanionPureCall names the MSVC CRT's
// pure-virtual handler directly (retail's `_purecall` slots, 0x18007ec28), and the
// unit links companion_operators.cpp. glibc has no such symbol. The game exe's is the
// POSIX crash handler's (src/diag/posix/crash_handler_posix.cpp: log, exit 4); the unit
// does not link the crash handler, so this is the unit's: the message GCC's
// __cxa_pure_virtual prints, then abort -- a test that reaches it fails loudly. (It
// was src/platform/stub/seam_stubs.cpp's PORT GAP until the crash handler existed.)
// ============================================================================

#if !defined(_WIN32)

#include <cstdio>
#include <cstdlib>

extern "C" void _purecall() {
    std::fputs("pure virtual method called (_purecall)\n", stderr);
    std::abort();
}

#endif  // !_WIN32
