#pragma once
// statemachine_funcs.h -- Declarations for StateMachine_Tick reimplementation.
// Source: Game/Game/StateMachine.cpp (from profile build debug strings)
//
// StateMachine_Tick is the core function that drives all citizen AI in Banished.
// Called from StateMachineComponent::Update as:
//   fn_StateMachine_Tick(&self->controllerSelf, deltaTime, true, true)
//
// The first parameter is the ADDRESS of the controllerSelf field at offset +0x48
// of StateMachineComponent. This forms a small "tick context" struct that overlaps
// the end of the StateMachineComponent layout:
//
//   Component offset | Tick offset | Field            | Init (from 0xFF00FFFF)
//   +0x48            | +0x00       | headNode ptr     | ptr to controller sub-obj (+0x28)
//   +0x50            | +0x08       | currentState     | 0xFF (-1, signed)
//   +0x51            | +0x09       | subState         | 0xFF (-1, signed)
//   +0x52            | +0x0a       | pendingState     | 0x00 (triggers initial transition)
//   +0x53            | +0x0b       | exitState        | 0xFF (-1, signed)
//
// The headNode points into a linked list of "behavior" nodes. Each node has:
//   +0x00: vtable pointer
//   +0x08: next pointer (singly linked list for sibling traversal)
//   +0x10: child pointer (for action stack depth traversal)
//
// Behavior node vtable slots used by Tick:
//   [0] +0x00: CanProcess(node, currentState) -> int  (state match check)
//   [3] +0x18: Enter(node, smPtr) -> bool             (enter new state)
//   [4] +0x20: CheckTransition(node, smPtr) -> bool   (check for state change)
//   [5] +0x28: OnStateChange(node, smPtr, fromSubState, toExitState) -> void
//   [6] +0x30: Update(node, smPtr, deltaTime) -> bool (tick without transitions)
//   [7] +0x38: UpdateFull(node, smPtr, deltaTime) -> bool (tick with transitions)
//
// Original address: 0x1400a9660 in Application-x64.exe
// Decompiled from Ghidra, verified against disassembly.

#include <cstdint>

// ============================================================
// StateMachine tick context struct (overlaps StateMachineComponent +0x48..+0x57)
// ============================================================
struct StateMachineTickContext {
    void*   headNode;       // +0x00: pointer to first behavior node in linked list
    int8_t  currentState;   // +0x08: active state index (-1 = inactive)
    int8_t  subState;       // +0x09: sub-state within current behavior
    int8_t  pendingState;   // +0x0a: next state to transition to (-1 = none)
    int8_t  exitState;      // +0x0b: exit sub-state for OnStateChange (-1 = none)
    uint8_t _pad0c[4];      // +0x0c: padding to 0x10
};

static_assert(sizeof(StateMachineTickContext) == 0x10, "StateMachineTickContext size mismatch");
static_assert(offsetof(StateMachineTickContext, headNode)     == 0x00, "");
static_assert(offsetof(StateMachineTickContext, currentState) == 0x08, "");
static_assert(offsetof(StateMachineTickContext, subState)     == 0x09, "");
static_assert(offsetof(StateMachineTickContext, pendingState) == 0x0a, "");
static_assert(offsetof(StateMachineTickContext, exitState)    == 0x0b, "");

// ============================================================
// Behavior node struct (linked list element with vtable dispatch)
// ============================================================
struct BehaviorNode {
    void*         vtable;   // +0x00: pointer to vtable
    BehaviorNode* next;     // +0x08: next sibling in linked list
    BehaviorNode* child;    // +0x10: child node (action stack depth)
    // ... additional fields vary by behavior type
};

static_assert(offsetof(BehaviorNode, vtable) == 0x00, "");
static_assert(offsetof(BehaviorNode, next)   == 0x08, "");
static_assert(offsetof(BehaviorNode, child)  == 0x10, "");

// ============================================================
// Public API
// ============================================================

// StateMachine_Tick -- drives the state machine for one frame.
// Processes pending state transitions, then ticks the active behavior.
//
// Parameters:
//   ctx:                pointer to StateMachineTickContext (NOT the component)
//   deltaTime:          engine time delta (int64_t ticks)
//   processTransitions: if true, call UpdateFull (vtable +0x38) which handles transitions
//   processActions:     if true, tick the active behavior after transitions settle
void StateMachine_Tick_Reimpl(StateMachineTickContext* ctx, int64_t deltaTime,
                              int8_t processTransitions, int8_t processActions);

// Initialize local function pointers (no-op currently, all dispatch is via vtable)
void StateMachine_InitFuncs();
