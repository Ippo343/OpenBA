// engine/src/ui/cemeteryui.cpp
// Clean reimplementation of CemeteryUI game logic.
//
// CemeteryUI (0x38 bytes) — cemetery progress bar panel.
// Extends DialogController. Fields:
//   +0x30 progressElement (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>

// ============================================================
// File-local helper: CemeteryGetBurialCount (FUN_140046dd0)
// ============================================================
// Counts the number of graves in a CemeteryComponent that have a
// TombstoneComponent present (meaning they are occupied by a buried citizen).
//
// CemeteryComponent (param_1) fields:
//   +0x48: void* graveIndexArray   (int32[] of entity-slot indices)
//   +0x50: int32 graveCount
//
// Entity storage (accessed via gameWorld+0xf8):
//   entityStore = *(gameWorld + 0xf8)
//   entitySlot  = *(entityStore + 0x80) (the flat entity slot array)
//   entity      = *(entitySlot + 8 + graveIndex*0x30)  -- InternalPtr value
//
// Entity component search: looks through entity's component list (+0x30, count +0x38)
//   for a component with TypeId == TombstoneComponent (DAT_1401c4a30).
//
// Original: FUN_140046dd0.
//
static int CemeteryGetBurialCount_Impl(void* cemeteryComp) {
    uintptr_t cem = (uintptr_t)cemeteryComp;
    int count = *(int*)(cem + 0x50);
    if (count <= 0) return 0;

    uintptr_t entityStore = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xF8);
    uintptr_t* graveIdxArray = *(uintptr_t**)(cem + 0x48);
    int burials = 0;

    for (int i = 0; i < count; i++) {
        int graveIdx = ((int*)graveIdxArray)[i];

        // Get entity InternalPtr from entity slot array
        uintptr_t entitySlot = *(uintptr_t*)(entityStore + 0x80);
        uintptr_t entityIPtr = *(uintptr_t*)(entitySlot + 8 + (uintptr_t)graveIdx * 0x30);
        if (entityIPtr == 0) continue;

        fn_InternalBasePtr_CheckPtr((void*)&entityIPtr);
        uintptr_t entity = entityIPtr;
        if (!entity) continue;

        // Search entity's component list for TombstoneComponent
        int compCount = *(int*)(entity + 0x38);
        uintptr_t* compArray = *(uintptr_t**)(entity + 0x30);
        uint32_t tombTypeId = g_TypeId_TombstoneComponent ? *g_TypeId_TombstoneComponent : 0;

        bool found = false;
        for (int c = 0; c < compCount; c++) {
            uintptr_t compIPtr = compArray[c];
            fn_InternalBasePtr_CheckPtr((void*)&compIPtr);
            void* comp = (void*)compIPtr;
            if (!comp) continue;

            // Check TypeId via vtable[2]
            typedef int (*GetTypeId_t)(void* self);
            uintptr_t* vtbl = *(uintptr_t**)comp;
            int typeId = ((GetTypeId_t)(vtbl[2]))(comp);
            if (typeId == (int)tombTypeId) {
                bool isTomb = fn_IsDerivedFrom(comp, tombTypeId);
                if (isTomb) { found = true; break; }
            }
        }
        if (found) burials++;
    }
    return burials;
}

// ============================================================
// CemeteryUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds ProgressElement by type, stores at +0x30.
// Gets CemeteryComponent, reads max slots from +0x50,
// sets as progress maximum.
//
// Original: 0x14005F090.

void CemeteryUI_Slot4(CemeteryUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    fn_Element_FindElementType(dialogElem, &result, *g_TypeId_ProgressElement);
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = fn_IsDerivedFrom(foundElem, *g_TypeId_ProgressElement);
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, progressPtr);
    self->data30 = *(uint64_t*)basePtrBuf;

    // Setup: bail on a dead / marked-for-removal datasource (see Slot7).
    if (!SA_DialogEntityAlive(self)) return;
    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    // Set progress maximum from component's max slots field (+0x50)
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    void* progress = (void*)self->data30;
    *(float*)((uintptr_t)progress + 0x124) = (float)maxSlots;
}

// ============================================================
// CemeteryUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Gets burial count and max slots from CemeteryComponent,
// updates progress bar value and two text parameters.
//
// Original: 0x14005EF50.

void CemeteryUI_Slot7(CemeteryUI* self) {
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(self)) return;
    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    void* progress = (void*)self->data30;

    // Get burial count and set as progress current value
    int burialCount = CemeteryGetBurialCount_Impl(cemetComp);
    *(float*)((uintptr_t)progress + 0x128) = (float)burialCount;

    // Set text param 0: burial count
    {
        System::String str = System::String::Format("%i", (unsigned int)burialCount);
        fn_ProgressElement_SetProgressTextParameter(progress, 0, &str);
    }

    // Set text param 1: max slots
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    {
        System::String str = System::String::Format("%i", (unsigned int)maxSlots);
        fn_ProgressElement_SetProgressTextParameter(progress, 1, &str);
    }
}
