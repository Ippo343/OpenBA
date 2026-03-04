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
