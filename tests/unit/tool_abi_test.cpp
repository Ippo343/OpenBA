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
#include "Player/Tool.h"
#include "Player/dragtool.h"

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

// ===========================================================================
// Tool / DragTool (the abstract bases)
// ===========================================================================
UNIT_TEST(tool_base_layout) {
    CHECK_EQ(sizeof(Tool),     (size_t)0x28);
    CHECK_EQ(sizeof(DragTool), (size_t)0x38);
    CHECK_EQ(offsetof(Tool, dialogPtr),      (size_t)0x10);
    CHECK_EQ(offsetof(Tool, toolSetting),    (size_t)0x18);
    CHECK_EQ(offsetof(Tool, descriptionPtr), (size_t)0x20);
    CHECK_EQ(offsetof(DragTool, dragState),   (size_t)0x28);
    CHECK_EQ(offsetof(DragTool, singleClick), (size_t)0x29);
    CHECK_EQ(offsetof(DragTool, startCell),   (size_t)0x2c);
    CHECK_EQ(offsetof(DragTool, endCell),     (size_t)0x30);
    CHECK_EQ(offsetof(DragTool, lastEndCell), (size_t)0x34);
}

UNIT_TEST(tool_base_slot_order) {
    // Tool 0x140161f10, 13 slots.
    CHECK_EQ(SlotOffset(&Tool::DeletingDtor),   (uintptr_t)0x00);
    CHECK_EQ(SlotOffset(&Tool::TypeName),       (uintptr_t)0x08);
    CHECK_EQ(SlotOffset(&Tool::Type),           (uintptr_t)0x10);
    CHECK_EQ(SlotOffset(&Tool::Persist),        (uintptr_t)0x18);
    CHECK_EQ(SlotOffset(&Tool::Startup),        (uintptr_t)0x20);
    CHECK_EQ(SlotOffset(&Tool::Shutdown),       (uintptr_t)0x28);
    CHECK_EQ(SlotOffset(&Tool::OnKey),          (uintptr_t)0x30);
    CHECK_EQ(SlotOffset(&Tool::Update),         (uintptr_t)0x38);
    CHECK_EQ(SlotOffset(&Tool::Draw),           (uintptr_t)0x40);
    CHECK_EQ(SlotOffset(&Tool::OnCancel),       (uintptr_t)0x48);
    CHECK_EQ(SlotOffset(&Tool::IsSelected),     (uintptr_t)0x50);
    CHECK_EQ(SlotOffset(&Tool::GetSettingType), (uintptr_t)0x58);
    CHECK_EQ(SlotOffset(&Tool::GetCursor),      (uintptr_t)0x60);   // 13th and last
    // DragTool 0x140161c10, 15 slots.
    CHECK_EQ(SlotOffset(&DragTool::DeletingDtor), (uintptr_t)0x00);   // 0x14002da20
    CHECK_EQ(SlotOffset(&DragTool::OnKey),        (uintptr_t)0x30);   // 0x1400fc6b0
    CHECK_EQ(SlotOffset(&DragTool::OnCancel),     (uintptr_t)0x48);   // 0x1400fc640
    CHECK_EQ(SlotOffset(&DragTool::GetCursor),    (uintptr_t)0x60);   // 0x1400fc9d0
    CHECK_EQ(SlotOffset(&DragTool::CalcPath),     (uintptr_t)0x68);   // pure
    CHECK_EQ(SlotOffset(&DragTool::CalcCost),     (uintptr_t)0x70);   // pure, 15th and last
}

UNIT_TEST(tool_base_shape) {
    CHECK(std::is_abstract<Tool>::value);       // 4-9 pure
    CHECK(std::is_abstract<DragTool>::value);   // 4, 5, 7, 8, 13, 14 pure
    CHECK(!std::has_virtual_destructor<Tool>::value);
    CHECK(!std::has_virtual_destructor<DragTool>::value);
    CHECK((std::is_base_of<Object::Internal, Tool>::value));
    CHECK((std::is_base_of<Tool, DragTool>::value));
}

// ===========================================================================
// PART D — the hidden-return-slot shape, raw retail-order call (B8-11).
// A local class with Tool's slot-11 declaration shape: retail passes `this`
// in RCX and the return slot in RDX and reads the result from RAX.
// ===========================================================================
struct RetslotProbe {
    uint32_t tag = 0x5a5a;
    virtual Object::Type* GetSettingType(Object::Type* ret) const {
        *ret = Object::Type(tag);
        return ret;
    }
};

UNIT_TEST(tool_retslot_raw_call_order) {
    RetslotProbe probe;
    void** vt = *reinterpret_cast<void***>(&probe);
    using RawFn = Object::Type* (*)(const void* self, Object::Type* ret);
    Object::Type out(0);
    Object::Type* got = reinterpret_cast<RawFn>(vt[0])(&probe, &out);
    CHECK(got == &out);
    CHECK_EQ(static_cast<uint32_t>(out), 0x5a5au);
}

}  // namespace
