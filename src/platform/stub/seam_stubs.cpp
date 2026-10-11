// ============================================================================
// src/platform/stub/seam_stubs.cpp -- link stubs for seam functions with no POSIX body
// ============================================================================
// Phase D.4 POSIX (plans/ABI.md C.7). NOT COMPILED ON WINDOWS: CMakeLists.txt globs
// src/platform/stub/ only off WIN32, and the Makefile never sees it.
//
// Each entry is a function the portable tree calls whose Windows body lives in a
// Windows-only place (the MSVC / MinGW CRT, src/platform/win, src/diag/win, src/app)
// and that has no POSIX body yet. Each says which seam owes the body. A stub here is
// a PORT GAP, not an implementation: it reports and aborts, never pretends.
// ============================================================================

#include <cstdio>
#include <cstdlib>

// PORT GAP (CRT / diag seam, D4_RULES C10): _purecall. The MSVC-ABI CRT's pure-virtual
// handler, which retail's `_purecall` slots (0x18007ec28) call and which
// src/engine/video/CompanionData.h's CompanionPureCall names directly. On Windows the
// CRT provides it and src/diag/win/crash_handler_win.cpp overrides it to log the
// caller before the exit-3 crash path. glibc has no such symbol (GCC's own handler is
// __cxa_pure_virtual); the POSIX crash handler (crash_handler_posix.cpp, when a Linux
// exe exists) takes this over. Until then: the same message __cxa_pure_virtual prints,
// then abort.
extern "C" void _purecall() {
    std::fputs("pure virtual method called (_purecall)\n", stderr);
    std::abort();
}
