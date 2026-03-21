#pragma once
// Function declarations for deeply reimplemented TutorialGoalCount methods.
// Called from engine/reimpl/tutorialgoalcount.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "TutorialGoals.h"
#include <cstdint>

// ============================================================
// TutorialGoalCount — wait for item/building/citizen count
// Vtable: 0x140160950 (10 slots)
// Size: 0x50 bytes
// Extends TutorialGoal -> Object::External
//
// Fields:
//   +0x20: uint32_t countMode       — selects what to count (0-10)
//   +0x24: int32_t  targetCount     — required count
//   +0x28: bool     greaterOrEqual  — if true, count >= target; else count == target
//   +0x30: System::String typeName  — type name for BST/profession lookup
//   +0x38: int32_t  (sentinel, -1)
//   +0x40: void*    ExternalPtr     — type reference (building/profession description)
//   +0x48: System::String elementName — UI element path (used in mode 9)
//
// Count modes (switch in Check/Slot8):
//   0: Building count from world BST (+0x160) by type
//   1: Citizen count by profession
//   2: Population count (world+0x28)
//   3: Profession count by type
//   4: Population count (world+0x08)
//   5: Has trade posts (world+0xF8 merchant list)
//   6: Stored resource count by profession type
//   7: Building storage total by building type (BST at +0x160, iterate buildings)
//   8: Total stored items across all buildings
//   9: UI element value (spinner)
//  10: Occupied building count by type
// ============================================================

// Slot0 (vtable +0x00, 0x140027820): Scalar deleting destructor.
// Destroys Strings at +0x48 and +0x30, then base TutorialGoal dtor.
void* TutorialGoalCount_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400df9f0): GetDescription — returns constant 5.
// Original is "mov eax,5; ret" (6 bytes, unhookable).
inline int TutorialGoalCount_GetDescription(void* /*self*/) { return 5; }

// Slot5 (vtable +0x28, 0x1400df900): Serialize.
// Serializes: ExternalPtr at +0x20, int at +0x24, String at +0x30,
//             bool at +0x28, String at +0x48.
void TutorialGoalCount_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140027800): Cleanup.
// Calls ScalarDtor(self, 0) via vtable, then calls Init (0x1400dfa60).
void TutorialGoalCount_Cleanup(void* self);

// Slot8 (vtable +0x40, 0x1400df290): Check — count items and compare to target.
// 11-case switch on countMode (+0x20):
//   0=buildings by type, 1=citizens by profession, 2=adult pop, 3=profession slots,
//   4=total pop, 5=has trade posts, 6=stored resources by profession, 7=building
//   storage by type, 8=total stored items, 9=UI spinner value, 10=occupied buildings.
// Returns 1 if goal met (count compared to targetCount per greaterOrEqual flag), 0 otherwise.
// Uses int return type (bug class 7).
// param3: InternalBasePtr* used only in case 9 (CheckPtr called on it).
int TutorialGoalCount_Check(void* self, void* param2, void* param3);

// Slot9/Init (vtable +0x48, wrapper at 0x1400277f0 -> 0x1400dfa60): Init.
// Calls External::External, sets vtable chain, zeroes all fields,
// sets targetCount to -1.
void* TutorialGoalCount_Init(void* self);
