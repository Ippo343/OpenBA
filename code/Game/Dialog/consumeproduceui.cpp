// engine/src/ui/consumeproduceui.cpp
// Clean reimplementation of ConsumeProduceUI game logic.
//
// ConsumeProduceUI (0x40 bytes) — production progress panel.
// Extends DialogController. data30 = combo element InternalPtr,
// data38 = progress element InternalPtr.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// ConsumeProduceUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Gets entity's ConsumeProduceComponent, finds "comboProduct" and
// "progressRemaining" UI elements, populates the combo with product
// entries (sprite + name) from the component's Description, and
// sets the combo to the current product selection.
//
// Original: 0x14005F3D0.

void ConsumeProduceUI_Slot4(ConsumeProduceUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find StateMachine component
    uint64_t component = 0;
    fn_FindComponent_StateMachine(entityInternal, &component);

    // Get Description at component+0x18, check ConsumeProduceComponent
    void* desc = *(void**)((uintptr_t)component + 0x18);
    bool isCPComp = fn_IsDerivedFrom(desc, *g_TypeId_ConsumeProduceComponent);
    void* cpComp = isCPComp ? desc : nullptr;

    // Find "comboProduct" element via DialogInstance
    void* dialogInst = *(void**)(s + 0x18);
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "comboProduct");

    // Check if element exists (first FindElement call to test)
    uint64_t testBuf = 0;
    fn_DialogInstance_FindElement(dialogInst, &testBuf, nameBuf);
    bool hasCombo = (testBuf != 0);
    fn_String_Dtor(nameBuf);

    if (hasCombo) {
        // Find "comboProduct" again, wrap as InternalBasePtr, check ComboElement type
        fn_String_Ctor_CStr(nameBuf, "comboProduct");
        uint8_t elemBuf[16];
        fn_DialogInstance_FindElement(dialogInst, elemBuf, nameBuf);
        void* foundElem = *(void**)elemBuf;

        // IsDerivedFrom ComboElement type check
        bool isCombo = fn_IsDerivedFrom(foundElem, *g_TypeId_ComboElement);
        void* comboRaw = isCombo ? foundElem : nullptr;
        uint64_t comboBP = 0;
        fn_InternalBasePtr_Ctor(&comboBP, comboRaw);
        *(uint64_t*)(s + 0x30) = comboBP;
        fn_String_Dtor(nameBuf);

        // Set element ID 22000
        fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 22000);

        // Iterate description entries and populate combo
        // cpComp+0x28 = entries array ptr, cpComp+0x30 = entry count, stride 0x28
        uintptr_t cp = (uintptr_t)cpComp;
        int entryCount = *(int*)(cp + 0x30);
        uintptr_t entriesBase = *(uintptr_t*)(cp + 0x28);

        for (int i = 0; i < entryCount; i++) {
            // Each entry at entriesBase + i*0x28 has sub-descriptions
            // at entry+0x10 = pointer to sub-entry
            uintptr_t entryPtr = *(uintptr_t*)(entriesBase + (uintptr_t)i * 0x28 + 0x10);

            // Search sub-entries for ResourceDescription type (g_TypeId_RawMaterialDescription)
            int subCount = *(int*)(entryPtr + 0x30);
            uintptr_t subArray = *(uintptr_t*)(entryPtr + 0x28);
            void* resourceDesc = nullptr;

            for (int j = 0; j < subCount; j++) {
                void* subEntry = *(void**)(subArray + (uintptr_t)j * 8);
                // Call GetTypeId virtual (vtable+0x10) and compare
                typedef int (*fn_GetTypeId_t)(void*);
                void** vtbl = *(void***)subEntry;
                int typeId = ((fn_GetTypeId_t)vtbl[2])(subEntry);
                if (typeId == (int)*g_TypeId_RawMaterialDescription) {
                    // Verify with IsDerivedFrom
                    if (fn_IsDerivedFrom((void*)subEntry, *g_TypeId_RawMaterialDescription)) {
                        resourceDesc = subEntry;
                    }
                    break;
                }
            }

            // Create UI::String from description string table + entry name
            // String table at cpComp+0x20, name at entriesBase + i*0x28 + 0x20
            // Note: stride 0x28, offset for name within entry is calculated as:
            // base + i * (i+1) * 4 * 8 -> no, from disasm: LEA RCX,[RSI+RCX*8] where
            // RCX = RSI + RSI*4 = i * 5, so address = base + i*5*8 = base + i*0x28 + 0x20
            uintptr_t nameAddr = entriesBase + (uintptr_t)i * 0x28 + 0x20;
            uint64_t stringTable = *(uint64_t*)(cp + 0x20);
            uint8_t uiStrBuf[16];
            fn_UIString_Ctor(uiStrBuf, stringTable, (void*)nameAddr);

            // Convert UI::String to System::String
            uint8_t sysStrBuf[8];
            fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);

            // Create UI::Sprite from ResourceDescription
            // resourceDesc+0x20 = sprite sheet, resourceDesc+0x38 = sprite name
            void* comboElem = (void*)*(uint64_t*)(s + 0x30);
            uint8_t spriteBuf[32];
            uintptr_t rd = (uintptr_t)resourceDesc;
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(rd + 0x20), (void*)(rd + 0x38));

            // Add item to combo: sprite, string, null userData
            fn_ComboElement_AddItem(comboElem, spriteBuf, sysStrBuf, nullptr);
            fn_String_Dtor(sysStrBuf);
        }

        // Get current product index and set combo selection
        uint32_t productIdx = fn_GetCurrentProductIndex((void*)component);
        int selIdx = (int)productIdx;
        if ((int)productIdx < 0) {
            // Invalid — reset to 0
            *(uint32_t*)(component + 0x50) = 0;
            selIdx = 0;
        }
        void* comboFinal = (void*)*(uint64_t*)(s + 0x30);
        fn_ComboElement_SetCurrentItem(comboFinal, selIdx);
    }

    // Find "progressRemaining" element
    fn_String_Ctor_CStr(nameBuf, "progressRemaining");
    uint64_t testBuf2 = 0;
    fn_DialogInstance_FindElement(dialogInst, &testBuf2, nameBuf);
    bool hasProgress = (testBuf2 != 0);
    fn_String_Dtor(nameBuf);

    if (hasProgress) {
        fn_String_Ctor_CStr(nameBuf, "progressRemaining");
        uint8_t elemBuf2[16];
        fn_DialogInstance_FindElement(dialogInst, elemBuf2, nameBuf);
        void* progElem = *(void**)elemBuf2;

        // Check ProgressElement type
        bool isProgress = fn_IsDerivedFrom(progElem, *g_TypeId_ProgressElement);
        void* progRaw = isProgress ? progElem : nullptr;
        uint64_t progBP = 0;
        fn_InternalBasePtr_Ctor(&progBP, progRaw);
        *(uint64_t*)(s + 0x38) = progBP;
        fn_String_Dtor(nameBuf);

        // Set initial progress to 1.0f at element+0x124
        void* progFinal = (void*)*(uint64_t*)(s + 0x38);
        *(float*)((uintptr_t)progFinal + 0x124) = 1.0f;
    }
}

// ============================================================
// ConsumeProduceUI::Slot7 (vtable +0x38) — Update progress bar
// ============================================================
// Updates the production progress bar. Reads current/total ticks
// from the entity's components, calculates ratio, and formats
// as a percentage string.
//
// Original: 0x14005F1F0.

void ConsumeProduceUI_Slot7(ConsumeProduceUI* self) {
    // data38 = progress element InternalPtr
    void* progressElem = (void*)self->data38;
    if (!progressElem) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find component via FUN_14000b6f0 (same FindComponent used in StorageCapacityUI)
    void* component = nullptr;
    fn_FindComponent_StateMachine(entityInternal, &component);

    // Get sub-component at +0x18 of found component, check if ConsumeProduceComponent
    void* subComp = *(void**)((uintptr_t)component + 0x18);
    bool isCPComp = fn_IsDerivedFrom(subComp, *g_TypeId_ConsumeProduceComponent);
    void* cpComp = isCPComp ? subComp : nullptr;

    // Calculate production ratio: 1.0 - currentTicks / totalTicks
    int currentTicks = *(int*)((uintptr_t)component + 0x68);
    int totalTicks = *(int*)((uintptr_t)cpComp + 0x48);
    float ratio = 1.0f - (float)currentTicks / (float)totalTicks;

    // Store ratio at progress element +0x128
    void* progPtr = (void*)self->data38;
    *(float*)((uintptr_t)progPtr + 0x128) = ratio;

    // Format as integer percentage: (int)(ratio * 100.0 + 0.5)
    int percent = (int)(ratio * 100.0f + 0.5f);
    uint8_t strBuf[8];
    fn_String_Format(strBuf, "%i", (uint64_t)(uint32_t)percent);

    // Set on progress element
    fn_ProgressElement_SetProgressTextParameter(progPtr, 0, strBuf);
    fn_String_Dtor(strBuf);
}

// ============================================================
// ConsumeProduceUI::Slot9 (vtable +0x48) — Combo selection handler
// ============================================================
// When the combo selection changes (param3==0x14, param4==22000),
// reads the selected product index from the UI combo element and
// writes it to the entity's component.
//
// Original: 0x14005F330.

void ConsumeProduceUI_Slot9(ConsumeProduceUI* self, uint64_t /*param2*/,
                            int param3, int param4) {
    if (param3 != 0x14 || param4 != 22000) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find component
    void* component = nullptr;
    fn_FindComponent_StateMachine(entityInternal, &component);

    // Read selection from combo element (data30 InternalPtr) at +0x118
    void* comboElem = (void*)self->data30;
    uint32_t selection = *(uint32_t*)((uintptr_t)comboElem + 0x118);

    // Write to found component at +0x50
    *(uint32_t*)((uintptr_t)component + 0x50) = selection;
}
