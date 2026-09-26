// ============================================================================
// tests/unit/loop_abi_test.cpp — ENDGAME B.3.8 block LOOP (B38_RULES.md B8-14)
//
// The ABI claims block LOOP makes for its classes, measured on the SHIPPED
// HEADERS (code/Game/Game/<Class>.h):
//
// PART A — LAYOUT. sizeof / alignof against each retail create site's MemAlloc
//   and the member offsets the ported bodies read.
// PART B — SHAPE. Every class is concrete, `final`, single inheritance from its
//   proved Runtime base, polymorphic, NO virtual destructor.
// PART C — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//   1 + vtable byte offset, adjustment 0): each override lands on the retail
//   index of B38_SLOTS.md sec. 6 / B37_RULES sec. 3.1, and slot 0 is
//   `DeletingDtor`.
//
// NOT PROVEN HERE: the bodies (they stand on the address registry and cannot
// link into a host test), and `Vtable() == vptr` (tests/unit/exe_class_abi_test.cpp
// PART C proves DEFINE_EXE_VTABLE on this toolchain; rb_engine_abi_test.cpp
// proves the loop bases' ctor/dtor stamp order through stand-in subclasses).
// ============================================================================
#include "unit.h"

#include "Game/InnerLoaderLoop.h"
#include "Game/GameLoaderLoop.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

template <typename PMF>
uintptr_t SlotOffset(PMF pmf) {
    static_assert(sizeof(PMF) == 2 * sizeof(uintptr_t), "Itanium PMF is two words");
    uintptr_t words[2];
    std::memcpy(words, &pmf, sizeof words);
    if ((words[0] & 1u) == 0) return ~uintptr_t(0);   // not virtual
    if (words[1] != 0) return ~uintptr_t(0);           // this-adjusting: not single inheritance
    return words[0] - 1;
}

constexpr uintptr_t S(int slot) { return static_cast<uintptr_t>(slot) * 8; }

// ===========================================================================
// PART A — LAYOUT
// ===========================================================================
UNIT_TEST(loop_sizes_match_retail) {
    CHECK_EQ(sizeof(InnerLoaderLoop), (size_t)0x48);   // GameMain MemAlloc(0x48)
    CHECK_EQ(sizeof(GameLoaderLoop),  (size_t)0x48);   // MemAlloc(0x48) at all 7 sites
    CHECK_EQ(alignof(InnerLoaderLoop), (size_t)8);
    CHECK_EQ(alignof(GameLoaderLoop),  (size_t)8);
}

// ===========================================================================
// PART B — SHAPE
// ===========================================================================
UNIT_TEST(loop_classes_shape) {
    CHECK((std::is_base_of<Engine::LoaderLoop, InnerLoaderLoop>::value));
    CHECK((std::is_base_of<Engine::LoaderLoop, GameLoaderLoop>::value));
    CHECK(!std::is_abstract<InnerLoaderLoop>::value);
    CHECK(!std::is_abstract<GameLoaderLoop>::value);
    CHECK(std::is_final<InnerLoaderLoop>::value);
    CHECK(std::is_final<GameLoaderLoop>::value);
    CHECK(!std::has_virtual_destructor<InnerLoaderLoop>::value);
    CHECK(!std::has_virtual_destructor<GameLoaderLoop>::value);
}

// ===========================================================================
// PART C — SLOT ORDER
// ===========================================================================
UNIT_TEST(loop_loaderloop_slots) {
    // 0x140158870: own [0], [6], [10].
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::DeletingDtor),        S(0));
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::Load),                S(6));
    CHECK_EQ(SlotOffset(&InnerLoaderLoop::GetResourceFilename), S(10));
    // 0x1401588d0: own [0] only.
    CHECK_EQ(SlotOffset(&GameLoaderLoop::DeletingDtor),         S(0));
    // Both tables are 12 slots: the last is AllowsModChanges, UpdateLoop's.
    CHECK_EQ(SlotOffset(&Engine::UpdateLoop::AllowsModChanges), S(11));
}

}  // namespace
