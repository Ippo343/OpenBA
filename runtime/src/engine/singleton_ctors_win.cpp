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

#include <windows.h>

// Defined here, declared in singleton_ctors.cpp. Not a general seam: this lane
// is the only caller and the two calls are one retail body's, verbatim.
void RtLaneXPlatformLockInit(void* cs) {
    InitializeCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
    SetCriticalSectionSpinCount(static_cast<CRITICAL_SECTION*>(cs), 0x1388);
}

void RtLaneXPlatformLockDelete(void* cs) {
    DeleteCriticalSection(static_cast<CRITICAL_SECTION*>(cs));
}
