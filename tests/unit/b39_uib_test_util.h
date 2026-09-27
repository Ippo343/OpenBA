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

#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "runtime/types/object/Internal.h"

inline int         g_b39DtorCount = 0;
inline const void* g_b39DtorOrder[8] = {};
inline void B39NoteDtor(const void* self) {
    if (g_b39DtorCount < 8) g_b39DtorOrder[g_b39DtorCount] = self;
    ++g_b39DtorCount;
}

// Itanium PMF: {1 + vtable byte offset, this adjustment} for a virtual.
template <typename P>
uintptr_t B39SlotOffset(P pmf) {
    static_assert(sizeof(P) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting
    return words[0] - 1;
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
    CHECK_EQ(B39SlotOffset(&T::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(B39SlotOffset(&T::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(B39SlotOffset(&T::Type),           (uintptr_t)0x10);
    CHECK_EQ(B39SlotOffset(&T::GetFileVersion), (uintptr_t)0x18);
    CHECK_EQ(B39SlotOffset(&T::Compile),        (uintptr_t)0x20);
    CHECK_EQ(B39SlotOffset(&T::Serialize),      (uintptr_t)0x28);
    CHECK_EQ(B39SlotOffset(&T::Reconstruct),    (uintptr_t)0x30);
    CHECK_EQ(B39SlotOffset(&T::Destruct),       (uintptr_t)0x38);
    CHECK_EQ(B39SlotOffset(&T::CreateInstance), (uintptr_t)0x40);
    CHECK_EQ(B39SlotOffset(&T::Construct),      (uintptr_t)0x48);
}

// An element's twelve slots.
template <typename T>
void B39CheckElementPmf() {
    CHECK_EQ(B39SlotOffset(&T::DeletingDtor),  (uintptr_t)0x00);
    CHECK_EQ(B39SlotOffset(&T::TypeName),      (uintptr_t)0x08);
    CHECK_EQ(B39SlotOffset(&T::Type),          (uintptr_t)0x10);
    CHECK_EQ(B39SlotOffset(&T::Persist),       (uintptr_t)0x18);
    CHECK_EQ(B39SlotOffset(&T::ComputeSize),   (uintptr_t)0x20);
    CHECK_EQ(B39SlotOffset(&T::ComputeLayout), (uintptr_t)0x28);
    CHECK_EQ(B39SlotOffset(&T::Draw),          (uintptr_t)0x30);
    CHECK_EQ(B39SlotOffset(&T::Update),        (uintptr_t)0x38);
    CHECK_EQ(B39SlotOffset(&T::Enable),        (uintptr_t)0x40);
    CHECK_EQ(B39SlotOffset(&T::Contains),      (uintptr_t)0x48);
    CHECK_EQ(B39SlotOffset(&T::SetColor),      (uintptr_t)0x50);
    CHECK_EQ(B39SlotOffset(&T::GetBorder),     (uintptr_t)0x58);
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
