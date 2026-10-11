// ============================================================================
// src/diag/posix/redzone_alloc_posix.cpp -- the redzone allocator's POSIX twin
// ============================================================================
// Phase D.4 LINUX-1 (D4_RULES C10). src/diag/win/redzone_alloc.cpp is the
// BANISHED_REDZONE heap instrument: a tail-canary shim over the engine allocator,
// a watchdog thread that walks the boundary tags under the allocator's lock, and
// the heap dump the crash handler prints. It is built on Win32 (CreateThread,
// VirtualQuery probes, CaptureStackBackTrace, the toolhelp snapshot) and has no
// POSIX body. This twin keeps every entry point the portable tree calls, inert:
//
//   * RedzoneAlloc_Install says, once, that the instrument is Windows-only when
//     BANISHED_REDZONE asked for it (Opt().redzone), so a Linux run that wanted it
//     does not silently run without it;
//   * every check reports "clean" (0 / null), which is what the Windows bodies
//     return when the instrument is off -- their callers' "no shim" path;
//   * RedzoneAlloc_HeapDump (the crash handler's heap walk) writes one line saying
//     the walk is unavailable, through the sink when one is set.
// ============================================================================

#include "redzone_alloc.h"
#include "options.h"

#include <cstdio>

namespace {
void (*g_sink)(const char*) = nullptr;
}  // namespace

void RedzoneAlloc_Install() {
    if (Opt().redzone != nullptr) {
        std::fprintf(stderr, "[Redzone] BANISHED_REDZONE=%s requested, but the redzone "
                             "allocator is Windows-only (src/diag/win/redzone_alloc.cpp): "
                             "NOT installed on this platform\n", Opt().redzone);
        std::fflush(stderr);
    }
}

bool RedzoneAlloc_Enabled() { return false; }

int RedzoneAlloc_ValidateAll(const char*) { return 0; }

int RedzoneAlloc_HeapCheck(const char*) { return 0; }

extern "C" void RedzoneAlloc_HeapDump(const char* tag) {
    char line[160];
    std::snprintf(line, sizeof(line), "[HeapDump %s] the engine-heap walk is Windows-only "
                                      "(src/diag/win/redzone_alloc.cpp); not available here\n",
                  tag ? tag : "?");
    if (g_sink != nullptr) g_sink(line);
    else std::fputs(line, stderr);
}

extern "C" void* RedzoneAlloc_FindCorruptBlockNative() { return nullptr; }

extern "C" void RedzoneAlloc_ReportWildWrite(void*) {}

extern "C" void RedzoneAlloc_PrintBacktrace(const char*) {}

extern "C" int RedzoneAlloc_CheckLinks(const char*) { return 0; }

extern "C" void RedzoneAlloc_SetDumpSink(void (*sink)(const char*)) { g_sink = sink; }

// The teardown heap check (gameupdateloop.cpp, alloc_cutover_arm.cpp): armed only by
// BANISHED_REDZONE=watch + BANISHED_TEARDOWN_HEAPCHECK on Windows; nothing to arm here.
extern "C" void RedzoneAlloc_ArmTeardownCheck() {}

extern "C" void RedzoneAlloc_TeardownOp(const char*) {}
