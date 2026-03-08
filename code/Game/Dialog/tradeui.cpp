// code/Game/Dialog/tradeui.cpp
// Clean reimplementation of TradeUI methods.
//
// TradeUI (0x148 bytes): Trading post panel — shows incoming merchants,
// trade items, buy/sell interface with order management.
// Extends DialogController (0x30 base).
//
// Layout:
//   +0x30..+0x58: sell order array (ptr, ptr, count, capacity) x3 vectors
//   +0x60..+0x78: sell entries (ptr, count, capacity, stride 0x20)
//   +0x70..+0x88: buy entries (ptr, count, capacity, stride 0x20)
//   +0x80..+0x98: storage entries (ptr, count, capacity, stride 0x18)
//   +0x90..+0xA8: order entries (ptr, count, capacity, stride 0x20)
//   +0xA0..+0xB8: purchase entries (ptr, count, capacity, stride 0x08)
//   +0xB0..+0xC8: reserved
//   +0xC0: comboOrder element
//   +0xC8: tableSell element
//   +0xD0: tableBuy element
//   +0xD8: tableStorage element
//   +0xE0: tablePurchase element
//   +0xE8: tableOrder element
//   +0xF0: buttonTrade element
//   +0xF8: buttonDismiss element
//   +0x100: numberSellTotal element
//   +0x108: numberBuyTotal element
//   +0x110: textNoStorage element
//   +0x118: textTooMuch element
//   +0x120: textNotEnough element
//   +0x128: reserved
//   +0x130: reserved
//   +0x138: comboPurchase element
//   +0x140: sortCombo element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// Draw (vtable +0x28): Mark tables for layout recalc, set building dirty flag
// ============================================================
// Original: 0x140082a70
//
// Marks tableSell (+0xC8) and tableBuy (+0xD0) as needing size recalculation.
// Then finds the BuildingComponent on the owning entity and sets its
// dirty flag (byte at +0x71) to 1, triggering a visual refresh.

void TradeUI_Draw(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Mark sell and buy tables for layout recalculation
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xC8));

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xD0));

    // Get owning entity and find BuildingComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Set building dirty flag
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    *(uint8_t*)(buildingPtr[0] + 0x71) = 1;
}

// ============================================================
// Slot6 (vtable +0x30): Clear building dirty flag
// ============================================================
// Original: 0x140082a00
//
// Finds BuildingComponent on owning entity, clears its dirty flag
// (byte at +0x71 = 0). Called when panel is hidden/deactivated.

void TradeUI_Slot6(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    *(uint8_t*)(buildingPtr[0] + 0x71) = 0;
}

// ============================================================
// Serialize (vtable +0x20): Full UI setup — element lookup, table building
// ============================================================
// Original: 0x140084dd0
//
// Phase 1: Finds 15 named elements and stores as InternalBasePtr in self.
// Phase 2: Sets element IDs on buttons, combos, and sort combo.
// Phase 3: Builds tablePurchase rows with child spinner elements (stride 0x20).
// Phase 4: Counts resource types via ExternalFactory, counts selectable
//          build types, sizes tableStorage/tableSell/tableBuy/tableOrder,
//          clones template elements, finds row sub-elements (spinners etc).
// Phase 5: Calls RefreshDisplay, RefreshSellTable, RefreshBuyTable.

// Helper: find named element in dialog, cast via IsDerivedFrom, return raw ptr
static void* TradeUI_FindTypedElement(void* dialogElem, const char* name, uint32_t* typeId) {
    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, name);
    fn_InternalBasePtr_CheckPtr((void*)dialogElem);
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)dialogElem, foundBuf, strBuf);
    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* elem = *(void**)foundBuf;
    void* result = nullptr;
    if (elem && typeId) {
        bool derived = fn_IsDerivedFrom(elem, *typeId);
        if (derived) result = elem;
    }
    uint64_t ibpBuf[1] = {0};
    fn_InternalBasePtr_Ctor(ibpBuf, result);
    fn_String_Dtor(strBuf);
    return result;
}

// Helper: push entry into a dynamic array with given stride
// arrayPtr/countPtr/capacityPtr point into the TradeUI struct fields
static void TradeUI_DynArrayPush(uintptr_t s, int ptrOff, int countOff, int capOff,
                                  void* entryData, int stride) {
    int oldCount = *(int*)(s + countOff);
    int newCount = oldCount + 1;
    void* oldBuf = *(void**)(s + ptrOff);
    *(int*)(s + countOff) = newCount;

    if (*(int*)(s + capOff) < newCount) {
        int newCap = newCount;
        if (oldCount != 0) newCap = newCount * 3;
        *(int*)(s + capOff) = newCap;
        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * stride, 8);
        *(void**)(s + ptrOff) = newBuf;
    }

    void* curBuf = *(void**)(s + ptrOff);
    int copyStart = (oldBuf == curBuf) ? oldCount : 0;
    for (int k = copyStart; k < newCount; k++) {
        uint8_t* dst = (uint8_t*)curBuf + k * stride;
        uint8_t* src = (uint8_t*)entryData;
        if (k < oldCount) {
            src = (uint8_t*)oldBuf + k * stride;
        }
        for (int b = 0; b < stride; b++) {
            dst[b] = src[b];
        }
    }
    if (oldBuf != curBuf) {
        fn_MemFreeStatic(oldBuf);
    }
}

// Helper: clone template into all table cells
static void TradeUI_CloneTableCells(uintptr_t s, void* tableElem) {
    // Get template object from first child's element at +0xB8
    uint64_t childBuf[2] = {0, 0};
    fn_Element_Child(tableElem, childBuf, 0);
    fn_InternalBasePtr_CheckPtr(childBuf);
    void* firstChild = *(void**)childBuf;
    void* templateObj = *(void**)(*(uintptr_t*)firstChild + 0xB8);

    int totalCells = fn_Element_ChildCount(tableElem);
    for (int i = 0; i < totalCells; i++) {
        // Get dialog resource reference
        void* dialogInst = *(void**)(s + 0x18);
        void* dialogResource = *(void**)((uintptr_t)dialogInst + 0x18);

        // Clone via template vtable[8]: Clone(this, retBuf, dialogResource)
        void** templateVt = *(void***)templateObj;
        typedef void* (*fn_Clone_t)(void*, void*, void*);
        uint64_t cloneBuf[2] = {0, 0};
        ((fn_Clone_t)templateVt[8])(templateObj, cloneBuf, dialogResource);

        fn_TableElement_SetChild2(tableElem, cloneBuf, i);
    }
}

// ============================================================
// Helper: find a description sub-component by typeId
// ============================================================
static void* FindDescComponent(void* descEntity, uint32_t typeId)
{
    if (!descEntity) return nullptr;
    int count = *(int*)((uintptr_t)descEntity + 0x30);
    void** arr = *(void***)((uintptr_t)descEntity + 0x28);
    for (int i = 0; i < count; i++) {
        void* comp = arr[i];
        void** vtbl = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int tid = ((fn_GetTypeId_t)vtbl[2])(comp);
        if ((uint32_t)tid == typeId) {
            if (fn_IsDerivedFrom(comp, typeId))
                return comp;
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: set spinner increment based on max value
// ============================================================
static void SetSpinnerIncrement(void* spinner)
{
    fn_InternalBasePtr_CheckPtr(&spinner);
    int maxVal = fn_SpinnerElement_GetMaxValue(spinner);
    if (maxVal < 0x15) {
        fn_InternalBasePtr_CheckPtr(&spinner);
        fn_SpinnerElement_SetIncrement(spinner, 1);
    } else if (maxVal < 0x33) {
        fn_InternalBasePtr_CheckPtr(&spinner);
        fn_SpinnerElement_SetIncrement(spinner, 5);
    } else if (maxVal < 0x65) {
        fn_InternalBasePtr_CheckPtr(&spinner);
        fn_SpinnerElement_SetIncrement(spinner, 0x14);
    }
    // >= 0x65: no increment set (uses default)
}

// ============================================================
// Helper: set element name from External resource path
// ============================================================
static void SetElementNameFromExternal(void* element, void* externalPtr)
{
    void* path = fn_External_GetPath(externalPtr);
    uint8_t filenameBuf1[16] = {0};
    uint8_t filenameBuf2[16] = {0};
    void* nameStr = fn_Filename_GetPathFilename(path, filenameBuf1, false);
    fn_InternalBasePtr_CheckPtr(&element);
    fn_Element_SetName(element, nameStr);
    fn_Filename_Dtor(filenameBuf1);
    fn_Filename_Dtor(filenameBuf2);
}

// ============================================================
// Helper: virtual call for Element::SetEnabled(this, bool)
// vtable[8] (offset 0x40)
// ============================================================
static inline void VCall_Element_SetEnabled(void* elem, bool enabled)
{
    void** vtbl = *(void***)elem;
    typedef void (*fn_t)(void*, uint8_t);
    ((fn_t)vtbl[8])(elem, enabled ? 1 : 0);
}

// ============================================================
// Helper: virtual call for Element::SetColor(this, uint32_t)
// vtable[10] (offset 0x50)
// ============================================================
static inline void VCall_Element_SetColor(void* elem, uint32_t color)
{
    void** vtbl = *(void***)elem;
    typedef void (*fn_t)(void*, uint32_t);
    ((fn_t)vtbl[10])(elem, color);
}

// ============================================================
// RefreshSellTable (0x140083c70)
// ============================================================
// Rebuilds the sell table data and UI.
//
// Phase 1: Get entity components (Building, StateMachine).
// Phase 2: Iterate resource entries from the game world's resource table.
//          For each resource type, check building's storage BST for availability.
//          If available (count > 0), find ValueDescription + ResourceType sub-components
//          and push a 0x20-byte sell data entry.
// Phase 3: Build state check, then populate sell table UI cells.
//          For each sell entry: set sprite, text, counts, spinner limits, color.
// Phase 4: Clear unused sell table cells.

void TradeUI_RefreshSellTable(TradeUI* self)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity, BuildingComponent, StateMachineComponent
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    // Check TradeDescription on building
    fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), *g_TypeInfo_TradeDescription);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t smPtr[2] = {0, 0};
    fn_FindComponent_SM((void*)entityPtr[0], &smPtr);

    // Get resource table from game world singleton (+0xf0 = resource manager)
    void* resourceMgr = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf0);
    int resourceCount = *(int*)((uintptr_t)resourceMgr + 0x118);

    // Clear sell data array at +0x30 (stride 0x20)
    fn_DynArray_Clear_0x20((void*)(s + 0x30));

    // Phase 2: Iterate resources and build sell entries
    if (resourceCount > 0) {
        for (int ri = 0; ri < resourceCount; ri++) {
            void* valueDesc = nullptr;
            void* resTypeComp = nullptr;

            // Get resource entity from resource table
            uint64_t resEntity = *(uint64_t*)(*(uintptr_t*)((uintptr_t)resourceMgr + 0x110)
                                  + (uintptr_t)ri * 8);

            // Search building's storage BST for this resource
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uintptr_t* bstNode = *(uintptr_t**)(buildingPtr[0] + 0x60);
            bool found = false;
            while (bstNode != nullptr) {
                if (resEntity < *bstNode) {
                    bstNode = (uintptr_t*)bstNode[3];  // left at +0x18? No...
                } else if (resEntity <= *bstNode) {
                    // Found — check if count (at node+0x08) > 0
                    if (bstNode != nullptr && (int)bstNode[1] != 0) {
                        // Find ValueDescription and ResourceType in resource entity
                        int descCount = *(int*)(resEntity + 0x30);
                        void** descArray = *(void***)(resEntity + 0x28);
                        for (int d = 0; d < descCount; d++) {
                            void* comp = descArray[d];
                            void** cvtbl = *(void***)comp;
                            typedef int (*fn_GetTypeId_t)(void*);
                            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
                            if ((uint32_t)tid == *g_TypeId_ResourceType) {
                                if (fn_IsDerivedFrom(comp, *g_TypeId_ResourceType))
                                    resTypeComp = comp;
                                break;
                            }
                        }
                        // Find ValueDescription
                        for (int d = 0; d < descCount; d++) {
                            void* comp = descArray[d];
                            void** cvtbl = *(void***)comp;
                            typedef int (*fn_GetTypeId_t)(void*);
                            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
                            if ((uint32_t)tid == *g_TypeInfo_ValueDescription) {
                                if (fn_IsDerivedFrom(comp, *g_TypeInfo_ValueDescription))
                                    valueDesc = comp;
                                break;
                            }
                        }
                        found = true;
                    }
                    break;
                } else {
                    bstNode = (uintptr_t*)bstNode[4];  // right
                }
            }

            if (!found) continue;

            // Build 0x20-byte sell data entry and push to array at +0x30
            // Entry: {resEntity(8), valueDesc(8), resTypeComp(8), pad(8)}
            uint64_t entry[4] = {resEntity, (uint64_t)valueDesc, (uint64_t)resTypeComp, 0};
            TradeUI_DynArrayPush(s, 0x30, 0x38, 0x3C, entry, 0x20);
        }
    }

    // Phase 3: Get build state, populate sell table UI
    int sellCount = *(int*)(s + 0x38);
    uint64_t sellCountU = (uint64_t)sellCount;

    // Find BuildState component on entity
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    void* buildStateComp = nullptr;
    {
        int compCount = *(int*)(entityPtr[0] + 0x38);
        void** compArr = *(void***)(entityPtr[0] + 0x30);
        for (int i = 0; i < compCount; i++) {
            void* comp = compArr[i];
            void** cvtbl = *(void***)comp;
            typedef int (*fn_GetTypeId_t)(void*);
            int tid = ((fn_GetTypeId_t)cvtbl[2])(comp);
            if ((uint32_t)tid == *g_TypeId_BuildStateComponent) {
                if (fn_IsDerivedFrom(comp, *g_TypeId_BuildStateComponent))
                    buildStateComp = comp;
                break;
            }
        }
    }

    // Check trade state: buildState byte +0x52 (or +0x50 if negative)
    bool tradeActive = false;
    if (buildStateComp) {
        fn_InternalBasePtr_Ctor(&buildStateComp, buildStateComp);
        fn_InternalBasePtr_CheckPtr(&buildStateComp);
        int8_t state52 = *(int8_t*)((uintptr_t)buildStateComp + 0x52);
        int8_t stateVal = state52;
        if (state52 < 0) stateVal = *(int8_t*)((uintptr_t)buildStateComp + 0x50);
        tradeActive = (stateVal == 0x13);
    }

    // Populate sell table cells
    if (sellCount > 0) {
        uintptr_t dataOffset = 0;
        for (int i = 0; i < sellCount; i++) {
            uintptr_t* sellData = (uintptr_t*)(*(uintptr_t*)(s + 0x30) + dataOffset);
            uintptr_t* sellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x60) + dataOffset);

            // Get sellable count from building
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            int sellableCount = fn_Building_GetSellableCount(
                (void*)buildingPtr[0], (void*)sellData[0]);

            bool hasStock = (sellableCount > 0);
            uint32_t color = hasStock ? 0xFFFFFFFF : 0xFF808080;

            // Set sprite from ValueDescription (+0x20=spriteSheet, +0x28=spriteName)
            void* valueDesc = (void*)sellData[1];
            uint8_t spriteBuf[16] = {0};
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)valueDesc + 0x20),
                          (void*)((uintptr_t)valueDesc + 0x28));
            fn_InternalBasePtr_CheckPtr(sellUI);
            fn_LabelElement_SetSprite(*(void**)sellUI, spriteBuf);

            // Set text from ValueDescription (+0x30=stringTable)
            uint8_t uiStrBuf[16] = {0};
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)valueDesc + 0x30),
                            (void*)((uintptr_t)valueDesc + 0x28));
            fn_InternalBasePtr_CheckPtr(sellUI);
            fn_TextElement_SetText(*(void**)sellUI, uiStrBuf);

            // Set sellable count on number element (sellUI+0x10)
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x10));
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)sellUI + 0x10), sellableCount);

            // Search StateMachine's order list for matching resource to get order qty
            int orderQty = 0;
            fn_InternalBasePtr_CheckPtr(&smPtr);
            int smOrderCount = *(int*)(smPtr[0] + 0x30);
            if (smOrderCount > 0) {
                uintptr_t* orderArr = *(uintptr_t**)(smPtr[0] + 0x28);
                for (int j = 0; j < smOrderCount; j++) {
                    if (orderArr[j * 2] == sellData[0]) {
                        if (j >= 0) {
                            orderQty = (int)orderArr[j * 2 + 1]
                                       - *(int*)((uintptr_t)&orderArr[j * 2 + 1] + 4);
                        }
                        break;
                    }
                }
            }

            // Set order qty on number element (sellUI+0x08)
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x08));
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)sellUI + 0x08), orderQty);

            // Set spinner enabled state
            bool spinnerEnabled = tradeActive && hasStock;
            void* spinner = *(void**)((uintptr_t)sellUI + 0x18);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x18));
            VCall_Element_SetEnabled(spinner, spinnerEnabled);

            // Set number and label elements enabled
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x08));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + 0x08), true);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x10));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)sellUI + 0x10), true);

            // Set spinner min/max
            int spinnerMax = 0;
            if (tradeActive && hasStock) {
                spinnerMax = sellableCount;
            }
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x18));
            fn_SpinnerElement_SetMinMax(*(void**)((uintptr_t)sellUI + 0x18), 0, spinnerMax);

            // Set spinner increment based on max value
            void* spinnerElem = *(void**)((uintptr_t)sellUI + 0x18);
            SetSpinnerIncrement(spinnerElem);

            // Reset spinner value to 0
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x18));
            fn_SpinnerElement_SetValue(*(void**)((uintptr_t)sellUI + 0x18), 0);

            // Set element name from resource path
            SetElementNameFromExternal(*(void**)((uintptr_t)sellUI + 0x18),
                                       (void*)sellData[0]);

            // Set color on label, number, and count elements
            fn_InternalBasePtr_CheckPtr(sellUI);
            VCall_Element_SetColor(*(void**)sellUI, color);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x08));
            VCall_Element_SetColor(*(void**)((uintptr_t)sellUI + 0x08), color);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)sellUI + 0x10));
            VCall_Element_SetColor(*(void**)((uintptr_t)sellUI + 0x10), color);

            dataOffset += 0x20;
        }
    }

    // Phase 4: Clear unused sell table cells
    int totalSellCells = 0;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    totalSellCells = *(int*)(*(uintptr_t*)(s + 0xC8) + 0xC0);
    if ((int)sellCountU < totalSellCells) {
        uintptr_t clearOffset = sellCountU << 5;
        for (int i = (int)sellCountU; i < totalSellCells; i++) {
            uintptr_t* cellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x60) + clearOffset);

            // Set empty sprite
            uint8_t emptySprite[16] = {0};
            fn_Sprite_Default_Ctor(emptySprite);
            fn_InternalBasePtr_CheckPtr(cellUI);
            fn_LabelElement_SetSprite(*(void**)cellUI, emptySprite);

            // Set empty text
            uint8_t emptyStr[16] = {0};
            fn_String_Ctor_Default(emptyStr);
            fn_InternalBasePtr_CheckPtr(cellUI);
            fn_TextElement_SetText(*(void**)cellUI, emptyStr);

            // Disable spinner, number, and label elements
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x18));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x18), false);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x08));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x08), false);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x10));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x10), false);

            // Set spinner min/max to 0
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x18));
            fn_SpinnerElement_SetMinMax(*(void**)((uintptr_t)cellUI + 0x18), 0, 0);

            clearOffset += 0x20;
            fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
        }
    }
}

// ============================================================
// RefreshBuyTable (0x140083100)
// ============================================================
// Rebuilds the buy table data and UI.
//
// Phase 1: Get entity components (Building + TradeDescription).
// Phase 2: Set up order header text ("textName"/"textOrder") with
//          trade order name and current merchant info.
// Phase 3: Build buy entries array from trade orders (building+0x50/+0x58).
// Phase 4: Populate buy table UI cells with sprite/text/numbers.
// Phase 5: Clear unused buy table cells.
// Phase 6: Build purchase reserved entries (building+0xb0/+0xb8).
// Phase 7: Set combo and enable state for order combo.
// Phase 8: Populate purchase button array with checked states.

void TradeUI_RefreshBuyTable(TradeUI* self)
{
    uintptr_t s = (uintptr_t)self;

    // Get entity, BuildingComponent, TradeDescription
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    void* tradeDescRaw = *(void**)(buildingPtr[0] + 0x18);
    bool isTrade = fn_IsDerivedFrom(tradeDescRaw, *g_TypeInfo_TradeDescription);
    void* tradeDesc = isTrade ? tradeDescRaw : nullptr;

    // Clear buy data array at +0x40 (stride 0x18)
    fn_DynArray_Clear_0x18_Buy((void*)(s + 0x40));

    // Phase 2: Set order header text elements
    for (int ti = 0; ti < 2; ti++) {
        const char* elemName = (ti == 0) ? "textName" : "textOrder";
        uint8_t nameStr[8] = {0};
        fn_String_Ctor_CStr(nameStr, elemName);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
        uint8_t foundBuf[16] = {0};
        fn_FindElement(*(void**)(s + 0x20), foundBuf, nameStr);
        fn_InternalBasePtr_CheckPtr(foundBuf);

        // IsDerivedFrom TextElement
        void* textElem = nullptr;
        if (*(void**)foundBuf) {
            bool isText = fn_IsDerivedFrom(*(void**)foundBuf, *g_TypeId_TextElement);
            if (isText) textElem = *(void**)foundBuf;
        }
        uint64_t textIbp[1] = {0};
        fn_InternalBasePtr_Ctor(textIbp, textElem);
        fn_String_Dtor(nameStr);

        // Set text parameters based on current order
        fn_InternalBasePtr_CheckPtr(&buildingPtr);
        int orderIdx = *(int*)(buildingPtr[0] + 0x2c);
        if (orderIdx < 0) {
            fn_InternalBasePtr_CheckPtr(textIbp);
            // No active order — text stays default
        } else {
            // Get order entry from trade description
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uintptr_t orderEntry = *(uintptr_t*)((uintptr_t)tradeDesc + 0x48)
                                   + (uintptr_t)orderIdx * 0x48;

            // Set parameter 0: order name string
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uint8_t orderNameBuf[8] = {0};
            fn_Building_GetOrderName((void*)buildingPtr[0], orderNameBuf);
            fn_InternalBasePtr_CheckPtr(textIbp);
            fn_TextElement_SetParameter((void*)*textIbp, 0, orderNameBuf);
            fn_String_Dtor(orderNameBuf);

            // Set parameter 1: UI string from order entry
            uint8_t uiStrBuf[16] = {0};
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)orderEntry,
                            (void*)(orderEntry + 8));
            // Convert to System::String
            uint8_t sysStrBuf[8] = {0};
            fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);
            fn_InternalBasePtr_CheckPtr(textIbp);
            fn_TextElement_SetParameter((void*)*textIbp, 1, sysStrBuf);
            fn_String_Dtor(sysStrBuf);
        }

        // Set enabled state based on order validity
        fn_InternalBasePtr_CheckPtr(textIbp);
        VCall_Element_SetColor((void*)*textIbp, 0xFFFFFFFF);
    }

    // Phase 3: Build buy entries from trade orders
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int tradeOrderCount = *(int*)(buildingPtr[0] + 0x58);
    if (tradeOrderCount > 0) {
        for (int oi = 0; oi < tradeOrderCount; oi++) {
            // Get trade order entry
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uintptr_t orderList = *(uintptr_t*)(buildingPtr[0] + 0x50);
            uintptr_t orderEntry = *(uintptr_t*)(orderList + (uintptr_t)oi * 0x10);

            // Find ValueDescription in order entry's description
            void* valDesc = FindDescComponent((void*)orderEntry, *g_TypeInfo_ValueDescription);

            // Build 0x18-byte buy data entry
            uint64_t buyEntry[3] = {orderEntry, (uint64_t)valDesc, 0};
            TradeUI_DynArrayPush(s, 0x40, 0x48, 0x4C, buyEntry, 0x18);
        }
    }

    // Phase 4: Populate buy table UI cells
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int buyCount = *(int*)(buildingPtr[0] + 0x58);
    uint64_t buyCountU = (uint64_t)buyCount;

    if (buyCount > 0) {
        uintptr_t dataOffset = 0;
        uintptr_t buyDataOffset = 0;
        for (int i = 0; i < buyCount; i++) {
            uintptr_t* buyUI = (uintptr_t*)(*(uintptr_t*)(s + 0x70) + dataOffset);

            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uintptr_t* buyOrder = (uintptr_t*)(*(uintptr_t*)(buildingPtr[0] + 0x50) + buyDataOffset);
            uintptr_t orderDesc = *buyOrder;

            // Find ValueDescription
            void* valDesc = FindDescComponent((void*)orderDesc, *g_TypeInfo_ValueDescription);

            // Set sprite from ValueDescription
            uint8_t spriteBuf[16] = {0};
            fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)valDesc + 0x20),
                          (void*)((uintptr_t)valDesc + 0x28));
            fn_InternalBasePtr_CheckPtr(buyUI);
            fn_LabelElement_SetSprite(*(void**)buyUI, spriteBuf);

            // Set text from ValueDescription
            uint8_t uiStrBuf[16] = {0};
            fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)valDesc + 0x30),
                            (void*)((uintptr_t)valDesc + 0x38));
            fn_InternalBasePtr_CheckPtr(buyUI);
            fn_TextElement_SetText(*(void**)buyUI, uiStrBuf);

            // Set price number (buyUI+0x18 = price element)
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x18));
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)buyUI + 0x18),
                                        *(int*)((uintptr_t)buyOrder + 0x0C));

            // Set quantity number (buyUI+0x08)
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x08));
            fn_NumberElement_SetValueInt(*(void**)((uintptr_t)buyUI + 0x08),
                                        (int)buyOrder[1]);

            // Set spinner min/max (buyUI+0x10)
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x10));
            fn_SpinnerElement_SetMinMax(*(void**)((uintptr_t)buyUI + 0x10),
                                        0, *(int*)((uintptr_t)buyOrder + 0x0C));

            // Set spinner increment
            void* spinnerElem = *(void**)((uintptr_t)buyUI + 0x10);
            SetSpinnerIncrement(spinnerElem);

            // Set element name from resource path
            SetElementNameFromExternal(*(void**)((uintptr_t)buyUI + 0x10),
                                       (void*)*buyOrder);

            // Enable all sub-elements
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x10));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x10), true);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x18));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x18), true);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)buyUI + 0x08));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)buyUI + 0x08), true);

            dataOffset += 0x20;
            buyDataOffset += 0x10;
        }
    }

    // Phase 5: Clear unused buy table cells
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    int totalBuyCells = *(int*)(*(uintptr_t*)(s + 0xD0) + 0xC0);
    if ((int)buyCountU < totalBuyCells) {
        uintptr_t clearOffset = buyCountU << 5;
        for (int i = (int)buyCountU; i < totalBuyCells; i++) {
            uintptr_t* cellUI = (uintptr_t*)(*(uintptr_t*)(s + 0x70) + clearOffset);

            // Set empty sprite
            uint8_t emptySprite[16] = {0};
            fn_Sprite_Default_Ctor(emptySprite);
            fn_InternalBasePtr_CheckPtr(cellUI);
            fn_LabelElement_SetSprite(*(void**)cellUI, emptySprite);

            // Set empty text
            uint8_t emptyStr[16] = {0};
            fn_String_Ctor_Default(emptyStr);
            fn_InternalBasePtr_CheckPtr(cellUI);
            fn_TextElement_SetText(*(void**)cellUI, emptyStr);

            // Set spinner min/max to 0
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x10));
            fn_SpinnerElement_SetMinMax(*(void**)((uintptr_t)cellUI + 0x10), 0, 0);

            // Disable elements
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x10));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x10), false);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x18));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x18), false);
            fn_InternalBasePtr_CheckPtr((void*)((uintptr_t)cellUI + 0x08));
            VCall_Element_SetEnabled(*(void**)((uintptr_t)cellUI + 0x08), false);

            clearOffset += 0x20;
            fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
        }
    }

    // Phase 6: Build purchase reserved entries (building+0xb0/+0xb8)
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int orderIdx = *(int*)(buildingPtr[0] + 0x2c);
    if (orderIdx < 0) {
        // No active order — clear purchase list
        fn_MemFreeStatic(*(void**)(s + 0xb0));
        *(uint64_t*)(s + 0xb0) = 0;
        *(uint64_t*)(s + 0xb8) = 0;
    } else {
        // Populate purchase reserved entries via helper
        fn_InternalBasePtr_CheckPtr(&buildingPtr);
        fn_Building_GetBuyEntries((void*)buildingPtr[0], (void*)(s + 0xb0));
    }

    // Phase 7: Set order combo current item and enabled state
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int comboIdx = 0;
    if (*(int*)(buildingPtr[0] + 0x2c) >= 0) {
        comboIdx = *(int*)(*(uintptr_t*)(buildingPtr[0] + 0x30)
                    + (uintptr_t)*(int*)(buildingPtr[0] + 0x2c) * 0x20 + 8);
    }
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0xC0), comboIdx);

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
    void* comboElem = *(void**)(s + 0xC0);
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    bool comboEnabled = (*(int*)(buildingPtr[0] + 0x2c) >= 0);
    VCall_Element_SetEnabled(comboElem, comboEnabled);

    // Phase 8: Populate purchase button array with checked states
    int purchaseCount = *(int*)(s + 0xb8);
    if (purchaseCount > 0) {
        uintptr_t offset = 0;
        for (int pi = 0; pi < purchaseCount; pi++) {
            uintptr_t purchaseEntity = *(uintptr_t*)(offset + *(uintptr_t*)(s + 0xb0));

            // Find ValueDescription for this purchase entity
            void* valDesc = FindDescComponent((void*)purchaseEntity, *g_TypeInfo_ValueDescription);

            // Set sprite and text on purchase button element
            void* purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            fn_InternalBasePtr_CheckPtr(purchaseUI);
            int childCount = fn_Element_ChildCount(*(void**)purchaseUI);

            for (int ci = 0; ci < childCount; ci++) {
                void* purchaseUIPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                fn_InternalBasePtr_CheckPtr(purchaseUIPtr);
                uint64_t childBuf[2] = {0, 0};
                fn_Element_Child(*(void**)purchaseUIPtr, childBuf, ci);
                fn_InternalBasePtr_CheckPtr(childBuf);

                // IsDerivedFrom TextElement check
                void* childElem = nullptr;
                if (*(void**)childBuf) {
                    bool isText = fn_IsDerivedFrom(*(void**)childBuf, *g_TypeId_TextElement);
                    if (isText) childElem = *(void**)childBuf;
                }
                uint64_t childIbp[1] = {0};
                fn_InternalBasePtr_Ctor(childIbp, childElem);

                if ((void*)*childIbp != nullptr) {
                    // Set sprite
                    uint8_t spriteBuf[16] = {0};
                    fn_Sprite_Ctor(spriteBuf, *(uint64_t*)((uintptr_t)valDesc + 0x20),
                                  (void*)((uintptr_t)valDesc + 0x28));
                    fn_InternalBasePtr_CheckPtr(childIbp);
                    fn_LabelElement_SetSprite((void*)*childIbp, spriteBuf);

                    // Set text
                    uint8_t uiStrBuf[16] = {0};
                    fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)valDesc + 0x30),
                                    (void*)((uintptr_t)valDesc + 0x38));
                    fn_InternalBasePtr_CheckPtr(childIbp);
                    fn_TextElement_SetText((void*)*childIbp, uiStrBuf);
                }

                purchaseUIPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                fn_InternalBasePtr_CheckPtr(purchaseUIPtr);
                childCount = fn_Element_ChildCount(*(void**)purchaseUIPtr);
            }

            // Enable purchase button
            void* purchaseBtn = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            fn_InternalBasePtr_CheckPtr(purchaseBtn);
            VCall_Element_SetEnabled(*(void**)purchaseBtn, true);

            // Check if this purchase entity is in the current order's selection list
            fn_InternalBasePtr_CheckPtr(&buildingPtr);
            uintptr_t orderData = (uintptr_t)*(int*)(buildingPtr[0] + 0x2c) * 0x20
                                  + *(uintptr_t*)(buildingPtr[0] + 0x30);
            int selCount = *(int*)(orderData + 0x18);
            if (selCount > 0) {
                uintptr_t* selList = *(uintptr_t**)(orderData + 0x10);
                for (int si = 0; si < selCount; si++) {
                    if (selList[si] == *(uintptr_t*)(*(uintptr_t*)(s + 0xb0) + offset)) {
                        void* purchaseBtnPtr = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                        fn_InternalBasePtr_CheckPtr(purchaseBtnPtr);
                        *(uint8_t*)(*(uintptr_t*)purchaseBtnPtr + 0xd8) = 1;
                        break;
                    }
                }
            }

            offset += 8;
        }
    }

    // Clear unused purchase buttons
    int totalPurchaseSlots = *(int*)(s + 0xa8);
    if (purchaseCount < totalPurchaseSlots) {
        uintptr_t offset = (uintptr_t)purchaseCount * 8;
        for (int i = purchaseCount; i < totalPurchaseSlots; i++) {
            // Clear children
            int ci = 0;
            void* purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            fn_InternalBasePtr_CheckPtr(purchaseUI);
            int childCount = fn_Element_ChildCount(*(void**)purchaseUI);
            while (ci < childCount) {
                purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                fn_InternalBasePtr_CheckPtr(purchaseUI);
                uint64_t childBuf[2] = {0, 0};
                fn_Element_Child(*(void**)purchaseUI, childBuf, ci);
                fn_InternalBasePtr_CheckPtr(childBuf);

                void* childElem = nullptr;
                if (*(void**)childBuf) {
                    bool isText = fn_IsDerivedFrom(*(void**)childBuf, *g_TypeId_TextElement);
                    if (isText) childElem = *(void**)childBuf;
                }
                uint64_t childIbp[1] = {0};
                fn_InternalBasePtr_Ctor(childIbp, childElem);

                if ((void*)*childIbp != nullptr) {
                    uint8_t emptySprite[16] = {0};
                    fn_Sprite_Default_Ctor(emptySprite);
                    fn_InternalBasePtr_CheckPtr(childIbp);
                    fn_LabelElement_SetSprite((void*)*childIbp, emptySprite);

                    uint8_t emptyStr[16] = {0};
                    fn_String_Ctor_Default(emptyStr);
                    fn_InternalBasePtr_CheckPtr(childIbp);
                    fn_TextElement_SetText((void*)*childIbp, emptyStr);
                }

                ci++;
                purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
                fn_InternalBasePtr_CheckPtr(purchaseUI);
                childCount = fn_Element_ChildCount(*(void**)purchaseUI);
            }

            // Disable button and clear checked state
            purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            fn_InternalBasePtr_CheckPtr(purchaseUI);
            void* btn = *(void**)purchaseUI;
            VCall_Element_SetEnabled(btn, false);

            purchaseUI = (void*)(*(uintptr_t*)(s + 0xa0) + offset);
            fn_InternalBasePtr_CheckPtr(purchaseUI);
            *(uint8_t*)(*(uintptr_t*)purchaseUI + 0xd8) = 0;

            offset += 8;
        }
    }
}

void TradeUI_Serialize(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Get entity and find BuildingComponent ---
    void* entityRaw = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entityRaw, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entityRaw : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // --- Phase 1: Find all 15 named elements ---
    void* dialogElemPtr = (void*)(s + 0x20);

    // Tables (IsDerivedFrom TableElement)
    *(uintptr_t*)(s + 0xC8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableSell", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xD0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableBuy", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xD8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableStorage", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xE0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tablePurchase", g_TypeId_TableElement);
    *(uintptr_t*)(s + 0xE8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "tableOrder", g_TypeId_TableElement);

    // Number elements (IsDerivedFrom SpinnerElement/NumberElement)
    *(uintptr_t*)(s + 0x100) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "numberSellTotal", g_TypeId_SpinnerElement);
    *(uintptr_t*)(s + 0x108) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "numberBuyTotal", g_TypeId_SpinnerElement);

    // Buttons (IsDerivedFrom ButtonElement)
    *(uintptr_t*)(s + 0xF0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "buttonTrade", g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xF0));
    fn_Element_SetElementId(*(void**)(s + 0xF0), 0x4e20);

    *(uintptr_t*)(s + 0xF8) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "buttonDismiss", g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xF8));
    fn_Element_SetElementId(*(void**)(s + 0xF8), 0x4e21);

    // Text elements (IsDerivedFrom TextElement)
    *(uintptr_t*)(s + 0x110) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textNoStorage", g_TypeId_TextElement);
    *(uintptr_t*)(s + 0x118) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textTooMuch", g_TypeId_TextElement);
    *(uintptr_t*)(s + 0x120) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "textNotEnough", g_TypeId_TextElement);

    // Combo elements (IsDerivedFrom ComboElement)
    *(uintptr_t*)(s + 0x138) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "comboPurchase", g_TypeId_HelpComboElem);
    // Set comboPurchase current item from building component field
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int purchaseIdx = *(int*)(buildingPtr[0] + 0x28);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x138));
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0x138), purchaseIdx);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x138));
    fn_Element_SetElementId(*(void**)(s + 0x138), 0x4e22);

    *(uintptr_t*)(s + 0xC0) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "comboOrder", g_TypeId_HelpComboElem);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
    fn_Element_SetElementId(*(void**)(s + 0xC0), 0x4e23);

    *(uintptr_t*)(s + 0x140) = (uintptr_t)TradeUI_FindTypedElement(
        dialogElemPtr, "sortCombo", g_TypeId_HelpComboElem);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x140));
    fn_Element_SetElementId(*(void**)(s + 0x140), 199);  // 0xC7
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x140));
    fn_ComboElement_SetCurrentItem(*(void**)(s + 0x140), 0);

    // --- Phase 2: Build tablePurchase row array (stride 0x20, 4 elements per row) ---
    // Each row has child elements accessed by index: 0=labelPurchase, 1=numberPurchase,
    // 2=buttonPurchase(SpinnerElement), 3=amountSpinner(SpinnerElement)
    // Element IDs: label=baseId-1000, number=baseId, spinner=baseId+1000
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xE0));
    void* tablePurchase = *(void**)(s + 0xE0);
    int purchaseChildCount = *(int*)((uintptr_t)tablePurchase + 0xC0);

    int baseId = 6000;
    for (int i = 0; i < purchaseChildCount; i++) {
        // Get child row i from tablePurchase
        uint64_t childBuf[2] = {0, 0};
        fn_Element_Child(tablePurchase, childBuf, i);
        fn_InternalBasePtr_CheckPtr(childBuf);
        void* rowElem = *(void**)childBuf;

        // Get child(0) -> child(0) => cast to TableElement (label)
        uint64_t buf0[2] = {0, 0};
        fn_Element_Child(rowElem, buf0, 0);
        fn_InternalBasePtr_CheckPtr(buf0);
        uint64_t buf0b[2] = {0, 0};
        fn_Element_Child(*(void**)buf0, buf0b, 0);
        fn_InternalBasePtr_CheckPtr(buf0b);
        uint64_t r0[2], r1[2];

        // child(0) -> child(0) = label element
        fn_Element_Child(rowElem, r0, 0);
        fn_InternalBasePtr_CheckPtr(r0);
        fn_Element_Child(*(void**)r0, r1, 0);
        fn_InternalBasePtr_CheckPtr(r1);
        void* label = nullptr;
        if (*(void**)r1 && g_TypeId_TableElement) {
            if (fn_IsDerivedFrom(*(void**)r1, *g_TypeId_TableElement))
                label = *(void**)r1;
        }

        // child(0) -> child(1) = number element
        fn_Element_Child(rowElem, r0, 0);
        fn_InternalBasePtr_CheckPtr(r0);
        fn_Element_Child(*(void**)r0, r1, 1);
        fn_InternalBasePtr_CheckPtr(r1);
        void* number = nullptr;
        if (*(void**)r1 && g_TypeId_TableElement) {
            if (fn_IsDerivedFrom(*(void**)r1, *g_TypeId_TableElement))
                number = *(void**)r1;
        }

        // child(1) = button/text element
        fn_Element_Child(rowElem, r0, 1);
        fn_InternalBasePtr_CheckPtr(r0);
        void* button = nullptr;
        if (*(void**)r0 && g_TypeId_TableElement) {
            if (fn_IsDerivedFrom(*(void**)r0, *g_TypeId_TableElement))
                button = *(void**)r0;
        }

        // child(2) = spinner element
        fn_Element_Child(rowElem, r0, 2);
        fn_InternalBasePtr_CheckPtr(r0);
        void* spinner = nullptr;
        if (*(void**)r0 && g_TypeId_SpinnerElement) {
            if (fn_IsDerivedFrom(*(void**)r0, *g_TypeId_SpinnerElement))
                spinner = *(void**)r0;
        }

        // Set element IDs
        fn_InternalBasePtr_Ctor(r0, label);
        fn_InternalBasePtr_CheckPtr(r0);
        fn_Element_SetElementId(*(void**)r0, baseId - 1000);

        fn_InternalBasePtr_Ctor(r0, number);
        fn_InternalBasePtr_CheckPtr(r0);
        fn_Element_SetElementId(*(void**)r0, baseId);

        fn_InternalBasePtr_Ctor(r0, spinner);
        fn_InternalBasePtr_CheckPtr(r0);
        fn_SpinnerElement_SetElementId(*(void**)r0, baseId + 1000);

        // Build 0x20-byte entry: {label, number, button, spinner}
        uint64_t entry[4] = {(uint64_t)label, (uint64_t)number, (uint64_t)button, (uint64_t)spinner};
        TradeUI_DynArrayPush(s, 0x90, 0x98, 0x9C, entry, 0x20);

        baseId++;
    }

    // --- Phase 3: Count resources and selectable builds via ExternalFactory ---
    int resourceTypeCount = 0;
    void* factory = *g_pExternalFactorySingleton;
    void* ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_ResourceType, nullptr);
    while (ext != nullptr) {
        resourceTypeCount++;
        ext = fn_ExternalFactory_FindNext(factory, *g_TypeId_ResourceType, ext);
    }

    int selectableCount = 0;
    void* buildExt = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, nullptr);
    while (buildExt != nullptr) {
        // Check if this BuildComponent external has a SelectDescription sub-component
        bool isDerivedBuild = fn_IsDerivedFrom(buildExt, *g_TypeId_BuildComponent);
        void* buildComp = isDerivedBuild ? buildExt : nullptr;
        if (buildComp) {
            int compCount = *(int*)((uintptr_t)buildComp + 0x30);
            void** compArray = *(void***)((uintptr_t)buildComp + 0x28);
            for (int c = 0; c < compCount; c++) {
                void* comp = compArray[c];
                typedef int (*GetTypeId_t)(void*);
                uintptr_t vtbl = *(uintptr_t*)comp;
                int typeId = ((GetTypeId_t)(*(void**)(vtbl + 0x10)))(comp);
                if (typeId == (int)*g_TypeId_SelectDescription) {
                    void* selectComp = nullptr;
                    if (fn_IsDerivedFrom(comp, *g_TypeId_SelectDescription))
                        selectComp = comp;
                    if (selectComp) selectableCount++;
                    break;
                }
            }
        }
        buildExt = fn_ExternalFactory_FindNext(factory, *g_TypeId_BuildComponent, buildExt);
    }

    // --- Phase 4a: Setup tableStorage ---
    void* tableStorage = *(void**)(s + 0xD8);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD8));
    int storageColCount = *(int*)((uintptr_t)tableStorage + 0xC4);

    // Get template from first child
    {
        // Resize table: rows = ceil(resourceTypeCount / colCount)
        int rowCount = ((resourceTypeCount - 1 + storageColCount) / storageColCount * storageColCount)
                       / storageColCount;
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xD8));
        fn_TableElement_SetSize(*(void**)(s + 0xD8), rowCount, storageColCount);

        // Clone template into all cells
        TradeUI_CloneTableCells(s, *(void**)(s + 0xD8));
    }

    // Iterate tableStorage cells, find "ribbonStorageRow" sub-elements
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD8));
    tableStorage = *(void**)(s + 0xD8);
    int storageCellCount = fn_Element_ChildCount(tableStorage);
    for (int i = 0; i < storageCellCount; i++) {
        // Find "ribbonStorageRow" in cell
        uint64_t strBuf[2] = {0, 0};
        fn_String_Ctor_CStr(strBuf, "ribbonStorageRow");
        uint64_t cellBuf[2] = {0, 0};
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xD8));
        fn_Element_Child(*(void**)(s + 0xD8), cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);
        uint64_t rowBuf[2] = {0, 0};
        fn_FindElement(*(void**)cellBuf, rowBuf, strBuf);
        fn_String_Dtor(strBuf);

        // Get 3 child elements from row: child(0), child(1), child(2)
        fn_InternalBasePtr_CheckPtr(rowBuf);
        void* rowElem = *(void**)rowBuf;

        uint64_t c0[2] = {0, 0}, c1[2] = {0, 0}, c2[2] = {0, 0};
        fn_Element_Child(rowElem, c0, 0);
        fn_InternalBasePtr_CheckPtr(c0);
        void* e0 = nullptr;
        if (*(void**)c0 && g_TypeId_TableElement) {
            if (fn_IsDerivedFrom(*(void**)c0, *g_TypeId_TableElement)) e0 = *(void**)c0;
        }

        fn_Element_Child(rowElem, c1, 1);
        fn_InternalBasePtr_CheckPtr(c1);
        void* e1 = nullptr;
        if (*(void**)c1 && g_TypeId_SpinnerElement) {
            if (fn_IsDerivedFrom(*(void**)c1, *g_TypeId_SpinnerElement)) e1 = *(void**)c1;
        }

        fn_Element_Child(rowElem, c2, 2);
        fn_InternalBasePtr_CheckPtr(c2);
        void* e2 = nullptr;
        if (*(void**)c2 && g_TypeId_TableElement) {
            if (fn_IsDerivedFrom(*(void**)c2, *g_TypeId_TableElement)) e2 = *(void**)c2;
        }

        // Set spinner element ID
        fn_InternalBasePtr_Ctor(c0, e1);
        fn_InternalBasePtr_CheckPtr(c0);
        fn_SpinnerElement_SetElementId(*(void**)c0, i + 2000);

        // Build 0x18-byte entry: {e0, e1, e2}
        uint64_t entry[3] = {(uint64_t)e0, (uint64_t)e1, (uint64_t)e2};
        TradeUI_DynArrayPush(s, 0x80, 0x88, 0x8C, entry, 0x18);
    }

    // --- Phase 4b: Setup tableSell ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    void* tableSell = *(void**)(s + 0xC8);
    int sellColCount = *(int*)((uintptr_t)tableSell + 0xC4);

    {
        int rowCount = (((resourceTypeCount - 1 + sellColCount) / sellColCount) * sellColCount)
                       / sellColCount;
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
        fn_TableElement_SetSize(*(void**)(s + 0xC8), rowCount, sellColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xC8));
    }

    // Iterate tableSell cells, find "ribbonSellRow" sub-elements (4 children per row)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    tableSell = *(void**)(s + 0xC8);
    int sellCellCount = *(int*)((uintptr_t)tableSell + 0xC0);
    for (int i = 0; i < sellCellCount; i++) {
        uint64_t strBuf[2] = {0, 0};
        fn_String_Ctor_CStr(strBuf, "ribbonSellRow");
        uint64_t cellBuf[2] = {0, 0};
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
        fn_Element_Child(*(void**)(s + 0xC8), cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);
        uint64_t rowBuf[2] = {0, 0};
        fn_FindElement(*(void**)cellBuf, rowBuf, strBuf);
        fn_String_Dtor(strBuf);

        fn_InternalBasePtr_CheckPtr(rowBuf);
        void* rowElem = *(void**)rowBuf;

        // Get 4 child elements: child(0)=label, child(1)=numberLabel,
        // child(2)=amountLabel, child(3)=spinner
        uint64_t c[2];
        void* label = nullptr;
        fn_Element_Child(rowElem, c, 0);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_SpinnerElement))
            label = *(void**)c;

        void* numberLbl = nullptr;
        fn_Element_Child(rowElem, c, 1);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_TableElement))
            numberLbl = *(void**)c;

        void* amountLbl = nullptr;
        fn_Element_Child(rowElem, c, 2);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_TableElement))
            amountLbl = *(void**)c;

        void* spinner = nullptr;
        fn_Element_Child(rowElem, c, 3);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_SpinnerElement))
            spinner = *(void**)c;

        // Set spinner element ID
        fn_InternalBasePtr_Ctor(c, spinner);
        fn_InternalBasePtr_CheckPtr(c);
        fn_SpinnerElement_SetElementId(*(void**)c, i + 3000);

        // Build 0x20-byte entry: {numberLbl, label, amountLbl, spinner}
        uint64_t entry[4] = {(uint64_t)numberLbl, (uint64_t)label,
                              (uint64_t)amountLbl, (uint64_t)spinner};
        TradeUI_DynArrayPush(s, 0x60, 0x68, 0x6C, entry, 0x20);
    }

    // --- Phase 4c: Setup tableBuy ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    void* tableBuy = *(void**)(s + 0xD0);
    int buyColCount = *(int*)((uintptr_t)tableBuy + 0xC4);

    {
        // tableBuy size: rows for (selectableCount + resourceTypeCount - 1) items
        int totalItems = selectableCount + resourceTypeCount - 1;

        int rowCount = (((totalItems + buyColCount) / buyColCount) * buyColCount) / buyColCount;
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
        fn_TableElement_SetSize(*(void**)(s + 0xD0), rowCount, buyColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xD0));
    }

    // Iterate tableBuy cells, find "ribbonBuyRow" sub-elements (4 children per row)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    tableBuy = *(void**)(s + 0xD0);
    int buyCellCount = *(int*)((uintptr_t)tableBuy + 0xC0);
    for (int i = 0; i < buyCellCount; i++) {
        uint64_t strBuf[2] = {0, 0};
        fn_String_Ctor_CStr(strBuf, "ribbonBuyRow");
        uint64_t cellBuf[2] = {0, 0};
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
        fn_Element_Child(*(void**)(s + 0xD0), cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);
        uint64_t rowBuf[2] = {0, 0};
        fn_FindElement(*(void**)cellBuf, rowBuf, strBuf);
        fn_String_Dtor(strBuf);

        fn_InternalBasePtr_CheckPtr(rowBuf);
        void* rowElem = *(void**)rowBuf;

        // 4 children: child(0)=spinner, child(1)=label, child(2)=numberLbl, child(3)=amountSpinner
        uint64_t c[2];
        void* spinner = nullptr;
        fn_Element_Child(rowElem, c, 0);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_SpinnerElement))
            spinner = *(void**)c;

        void* label = nullptr;
        fn_Element_Child(rowElem, c, 1);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_TableElement))
            label = *(void**)c;

        void* numberLbl = nullptr;
        fn_Element_Child(rowElem, c, 2);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_TableElement))
            numberLbl = *(void**)c;

        void* amountSpinner = nullptr;
        fn_Element_Child(rowElem, c, 3);
        fn_InternalBasePtr_CheckPtr(c);
        if (*(void**)c && fn_IsDerivedFrom(*(void**)c, *g_TypeId_SpinnerElement))
            amountSpinner = *(void**)c;

        // Set spinner element ID
        fn_InternalBasePtr_Ctor(c, amountSpinner);
        fn_InternalBasePtr_CheckPtr(c);
        fn_SpinnerElement_SetElementId(*(void**)c, i + 4000);

        // Build 0x20-byte entry: {label, spinner, numberLbl, amountSpinner}
        uint64_t entry[4] = {(uint64_t)label, (uint64_t)spinner,
                              (uint64_t)numberLbl, (uint64_t)amountSpinner};
        TradeUI_DynArrayPush(s, 0x70, 0x78, 0x7C, entry, 0x20);
    }

    // --- Phase 4d: Setup tableOrder ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xE8));
    void* tableOrder = *(void**)(s + 0xE8);
    uint32_t orderColCount = *(uint32_t*)((uintptr_t)tableOrder + 0xC4);

    {
        int totalOrderItems = selectableCount + orderColCount + resourceTypeCount - 1;

        int rowCount = ((int)(totalOrderItems / (int)orderColCount * (int)orderColCount))
                       / (int)orderColCount;
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xE8));
        fn_TableElement_SetSize(*(void**)(s + 0xE8), rowCount, (int)orderColCount);

        TradeUI_CloneTableCells(s, *(void**)(s + 0xE8));
    }

    // Iterate tableOrder cells, set element IDs (0x1F40+i) on purchase button children
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xE8));
    tableOrder = *(void**)(s + 0xE8);
    int orderCellCount = fn_Element_ChildCount(tableOrder);
    for (int i = 0; i < orderCellCount; i++) {
        uint64_t cellBuf[2] = {0, 0};
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xE8));
        fn_Element_Child(*(void**)(s + 0xE8), cellBuf, i);
        fn_InternalBasePtr_CheckPtr(cellBuf);

        void* child = *(void**)cellBuf;
        void* typed = nullptr;
        if (child && g_TypeId_HelpButtonElem) {
            if (fn_IsDerivedFrom(child, *g_TypeId_HelpButtonElem))
                typed = child;
        }
        uint64_t ibp[1] = {0};
        fn_InternalBasePtr_Ctor(ibp, typed);
        fn_InternalBasePtr_CheckPtr(ibp);
        fn_Element_SetElementId(*(void**)ibp, i + 0x1F40);

        // Push 8-byte entry (just the element pointer) into purchase array
        uint64_t entry[1] = {*(uint64_t*)ibp};
        TradeUI_DynArrayPush(s, 0xA0, 0xA8, 0xAC, entry, 0x08);
    }

    // --- Phase 5: Call refresh functions ---
    TradeUI_RefreshDisplay(self, 0, 0, 0);
    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshBuyTable(self);
}

// ============================================================
// Startup (vtable +0x38): Trade value calculations
// ============================================================
// Original: 0x140082150
//
// Calculates total sell/buy values from order arrays and resource values.
// Checks storage capacity, shows/hides warning text (textNoStorage,
// textTooMuch, textNotEnough). Enables/disables trade and dismiss buttons
// based on trade state and merchant presence.
//
// sell orders: self+0x30 = ptr, self+0x38 = count, stride 0x20
//   entry[i]: +0x00 = resource entity ptr, +0x18 = quantity int
// buy orders: self+0x40 = ptr, self+0x48 = count, stride 0x18
//   entry[j]: +0x00 = resource entity ptr, +0x10 = quantity int
// resource entity: +0x28 = component array ptr, +0x30 = component count
//   component value at +0x48 (identified by vtable+0x10 returning g_TypeId_ResourceType)
// merchant state check: BuildState component, byte at +0x52 (or +0x50 if negative)
//   0x13 = merchant arrived (enables dismiss button)
// storage capacity: StateMachineComp+0x38 vs StorageComponent_QueryCount
//   bStorageFull = capacity < storageCount + (buyTotal - sellTotal)

// Inline helper to call UI::Element::SetEnabled via vtable slot 8 (+0x40)
static inline void Element_SetEnabled(void* element, bool enabled) {
    typedef void (*SetEnabled_t)(void* self, int enabled);
    uintptr_t vtbl = *(uintptr_t*)element;
    SetEnabled_t fn = *(SetEnabled_t*)(vtbl + 0x40);
    fn(element, enabled ? 1 : 0);
}

// Find resource value for a given resource entity ptr by searching its component array
// Returns 0 if not found. Resource type identified via vtable+0x10 == g_TypeId_ResourceType.
static int FindResourceValue(void* resourceEntity) {
    if (!resourceEntity || !g_TypeId_ResourceType) return 0;
    uintptr_t entity = (uintptr_t)resourceEntity;
    int compCount = *(int*)(entity + 0x30);
    void** compArray = *(void***)(entity + 0x28);
    for (int k = 0; k < compCount; k++) {
        void* comp = compArray[k];
        if (!comp) continue;
        // Call vtable+0x10 to get type ID
        typedef int (*GetTypeId_t)(void* self);
        uintptr_t vtbl = *(uintptr_t*)comp;
        GetTypeId_t getType = *(GetTypeId_t*)(vtbl + 0x10);
        int typeId = getType(comp);
        if (typeId == (int)*g_TypeId_ResourceType) {
            // Verify with IsDerivedFrom, then return value at +0x48
            bool isRes = fn_IsDerivedFrom(comp, *g_TypeId_ResourceType);
            if (isRes) return *(int*)((uintptr_t)comp + 0x48);
            break;
        }
    }
    return 0;
}

void TradeUI_Startup(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find BuildingComponent and StateMachineComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t stateMachinePtr[2] = {0, 0};
    fn_FindComponent_StateMachine((void*)entityPtr[0], &stateMachinePtr);

    // --- Compute sell total ---
    // sell orders: ptr at self+0x30, count at self+0x38, stride 0x20
    // each entry: +0x00=resource entity ptr, +0x18=quantity
    int sellTotal = 0;
    int sellCount = *(int*)(s + 0x38);
    uintptr_t sellBase = *(uintptr_t*)(s + 0x30);
    for (int i = 0; i < sellCount; i++) {
        uintptr_t entry = sellBase + (uintptr_t)i * 0x20;
        int qty = *(int*)(entry + 0x18);
        if (qty != 0) {
            void* resEntity = *(void**)(entry);
            int resVal = FindResourceValue(resEntity);
            sellTotal += resVal * qty;
        }
    }

    // --- Compute buy total ---
    // buy orders: ptr at self+0x40, count at self+0x48, stride 0x18
    // each entry: +0x00=resource entity ptr, +0x10=quantity
    int buyTotal = 0;
    int buyCount = *(int*)(s + 0x48);
    uintptr_t buyBase = *(uintptr_t*)(s + 0x40);
    for (int j = 0; j < buyCount; j++) {
        uintptr_t entry = buyBase + (uintptr_t)j * 0x18;
        int qty = *(int*)(entry + 0x10);
        if (qty != 0) {
            void* resEntity = *(void**)(entry);
            int resVal = FindResourceValue(resEntity);
            buyTotal += resVal * qty;
        }
    }

    // --- Read displayed totals from NumberElements ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    int displaySellTotal = *(int*)(*(uintptr_t*)(s + 0x100) + 0xd8);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
    int displayBuyTotal  = *(int*)(*(uintptr_t*)(s + 0x108) + 0xd8);

    bool bOverBought  = displaySellTotal < displayBuyTotal;  // buy value > sell value
    bool bBuySell     = displayBuyTotal  < displaySellTotal; // buy < sell (textTooMuch case)

    // --- Storage capacity check ---
    fn_InternalBasePtr_CheckPtr(&stateMachinePtr);
    uintptr_t smComp = stateMachinePtr[0];
    int storageCapacity = *(int*)(smComp + 0x38);
    int storageCount = fn_StorageComponent_QueryCount((void*)smComp);
    bool bStorageFull = storageCapacity < storageCount + (buyTotal - sellTotal);

    // --- Show/hide warning texts ---
    if (bOverBought) {
        // Buy value exceeds sell value — show "not enough" text with difference
        uint64_t strBuf[2] = {0, 0};
        fn_String_Format(strBuf, "%d", (unsigned)(displayBuyTotal - displaySellTotal));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x120));
        fn_TextElement_SetParameter(*(void**)(s + 0x120), 0, strBuf);
        fn_String_Dtor(strBuf);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x110), 1, 0);  // hide textNoStorage
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x118));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x118), 1, 0);  // hide textTooMuch
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x120));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x120), 0, 1);  // show textNotEnough
    } else if (bStorageFull) {
        // Storage overflow — show "no storage" text
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x110), 0, 1);  // show textNoStorage
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x118));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x118), 1, 0);  // hide textTooMuch
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x120));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x120), 1, 0);  // hide textNotEnough
    } else if (!bBuySell) {
        // Totals are equal (no net trade) — hide all warning texts
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x110), 1, 0);  // hide textNoStorage
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x118));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x118), 1, 0);  // hide textTooMuch
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x120));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x120), 1, 0);  // hide textNotEnough
    } else {
        // Sell value exceeds buy value — show "too much" text with difference
        uint64_t strBuf[2] = {0, 0};
        fn_String_Format(strBuf, "%d", (unsigned)(displaySellTotal - displayBuyTotal));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x118));
        fn_TextElement_SetParameter(*(void**)(s + 0x118), 0, strBuf);
        fn_String_Dtor(strBuf);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x110));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x110), 1, 0);  // hide textNoStorage
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x118));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x118), 0, 1);  // show textTooMuch
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x120));
        fn_Element_SetFlagsOnOff(*(void**)(s + 0x120), 1, 0);  // hide textNotEnough
    }

    // --- Find BuildState (merchant) component and check merchant arrival state ---
    // FindComponent_BuildState walks building's component array for the BuildState type
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], &buildStatePtr);

    fn_InternalBasePtr_CheckPtr(&buildStatePtr);
    uintptr_t buildState = buildStatePtr[0];
    // State byte at +0x52 (primary), +0x50 (fallback if negative)
    int8_t stateByte = *(int8_t*)(buildState + 0x52);
    if (stateByte < 0) stateByte = *(int8_t*)(buildState + 0x50);
    bool merchantArrived = (stateByte == 0x13);

    // --- Enable/disable buttonDismiss (enabled when merchant is present) ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xf8));
    Element_SetEnabled(*(void**)(s + 0xf8), merchantArrived);

    // --- Enable/disable buttonTrade ---
    // Enabled only if: buyTotal.value > 0, no storage overflow, no over-bought
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
    bool tradeEnabled = (displayBuyTotal > 0) && !bStorageFull && !bOverBought;
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xf0));
    Element_SetEnabled(*(void**)(s + 0xf0), tradeEnabled);

    // --- Layout recalc for sell and buy tables ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xC8));
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
    fn_Element_SetNeedsSizeCalc(*(void**)(s + 0xD0));

    // --- Refresh table displays ---
    TradeUI_RefreshOrderQuantityDisplay(self);
    TradeUI_RefreshSellSpinnerValues(self);
    TradeUI_RefreshOrderDisplay(self);
}

// ============================================================
// OnCreate (vtable +0x40): Refresh on order changes
// ============================================================
// Original: 0x1400827b0
//
// Finds BuildingComponent, checks if the buy order count (+0x48 on self)
// differs from building's order count (+0x58 on building). If changed,
// refreshes sell, buy, and order tables.

void TradeUI_OnCreate(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get owning entity and find BuildingComponent
    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Check if order count changed
    int selfOrderCount = *(int*)(s + 0x48);
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    int buildingOrderCount = *(int*)(buildingPtr[0] + 0x58);

    if (selfOrderCount != buildingOrderCount) {
        // Order count changed — refresh sell, buy, and order tables
        TradeUI_RefreshSellTable(self);
        TradeUI_RefreshBuyTable(self);
        TradeUI_RefreshDisplay(self, 0, 0, 0);
    }

    // Always update totals
    TradeUI_UpdateSellTotals(self);
    TradeUI_UpdateBuyTotals(self);
}

// ============================================================
// OnRemove (vtable +0x48): Event dispatcher
// ============================================================
// Original: 0x140081f20
//
// Dispatches UI events based on type and element ID:
//   eventType 0x15: storage spinners (0x7d0+), sell spinners (0xbb8+),
//     buy spinners (0xfa0+), order amount spinners (0x1b58+)
//   eventType 0x12: buttonTrade (0x4e20), buttonDismiss (0x4e21),
//     orderUp spinners (0x1388+), orderDown spinners (0x1770+),
//     purchase buttons (0x1f40+)
//   eventType 0x14: comboPurchase (0x4e22), comboOrder (0x4e23), sortCombo (0xc7)
//
// Note: The three event type sections fall through to a shared check for
// eventType 0x14 IDs, so eventType 0x12 events can also trigger 0x14 handlers
// if no 0x12 match was found.

void TradeUI_OnRemove(TradeUI* self, void* param2, int eventType, int eventId) {
    uintptr_t s = (uintptr_t)self;

    int switchVal = eventType - 0x12;
    if (switchVal == 0) {
        // --- eventType 0x12: click events ---
        if (eventId == 0x4e20) {
            fn_TradeUI_OnTradeButton(self, param2, nullptr, 0);
            return;
        }
        if (eventId == 0x4e21) {
            TradeUI_OnDismissButton(self);
            return;
        }
        // orderUp spinners: id range [0x1388, 0x1388 + orderCount]
        {
            int orderCount = *(int*)(s + 0x98);
            if (eventId >= 0x1388 && eventId <= orderCount + 0x1388) {
                TradeUI_OnOrderSpinner(self, eventId - 0x1388, eventId - 0x1389);
                goto check_type14;
            }
        }
        // orderDown spinners: id range [0x1770, 0x1770 + orderCount]
        {
            int orderCount = *(int*)(s + 0x98);
            if (eventId >= 0x1770 && eventId <= orderCount + 0x1770) {
                // Note: second arg is idx+1 relative to base (idx-0x176f = idx-0x1770+1)
                TradeUI_OnOrderSpinner(self, eventId - 0x1770, eventId - 0x176f);
                goto check_type14;
            }
        }
        // purchase buttons: id range [0x1f40, 0x1f40 + purchaseCount]
        {
            int purchaseCount = *(int*)(s + 0xa8);
            if (eventId >= 0x1f40 && eventId <= purchaseCount + 0x1f40) {
                TradeUI_OnPurchaseButton(self, eventId - 0x1f40);
                goto check_type14;
            }
        }
        goto check_type14;
    }

    switchVal -= 2;
    if (switchVal == 0) {
        // --- eventType 0x14: combo/sort selection events ---
        // (handled below in check_type14)
        goto check_type14;
    }

    switchVal -= 1;
    if (switchVal != 0) {
        return;
    }

    // --- eventType 0x15: spinner change events ---
    // storage spinners: id range [0x7d0, 0x7d0 + storageCount)
    {
        int storageCount = *(int*)(s + 0x58);
        if (eventId >= 0x7d0 && eventId < storageCount + 0x7d0) {
            TradeUI_OnStorageSpinner(self, eventId - 0x7d0, param2);
            return;
        }
    }
    // sell spinners: id range [0xbb8, 0xbb8 + tableSell.childCount)
    if (eventId >= 0xbb8) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xC8));
        void* tableSell = *(void**)(s + 0xC8);
        int sellChildCount = *(int*)((uintptr_t)tableSell + 0xC0);
        if (eventId < sellChildCount + 0xbb8) {
            TradeUI_OnSellSpinner(self, eventId - 0xbb8);
            return;
        }
    }
    // buy spinners: id range [0xfa0, 0xfa0 + tableBuy.childCount)
    if (eventId >= 0xfa0) {
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xD0));
        void* tableBuy = *(void**)(s + 0xD0);
        int buyChildCount = *(int*)((uintptr_t)tableBuy + 0xC0);
        if (eventId < buyChildCount + 0xfa0) {
            TradeUI_OnBuySpinner(self, eventId - 0xfa0);
            return;
        }
    }
    // order amount spinners: id range [0x1b58, 0x1b58 + orderCount)
    {
        int orderCount = *(int*)(s + 0x98);
        if (eventId >= 0x1b58 && eventId < orderCount + 0x1b58) {
            TradeUI_OnOrderAmountSpinner(self, eventId - 0x1b58);
        }
    }
    return;

check_type14:
    // Shared check for combo/sort events (reached from type 0x12 and 0x14)
    if (eventId == 0x4e22) {
        TradeUI_OnComboPurchase(self);
        return;
    }
    if (eventId == 0x4e23) {
        TradeUI_OnComboOrder(self);
        return;
    }
    if (eventId == 0xc7) {
        TradeUI_RefreshDisplay(self, 0, 0, 0);
    }
}

// ============================================================
// Update (vtable +0x50): Serialize tab/sort state
// ============================================================
// Original: 0x1400868c0
//
// Finds "tradeTabs" TabElement, serializes active tab index and
// sortCombo selection via IO::Serializer. On load, restores tab
// and combo state, then refreshes the sorted display.

void TradeUI_Update(TradeUI* self, void* stream, void* p3, int64_t p4) {
    uintptr_t s = (uintptr_t)self;

    // Find "tradeTabs" TabElement
    uint64_t strBuf[2] = {0, 0};
    fn_String_Ctor_CStr(strBuf, "tradeTabs");

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    uint64_t foundBuf[2] = {0, 0};
    fn_FindElement(*(void**)(s + 0x20), foundBuf, strBuf);

    fn_InternalBasePtr_CheckPtr(foundBuf);
    void* element = *(void**)foundBuf;

    // Cast to TabElement via IsDerivedFrom
    void* tabElement = nullptr;
    if (element && g_TypeId_TabElement) {
        bool isDerived = fn_IsDerivedFrom(element, *g_TypeId_TabElement);
        if (isDerived) tabElement = element;
    }

    uint64_t tabIBP = 0;
    fn_InternalBasePtr_Ctor(&tabIBP, tabElement);

    fn_String_Dtor(strBuf);

    // Read current tab index from TabElement (+0xf8)
    fn_InternalBasePtr_CheckPtr(&tabIBP);
    int tabIndex = *(int*)(tabIBP + 0xf8);

    // Read current sortCombo selection (+0x140 element, +0x118 = combo index)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x140));
    uint32_t sortIndex = *(uint32_t*)(*(uintptr_t*)(s + 0x140) + 0x118);

    // Create serializer
    uint64_t serializer[3] = {0, 0, 0};
    g_fn_Serializer_Ctor(serializer, stream, 1);

    // Only serialize if version > 0 (newer save format)
    int version = g_fn_Serializer_GetVersion(serializer);
    if (version > 0) {
        g_fn_Serialize_Int(serializer, (int*)&sortIndex, nullptr);
        g_fn_Serialize_Int(serializer, &tabIndex, nullptr);
    }

    // On load (mode == 0), restore tab and combo state
    if (*(int*)(serializer[0] + 8) == 0) {
        fn_InternalBasePtr_CheckPtr(&tabIBP);
        fn_TabElement_SetActiveTab((void*)tabIBP, tabIndex);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x140));
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x140), sortIndex);

        // Refresh sorted display
        TradeUI_RefreshDisplay(self, (uint64_t)sortIndex, 0, p4);
    }

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// UpdateBuyTotals (FUN_140082850): Sum buy order values into numberBuyTotal
// ============================================================
// Iterates all buy orders (ptr at self+0x40, count at self+0x48, stride 0x18).
// Each entry: +0x10 = quantity. Resource value = building's resource value table:
//   building+0x50 = resource type ptr array (sorted), stride 0x10
//   For entry i: value at *(building+0x50)[stride*i - 8]
// Then sets numberBuyTotal (+0x108) to the accumulated total.

void TradeUI_UpdateBuyTotals(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Iterate buy orders: ptr at +0x40, count at +0x48, stride 0x18
    // Each entry: [0]=resource entity ptr, [+0x10]=quantity int
    // Resource value = building's type-indexed price table:
    //   building+0x50 = ptr to struct of resource types (sorted set/tree?)
    //   Ghidra: *(int *)(pIVar3 + *(longlong *)(local_res10 + 0x50) + -8)
    //   where pIVar3 increments by 0x10 per iteration starting at 0
    int total = 0;
    int buyCount = *(int*)(s + 0x48);
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];

    uintptr_t resourceTypeBase = *(uintptr_t*)(building + 0x50);
    uintptr_t buyBase = *(uintptr_t*)(s + 0x40);
    for (int i = 0; i < buyCount; i++) {
        uintptr_t entry = buyBase + (uintptr_t)i * 0x18;
        int qty = *(int*)(entry + 0x10);
        if (qty != 0) {
            // Price is at resourceTypeBase + i*0x10 - 8 (relative to i-th type)
            // This is a sorted/indexed structure: each 0x10-byte slot has price at offset -8
            // Note: pIVar3 starts at 0, increments by 0x10, so at iteration i:
            //   offset = i*0x10, value at resourceTypeBase + i*0x10 - 8 = resourceTypeBase - 8 + i*0x10
            int price = *(int*)(resourceTypeBase - 8 + (uintptr_t)i * 0x10);
            total += price * qty;
        }
    }

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x108));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x108), total);
}

// ============================================================
// UpdateSellTotals (FUN_140082920): Sum sell order values into numberSellTotal
// ============================================================
// Iterates all sell orders (ptr at self+0x30, count at self+0x38, stride 0x20).
// Each entry: +0x18 = quantity. Uses Building_GetSellableCount to get unit value.
// Sets numberSellTotal (+0x100) to accumulated total.

void TradeUI_UpdateSellTotals(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    int total = 0;
    int sellCount = *(int*)(s + 0x38);
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];

    uintptr_t sellBase = *(uintptr_t*)(s + 0x30);
    for (int i = 0; i < sellCount; i++) {
        uintptr_t entry = sellBase + (uintptr_t)i * 0x20;
        int qty = *(int*)(entry + 0x18);
        if (qty != 0) {
            void* resourceEntity = *(void**)(entry);
            // FUN_14005a520: gets available sell value for this resource at the building
            int unitVal = fn_Building_GetSellableCount((void*)building, resourceEntity);
            total += qty * unitVal;
        }
    }

    fn_InternalBasePtr_CheckPtr((void*)(s + 0x100));
    fn_NumberElement_SetValueInt(*(void**)(s + 0x100), total);
}

// ============================================================
// OnBuySpinner (FUN_1400819f0): Store buy spinner value into buy order entry
// ============================================================
// Reads spinner value from buy entry [idx].spinner (+0x10 in 0x20-stride buyEntries array).
// Stores to buy orders (+0x40 base, stride 0x18, offset +0x10 for qty).
// Then calls UpdateBuyTotals.

void TradeUI_OnBuySpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    // Buy entries at self+0x70, stride 0x20, spinner at +0x10
    uintptr_t entryBase = *(uintptr_t*)(s + 0x70);
    uintptr_t entry = entryBase + (uintptr_t)idx * 0x20;

    fn_InternalBasePtr_CheckPtr((void*)(entry + 0x10));
    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x10));

    // Store to buy order qty at (buyOrdersBase + idx*0x18 + 0x10)
    uintptr_t buyOrders = *(uintptr_t*)(s + 0x40);
    *(int*)(buyOrders + (uintptr_t)idx * 0x18 + 0x10) = val;

    TradeUI_UpdateBuyTotals(self);
}

// ============================================================
// OnSellSpinner (FUN_140081a50): Store sell spinner value into sell order entry
// ============================================================
// Reads spinner value from sell entry [idx].spinner (+0x18 in 0x20-stride sellEntries).
// Stores to sell orders (+0x30 base, stride 0x20, offset +0x18 for qty).
// Then calls UpdateSellTotals.

void TradeUI_OnSellSpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    // Sell entries at self+0x60, stride 0x20, spinner at +0x18
    uintptr_t entryBase = *(uintptr_t*)(s + 0x60);
    uintptr_t entry = entryBase + (uintptr_t)idx * 0x20;

    fn_InternalBasePtr_CheckPtr((void*)(entry + 0x18));
    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x18));

    // Store to sell order qty at (sellOrdersBase + idx*0x20 + 0x18)
    uintptr_t sellOrders = *(uintptr_t*)(s + 0x30);
    *(int*)(sellOrders + (uintptr_t)idx * 0x20 + 0x18) = val;

    TradeUI_UpdateSellTotals(self);
}

// ============================================================
// OnComboPurchase (FUN_140081bc0): Save comboPurchase selection to building
// ============================================================
// Reads comboPurchase (+0x138) current item index (+0x118).
// Stores it to building component's field at +0x28.

void TradeUI_OnComboPurchase(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read combo selection
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x138));
    uint32_t comboIdx = *(uint32_t*)(*(uintptr_t*)(s + 0x138) + 0x118);

    // Store to building +0x28
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    *(uint32_t*)(buildingPtr[0] + 0x28) = comboIdx;
}

// ============================================================
// OnComboOrder (FUN_140081e80): Save comboOrder selection to building's order record
// ============================================================
// Reads comboOrder (+0xC0) current item index (+0x118).
// Stores to building's order slot at: orders_ptr + (curOrderSlot * 0x20 + 8)
// where curOrderSlot = building +0x2c.

void TradeUI_OnComboOrder(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read comboOrder selection
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xC0));
    uint32_t comboIdx = *(uint32_t*)(*(uintptr_t*)(s + 0xC0) + 0x118);

    // Store to building's current order slot +8
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];
    int curSlot = *(int*)(building + 0x2c);
    uintptr_t ordersBase = *(uintptr_t*)(building + 0x30);
    *(uint32_t*)(ordersBase + (uintptr_t)(uint32_t)curSlot * 0x20 + 8) = comboIdx;
}

// ============================================================
// OnOrderAmountSpinner (FUN_140081c50): Save order amount to building order table
// ============================================================
// Reads order amount spinner from orderEntries (+0x90, stride 0x20, spinner at +0x18).
// Stores to building's order amount at: building.orders (+0x40) + idx*0xc + 8.

void TradeUI_OnOrderAmountSpinner(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Read spinner value from orderEntries[idx].spinner (at +0x18 in 0x20-stride array)
    uintptr_t orderEntryBase = *(uintptr_t*)(s + 0x90);
    uintptr_t entry = orderEntryBase + (uintptr_t)idx * 0x20;

    fn_InternalBasePtr_CheckPtr((void*)(entry + 0x18));
    int val = fn_SpinnerElement_GetValue(*(void**)(entry + 0x18));

    // Store to building's order amount table: building.ordersAmt (+0x40) + idx*0xc + 8
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t ordersAmt = *(uintptr_t*)(buildingPtr[0] + 0x40);
    *(int*)(ordersAmt + (uintptr_t)idx * 0xc + 8) = val;
}

// ============================================================
// OnOrderSpinner (FUN_140081d00): Swap two order entries in building's order table
// ============================================================
// Swaps entries at indices param_2 and param_3 in building's 0xc-stride order array (+0x40).
// Used when up/down order spinner buttons are clicked.

void TradeUI_OnOrderSpinner(TradeUI* self, int idx, int idx2) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t ordersBase = *(uintptr_t*)(buildingPtr[0] + 0x40);

    // Swap 12-byte entries at idx and idx2 (stride 0xc)
    uintptr_t a = ordersBase + (uintptr_t)idx  * 0xc;
    uintptr_t b = ordersBase + (uintptr_t)idx2 * 0xc;

    uint32_t a0 = *(uint32_t*)(a + 0);
    uint32_t a1 = *(uint32_t*)(a + 4);
    uint32_t a2 = *(uint32_t*)(a + 8);

    *(uint32_t*)(a + 0) = *(uint32_t*)(b + 0);
    *(uint32_t*)(a + 4) = *(uint32_t*)(b + 4);
    *(uint32_t*)(a + 8) = *(uint32_t*)(b + 8);

    *(uint32_t*)(b + 0) = a0;
    *(uint32_t*)(b + 4) = a1;
    *(uint32_t*)(b + 8) = a2;
}

// ============================================================
// OnDismissButton (FUN_140081950): Dismiss the merchant
// ============================================================
// Finds BuildState component on the owning entity. Reads merchant
// state from +0x51 (or +0x50 if +0x51 is 0). Then calls
// RefreshDisplay with that state info and refreshes sell/buy tables.

void TradeUI_OnDismissButton(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    // Find BuildState component (FUN_140003ea0)
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], buildStatePtr);

    fn_InternalBasePtr_CheckPtr(buildStatePtr);
    uintptr_t buildState = buildStatePtr[0];

    // Read merchant state byte from +0x51 (fall back to +0x50 if zero)
    uint8_t stateByte = *(uint8_t*)(buildState + 0x51);
    if (stateByte == 0) {
        stateByte = *(uint8_t*)(buildState + 0x50);
    }
    uintptr_t stateAddr = buildState + 0x48;
    uint64_t stateVal = (uint64_t)(uint32_t)stateByte;

    // Call vtable[2] on +0x48 (some state notification call)
    // Original: (**(code **)(**(longlong **)(lVar1 + 0x48) + 0x10))()
    void** vtbl = *(void***)(buildState + 0x48);
    typedef void (*StateCall_t)();
    ((StateCall_t)(vtbl[2]))();

    // Refresh display and tables
    TradeUI_RefreshDisplay(self, stateAddr, stateVal, 10);
    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshBuyTable(self);
}

// ============================================================
// OnPurchaseButton (FUN_140081dd0): Toggle a purchase order entry
// ============================================================
// Reads the checked state of the purchase button at index param_2 (+0xd8 = checked byte).
// Gets the corresponding resource entity from self's purchaseReserved (+0xb0, stride 8).
// Calls Building_AddPurchaseOrder(building, resource, checkedState).

void TradeUI_OnPurchaseButton(TradeUI* self, int idx) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Get purchase button element from purchaseEntries (+0xA0, stride 8)
    uintptr_t purchaseBase = *(uintptr_t*)(s + 0xA0);
    uintptr_t buttonPtr = purchaseBase + (uintptr_t)idx * 8;
    fn_InternalBasePtr_CheckPtr((void*)buttonPtr);
    void* button = *(void**)buttonPtr;

    // Read checked state byte at +0xd8
    int8_t checked = *(int8_t*)((uintptr_t)button + 0xd8);

    // Get corresponding resource entity from purchaseReserved (+0xb0, stride 8)
    uintptr_t reservedBase = *(uintptr_t*)(s + 0xB0);
    void* resourceEntity = *(void**)(reservedBase + (uintptr_t)idx * 8);

    // Call Building_AddPurchaseOrder
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    fn_Building_AddPurchaseOrder((void*)buildingPtr[0], resourceEntity, checked);
}

// ============================================================
// OnStorageSpinner (FUN_140081ab0): Update storage resource assignment
// ============================================================
// Reads the new storage spinner value for resource at idx.
// Looks up resource entity from self's storageOrdersArray (+0x50, stride 0x18).
// Searches building's storage BST (+0x60) for the resource.
// If found: update quantity. If not found: insert new entry.
// Then refreshes sell table and display.

void TradeUI_OnStorageSpinner(TradeUI* self, int idx, void* /*reserved*/) {
    uintptr_t s = (uintptr_t)self;

    // storageEntries at self+0x80, stride 0x18, spinner at +0x08
    uintptr_t storageEntries = *(uintptr_t*)(s + 0x80);
    uintptr_t entryOffset = (uintptr_t)idx * 0x18;
    fn_InternalBasePtr_CheckPtr((void*)(storageEntries + 0x08 + entryOffset));
    int newVal = fn_SpinnerElement_GetValue(*(void**)(storageEntries + 0x08 + entryOffset));

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Get resource entity ptr from storageOrdersArray (+0x50, stride 0x18, first field)
    uintptr_t storageOrders = *(uintptr_t*)(s + 0x50);
    void* resourceEntity = *(void**)(storageOrders + entryOffset);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];
    uintptr_t storageBST = building + 0x60;  // BST root pointer at building+0x60

    // Search BST for existing entry and update or insert
    // The insert/update logic (FUN_14000e1c0) handles both cases:
    // If resource already in BST: update quantity
    // If not found: insert new leaf
    uint64_t newEntry[2] = { (uint64_t)resourceEntity, (uint64_t)(uint32_t)newVal };
    void* retval = nullptr;
    fn_StorageTree_Insert((void*)storageBST, &retval, newEntry);

    // If we got a result back (found existing), update its qty
    if (retval) {
        *(int*)((uintptr_t)retval + 8) = newVal;
    }

    TradeUI_RefreshSellTable(self);
    TradeUI_RefreshDisplay(self, (uint64_t)retval, (uint64_t)newEntry, (int64_t)building);
}

// ============================================================
// RefreshOrderQuantityDisplay (FUN_140082fb0): Update order quantity spinners
// ============================================================
// For each storage entry (count at self+0x58, storageArray at self+0x50, stride 0x18):
//   - Gets resource entity from storageArray[i] (offset 0)
//   - Searches StateMachine's storage list (at +0x28/+0x30) for matching entity
//   - Sets NumberElement value (storageEntries[i].numberElement at +0x10) to found qty
// Note: StateMachine is found via entity+FindComponent_StateMachine.

void TradeUI_RefreshOrderQuantityDisplay(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    // Check building's base type — used later for sprite/text lookups
    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    // IsDerivedFrom call on building's type desc at +0x18 — just a type check, result unused here
    // fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), DAT_1401c4bb8);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t smPtr[2] = {0, 0};
    fn_FindComponent_StateMachine((void*)entityPtr[0], &smPtr);

    int storageCount = *(int*)(s + 0x58);
    uintptr_t storageOrders = *(uintptr_t*)(s + 0x50);  // resource entity ptrs, stride 0x18
    uintptr_t storageEntries = *(uintptr_t*)(s + 0x80);  // UI entries, stride 0x18, numberElem at +0x10

    for (int i = 0; i < storageCount; i++) {
        uintptr_t orderOffset = (uintptr_t)i * 0x18;
        void* resourceEntity = *(void**)(storageOrders + orderOffset);

        // Search StateMachine's storage list for this resource entity
        // StateMachine storage list: ptr at smPtr[0]+0x28, count at smPtr[0]+0x30
        // List entries: pairs [resource_entity_ptr, qty] each 0x10 bytes (2 x 8-byte)
        fn_InternalBasePtr_CheckPtr(&smPtr);
        uintptr_t sm = smPtr[0];
        int smCount = *(int*)(sm + 0x30);
        void** smList = *(void***)(sm + 0x28);

        int foundQty = 0;
        for (int j = 0; j < smCount; j++) {
            if (smList[j * 2] == resourceEntity) {
                foundQty = (int)(uintptr_t)smList[j * 2 + 1];
                break;
            }
        }

        // Set NumberElement value
        fn_InternalBasePtr_CheckPtr((void*)(storageEntries + orderOffset + 0x10));
        fn_NumberElement_SetValueInt(*(void**)(storageEntries + orderOffset + 0x10), foundQty);
    }
}

// ============================================================
// RefreshSellSpinnerValues (FUN_140082d80): Update sell spinners with available amounts
// ============================================================
// For each sell order entry (count at self+0x38, sellOrders at self+0x30, stride 0x20):
//   - Gets available resource count via Building_GetSellableCount
//   - Searches StateMachine storage for current qty
//   - Sets NumberElement (+0x08) to found qty
//   - Sets SpinnerElement (+0x18) min=0 max=availableCount
//   - Sets spinner increment (1 if max<21, 5 if max<51, 20 otherwise)
// Also checks merchant state: if not arrived, force max=0 (spinners disabled).

void TradeUI_RefreshSellSpinnerValues(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    // Type check on building's class info at +0x18 (unused result, but done in original)
    // fn_IsDerivedFrom(*(void**)(buildingPtr[0] + 0x18), DAT_1401c4bb8);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t smPtr[2] = {0, 0};
    fn_FindComponent_StateMachine((void*)entityPtr[0], &smPtr);

    int sellCount = *(int*)(s + 0x38);

    // Check merchant state: Find BuildState component
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildStatePtr[2] = {0, 0};
    fn_FindComponent_BuildState((void*)entityPtr[0], buildStatePtr);

    fn_InternalBasePtr_CheckPtr(buildStatePtr);
    int8_t stateByte = *(int8_t*)(buildStatePtr[0] + 0x52);
    if (stateByte < 0) stateByte = *(int8_t*)(buildStatePtr[0] + 0x50);
    bool merchantArrived = (stateByte == 0x13);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];

    uintptr_t sellOrders  = *(uintptr_t*)(s + 0x30);   // resource entities, stride 0x20
    uintptr_t sellEntries = *(uintptr_t*)(s + 0x60);   // UI entries, stride 0x20

    fn_InternalBasePtr_CheckPtr(&smPtr);
    uintptr_t sm = smPtr[0];

    for (int i = 0; i < sellCount; i++) {
        uintptr_t sellOffset = (uintptr_t)i * 0x20;
        void* resourceEntity = *(void**)(sellOrders + sellOffset);

        // Get available sell count
        int available = fn_Building_GetSellableCount((void*)building, resourceEntity);

        // Search StateMachine storage list for current stored qty
        int smCount = *(int*)(sm + 0x30);
        void** smList = *(void***)(sm + 0x28);
        int storedQty = 0;
        for (int j = 0; j < smCount; j++) {
            if (smList[j * 2] == resourceEntity) {
                storedQty = (int)(uintptr_t)smList[j * 2 + 1];
                break;
            }
        }

        // Set NumberElement (+0x08) to storedQty
        fn_InternalBasePtr_CheckPtr((void*)(sellEntries + sellOffset + 0x08));
        fn_NumberElement_SetValueInt(*(void**)(sellEntries + sellOffset + 0x08), storedQty);

        // Set SpinnerElement (+0x18) min=0, max=available (or 0 if no merchant)
        int maxVal = merchantArrived ? available : 0;
        fn_InternalBasePtr_CheckPtr((void*)(sellEntries + sellOffset + 0x18));
        void* spinner = *(void**)(sellEntries + sellOffset + 0x18);
        fn_SpinnerElement_SetMinMax(spinner, 0, maxVal);

        // Set increment based on max value
        if (fn_SpinnerElement_GetMaxValue) {
            int actualMax = fn_SpinnerElement_GetMaxValue(spinner);
            int increment;
            if (actualMax < 0x15)      increment = 1;
            else if (actualMax < 0x33) increment = 5;
            else if (actualMax < 0x65) increment = 20;
            else                       increment = 1;  // fallback
            if (fn_SpinnerElement_SetIncrement) {
                fn_InternalBasePtr_CheckPtr((void*)(sellEntries + sellOffset + 0x18));
                fn_SpinnerElement_SetIncrement(*(void**)(sellEntries + sellOffset + 0x18), increment);
            }
        }
    }
}

// ============================================================
// RefreshOrderDisplay (FUN_140082b20): Update order table UI rows
// ============================================================
// For each active order in building.orders (+0x40, count at building+0x48, stride 0xc):
//   entry +0 = type index, entry +4 = resource type index, entry +8 = amount
// Gets sprite from BuildType description sprite sheet and sets it on LabelElement.
// Gets name text from BuildType string table and sets it on TextElement/LabelElement.
// Sets SpinnerElement value (order amount).
// Also handles enable/disable on the last element differently.
// NOTE: This function requires UI::Sprite and UI::String constructors — calls via
//       existing fn_LabelElement_SetSprite and fn_TextElement_SetText as raw vtable calls.

void TradeUI_RefreshOrderDisplay(TradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* entity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? entity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t buildingPtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &buildingPtr);

    fn_InternalBasePtr_CheckPtr(&buildingPtr);
    uintptr_t building = buildingPtr[0];

    // Get building's class descriptor at +0x18 (BuildTypeDescription)
    void* buildTypeDesc = *(void**)(building + 0x18);
    // Cast via IsDerivedFrom check (DAT_1401c4bb8)
    // Result used for sprite sheet lookups below

    int orderCount = *(int*)(building + 0x48);
    if (orderCount <= 0) return;

    uintptr_t ordersAmt = *(uintptr_t*)(building + 0x40);  // order entries, stride 0xc
    uintptr_t orderEntries = *(uintptr_t*)(s + 0x90);      // UI entries, stride 0x20

    for (int i = 0; i < orderCount; i++) {
        uintptr_t uiEntry = orderEntries + (uintptr_t)i * 0x20;
        uintptr_t orderSlot = ordersAmt + (uintptr_t)i * 0xc;

        int amount   = *(int*)(orderSlot + 8);
        int typeIdx  = *(int*)(orderSlot + 4);

        // Set spinner value (order amount)
        fn_InternalBasePtr_CheckPtr((void*)(uiEntry + 0x18));
        fn_SpinnerElement_SetValue(*(void**)(uiEntry + 0x18), amount);

        // Set sprite on LabelElement (+0x10) from buildTypeDesc sprite sheet
        // buildTypeDesc+0x20 = spriteSheet ptr, buildTypeDesc+0x38 = entries array base
        // Sprite at entries[typeIdx*3 + 1] * 8 = entries array offset
        if (buildTypeDesc) {
            void* spriteSheet = *(void**)((uintptr_t)buildTypeDesc + 0x20);
            uintptr_t entriesBase = *(uintptr_t*)((uintptr_t)buildTypeDesc + 0x38);
            uintptr_t spritePtr = entriesBase + ((uintptr_t)(uint32_t)(typeIdx * 3 + 1)) * 8;

            // Construct UI::Sprite on stack: {spriteSheet, spritePtr}
            // Then call fn_LabelElement_SetSprite
            uint64_t sprite[4] = { (uint64_t)spriteSheet, (uint64_t)spritePtr, 0, 0 };
            fn_InternalBasePtr_CheckPtr((void*)(uiEntry + 0x10));
            fn_LabelElement_SetSprite(*(void**)(uiEntry + 0x10), sprite);

            // Set text on TextElement (+0x10) from buildTypeDesc string table
            // buildTypeDesc+0x28 = stringTable ptr, entries[typeIdx*3] (name)
            void* stringTable = *(void**)((uintptr_t)buildTypeDesc + 0x28);
            uintptr_t namePtr = entriesBase + (uintptr_t)(uint32_t)(typeIdx) * 0x18 + 0x10;

            // Construct UI::String: {stringTable, namePtr}
            uint64_t uiStr[4] = { (uint64_t)stringTable, (uint64_t)namePtr, 0, 0 };
            fn_InternalBasePtr_CheckPtr((void*)(uiEntry + 0x10));
            fn_TextElement_SetText(*(void**)(uiEntry + 0x10), uiStr);
        }

        // Enable/disable logic:
        // - Entry 0 is disabled (first row always disabled)
        // - Last entry (i == orderCount-1) also disabled differently
        if (i == 0) {
            fn_InternalBasePtr_CheckPtr((void*)(uiEntry));
            void** vtbl = *(void***)(*(uintptr_t*)(uiEntry));
            typedef void (*SetEnabled_t)(void* self, int enabled);
            ((SetEnabled_t)(vtbl[0x40/8]))(*(void**)(uiEntry), 0);
        } else if (i == orderCount - 1) {
            fn_InternalBasePtr_CheckPtr((void*)(uiEntry + 8));
            void** vtbl = *(void***)(*(uintptr_t*)(uiEntry + 8));
            typedef void (*SetEnabled_t)(void* self, int enabled);
            ((SetEnabled_t)(vtbl[0x40/8]))(*(void**)(uiEntry + 8), 0);
        }
    }
}

// ============================================================
// RefreshDisplay (FUN_1400846b0): Rebuild sorted storage table display
// ============================================================
// Original: 0x1400846b0 (336 lines)
//
// Iterates the global trade entity list (game world +0xF0), finds
// ResourceType and ValueDescription sub-objects on each entity,
// builds a display array (stride 0x18: {entity, resourceComp, valueDescComp}).
//
// Finds TradeComponent and StorageComponent on the owning building entity.
// Sorts entries by name (sort combo == 0) or by quantity (sort combo != 0).
//
// For each entry: sets sprite icon + name text on label element, looks up
// inventory count from StorageComponent's item array, looks up order amount
// from TradeComponent's BST, sets NumberElement and SpinnerElement values.
// Clears remaining UI rows beyond the data.

// Helper: find a sub-object on a description/external entity (array at +0x28, count at +0x30)
static void* FindDescSubObject(void* descEntity, uint32_t* typeIdPtr) {
    if (!descEntity || !typeIdPtr) return nullptr;
    uint32_t target = *typeIdPtr;
    void** compArray = *(void***)((uintptr_t)descEntity + 0x28);
    int compCount = *(int*)((uintptr_t)descEntity + 0x30);
    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if (!comp) continue;
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int typeId = ((fn_GetTypeId_t)vtable[2])(comp);
        if ((uint32_t)typeId == target) {
            if (fn_IsDerivedFrom(comp, target))
                return comp;
            break;
        }
    }
    return nullptr;
}

void TradeUI_RefreshDisplay(TradeUI* self, uint64_t p2, uint64_t p3, int64_t p4) {
    uintptr_t s = (uintptr_t)self;

    // Get trade entity list from game world singleton (+0xF0)
    uintptr_t tradeList = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xF0);
    int entityCount = *(int*)(tradeList + 0x118);

    // Clear display entries array at self+0x50
    fn_DynArray_Clear_0x18((void*)(s + 0x50));

    // Build display entries: for each trade entity, find ResourceType + ValueDescription
    for (int i = 0; i < entityCount; i++) {
        uintptr_t entity = *(uintptr_t*)(*(uintptr_t*)(tradeList + 0x110) + (uintptr_t)i * 8);

        void* resourceComp = FindDescSubObject((void*)entity, g_TypeId_ResourceType);
        void* valueComp = FindDescSubObject((void*)entity, g_TypeInfo_ValueDescription);

        // Push 0x18-byte entry: {entity, resourceComp, valueComp}
        uint64_t entry[3] = {entity, (uint64_t)resourceComp, (uint64_t)valueComp};
        TradeUI_DynArrayPush(s, 0x50, 0x58, 0x5C, entry, 0x18);
    }

    // Get owning entity -> TradeComponent + StorageComponent
    void* ownerEntity = *(void**)(s + 0x28);
    bool isDerived = fn_IsDerivedFrom(ownerEntity, *g_TypeId_ComponentEntity);
    void* compEntity = isDerived ? ownerEntity : nullptr;

    uint64_t entityPtr[2] = {0, 0};
    fn_InternalBasePtr_Ctor(&entityPtr, compEntity);

    // Find TradeComponent (uses g_TypeId_BuildingComponent = DAT_1401c4b80)
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t tradePtr[2] = {0, 0};
    fn_FindComponent_Building((void*)entityPtr[0], &tradePtr);

    // Validate TradeDescription derivation (result unused — validation only)
    fn_InternalBasePtr_CheckPtr(&tradePtr);
    fn_IsDerivedFrom(*(void**)(tradePtr[0] + 0x18), *g_TypeInfo_TradeDescription);

    // Find StorageComponent
    fn_InternalBasePtr_CheckPtr(&entityPtr);
    uint64_t storagePtr[2] = {0, 0};
    fn_FindComponent_Storage((void*)entityPtr[0], &storagePtr);

    // Sort entries based on sort combo selection (+0x140 = sortCombo element)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x140));
    int entryCount = *(int*)(s + 0x58);
    int sortMode = *(int*)(*(uintptr_t*)(s + 0x140) + 0x118);

    if (sortMode == 0) {
        // Sort by name
        if (entryCount > 1) {
            uint64_t sortCtx = (uint64_t)tradeList;
            fn_IntroSort_ByName(*(void**)(s + 0x50), entryCount, &sortCtx, p4);
        }
    } else {
        // Sort by quantity (uses TradeComponent as context)
        uint64_t sortCtx = tradePtr[0];
        fn_IntroSort_ByComponent(*(void**)(s + 0x50), entryCount, &sortCtx);
    }

    // Re-read count (may have been modified)
    entryCount = *(int*)(s + 0x58);

    // --- Display loop: populate UI rows with data ---
    for (int i = 0; i < entryCount; i++) {
        int byteOff = i * 0x18;
        uintptr_t* entry = (uintptr_t*)(*(uintptr_t*)(s + 0x50) + byteOff);
        uintptr_t* uiRow = (uintptr_t*)(*(uintptr_t*)(s + 0x80) + byteOff);

        uintptr_t valueDesc = entry[2];  // ValueDescription component

        // Set sprite icon from ValueDescription (+0x20=spriteSheet, +0x28=spriteName)
        uint8_t spriteTmp[32] = {};
        fn_Sprite_Sprite(spriteTmp, *(uint64_t*)(valueDesc + 0x20), (void*)(valueDesc + 0x28));
        uint64_t spriteVal[2] = {((uint64_t*)spriteTmp)[0], ((uint64_t*)spriteTmp)[1]};
        fn_InternalBasePtr_CheckPtr(uiRow);
        fn_LabelElement_SetSprite(*(void**)uiRow, spriteVal);

        // Set text name from ValueDescription (+0x30 = name string)
        uint64_t strBuf[2] = {0, 0};
        fn_String_Ctor_Copy(strBuf, (void*)(valueDesc + 0x30));
        uint64_t strVal[2] = {strBuf[0], strBuf[1]};
        fn_InternalBasePtr_CheckPtr(uiRow);
        fn_TextElement_SetText(*(void**)uiRow, strVal);

        // Lookup inventory count from StorageComponent item array (+0x28/+0x30)
        // Each item is 0x10 bytes: {key(uint64), countA(int32), countB(int32)}
        // Result = countA - countB (available = total - committed)
        int invCount = 0;
        fn_InternalBasePtr_CheckPtr(&storagePtr);
        uintptr_t scAddr = storagePtr[0];
        int scItemCount = *(int*)(scAddr + 0x30);
        uintptr_t scItems = *(uintptr_t*)(scAddr + 0x28);
        for (int j = 0; j < scItemCount; j++) {
            uintptr_t itemAddr = scItems + (uintptr_t)j * 0x10;
            if (*(uintptr_t*)itemAddr == entry[0]) {
                if (j >= 0) {
                    invCount = *(int*)(itemAddr + 8) - *(int*)(itemAddr + 0xC);
                }
                break;
            }
        }

        // Set NumberElement value (at uiRow + 0x10)
        void* numSlot = (void*)((uintptr_t)uiRow + 0x10);
        fn_InternalBasePtr_CheckPtr(numSlot);
        fn_NumberElement_SetValueInt(*(void**)numSlot, invCount);

        // Lookup trade order amount from TradeComponent BST (red-black tree at +0x60)
        // Node: {key(+0), value(+8), color(+10), left(+18), right(+20)}
        int orderAmt = 0;
        fn_InternalBasePtr_CheckPtr(&tradePtr);
        uintptr_t* bstNode = *(uintptr_t**)(tradePtr[0] + 0x60);
        while (bstNode != nullptr) {
            if (entry[0] < *bstNode) {
                bstNode = (uintptr_t*)bstNode[3];  // left child at +0x18
            } else if (entry[0] > *bstNode) {
                bstNode = (uintptr_t*)bstNode[4];  // right child at +0x20
            } else {
                orderAmt = (int)bstNode[1];  // value at +0x08
                break;
            }
        }

        // Set SpinnerElement value (at uiRow + 0x08)
        void* spinSlot = (void*)((uintptr_t)uiRow + 8);
        fn_InternalBasePtr_CheckPtr(spinSlot);
        fn_SpinnerElement_SetValue(*(void**)spinSlot, orderAmt);

        // Enable spinner and number elements via vtable[8] (+0x40) = SetEnabled
        fn_InternalBasePtr_CheckPtr(spinSlot);
        Element_SetEnabled(*(void**)spinSlot, true);
        fn_InternalBasePtr_CheckPtr(numSlot);
        Element_SetEnabled(*(void**)numSlot, true);

        // Set spinner min/max range
        fn_InternalBasePtr_CheckPtr(spinSlot);
        fn_SpinnerElement_SetMinMax(*(void**)spinSlot, 0, 9999);

        // Set spinner element name to resource path filename
        void* path = fn_External_GetPath((void*)entry[0]);
        uint64_t filenameBuf[4] = {0};
        fn_Filename_GetPathFilename(path, filenameBuf, false);
        fn_InternalBasePtr_CheckPtr(spinSlot);
        fn_Element_SetName(*(void**)spinSlot, filenameBuf);
        fn_Filename_Dtor(filenameBuf);
    }

    // --- Clear remaining UI rows beyond data ---
    int uiRowCount = *(int*)(s + 0x88);
    for (int i = entryCount; i < uiRowCount; i++) {
        int byteOff = i * 0x18;
        uintptr_t* uiRow = (uintptr_t*)(*(uintptr_t*)(s + 0x80) + byteOff);

        // Set blank sprite
        uint8_t blankSprite[16] = {};
        fn_Sprite_DefaultCtor(blankSprite);
        uint64_t spriteVal[2] = {((uint64_t*)blankSprite)[0], ((uint64_t*)blankSprite)[1]};
        fn_InternalBasePtr_CheckPtr(uiRow);
        fn_LabelElement_SetSprite(*(void**)uiRow, spriteVal);

        // Set blank text
        uint64_t blankStr[2] = {0, 0};
        fn_String_Ctor_Default(blankStr);
        uint64_t strVal[2] = {blankStr[0], blankStr[1]};
        fn_InternalBasePtr_CheckPtr(uiRow);
        fn_TextElement_SetText(*(void**)uiRow, strVal);

        // Disable spinner (at +8) via vtable SetEnabled
        void* spinSlot = (void*)((uintptr_t)uiRow + 8);
        fn_InternalBasePtr_CheckPtr(spinSlot);
        Element_SetEnabled(*(void**)spinSlot, false);

        // Disable number (at +0x10) via vtable SetEnabled
        void* numSlot = (void*)((uintptr_t)uiRow + 0x10);
        fn_InternalBasePtr_CheckPtr(numSlot);
        Element_SetEnabled(*(void**)numSlot, false);

        // Set spinner min/max to 0,0 (no range)
        fn_InternalBasePtr_CheckPtr(spinSlot);
        fn_SpinnerElement_SetMinMax(*(void**)spinSlot, 0, 0);
    }
}

// ============================================================
// NOTE: The following functions are too complex for this pass (100+ lines
// with many unknown sub-calls) and are marked "needs opus":
//
//   fn_TradeUI_OnTradeButton   (0x140081420, ~160 lines)
//     — executes the actual trade: moves sell items to StateMachine,
//       processes buy orders (destroys/removes/spawns entities),
//       updates totals, refreshes all tables
//
//   fn_TradeUI_RefreshSellTable (0x140083c70, 381 lines)
//     — populates sell table UI elements with resource info, amounts, icons
//
//   fn_TradeUI_RefreshBuyTable  (0x140083100, 488 lines)
//     — populates buy table UI elements with resource/buildtype info
// ============================================================
