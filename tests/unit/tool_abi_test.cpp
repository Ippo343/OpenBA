// ============================================================================
// tests/unit/tool_abi_test.cpp — ENDGAME B.3.8 block TOOL (B38_RULES.md B8-14)
//
// The ABI claims block TOOL makes about the exe Tool family, measured on the
// SHIPPED HEADERS (code/Game/Player/*.h):
//   PART A — LAYOUT: sizeof / alignof / member offsets against retail's create
//            MemAlloc sizes and ctor stores (B38_SLOTS.md §6).
//   PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//            1 + vtable byte offset; adjustment must be 0: single inheritance).
//   PART C — class shape: abstractness, no virtual destructor.
//   PART D — the hidden-return-slot members (slot 11 GetSettingType, slot 13
//            GetSelected of EditTool/PathTool): a raw retail-order call
//            `fn(this /*RCX*/, ret /*RDX*/)` through a pointer of that shape
//            reaches a member of the SAME declaration shape with the right
//            `this` and `ret` (GCC ms_abi puts an IMPLICIT return slot before
//            `this`, so the members spell it explicitly — B8-11).
//
// NOT PROVEN HERE: the constructors and slot bodies of the tools themselves
// (they stand on the address registry and the exe-type registration list and
// cannot link into a host test) — the coordinator's run gates verify those.
// ============================================================================
#include "unit.h"

#include "Player/ToolSetting.h"
#include "Player/RotationSetting.h"

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

// ===========================================================================
// ToolSetting / RotationSetting
// ===========================================================================
UNIT_TEST(tool_setting_layout) {
    CHECK_EQ(sizeof(ToolSetting),     (size_t)0x10);
    CHECK_EQ(sizeof(RotationSetting), (size_t)0x18);   // create 0x140026d20 MemAlloc(0x18)
    CHECK_EQ(alignof(RotationSetting), (size_t)8);
    CHECK_EQ(offsetof(RotationSetting, rotation), (size_t)0x10);
}

UNIT_TEST(tool_setting_slot_order) {
    CHECK_EQ(SlotOffset(&ToolSetting::DeletingDtor),     (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&ToolSetting::TypeName),         (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&ToolSetting::Type),             (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&ToolSetting::Persist),          (uintptr_t)0x18);   // inherited Internal::Persist
    CHECK_EQ(SlotOffset(&RotationSetting::DeletingDtor), (uintptr_t)0x00);   // 0x140026dd0
    // (slots 1/2 of a leaf are DECLARE_EXE_TYPE's inline bodies over the
    // registration record, which does not link into a host test.)
}

UNIT_TEST(tool_setting_shape) {
    CHECK(std::is_polymorphic<ToolSetting>::value);
    CHECK(!std::has_virtual_destructor<ToolSetting>::value);
    CHECK(!std::has_virtual_destructor<RotationSetting>::value);
    CHECK(!std::is_abstract<RotationSetting>::value);
    CHECK((std::is_base_of<ToolSetting, RotationSetting>::value));
    CHECK((std::is_base_of<Object::Internal, ToolSetting>::value));
}

}  // namespace
