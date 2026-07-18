// code/Game/Game/statemachine.cpp
// Clean C++ reimplementation of StateMachine_Tick (0x1400a9660).
// Source: Game/Game/StateMachine.cpp (from profile build debug strings)
//
// This is THE core function that drives all citizen AI in Banished.
// Called every frame for every citizen from StateMachineComponent::Update.
//
// The function operates on a StateMachineTickContext struct (overlapping
// StateMachineComponent fields at +0x48..+0x57) and a linked list of
// BehaviorNode objects that implement the state machine's states.
//
// Algorithm summary:
//   1. OUTER LOOP: while pendingState >= 0 (there's a state transition to process)
//     a. TRANSITION PHASE: If currentState >= 0 (we're in a valid state):
//        - Walk the behavior node linked list, find the node that handles currentState
//          (via CanProcess vtable[0]). If found, call CheckTransition (vtable[4]).
//          If CheckTransition returns true, transition is consumed.
//        - If exitState >= 0, call OnStateChange (vtable[5]) on the head node.
//     b. STATE SWITCH: Set currentState = pendingState.
//        Find the deepest child node from head (traverse child chain to last).
//        Set head to that deepest child.
//        Call CanProcess on the new head with the new currentState.
//        Store result as subState.
//     c. ENTER PHASE: If subState < 0, pop up the chain (follow next pointers)
//        calling CanProcess until subState >= 0 or we run out of nodes.
//     d. POST-ENTER: Walk the behavior list from head, find handler for currentState
//        (via CanProcess), then call Enter (vtable[3]). Reset pendingState = -1
//        and exitState = -1.
//     e. If pendingState is still < 0 after Enter, process actions.
//   2. ACTION PHASE (if processActions is true and currentState >= 0):
//      Walk the behavior list, find handler for currentState.
//      If processTransitions: call UpdateFull (vtable[7]) with deltaTime.
//      Else: call Update (vtable[6]) with deltaTime.
//      Only runs once (processActions cleared after first execution).
//   3. Loop back to check pendingState again (Enter or Update may set a new one).
//
// Decompiled from Application-x64.exe using Ghidra, verified against disassembly.

#include "Game/statemachine_funcs.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// BehaviorNode vtable dispatch helpers
//
// These inline functions call through the behavior node's vtable.
// Each corresponds to a virtual method slot used by Tick.
// ============================================================

// vtable[0] (+0x00): CanProcess(node, stateIndex) -> int
// Returns the sub-state index if this node handles the given state, or -1.
static inline int BehaviorNode_CanProcess(BehaviorNode* node, int stateIndex) {
    typedef int (*fn_t)(BehaviorNode* self, int stateIndex);
    void** vt = (void**)node->vtable;
    return ((fn_t)vt[0])(node, stateIndex);
}

// vtable[3] (+0x18): Enter(node, ctx) -> bool
// Called when transitioning into a new state. Returns true if the node
// accepted the transition (which may set a new pendingState).
static inline bool BehaviorNode_Enter(BehaviorNode* node, StateMachineTickContext* ctx) {
    typedef char (*fn_t)(BehaviorNode* self, StateMachineTickContext* ctx);
    void** vt = (void**)node->vtable;
    return ((fn_t)vt[3])(node, ctx) != 0;
}

// vtable[4] (+0x20): CheckTransition(node, ctx) -> bool
// Checks if the current state wants to trigger a transition.
static inline bool BehaviorNode_CheckTransition(BehaviorNode* node, StateMachineTickContext* ctx) {
    typedef char (*fn_t)(BehaviorNode* self, StateMachineTickContext* ctx);
    void** vt = (void**)node->vtable;
    return ((fn_t)vt[4])(node, ctx) != 0;
}

// vtable[5] (+0x28): OnStateChange(node, ctx, fromSubState, toExitState)
// Notification that the state is changing. Used for cleanup.
static inline void BehaviorNode_OnStateChange(BehaviorNode* node, StateMachineTickContext* ctx,
                                               int fromSubState, int toExitState) {
    typedef void (*fn_t)(BehaviorNode* self, StateMachineTickContext* ctx,
                         int fromSubState, int toExitState);
    void** vt = (void**)node->vtable;
    ((fn_t)vt[5])(node, ctx, fromSubState, toExitState);
}

// vtable[6] (+0x30): Update(node, ctx, deltaTime) -> bool
// Tick without transition processing. Returns true if the behavior consumed the tick.
static inline bool BehaviorNode_Update(BehaviorNode* node, StateMachineTickContext* ctx,
                                        int64_t deltaTime) {
    typedef char (*fn_t)(BehaviorNode* self, StateMachineTickContext* ctx, int64_t deltaTime);
    void** vt = (void**)node->vtable;
    return ((fn_t)vt[6])(node, ctx, deltaTime) != 0;
}

// vtable[7] (+0x38): UpdateFull(node, ctx, deltaTime) -> bool
// Tick with full transition processing. Returns true if the behavior consumed the tick.
static inline bool BehaviorNode_UpdateFull(BehaviorNode* node, StateMachineTickContext* ctx,
                                            int64_t deltaTime) {
    typedef char (*fn_t)(BehaviorNode* self, StateMachineTickContext* ctx, int64_t deltaTime);
    void** vt = (void**)node->vtable;
    return ((fn_t)vt[7])(node, ctx, deltaTime) != 0;
}

// ============================================================
// Helper: find the behavior node that handles a given state
//
// Walks the linked list starting from 'node'. For each node,
// if subState >= 0 (i.e., we have a valid sub-state from the previous
// CanProcess), we try calling the target vtable method. If it returns
// true, we're done. Otherwise, advance to next and call CanProcess
// to get a new sub-state for the new node.
//
// This pattern appears 4 times in Tick with different vtable calls.
// We factor out the iteration pattern and use a callback.
// ============================================================

// Callback type for the "find and dispatch" pattern.
// Returns true if the dispatch was handled (stop iterating).
typedef bool (*DispatchFn)(BehaviorNode* node, StateMachineTickContext* ctx, int64_t param);

// ============================================================
// StateMachine_Tick_Reimpl
// ============================================================
void StateMachine_Tick_Reimpl(StateMachineTickContext* ctx, int64_t deltaTime,
                              int8_t processTransitions, int8_t processActions) {
    // If no pending state transition, skip directly to action phase
    if (ctx->pendingState < 0) {
        goto action_phase;
    }

    // --- OUTER LOOP: process state transitions until pendingState < 0 ---
    do {
        // --- Inner transition loop ---
        do {
            // --- TRANSITION PHASE ---
            // If currentState >= 0, we're in a valid state that needs transition handling
            if (ctx->currentState >= 0) {
                int subState = (int)ctx->subState;
                BehaviorNode* node = (BehaviorNode*)ctx->headNode;

                // Walk the behavior list to find the handler for currentState,
                // then call CheckTransition (vtable[4] / +0x20)
                while (true) {
                    if (subState >= 0) {
                        // This node handles the current state -- check for transition
                        if (BehaviorNode_CheckTransition(node, ctx)) {
                            break; // transition was consumed
                        }
                    }
                    // Move to next node
                    node = node->next;
                    if (!node) break;
                    // Ask next node if it can process the current state
                    subState = BehaviorNode_CanProcess(node, (int)ctx->currentState);
                }

                // If exitState >= 0, notify head node about the state change
                if (ctx->exitState >= 0) {
                    BehaviorNode* head = (BehaviorNode*)ctx->headNode;
                    BehaviorNode_OnStateChange(head, ctx,
                                               (int)ctx->subState,
                                               (int)ctx->exitState);
                }
            }

            // --- STATE SWITCH ---
            // Set currentState = pendingState
            ctx->currentState = ctx->pendingState;

            // Find the deepest child node from the current head.
            // Traverse the child chain (+0x10) to the last node.
            BehaviorNode* head = (BehaviorNode*)ctx->headNode;
            BehaviorNode* deepest = head;
            BehaviorNode* childIter = head->child;
            while (childIter) {
                deepest = childIter;
                childIter = childIter->child;
            }
            // Set head to the deepest child
            ctx->headNode = deepest;

            // Call CanProcess on the new head with the new currentState
            int8_t newSubState = (int8_t)BehaviorNode_CanProcess(deepest, (int)ctx->currentState);
            ctx->subState = newSubState;

            // --- POP-UP PHASE ---
            // If subState < 0, this node can't handle the state.
            // Pop up the chain via next pointers until we find one that can.
            while (newSubState < 0) {
                BehaviorNode* current = (BehaviorNode*)ctx->headNode;
                BehaviorNode* parent = current->next;
                ctx->headNode = parent;
                newSubState = (int8_t)BehaviorNode_CanProcess(parent, (int)ctx->currentState);
                ctx->subState = newSubState;
            }

            // --- ENTER PHASE ---
            // Walk the behavior list, find handler for currentState,
            // then call Enter (vtable[3] / +0x18)
            {
                int subState = (int)ctx->subState;
                BehaviorNode* node = (BehaviorNode*)ctx->headNode;

                // Reset pendingState and exitState to -1 (0xFF each = 0xFFFF as word)
                ctx->pendingState = -1;
                ctx->exitState = -1;

                while (true) {
                    if (subState >= 0) {
                        // This node handles the state -- call Enter
                        if (BehaviorNode_Enter(node, ctx)) {
                            break; // Enter was handled
                        }
                    }
                    // Move to next node
                    node = node->next;
                    if (!node) break;
                    // Ask next node if it can process the current state
                    subState = BehaviorNode_CanProcess(node, (int)ctx->currentState);
                }
            }

            // If Enter set a new pendingState, loop back to process it
        } while (ctx->pendingState >= 0);

        // --- ACTION PHASE ---
action_phase:
        if (processActions && ctx->currentState >= 0) {
            int subState = (int)ctx->subState;
            BehaviorNode* node = (BehaviorNode*)ctx->headNode;

            if (!processTransitions) {
                // No transitions: call Update (vtable[6] / +0x30)
                while (true) {
                    if (subState >= 0) {
                        if (BehaviorNode_Update(node, ctx, deltaTime)) {
                            break;
                        }
                    }
                    node = node->next;
                    if (!node) break;
                    subState = BehaviorNode_CanProcess(node, (int)ctx->currentState);
                }
            } else {
                // With transitions: call UpdateFull (vtable[7] / +0x38)
                while (true) {
                    if (subState >= 0) {
                        if (BehaviorNode_UpdateFull(node, ctx, deltaTime)) {
                            break;
                        }
                    }
                    node = node->next;
                    if (!node) break;
                    subState = BehaviorNode_CanProcess(node, (int)ctx->currentState);
                }
            }

            // Only run actions once per Tick call
            processActions = 0;
        }

        // If an Update/Enter callback set a new pendingState, loop again
    } while (ctx->pendingState >= 0);
}

// ============================================================
// StateMachine_InitFuncs -- resolve dependencies
// Currently all dispatch is via vtable on behavior nodes, so nothing to resolve.
// ============================================================
void StateMachine_InitFuncs() {
    // No external function pointers needed -- all calls go through behavior vtables.
    // This function exists for future use if we need to resolve helper functions.
#ifdef _DEBUG
    printf("[StateMachine] InitFuncs complete (no external deps)\n");
#endif
}
