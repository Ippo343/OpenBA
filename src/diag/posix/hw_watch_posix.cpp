// ============================================================================
// src/diag/posix/hw_watch_posix.cpp -- the hardware-watchpoint instrument's POSIX twin
// ============================================================================
// Phase D.4 LINUX-1 (D4_RULES C10). src/diag/win/hw_watch.cpp arms the x86 debug
// registers DR0-DR3 on the main thread (SetThreadContext) and logs each hit from the
// crash handler's first-chance single-step. Linux gives a process no user-mode write
// to its own debug registers (they are set through ptrace or perf_event_open from
// outside), so the twin arms nothing: HwWatch_Arm returns -1 ("no slot") and says
// so on stderr, and nothing is ever armed.
// ============================================================================

#include "hw_watch.h"

#include <cstdio>

extern "C" void HwWatch_Init() {}

extern "C" int HwWatch_Arm(void* addr, int len, const char* why) {
    std::fprintf(stderr, "[HwWatch] not armed (%s, %p len %d): hardware watchpoints are "
                         "Windows-only (src/diag/win/hw_watch.cpp)\n",
                 why ? why : "?", addr, len);
    std::fflush(stderr);
    return -1;
}

extern "C" int HwWatch_Armed() { return 0; }

extern "C" int HwWatch_OnSingleStep(void*, void (*)(void*)) { return 0; }

void* HwWatch_MainThreadHandle() { return nullptr; }
