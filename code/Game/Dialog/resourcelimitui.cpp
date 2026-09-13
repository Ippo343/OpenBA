// engine/src/ui/resourcelimitui.cpp
// Clean reimplementation of ResourceLimitUI game logic.
//
// ResourceLimitUI (0x40 bytes) — resource limit spinner panel.
// Extends DialogController. Fields:
//   +0x10 dataSrc (pointer to typed object with resource index at +0x20)
//   +0x30 spinnerElement (InternalBasePtr)
//
// The resource limit value is stored in a global array accessed through
// the game world singleton: world->+0xF0->+0x170[index*0x20+8].
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>

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
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(uint64_t*)basePtrBuf;
}

// Helper: Read the resource limit value from the global array.
// Path: world->+0xF0->+0x170[resourceIndex * 0x20 + 8]
static int GetResourceLimit(void* dataSrc) {
    int resourceIndex = *(int*)((uintptr_t)dataSrc + 0x20);
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    return *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8);
}

// Helper: Write the resource limit value to the global array.
static void SetResourceLimit(void* dataSrc, int value) {
    int resourceIndex = *(int*)((uintptr_t)dataSrc + 0x20);
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8) = value;
}

// ============================================================
// ResourceLimitUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds "editLimit" spinner element, sets ID/min/max,
// reads current limit from global array.
//
// Original: 0x140077970.

void ResourceLimitUI_Slot4(ResourceLimitUI* self) {
    // Get data source from base class +0x10 and type-check
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, *g_TypeId_ResourceLimitType);
    void* typedSrc = isType ? dataSrc : nullptr;

    // Find "editLimit" spinner element
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);
    self->data30 = FindElemAsBasePtr(dialogElem, "editLimit");

    // Configure spinner
    void* spinner = (void*)self->data30;
    fn_SpinnerElement_SetElementId(spinner, 0x3039);
    fn_SpinnerElement_SetMinMax(spinner, 0, 999999);

    // Set current value from global resource limit array
    int currentLimit = GetResourceLimit(typedSrc);
    fn_SpinnerElement_SetValue(spinner, currentLimit);
}

// ============================================================
// ResourceLimitUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Syncs spinner display with current limit (unless spinner is active).
//
// Original: 0x1400778D0.

void ResourceLimitUI_Slot7(ResourceLimitUI* self) {
    // Get data source
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, *g_TypeId_ResourceLimitType);
    void* typedSrc = isType ? dataSrc : nullptr;

    void* spinner = (void*)self->data30;

    int currentLimit = GetResourceLimit(typedSrc);
    int spinnerVal = fn_SpinnerElement_GetValue(spinner);

    if (currentLimit != spinnerVal) {
        bool active = fn_SpinnerElement_IsActive(spinner);
        if (!active) {
            fn_SpinnerElement_SetValue(spinner, currentLimit);
        }
    }
}

// ============================================================
// ResourceLimitUI::Slot9 (vtable +0x48) — Notification handler
// ============================================================
// When spinner value changes (event 0x15, ID 0x3039), writes
// the new value back to the global resource limit array.
//
// Original: 0x1400777E0.

void ResourceLimitUI_Slot9(ResourceLimitUI* self, void* param2,
                           int param3, int param4) {
    if (param3 != 0x15 || param4 != 0x3039) return;

    // Validate sender is the spinner element or its child
    void* spinner = (void*)self->data30;
    void* sender = *(void**)param2;

    if (sender != spinner) {
        uint64_t childResult = 0;
        fn_Element_Child(spinner, &childResult, 0);
        void* child = (void*)childResult;
        if (sender != child) return;
    }

    // Get data source
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, *g_TypeId_ResourceLimitType);
    void* typedSrc = isType ? dataSrc : nullptr;

    // Write spinner value to global limit
    int newVal = fn_SpinnerElement_GetValue(spinner);
    SetResourceLimit(typedSrc, newVal);
}

// ============================================================
// Standalone entry points — resource index supplied explicitly
// ============================================================
// In standalone the ResourceLimitUI child controllers never get their datasource
// bound (self+0x10 == NULL): the config->controller binding flows through base
// DialogController slots that the generic dialog stub no-ops, and through the
// ResourceLimitUIConfig field-deserialize that the stub skips. The framework still
// creates + dispatches the 11 row controllers in template/table order, which is
// exactly the resource-index order the Town Hall's (user-verified) count columns
// use: Log=0, Stone=1, Iron=2, Herbs=3, Tools=4, Fuel=5, Food=6, Clothing=7,
// Alcohol=8, Coal=9, Textile=10 -> townMgr(gw+0xf0)+0x170[idx*0x20+8]. So the
// standalone promotion supplies the index by dispatch order and drives the
// "editLimit" spinner directly, bypassing the missing config. These mirror the
// config-based Slot4/7/9 above but take an explicit index and never touch
// self+0x10 (so IsDerivedFrom is never called on a NULL datasource).

static int GetResourceLimitByIdx(int resourceIndex) {
    if (!g_pGameWorldSingleton) return 0;
    void* worldSingleton = *g_pGameWorldSingleton;
    if (!worldSingleton) return 0;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    if (!subObj) return 0;
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    if (!arrayBase) return 0;
    return *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8);
}

static void SetResourceLimitByIdx(int resourceIndex, int value) {
    if (!g_pGameWorldSingleton) return;
    void* worldSingleton = *g_pGameWorldSingleton;
    if (!worldSingleton) return;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    if (!subObj) return;
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    if (!arrayBase) return;
    *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8) = value;
}

// Read the live default/limit for a row (used by the trace + external checks).
int ResourceLimitUI_ReadLimit_SA(int resourceIndex) {
    return GetResourceLimitByIdx(resourceIndex);
}

// Setup (vtable +0x20): find "editLimit" within this row's element subtree, cache
// it at self->data30, configure id/min/max, seed it from the live limit.
void ResourceLimitUI_Setup_SA(ResourceLimitUI* self, int resourceIndex) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);
    if (!dialogElem) return;
    self->data30 = FindElemAsBasePtr(dialogElem, "editLimit");
    void* spinner = (void*)self->data30;
    if (!spinner) return;
    fn_SpinnerElement_SetElementId(spinner, 0x3039);
    fn_SpinnerElement_SetMinMax(spinner, 0, 999999);
    fn_SpinnerElement_SetValue(spinner, GetResourceLimitByIdx(resourceIndex));
}

// Update (vtable +0x38): keep the spinner synced with the live limit unless the
// user is actively editing it.
void ResourceLimitUI_Sync_SA(ResourceLimitUI* self, int resourceIndex) {
    void* spinner = (void*)self->data30;
    if (!spinner) return;
    int currentLimit = GetResourceLimitByIdx(resourceIndex);
    int spinnerVal = fn_SpinnerElement_GetValue(spinner);
    if (currentLimit != spinnerVal && !fn_SpinnerElement_IsActive(spinner))
        fn_SpinnerElement_SetValue(spinner, currentLimit);
}

// Event (vtable +0x48): on spinner change (msg 0x15, id 0x3039) write the new
// value back to the live limit.
void ResourceLimitUI_Write_SA(ResourceLimitUI* self, void* param2, int msg, int id,
                              int resourceIndex) {
    if (msg != 0x15 || id != 0x3039) return;
    void* spinner = (void*)self->data30;
    if (!spinner || !param2) return;
    void* sender = *(void**)param2;
    if (sender != spinner) {
        uint64_t childResult = 0;
        fn_Element_Child(spinner, &childResult, 0);
        if (sender != (void*)childResult) return;
    }
    SetResourceLimitByIdx(resourceIndex, fn_SpinnerElement_GetValue(spinner));
}
