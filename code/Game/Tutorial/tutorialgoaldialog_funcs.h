#pragma once
// Function declarations for deeply reimplemented TutorialGoalDialog methods.
// Called from engine/reimpl/tutorialgoaldialog.cpp wrappers.

#include <cstdint>

// ============================================================
// TutorialGoalDialog (0x40 bytes), vtable at 0x140160870.
// Extends Object::External (NOT DialogInstance).
// Object::External base ~0x20 bytes, subclass fields start at +0x20.
//
// Fields:
//   +0x20: System::String dialogName
//   +0x28: System::String targetEntityName
//   +0x30: int32_t targetIndex (-1 = none)
//   +0x38: void* targetDescription
//
// Vtable layout (10 slots):
//   +0x00: ScalarDtor (hookable)
//   +0x08: GetTypeInfo (unhookable)
//   +0x10: GetTypeId (unhookable)
//   +0x18: Slot3: returns 1 (unhookable)
//   +0x20: Serialize thunk (unhookable)
//   +0x28: Shutdown/Serialize (hookable)
//   +0x30: OnEvent/Cleanup (hookable)
//   +0x38: Delegator (unhookable)
//   +0x40: OnInit (hookable)
//   +0x48: OnDestroy (hookable)
// ============================================================

// Slot0 (vtable +0x00, 0x140027590): Scalar deleting destructor.
// Sets TutorialGoalDialog vtable, destroys strings at +0x28 and +0x20,
// sets External base vtable, calls External::~External, conditionally frees.
void* TutorialGoalDialog_ScalarDtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400df080): Returns constant 1.
inline int TutorialGoalDialog_Slot3(void* /*self*/) { return 1; }

// Slot5 (vtable +0x28, 0x1400defd0): Shutdown/Serialize.
// Creates Serializer, serializes dialog name (+0x20) and target entity (+0x28).
// On load (mode==0), resolves external reference by name.
uint64_t TutorialGoalDialog_Shutdown(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140027520): OnEvent/Cleanup.
// Destructs and reconstructs the object in place.
void* TutorialGoalDialog_OnEvent(void* self, uint64_t p2, uint64_t p3, uint64_t p4);

// Slot7 (vtable +0x38, 0x140001640): Delegator.
// Calls vtable slot 0 (scalar dtor) with flags=0.
void TutorialGoalDialog_Slot7(void* self);

// Slot8 (vtable +0x40, 0x1400deee0): OnInit.
// Finds dialog by name, checks user data against target description.
uint64_t TutorialGoalDialog_OnInit(void* self);

// Slot9 (vtable +0x48, 0x1400274b0): OnDestroy.
// Reinitializes to default state.
void* TutorialGoalDialog_OnDestroy(void* self);
