// engine/src/ui/destroyui.cpp
// Clean reimplementation of DestroyUI game logic.
//
// DestroyUI (0x78 bytes) — building destruction/reclaim panel.
// Extends DialogController. Fields +0x30..+0x70 store element pointers:
//   +0x30 progressRemove, +0x38 progressDestruct (InternalPtr<Element>)
//   +0x40 textRemove, +0x48 textUpgrade, +0x50 textDamage, +0x58 textFixing (InternalBasePtr)
//   +0x60 buttonReclaim, +0x68 buttonDestroy, +0x70 buttonRebuild (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// DestroyComponent_CanReclaim (0x14011e690)
// ============================================================
// Returns true if BuildState state byte is 6 (damaged) or 7.
// Reads state byte from buildState+0x52; if negative, uses +0x50.
static bool DestroyComponent_CanReclaim_Impl(void* destroyComp)
{
    if (!destroyComp) return false;

    // Get entity at +0x10
    void* entity = *(void**)((uintptr_t)destroyComp + 0x10);
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));

    // Find BuildState component on entity
    uint64_t bsRef = 0;
    fn_FindComponent_BuildState(entity, &bsRef);
    if (!bsRef) return false;

    fn_InternalBasePtr_CheckPtr((void*)&bsRef);
    uintptr_t buildState = bsRef;

    int8_t stateByte = *(int8_t*)(buildState + 0x52);
    if (stateByte < 0) {
        stateByte = *(int8_t*)(buildState + 0x50);
    }

    if (stateByte != 6) {
        // Check again (second pass from decompile)
        fn_InternalBasePtr_CheckPtr((void*)&bsRef);
        stateByte = *(int8_t*)(buildState + 0x52);
        if (stateByte < 0) stateByte = *(int8_t*)(buildState + 0x50);
        if (stateByte != 7) return false;
    }
    return true;
}

// ============================================================
// DestroyComponent_CanRebuild (0x14011e600)
// ============================================================
// Returns true if upgrade is pending (destroyComp+0x60 points to upgrade target).
// Checks that destroyComp+0x60 != entity.descState AND != desc+0x20.
//
// destroyComp+0x18 = description (checked as DAT_1401c7628 = g_TypeId_DestroyComponent)
// destroyComp+0x60 = upgrade description reference (int64/ptr)
// destroyComp+0x10 = entity (checked via CheckPtr)
static bool DestroyComponent_CanRebuild_Impl(void* destroyComp)
{
    if (!destroyComp) return false;

    // Check if description is of upgrade type
    void* desc = *(void**)((uintptr_t)destroyComp + 0x18);
    bool isType = fn_IsDerivedFrom(desc, *g_TypeId_DestroyComponent);
    void* typedDesc = isType ? desc : nullptr;

    // Also check separately (decompile does it twice)
    fn_IsDerivedFrom(*(void**)((uintptr_t)destroyComp + 0x18), *g_TypeId_DestroyComponent);

    int64_t upgradeRef = *(int64_t*)((uintptr_t)destroyComp + 0x60);

    // Get entity
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uintptr_t entity = *(uintptr_t*)((uintptr_t)destroyComp + 0x10);

    // Return: upgradeRef != entity.stateDesc (at entity+0x28) AND upgradeRef != desc+0x20
    if (upgradeRef == *(int64_t*)(entity + 0x28)) {
        return false;  // matches entity state
    }
    uintptr_t descUpgradeTarget = typedDesc ? *(uintptr_t*)((uintptr_t)typedDesc + 0x20) : 0;
    if ((uintptr_t)upgradeRef == descUpgradeTarget) {
        return false;  // matches description's upgrade target
    }
    return true;
}

// ============================================================
// DestroyComponent action helpers (common pattern)
// ============================================================
// Reclaim/Rebuild/Destroy all call the StateMachine at BuildState+0x48:
//   vtable[2](sm, sm_ref, current_state_byte, param)
// where param = 0 (reclaim/rebuild) or 1 (destroy).
//
// Reclaim (14011e400): also calls destroyComp->vtable[4](self)
// Rebuild (14011e470): finds BuildState on entity at destroyComp+0x10
// Destroy (14011e4d0): same but param=1

static void DestroyComp_CallBuildStateAction(void* destroyComp_entity_field, int last_param)
{
    // entity at +0x10 (passed as the entity ptr for FindBuildState)
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp_entity_field));

    // Find BuildState on entity
    uint64_t bsRef = 0;
    uintptr_t entity = *(uintptr_t*)destroyComp_entity_field;
    fn_FindComponent_BuildState((void*)entity, &bsRef);
    fn_InternalBasePtr_CheckPtr((void*)&bsRef);

    uintptr_t buildState = bsRef;
    int8_t stateFrom = *(int8_t*)(buildState + 0x51);  // current sub-state

    // Get StateMachine at buildState+0x48, call vtable[2]
    uintptr_t smPtrAddr = buildState + 0x48;
    uintptr_t smPtr = *(uintptr_t*)smPtrAddr;
    void** smVtable = *(void***)smPtr;

    typedef int (*fn_StateTransition_t)(uintptr_t sm, uintptr_t smRef, int fromState, int param);
    ((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (int)stateFrom, last_param);
}

static void DestroyComponent_Reclaim_Impl(void* destroyComp)
{
    if (!destroyComp) return;
    // Find BuildState on entity (param_1+0x10 in decompile, where param_1 is longlong*)
    // param_1[2] = *(param_1 + 0x10) = *(destroyComp + 0x10)
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uint64_t bsRef = 0;
    uintptr_t entity = *(uintptr_t*)((uintptr_t)destroyComp + 0x10);
    fn_FindComponent_BuildState((void*)entity, &bsRef);
    fn_InternalBasePtr_CheckPtr((void*)&bsRef);

    uintptr_t buildState = bsRef;
    int8_t stateFrom = *(int8_t*)(buildState + 0x51);
    uintptr_t smPtrAddr = buildState + 0x48;
    uintptr_t smPtr = *(uintptr_t*)smPtrAddr;
    void** smVtable = *(void***)smPtr;
    typedef int (*fn_StateTransition_t)(uintptr_t sm, uintptr_t smRef, int fromState, int param);
    ((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (int)stateFrom, 0);

    // Also call destroyComp->vtable[4](self) — the "Reclaim" notification
    void** selfVtable = *(void***)destroyComp;
    typedef void (*fn_SelfNotify_t)(void* self);
    ((fn_SelfNotify_t)selfVtable[4])(destroyComp);
}

static void DestroyComponent_Rebuild_Impl(void* destroyComp)
{
    if (!destroyComp) return;
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uint64_t bsRef = 0;
    uintptr_t entity = *(uintptr_t*)((uintptr_t)destroyComp + 0x10);
    fn_FindComponent_BuildState((void*)entity, &bsRef);
    fn_InternalBasePtr_CheckPtr((void*)&bsRef);

    uintptr_t buildState = bsRef;
    int8_t stateFrom = *(int8_t*)(buildState + 0x51);
    uintptr_t smPtrAddr = buildState + 0x48;
    uintptr_t smPtr = *(uintptr_t*)smPtrAddr;
    void** smVtable = *(void***)smPtr;
    typedef int (*fn_StateTransition_t)(uintptr_t sm, uintptr_t smRef, int fromState, int param);
    ((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (int)stateFrom, 0);
}

static void DestroyComponent_Destroy_Impl(void* destroyComp)
{
    if (!destroyComp) return;
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uint64_t bsRef = 0;
    uintptr_t entity = *(uintptr_t*)((uintptr_t)destroyComp + 0x10);
    fn_FindComponent_BuildState((void*)entity, &bsRef);
    fn_InternalBasePtr_CheckPtr((void*)&bsRef);

    uintptr_t buildState = bsRef;
    int8_t stateFrom = *(int8_t*)(buildState + 0x51);
    uintptr_t smPtrAddr = buildState + 0x48;
    uintptr_t smPtr = *(uintptr_t*)smPtrAddr;
    void** smVtable = *(void***)smPtr;
    typedef int (*fn_StateTransition_t)(uintptr_t sm, uintptr_t smRef, int fromState, int param);
    ((fn_StateTransition_t)smVtable[2])(smPtr, smPtrAddr, (int)stateFrom, 1);
}

// ============================================================
// StateMachine_Tick — resolved locally for StartUpgrade
// ============================================================
// Located at 0x1400a9660 in Application-x64.exe.
// Signature: (stateMachine*, Time*, bool processTransitions, bool processActions)
// NOTE: Second arg is a POINTER to Time (LEA RDX), not a Time value.
typedef void (*fn_StateMachine_Tick_Local_t)(void* stateMachine, void* timePtr,
                                             char param3, char param4);
static fn_StateMachine_Tick_Local_t g_StateMachine_Tick = nullptr;

// ============================================================
// State machine node vtable layout (linked list at BuildState+0x48)
// ============================================================
// Each node in the state machine linked list has:
//   vtable+0x00: CanHandle(node, fromState) -> int (state index, or -1 if can't handle)
//   vtable+0x08: (next pointer access, offset +0x08 = node->prev)
//   vtable+0x10: (linked list next, offset +0x10 = node->next)
//   vtable+0x18: HasExitHandler(node, smRef) -> bool
//   vtable+0x20: CanTransition(node, smRef) -> bool
//   vtable+0x28: DoTransition(node, smRef, fromState [, targetState]) -> void
//   vtable+0x30: OnFirstEntry(node, smRef [, time]) -> bool
//
// State bytes in BuildState:
//   +0x50: previousState (int8)
//   +0x51: currentSubState (int8) — returned by CanHandle
//   +0x52: targetState (int8) — -1 (0xFF) = needs processing
//   +0x53: transitionTarget (int8)

// ============================================================
// ProcessStateMachine — inner state machine transition loop
// ============================================================
// Runs the state machine through its transition cycle.
// This is the core loop that appears twice in StartUpgrade:
// once to exit the current state, and once to enter the new state.
//
// Parameters:
//   buildState: pointer to BuildState component
//   smPtrAddr: address of the state machine head pointer (BuildState+0x48)
//   isFirstCall: true on first invocation (triggers OnFirstEntry call)
//   time: optional Time value (only used with OnFirstEntry on second call)
static void ProcessStateMachine(uintptr_t buildState, uintptr_t* smPtrAddr,
                                 bool isFirstCall, void* time) {
    if (*(int8_t*)(buildState + 0x52) < 0) goto first_entry_check;

    do {
        do {
            // --- Exit current state if state is valid ---
            if (*(int8_t*)(buildState + 0x50) >= 0) {
                int subState = (int)*(int8_t*)(buildState + 0x51);
                uintptr_t node = *smPtrAddr;

                // Walk linked list to find handler that can handle current state
                // and has CanTransition returning true
                while (true) {
                    if (subState >= 0) {
                        // vtable+0x20: CanTransition(node, smRef, subState) -> bool
                        // NOTE: R8 carries subState as implicit 3rd arg (dispatch index).
                        typedef bool (*fn_CanTransition_t)(void* node, void* smRef, int subState);
                        bool canTrans = ((fn_CanTransition_t)(*(void**)(*(uintptr_t*)node + 0x20)))
                                        ((void*)node, (void*)smPtrAddr, subState);
                        if (canTrans) break;
                    }
                    // Move to next node in linked list (offset +0x08)
                    node = *(uintptr_t*)(node + 0x08);
                    if (node == 0) break;
                    // vtable+0x00: CanHandle(node, fromState) -> int
                    typedef int (*fn_CanHandle_t)(void* node, int fromState);
                    subState = ((fn_CanHandle_t)(*(void**)(*(uintptr_t*)node)))
                               ((void*)node, (int)*(int8_t*)(buildState + 0x50));
                }

                // If target state is valid, call DoTransition
                if (*(int8_t*)(buildState + 0x53) >= 0) {
                    // vtable+0x28: DoTransition(node, smRef, fromState [, targetState])
                    typedef void (*fn_DoTransition_t)(void* node, void* smRef,
                                                       int fromState, int targetState);
                    uintptr_t headNode = *smPtrAddr;
                    ((fn_DoTransition_t)(*(void**)(*(uintptr_t*)headNode + 0x28)))
                        ((void*)headNode, (void*)smPtrAddr,
                         (int)*(int8_t*)(buildState + 0x51),
                         (int)*(int8_t*)(buildState + 0x53));
                }
            }

            // --- Update previousState from targetState ---
            *(int8_t*)(buildState + 0x50) = *(int8_t*)(buildState + 0x52);

            // --- Walk to tail of linked list ---
            uintptr_t current = *smPtrAddr;
            uintptr_t next = *(uintptr_t*)(current + 0x10);  // offset +0x10 = next
            while (next != 0) {
                current = next;
                next = *(uintptr_t*)(current + 0x10);
            }
            *smPtrAddr = current;

            // --- Call CanHandle on tail node with targetState ---
            {
                typedef int8_t (*fn_CanHandle_t)(void* node, int fromState);
                int8_t result = ((fn_CanHandle_t)(*(void**)(*(uintptr_t*)current)))
                                ((void*)current, (int)*(int8_t*)(buildState + 0x52));
                *(int8_t*)(buildState + 0x51) = result;

                // If can't handle, walk back through prev pointers
                while (result < 0) {
                    uintptr_t prev = *(uintptr_t*)(*smPtrAddr + 0x08);
                    *smPtrAddr = prev;
                    result = ((fn_CanHandle_t)(*(void**)(*(uintptr_t*)prev)))
                             ((void*)prev, (int)*(int8_t*)(buildState + 0x50));
                    *(int8_t*)(buildState + 0x51) = result;
                }
            }

            // --- Walk list to check HasExitHandler for new state ---
            {
                int subState = (int)*(int8_t*)(buildState + 0x51);
                uintptr_t node = *smPtrAddr;
                *(uint16_t*)(buildState + 0x52) = 0xFFFF;  // reset target+transition

                while (true) {
                    if (subState >= 0) {
                        // vtable+0x18: HasExitHandler(node, smRef, subState) -> bool
                        // NOTE: R8 carries subState as implicit 3rd arg (dispatch index).
                        typedef bool (*fn_HasExit_t)(void* node, void* smRef, int subState);
                        bool hasExit = ((fn_HasExit_t)(*(void**)(*(uintptr_t*)node + 0x18)))
                                       ((void*)node, (void*)smPtrAddr, subState);
                        if (hasExit) break;
                    }
                    node = *(uintptr_t*)(node + 0x08);
                    if (node == 0) break;
                    typedef int (*fn_CanHandle_t)(void* node, int fromState);
                    subState = ((fn_CanHandle_t)(*(void**)(*(uintptr_t*)node)))
                               ((void*)node, (int)*(int8_t*)(buildState + 0x50));
                }
            }
        } while (*(int8_t*)(buildState + 0x52) >= 0);

first_entry_check:
        // --- OnFirstEntry callback (only on first invocation) ---
        if (isFirstCall && *(int8_t*)(buildState + 0x50) >= 0) {
            int subState = (int)*(int8_t*)(buildState + 0x51);
            uintptr_t node = *smPtrAddr;

            while (true) {
                if (subState >= 0) {
                    // vtable+0x30: OnFirstEntry(node, smRef, time, subState) -> bool
                    // NOTE: Original passes 4 args — Ghidra drops both R8 (time) and R9 (subState).
                    // R9 is the state dispatch index used by the controller's jump table.
                    typedef bool (*fn_OnFirstEntry_t)(void* node, void* smRef, void* time, int subState);
                    bool result = ((fn_OnFirstEntry_t)(*(void**)(*(uintptr_t*)node + 0x30)))
                                  ((void*)node, (void*)smPtrAddr, time, subState);
                    if (result) break;
                }
                node = *(uintptr_t*)(node + 0x08);
                if (node == 0) break;
                typedef int (*fn_CanHandle_t)(void* node, int fromState);
                subState = ((fn_CanHandle_t)(*(void**)(*(uintptr_t*)node)))
                           ((void*)node, (int)*(int8_t*)(buildState + 0x50));
            }
            isFirstCall = false;
        }
    } while (*(int8_t*)(buildState + 0x52) >= 0);
}

// ============================================================
// DestroyComponent_StartUpgrade (0x14011dfa0)
// ============================================================
// Initiates an upgrade on a building by forcing its BuildState
// state machine to transition to upgrade state (state 3).
//
// Logic:
// 1. Find BuildState on the entity at destroyComp+0x10
// 2. Read current state byte:
//    - If state == 6 (damaged): check again, if still 6 or 7, call Destroy instead
//    - Otherwise: proceed with upgrade transition
// 3. Run the state machine exit sequence (process current state exit)
// 4. If sub-state is negative after exit: call StateMachine_Tick to settle
// 5. Request transition to state 3 (upgrade) via sm->vtable[1]
// 6. Run the state machine entry sequence (process new state entry)
//
// Original: 0x14011dfa0 (177 lines in Ghidra).
void DestroyComponent_StartUpgrade_Impl(void* destroyComp) {
    // --- Resolve StateMachine_Tick if not already done ---
    if (!g_StateMachine_Tick) {
        g_StateMachine_Tick = reinterpret_cast<fn_StateMachine_Tick_Local_t>(
            ResolveAddr(Module::Exe, 0x1400a9660));
    }

    // --- Find BuildState component on entity ---
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uint64_t bsRef = 0;
    fn_FindComponent_BuildState(*(void**)((uintptr_t)destroyComp + 0x10), &bsRef);
    fn_InternalBasePtr_CheckPtr(&bsRef);

    uintptr_t buildState = bsRef;

    // --- Read current state byte ---
    int8_t stateByte = *(int8_t*)(buildState + 0x52);
    if (stateByte < 0) {
        stateByte = *(int8_t*)(buildState + 0x50);
    }

    if (stateByte == 6) {
        // State is 6 (damaged) — double-check
        fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
        fn_FindComponent_BuildState(*(void**)((uintptr_t)destroyComp + 0x10), &bsRef);
        fn_InternalBasePtr_CheckPtr(&bsRef);
        buildState = bsRef;

        stateByte = *(int8_t*)(buildState + 0x52);
        if (stateByte < 0) {
            stateByte = *(int8_t*)(buildState + 0x50);
        }

        if (stateByte == 6) {
            // Still state 6 — check for state 7
            fn_InternalBasePtr_CheckPtr(&bsRef);
            stateByte = *(int8_t*)(buildState + 0x52);
            if (stateByte < 0) {
                stateByte = *(int8_t*)(buildState + 0x50);
            }
            if (stateByte != 7) {
                // Not state 7 either — proceed with upgrade (fall through below)
                goto do_upgrade;
            }
        }
        // State is 6 or 7 — destroy instead of upgrade
        DestroyComponent_Destroy_Impl(destroyComp);
        return;
    }

do_upgrade:
    // --- Phase 1: Exit current state ---
    fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)destroyComp + 0x10));
    uint64_t bsRef2 = 0;
    fn_FindComponent_BuildState(*(void**)((uintptr_t)destroyComp + 0x10), &bsRef2);

    // Create zero Time for state machine operations
    int64_t zeroTime = 0;
    fn_Time_Ctor_Float((void*)&zeroTime, 0.0f);

    fn_InternalBasePtr_CheckPtr(&bsRef2);
    buildState = bsRef2;
    uintptr_t* smPtrAddr = (uintptr_t*)(buildState + 0x48);

    // Run the state machine exit sequence
    // NOTE: Original always passes &zeroTime to OnFirstEntry (Ghidra drops R8).
    ProcessStateMachine(buildState, smPtrAddr, true, &zeroTime);

    // --- Phase 2: Settle state machine if sub-state is negative ---
    fn_InternalBasePtr_CheckPtr(&bsRef2);
    buildState = bsRef2;

    if (*(int8_t*)(buildState + 0x51) < 0) {
        int64_t tickTime = 0;
        fn_Time_Ctor_Float((void*)&tickTime, 0.0f);
        if (g_StateMachine_Tick) {
            g_StateMachine_Tick((void*)(buildState + 0x48), &tickTime, 0, 0);
        }
    }

    // --- Phase 3: Request transition to state 3 (upgrade) ---
    int targetState = 3;
    // vtable[1]: RequestTransition(node, currentSubState, &targetState)
    uintptr_t headNode = *(uintptr_t*)(buildState + 0x48);
    typedef int (*fn_RequestTransition_t)(void* node, int currentState, int* targetState);
    int result = ((fn_RequestTransition_t)(*(void**)(*(uintptr_t*)headNode + 0x08)))
                 ((void*)headNode, (int)*(int8_t*)(buildState + 0x51), &targetState);

    if (result >= 0) {
        *(int8_t*)(buildState + 0x53) = (int8_t)result;
        *(int8_t*)(buildState + 0x52) = (int8_t)targetState;
    }

    // --- Phase 4: Enter new state ---
    int64_t entryTime = 0;
    fn_Time_Ctor_Float((void*)&entryTime, 0.0f);

    fn_InternalBasePtr_CheckPtr(&bsRef2);
    buildState = bsRef2;
    smPtrAddr = (uintptr_t*)(buildState + 0x48);

    ProcessStateMachine(buildState, smPtrAddr, true, &entryTime);
}

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t result = 0;
    fn_FindElement(dialogElem, &result, nameBuf);

    fn_String_Dtor(nameBuf);
    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    // Dereference InternalPtr to get raw Element*
    void* rawElem = (void*)intPtr;
    // Wrap in InternalBasePtr (skip IsDerivedFrom check — dialog layout is fixed)
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(uint64_t*)basePtrBuf;
}

// ============================================================
// DestroyUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds all named UI elements in the dialog and stores them in
// the panel's fields. Sets element IDs on the action buttons.
// Checks if the building can be reclaimed and hides the button
// if appropriate.
//
// Original: 0x140066150.

void DestroyUI_Slot4(DestroyUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // --- Progress elements (stored as InternalPtr directly) ---

    // progressRemove → +0x30
    self->data30 = FindElemByName(dialogElem, "progressRemove");
    // Set progress max at element +0x124
    *(float*)(*(uintptr_t*)&self->data30 + 0x124) = 1.0f;

    // progressDestruct → +0x38
    self->data38 = FindElemByName(dialogElem, "progressDestruct");
    *(float*)(*(uintptr_t*)&self->data38 + 0x124) = 1.0f;

    // --- Text elements (stored as InternalBasePtr) ---
    self->data40 = FindElemAsBasePtr(dialogElem, "textRemove");
    self->data48 = FindElemAsBasePtr(dialogElem, "textUpgrade");
    self->data50 = FindElemAsBasePtr(dialogElem, "textDamage");
    self->data58 = FindElemAsBasePtr(dialogElem, "textFixing");

    // --- Get entity for component checks ---
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // --- Button elements (stored as InternalBasePtr, with element IDs) ---

    // buttonReclaim → +0x60, ID = 0x1771
    self->data60 = FindElemAsBasePtr(dialogElem, "buttonReclaim");
    fn_Element_SetElementId((void*)self->data60, 0x1771);

    // buttonDestroy → +0x68, ID = 0x1773
    self->data68 = FindElemAsBasePtr(dialogElem, "buttonDestroy");
    fn_Element_SetElementId((void*)self->data68, 0x1773);

    // buttonRebuild → +0x70, ID = 0x1772
    self->data70 = FindElemAsBasePtr(dialogElem, "buttonRebuild");
    fn_Element_SetElementId((void*)self->data70, 0x1772);

    // --- Check if reclaim should be hidden ---
    // Find DestroyComponent on entity
    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityInternal, &destroyComp);

    bool canReclaim = DestroyComponent_CanReclaim_Impl(destroyComp);
    bool compFlag = *(uint8_t*)((uintptr_t)destroyComp + 0x50) != 0;

    if (canReclaim || compFlag) {
        // Hide the reclaim button (SetFlagsOnOff: set flag 1 = hidden)
        fn_Element_SetFlagsOnOff((void*)self->data60, 1, 0);
    }
}

// ============================================================
// DestroyUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Complex visibility state machine for the destroy/reclaim panel.
// Two main branches depending on whether the building can be reclaimed.
//
// CanReclaim == true:  Show rebuild/destroy buttons, hide progress bars,
//                      enable/disable buttons based on build state.
// CanReclaim == false: Show reclaim button, update storage/destruction
//                      progress bars, show appropriate status text.
//
// Original: 0x140065B00.

void DestroyUI_Slot7(DestroyUI* self) {
    // Get entity, wrap in InternalBasePtr
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find StateMachine component and Destroy component
    void* smComp = nullptr;
    fn_FindComponent_StateMachine(entityInternal, &smComp);
    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityInternal, &destroyComp);

    // Check if building can be reclaimed
    bool canReclaim = DestroyComponent_CanReclaim_Impl(destroyComp);

    // Element pointers for convenience
    void* progressRemove  = (void*)self->data30;
    void* progressDestruct = (void*)self->data38;
    void* textRemove      = (void*)self->data40;
    void* textUpgrade     = (void*)self->data48;
    void* textDamage      = (void*)self->data50;
    void* textFixing      = (void*)self->data58;
    void* buttonReclaim   = (void*)self->data60;
    void* buttonDestroy   = (void*)self->data68;
    void* buttonRebuild   = (void*)self->data70;

    // Guard: if critical UI elements aren't initialized yet, bail out
    if (!progressRemove || !progressDestruct || !buttonReclaim || !buttonDestroy) return;

    // The last element to be hidden at function exit
    void* hideAtEnd = nullptr;

    if (!canReclaim) {
        // --- Building is being demolished: show reclaim, update progress ---

        // Hide rebuild, destroy buttons and textDamage
        if (buttonRebuild) fn_Element_SetFlagsOnOff(buttonRebuild, 1, 0);
        fn_Element_SetFlagsOnOff(buttonDestroy, 1, 0);
        if (textDamage) fn_Element_SetFlagsOnOff(textDamage, 1, 0);

        // Show reclaim button
        fn_Element_SetFlagsOnOff(buttonReclaim, 0, 1);

        // --- Update storage removal progress bar ---
        if (smComp != nullptr) {
            int capacity = *(int*)((uintptr_t)smComp + 0x38);
            int count = fn_StorageComponent_QueryCount(smComp);

            float fillRatio;
            if (capacity < 1) {
                fillRatio = 0.0f;
            } else {
                fillRatio = (float)count / (float)capacity;
            }

            float fillPercent = fillRatio * 100.0f + 0.5f;

            // Set progress bar value
            *(float*)((uintptr_t)progressRemove + 0x128) = fillRatio;

            // Format and set percentage text
            uint8_t strBuf[8];
            fn_String_Format(strBuf, "%i", (uint64_t)(uint32_t)(int)fillPercent);
            fn_ProgressElement_SetProgressTextParameter(progressRemove, 0, strBuf);
            fn_String_Dtor(strBuf);
        }

        // --- Update destruction progress bar ---
        int destroyDone = *(int*)((uintptr_t)destroyComp + 0x54);
        int destroyTotal = *(int*)((uintptr_t)destroyComp + 0x58);

        float destroyRatio;
        if (destroyTotal == 0) {
            destroyRatio = 0.0f;
        } else {
            destroyRatio = (float)destroyDone / (float)destroyTotal;
        }

        float destroyPercent = destroyRatio * 100.0f + 0.5f;

        *(float*)((uintptr_t)progressDestruct + 0x128) = destroyRatio;

        uint8_t strBuf2[8];
        fn_String_Format(strBuf2, "%i", (uint64_t)(uint32_t)(int)destroyPercent);
        fn_ProgressElement_SetProgressTextParameter(progressDestruct, 0, strBuf2);
        fn_String_Dtor(strBuf2);

        // --- Show/hide progress bars based on destruction state ---
        bool isDestroying = (destroyRatio != 0.0f);
        if (isDestroying) {
            fn_Element_SetFlagsOnOff(progressRemove, 1, 0);   // hide storage bar
        } else {
            fn_Element_SetFlagsOnOff(progressRemove, 0, 1);   // show storage bar
        }
        fn_Element_SetFlagsOnOff(progressDestruct,
                                 isDestroying ? 0 : 1,
                                 isDestroying ? 1 : 0);

        // --- Text visibility based on component state ---
        // Check if the building's current state matches its target state
        int64_t currentState = *(int64_t*)((uintptr_t)destroyComp + 0x60);
        void* compEntity = *(void**)((uintptr_t)destroyComp + 0x10);
        int64_t targetState = *(int64_t*)((uintptr_t)compEntity + 0x28);

        if (currentState == targetState) {
            // Building state matches target → show "fixing" text
            if (textFixing) fn_Element_SetFlagsOnOff(textFixing, 0, 1);
            if (textRemove) fn_Element_SetFlagsOnOff(textRemove, 1, 0);
            hideAtEnd = textUpgrade;
        } else {
            // Check if building can be rebuilt (upgraded)
            bool canRebuild = DestroyComponent_CanRebuild_Impl(destroyComp);
            if (!canRebuild) {
                // Regular removal: show textRemove
                if (textUpgrade) fn_Element_SetFlagsOnOff(textUpgrade, 1, 0);
                if (textRemove) fn_Element_SetFlagsOnOff(textRemove, 0, 1);
            } else {
                // Can rebuild: show textUpgrade
                if (textRemove) fn_Element_SetFlagsOnOff(textRemove, 1, 0);
                if (textUpgrade) fn_Element_SetFlagsOnOff(textUpgrade, 0, 1);
            }
            hideAtEnd = textFixing;
        }
    } else {
        // --- Building can be reclaimed: show rebuild/destroy buttons ---

        // Virtual call on buttonRebuild: vtable+0x40 with enabled state
        // FUN_140003ea0 finds a BuildState component on the entity
        void* buildState = nullptr;
        void* compEntity = *(void**)((uintptr_t)destroyComp + 0x10);
        fn_FindComponent_BuildState(compEntity, &buildState);

        // Read state byte: prefer +0x52, fall back to +0x50 if bit 7 set
        int8_t stateByte = *(int8_t*)((uintptr_t)buildState + 0x52);
        if (stateByte < 0) {
            stateByte = *(int8_t*)((uintptr_t)buildState + 0x50);
        }

        // Call virtual method at vtable+0x40 on buttonRebuild
        // (likely SetEnabled or SetChecked: true when state != 6)
        {
            typedef void (*fn_VSlot8_t)(void* self, bool param);
            void** vtable = *(void***)buttonRebuild;
            fn_VSlot8_t vfunc = (fn_VSlot8_t)vtable[8];
            vfunc(buttonRebuild, stateByte != 6);
        }

        // Same virtual call on buttonDestroy
        void* buildState2 = nullptr;
        fn_FindComponent_BuildState(compEntity, &buildState2);
        {
            typedef void (*fn_VSlot8_t)(void* self, bool param);
            void** vtable = *(void***)buttonDestroy;
            fn_VSlot8_t vfunc = (fn_VSlot8_t)vtable[8];
            vfunc(buttonDestroy, true);
        }

        // Show rebuild and destroy buttons
        fn_Element_SetFlagsOnOff(buttonRebuild, 0, 1);
        fn_Element_SetFlagsOnOff(buttonDestroy, 0, 1);

        // Show textDamage
        if (textDamage) fn_Element_SetFlagsOnOff(textDamage, 0, 1);

        // Hide everything else
        if (textFixing) fn_Element_SetFlagsOnOff(textFixing, 1, 0);
        if (textRemove) fn_Element_SetFlagsOnOff(textRemove, 1, 0);
        if (textUpgrade) fn_Element_SetFlagsOnOff(textUpgrade, 1, 0);
        fn_Element_SetFlagsOnOff(progressRemove, 1, 0);
        fn_Element_SetFlagsOnOff(progressDestruct, 1, 0);

        hideAtEnd = buttonReclaim;
    }

    // Hide the last element (buttonReclaim when reclaim active, or a text element)
    if (hideAtEnd) fn_Element_SetFlagsOnOff(hideAtEnd, 1, 0);
}

// ============================================================
// DestroyUI::Slot9 (vtable +0x48) — Notification handler
// ============================================================
// Handles button press notifications. Dispatches to reclaim,
// rebuild, or destroy actions on the DestroyComponent based
// on the button element ID.
//
// Original: 0x140066080.

void DestroyUI_Slot9(DestroyUI* self, uint64_t /*param2*/, int param3, int param4) {
    if (param3 != 0x12) return;

    // Get entity, wrap in InternalBasePtr
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find DestroyComponent on entity
    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityInternal, &destroyComp);

    // Dispatch based on button ID
    if (param4 == 0x1771) {
        DestroyComponent_Reclaim_Impl(destroyComp);
    } else if (param4 == 0x1772) {
        DestroyComponent_Rebuild_Impl(destroyComp);
    } else if (param4 == 0x1773) {
        DestroyComponent_Destroy_Impl(destroyComp);
    }
}
