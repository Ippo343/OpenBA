// ============================================================================
// runtime/src/engine/singleton_ctors_win.cpp — Wave 9 lane X, platform half
// ============================================================================
// PLAN.md §4.1 decision 1 keeps every non-`_win` file under runtime/src/ free
// of <windows.h>. Two of this lane's fourteen constructors
// (System::MemoryAllocator — already owned by allocator.cpp — and
// Video::Device) initialise a CRITICAL_SECTION inline, so the lane needs the
// same two Win32 calls the retail bodies make and nothing else:
//
//   ??0Device@Video@@QEAA@W4DeviceType@01@@Z  0x1800f7160
//       ADD  RCX,0x60
//       CALL qword ptr [0x18014b380]        ; InitializeCriticalSection
//       MOV  EDX,0x1388                     ; 5000
//       LEA  RCX,[RSI+0x60]
//       CALL qword ptr [0x18014b208]        ; SetCriticalSectionSpinCount
//
// The spin count is the literal 0x1388 out of the disassembly, not a guess, and
// it is the same value MemoryAllocator's ctor passes (allocator_win.cpp).
// ============================================================================

#include "../../types/system/MemoryAllocator.h"

#include <windows.h>

// ---------------------------------------------------------------------------
// The allocator platform seam, for the probe's MemoryAllocator subject only.
//
// `rtalloc::Construct` (runtime/src/system/allocator.cpp:635) opens with
// `g_plat.lockInit(self->lock)`. `g_plat` is a table of function pointers that
// `RtSystemAllocatorInstallPlatform()` fills, and on a normal run NOBODY calls
// it: its only two callers are `RtAllocShadowStartup()` and
// `RtSystemAllocatorRegister()`, both opt-in and both disarmed. So the table is
// all-null and that first call is an EXEC of 0 -- which is exactly what the
// first version of this lane's probe hit.
//
// Installing the seam is NOT arming the allocator port. It fills `g_plat` and
// does nothing else: no export is registered, no detour is installed, and
// `RtSystemAllocatorRegister()` still refuses on its own separate grounds. The
// two functions that consume the seam re-install it when it is absent, so doing
// it early is a no-op for them, and `g_platInstalled` is read nowhere else.
//
// `g_plat` is extern-declared here the same way runtime/src/system/
// allocator_win.cpp:40 declares it -- the precedent this file mirrors.
// ---------------------------------------------------------------------------
extern rt::system::RtSysMemPlatform g_plat;

bool RtLaneXEnsureAllocatorPlatform() {
    if (!g_plat.lockInit) RtSystemAllocatorInstallPlatform();
    // Report the seam the caller is about to USE, not a separate `installed`
    // flag: the whole failure being fixed here was a null lockInit.
    return g_plat.lockInit != nullptr;
}

// Defined here, declared in singleton_ctors.cpp. Not a general seam: this lane
// is the only caller and the two calls are one retail body's, verbatim.
void RtLaneXPlatformLockInit(void* cs) {
    InitializeCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
    SetCriticalSectionSpinCount(static_cast<CRITICAL_SECTION*>(cs), 0x1388);
}

