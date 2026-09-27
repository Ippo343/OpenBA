#pragma once
// Function declarations for deeply reimplemented StateMachineComponent methods.
// Called from engine/reimpl/statemachinecomponent.cpp wrappers.

#include "StateMachineComponent.h"
#include <cstdint>

// ============================================================
// StateMachineComponent -- state machine tick dispatcher
// Vtable: 0x140164520 (16 slots)
// Size: 0x58 bytes
// ============================================================

// Slot 0 (ScalarDtor 0x1400384a0) and slot 2 (Type 0x1400382b0) are the
// class's members now (ENDGAME B.3.8 VIS, StateMachineComponent.h).

// Serialize (vtable slot [4], 0x14012b6a0):
// Post-deserialization re-linking: iterates entity's components, finds
// ControllerComponents via IsDerivedFrom, calls virtual +0x80 (GetDefaultState)
// and +0x88 (CreateState) to build the state chain, linking them via the
// controller sub-object at +0x28.
void StateMachineComponent_Serialize(StateMachineComponent* self);

// Draw (vtable slot [6], 0x14012b660):
// Constructs Time(0.0) and calls StateMachine_Tick(&controllerSelf, 0, false, false).
// Used for visual-only state machine updates (no gameplay tick).
void StateMachineComponent_Draw(StateMachineComponent* self);

// OnRemove (vtable slot [8], 0x14012b640):
// Calls StateMachine_Tick(&controllerSelf, deltaTime, false, true).
// The RDX parameter is forwarded from the caller (implicit register).
void StateMachineComponent_OnRemove(StateMachineComponent* self, int64_t deltaTime);

// Update (vtable slot [9], 0x14012b620):
// Calls StateMachine_Tick(&controllerSelf, deltaTime, true, true).
// Full gameplay tick -- processes both transitions and actions.
void StateMachineComponent_Update(StateMachineComponent* self, int64_t deltaTime);

// OnUpdate2 (vtable slot [11], 0x14012b7d0):
// Same component iteration as Serialize (re-links ControllerComponents),
// then walks the state machine linked list to find the tail node and
// dispatches a virtual call through slot [0] of each state's vtable
// with the stateFlags byte at +0x50 as argument.
void StateMachineComponent_OnUpdate2(StateMachineComponent* self);
