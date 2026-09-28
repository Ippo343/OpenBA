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
// (set_core.cpp) and the LinkedList pair (linkedlist.cpp). They all ask THIS
// file which function to call.
//
// B3R W0-A0: THE ANSWER IS THE ALLOCATOR'S OWN STATICS, DIRECTLY.
// ---------------------------------------------------------------------------
// Until W0-A0 the door read the exe `.rdata` dispatch slots 0x140152b60 /
// 0x140152b68 on every call (falling back to ResolveAddr(Runtime, 0x1800c7a50 /
// 0x1800c7ad0)), because `standalone/redzone_alloc.cpp` armed BANISHED_REDZONE
// by rewriting those cells, and a door that cached the pointer would have put
// the containers outside the instrument. The instruments now sit on the
// allocator's ONE instrument seat, `System::g_memoryAllocatorInstrument`,
// consulted at the entry of `System::MemoryAllocator::MemAlloc` / `MemFree`
// (runtime/types/system/MemoryAllocator.h). So there is no cell to re-read:
// the door answers the two statics, and every container allocation reaches
// whichever instrument is armed through the statics themselves — including one
// armed after the containers first allocated, which is what the per-call slot
// read used to guarantee (tests/unit/alloc_door_test.cpp pins that fact now).
//
// WHAT IS UNCHANGED
// -----------------
//   * The allocator that runs: `?MemAlloc@` / `?MemFree@` (the slots, the name
//     door and the two runtime addresses were all bound to these two statics).
//   * The alignment argument: `alignof(T)` (Array), 8 (Set and LinkedList
//     nodes). Collection.h's DefaultAllocator banner is the evidence.
//   * Each caller's own null handling. This file still hands back a FUNCTION
//     POINTER rather than performing the call; it is never null now.
//
// PORTABILITY: no <windows.h> (PLAN.md §4.1 decision 1).
// ============================================================================

#include <cstdint>

#include "../../types/collection/Collection.h"
#include "../../types/system/MemoryAllocator.h"

namespace rtcoll {

DoorAllocFn_t DoorAlloc() {
    return &System::MemoryAllocator::MemAlloc;   // ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z
}

DoorFreeFn_t DoorFree() {
    return &System::MemoryAllocator::MemFree;    // ?MemFree@MemoryAllocator@System@@SAXPEAX@Z
}

}  // namespace rtcoll
