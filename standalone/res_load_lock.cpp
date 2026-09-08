// ============================================================================
// standalone/res_load_lock.cpp — the resource-loader serialize lock
//                                (Wave 9 lane FNHOOK)
// ============================================================================
//
// Rationale, measurements and the rule that a ported FindOrLoad MUST take this
// in its own body: standalone/res_load_lock.h. Read that first.
//
// INITIALIZATION. `InitializeCriticalSection` on a recursive CS, run exactly
// once, from whichever thread gets here first. The old code initialized inside
// `InstallFindOrLoadWatch()`, which is a place that does not necessarily run —
// and at cutover definitely does not do anything useful. A ported body taking
// an uninitialized CRITICAL_SECTION is undefined behaviour that would present
// as a random hang or a corrupt wait list, i.e. exactly the failure mode the
// lock exists to remove, so the init cannot be anybody's install-time
// side effect.
//
// `InitOnceExecuteOnce` is used rather than a function-local `static` because
// this may be entered from a Runtime-created loader thread, and MinGW's
// thread-safe-statics guard pulls in the GCC runtime's own once machinery on a
// thread whose stack and TLS were not set up by our CRT (PLAN.md §4.3 risk 5,
// "keep C++ exceptions and CRT-dependent machinery out of detoured code").
// The Win32 primitive has no such dependency.
// ============================================================================

#include "res_load_lock.h"

#include <windows.h>
#include <cstdlib>
#include <cstdio>

namespace {

CRITICAL_SECTION g_resCS;
INIT_ONCE        g_resOnce = INIT_ONCE_STATIC_INIT;
bool             g_armed   = true;   // valid only after the once-init has run

BOOL CALLBACK ResLoadInitOnce(PINIT_ONCE, PVOID, PVOID*) {
    // Read the kill switch ONCE and cache it. Reading it per call would let a
    // mid-run _putenv_s turn the lock off between an acquire and its release —
    // a leaked lock rather than an A/B.
    const char* noLock = getenv("BANISHED_NO_FOL_LOCK");
    g_armed = !(noLock && noLock[0] && noLock[0] != '0');
    if (g_armed) {
        // Spin count 0: contention here is a real load on another thread doing
        // file I/O, so spinning burns a core for nothing.
        InitializeCriticalSection(&g_resCS);
    } else {
        fprintf(stderr, "[ResLoad] BANISHED_NO_FOL_LOCK set — resource loader "
                        "NOT serialized (A/B only; ~33%% menu-crash baseline)\n");
        fflush(stderr);
    }
    return TRUE;
}

inline void EnsureInit() {
    InitOnceExecuteOnce(&g_resOnce, &ResLoadInitOnce, nullptr, nullptr);
}

}  // namespace

void ResLoadLockAcquire() {
    EnsureInit();
    if (g_armed) EnterCriticalSection(&g_resCS);
}

void ResLoadLockRelease() {
    // No EnsureInit: a release without a matching acquire is a bug, and
    // initializing here would hide it behind a Leave on a fresh CS.
    if (g_armed) LeaveCriticalSection(&g_resCS);
}

bool ResLoadLockArmed() {
    EnsureInit();
    return g_armed;
}
