// standalone/allocator_lock.h
// Make the Runtime engine allocator thread-safe in the standalone build.
//
// System::MemoryAllocator::Allocate (Runtime 0x1800c6e60) mutates a boundary-tag
// free-list with NO synchronization, while Free/MemFree take the allocator's
// critical section (_singleton+0x640). When two threads call Allocate (or
// Allocate vs Free) concurrently the free-list links get clobbered -> the next
// Allocate follows a wild pointer -> access violation / abort (the intermittent
// "menu heap crash"). This is reachable in the standalone whenever the engine
// background Loader thread loads resources (achievements/externals) while the
// main thread builds the menu scene.
//
// InstallAllocatorLock() detours Allocate so it takes the SAME critical section
// Free uses, making Allocate/Free mutually exclusive. Gated by env var
// BANISHED_ALLOC_LOCK (default ON; set to 0 to disable for A/B testing).

#pragma once


// Returns true if the detour was installed (or intentionally skipped via env).
bool InstallAllocatorLock();

// Detours the raw System::MemoryAllocator::Free (0x1800c6c00) — which mutates
// the free-list with no lock — so it takes the same critical section, closing
// the Allocate-vs-raw-Free race. Call after InstallAllocatorLock. Gated by
// BANISHED_ALLOC_LOCK (shared) and BANISHED_FREE_LOCK (free-lock-specific).
bool InstallFreeLock();

// Remove both prologue patches, handing MemoryAllocator::Allocate and ::Free
// back to retail's own bytes. Returns true if anything was actually retired.
//
// This exists for ONE caller: the ported allocator's atomic-swap installer
// (standalone/rt_alloc_swap.cpp), which detours the same two prologues. The
// lock and the swap must never coexist -- whichever patches second either
// clobbers the first's saved bytes or is refused by its guard, and both
// outcomes end with an UNLOCKED free list (docs/runtime_port/WAVE2_ALLOCATOR.md
// section 4). The swap's composed stubs take the same critical section, so
// nothing is lost by retiring.
//
// Boot-time only: it does not synchronise with threads that may be inside the
// wrappers. Call it before Engine::Startup creates the loader thread.
bool AllocatorLockRetireForPort();

// Put both patches BACK after a retire that did not lead to an armed port.
//
// Wave 10 lane ALLOC. RtSystemAllocatorRegister() now retires the lock BEFORE
// its prologue guard runs (that ordering is the whole reason the ported allocator
// could never arm with the DLL present). Every refusal downstream of that retire
// -- a foreign patch on the prologues, a detour that would not install -- would
// otherwise leave the process running retail's own UNLOCKED Allocate against a
// Free that no longer takes the critical section: the free-list race, reached by
// way of a SAFETY guard. This calls InstallAllocatorLock() + InstallFreeLock()
// again; both re-validate retail's bytes, which the retire has just restored.
// Returns true only if both re-installed.
bool AllocatorLockRestoreAfterRefusal();
