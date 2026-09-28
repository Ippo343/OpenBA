#pragma once
// ============================================================================
// tests/unit/b39_uib_test_util.h — ENDGAME B.3 step 9, block UI-B (rule B9-16)
// ============================================================================
// The shared checks for a widget class made CONCRETE (B39_RULES D1): the shape
// (not abstract, protected non-virtual dtor), the slot order by
// pointer-to-virtual-member (adjustment 0), and both `DeletingDtor` arms. The
// leaves' destructors are MARKERS in ui_leaf_c4/c5_abi_test.cpp (runtime/src/ui
// does not link into the host test); each marker calls B39NoteDtor(this), so
// the arms can be counted and ordered.
#include "unit.h"
#include "slot_check.h"

#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "runtime/types/object/Internal.h"
#include "runtime/types/ui/ElementDescription.h"   // UI::Element / UI::Dialog (declared)

inline int         g_b39DtorCount = 0;
inline const void* g_b39DtorOrder[8] = {};
inline void B39NoteDtor(const void* self) {
    if (g_b39DtorCount < 8) g_b39DtorOrder[g_b39DtorCount] = self;
    ++g_b39DtorCount;
}

template <typename T>
constexpr bool B39IsConcreteShape() {
    return !std::is_abstract<T>::value && !std::is_destructible<T>::value &&
           !std::has_virtual_destructor<T>::value && std::is_polymorphic<T>::value;
}

// A description's ten slots (B36_SLOTS.md section 1): [4] Compile inherited,
// [9] Construct introduced behind CreateInstance.
template <typename T>
void B39CheckDescriptionPmf() {
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,       0x08/8);
    CHECK_SLOT(T, Type,           0x10/8);
    CHECK_SLOT(T, GetFileVersion, 0x18/8);
    CHECK_SLOT(T, Compile,        0x20/8);
    CHECK_SLOT(T, Serialize,      0x28/8);
    CHECK_SLOT(T, Reconstruct,    0x30/8);
    CHECK_SLOT(T, Destruct,       0x38/8);
    // (static_cast: a leaf may also declare the non-virtual applier overload)
    using CreateFn = Object::InternalPtr<UI::Element>* (T::*)(
        Object::InternalPtr<UI::Element>*, const UI::Dialog*) const;
    CHECK_PMF_SLOT(static_cast<CreateFn>(&T::CreateInstance), 0x40/8);
    CHECK_SLOT(T, Construct, 0x48/8);
}

// An element's twelve slots.
template <typename T>
void B39CheckElementPmf() {
    CHECK_DTOR_SLOT(T);
    CHECK_SLOT(T, TypeName,      0x08/8);
    CHECK_SLOT(T, Type,          0x10/8);
    CHECK_SLOT(T, Persist,       0x18/8);
    CHECK_SLOT(T, ComputeSize,   0x20/8);
    CHECK_SLOT(T, ComputeLayout, 0x28/8);
    CHECK_SLOT(T, Draw,          0x30/8);
    CHECK_SLOT(T, Update,        0x38/8);
    CHECK_SLOT(T, Enable,        0x40/8);
    CHECK_SLOT(T, Contains,      0x48/8);
    CHECK_SLOT(T, SetColor,      0x50/8);
    CHECK_SLOT(T, GetBorder,     0x58/8);
}

// Both arms of slot 0, exactly as the retired RT_DELDTOR row ran them: flags 0
// destroys without freeing; flags 1 destroys and frees `this` (host seam:
// InternalMemFree == free, so a wrong pointer is a heap fault); flags 3 reads
// the int32 count at `this - 8`, destroys DOWNWARD with stride `Size`, frees
// and returns `this - 8`. Also: two real instances share ONE table, which is
// not `probeVt` (the test probe's). Run inside the caller's FactoryScope for an
// Internal-rooted class.
template <typename T, size_t Size>
void B39CheckDeletingDtorArms(const void* probeVt) {
    static_assert(sizeof(T) == Size, "the row's element size is sizeof");
    alignas(8) uint8_t buf[Size];
    T* a = ::new (static_cast<void*>(buf)) T();
    void* q = Object::detail::InternalMemAlloc(Size, 8);
    CHECK(q != nullptr);
    T* b = ::new (q) T();
    const void* vtA = *reinterpret_cast<const void* const*>(a);
    CHECK(vtA != nullptr);
    CHECK(vtA == *reinterpret_cast<const void* const*>(b));
    CHECK(vtA != probeVt);

    g_b39DtorCount = 0;
    CHECK(static_cast<void*>(a->DeletingDtor(0)) == static_cast<void*>(a));
    CHECK_EQ(g_b39DtorCount, 1);
    CHECK(g_b39DtorOrder[0] == a);
    g_b39DtorCount = 0;
    CHECK(static_cast<void*>(b->DeletingDtor(1)) == static_cast<void*>(b));
    CHECK_EQ(g_b39DtorCount, 1);

    uint8_t* block = static_cast<uint8_t*>(Object::detail::InternalMemAlloc(8 + 3 * Size, 8));
    CHECK(block != nullptr);
    const int64_t cookie = 3;
    std::memcpy(block, &cookie, sizeof cookie);
    T* e[3];
    for (int i = 0; i < 3; ++i) e[i] = ::new (static_cast<void*>(block + 8 + i * Size)) T();
    g_b39DtorCount = 0;
    CHECK(reinterpret_cast<uint8_t*>(e[0]->DeletingDtor(3)) == block);
    CHECK_EQ(g_b39DtorCount, 3);
    CHECK(g_b39DtorOrder[0] == e[2]);
    CHECK(g_b39DtorOrder[1] == e[1]);
    CHECK(g_b39DtorOrder[2] == e[0]);
}
