#pragma once
// ============================================================================
// code/Game/Component/coll_bridge.h — ENDGAME B.3.2 step 2, batch COLL-COMPONENT
// ============================================================================
//
// WHAT THIS IS FOR
// ----------------
// The containers the exe reaches are `Collection::Array<T>` just like the DLL's,
// but MSVC MONOMORPHISED them into Application-x64.exe: every instantiation is
// its own little function (`FUN_1400021d0` = `Array<int32>::SetSize`,
// `FUN_1400032e0` = `Array<ptr>::SetSize`, `FUN_140003110` = the three-argument
// `SetSize(n, fill)`, …). Those bodies were ported one at a time into
// `code/Game/` and are reached from a caller as
//
//     s_fn_ResizeIntArray = ResolveAddr(Module::Exe, 0x1400021d0);
//     …
//     s_fn_ResizeIntArray(&self->someArrayField, n);
//
// which is the same "a pointer stands in for a member call" shape B.3.1 removed
// for `System::String`. None of these addresses is a `runtime/src/collection`
// shim — a call site in `code/Game/` never reaches one (grep: zero
// `@Collection@@` lookups and zero `Module::Runtime` collection addresses under
// Component/, Action/, Work/, Entity/). What step 2 converts here is therefore
// the EXE-side instantiation set, onto the same `Collection::Array<T>` template.
//
// THE ELEMENT TYPES BELOW, AND WHY THEY CARRY AN NSDMI
// ---------------------------------------------------
// `rtcoll::SpecFor<T>()` (runtime/types/collection/Collection.h) derives the
// `ArraySpec` from T's traits:
//
//   * `init` is `&TInit<T>` exactly when T is NOT trivially default
//     constructible, and `nullptr` otherwise — and a null `init` is retail's
//     EMPTY else arm, i.e. the new tail is left holding whatever the allocator
//     block contained.
//   * `byteCopy` is `is_trivially_copyable<T>`.
//   * `destroy` is null exactly when T is trivially destructible.
//   * stride is `sizeof(T)`, align is `alignof(T)`.
//
// *** NOT EVERY exe array helper zeroes its new tail. The stride-8
// monomorphisations do and the stride-4 ones do NOT — read out of the shipped
// bytes in the B.3.2 closing pass; see the `I32NoInit`/`I32Pair` block below
// for the five addresses. The paragraph that follows is true of the helpers
// THIS batch converted and was wrongly generalised to "every". ***
//
// Every exe array helper this batch converts ZEROES its new tail (each ported
// body writes `*slot = 0` on the else arm; `serialize_resize_helpers.cpp`'s
// `GenericArrayResize` does it unconditionally and says why). So the element
// type has to be one whose default constructor zeroes — that is the whole
// reason for the `= 0` / `= {}` initialisers below. A bare `int32_t` would take
// the `init == nullptr` arm and leave the tail uninitialised: a DIFFERENT array,
// and the kind of difference the resave gate and the stress CSV see.
//
// They all stay trivially COPYABLE, so `byteCopy` is true and the realloc
// prefix copy stays the byte copy the helper ran, and trivially DESTRUCTIBLE,
// so `destroy` stays null exactly as the helpers (which never destroy an
// element) require.
//
// `AsArray<T>(p)` is the "B.3.2 bridge" cast: the array lives as raw bytes
// inside a component/action struct that is not a real C++ class yet, so the use
// site reinterprets those 16 bytes as the container. `ArrayAt<T>(base, off)` is
// the same cast for the commoner spelling where the call site holds `self` and
// a byte offset. The cast disappears when the CONTAINING struct is converted
// (§B.2), not here.
//
// ---------------------------------------------------------------------------
// THIS HEADER ABSORBED code/Game/Dialog/dialog_array_bridge.h
// ---------------------------------------------------------------------------
// The Dialog caller batch wrote the same construction under a second
// vocabulary (`DlgElem<N>`, `DlgArray<N>`, `DlgI32`, `DlgI32Pair`) in the same
// step; both headers' banners said the closing pass would merge them, and this
// is that merge. `DlgElem<N>` was `Bytes<N,8>`, `DlgI32` was `I32` and
// `DlgI32Pair` is `I32Pair` above. Two facts from that header that are not
// obvious from this one and must not be lost:
//
// WHY A BYTE BLOB AND NOT THE REAL ELEMENT TYPE. The elements these dialogs
// push are `InternalBasePtr`/`ExternalPtr` handles and small PODs whose real
// C++ types belong to classes §B.2 has not converted yet. Retail's exe copies
// them with a plain N-byte copy — NO refcount touch — and so did every
// hand-rolled loop this header replaced. A blob reproduces exactly that. When
// §B.2 gives one of these arrays a real element type, THAT conversion replaces
// the blob with it.
//
// THE ONE DELIBERATE DIFFERENCE FROM THE HAND-ROLLED LOOPS. They allocated with
// `fn_MemAlloc(g_pMemAllocSingleton, …)`, i.e. `System::MemoryAllocator::
// Allocate` (0x1800c6e60) — the MEMBER, which does NOT take the allocator's
// critical section. Retail's inlined Array growth takes it (`Enter(_singleton+
// 0x640); Allocate(…); Leave`, quoted in Collection.h's DefaultAllocator
// banner), and that is what `rtcoll::MemAlloc` (0x1800c7a50) does. So the
// converted sites LOCK where the hand-rolled ones did not: a move toward
// retail, not away from it. The free side is unchanged
// (`?MemFree@MemoryAllocator@System@@SAXPEAX@Z` IS `rtcoll::MemFree`'s
// 0x1800c7ad0).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "collection/Collection.h"

namespace GameColl {

// ---------------------------------------------------------------------------
// The generic element: N bytes at alignment A, zeroed by its default ctor.
// Use it for a stride whose real element type is a struct this batch has no
// name for yet (`Bytes<0x14, 4>` is the 20-byte pathfinding vector element).
// ---------------------------------------------------------------------------
template <class X> inline uint64_t ToWord(X v)  { return static_cast<uint64_t>(v); }
template <class X> inline uint64_t ToWord(X* p) { return reinterpret_cast<uint64_t>(p); }

template <int N, int A>
struct alignas(A) Bytes {
    uint8_t b[N] = {};

    Bytes() = default;

    // Word construction, for the append sites that build the fill element out
    // of handles: `Bytes<0x18>(a, b, c)` is the `{a, b, c}` brace list the
    // Dialog batch used to write as `DlgElem<0x18>{{a, b, c}}`. Fewer words
    // than the stride is allowed and the remainder stays zero, exactly as the
    // brace list's zero-fill did. Pointers go through `ToWord` so a call site
    // does not have to spell a cast. This is a CONSTRUCTOR, not a copy: the
    // type stays trivially copyable and trivially destructible, so `SpecFor`
    // still derives `byteCopy = true, destroy = nullptr`.
    template <class... W>
    explicit Bytes(W... words) {
        static_assert(sizeof...(W) > 0 && N % 8 == 0,
                      "word construction needs an 8-multiple stride");
        static_assert(sizeof...(W) * 8 <= N, "too many words for this stride");
        const uint64_t w[sizeof...(W)] = { ToWord(words)... };
        __builtin_memcpy(b, w, sizeof...(W) * 8);
    }
};

// The named strides, for the call sites where the element type IS known.
struct Ptr8 { void*    v = nullptr; };   // stride 0x08, align 8 — Object ptr / InternalBasePtr slot
struct I32  { int32_t  v = 0; };         // stride 0x04, align 4
struct U8   { uint8_t  v = 0; };         // stride 0x01, align 1
struct U16  { uint16_t v = 0; };         // stride 0x02, align 2
struct F32  { float    v = 0.0f; };      // stride 0x04, align 4
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };   // stride 0x0c, align 4

// ---------------------------------------------------------------------------
// THE ELEMENTS WHOSE NEW TAIL RETAIL LEAVES UNINITIALISED
// ---------------------------------------------------------------------------
// The banner above says "every exe array helper this batch converts ZEROES its
// new tail". That is true of the STRIDE-8 monomorphisations and FALSE of the
// stride-4 ones, read out of the shipped bytes (ENDGAME B.3.2 closing pass,
// Ghidra Application-x64.exe):
//
//   0x1400032e0  Array<qword>::SetSize(n)      stride 8, align 8: the else arm
//   0x140001bb0  Array<qword>::SetSize(n)      is `*slot = 0` -> ZEROES.
//   0x140003110  Array<qword>::SetSize(n,fill) stride 8, align 8, fill.
//   0x1400021d0  Array<dword>::SetSize(n)      stride 4, align 4: the loop body
//                                              is `if (i < oldSize) *slot =
//                                              old[i];` and NOTHING ELSE — the
//                                              grown tail is left as the
//                                              allocation found it.
//   0x140003600  Array<dword>::SetSize(n,fill) stride 4, align 4, fill; no
//                                              init arm exists either.
//
// So a stride-4 site converted onto `I32` (which zeroes) is a DIFFERENT array
// from retail's. `I32NoInit` is the one to use wherever the ported body came
// from 0x1400021d0 / 0x140003600; `I32` stays for the sites whose own ported
// body does write a zero.
//
// *** OPEN DIVERGENCE, LEFT DELIBERATELY UNFIXED BY THE CLOSING PASS ***
// About twenty sites converted in step 2 name 0x1400021d0 in their own comment
// and use `I32` — i.e. they ZERO a tail retail leaves as-found. Grep
// `GameColl::I32>` together with `0x1400021d0` to list them (action_pf_region,
// action_pf_astar, gatherrawmaterials{,_ctor}, cemeterycomponent,
// createplaceddescription, autopickupcomponent, resourceproduce_creatework,
// consumeproduce, fishingwork, …). Most are `SetSize(0)` or a shrink, where the
// two agree; the growing ones differ only on a REUSED allocator block, and in
// every one of them the caller writes all `n` elements straight afterwards.
// Flipping them to `I32NoInit` is the faithful change, but it turns a zeroed
// tail into allocator garbage across the pathfinder and the work scheduler, so
// it belongs to a change that the resave gate and the stress run can cover —
// not to a closing pass verified by one boot. Do it as its own step.
struct I32NoInit { int32_t v; };          // stride 0x04, align 4, tail as-found

// stride 0x08 at ALIGN 4 — a {int32,int32} pair, and retail's helper for this
// shape (HelpUI's history array, 0x140011d50) passes 4 as MemAlloc's alignment
// argument and leaves a grown tail uninitialised too. Deliberately NO NSDMI.
struct I32Pair { int32_t a; int32_t b; };

// Every one of these must satisfy the three trait questions the way the ported
// helper body answers them, or the converted call site is a different array.
#define GAMECOLL_CHECK_ELEM(T, SIZE, ALIGN)                                        \
    static_assert(sizeof(T) == (SIZE),  #T " stride");                             \
    static_assert(alignof(T) == (ALIGN), #T " align");                             \
    static_assert(std::is_trivially_copyable<T>::value,                            \
                  #T " must stay byteCopy (SpecFor: byteCopy = is_trivially_copyable)"); \
    static_assert(std::is_trivially_destructible<T>::value,                        \
                  #T " must stay destroy-free (SpecFor: destroy = !is_trivially_destructible)"); \
    static_assert(!std::is_trivially_default_constructible<T>::value,              \
                  #T " must ZERO its new tail (SpecFor: init = !is_trivially_default_constructible)")

GAMECOLL_CHECK_ELEM(Ptr8, 8, 8);
GAMECOLL_CHECK_ELEM(I32,  4, 4);
GAMECOLL_CHECK_ELEM(U8,   1, 1);
GAMECOLL_CHECK_ELEM(U16,  2, 2);
GAMECOLL_CHECK_ELEM(F32,  4, 4);
GAMECOLL_CHECK_ELEM(Vec3, 12, 4);

// `Bytes<N, A>` answers the three questions the same way for every N and A;
// two representative widths are checked so a regression in the template shows
// up at compile time rather than at a call site.
using Bytes16x4 = Bytes<0x10, 4>;
using Bytes20x4 = Bytes<0x14, 4>;
GAMECOLL_CHECK_ELEM(Bytes16x4, 0x10, 4);
GAMECOLL_CHECK_ELEM(Bytes20x4, 0x14, 4);

// The same three questions for the two elements whose tail must stay as the
// allocation found it: the LAST assert is inverted against GAMECOLL_CHECK_ELEM.
#define GAMECOLL_CHECK_ELEM_NOINIT(T, SIZE, ALIGN)                                 \
    static_assert(sizeof(T) == (SIZE),  #T " stride");                             \
    static_assert(alignof(T) == (ALIGN), #T " align");                             \
    static_assert(std::is_trivially_copyable<T>::value, #T " must stay byteCopy");  \
    static_assert(std::is_trivially_destructible<T>::value, #T " must stay destroy-free"); \
    static_assert(std::is_trivially_default_constructible<T>::value,                \
                  #T " must LEAVE its new tail alone (SpecFor: init == nullptr)")

GAMECOLL_CHECK_ELEM_NOINIT(I32NoInit, 4, 4);
GAMECOLL_CHECK_ELEM_NOINIT(I32Pair,   8, 4);

// ---------------------------------------------------------------------------
// The bridge cast. `header` points at the 16 bytes { T* data; int32 size;
// int32 capacity } living inside a struct that is still raw bytes.
// ---------------------------------------------------------------------------
template <class T>
inline Collection::Array<T>& AsArray(void* header) {
    return *reinterpret_cast<Collection::Array<T>*>(header);   // B.3.2 bridge
}
template <class T>
inline const Collection::Array<T>& AsArray(const void* header) {
    return *reinterpret_cast<const Collection::Array<T>*>(header);   // B.3.2 bridge
}
template <class T>
inline Collection::Array<T>& AsArray(uintptr_t header) {
    return *reinterpret_cast<Collection::Array<T>*>(header);   // B.3.2 bridge
}

// Same cast, for the very common "the array is at a raw byte offset inside a
// struct §B.2 has not converted yet" spelling. This is the Dialog batch's
// `DlgArray<N>(self, off)` under the one vocabulary.
template <class T>
inline Collection::Array<T>& ArrayAt(uintptr_t base, int off) {
    return *reinterpret_cast<Collection::Array<T>*>(base + off);   // B.3.2 bridge
}
template <class T>
inline Collection::Array<T>& ArrayAt(void* base, int off) {
    return ArrayAt<T>(reinterpret_cast<uintptr_t>(base), off);
}
template <class T>
inline const Collection::Array<T>& ArrayAt(const void* base, int off) {
    return *reinterpret_cast<const Collection::Array<T>*>(
        reinterpret_cast<uintptr_t>(base) + off);   // B.3.2 bridge
}

}  // namespace GameColl
