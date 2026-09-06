#pragma once
// ============================================================================
// runtime/types/system/Thread.h — System::CriticalSection / System::Thread
// ============================================================================
// Binary-compatible layouts recovered from Runtime-x64.dll (retail addresses
// in the comments). Platform-neutral by design (PLAN.md §4.1 decision 1): this
// header must never include <windows.h>. Windows-only storage is represented
// as opaque, correctly-sized/aligned bytes with static_asserts, and HANDLEs are
// spelled `void*` (their real Win32 type, no header needed) so a `_posix` twin
// can later give this same layout POSIX-native contents. The Win32-specific
// code that interprets these fields lives in runtime/src/system/thread_win.cpp.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace System {

// ----------------------------------------------------------------------------
// System::CriticalSection
// ----------------------------------------------------------------------------
// Retail (0x18000c5c0, ??0CriticalSection@System@@QEAA@XZ):
//   InitializeCriticalSection(this);
//   SetCriticalSectionSpinCount(this, 5000);
// i.e. the object IS a Win32 CRITICAL_SECTION, spin count 5000. Represented
// here as opaque storage sized/aligned exactly like CRITICAL_SECTION on x64
// (40 bytes: DebugInfo ptr, LockCount, RecursionCount, OwningThread,
// LockSemaphore, SpinCount — all 8-byte-aligned). A _posix twin would replace
// this storage with e.g. a pthread_mutex_t of matching size/alignment; the
// class has no vtable, so there is no ABI concern beyond size and alignment.
struct CriticalSection {
    alignas(8) uint8_t _storage[0x28];   // opaque Win32 CRITICAL_SECTION
};
static_assert(sizeof(CriticalSection) == 0x28,
              "CriticalSection must be exactly sizeof(CRITICAL_SECTION) on x64");

// ----------------------------------------------------------------------------
// System::Thread
// ----------------------------------------------------------------------------
// Retail is ALWAYS subclassed: the base vtable's slot 0 is _purecall
// (0x18007ec28) and only slot 1 (OnCompleted, 0x18000c590) has a real base
// body (a bare RET). A derived class supplies its own vtable and stamps it
// over this one right after the base ctor runs (see code/Game/Game/
// pathfinder_seed.cpp and keystone_a_gamedtor.cpp for two such subclasses).
//
// Layout, from 0x1800ca4e0 (ctor), 0x1800ca590 (dtor), 0x1800ca610 (Start),
// 0x1800ca5f0 (WaitForFinish), 0x1800ca440 (ThreadFunc):
//   +0x00 vtable        System::Thread's own vtable (??_7Thread@System@@6B@,
//                        a real PE export at 0x180155f20) until a subclass
//                        ctor overwrites it.
//   +0x08 runFlags      uint32; bit0 set for the duration of the work call
//                        inside ThreadFunc (cleared again right after).
//   +0x0c (padding)     implicit — the next field is 8-byte aligned.
//   +0x10 hThread       HANDLE from _beginthreadex.
//   +0x18 hStart        HANDLE, auto-reset, CreateEventW(NULL,FALSE,FALSE,NULL)
//                        — Start() SetEvents this to wake the worker.
//   +0x20 hStop         HANDLE, auto-reset, CreateEventW(NULL,FALSE,FALSE,NULL)
//                        — the dtor SetEvents this to wake ThreadFunc for
//                        shutdown (ThreadFunc waits on {hStart, hStop} and the
//                        hStop slot, index 1, is the exit branch).
//   +0x28 hFinished     HANDLE, MANUAL-reset, initially SIGNALED
//                        (CreateEventW(NULL,TRUE,TRUE,NULL)) — WaitForFinish()
//                        blocks on this; Start() ResetEvents it before waking
//                        hStart, so a fresh WaitForFinish() blocks until the
//                        worker's next pass sets it again.
// Total base size 0x30. A derived class's own fields start at +0x30 (e.g. the
// worker-queue Thread in keystone_a_gamedtor.cpp embeds a CriticalSection
// there; the pathfinder's Thread subclass is 0x1f8 bytes total).
struct Thread {
    void*    vtable;      // +0x00
    uint32_t runFlags;    // +0x08
    uint32_t _pad0c;      // +0x0c
    void*    hThread;     // +0x10  HANDLE
    void*    hStart;      // +0x18  HANDLE
    void*    hStop;       // +0x20  HANDLE
    void*    hFinished;   // +0x28  HANDLE
};
static_assert(offsetof(Thread, vtable)    == 0x00, "Thread::vtable offset");
static_assert(offsetof(Thread, runFlags)  == 0x08, "Thread::runFlags offset");
static_assert(offsetof(Thread, hThread)   == 0x10, "Thread::hThread offset");
static_assert(offsetof(Thread, hStart)    == 0x18, "Thread::hStart offset");
static_assert(offsetof(Thread, hStop)     == 0x20, "Thread::hStop offset");
static_assert(offsetof(Thread, hFinished) == 0x28, "Thread::hFinished offset");
static_assert(sizeof(Thread) == 0x30, "Thread base size");

}  // namespace System
