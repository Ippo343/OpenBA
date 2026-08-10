// code/Game/Dialog/productionui.cpp
// Clean reimplementation of ProductionUI game logic.
//
// ProductionUI (0x50 bytes) -- production display panel for buildings.
// Extends DialogController. Two dynamic arrays at +0x30 and +0x40 hold
// pairs of {labelStorage, numberCount} element InternalBasePtrs for
// consumed items (array1) and produced items (array2).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // Object::ResolveTypeIdByNameLazy
#include <cstdint>
#include <cstdio>

// ============================================================
// ProductionUI struct layout:
//   +0x00: DialogController base (0x30 bytes)
//   +0x30: consumed array data ptr (8 bytes)
//   +0x38: consumed array count (uint32_t)
//   +0x3C: consumed array capacity (uint32_t)
//   +0x40: produced array data ptr (8 bytes)
//   +0x48: produced array count (uint32_t)
//   +0x4C: produced array capacity (uint32_t)
//
// Each array element = 0x10 bytes:
//   +0x00: InternalBasePtr to "labelStorage" element
//   +0x08: InternalBasePtr to "numberCount" element
// ============================================================

// Helper: dynamically grow and append a 0x10-byte entry to one of the arrays.
// arrayPtr = &self->array[N].data, arrayCnt = &self->array[N].count, arrayCap = &self->array[N].capacity
static void PushElementPair(uintptr_t self, int arrayOffset,
                            uint64_t labelPtr, uint64_t numberPtr) {
    uintptr_t dataField = self + arrayOffset;           // +0x30 or +0x40
    uintptr_t cntField  = self + arrayOffset + 0x08;    // +0x38 or +0x48
    uintptr_t capField  = self + arrayOffset + 0x0C;    // +0x3C or +0x4C

    uint32_t count = *(uint32_t*)cntField;
    uint32_t newCount = count + 1;
    if (newCount == count) return;  // overflow guard

    void* oldData = *(void**)dataField;
    *(uint32_t*)cntField = newCount;

    if ((int32_t)*(uint32_t*)capField < (int32_t)newCount) {
        uint32_t newCap = newCount;
        if (count != 0) {
            newCap = newCount * 3;
        }
        *(uint32_t*)capField = newCap;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)(int32_t)newCap * 0x10, 8);
        *(void**)dataField = newData;
    }

    // Copy existing entries if buffer changed
    uint32_t copyStart = 0;
    if (oldData == *(void**)dataField) {
        copyStart = count;
    }
    for (uint32_t i = copyStart; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*(uintptr_t*)dataField + (uintptr_t)i * 0x10);
        if (dst) {
            if ((int32_t)i < (int32_t)count) {
                // Copy from old buffer
                uint64_t* src = (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 0x10);
                dst[0] = src[0];
                dst[1] = src[1];
            } else {
                // New entry
                dst[0] = labelPtr;
                dst[1] = numberPtr;
            }
        }
    }

    if (oldData != *(void**)dataField) {
        fn_MemFreeStatic(oldData);
    }
}

// ---- element type ids for retail's two casts --------------------------------
// Retail Slot4 stores `IsDerivedFrom(hit, id) ? hit : nullptr`, reading the ids
// through exe .rdata exrefs 0x140151668 (UI::LabelElement) and 0x1401516c8
// (UI::NumberElement — the engine_funcs label "SpinnerElement" is a known
// misnomer, see standalone/reg_element_typeids.cpp:35).
//
// That element-type-id table is registered by reg_element_typeids.cpp but GATED
// on BANISHED_LIVE_ELEMENT_TIDS, which is NOT in kDefaultOnFlags — so by default
// both globals are null. We resolve PRIVATE copies by name rather than filling
// g_TypeId_LabelElement / g_TypeId_SpinnerElement, because those globals are
// shared with townhallui / helpui / toolbardialog and waking them on the default
// path is precisely the broad change that gate exists to hold back. When the gate
// IS on we use the registry value, so the two paths cannot disagree.
static uint32_t ProductionUI_LabelElementTypeId() {
    if (g_TypeId_LabelElement && *g_TypeId_LabelElement) return *g_TypeId_LabelElement;
    static uint32_t s_local = 0;
    if (!s_local) s_local = ::Object::ResolveTypeIdByNameLazy("LabelElement");
    return s_local;
}
static uint32_t ProductionUI_NumberElementTypeId() {
    if (g_TypeId_SpinnerElement && *g_TypeId_SpinnerElement) return *g_TypeId_SpinnerElement;
    static uint32_t s_local = 0;
    if (!s_local) s_local = ::Object::ResolveTypeIdByNameLazy("NumberElement");
    return s_local;
}

// Gate helper for the promotion in ui_dialog_stub_factories.cpp.
bool ProductionUI_TypeIdsReady() {
    return ProductionUI_LabelElementTypeId() != 0
        && ProductionUI_NumberElementTypeId() != 0;
}

static void* CastLabel(void* elem) {
    if (!elem) return nullptr;
    return fn_IsDerivedFrom(elem, ProductionUI_LabelElementTypeId()) ? elem : nullptr;
}
static void* CastNumber(void* elem) {
    if (!elem) return nullptr;
    return fn_IsDerivedFrom(elem, ProductionUI_NumberElementTypeId()) ? elem : nullptr;
}

// ============================================================
// ProductionUI::Slot4 (vtable +0x20) -- Setup
// ============================================================
// Finds the "production" container (fallback "production3", "production4"),
// gets two child containers (consumed items, produced items), and for
// each child's sub-children (starting from index 1), finds "labelStorage"
// and "numberCount" elements and caches them in the dynamic arrays.
//
// Original: 0x140075DA0.

void ProductionUI_Slot4(ProductionUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)(s + 0x20);

    // Find "production" element (with fallbacks)
    uint8_t nameBuf[8];
    uint64_t result = 0;

    fn_String_Ctor_CStr(nameBuf, "production");
    fn_FindElement(dialogElem, &result, nameBuf);
    fn_String_Dtor(nameBuf);

    if (result == 0) {
        fn_String_Ctor_CStr(nameBuf, "production3");
        uint64_t retBuf = 0;
        fn_FindElement(dialogElem, &retBuf, nameBuf);
        result = retBuf;
        fn_String_Dtor(nameBuf);

        if (result == 0) {
            fn_String_Ctor_CStr(nameBuf, "production4");
            uint64_t retBuf2 = 0;
            fn_FindElement(dialogElem, &retBuf2, nameBuf);
            result = retBuf2;
            fn_String_Dtor(nameBuf);
        }
    }

    void* productionElem = (void*)result;

    // Get two child containers: Child(0) = consumed, Child(1) = produced
    uint64_t consumedContainer = 0;
    fn_Element_Child(productionElem, &consumedContainer, 0);

    uint64_t producedContainer = 0;
    fn_Element_Child(productionElem, &producedContainer, 1);

    // Iterate consumed container children (start at index 1)
    int childCount = fn_Element_ChildCount((void*)consumedContainer);
    int idx = 1;
    while (idx < childCount) {
        // Get child element at index
        uint64_t childElem = 0;
        fn_Element_Child((void*)consumedContainer, &childElem, idx);

        // Find "labelStorage" sub-element
        fn_String_Ctor_CStr(nameBuf, "labelStorage");
        uint64_t labelResult = 0;
        fn_FindElement((void*)childElem, &labelResult, nameBuf);
        fn_String_Dtor(nameBuf);

        // Retail casts the hit through IsDerivedFrom(LabelElement) and stores NULL
        // when it isn't one (0x140075da0 uses g_TypeId_LabelElement_exref @0x140151668).
        uint64_t labelBP = 0;
        fn_InternalBasePtr_Ctor(&labelBP, CastLabel((void*)labelResult));

        // Find "numberCount" sub-element
        fn_String_Ctor_CStr(nameBuf, "numberCount");
        uint64_t numberResult = 0;
        fn_FindElement((void*)childElem, &numberResult, nameBuf);
        fn_String_Dtor(nameBuf);

        // Retail casts through IsDerivedFrom(NumberElement) (exref 0x1401516c8,
        // engine_funcs label "SpinnerElement" — the id is UI::NumberElement's).
        uint64_t numberBP = 0;
        fn_InternalBasePtr_Ctor(&numberBP, CastNumber((void*)numberResult));

        // Push pair to consumed array at +0x30
        PushElementPair(s, 0x30, labelBP, numberBP);

        idx++;
        childCount = fn_Element_ChildCount((void*)consumedContainer);
    }

    // Iterate produced container children (start at index 1)
    childCount = fn_Element_ChildCount((void*)producedContainer);
    idx = 1;
    while (idx < childCount) {
        uint64_t childElem = 0;
        fn_Element_Child((void*)producedContainer, &childElem, idx);

        fn_String_Ctor_CStr(nameBuf, "labelStorage");
        uint64_t labelResult = 0;
        fn_FindElement((void*)childElem, &labelResult, nameBuf);
        fn_String_Dtor(nameBuf);

        uint64_t labelBP = 0;
        fn_InternalBasePtr_Ctor(&labelBP, CastLabel((void*)labelResult));

        fn_String_Ctor_CStr(nameBuf, "numberCount");
        uint64_t numberResult = 0;
        fn_FindElement((void*)childElem, &numberResult, nameBuf);
        fn_String_Dtor(nameBuf);

        uint64_t numberBP = 0;
        fn_InternalBasePtr_Ctor(&numberBP, CastNumber((void*)numberResult));

        // Push pair to produced array at +0x40
        PushElementPair(s, 0x40, labelBP, numberBP);

        idx++;
        childCount = fn_Element_ChildCount((void*)producedContainer);
    }
}

// ============================================================
// ProductionUI::Slot7 helper — update one production list
// ============================================================
// Updates the UI for either consumed (listIndex=0) or produced (listIndex=1)
// items by reading from the TrackerComponent and setting sprite/text/count
// on cached label+number elements.
//
// Original helper: 0x140075A50.

static void UpdateProductionList(uintptr_t self, int listIndex) {
    // Get entity from controller data (+0x28)
    void* entity = *(void**)(self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    uint64_t entityBP = 0;
    fn_InternalBasePtr_Ctor(&entityBP, entityPtr);

    // Find TrackerComponent via fn_FindComponent_Tracker
    uint64_t trackerBP = 0;
    fn_FindComponent_Tracker((void*)entityBP, &trackerBP);
    uintptr_t tracker = (uintptr_t)trackerBP;

    // STANDALONE DEVIATION (the only guard in this file). Retail derefs
    // tracker+0x48 unguarded because every entity that can reach Dialog/
    // Production.rsc carries a TrackerComponent. Our panel templates are shared
    // across entity kinds and this controller runs EVERY frame (slot 7 is the
    // per-frame update, dispatched before any click ever selects a producing
    // building), so a bound entity legitimately without a TrackerComponent would
    // fault here. Skipping the list update is the correct standalone behaviour —
    // there is nothing to show. Do NOT widen this into "null checks everywhere":
    // if the pasture/crop entity itself lacks a tracker, that is an upstream bug.
    if (!tracker) return;

    // Check if game world item count changed; rebuild tracker if so
    int gameItemCount = **(int**)((uintptr_t)*g_pGameWorldSingleton + 0x128);
    if (gameItemCount != *(int*)(tracker + 0x48)) {
        // Call tracker rebuild: FUN_14000d9d0 moves data from +0x38 to +0x28
        fn_TrackerComponent_Rebuild((void*)(tracker + 0x38), (void*)(tracker + 0x28));
        fn_MemFreeStatic(*(void**)(tracker + 0x28));
        *(uint64_t*)(tracker + 0x28) = 0;
        *(uint64_t*)(tracker + 0x30) = 0;
        *(int*)(tracker + 0x48) = gameItemCount;
    }

    // Get item count for this list
    int itemCount;
    if (listIndex == 0) {
        itemCount = *(int*)(tracker + 0x30);
    } else {
        itemCount = *(int*)(tracker + 0x40);
    }

    // Get element array for this list
    uintptr_t elemArrayPtr = *(uintptr_t*)(self + ((int64_t)listIndex + 3) * 0x10);
    int elemCount = *(int*)(self + (int64_t)listIndex * 0x10 + 0x38);

    int i = 0;
    while (i < itemCount) {
        // Stop if we've used all cached UI elements
        if (i == elemCount) break;

        uintptr_t elemPair = elemArrayPtr + (uintptr_t)i * 0x10;

        // Get item array from tracker
        uintptr_t itemArray;
        if (listIndex == 0) {
            itemArray = *(uintptr_t*)(tracker + 0x28);
        } else {
            itemArray = *(uintptr_t*)(tracker + 0x38);
        }

        uintptr_t item = *(uintptr_t*)(itemArray + (uintptr_t)i * 0x10);

        // Search sub-entries for ResourceType description
        int subCount = *(int*)(item + 0x30);
        uintptr_t subArray = *(uintptr_t*)(item + 0x28);
        void* resourceDesc = nullptr;

        for (int j = 0; j < subCount; j++) {
            void* subEntry = *(void**)(subArray + (uintptr_t)j * 8);
            // Call GetTypeId virtual (vtable+0x10)
            typedef int (*fn_GetTypeId_t)(void*);
            void** vtbl = *(void***)subEntry;
            int typeId = ((fn_GetTypeId_t)vtbl[2])(subEntry);
            if (typeId == (int)*g_TypeId_RawMaterialDescription) {
                if (fn_IsDerivedFrom(subEntry, *g_TypeId_RawMaterialDescription)) {
                    resourceDesc = subEntry;
                }
                break;
            }
        }

        // Get count from item entry (+0x08 relative to item array base)
        if (listIndex == 0) {
            itemArray = *(uintptr_t*)(tracker + 0x28);
        } else {
            itemArray = *(uintptr_t*)(tracker + 0x38);
        }
        int count = *(int*)(itemArray + (uintptr_t)i * 0x10 + 0x08);

        // Create sprite from ResourceDescription
        uintptr_t rd = (uintptr_t)resourceDesc;
        uint8_t spriteBuf[16];
        fn_Sprite_Ctor(spriteBuf, *(uint64_t*)(rd + 0x20), (void*)(rd + 0x30));

        // Set sprite on label element
        void* labelElem = (void*)*(uint64_t*)elemPair;
        fn_LabelElement_SetSprite(labelElem, spriteBuf);

        // Create UI::String from ResourceDescription
        uint8_t uiStrBuf[16];
        fn_UIString_Ctor(uiStrBuf, *(uint64_t*)(rd + 0x28), (void*)(rd + 0x30));

        // Set text on label element
        fn_TextElement_SetText(labelElem, uiStrBuf);

        // Set count on number element
        void* numberElem = (void*)*(uint64_t*)(elemPair + 8);
        fn_NumberElement_SetValueInt(numberElem, count);

        // Show both elements
        fn_Element_SetFlagsOnOff(labelElem, 0, 1);
        fn_Element_SetFlagsOnOff(numberElem, 0, 1);

        i++;
    }

    // Hide unused elements
    while (i < elemCount) {
        uintptr_t elemPair = elemArrayPtr + (uintptr_t)i * 0x10;

        // Set empty sprite on label element
        uint8_t emptySprite[16];
        fn_Sprite_Default_Ctor(emptySprite);
        void* labelElem = (void*)*(uint64_t*)elemPair;
        fn_LabelElement_SetSprite(labelElem, emptySprite);

        // Set empty text on label element
        uint8_t emptyStr[16];
        fn_UIString_DefaultCtor(emptyStr);
        fn_TextElement_SetText(labelElem, emptyStr);

        // Hide number element
        void* numberElem = (void*)*(uint64_t*)(elemPair + 8);
        fn_Element_SetFlagsOnOff(numberElem, 1, 0);

        i++;
    }
}

// ============================================================
// ProductionUI::Slot7 (vtable +0x38) -- Update
// ============================================================
// Updates both consumed and produced item lists.
//
// Original: 0x140075A20.

void ProductionUI_Slot7(ProductionUI* self) {
    uintptr_t s = (uintptr_t)self;
    UpdateProductionList(s, 0);
    UpdateProductionList(s, 1);
}

// ============================================================
