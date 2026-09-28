// ============================================================================
// runtime/src/collection/set_core.cpp — ENDGAME B.3.2 step 1 (SET core)
//   The out-of-line half of `Collection::Set<T, Compare, Allocator, Key>`:
//   the two allocator doors, retail's inlined `strcmp`, and one forwarder per
//   ported AVL core.
// ============================================================================
//
// WHY THE TEMPLATE DOES NOT CONTAIN THESE
// ---------------------------------------
// `runtime/types/collection/Collection.h` is a TYPES header: no <windows.h>, no
// registry, no knowledge of the three ported node types (PLAN.md §4.1 decision
// 1). Everything in here needs one of those three things, so the template
// declares and this file defines.
//
// WHAT IS AND IS NOT NEW CODE
// ---------------------------
// Nothing here is a new AVL body. Every function below is either an address
// door that already existed in three copies (`MemAlloc` 0x1800c7a50,
// `SetNodeFree` 0x180009c30) or a one-line forward into a body that was ported
// in Wave 9 and is still registered at its retail address:
//
//   element 0x10 (node 0x30)  rtsetavl::   set_avl.cpp      0x18000adc0 / b040 / b170 / aba0
//   element 0x18 (node 0x38)  rtsetavl24:: set_avl_e24.cpp  0x180023170 / 23570 / 236a0
//   element 0x30 (node 0x50)  rtavl::      avl_trees.cpp    0x180030540 / 307b0 / 308e0
//
// THE THREE NODE LAYOUTS STAY DISTINCT — that is the whole point of having
// three forwarders instead of one (`reference_one_bst_find_two_node_layouts.md`:
// one walk over two layouts is right for a key that happens to be the root and
// permanently wrong for the next one). The forwarders exist so the TEMPLATE can
// pick the right one from `sizeof(T)` at compile time rather than a caller
// picking it by hand.
//
// The `void*` at each boundary is the transitional cast ENDGAME §B.4 allows for
// a resolved-body seam; it disappears when the ported bodies themselves become
// members (step 2).
//
// PORTABILITY: no <windows.h>.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../types/collection/Collection.h"
#include "set_avl.h"
#include "set_avl_e24.h"
#include "avl_trees.h"
#include "../../types/system/MemoryAllocator.h"   // B3R: MemAlloc / MemFree by name

namespace Collection {
namespace detail {

// ---------------------------------------------------------------------------
// The allocator boundary, retail 0x1800c7a50
// (`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`).
//
// Every retail Set node allocator INLINES `EnterCriticalSection(_singleton +
// 0x640); Allocate(_singleton, bytes, align); LeaveCriticalSection(...)`, which
// IS that function — the reading avl_trees.cpp:165, set_avl_e24.cpp's MemAlloc
// and array.cpp:159 all already make. Going through the one door keeps the heap
// from being half ours.
//
// CACHE A HIT, RETRY A MISS (the RtResolveCached contract). Freezing a null
// would make every later insert return nullptr for the life of the process —
// the ordering defect Wave 9 attempt 14 fixed for `rtcoll::MemAlloc`.
// ---------------------------------------------------------------------------
//
// ENDGAME B.3.2 closing pass: the resolution moved into `rtcoll::DoorAlloc`
// (runtime/src/collection/alloc_door.cpp) so this seam reaches the allocator
// through the exe dispatch slot 0x140152b60 — the door the rest of the tree
// uses and the one `BANISHED_REDZONE` rewrites — falling back to the runtime
// address above. B3R RT-Core: the static, called by name.
void* SetNodeAlloc(uint64_t bytes, int align) {
    return System::MemoryAllocator::MemAlloc(bytes, align);   // 0x1800c7a50
}

// ---------------------------------------------------------------------------
// The node free, retail 0x180009c30 — the allocator-lock Free wrapper.
//
// *** IT TAKES TWO PARAMETERS AND IGNORES THE FIRST. *** Registering a
// one-argument `MemFree` there would free the ignored `this` and corrupt the
// free list; `runtime/src/system/allocator.cpp` is the single owner of that
// registration (set_avl.cpp's "THE FIFTH HELPER" banner records the three-way
// shadowed registration this rule came out of). Reached here through
// `rtsetavl::SetNodeFree`, which already speaks that contract, with the ignored
// first argument passed as null — retail passes whatever the register held.
// ---------------------------------------------------------------------------
void SetNodeFree(void* node) {
    rtsetavl::SetNodeFree(nullptr, node);
}

// ---------------------------------------------------------------------------
// `Collection::AVLCompare` for a C-string key: MSVC's inlined `strcmp` at
// retail 0x18002a1c4, transcribed from the flags.
//
// NOT `std::strcmp`: the standard compares as `unsigned char` and this one
// finishes with a SIGNED char subtraction (`MOVSX`, not `MOVZX`). The two
// disagree in SIGN on any byte >= 0x80, which is the difference between a
// subtree and its sibling. This is the same body avl_trees.cpp's group-B
// `StrCmpSigned` carries; that copy stays where it is because it also feeds the
// group's probe counters.
// ---------------------------------------------------------------------------
int AvlStrCmpSigned(const char* a, const char* b) {
    unsigned char c = static_cast<unsigned char>(a[0]);
    int64_t i = 0;
    if (c == static_cast<unsigned char>(b[0])) {
        for (;;) {
            if (c == 0) break;                                   // TEST CL,CL
            if (b[i] == 0) break;                                // CMP byte [RAX],0
            c = static_cast<unsigned char>(a[i + 1]);
            ++i;
            if (c != static_cast<unsigned char>(b[i])) break;    // CMP CL,[RAX]
        }
    }
    return static_cast<int>(static_cast<signed char>(a[i])) -
           static_cast<int>(static_cast<signed char>(b[i]));
}

// ---------------------------------------------------------------------------
// element 0x10 / node 0x30 — set_avl.cpp (Wave 9 lane VID2).
// ---------------------------------------------------------------------------
void PortedRetrace16(void* set, void** slot) {              // 0x18000adc0
    rtsetavl::Retrace(static_cast<rtsetavl::Set*>(set),
                      reinterpret_cast<rtsetavl::Node**>(slot));
}
void* PortedRotateLeft16(void* set, void* n) {              // 0x18000b040
    return rtsetavl::RotateLeft(static_cast<rtsetavl::Set*>(set),
                                static_cast<rtsetavl::Node*>(n));
}
void* PortedRotateRight16(void* set, void* n) {             // 0x18000b170
    return rtsetavl::RotateRight(static_cast<rtsetavl::Set*>(set),
                                 static_cast<rtsetavl::Node*>(n));
}
void PortedErase16(void* set, void* z) {                    // 0x18000aba0
    rtsetavl::Erase(static_cast<rtsetavl::Set*>(set), static_cast<rtsetavl::Node*>(z));
}

// ---------------------------------------------------------------------------
// element 0x18 / node 0x38 — set_avl_e24.cpp (Wave 9 lane INPUT). There is no
// erase at this width in either shipped build (set_avl_e24.h's table says
// "erase (no door)"), so `AvlOps<…,0x18>` keeps the template's generic erase,
// which is the same algorithm.
// ---------------------------------------------------------------------------
void PortedRetrace24(void* set, void** slot) {              // 0x180023170
    rtsetavl24::Retrace(static_cast<rtsetavl24::Set*>(set),
                        reinterpret_cast<rtsetavl24::Node**>(slot));
}
void* PortedRotateLeft24(void* set, void* n) {              // 0x180023570
    return rtsetavl24::RotateLeft(static_cast<rtsetavl24::Set*>(set),
                                  static_cast<rtsetavl24::Node*>(n));
}
void* PortedRotateRight24(void* set, void* n) {             // 0x1800236a0
    return rtsetavl24::RotateRight(static_cast<rtsetavl24::Set*>(set),
                                   static_cast<rtsetavl24::Node*>(n));
}

// ---------------------------------------------------------------------------
// element 0x30 / node 0x50 — avl_trees.cpp group A (Object::Factory's type
// tree). Its container header is `RtObject::Factory`, which is the same
// {root, count} pair every other Set has; the cast is the same transitional
// seam as above.
// ---------------------------------------------------------------------------
void PortedRetrace48(void* set, void** slot) {              // 0x180030540
    rtavl::Retrace50(reinterpret_cast<RtObject::Factory*>(set),
                     reinterpret_cast<RtObject::TypeNode**>(slot));
}
void* PortedRotateLeft48(void* set, void* n) {              // 0x1800307b0
    return rtavl::RotateLeft50(reinterpret_cast<RtObject::Factory*>(set),
                               reinterpret_cast<RtObject::TypeNode*>(n));
}
void* PortedRotateRight48(void* set, void* n) {             // 0x1800308e0
    return rtavl::RotateRight50(reinterpret_cast<RtObject::Factory*>(set),
                                reinterpret_cast<RtObject::TypeNode*>(n));
}

}  // namespace detail
}  // namespace Collection
