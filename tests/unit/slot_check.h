#pragma once
// ============================================================================
// tests/unit/slot_check.h — vtable slot checks written in RETAIL numbers
// ============================================================================
// Every slot-order test states the RETAIL slot (retail's `[RAX+0x40]` is slot
// 0x40/8). The compiler's slot is derived from the TYPE (B3X_RULES X5):
//
//   - a class whose retail slot 0 is a deleting destructor and which declares
//     `virtual ~T()` spends TWO Itanium slots on it (D1 complete, D0 deleting),
//     so its retail slot n >= 1 sits at compiler slot n + 1;
//   - a class still on `virtual T* DeletingDtor(unsigned)` in slot 0, and every
//     class with no destructor slot at all (the Thread family, the Controller
//     interface, IO::Encoding, PlatformChannel, IPathInfo, the Video::*Data
//     seam), has no virtual destructor and is not shifted.
//
// There is no exempt list here: `std::has_virtual_destructor` says which a
// class is. The shift is keyed on the class that DECLARES the member (the
// class of the pointer-to-member): a primary base that has no virtual
// destructor keeps its slots in a derived class that adds one (the derived
// destructor is appended), and an exempt secondary base (Controller) keeps its
// own numbering inside a class whose primary table flips.
//
// Constants are never bumped by hand. A raw read through a vtable takes the
// retail number and translates it with RetailSlot<T> (or RetailByteOffset<T>).
// ============================================================================
#include "unit.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "runtime/types/object/VSlot.h"

// Itanium PMF decode: the compiler vtable INDEX of a virtual member, -1 when
// the member is non-virtual or the pointer carries a this-adjustment.
template <class PMF>
inline long CompilerSlot(PMF pmf) {
    return Object::CompilerSlotOf(pmf);
}

// Retail slot n of class T, as the compiler numbers it.
template <class T>
constexpr long RetailSlot(int n) {
    return n + (std::has_virtual_destructor<T>::value && n >= 1 ? 1 : 0);
}

// The same for a retail BYTE offset into T's table (a raw `vt + off` read).
template <class T>
constexpr uintptr_t RetailByteOffset(uintptr_t retailOff) {
    return static_cast<uintptr_t>(RetailSlot<T>(static_cast<int>(retailOff / sizeof(void*)))) *
           sizeof(void*);
}

namespace slot_check {

// The class a pointer-to-member belongs to (the declaring class of `&T::M`).
template <class C, class F>
C* ClassOfPmf(F C::*);
template <class PMF>
using PmfClass = std::remove_pointer_t<decltype(ClassOfPmf(std::declval<PMF>()))>;

// Retail slot n for the class of `pmf`; -1 for retail slot 0 of a class with a
// virtual destructor (slot 0 is the destructor pair: use CHECK_DTOR_SLOT).
template <class PMF>
constexpr long RetailSlotOfPmf(int n) {
    using C = PmfClass<PMF>;
    static_assert(sizeof(C) > 0, "the class must be complete");
    return (std::has_virtual_destructor<C>::value && n == 0) ? -1 : RetailSlot<C>(n);
}

// true when `pmf` is a virtual member at retail slot n.
template <class PMF>
inline bool SlotOk(PMF pmf, int n) {
    const long want = RetailSlotOfPmf<PMF>(n);
    return want >= 0 && CompilerSlot(pmf) == want;
}

template <class T, class = void>
struct HasDeletingDtor : std::false_type {};
template <class T>
struct HasDeletingDtor<T, std::void_t<decltype(&T::DeletingDtor)>> : std::true_type {};

// Retail slot 0 of T is its deleting destructor. While T is on
// `virtual T* DeletingDtor(unsigned)` that member sits in compiler slot 0 and T
// has no virtual destructor; once T declares `virtual ~T()` the member is gone
// and the destructor pair occupies compiler slots 0 and 1.
template <class T>
inline bool DtorSlotOk() {
    static_assert(sizeof(T) > 0, "the class must be complete");
    if constexpr (HasDeletingDtor<T>::value)
        return CompilerSlot(&T::DeletingDtor) == 0 && !std::has_virtual_destructor<T>::value;
    else
        return std::has_virtual_destructor<T>::value;
}

template <class PMF>
inline void CheckSlot(PMF pmf, int n, const char* what, const char* file, int line) {
    const long want = RetailSlotOfPmf<PMF>(n);
    const long got = CompilerSlot(pmf);
    if (want < 0) {
        std::printf("  FAIL %s:%d: %s: retail slot 0 of a class with a virtual destructor "
                    "(use CHECK_DTOR_SLOT)\n", file, line, what);
        ++unit::Failures();
    } else if (got != want) {
        std::printf("  FAIL %s:%d: %s: compiler slot %ld, retail slot %d wants %ld\n",
                    file, line, what, got, n, want);
        ++unit::Failures();
    }
}

template <class T>
inline void CheckDtorSlot(const char* what, const char* file, int line) {
    if (!DtorSlotOk<T>()) {
        std::printf("  FAIL %s:%d: %s: retail slot 0 is not the destructor\n", file, line, what);
        ++unit::Failures();
    }
}

}  // namespace slot_check

// `&T::M` sits at retail slot n (an index; a retail byte offset is written off/8).
#define CHECK_SLOT(T, M, n) \
    slot_check::CheckSlot(&T::M, (n), "&" #T "::" #M, __FILE__, __LINE__)
// The same for a pointer-to-member expression (an overload picked by cast, a
// pointer held in a variable).
#define CHECK_PMF_SLOT(pmf, n) \
    slot_check::CheckSlot((pmf), (n), #pmf, __FILE__, __LINE__)
// Retail slot 0 of T is the deleting destructor (see DtorSlotOk).
#define CHECK_DTOR_SLOT(T) \
    slot_check::CheckDtorSlot<T>(#T, __FILE__, __LINE__)
