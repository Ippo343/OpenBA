// engine/src/ui/immigrationui.cpp
// Clean reimplementation of ImmigrationUI game logic.
//
// ImmigrationUI (0x50 bytes) — immigration allow/deny panel.
// Extends DialogController. Fields:
//   +0x30 textNone, +0x38 textNomad (InternalBasePtr)
//   +0x40 buttonAllow, +0x48 buttonDeny (InternalBasePtr)
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
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(uint64_t*)basePtrBuf;
}

// ============================================================
// ImmigrationUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds textNone, textNomad, buttonAllow, and buttonDeny
// elements in the dialog. Stores as InternalBasePtr and sets
// element IDs on the buttons.
//
// Original: 0x14006B330.

void ImmigrationUI_Slot4(ImmigrationUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find text elements
    self->data30 = FindElemAsBasePtr(dialogElem, "textNone");
    self->data38 = FindElemAsBasePtr(dialogElem, "textNomad");

    // Find button elements
    self->data40 = FindElemAsBasePtr(dialogElem, "buttonAllow");
    self->data48 = FindElemAsBasePtr(dialogElem, "buttonDeny");

    // Set element IDs on buttons
    fn_Element_SetElementId((void*)self->data40, 39000);
    fn_Element_SetElementId((void*)self->data48, 0x9859);
}

// ============================================================
// ImmigrationUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Finds ImmigrationComponent, checks build state to determine
// if nomads are available. Enables/disables buttons accordingly,
// shows/hides text, formats immigration count + world name.
//
// Original: 0x14006B130.

void ImmigrationUI_Slot7(ImmigrationUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check IsA ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }

    // Wrap in InternalBasePtr for FindComponent
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find ImmigrationComponent
    uint64_t immigComp = 0;
    fn_FindComponent_Immigration((void*)entityBasePtr, &immigComp);

    // Read immigration count at comp+0x60
    uint32_t immigCount = *(uint32_t*)(immigComp + 0x60);

    // Get sub-entity from ImmigrationComponent+0x10 (InternalBasePtr field)
    void* subEntity = (void*)*(uint64_t*)(immigComp + 0x10);

    // Find BuildStateComponent on sub-entity
    uint64_t buildStateComp = 0;
    fn_FindComponent_BuildState(subEntity, &buildStateComp);

    // Check build state: byte at +0x52, if negative use +0x50, compare to 0x11
    char state = *(char*)(*(uint64_t*)&buildStateComp + 0x52);
    if (state < 0) {
        state = *(char*)(*(uint64_t*)&buildStateComp + 0x50);
    }
    bool buildComplete = (state == 0x11);

    // Enable/disable buttons at +0x40 and +0x48 via vtable[8] (SetEnabled)
    void* buttonAllow = (void*)*(uint64_t*)(s + 0x40);
    void** vtAllow = *(void***)buttonAllow;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtAllow[8])(buttonAllow, buildComplete);

    void* buttonDeny = (void*)*(uint64_t*)(s + 0x48);
    void** vtDeny = *(void***)buttonDeny;
    ((fn_SetEnabled_t)vtDeny[8])(buttonDeny, buildComplete);

    // Show/hide text elements based on build state
    // If build complete: show textNomad (+0x38), hide textNone (+0x30)
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x38),
        (uint32_t)!buildComplete, (uint32_t)buildComplete);
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x30),
        (uint32_t)buildComplete, (uint32_t)!buildComplete);

    // Format immigration count as "%i" and set on textNomad parameter 0
    uint64_t countStr = 0;
    fn_String_Format(&countStr, "%i", (uint64_t)immigCount);
    fn_TextElement_SetParameter((void*)*(uint64_t*)(s + 0x38), 0, &countStr);
    fn_String_Dtor(&countStr);

    // Copy world name from worldSingleton+0x28, set as parameter 1
    void* worldSingleton = *g_pGameWorldSingleton;
    uint64_t worldNameStr = 0;
    fn_String_Ctor_Copy(&worldNameStr, (void*)((uintptr_t)worldSingleton + 0x28));
    fn_TextElement_SetParameter((void*)*(uint64_t*)(s + 0x38), 1, &worldNameStr);
    fn_String_Dtor(&worldNameStr);
}
