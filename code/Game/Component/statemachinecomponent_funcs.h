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

// Every body is a member (StateMachineComponent.h): LinkControllers (retail Startup
// 0x14012b6a0, the reference port; not called), OnAdded 0x14012b660, Tick 0x14012b640,
// Update 0x14012b620, RelinkAndResume (the body of slot 11 0x14012b7d0, not installed),
// StateMachine::Tick 0x1400a9660.
