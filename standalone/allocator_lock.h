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

#ifdef STANDALONE_EXE

// Returns true if the detour was installed (or intentionally skipped via env).
bool InstallAllocatorLock();

// Detours the raw System::MemoryAllocator::Free (0x1800c6c00) — which mutates
// the free-list with no lock — so it takes the same critical section, closing
// the Allocate-vs-raw-Free race. Call after InstallAllocatorLock. Gated by
// BANISHED_ALLOC_LOCK (shared) and BANISHED_FREE_LOCK (free-lock-specific).
bool InstallFreeLock();

#endif // STANDALONE_EXE
