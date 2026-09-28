#pragma once
// ============================================================================
// tests/unit/b39_uib_test_util.h — ENDGAME B.3 step 9, block UI-B (rule B9-16)
// ============================================================================
// The shared checks for a widget class made CONCRETE (B39_RULES D1): the shape
// (not abstract, public virtual dtor since B3X W1), the slot order by
// pointer-to-virtual-member (adjustment 0), and the retired slot-0 arms. The
// leaves' destructors are MARKERS in ui_leaf_c4/c5_abi_test.cpp (runtime/src/ui
// does not link into the host test); each marker calls B39NoteDtor(this), so
// the arms can be counted and ordered.
#include "unit.h"
#include "slot_check.h"
#include "dtor_check.h"

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

// B3X W1: the destructor is public and virtual (retail slot 0 = D1 + D0).
template <typename T>
constexpr bool B39IsConcreteShape() {
    return !std::is_abstract<T>::value && std::is_destructible<T>::value &&
           std::has_virtual_destructor<T>::value && std::is_polymorphic<T>::value;
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

// Retail slot 0 (the retired RT_DELDTOR row) is now the destructor pair
// (B3X W1). What its arms proved, proven the C++ way: `a->~T()` (flags 0)
// destroys without freeing; `delete b` (flags 1) runs T's own D0, which frees
// through T's class-scope operator delete (InternalMemFree == free here,
// matching InternalMemAlloc), and on a dtor_check::Recording<T> stand-in the
// free is RECORDED with the object's address; retail's flags-3 vector arm has
// no Itanium counterpart: `delete[]` of a `new T[3]` block runs three
// destructors, DOWNWARD. Also: two real instances share ONE table, which is
// not `probeVt` (the test probe's), and D1 != D0 in it. Run inside the
// caller's FactoryScope for an Internal-rooted class.
template <typename T, size_t Size>
void B39CheckDestructorArms(const void* probeVt) {
    static_assert(sizeof(T) == Size, "the row's element size is sizeof");
    static_assert(dtor_check::HasClassDelete<T>::value,
                  "D0 frees through a class-scope operator delete (MemFree)");
    alignas(8) uint8_t buf[Size];
    T* a = ::new (static_cast<void*>(buf)) T();
    void* q = Object::detail::InternalMemAlloc(Size, 8);
    CHECK(q != nullptr);
    T* b = ::new (q) T();
    const void* vtA = *reinterpret_cast<const void* const*>(a);
    CHECK(vtA != nullptr);
    CHECK(vtA == *reinterpret_cast<const void* const*>(b));
    CHECK(vtA != probeVt);
    CHECK(static_cast<const void* const*>(vtA)[0] != static_cast<const void* const*>(vtA)[1]);

    g_b39DtorCount = 0;
    dtor_check::ResetFrees();
    a->~T();                                   // flags 0: no free
    CHECK_EQ(g_b39DtorCount, 1);
    CHECK(g_b39DtorOrder[0] == a);
    CHECK_EQ(dtor_check::g_frees, 0);
    g_b39DtorCount = 0;
    delete b;                                  // flags 1: T's own D0
    CHECK_EQ(g_b39DtorCount, 1);
    CHECK(g_b39DtorOrder[0] == b);

    T* r = dtor_check::NewOnMalloc<dtor_check::Recording<T>>();
    g_b39DtorCount = 0;
    dtor_check::ResetFrees();
    delete r;                                  // the free, recorded
    CHECK_EQ(g_b39DtorCount, 1);
    CHECK(g_b39DtorOrder[0] == r);
    CHECK_EQ(dtor_check::g_frees, 1);
    CHECK(dtor_check::g_lastFree == r);

    CHECK_ARRAY_DELETE(T, g_b39DtorCount, g_b39DtorOrder);
}
