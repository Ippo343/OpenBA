#pragma once
// Tutorial Goal types — binary-compatible struct layouts.
// Application-x64.exe, build 170910.
//
// TutorialGoal base (vtable 0x1401607a0) is abstract.
// All tutorial goals inherit from Object::External -> TutorialGoal -> [concrete].
//
// Vtable layout (shared across all tutorial goals, 10 slots):
//   +0x00: ScalarDtor(self, flags)
//   +0x08: GetTypeInfo (unhookable)
//   +0x10: GetTypeId (unhookable)
//   +0x18: GetDescription (unhookable, returns constant)
//   +0x20: Serialize thunk (unhookable)
//   +0x28: Serialize(self, stream)
//   +0x30: Cleanup(self)
//   +0x38: Base dispatch (unhookable)
//   +0x40: Check(self, ...) — goal completion check
//   +0x48: Init(self) — called from factory after construction

#include <cstdint>
#include <cstddef>

namespace TutorialGoalAddr {
    constexpr uintptr_t Vtable       = 0x1401607a0;
    constexpr uintptr_t RegisterType = 0x14014d790;
    constexpr uintptr_t Dtor         = 0x140027220;
    constexpr uintptr_t Serialize    = 0x1400dec30;
}

// TutorialGoalButton — wait for UI element state (button/checkbox).
// Size = 0x30.
// Decompiled from: TutorialGoalButton_Factory (0x140027290, inline ctor)
namespace TutorialGoalButtonAddr {
    constexpr uintptr_t Factory      = 0x140027290;
    constexpr uintptr_t Vtable       = 0x140160800;
    constexpr uintptr_t RegisterType = 0x14014d7d0;
    constexpr uintptr_t Dtor         = 0x1400273b0;
    constexpr uintptr_t Serialize    = 0x1400deda0;
    constexpr uintptr_t Cleanup      = 0x140027350;
    constexpr uintptr_t Check        = 0x1400dece0;
    constexpr uintptr_t Init         = 0x140027300;
}

struct TutorialGoalButton {
    void*    vtable;             // +0x00: -> 0x140160800
    uint8_t  _extBase[0x18];     // +0x08: External base (Object::External fields)
    uint64_t elementPath;        // +0x20: System::String — UI element path to find
    uint8_t  invertCheck;        // +0x28: bool — if true, check bit 23 inverted; else check bit 22
    uint8_t  _pad29[7];          // +0x29: padding
};
static_assert(sizeof(TutorialGoalButton) == 0x30, "TutorialGoalButton size mismatch");
static_assert(offsetof(TutorialGoalButton, elementPath) == 0x20, "elementPath offset");
static_assert(offsetof(TutorialGoalButton, invertCheck) == 0x28, "invertCheck offset");

// TutorialGoalGameSpeed — wait for game speed to match target. Size = 0x28.
// Decompiled from: TutorialGoalGameSpeed_Factory (0x140027630, inline ctor)
namespace TutorialGoalGameSpeedAddr {
    constexpr uintptr_t Factory      = 0x140027630;
    constexpr uintptr_t Vtable       = 0x1401608e0;
    constexpr uintptr_t Dtor         = 0x140027730;
    constexpr uintptr_t Serialize    = 0x1400df170;
    constexpr uintptr_t Cleanup      = 0x1400276e0;
    constexpr uintptr_t Check        = 0x1400df150;
    constexpr uintptr_t Init         = 0x140027690;
}

struct TutorialGoalGameSpeed {
    void*    vtable;             // +0x00: -> 0x1401608e0
    uint8_t  _extBase[0x18];     // +0x08: External base
    int32_t  targetSpeed;        // +0x20: target game speed (default 1)
    uint8_t  _pad24[4];          // +0x24: padding
};
static_assert(sizeof(TutorialGoalGameSpeed) == 0x28, "TutorialGoalGameSpeed size mismatch");
static_assert(offsetof(TutorialGoalGameSpeed, targetSpeed) == 0x20, "targetSpeed offset");

// TutorialGoalCount — wait for item/building/citizen count. Size = 0x50.
// Decompiled from: TutorialGoalCount_Ctor (0x1400dfa60)
// The countMode field (+0x20) selects what to count (switch with 11 cases).
namespace TutorialGoalCountAddr {
    constexpr uintptr_t Ctor         = 0x1400dfa60;
    constexpr uintptr_t Factory      = 0x1400277b0;
    constexpr uintptr_t Vtable       = 0x140160950;
    constexpr uintptr_t Dtor         = 0x140027820;
    constexpr uintptr_t Serialize    = 0x1400df900;
    constexpr uintptr_t CleanupSlot  = 0x140027800;
    constexpr uintptr_t Check        = 0x1400df290;
    constexpr uintptr_t InitSlot     = 0x1400277f0;
}

struct TutorialGoalCount {
    void*    vtable;             // +0x00: -> 0x140160950
    uint8_t  _extBase[0x18];     // +0x08: External base
    uint32_t countMode;          // +0x20: count mode (0-10, selects what to count)
    int32_t  targetCount;        // +0x24: required count to complete goal
    uint8_t  greaterOrEqual;     // +0x28: bool — if true, count >= target; else count == target
    uint8_t  _pad29[7];          // +0x29: padding
    uint64_t typeName;           // +0x30: System::String — type/resource name for lookup
    int32_t  _field38;           // +0x38: -1 sentinel (unused in most modes?)
    uint8_t  _pad3C[4];          // +0x3C: padding
    void*    _field40;           // +0x40: ExternalPtr — type reference (building/profession)
    uint64_t elementName;        // +0x48: System::String — UI element path (mode 9)
};
static_assert(sizeof(TutorialGoalCount) == 0x50, "TutorialGoalCount size mismatch");
static_assert(offsetof(TutorialGoalCount, countMode) == 0x20, "countMode offset");
static_assert(offsetof(TutorialGoalCount, targetCount) == 0x24, "targetCount offset");
static_assert(offsetof(TutorialGoalCount, greaterOrEqual) == 0x28, "greaterOrEqual offset");
static_assert(offsetof(TutorialGoalCount, typeName) == 0x30, "typeName offset");
static_assert(offsetof(TutorialGoalCount, _field40) == 0x40, "_field40 offset");
static_assert(offsetof(TutorialGoalCount, elementName) == 0x48, "elementName offset");
