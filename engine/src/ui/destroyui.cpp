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

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t result = 0;
    fn_FindElement(&result, dialogElem, nameBuf);

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

    bool canReclaim = fn_DestroyComponent_CanReclaim(destroyComp);
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
    bool canReclaim = fn_DestroyComponent_CanReclaim(destroyComp);

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

    // The last element to be hidden at function exit
    void* hideAtEnd = nullptr;

    if (!canReclaim) {
        // --- Building is being demolished: show reclaim, update progress ---

        // Hide rebuild, destroy buttons and textDamage
        fn_Element_SetFlagsOnOff(buttonRebuild, 1, 0);
        fn_Element_SetFlagsOnOff(buttonDestroy, 1, 0);
        fn_Element_SetFlagsOnOff(textDamage, 1, 0);

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
            fn_Element_SetFlagsOnOff(textFixing, 0, 1);
            fn_Element_SetFlagsOnOff(textRemove, 1, 0);
            hideAtEnd = textUpgrade;
        } else {
            // Check if building can be rebuilt (upgraded)
            bool canRebuild = fn_DestroyComponent_CanRebuild(destroyComp);
            if (!canRebuild) {
                // Regular removal: show textRemove
                fn_Element_SetFlagsOnOff(textUpgrade, 1, 0);
                fn_Element_SetFlagsOnOff(textRemove, 0, 1);
            } else {
                // Can rebuild: show textUpgrade
                fn_Element_SetFlagsOnOff(textRemove, 1, 0);
                fn_Element_SetFlagsOnOff(textUpgrade, 0, 1);
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
        fn_Element_SetFlagsOnOff(textDamage, 0, 1);

        // Hide everything else
        fn_Element_SetFlagsOnOff(textFixing, 1, 0);
        fn_Element_SetFlagsOnOff(textRemove, 1, 0);
        fn_Element_SetFlagsOnOff(textUpgrade, 1, 0);
        fn_Element_SetFlagsOnOff(progressRemove, 1, 0);
        fn_Element_SetFlagsOnOff(progressDestruct, 1, 0);

        hideAtEnd = buttonReclaim;
    }

    // Hide the last element (buttonReclaim when reclaim active, or a text element)
    fn_Element_SetFlagsOnOff(hideAtEnd, 1, 0);
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
        fn_DestroyComponent_Reclaim(destroyComp);
    } else if (param4 == 0x1772) {
        fn_DestroyComponent_Rebuild(destroyComp);
    } else if (param4 == 0x1773) {
        fn_DestroyComponent_Destroy(destroyComp);
    }
}
