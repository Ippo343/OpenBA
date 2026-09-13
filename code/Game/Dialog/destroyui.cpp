// code/Game/Dialog/destroyui.cpp
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
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
    if (!g_TypeId_DestroyComponent) return false;  // SA-guard (see EnsureGlobals)

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
// ⚠ NAMING: fn_FindComponent_BuildState is FUN_140003ea0, which is really the
// STATEMACHINECOMPONENT finder (it matches g_BaseTypeInfo_StateMachineComponent).
// Every offset used below is an SMC offset; the name is wrong, the code is not.
//
// Reclaim/Rebuild/Destroy all POST AN EVENT on the state machine at SMC+0x48:
//   vtable[2](node, &SMC[0x48], current_sub_state, eventId)
// where eventId = 0 (reclaim/rebuild) or 1 (destroy). SMC+0x48 is the Tick context:
// +0x00 is the CURRENT node (StateMachine_Tick re-points it to the deepest active
// node each pass) and +0x51 is its sub-state index — so for a collapsed building
// this lands on the DestroyComponent's node in state 7. The handlers for events 0
// and 1 there are Destroy_State7Event0_Rebuild_d2a0 / Destroy_State7Event1_Remove_d1a0
// in destroycomponent_states.cpp; until 2026-08-23 they were registered as null and
// the dispatcher's graceful no-op swallowed every click.
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
// 0x14011dfa0 state-machine driver — moved (2026-07-20)
// ============================================================
// An earlier decompile-based port of 0x14011dfa0 ("StartUpgrade" — really the
// generic BuildState state-machine driver) lived here as
// DestroyComponent_StartUpgrade_Impl + a ProcessStateMachine helper. It
// diverged from the disassembly on the damaged-building short-circuit: per the
// disasm, state 6 is a plain no-op return and state 7 only dispatches
// 0x14011e4d0 — the old port ran the full upgrade pump for both. The
// authoritative, disasm-verified port is RemovalCalc_StateMachine_dfa0 in
// code/Game/Player/keystone_a_removalcalcpath.cpp, registered for 0x14011dfa0;
// UpgradeUI_OnRemove reaches it via ResolveAddr(Module::Exe, 0x14011dfa0).
//
// BuildState state bytes (used by the helpers in this file):
//   +0x50 previousState, +0x51 currentSubState,
//   +0x52 targetState (-1 = needs processing), +0x53 transitionTarget

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    System::String nameStr(name);

    uint64_t result = 0;
    fn_FindElement(dialogElem, &result, &nameStr);

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
    // Set progress max at element +0x124 (SA-guard: retail derefs blindly;
    // skip when the element is missing from the bound dialog)
    if (self->data30) *(float*)(*(uintptr_t*)&self->data30 + 0x124) = 1.0f;

    // progressDestruct → +0x38
    self->data38 = FindElemByName(dialogElem, "progressDestruct");
    if (self->data38) *(float*)(*(uintptr_t*)&self->data38 + 0x124) = 1.0f;

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
    // SA-guard: entity may have no DestroyComponent in standalone
    bool compFlag = destroyComp && *(uint8_t*)((uintptr_t)destroyComp + 0x50) != 0;

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

    // Find StorageComponent and Destroy component. (Ghidra-verified: retail
    // 0x140065b00 calls FUN_140006270, the StorageComponent finder — the
    // original port's ConsumeProduce finder was wrong.)
    void* smComp = nullptr;
    fn_FindComponent_Storage(entityInternal, &smComp);
    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityInternal, &destroyComp);
    if (!destroyComp) return;  // SA-guard: +0x54/+0x58 are read blindly below

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
            System::String strObj = System::String::Format("%i", (uint64_t)(uint32_t)(int)fillPercent);
            fn_ProgressElement_SetProgressTextParameter(progressRemove, 0, &strObj);
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

        System::String strObj2 = System::String::Format("%i", (uint64_t)(uint32_t)(int)destroyPercent);
        fn_ProgressElement_SetProgressTextParameter(progressDestruct, 0, &strObj2);

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
    if (!destroyComp) return;  // SA-guard

    // Dispatch based on button ID
    if (param4 == 0x1771) {
        DestroyComponent_Reclaim_Impl(destroyComp);
    } else if (param4 == 0x1772) {
        DestroyComponent_Rebuild_Impl(destroyComp);
    } else if (param4 == 0x1773) {
        DestroyComponent_Destroy_Impl(destroyComp);
    }
}

// ============================================================
// Standalone promotion — real DestroyUI vtable (2026-07-20)
// ============================================================
// Retail vtable @ 0x14015bb00 (11 slots): overrides 0,1,2,4,7,9; slots
// 3,5,6,8,10 are inherited UI::DialogController base thunks in the exe. In
// standalone we mirror the residenceui_sa promotion pattern: wire the ported
// Slot4/7/9 and drive the refresh from slot 8 (OnUpdate) — element cache is
// built once (Slot4, guarded on +0x30 still null), then the visibility state
// machine (Slot7) runs every frame. Replaces the UI_DIALOG_STUB(DestroyUI, ...)
// no-op registration in ui_dialog_stub_factories.cpp.
#include "Object/factory_registration.h"
#include "dialog_controller_vtable.h"

namespace destroyui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

// Standalone-local fallbacks for exe-.rdata type-id globals that resolve NULL.
static uint32_t s_ceTid = 0;
static uint32_t s_ddTid = 0;   // DestroyDescription (slot 0x1401c7628 — the
                               // misleadingly-named g_TypeId_DestroyComponent)

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("DestroyUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    // Fields +0x30..+0x70 are unowned element references — nothing to free.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void EnsureGlobals() {
    if (!g_TypeId_ComponentEntity) {
        s_ceTid = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ceTid) g_TypeId_ComponentEntity = &s_ceTid;
    }
    // CanRebuild_Impl derefs *g_TypeId_DestroyComponent (= DestroyDescription's
    // slot 0x1401c7628, null in standalone) — crashed live 2026-07-20 once
    // Slot7 started running per-frame. Point it at the name-resolved id.
    if (!g_TypeId_DestroyComponent) {
        s_ddTid = ::Object::ResolveTypeIdByNameLazy("DestroyDescription");
        if (s_ddTid) g_TypeId_DestroyComponent = &s_ddTid;
    }
}

bool Ready() {
    return g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
        && g_TypeId_DestroyComponent && *g_TypeId_DestroyComponent
        && fn_IsDerivedFrom && fn_InternalBasePtr_Ctor
        && fn_InternalBasePtr_CheckPtr && fn_Element_SetFlagsOnOff;
}

// SA-guard: the data-source entity ref (+0x28) is nulled by entity destruction
// (upgrade demolishes the house while its panel is open). IsDerivedFrom(null)
// crashes in Runtime — bail instead; the panel simply stops refreshing.
bool DataSourceAlive(void* self) {
    void* e = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x28);
    if (!e) return false;
    // Marked-for-removal (entity+0x40 bit 0x400): components may already be
    // torn down while the entity awaits the EntityList drain.
    if ((*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(e) + 0x40) >> 10) & 1)
        return false;
    return true;
}

// idx4 (+0x20): find + cache the named elements, set button ids.
void W_Slot4(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    if (!*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x20))
        return;  // dialog element tree not bound yet
    DestroyUI_Slot4(reinterpret_cast<DestroyUI*>(self));
}

// idx7 (+0x38): visibility/progress state machine.
void W_Slot7(void* self) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    DestroyUI_Slot7(reinterpret_cast<DestroyUI*>(self));
}

// idx8 (+0x40): per-frame. Build the element cache once (+0x30 still 0), then
// keep the panel visibility/progress live every frame.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) {
        W_Slot4(self);
        if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) return;  // build failed
    }
    DestroyUI_Slot7(reinterpret_cast<DestroyUI*>(self));
}

// idx9 (+0x48): button-click dispatch (reclaim 0x1771 / rebuild 0x1772 /
// destroy 0x1773). Dispatched (self, &elementPtr, msg, id, x, y).
void W_Slot9(void* self, void* p2, int msg, int id, int /*mx*/, int /*my*/) {
    EnsureGlobals();
    if (!self || !Ready() || !DataSourceAlive(self)) return;
    DestroyUI_Slot9(reinterpret_cast<DestroyUI*>(self),
                    reinterpret_cast<uint64_t>(p2), msg, id);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);
        g_vt[7] = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_Slot9);
    }
} g_init;

// Wave 10D lane VTFID: retail's DestroyUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "DestroyUI", &StubNop);

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x78); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x78), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace destroyui_sa

REGISTER_TYPE(DestroyUI, "UI::DialogController",
              &destroyui_sa::Factory, &destroyui_sa::InPlace);
