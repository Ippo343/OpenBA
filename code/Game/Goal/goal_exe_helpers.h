#pragma once
// Declarations for Goal-module exe-helper reimplementations.
// See code/Game/Goal/goal_exe_helpers.cpp.

#include <cstdint>

// FUN_1400265b0 — StatGoal lazy ExternalPtr resolver. void fn(externalRef).
void StatGoal_ResolveExternalPtr(void* externalRef);

// FUN_14001b340 - ExternalPtr-triple array resize (stride 0x18). void fn(arr, count).
void Goal_RefArray_Resize(void* arr, uint32_t newCount);

// Fill the three vtable slots StatGoal / PopulationGoal / BuildingGoal INHERIT from
// Achievement::Goal rather than override. Retail puts a JMP thunk in each:
//   [4] +0x20 -> import 0x140151240  Object::External::Compile
//   [7] +0x38 -> exe 0x140001640     `mov rax,[rcx]; xor edx,edx; jmp [rax]`
//                                    (the object's own dtor slot, flags = 0)
//   [9] +0x48 -> import 0x140152848  Achievement::Goal::GetInstanceType
// Our three arrays carried nullptr in all three, which was invisible for as long as
// nothing built an achievement GoalList: only the tutorial/scenario goals ran, and
// those are TimeGoal / FinishTutorialGoal, which override slot 9 themselves. The
// moment AchievementManager::Reset started creating the 36 instances,
// GoalList::Initialize asked a StatGoal for its instance type through slot 9 and
// executed address 0. Call this from each Register*Vtable().
void Goal_FillInheritedVtableSlots(void** vtable, const char* who);
