#pragma once
// ============================================================================
// code/Game/Dialog/dialog_array_bridge.h — ENDGAME B.3.2 step 2 (callers)
// ============================================================================
//
// TRANSITIONAL, AND IT HAS A TWIN: code/Game/Component/coll_bridge.h
// (`GameColl::I32`/`U8`/`U16`/`Ptr8`/`Vec3`/`Bytes<N,A>`) is the same
// construction, written by the Component caller batch in the same step. Both
// exist only because §B.2 has not turned the owning structs into real classes
// yet; the B.3.2 closing pass merges them into one header. `DlgElem<N>` here is
// `GameColl::Bytes<N,8>` there, and `DlgI32` is `GameColl::I32`.
//
// Every dialog in code/Game/Dialog owns one or more `Collection::Array<T>`
// fields inside a struct that is NOT yet a real C++ class (§B.2 converts those;
// until then the field is reached at a raw byte offset off `self`). Before
// B.3.2 each of those arrays was grown by a hand-transcribed copy of retail's
// INLINED `Collection::Array<T>::SetSize` — the exe monomorphized the same
// Collection/Array.h template the Runtime DLL did, so every one of those copies
// re-derived the same algorithm: exact-equal early return, capacity trebles on
// a +1 append from a NON-EMPTY array and is exact-fit otherwise, copy the old
// prefix into the new buffer, fill the new tail, free the old buffer.
//
// This header is the seam that lets a caller say that in one line instead:
//
//     // B.3.2 bridge: <Dialog>+0x30 is Collection::Array<{a,b}>, stride 0x10.
//     auto& rows = DlgArray<0x10>(s, 0x30);
//     rows.SetSize(rows.GetSize() + 1, DlgElem<0x10>{{ a, b }});
//
// `Collection::Array<T>::SetSize(n, fill)` calls `rtcoll::SetSizeCore`, which
// IS that algorithm (runtime/src/collection/array.cpp) — the one the B.3.2
// step-1 core wrapped rather than re-derived. The conversion is therefore not
// "a different array", it is the same body reached by name.
//
// WHY A BYTE BLOB AND NOT THE REAL ELEMENT TYPE
// ---------------------------------------------
// The elements these dialogs push are `InternalBasePtr`/`ExternalPtr` handles
// and small PODs whose real C++ types belong to classes §B.2 has not converted
// yet. Retail's exe copies them with a plain N-byte copy — no refcount touch —
// and so did every hand-rolled loop this header replaces. `DlgElem<N>`
// reproduces exactly that: trivially copyable and trivially destructible, so
// the trait-derived spec `rtcoll::SpecFor<DlgElem<N>>()` comes out
// `byteCopy = true, destroy = nullptr`. When §B.2 gives one of these arrays a
// real element type, THAT conversion replaces the blob with it.
//
// THE NSDMI IS LOAD-BEARING — DO NOT "SIMPLIFY" IT AWAY
// ------------------------------------------------------
// `uint64_t w[N/8] = {}` makes the blob NOT trivially default-constructible, so
// `SpecFor` emits `init = &TInit<DlgElem<N>>`, which VALUE-initialises (zeroes)
// the new tail. That is deliberate: every exe-side monomorphised
// `Array<T>::SetSize` ZEROES its new tail (the Component batch's finding, same
// as the exe helpers ported in code/Game/Game/serialize_arrays.cpp), whereas a
// bare trivially-default-constructible T would give retail's Runtime-side
// "leave the tail as the allocation found it" arm instead. Drop the `= {}` and
// every fill-less `SetSize(n)` on a Dialog array silently starts handing out
// uninitialised elements.
//
// The append sites this header was written for all pass a fill, so `init` is
// never reached on them (`SetSizeCore` takes `fill` first) — the NSDMI costs
// them nothing and makes the fill-less resizes correct.
//
// The Component batch spells the same idea `GameColl::Bytes<N,A>` in
// code/Game/Component/coll_bridge.h (not yet merged into this branch). The two
// headers are the same construction and the closing pass merges them; keep the
// zeroing-init property identical on both sides of that merge.
//
// THE ONE DELIBERATE DIFFERENCE FROM THE CODE THIS REPLACES
// ----------------------------------------------------------
// The hand-rolled loops allocated with `fn_MemAlloc(g_pMemAllocSingleton, …)`,
// i.e. `System::MemoryAllocator::Allocate` (0x1800c6e60) — the MEMBER, which
// does NOT take the allocator's critical section. Retail's inlined Array growth
// takes it (`Enter(_singleton+0x640); Allocate(…); Leave`, quoted in
// runtime/types/collection/Collection.h's DefaultAllocator banner), and that is
// what `rtcoll::MemAlloc` (0x1800c7a50) does. So the converted sites now LOCK
// where the hand-rolled ones did not. That is a move toward retail, not away
// from it; the free side is unchanged (`fn_MemFreeStatic` resolves
// `?MemFree@MemoryAllocator@System@@SAXPEAX@Z`, which IS `rtcoll::MemFree`'s
// 0x1800c7ad0).
// ============================================================================

#include "collection/Collection.h"

#include <cstdint>
#include <type_traits>

namespace dlgcoll {

// An N-byte, 8-aligned opaque element. N must be a multiple of 8 — every blob
// stride the Dialog arrays use (8, 0x10, 0x18, 0x20, 0x30) is.
template <int N>
struct Elem {
    static_assert(N % 8 == 0,
                  "Elem is the 8-aligned blob; a 4-byte stride is I32 below");
    uint64_t w[N / 8] = {};   // the NSDMI is load-bearing — see the banner
};

// The 4-byte, 4-aligned element, with the same zeroing init. `int32_t` itself
// would be trivially default-constructible and so would leave the new tail
// uninitialised, which is NOT what the exe's stride-4 SetSize does.
struct I32 {
    int32_t v = 0;
};

// 8 bytes at ALIGN 4 — a {int32, int32/float} pair. Two distinct facts make this
// its own element rather than `Elem<8>`:
//   * retail's helper for this shape passes 4 as MemAlloc's alignment argument,
//     not 8 (HelpUI's history array, FUN_140011d50), and `alignof(T)` is what
//     the template forwards;
//   * that same helper leaves a GROWN tail UNINITIALISED — its copy loop writes
//     only the surviving prefix. So this one deliberately has NO NSDMI: it stays
//     trivially default-constructible, `SpecFor` emits `init == nullptr`, and
//     `SetSize(n)` reproduces retail's empty else arm. Do not "make it
//     consistent" with Elem<N> by adding `= {}`.
struct I32Pair {
    int32_t a;
    int32_t b;
};

}  // namespace dlgcoll

// The element blob for a stride-N array.
template <int N>
using DlgElem = dlgcoll::Elem<N>;

// The array itself, at a raw byte offset inside a struct §B.2 has not yet
// converted. Two spellings because the call sites hold `self` both ways.
template <int N>
inline Collection::Array<DlgElem<N>>& DlgArray(uintptr_t base, int off) {
    return *reinterpret_cast<Collection::Array<DlgElem<N>>*>(base + off);
}
template <int N>
inline Collection::Array<DlgElem<N>>& DlgArray(void* base, int off) {
    return DlgArray<N>(reinterpret_cast<uintptr_t>(base), off);
}

// Same, for a stride-4 array of int32 (retail passes align 4 for these).
using DlgI32 = dlgcoll::I32;
inline Collection::Array<DlgI32>& DlgArrayI32(uintptr_t base, int off) {
    return *reinterpret_cast<Collection::Array<DlgI32>*>(base + off);
}
inline Collection::Array<DlgI32>& DlgArrayI32(void* base, int off) {
    return DlgArrayI32(reinterpret_cast<uintptr_t>(base), off);
}

// The stride-8/align-4 pair (see dlgcoll::I32Pair for why it is not DlgElem<8>).
using DlgI32Pair = dlgcoll::I32Pair;
inline Collection::Array<DlgI32Pair>& DlgArrayI32Pair(uintptr_t base, int off) {
    return *reinterpret_cast<Collection::Array<DlgI32Pair>*>(base + off);
}
inline Collection::Array<DlgI32Pair>& DlgArrayI32Pair(void* base, int off) {
    return DlgArrayI32Pair(reinterpret_cast<uintptr_t>(base), off);
}

static_assert(sizeof(Collection::Array<DlgElem<0x10>>) == 0x10,
              "the container header is 16 bytes whatever the element is");
static_assert(sizeof(DlgElem<0x10>) == 0x10 && alignof(DlgElem<0x10>) == 8,
              "retail passes stride N and align 8 for every blob element");
static_assert(sizeof(DlgI32) == 4 && alignof(DlgI32) == 4,
              "and align 4 for the int32 arrays (Collection.h, Input::KeyMap::GetIdList)");
static_assert(std::is_trivially_copyable<DlgElem<0x10>>::value &&
              std::is_trivially_destructible<DlgElem<0x10>>::value,
              "SpecFor must derive byteCopy=true, destroy=nullptr");
static_assert(!std::is_trivially_default_constructible<DlgElem<0x10>>::value &&
              !std::is_trivially_default_constructible<DlgI32>::value,
              "the NSDMI must survive: SpecFor's init is what zeroes the new tail");
static_assert(sizeof(DlgI32Pair) == 8 && alignof(DlgI32Pair) == 4,
              "HelpUI's history element: stride 8, and retail's MemAlloc align is 4");
static_assert(std::is_trivially_default_constructible<DlgI32Pair>::value,
              "and its grown tail must stay UNINITIALISED — no NSDMI on this one");
