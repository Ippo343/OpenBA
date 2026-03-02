// engine/src/ui/buildui.cpp
// Clean reimplementation of BuildUI panel methods.
//
// BuildUI (0x50 bytes): Building info panel.
// Layout: +0x28 = entity (InternalBasePtr)

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot9: On button click (event 0x12, id 6000), toggle workplace enabled state.
void BuildUI_Slot9(BuildUI* self, uint64_t param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x12 && param4 == 6000) {
        // Get entity at +0x28, check IsA ComponentEntity
        void* entity = (void*)*(uint64_t*)(s + 0x28);
        uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
        void* checkedEntity = nullptr;
        if (fn_IsDerivedFrom(entity, entityTypeId)) {
            checkedEntity = entity;
        }

        // Wrap in InternalBasePtr for FindComponent call
        uint64_t entityBasePtr = 0;
        fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

        // Find WorkplaceComponent on entity
        uint64_t workplaceComp = 0;
        fn_FindComponent_Workplace((void*)entityBasePtr, &workplaceComp);

        // Read enabled state byte at comp+0x50, toggle it
        char currentState = *(char*)(workplaceComp + 0x50);
        fn_WorkplaceComponent_Toggle((void*)workplaceComp, currentState == 0);
    }
}
