// code/Game/Dialog/townhallui.cpp
// Clean reimplementation of TownHallUI vtable methods.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0.
// Extends DialogController (0x30 base). Multi-tab town overview panel.
//
// Only Serialize (+0x20) and Update (+0x50) are hookable.
// Startup, OnCreate, OnRemove are DISABLED (prologue issues).
//
// Serialize finds the "tabs" TabElement, then calls 6 sub-functions
// to initialize each tab (General, Professions, Production, Inventory,
// Graph, Agriculture).
//
// Update implements IO::Serializer save/load of tab and combo states.

#include "ui/Panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// TownHallUI::Serialize (vtable +0x20, 0x140081000)
// ============================================================
// Finds the "tabs" TabElement child, stores at +0x30, then calls
// 6 sub-functions to initialize each tab's UI elements.
void TownHallUI_Serialize(TownHallUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "tabs" element via FindElement, then IsDerivedFrom(TabElement)
    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, "tabs");

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* element = *(void**)foundBuf;

    // Check if element is derived from TabElement
    void* tabElement = nullptr;
    if (element && g_TypeId_TabElement) {
        bool isDerived = fn_IsDerivedFrom(element, *g_TypeId_TabElement);
        if (isDerived) {
            tabElement = element;
        }
    }

    // Construct InternalBasePtr in local, then copy to +0x30
    uint64_t tempIBP = 0;
    fn_InternalBasePtr_Ctor(&tempIBP, tabElement);
    *(uint64_t*)(s + 0x30) = tempIBP;

    fn_String_Dtor(strBuf);

    // Initialize each tab
    fn_TownHallUI_InitGeneral(self);
    fn_TownHallUI_InitProfessions(self);
    fn_TownHallUI_InitProduction(self);
    fn_TownHallUI_InitInventory(self);
    fn_TownHallUI_InitGraph(self);
    fn_TownHallUI_InitAgriculture(self);
}

// ============================================================
// TownHallUI::Update (vtable +0x50, 0x140080db0)
// ============================================================
// IO::Serializer save/load of tab index and 5 combo box selections.
// On load (serializer mode == 0), restores tab/combo states and
// triggers graph update.
void TownHallUI_Update(TownHallUI* self, void* stream) {
    uintptr_t s = (uintptr_t)self;

    // Read current tab and combo values
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    int tabIndex = *(int*)(*(uintptr_t*)(s + 0x30) + 0xf8);  // TabElement active tab

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int combo1 = *(int*)(*(uintptr_t*)(s + 0x100) + 0x118);  // production combo

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
    int combo2 = *(int*)(*(uintptr_t*)(s + 0x128) + 0x118);  // inventory combo

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
    int combo3 = *(int*)(*(uintptr_t*)(s + 0x130) + 0x118);  // graph time combo

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
    int combo4 = *(int*)(*(uintptr_t*)(s + 0x168) + 0x118);  // graph data combo

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
    int combo5 = *(int*)(*(uintptr_t*)(s + 0x170) + 0x118);  // agriculture combo

    // Create serializer (24 bytes on stack)
    uint64_t serializer[3] = {0, 0, 0};
    g_fn_Serializer_Ctor(serializer, stream, 1);

    // Serialize tab index and combo states
    g_fn_Serialize_Int(serializer, &tabIndex, nullptr);
    g_fn_Serialize_Int(serializer, &combo1, nullptr);
    g_fn_Serialize_Int(serializer, &combo2, nullptr);

    // combo3 only serialized if version > 0
    int version = g_fn_Serializer_GetVersion(serializer);
    if (version > 0) {
        g_fn_Serialize_Int(serializer, &combo3, nullptr);
    }

    g_fn_Serialize_Int(serializer, &combo4, nullptr);
    g_fn_Serialize_Int(serializer, &combo5, nullptr);

    // On load (mode == 0), restore states
    if (*(int*)(serializer[0] + 8) == 0) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
        fn_TabElement_SetActiveTab(*(void**)(s + 0x30), tabIndex);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x100), combo1);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x128));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x128), combo2);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x130));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x130), combo3);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x168));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x168), combo4);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x170));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x170), combo5);

        fn_TownHallUI_UpdateGraph(self);
    }

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
}
