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
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#if defined(_DEBUG)
#include <cstdio>
#include <cstdlib>
#endif

#ifdef _DEBUG
// Debug sprite logging wrappers (runtime/src/ui/)
extern void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName);
extern void* Debug_Sprite_DefaultCtor(void* retval);
extern void Debug_ImageElement_SetSprite(void* self, void* spriteData);
extern void Debug_LabelElement_SetSprite(void* self, void* spriteData);
#endif

// Helper: find component by type ID in a description's component list.
// Description has component array at +0x28 and count at +0x30.
static inline bool SaneHeapPtr(const void* p) {
    uintptr_t v = (uintptr_t)p;
    // Non-null, 8-byte aligned, in user address space. Filters garbage/sentinel
    // values so a malformed component array can't fault the scan.
    return v != 0 && (v & 7) == 0 && v < 0x7FFFFFFFFFFFull;
}
static void* FindCompInDesc(uintptr_t desc, uint32_t typeId) {
    if (!SaneHeapPtr((void*)desc)) return nullptr;
    int count = *(int*)(desc + 0x30);
    uintptr_t array = *(uintptr_t*)(desc + 0x28);
    // Guard against a count/array mismatch (our standalone desc divergence can hand
    // a description whose +0x28 array is null while +0x30 is non-zero — the retail
    // BuildComponent never is). Bound the count so a garbage value can't run wild.
    if (!SaneHeapPtr((void*)array) || count <= 0 || count > 4096) return nullptr;

    for (int i = 0; i < count; i++) {
        void* comp = *(void**)(array + (uintptr_t)i * 8);
        if (!SaneHeapPtr(comp)) continue;

        // vtable[2] returns the component's type ID. NB: vtable is a data pointer
        // (8-aligned), but vtable[2] is a *function* pointer (code address) and is
        // NOT necessarily 8-aligned — only require it be non-null, not SaneHeapPtr,
        // or every component gets skipped.
        void** vtable = *(void***)comp;
        if (!SaneHeapPtr(vtable) || vtable[2] == nullptr) continue;
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
    System::String nameStr(name);
    uint8_t retBuf[16];
    fn_InternalBasePtr_CheckPtr((void*)&dialogInst);
    fn_DialogInstance_FindElement(dialogInst, retBuf, &nameStr);
    void* result = *(void**)retBuf;
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
// BuildRequirementUI::Shutdown (vtable +0x18) — No-op
// ============================================================

// ============================================================
// BuildRequirementUI::Slot4 (vtable +0x20) — Setup/populate
// ============================================================
// Resolves the building description from the datasource,
// finds ToolbarDescription for icon/title display,
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

    // NULL-datasource guard (standalone): the Runtime's Object::Base::IsDerivedFrom
    // (0x1800c0390) reads `this->vtable[2]()` with NO null check on `this`, so calling
    // it with a NULL dataSource faults immediately (RCX=NULL vtable read at +0xC039A —
    // the crash handler mislabels this "factory returned NULL"). Retail always binds a
    // BuildComponent to a build-requirement panel's +0x28; in standalone a house/info
    // dialog can leave +0x28 NULL. Skip all three IsDerivedFrom probes when null — the
    // desc stays unresolved and the function returns cleanly at `if (!desc)` below.
    // Try path 1: datasource IsA BuildComponent
    bool isBuild = dataSource && fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentDescription_FC);
    if (isBuild && dataSource) {
        // Re-verify (matches original double-check pattern)
        isBuild = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentDescription_FC);
        void* desc = isBuild ? dataSource : nullptr;
        *(void**)(s + 0x30) = desc;
    }
    // Try path 2: datasource IsA ComponentEntity
    else {
        bool isEntity = dataSource && fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
        if (isEntity && dataSource) {
            // Re-verify and extract description at entity+0x28
            isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
            void* entity = isEntity ? dataSource : nullptr;
            *(void**)(s + 0x30) = *(void**)((uintptr_t)entity + 0x28);
        }
        // Try path 3: datasource IsA Object::Internal (UI Element)
        else {
            uint32_t internalTypeId = g_TypeId_ObjectInternal ? *g_TypeId_ObjectInternal : 0;
            bool isInternal = dataSource && fn_IsDerivedFrom(dataSource, internalTypeId);
            void* internalObj = isInternal ? dataSource : nullptr;

            uint8_t bpBuf[8];
            fn_InternalBasePtr_Ctor(bpBuf, internalObj);
            void* elemPtr = *(void**)bpBuf;

            if (elemPtr) {
                fn_InternalBasePtr_CheckPtr(bpBuf);
                void* userData = fn_Element_GetUserData(elemPtr);
                // Element_GetUserData can return NULL (element carries no user data);
                // Runtime IsDerivedFrom has no null-this check (crash READ 0x0 at +0xC039A),
                // so guard exactly as the +0x28 datasource probes above do.
                isBuild = userData && fn_IsDerivedFrom(userData, *g_TypeId_ComponentDescription_FC);
                void* descResolved = isBuild ? userData : nullptr;
                // Standalone divergence: the in-game Toolbar sub-item buttons store
                // their building DESCRIPTION entity as userData (toolbardialog.cpp
                // SetUserData(button, item+0x58)), not a BuildComponent. The retail
                // build path passes a BuildComponent here, but downstream only uses
                // `desc` as the object whose component array (+0x28 list / +0x30
                // count) is scanned for the ToolbarDescription — which the
                // description entity also carries (verified: the toolbar override's
                // FindCompInDesc on the same object resolves the ToolbarDescription).
                // So accept a non-BuildComponent userData when it presents a sane
                // component array, rather than bailing and leaving the title/cost
                // unset (which rendered as "???????" / "x x x").
                if (!descResolved && userData) {
                    int32_t compCount = *(int32_t*)((uintptr_t)userData + 0x30);
                    void*   compArr   = *(void**)((uintptr_t)userData + 0x28);
                    if (compCount > 0 && compCount < 256 && compArr) descResolved = userData;
                }
                *(void**)(s + 0x30) = descResolved;
            }
            // else: +0x30 stays as-is (from previous init or nullptr)
        }
    }

    // --- Process description if resolved ---
    uintptr_t desc = *(uintptr_t*)(s + 0x30);
    // IsDerivedFromOrTrust: all three datasource type checks above go through
    // fn_IsDerivedFrom, which is unreliable in the partial standalone factory tree
    // and returns false for a legit description entity — leaving +0x30 null so the
    // title/cost/status elements keep their .rsc placeholder ("????"). When the
    // tool opens the CreateBar (OpenToolDialog) the datasource IS the building's
    // description ComponentEntity, bound to +0x28 by DialogInstance::AddController.
    // If nothing resolved but +0x28 presents a sane component array (the same guard
    // already applied to the toolbar-button userData path below), trust it directly.
    if (!desc) {
        void* ds = *(void**)(s + 0x28);
        if (SaneHeapPtr(ds)) {
            int32_t cc = *(int32_t*)((uintptr_t)ds + 0x30);
            void*   ca = *(void**)((uintptr_t)ds + 0x28);
            if (cc > 0 && cc < 256 && SaneHeapPtr(ca)) {
                desc = (uintptr_t)ds;
                *(void**)(s + 0x30) = ds;
            }
        }
    }
#if defined(_DEBUG)
#endif
    if (!desc) return;

    // Find ToolbarDescription in description's component list
    void* buildReqComp = FindCompInDesc(desc, *g_TypeId_ToolbarDescription_FC);

    // Get dialog instance at +0x18
    void* dialogInst = *(void**)(s + 0x18);

    // --- Set icon sprite from ToolbarDescription ---
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
            // Create sprite from ToolbarDescription: SpriteSheet at +0x20, name at +0x28
            uintptr_t brc = (uintptr_t)buildReqComp;
            uint8_t sprite[16];
#ifdef _DEBUG
            Debug_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));
#else
            fn_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));
#endif

            fn_InternalBasePtr_CheckPtr(ibpBuf);
#ifdef _DEBUG
            Debug_ImageElement_SetSprite(imagePtr, sprite);
#else
            fn_ImageElement_SetSprite(imagePtr, sprite);
#endif
        }
    }

    // --- Set title text from ToolbarDescription ---
    {
        void* textElem = FindDialogElem(dialogInst, "textTitle");
        fn_InternalBasePtr_CheckPtr((void*)&textElem);

        bool isInternal = fn_IsDerivedFrom(textElem, *g_TypeId_ObjectInternal);
        void* checked = isInternal ? textElem : nullptr;
        uint8_t ibpBuf[8];
        fn_InternalBasePtr_Ctor(ibpBuf, checked);
        void* textPtr = *(void**)ibpBuf;

        if (textPtr && buildReqComp) {
            // Create UI::String from ToolbarDescription: StringTable at +0x30, name at +0x38
            uintptr_t brc = (uintptr_t)buildReqComp;
#if defined(_DEBUG)
#endif
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
    // Reset BOTH counters. Retail resets neither: it starts the FindElement loop at the
    // current +0x48 and appends to +0x58, because the controller is constructed fresh per
    // dialog and this runs exactly once on it. In standalone the controller is reused
    // across opens, so without a reset the arrays grow every time. (This is NOT what
    // caused the reqCount=4 / labelCount=3 mismatch — that was a double-count at the push
    // below — but the two must still be reset together, or the same drift reappears the
    // second time a tool's create-bar is opened.)
    *(uint32_t*)(s + 0x48) = 0;
    *(uint32_t*)(s + 0x58) = 0;

    // Find first label element using format "labelBuild%i"
    {
        System::String fmtBuf = System::String::Format("labelBuild%i", (uint64_t)*(uint32_t*)(s + 0x48));
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* retBuf[2] = {0, 0};
        fn_DialogInstance_FindElement(*(void**)(s + 0x18), retBuf, &fmtBuf);
        void* labelElem = retBuf[0];

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

            // Push into dynamic array at +0x40 (8-byte InternalBasePtr elements).
            // The stride-4 push helper cannot be used here — the element is an
            // 8-byte handle — so this grows the array and writes the slot itself.
            // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
            // new tail zeroed. Element is the 8-byte InternalBasePtr handle (byte-copy).
            uint32_t newCount = *(uint32_t*)(s + 0x48) + 1;
            GameColl::ArrayAt<GameColl::Ptr8>(s, 0x40).SetSize((int)newCount);
            uint64_t* slot = (uint64_t*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)(newCount - 1) * 8);
            *slot = *(uint64_t*)ibpBuf;

            // Find next label
            fmtBuf = System::String::Format("labelBuild%i", (uint64_t)newCount);
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            fn_DialogInstance_FindElement(*(void**)(s + 0x18), retBuf, &fmtBuf);
            labelElem = retBuf[0];
        }
    }

    // --- Find ResourceComponent (the build-cost "BuildDescription") in the desc ---
    // Each entry's sub-entity (the resource-type ComponentEntity) is resolved during
    // BuildDescription_Serialize. The CreateFenced Pasture used to land here with an
    // empty entry (null sub-entity) because its `:build` sub-resource is FindOrLoad'd
    // multiple times and the empty reference-mode re-loads clobbered the good first
    // pass — fixed in code/Game/Component/builddescription.cpp (keep-populated guard).
    // The null-resType guard below stays as defensive belt-and-suspenders.
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
            // SetSize takes arrayBase = {data, count@+0x08, cap@+0x0c} and writes
            // the new count itself. Incrementing +0x58 again here DOUBLE-COUNTED every
            // requirement: a two-material building ended with reqCount(+0x58)=4 against
            // labelCount(+0x48)=3, and UpdateToolUI_e700 — which iterates +0x58 and indexes
            // the +0x40 label array — then ran off the end (retail would have faulted; we
            // logged "[e700] WARN: null label element"). Retail's own inline expansion
            // stores the count once.
            // B.3.2: Collection::Array<T>::SetSize(n, fill) — retail 0x140003600,
            // stride 4 align 4, no init arm (every new slot takes the fill).
            GameColl::ArrayAt<GameColl::I32NoInit>(s, 0x50).SetSize(
                *(int*)(s + 0x58) + 1,
                *reinterpret_cast<const GameColl::I32NoInit*>(countPtr));

            // Get the sub-entity from this entry (pointer at entry+0x00)
            uintptr_t subEntity = *(uintptr_t*)entry;

            // Find RawMaterialDescription component in sub-entity's component list
            void* resType = FindCompInDesc(subEntity, *g_TypeId_RawMaterialDescription);

            // Set sprite on label element — but only if the resource type resolved.
            // With our standalone desc divergence (description used as desc), some
            // resource entries can lack a RawMaterialDescription (resType==null);
            // the retail BuildComponent path always has one. Dereferencing a null
            // resType at +0x20 was the "last food icon" (Pasture) crash. Guard it:
            // skip the sprite but still set the count text below.
            void* label = *(void**)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8);
            if (SaneHeapPtr(resType)) {
                // Create sprite from RawMaterialDescription: SpriteSheet at +0x20, name at +0x38
                uint8_t sprite[24];
#ifdef _DEBUG
                Debug_Sprite_Ctor(sprite, *(uint64_t*)((uintptr_t)resType + 0x20),
                               (void*)((uintptr_t)resType + 0x38));
#else
                fn_Sprite_Ctor(sprite, *(uint64_t*)((uintptr_t)resType + 0x20),
                               (void*)((uintptr_t)resType + 0x38));
#endif
                fn_InternalBasePtr_CheckPtr((void*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8));
#ifdef _DEBUG
                Debug_LabelElement_SetSprite(label, sprite);
#else
                fn_LabelElement_SetSprite(label, sprite);
#endif
            }

            // Format count as "%i" and set as text parameter 0
            System::String countStr = System::String::Format("%i", (uint64_t)*(uint32_t*)countPtr);
            fn_InternalBasePtr_CheckPtr((void*)(*(uintptr_t*)(s + 0x40) + (uintptr_t)i * 8));
            fn_TextElement_SetParameter(label, 0, &countStr);

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
