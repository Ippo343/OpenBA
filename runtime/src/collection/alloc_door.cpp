// ============================================================================
// runtime/src/collection/alloc_door.cpp — ENDGAME B.3.2 closing pass
//   ONE allocator door for every container: `Collection::Array<T>`,
//   `Collection::Set<T,…>`, `Collection::LinkedList<T,…>` and the ported
//   collection cores they wrap.
// ============================================================================
//
// WHY THIS FILE EXISTS
// --------------------
// Four seams used to resolve the allocator independently and identically —
// `rtcoll::MemAlloc/MemFree` (array.cpp), `rtavl::MemAlloc/MemFree`
// (avl_trees.cpp), `Collection::detail::SetNodeAlloc/SetNodeFree`
// (set_core.cpp) and the LinkedList pair (linkedlist.cpp) — each caching
// `ResolveAddr(Module::Runtime, 0x1800c7a50 / 0x1800c7ad0)` in a static.
//
// That reaches the RIGHT allocator, but it reaches it through the wrong DOOR.
// `standalone/redzone_alloc.cpp` arms `BANISHED_REDZONE` by (a) swapping the
// `fn_MemAlloc` / `fn_MemFreeStatic` globals and (b) REWRITING the pointer
// stored in the two exe-side `.rdata` dispatch slots
//
//     0x140152b60  ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z  (size, align)
//     0x140152b68  ?MemFree@MemoryAllocator@System@@SAXPEAX@Z     (ptr)
//
// which is the door every other allocating body in the tree goes through
// (code/Game/Component/keystone_a_restree.cpp, tool_base_highlight*.cpp, the
// work_act_*.cpp family, …). A body that resolves the RUNTIME address instead
// gets the raw `rtalloc::MemAlloc` and is invisible to the instrument: the
// containers were the single largest allocator in the process and none of their
// blocks was ever tracked. Worse than invisible, in fact — redzone_alloc.cpp's
// own `ShimAllocStatic` banner records what a HALF-shimmed heap costs: an alloc
// through one channel and a free through the other leaves a stale `g_live`
// entry and the canary scan then reports a PHANTOM overflow ("corrupt decal
// vertex pool", twice).
//
// So the containers now ask THIS file which function to call, and this file
// re-reads the slot every time.
//
// WHY THE SLOT IS DEREFERENCED PER CALL AND NOT CACHED
// ---------------------------------------------------
// `RedzoneAlloc_Install()` overwrites the slot's CONTENTS at boot, after
// `EngineFuncsInit()`. Caching the function the slot held at first use would
// pin whatever was there before the install and quietly put the containers back
// outside the instrument — the same "snapshot too early" defect redzone's own
// comment relies on consumers NOT having. The slot ADDRESS is stable once
// registered, so that is what is cached; the load through it is one extra
// dependent load on a path that is about to take a critical section.
//
// WHAT IS UNCHANGED
// -----------------
//   * The allocator that ultimately runs: `?MemAlloc@`/`?MemFree@` either way.
//     The dispatch slots are registered (standalone/address_registry.cpp) with
//     `ResolveExport` of those same two mangled names, which
//     `runtime/src/system/allocator.cpp` binds to `rtalloc::MemAlloc/MemFree`,
//     and `RegisterAllocatorAddresses()` binds 0x1800c7a50/0x1800c7ad0 to the
//     very same pair. One allocator, two names for its front door.
//   * The alignment argument: `alignof(T)` (Array), 8 (Set and LinkedList
//     nodes). Collection.h's DefaultAllocator banner is the evidence.
//   * "Cache a hit, RETRY a miss" (the RtResolveCached contract). Freezing a
//     null would make every later allocation fail for the life of the process —
//     the ordering defect Wave 9 attempt 14 fixed for `rtcoll::MemAlloc`, and it
//     is preserved here for BOTH the slot and the fallback.
//   * Each caller's own null handling. This file hands back a FUNCTION POINTER
//     rather than performing the call, so `rtcoll::MemAlloc` keeps calling
//     unguarded (a faithful crash naming an unarmed allocator) while the Set and
//     LinkedList seams keep their `fn ? fn(…) : nullptr` shape. Changing either
//     would be a behaviour change dressed up as a refactor.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>

#include "../../types/collection/Collection.h"

// Declared locally rather than by including standalone/resolve.h, which drags
// in <windows.h> (whose <wingdi.h> declares a function called `Rectangle`).
// Byte-identical to array.cpp's, set_core.cpp's and avl_trees.cpp's copies; the
// linker checks they agree.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtcoll {

namespace {

// The exe `.rdata` dispatch slot, resolved once (hit only) and read every call.
inline void** Slot(uintptr_t exeAddr, void*** cache) {
    if (*cache == nullptr) {
        const uintptr_t s = ResolveAddr(Module::Exe, exeAddr);
        if (s != 0) *cache = reinterpret_cast<void**>(s);
    }
    return *cache;
}

}  // namespace

DoorAllocFn_t DoorAlloc() {
    static void**       slot = nullptr;
    static DoorAllocFn_t fallback = nullptr;

    if (void** const s = Slot(0x140152b60, &slot)) {
        if (*s != nullptr) return reinterpret_cast<DoorAllocFn_t>(*s);
    }
    if (fallback == nullptr) {
        fallback = reinterpret_cast<DoorAllocFn_t>(
            ResolveAddr(Module::Runtime, 0x1800c7a50));
    }
    return fallback;
}

DoorFreeFn_t DoorFree() {
    static void**      slot = nullptr;
    static DoorFreeFn_t fallback = nullptr;

    if (void** const s = Slot(0x140152b68, &slot)) {
        if (*s != nullptr) return reinterpret_cast<DoorFreeFn_t>(*s);
    }
    if (fallback == nullptr) {
        fallback = reinterpret_cast<DoorFreeFn_t>(
            ResolveAddr(Module::Runtime, 0x1800c7ad0));
    }
    return fallback;
}

}  // namespace rtcoll
