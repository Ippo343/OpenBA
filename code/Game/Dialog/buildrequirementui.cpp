// code/Game/Dialog/buildrequirementui.cpp
// Clean reimplementation of BuildRequirementUI::Slot4.
//
// BuildRequirementUI (0x60 bytes) — shows building icon, title, and resource requirements.
// Layout:
//   +0x10 = datasource description (from DialogController)
//   +0x18 = dialog instance (InternalBasePtr, from DialogController)
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x28 = entity/data source (InternalBasePtr, from DialogController)
//   +0x30 = resolved description pointer
//   +0x38 = initial value 1 (unused after init)
//   +0x40 = label element array ptr (dynamic array)
//   +0x44 = label count (uint32), +0x48 padding -> actually +0x48 = count, +0x4c = capacity
//   +0x50 = requirement data array ptr (dynamic array)
//   +0x58 = req data count (uint32)
//
// Decompiled from Application-x64.exe at 0x14005E7C0.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: find component by type ID in a description's component list.
// Description has component array at +0x28 and count at +0x30.
static void* FindCompInDesc(uintptr_t desc, uint32_t typeId) {
    int count = *(int*)(desc + 0x30);
    uintptr_t array = *(uintptr_t*)(desc + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = *(void**)(array + (uintptr_t)i * 8);
        if (!comp) continue;

        // vtable[2] returns the component's type ID
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int compTypeId = ((fn_GetTypeId_t)vtable[2])(comp);

        if (compTypeId == (int)typeId) {
            if (fn_IsDerivedFrom(comp, typeId)) {
                return comp;
            }
            break;
        }
    }
    return nullptr;
}

// Helper: find element by name via DialogInstance::FindElement, return raw ptr
static void* FindDialogElem(void* dialogInst, const char* name) {
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, name);
    fn_InternalBasePtr_CheckPtr((void*)&dialogInst);
    fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
    void* result = *(void**)retBuf;
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
static uint64_t FindDialogElemAsBasePtr(void* dialogInst, const char* name) {
    void* raw = FindDialogElem(dialogInst, name);
    bool isInternal = fn_IsDerivedFrom(raw, *g_TypeId_ObjectInternal);
    void* checked = isInternal ? raw : nullptr;
    uint8_t bpBuf[8];
    fn_InternalBasePtr_Ctor(bpBuf, checked);
    return *(uint64_t*)bpBuf;
}

// ============================================================
// BuildRequirementUI::Slot4 (vtable +0x20) — Setup/populate
// ============================================================
// Resolves the building description from the datasource,
// finds BuildRequirementComponent for icon/title display,
// finds ResourceComponent for material requirement list.
// Populates label elements with resource sprites and counts.
//
// Original: 0x14005E7C0.

void BuildRequirementUI_Slot4(BuildRequirementUI* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Resolve description from datasource (+0x28) ---
    // Three possible datasource types:
    //   1. BuildComponent — use it directly as description
    //   2. ComponentEntity — extract description from entity+0x28
    //   3. Object::Internal (UI Element) — get user data, check if BuildComponent
    void* dataSource = *(void**)(s + 0x28);

    // Try path 1: datasource IsA BuildComponent
    bool isBuild = fn_IsDerivedFrom(dataSource, *g_TypeId_BuildComponent);
    if (isBuild && dataSource) {
        // Re-verify (matches original double-check pattern)
        isBuild = fn_IsDerivedFrom(dataSource, *g_TypeId_BuildComponent);
        void* desc = isBuild ? dataSource : nullptr;
        *(void**)(s + 0x30) = desc;
    }
    // Try path 2: datasource IsA ComponentEntity
    else {
        bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
        if (isEntity && dataSource) {
            // Re-verify and extract description at entity+0x28
            isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
            void* entity = isEntity ? dataSource : nullptr;
            *(void**)(s + 0x30) = *(void**)((uintptr_t)entity + 0x28);
        }
        // Try path 3: datasource IsA Object::Internal (UI Element)
        else {
            uint32_t internalTypeId = g_TypeId_ObjectInternal ? *g_TypeId_ObjectInternal : 0;
            bool isInternal = fn_IsDerivedFrom(dataSource, internalTypeId);
            void* internalObj = isInternal ? dataSource : nullptr;

            uint8_t bpBuf[8];
            fn_InternalBasePtr_Ctor(bpBuf, internalObj);
            void* elemPtr = *(void**)bpBuf;

            if (elemPtr) {
                fn_InternalBasePtr_CheckPtr(bpBuf);
                void* userData = fn_Element_GetUserData(elemPtr);
                isBuild = fn_IsDerivedFrom(userData, *g_TypeId_BuildComponent);
                *(void**)(s + 0x30) = isBuild ? userData : nullptr;
            }
            // else: +0x30 stays as-is (from previous init or nullptr)
        }
    }

    // --- Process description if resolved ---
    uintptr_t desc = *(uintptr_t*)(s + 0x30);
    if (!desc) return;

    // Find BuildRequirementComponent in description's component list
    void* buildReqComp = FindCompInDesc(desc, *g_TypeId_BuildRequirementComponent);

    // Get dialog instance at +0x18
    void* dialogInst = *(void**)(s + 0x18);

    // --- Set icon sprite from BuildRequirementComponent ---
    {
        void* imageElem = FindDialogElem(dialogInst, "imageIcon");
        fn_InternalBasePtr_CheckPtr((void*)&imageElem);

        // Check if element is an Object::Internal for InternalBasePtr wrapping
        bool isInternal = fn_IsDerivedFrom(imageElem, *g_TypeId_ObjectInternal);
        void* checked = isInternal ? imageElem : nullptr;
        uint8_t ibpBuf[8];
        fn_InternalBasePtr_Ctor(ibpBuf, checked);
        void* imagePtr = *(void**)ibpBuf;

        if (imagePtr && buildReqComp) {
            // Create sprite from BuildRequirementComponent: SpriteSheet at +0x20, name at +0x28
            uintptr_t brc = (uintptr_t)buildReqComp;
            uint8_t sprite[16];
            fn_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));

            fn_InternalBasePtr_CheckPtr(ibpBuf);
            fn_ImageElement_SetSprite(imagePtr, sprite);
        }
    }

    // --- Set title text from BuildRequirementComponent ---
    {
        void* textElem = FindDialogElem(dialogInst, "textTitle");
        fn_InternalBasePtr_CheckPtr((void*)&textElem);

        bool isInternal = fn_IsDerivedFrom(textElem, *g_TypeId_ObjectInternal);
        void* checked = isInternal ? textElem : nullptr;
        uint8_t ibpBuf[8];
        fn_InternalBasePtr_Ctor(ibpBuf, checked);
        void* textPtr = *(void**)ibpBuf;

        if (textPtr && buildReqComp) {
            // Create UI::String from BuildRequirementComponent: StringTable at +0x30, name at +0x38
            uintptr_t brc = (uintptr_t)buildReqComp;
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, *(uint64_t*)(brc + 0x30), (void*)(brc + 0x38));

            fn_InternalBasePtr_CheckPtr(ibpBuf);
            fn_TextElement_SetText(textPtr, uiStr);
        }
    }

    // --- Find "labelBuildMulti" element, hide initially ---
    void* labelMulti = FindDialogElem(dialogInst, "labelBuildMulti");
    if (labelMulti) {
        fn_InternalBasePtr_CheckPtr(&labelMulti);
        fn_Element_SetFlagsOnOff(labelMulti, 1, 0);  // hide
    }

    // --- Build dynamic label array: find "labelBuild0", "labelBuild1", etc. ---
    // Reset label array count
    *(uint32_t*)(s + 0x48) = 0;

    // Find first label element using format "labelBuild%i"
    {
        uint8_t fmtBuf[16];
        fn_String_Format(fmtBuf, "labelBuild%i", (uint64_t)*(uint32_t*)(s + 0x48));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* retBuf[2] = {0, 0};
        fn_DialogInstance_FindElement(*(void**)(s + 0x18), retBuf, fmtBuf);
        void* labelElem = retBuf[0];
        fn_String_Dtor(fmtBuf);

        while (labelElem) {
            fn_InternalBasePtr_CheckPtr(&labelElem);

            // Wrap as InternalBasePtr
            bool isInternal = fn_IsDerivedFrom(labelElem, *g_TypeId_ObjectInternal);
            void* checked = isInternal ? labelElem : nullptr;
            uint8_t ibpBuf[8];
            fn_InternalBasePtr_Ctor(ibpBuf, checked);
            void* wrappedElem = *(void**)ibpBuf;
            if (!wrappedElem) break;

            // Hide the label initially
            fn_InternalBasePtr_CheckPtr(ibpBuf);
            fn_Element_SetFlagsOnOff(wrappedElem, 1, 0);

            // Push into dynamic array at +0x40 (fn_ExeArrayPush handles realloc)
            uint32_t newCount = *(uint32_t*)(s + 0x48) + 1;
            fn_ExeArrayPush((void*)(s + 0x40), (int)newCount, ibpBuf);
            *(uint32_t*)(s + 0x48) = newCount;

            // Find next label
            fn_String_Format(fmtBuf, "labelBuild%i", (uint64_t)newCount);
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            fn_DialogInstance_FindElement(*(void**)(s + 0x18), retBuf, fmtBuf);
            labelElem = retBuf[0];
            fn_String_Dtor(fmtBuf);
        }
    }

    // --- Find ResourceComponent in description ---
    void* resourceComp = FindCompInDesc(desc, *g_TypeId_ResourceComponent);
    if (!resourceComp) return;

    // Check resource component has flag at +0x34 and multi-build count
    uintptr_t rc = (uintptr_t)resourceComp;

    // Determine item count: min of label count and resource entry count at rc+0x28
    int labelCount = *(int*)(s + 0x48);
    int entryCount = *(int*)(rc + 0x28);
    int itemCount = (entryCount < labelCount) ? entryCount : labelCount;

    if (itemCount <= 0) goto check_multi;

    // --- Populate labels with resource sprites and counts ---
    {
        // Each resource entry is 0x10 bytes at rc+0x20 base array
        uintptr_t entryArray = *(uintptr_t*)(rc + 0x20);

        for (int i = 0; i < itemCount; i++) {
            uintptr_t entry = entryArray + (uintptr_t)i * 0x10;

            // Push requirement count data into array at +0x50
            // Entry+0x08 contains the requirement count (uint32)
            void* countPtr = (void*)(entry + 0x08);
            fn_ExeArrayPush((void*)(s + 0x50), *(int*)(s + 0x58) + 1, (void*)countPtr);
            *(uint32_t*)(s + 0x58) = *(uint32_t*)(s + 0x58) + 1;

            // Get the sub-entity from this entry (pointer at entry+0x00)
            uintptr_t subEntity = *(uintptr_t*)entry;

            // Find ResourceType component in sub-entity's component list
            void* resType = FindCompInDesc(subEntity, *g_TypeId_ResourceType);

            // Create sprite from ResourceType: SpriteSheet at +0x20, name at +0x38
            uint8_t sprite[24];
            fn_Sprite_Ctor(sprite, *(uint64_t*)((uintptr_t)resType + 0x20),
                           (void*)((uintptr_t)resType + 0x38));

            // Set sprite on label element
            void* label = *(void**)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8);
            fn_InternalBasePtr_CheckPtr((void*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8));
            fn_LabelElement_SetSprite(label, sprite);

            // Format count as "%i" and set as text parameter 0
            uint8_t countStr[8];
            fn_String_Format(countStr, "%i", (uint64_t)*(uint32_t*)countPtr);
            fn_InternalBasePtr_CheckPtr((void*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8));
            fn_TextElement_SetParameter(label, 0, countStr);
            fn_String_Dtor(countStr);

            // Show the label
            fn_InternalBasePtr_CheckPtr((void*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8));
            fn_Element_SetFlagsOnOff(label, 0, 1);
        }
    }

check_multi:
    // If resource component has multi-build flag at +0x34, show "labelBuildMulti"
    if (*(char*)(rc + 0x34) != 0) {
        if (labelMulti) {
            fn_InternalBasePtr_CheckPtr(&labelMulti);
            fn_Element_SetFlagsOnOff(labelMulti, 0, 1);  // show
        }
    }
}
