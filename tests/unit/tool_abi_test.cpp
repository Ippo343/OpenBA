// ============================================================================
// tests/unit/tool_abi_test.cpp — ENDGAME B.3.8 block TOOL (B38_RULES.md B8-14)
//
// The ABI claims block TOOL makes about the exe Tool family, measured on the
// SHIPPED HEADERS (code/Game/Player/*.h):
//   PART A — LAYOUT: sizeof / alignof / member offsets against retail's create
//            MemAlloc sizes and ctor stores (B38_SLOTS.md §6).
//   PART B — SLOT ORDER, read off pointers to VIRTUAL members (Itanium PMF =
//            1 + vtable byte offset; adjustment must be 0: single inheritance).
//   PART C — class shape: abstractness, a virtual destructor (B3X W1: retail slot 0 = D1 + D0).
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
#include "slot_check.h"

#include "Player/ToolSetting.h"
#include "Player/RotationSetting.h"
#include "Player/Tool.h"
#include "Player/dragtool.h"
#include "Player/BridgeTool.h"
#include "Player/DragPlacementTool.h"
#include "Player/EditTool.h"
#include "Player/FencedTool.h"
#include "Player/PathTool.h"
#include "Player/PlacementTool.h"
#include "Player/RemovalTool.h"
#include "Player/RoadTool.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

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
    CHECK_DTOR_SLOT(ToolSetting);
    CHECK_SLOT(ToolSetting, TypeName, 0x08/8);
    CHECK_SLOT(ToolSetting, Type,     0x10/8);
    CHECK_SLOT(ToolSetting, Persist,  0x18/8);   // inherited Internal::Persist
    CHECK_DTOR_SLOT(RotationSetting);   // 0x140026dd0
    // (slots 1/2 of a leaf are DECLARE_EXE_TYPE's inline bodies over the
    // registration record, which does not link into a host test.)
}

UNIT_TEST(tool_setting_shape) {
    CHECK(std::is_polymorphic<ToolSetting>::value);
    CHECK(std::has_virtual_destructor<ToolSetting>::value);
    CHECK(std::has_virtual_destructor<RotationSetting>::value);
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
    CHECK_DTOR_SLOT(Tool);
    CHECK_SLOT(Tool, TypeName,       0x08/8);
    CHECK_SLOT(Tool, Type,           0x10/8);
    CHECK_SLOT(Tool, Persist,        0x18/8);
    CHECK_SLOT(Tool, Startup,        0x20/8);
    CHECK_SLOT(Tool, Shutdown,       0x28/8);
    CHECK_SLOT(Tool, OnKey,          0x30/8);
    CHECK_SLOT(Tool, Update,         0x38/8);
    CHECK_SLOT(Tool, Draw,           0x40/8);
    CHECK_SLOT(Tool, OnCancel,       0x48/8);
    CHECK_SLOT(Tool, IsSelected,     0x50/8);
    CHECK_SLOT(Tool, GetSettingType, 0x58/8);
    CHECK_SLOT(Tool, GetCursor,      0x60/8);   // 13th and last
    // DragTool 0x140161c10, 15 slots.
    CHECK_DTOR_SLOT(DragTool);   // 0x14002da20
    CHECK_SLOT(DragTool, OnKey,     0x30/8);   // 0x1400fc6b0
    CHECK_SLOT(DragTool, OnCancel,  0x48/8);   // 0x1400fc640
    CHECK_SLOT(DragTool, GetCursor, 0x60/8);   // 0x1400fc9d0
    CHECK_SLOT(DragTool, CalcPath,  0x68/8);   // pure
    CHECK_SLOT(DragTool, CalcCost,  0x70/8);   // pure, 15th and last
}

UNIT_TEST(tool_base_shape) {
    CHECK(std::is_abstract<Tool>::value);       // 4-9 pure
    CHECK(std::is_abstract<DragTool>::value);   // 4, 5, 7, 8, 13, 14 pure
    CHECK(std::has_virtual_destructor<Tool>::value);
    CHECK(std::has_virtual_destructor<DragTool>::value);
    CHECK((std::is_base_of<Object::Internal, Tool>::value));
    CHECK((std::is_base_of<Tool, DragTool>::value));
}

// ===========================================================================
// The nine leaves
// ===========================================================================
UNIT_TEST(tool_leaf_sizes_match_retail_create) {
    CHECK_EQ(sizeof(BridgeTool),        (size_t)0x40);   // 0x14002d7a0
    CHECK_EQ(sizeof(DragPlacementTool), (size_t)0x48);   // 0x14002d8b0
    CHECK_EQ(sizeof(EditTool),          (size_t)0x38);   // 0x14002daa0
    CHECK_EQ(sizeof(FencedTool),        (size_t)0x70);   // 0x1400268d0
    CHECK_EQ(sizeof(PathTool),          (size_t)0x60);   // 0x140026b10
    CHECK_EQ(sizeof(PlacementTool),     (size_t)0x70);   // 0x14002dc40
    CHECK_EQ(sizeof(RemovalTool),       (size_t)0x40);   // 0x14002e7a0
    CHECK_EQ(sizeof(RoadTool),          (size_t)0x48);   // 0x14002e870
    CHECK_EQ(offsetof(BridgeTool, bridgeDesc),           (size_t)0x38);
    CHECK_EQ(offsetof(DragPlacementTool, areaDesc),      (size_t)0x40);
    CHECK_EQ(offsetof(EditTool, lastEntityRef),          (size_t)0x30);
    CHECK_EQ(offsetof(FencedTool, sceneObjectId),        (size_t)0x58);
    CHECK_EQ(offsetof(FencedTool, meshInstance),         (size_t)0x68);
    CHECK_EQ(offsetof(PathTool, selectedEntity),         (size_t)0x28);
    CHECK_EQ(offsetof(PathTool, _field58),               (size_t)0x58);
    CHECK_EQ(offsetof(PlacementTool, sceneObjectIdx),    (size_t)0x60);
    CHECK_EQ(offsetof(PlacementTool, meshInstance),      (size_t)0x68);
    CHECK_EQ(offsetof(RemovalTool, removalDesc),         (size_t)0x38);
    CHECK_EQ(offsetof(RoadTool, modifierFlag),           (size_t)0x40);
}

UNIT_TEST(tool_leaf_overrides_land_on_retail_slots) {
    CHECK_DTOR_SLOT(BridgeTool);   // 0x14002d7f0
    CHECK_SLOT(BridgeTool, Startup,           0x20/8);   // 0x1400fb7a0
    CHECK_SLOT(BridgeTool, Draw,              0x40/8);   // 0x1400fadc0
    CHECK_SLOT(BridgeTool, CalcPath,          0x68/8);   // 0x1400f9cd0
    CHECK_SLOT(BridgeTool, CalcCost,          0x70/8);   // 0x1400facf0
    CHECK_SLOT(RoadTool, OnKey,               0x30/8);   // 0x140105690
    CHECK_SLOT(FencedTool, Update,            0x38/8);   // 0x1400dcad0
    CHECK_SLOT(FencedTool, OnCancel,          0x48/8);   // 0x1400dcd10
    CHECK_SLOT(FencedTool, GetSettingType,    0x58/8); // 0x140026920
    CHECK_SLOT(PlacementTool, Shutdown,       0x28/8);   // 0x1400fe4e0
    CHECK_SLOT(PlacementTool, GetSettingType, 0x58/8);   // 0x140026920
    CHECK_SLOT(PlacementTool, GetCursor,      0x60/8);   // 0x1400fe800
    CHECK_SLOT(EditTool, Shutdown,            0x28/8);   // 0x1400fd2e0
    CHECK_SLOT(EditTool, IsSelected,          0x50/8);   // 0x1400fd290
    CHECK_SLOT(PathTool, IsSelected,          0x50/8);   // 0x1400de8d0
    CHECK_SLOT(PathTool, GetCursor,           0x60/8);   // 0x1400de910
    CHECK_SLOT(RemovalTool, CalcPath,         0x68/8);   // 0x1401035e0
    CHECK_SLOT(DragPlacementTool, CalcCost,   0x70/8);   // 0x1400fbae0
    // D9 #4: EditTool and PathTool have 14 slots in retail — their own slot 13.
    CHECK_SLOT(EditTool, GetSelected, 0x68/8);   // 0x14002daf0
    CHECK_SLOT(PathTool, GetSelected, 0x68/8);   // 0x140026b60
}

UNIT_TEST(tool_leaf_shape) {
    CHECK(!std::is_abstract<BridgeTool>::value);
    CHECK(!std::is_abstract<DragPlacementTool>::value);
    CHECK(!std::is_abstract<EditTool>::value);
    CHECK(!std::is_abstract<FencedTool>::value);
    CHECK(!std::is_abstract<PathTool>::value);
    CHECK(!std::is_abstract<PlacementTool>::value);
    CHECK(!std::is_abstract<RemovalTool>::value);
    CHECK(!std::is_abstract<RoadTool>::value);
    CHECK(std::has_virtual_destructor<BridgeTool>::value);
    CHECK(std::has_virtual_destructor<EditTool>::value);
    CHECK(std::has_virtual_destructor<PathTool>::value);
    CHECK(std::has_virtual_destructor<FencedTool>::value);
    CHECK((std::is_base_of<DragTool, BridgeTool>::value));
    CHECK((std::is_base_of<DragTool, DragPlacementTool>::value));
    CHECK((std::is_base_of<DragTool, FencedTool>::value));
    CHECK((std::is_base_of<DragTool, RemovalTool>::value));
    CHECK((std::is_base_of<DragTool, RoadTool>::value));
    CHECK(!(std::is_base_of<DragTool, EditTool>::value));
    CHECK(!(std::is_base_of<DragTool, PathTool>::value));
    CHECK(!(std::is_base_of<DragTool, PlacementTool>::value));
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
