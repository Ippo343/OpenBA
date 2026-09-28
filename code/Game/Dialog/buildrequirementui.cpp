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
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/VSlot.h"      // B.3.x W0: Object::SlotAddressOf
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ImageElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/LabelElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/BuildComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/BuildDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Component/ComponentDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/ToolbarDescription.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
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
        if (!SaneHeapPtr(vtable) ||
            Object::SlotAddressOf(comp, &Object::Base::Type) == nullptr) continue;   // B.3.x: slot from the member
        int compTypeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

        if (compTypeId == (int)typeId) {
            if (::Object::IsDerivedFromNullSafe(comp, typeId)) {
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
    ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
    void* result = *(void**)retBuf;
    return result;
}

// Helper: find element by name, wrap as InternalBasePtr
static uint64_t FindDialogElemAsBasePtr(void* dialogInst, const char* name) {
    void* raw = FindDialogElem(dialogInst, name);
    bool isInternal = ::Object::IsDerivedFromNullSafe(raw, UI::Element::TypeId());
    void* checked = isInternal ? raw : nullptr;
    uint8_t bpBuf[8];
    new (bpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    return *(uint64_t*)bpBuf;
}

// ============================================================
// BuildRequirementUI::Shutdown (vtable +0x18) — No-op
// ============================================================

// ---- BuildRequirementUI: PROMOTED to the real reimpl (2026-06-28) ------------
// This dialog is constructed + its Slot4(Update +0x20) dispatched on hover of a
// build-tool button (verified live: valid dlgInst@+0x18 / datasource@+0x28). The
// generic stub left Slot4 a no-op, so the build tooltip showed "????????" for the
// name and "x x x" for the cost. Wiring Slot4 to the real BuildRequirementUI_Slot4
// composes the title (ToolbarDescription+0x38 name key), the icon, and the
// cost labels. Construction stays the stub alloc+zero+vtable; the engine fills the
// base fields afterward.

// ENDGAME B.3.9: the exe .rdata type-id globals below all have a ported TypeId() now,
// so the EnsureTypeIds() lazy-cache/heal is dead. NOTE (Table 1/3 correction): this
// file's old comment claimed TypeId_ComponentDescription_FC (cell 0x1401c7318) was
// "BuildComponent" and healed it with BuildComponent::TypeId() when null — but the
// cell's real class (its RegisterExeAddr / Fill* binding, reg_tradeui_typeids.cpp:127,
// game_main.cpp Fill*) is ComponentDescription, and the heal never actually ran (the
// pointer is non-null from EngineFuncsInit before this file's first call), so no
// behaviour ever depended on the wrong guess. Converted to ComponentDescription::TypeId()
// per the cell, not the stale comment.

// ============================================================
// BuildRequirementUI::Slot4 (vtable +0x20) — Setup/populate
// ============================================================
// Resolves the building description from the datasource,
// finds ToolbarDescription for icon/title display,
// finds ResourceComponent for material requirement list.
// Populates label elements with resource sprites and counts.
//
// Original: 0x14005E7C0.

// [4] 0x14005e7c0. The body unconditionally dereferences these globals;
// if any failed to resolve, skip rather than fault (standalone-only).
void BuildRequirementUI::OnCreated() {
    uintptr_t s = (uintptr_t)this;

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
    bool isBuild = dataSource && ::Object::IsDerivedFromNullSafe(dataSource, ComponentDescription::TypeId());
    if (isBuild && dataSource) {
        // Re-verify (matches original double-check pattern)
        isBuild = ::Object::IsDerivedFromNullSafe(dataSource, ComponentDescription::TypeId());
        void* desc = isBuild ? dataSource : nullptr;
        *(void**)(s + 0x30) = desc;
    }
    // Try path 2: datasource IsA ComponentEntity
    else {
        bool isEntity = dataSource && ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
        if (isEntity && dataSource) {
            // Re-verify and extract description at entity+0x28
            isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
            void* entity = isEntity ? dataSource : nullptr;
            *(void**)(s + 0x30) = *(void**)((uintptr_t)entity + 0x28);
        }
        // Try path 3: datasource IsA Object::Internal (UI Element)
        else {
            uint32_t internalTypeId = UI::Element::TypeId();
            bool isInternal = dataSource && ::Object::IsDerivedFromNullSafe(dataSource, internalTypeId);
            void* internalObj = isInternal ? dataSource : nullptr;

            uint8_t bpBuf[8];
            new (bpBuf) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(internalObj));   // B.3.3 bridge
            void* elemPtr = *(void**)bpBuf;

            if (elemPtr) {
                void* userData = reinterpret_cast<UI::Element*>(elemPtr)->GetUserData();  // B.3.6 bridge
                // Element_GetUserData can return NULL (element carries no user data);
                // Runtime IsDerivedFrom has no null-this check (crash READ 0x0 at +0xC039A),
                // so guard exactly as the +0x28 datasource probes above do.
                isBuild = userData && ::Object::IsDerivedFromNullSafe(userData, ComponentDescription::TypeId());
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
    void* buildReqComp = FindCompInDesc(desc, ToolbarDescription::TypeId());

    // Get dialog instance at +0x18
    void* dialogInst = *(void**)(s + 0x18);

    // --- Set icon sprite from ToolbarDescription ---
    {
        void* imageElem = FindDialogElem(dialogInst, "imageIcon");

        // Check if element is an Object::Internal for InternalBasePtr wrapping
        bool isInternal = ::Object::IsDerivedFromNullSafe(imageElem, UI::Element::TypeId());
        void* checked = isInternal ? imageElem : nullptr;
        uint8_t ibpBuf[8];
        new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
        void* imagePtr = *(void**)ibpBuf;

        if (imagePtr && buildReqComp) {
            // Create sprite from ToolbarDescription: SpriteSheet at +0x20, name at +0x28
            uintptr_t brc = (uintptr_t)buildReqComp;
            uint8_t sprite[16];
#ifdef _DEBUG
            Debug_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));
#else
            ::new (sprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(brc + 0x20))), *reinterpret_cast<const System::String*>(brc + 0x28));  // B.3.6 bridge
#endif

#ifdef _DEBUG
            Debug_ImageElement_SetSprite(imagePtr, sprite);
#else
            reinterpret_cast<UI::ImageElement*>(imagePtr)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge
#endif
        }
    }

    // --- Set title text from ToolbarDescription ---
    {
        void* textElem = FindDialogElem(dialogInst, "textTitle");

        bool isInternal = ::Object::IsDerivedFromNullSafe(textElem, UI::Element::TypeId());
        void* checked = isInternal ? textElem : nullptr;
        uint8_t ibpBuf[8];
        new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
        void* textPtr = *(void**)ibpBuf;

        if (textPtr && buildReqComp) {
            // Create UI::String from ToolbarDescription: StringTable at +0x30, name at +0x38
            uintptr_t brc = (uintptr_t)buildReqComp;
#if defined(_DEBUG)
#endif
            uint8_t uiStr[16];
            ::new (uiStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(brc + 0x30))), *reinterpret_cast<const System::String*>(brc + 0x38));  // B.3.6 bridge

            reinterpret_cast<UI::TextElement*>(textPtr)->SetText(reinterpret_cast<const UI::String*>(uiStr));  // B.3.6 bridge
        }
    }

    // --- Find "labelBuildMulti" element, hide initially ---
    void* labelMulti = FindDialogElem(dialogInst, "labelBuildMulti");
    if (labelMulti) {
        reinterpret_cast<UI::Element*>(labelMulti)->SetFlagsOnOff(1, 0);  // B.3.6 bridge; hide
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
        void* retBuf[2] = {0, 0};
        ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(*(void**)(s + 0x18))->FindElement(fmtBuf));  // B.3.6 bridge
        void* labelElem = retBuf[0];

        while (labelElem) {

            // Wrap as InternalBasePtr
            bool isInternal = ::Object::IsDerivedFromNullSafe(labelElem, UI::Element::TypeId());
            void* checked = isInternal ? labelElem : nullptr;
            uint8_t ibpBuf[8];
            new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
            void* wrappedElem = *(void**)ibpBuf;
            if (!wrappedElem) break;

            // Hide the label initially
            reinterpret_cast<UI::Element*>(wrappedElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge

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
            ::new (retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(*(void**)(s + 0x18))->FindElement(fmtBuf));  // B.3.6 bridge
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
    void* resourceComp = FindCompInDesc(desc, BuildDescription::TypeId());
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
            void* resType = FindCompInDesc(subEntity, RawMaterialDescription::TypeId());

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
                ::new (sprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)resType + 0x20))),
                               *reinterpret_cast<const System::String*>((uintptr_t)resType + 0x38));  // B.3.6 bridge
#endif
#ifdef _DEBUG
                Debug_LabelElement_SetSprite(label, sprite);
#else
                reinterpret_cast<UI::LabelElement*>(label)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge
#endif
            }

            // Format count as "%i" and set as text parameter 0
            System::String countStr = System::String::Format("%i", (uint64_t)*(uint32_t*)countPtr);
            reinterpret_cast<UI::TextElement*>(label)->SetParameter(0, countStr);  // B.3.6 bridge

            // Show the label
            reinterpret_cast<UI::Element*>(label)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        }
    }

check_multi:
    // If resource component has multi-build flag at +0x34, show "labelBuildMulti"
    if (*(char*)(rc + 0x34) != 0) {
        if (labelMulti) {
            reinterpret_cast<UI::Element*>(labelMulti)->SetFlagsOnOff(0, 1);  // B.3.6 bridge; show
        }
    }
}
