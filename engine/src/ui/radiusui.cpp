// engine/src/ui/radiusui.cpp
// Clean reimplementation of RadiusUI game logic.
//
// RadiusUI (0x38 bytes) — displays building radius overlay.
// Extends DialogController. data30 (int32_t +0x30) = radius overlay index (-1 = none).
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// RadiusUI::Slot6 (vtable +0x30) — Remove radius overlay
// ============================================================
// When the panel is hidden/closed, removes the active radius overlay
// from the RadiusDisplayManager and resets the index to -1.
//
// Original: 0x140077600, 28 bytes.

// ============================================================
// RadiusUI::Slot5 (vtable +0x28) — Calculate and store radius overlay
// ============================================================
// When the panel is shown, finds the RadiusComponent on the entity's
// class sub-object, gets the BuildComponent, calculates the center
// tile index, and stores the radius overlay display index at +0x30.
//
// Original: 0x140077650.

void RadiusUI_Slot5(RadiusUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check IsA ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }

    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Get class/template sub-object from entity+0x28
    uint64_t subObj = *(uint64_t*)(entityBasePtr + 0x28);

    // Search sub-object's component array (+0x28 base, +0x30 count) for RadiusComponent
    uint32_t radiusTypeId = g_TypeId_RadiusComponent ? *g_TypeId_RadiusComponent : 0;
    void* radiusComp = nullptr;
    int count = *(int*)(subObj + 0x30);
    uint64_t arrayBase = *(uint64_t*)(subObj + 0x28);
    for (int i = 0; i < count; i++) {
        void* comp = (void*)*(uint64_t*)(arrayBase + (uint64_t)i * 8);
        // vtable[2] (offset 0x10) = GetTypeId
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)();
        int typeId = ((fn_GetTypeId_t)vtable[2])();
        if (typeId == (int)radiusTypeId) {
            if (fn_IsDerivedFrom(comp, radiusTypeId)) {
                radiusComp = comp;
            }
            break;
        }
    }

    // Find BuildComponent on the entity
    uint64_t buildComp = 0;
    fn_FindComponent_Build((void*)entityBasePtr, &buildComp);

    // Calculate center tile index from BuildComponent coordinates:
    // +0x28 = int16 baseX, +0x2a = int16 baseY
    // +0x2c = int16 sizeX, +0x2e = int16 sizeY
    int gridSize = **(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
    int centerY = (int)*(int16_t*)(buildComp + 0x2a) + ((int)*(int16_t*)(buildComp + 0x2e) >> 1);
    int centerX = (int)*(int16_t*)(buildComp + 0x28) + ((int)*(int16_t*)(buildComp + 0x2c) >> 1);
    int tileIndex = centerY * gridSize + centerX;

    // Calculate radius and store display index
    self->data30 = fn_RadiusCalc(radiusComp, tileIndex);
}

// ============================================================
// RadiusUI::Slot6 (vtable +0x30) — Remove radius overlay
// ============================================================
// When the panel is hidden/closed, removes the active radius overlay
// from the RadiusDisplayManager and resets the index to -1.
//
// Original: 0x140077600, 28 bytes.

void RadiusUI_Slot6(RadiusUI* self) {
    if (self->data30 >= 0) {
        // Game/World singleton +0x110 = RadiusDisplayManager
        void* gameWorld = *g_pGameWorldSingleton;
        void* radiusMgr = *(void**)((uintptr_t)gameWorld + 0x110);
        fn_RadiusDisplay_Remove(radiusMgr, self->data30);
    }
    self->data30 = -1;
}
