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
// site reinterprets those 16 bytes as the container. The cast disappears when
// the CONTAINING struct is converted (§B.2), not here.
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
template <int N, int A>
struct alignas(A) Bytes {
    uint8_t b[N] = {};
};

// The named strides, for the call sites where the element type IS known.
struct Ptr8 { void*    v = nullptr; };   // stride 0x08, align 8 — Object ptr / InternalBasePtr slot
struct I32  { int32_t  v = 0; };         // stride 0x04, align 4
struct U8   { uint8_t  v = 0; };         // stride 0x01, align 1
struct U16  { uint16_t v = 0; };         // stride 0x02, align 2
struct F32  { float    v = 0.0f; };      // stride 0x04, align 4
struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };   // stride 0x0c, align 4

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

}  // namespace GameColl
