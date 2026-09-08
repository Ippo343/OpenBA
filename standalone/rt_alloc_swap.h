// standalone/rt_alloc_swap.h — host side of the MemoryAllocator atomic swap
// (docs/runtime_port/WAVE2_ALLOCATOR.md §4/§6, gate steps 2 and 3).
//
// Two independent, both default-OFF facilities live in rt_alloc_swap.cpp:
//
//   1. The ATOMIC SWAP installer (BANISHED_RT_ALLOC=1). Detours the DLL's
//      Allocate/Free onto composed stubs that take the allocator critical
//      section, carry the allocator_lock diagnostics, and then run OUR ported
//      body. RtSystemAllocatorRegister() registers nothing at all if the
//      installer returns false, which is the atomicity guarantee.
//
//   2. The OBSERVE-ONLY SHADOW DRIVER
//      (BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1). Runs our port into a
//      private replica arena alongside retail's real work and compares, WITHOUT
//      taking the heap. This is the measurement that has to come back clean
//      before anyone flips (1) on.
//
// The two are mutually exclusive by construction: arming the shadow leaves
// allocator_lock's patches in place, and RtSystemAllocatorRegister() hard-
// refuses while those patches are there.

#pragma once

#include <cstdint>

// Hand this to RtSystemAllocatorSetDetourInstaller() BEFORE RuntimePortInit().
// Signature matches RtAllocDetourInstaller in runtime/types/system/MemoryAllocator.h.
extern "C" bool RtAllocSwapInstall(void* ourAllocate, void* ourFree);

// Call once, immediately AFTER RuntimePortInit(). No-op unless the swap took.
// Repoints the ?Allocate@/?Free@ export registrations at the composed stubs so
// OUR call sites get the same critical section the DLL's detoured callers get.
// See WAVE9_LANE_E.md "Correction 3".
extern "C" void RtAllocSwapPostRegister();

// Observe-only shadow driver. Call BEFORE RuntimePortInit() (it must be in
// place before MemoryAllocator::Startup runs, which happens much later, inside
// Engine::Startup). No-op unless BANISHED_RT_DIFF and BANISHED_RT_ALLOC_SHADOW
// are both set. Returns true if it armed.
extern "C" bool RtAllocShadowInstall();

// One-line summary of the shadow run; call next to RtDiffSummary() at exit.
extern "C" void RtAllocShadowSummary();
