// engine/src/ui/entityui.cpp
// Clean reimplementation of EntityUI panel methods.
//
// EntityUI (0x48 bytes): Entity info panel.
// Layout: +0x30 = tracked entity InternalBasePtr

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot6: Remove entity from world tracking set.
void EntityUI_Slot6(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Copy +0x30 (entity InternalBasePtr) to local
    uint64_t entityKey = *(uint64_t*)(s + 0x30);

    // Get BST root: g_pGameWorldSingleton -> +0x120 -> +0x60
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0x120);
    void* bstRoot = (void*)((uintptr_t)subObj + 0x60);

    fn_EntityTrackingSet_Remove(bstRoot, &entityKey);
}
