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
#include "Game/GameLoadDialogLoopT.h"
#include "Game/MenuDialogLoopT.h"

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
    CHECK_EQ(sizeof(GameLoadDialogLoopT), (size_t)0xb60);   // MemAlloc(0xb60, 4) at all 7 sites
    CHECK_EQ(alignof(GameLoadDialogLoopT), (size_t)8);
    CHECK_EQ(sizeof(MenuDialogLoopT), (size_t)0xc00);        // MemAlloc(0xc00, 8)
    CHECK_EQ(alignof(MenuDialogLoopT), (size_t)8);
}

UNIT_TEST(loop_member_offsets) {
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _dialogExternal), (size_t)0x50);   // base's
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _ui),             (size_t)0x60);   // base's
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeColor1),     (size_t)0x188);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeColor2),     (size_t)0x18c);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _fadeDone),       (size_t)0x190);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _random),         (size_t)0x194);
    CHECK_EQ(offsetof(GameLoadDialogLoopT, _tipTimer),       (size_t)0xb58);
    CHECK_EQ(offsetof(MenuDialogLoopT, _ui),                 (size_t)0x40);    // base's
    CHECK_EQ(offsetof(MenuDialogLoopT, _random),             (size_t)0x168);
    CHECK_EQ(offsetof(MenuDialogLoopT, _audioScene),         (size_t)0xb30);
    CHECK_EQ(offsetof(MenuDialogLoopT, _pendingLoop),        (size_t)0xb68);
    CHECK_EQ(offsetof(MenuDialogLoopT, _saveLoad),           (size_t)0xb70);
    CHECK_EQ(offsetof(MenuDialogLoopT, _menuScene),          (size_t)0xb90);
    CHECK_EQ(offsetof(MenuDialogLoopT, _scene),              (size_t)0xba0);
    CHECK_EQ(offsetof(MenuDialogLoopT, _sceneIdArray),       (size_t)0xbf0);
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
    CHECK((std::is_base_of<Engine::DialogLoaderLoopT, GameLoadDialogLoopT>::value));
    CHECK(!std::is_abstract<GameLoadDialogLoopT>::value);
    CHECK(std::is_final<GameLoadDialogLoopT>::value);
    CHECK(!std::has_virtual_destructor<GameLoadDialogLoopT>::value);
    CHECK((std::is_base_of<Engine::DialogLoopT, MenuDialogLoopT>::value));
    CHECK(!std::is_abstract<MenuDialogLoopT>::value);
    CHECK(std::is_final<MenuDialogLoopT>::value);
    CHECK(!std::has_virtual_destructor<MenuDialogLoopT>::value);
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

UNIT_TEST(loop_splash_slots) {
    // 0x140158800: own [0], [1], [2], [3], [12].
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::DeletingDtor),     S(0));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Startup),          S(1));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Shutdown),         S(2));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::Update),           S(3));
    CHECK_EQ(SlotOffset(&GameLoadDialogLoopT::CreateLoopDialog), S(12));
}

UNIT_TEST(loop_menu_slots) {
    // 0x14015ed00: own [0]-[4], [6], [8]-[12]; [5] and [7] are UpdateLoop's.
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::DeletingDtor),        S(0));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Startup),             S(1));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Shutdown),            S(2));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Update),              S(3));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Draw),                S(4));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::Load),                S(6));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::OnVideoError),        S(8));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::OnVideoRestart),      S(9));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::GetResourceFilename), S(10));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::AllowsModChanges),    S(11));
    CHECK_EQ(SlotOffset(&MenuDialogLoopT::CreateLoopDialog),    S(12));
}

// The retslot slot 12: RCX = this, RDX = the caller's InternalPtr slot, RAX =
// the slot back (B6-2). Declared type checked; the body is not linked here.
UNIT_TEST(loop_splash_slot12_is_retslot_first) {
    using Ret = Object::InternalPtr<UI::DialogInstance>;
    CHECK((std::is_same<decltype(&GameLoadDialogLoopT::CreateLoopDialog),
                        Ret* (GameLoadDialogLoopT::*)(Ret*)>::value));
    CHECK((std::is_same<decltype(&MenuDialogLoopT::CreateLoopDialog),
                        Ret* (MenuDialogLoopT::*)(Ret*)>::value));
}

}  // namespace
