// engine/src/ui/consumeproduceui.cpp
// Clean reimplementation of ConsumeProduceUI game logic.
//
// ConsumeProduceUI (0x40 bytes) — production progress panel.
// Extends DialogController. data30 = combo element InternalPtr,
// data38 = progress element InternalPtr.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ComboElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/ProgressElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/ConsumeProduceDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (ENDGAME B.3.9)

// ConsumeProduceUI: PROMOTED from a bare UI_DIALOG_STUB 2026-08-24. The Tavern panel
// rendered its authored layout -- worker spinner, Inventory, "Alcohol Limit 500",
// "Potato 300" and a Product combo -- but the combo was ALWAYS EMPTY, so no product
// could ever be selected and the tavern brewed nothing. The three slot bodies in
// code/Game/Dialog/consumeproduceui.cpp were fully ported and wired into NOTHING: the
// stub vtable left 4/7/9 as no-ops, so the combo was never populated (Slot4), the
// progress bar never moved (Slot7) and a selection could never be written back to the
// component (Slot9). Same shape as CemeteryUI directly above.
//
// This is also what kept HappinessComponent's GOODS arm untestable: a tavern with no
// product makes no ale, IsActive's desc+0x25 arm never finds a matching good, and the
// source can never register. Wiring the panel is the prerequisite for that A/B.
// ENDGAME B.3.9: ComponentEntity / UI::ComboElement / UI::ProgressElement /
// ConsumeProduceDescription (was TypeId_ConsumeProduceComponent, bound to 0x1401c7388
// which really is ConsumeProduceDescription's cell -- the guarded object at
// component+0x18 is that description, matching Table 1's authority) / RawMaterialDescription
// all have a ported TypeId() now, so the EnsureTypeIds()/Ready() lazy-cache is dead.

// ============================================================
// ConsumeProduceUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Gets entity's ConsumeProduceComponent, finds "comboProduct" and
// "progressRemaining" UI elements, populates the combo with product
// entries (sprite + name) from the component's Description, and
// sets the combo to the current product selection.
//
// Original: 0x14005F3D0.

// [4] 0x14005f3d0
void ConsumeProduceUI::OnCreated() {
    uintptr_t s = (uintptr_t)this;

    // Bail on a dead / marked-for-removal datasource before probing it (see Slot7).
    if (!SA_DialogEntityAlive(this)) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    // Find ConsumeProduceComponent
    uint64_t component = 0;
    fn_FindComponent_ConsumeProduce(entityInternal, &component);

    // Get Description at component+0x18, check ConsumeProduceComponent
    void* desc = *(void**)((uintptr_t)component + 0x18);
    bool isCPComp = ::Object::IsDerivedFromNullSafe(desc, ConsumeProduceDescription::TypeId());
    void* cpComp = isCPComp ? desc : nullptr;

    // Find "comboProduct" element via DialogInstance
    void* dialogInst = *(void**)(s + 0x18);

    // Check if element exists (first FindElement call to test)
    bool hasCombo;
    {
        System::String nameStr("comboProduct");
        uint64_t testBuf = 0;
        ::new (&testBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        hasCombo = (testBuf != 0);
    }
    {
        static int nh = 0;
        if (nh < 6) {
            ++nh;
            fprintf(stderr, "[CPUI] Slot4 hasCombo(comboProduct found)=%d\n", hasCombo ? 1 : 0);
            fflush(stderr);
        }
    }

    if (hasCombo) {
        // Find "comboProduct" again, wrap as InternalBasePtr, check ComboElement type
        System::String nameStr("comboProduct");
        uint8_t elemBuf[16];
        ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        void* foundElem = *(void**)elemBuf;

        // IsDerivedFrom ComboElement type check
        bool isCombo = ::Object::IsDerivedFromNullSafe(foundElem, UI::ComboElement::TypeId());
        void* comboRaw = isCombo ? foundElem : nullptr;
        uint64_t comboBP = 0;
        new (&comboBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(comboRaw));   // B.3.3 bridge
        *(uint64_t*)(s + 0x30) = comboBP;

        // Set element ID 22000
        reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x30))->SetElementId(22000);  // B.3.6 bridge

        // Iterate description entries and populate combo
        // cpComp+0x28 = entries array ptr, cpComp+0x30 = entry count, stride 0x28
        uintptr_t cp = (uintptr_t)cpComp;
        // [CPUI] the Tavern's Product combo renders EMPTY. Three different things
        // produce that same blank dropdown and they need different fixes:
        //   * hasCombo false      -> the "comboProduct" element was never found, so this
        //                           whole block is skipped (no crash, blank combo)
        //   * cpComp null         -> the IsDerivedFrom below tests the DESCRIPTION at
        //                           component+0x18 against the COMPONENT type id, which
        //                           is the wrong class entirely
        //   * entryCount == 0     -> the recipe array on the description never
        //                           deserialized (see the unregistered-resize trap)
        // Print all three rather than guessing between them.
        {
            static int n = 0;
            if (n < 6) {
                ++n;
                fprintf(stderr, "[CPUI] Slot4 comp=%p desc(comp+0x18)=%p cpComp(afterCast)=%p "
                        "entryCount(desc+0x30)=%d entriesBase(desc+0x28)=%p\n",
                        (void*)component, *(void**)((uintptr_t)component + 0x18), cpComp,
                        cpComp ? *(int*)(cp + 0x30) : -1,
                        cpComp ? *(void**)(cp + 0x28) : nullptr);
                fflush(stderr);
            }
        }
        int entryCount = *(int*)(cp + 0x30);
        uintptr_t entriesBase = *(uintptr_t*)(cp + 0x28);

        for (int i = 0; i < entryCount; i++) {
            // Each entry at entriesBase + i*0x28 has sub-descriptions
            // at entry+0x10 = pointer to sub-entry
            uintptr_t entryPtr = *(uintptr_t*)(entriesBase + (uintptr_t)i * 0x28 + 0x10);

            // Search sub-entries for ResourceDescription type (TypeId_RawMaterialDescription)
            int subCount = *(int*)(entryPtr + 0x30);
            uintptr_t subArray = *(uintptr_t*)(entryPtr + 0x28);
            void* resourceDesc = nullptr;

            for (int j = 0; j < subCount; j++) {
                void* subEntry = *(void**)(subArray + (uintptr_t)j * 8);
                // Call GetTypeId virtual (vtable+0x10) and compare
                int typeId = reinterpret_cast<Object::Base*>(subEntry)->Type();  // B.3.8 bridge
                if (typeId == (int)RawMaterialDescription::TypeId()) {
                    // Verify with IsDerivedFrom
                    if (::Object::IsDerivedFromNullSafe((void*)subEntry, RawMaterialDescription::TypeId())) {
                        resourceDesc = subEntry;
                    }
                    break;
                }
            }

            // Create UI::String from description string table + entry name
            // String table at cpComp+0x20, name at entriesBase + i*0x28 + 0x20
            // Note: stride 0x28, offset for name within entry is calculated as:
            // base + i * (i+1) * 4 * 8 -> no, from disasm: LEA RCX,[RSI+RCX*8] where
            // RCX = RSI + RSI*4 = i * 5, so address = base + i*5*8 = base + i*0x28 + 0x20
            uintptr_t nameAddr = entriesBase + (uintptr_t)i * 0x28 + 0x20;
            uint64_t stringTable = *(uint64_t*)(cp + 0x20);
            uint8_t uiStrBuf[16];
            ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(stringTable)), *reinterpret_cast<const System::String*>(nameAddr));  // B.3.6 bridge

            // Convert UI::String to System::String
            System::String sysStr;
            ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStrBuf)->operator System::String());  // B.3.6 bridge

            // Create UI::Sprite from ResourceDescription
            // resourceDesc+0x20 = sprite sheet, resourceDesc+0x38 = sprite name
            void* comboElem = (void*)*(uint64_t*)(s + 0x30);
            uint8_t spriteBuf[32];
            uintptr_t rd = (uintptr_t)resourceDesc;
            ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(rd + 0x20))), *reinterpret_cast<const System::String*>(rd + 0x38));  // B.3.6 bridge

            // Add item to combo: sprite, string, null userData
            reinterpret_cast<UI::ComboElement*>(comboElem)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), sysStr, nullptr);  // B.3.6 bridge
        }

        // Get current product index and set combo selection
        uint32_t productIdx = fn_GetCurrentProductIndex((void*)component);
        int selIdx = (int)productIdx;
        if ((int)productIdx < 0) {
            // Invalid — reset to 0
            *(uint32_t*)(component + 0x50) = 0;
            selIdx = 0;
        }
        void* comboFinal = (void*)*(uint64_t*)(s + 0x30);
        reinterpret_cast<UI::ComboElement*>(comboFinal)->SetCurrentItem(selIdx);  // B.3.6 bridge
    }

    // Find "progressRemaining" element
    bool hasProgress;
    {
        System::String nameStr("progressRemaining");
        uint64_t testBuf2 = 0;
        ::new (&testBuf2) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        hasProgress = (testBuf2 != 0);
    }

    if (hasProgress) {
        System::String nameStr("progressRemaining");
        uint8_t elemBuf2[16];
        ::new (elemBuf2) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameStr));  // B.3.6 bridge
        void* progElem = *(void**)elemBuf2;

        // Check ProgressElement type
        bool isProgress = ::Object::IsDerivedFromNullSafe(progElem, UI::ProgressElement::TypeId());
        void* progRaw = isProgress ? progElem : nullptr;
        uint64_t progBP = 0;
        new (&progBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(progRaw));   // B.3.3 bridge
        *(uint64_t*)(s + 0x38) = progBP;

        // Set initial progress to 1.0f at element+0x124
        void* progFinal = (void*)*(uint64_t*)(s + 0x38);
        *(float*)((uintptr_t)progFinal + 0x124) = 1.0f;
    }
}

// ============================================================
// ConsumeProduceUI::Slot7 (vtable +0x38) — Update progress bar
// ============================================================
// Updates the production progress bar. Reads current/total ticks
// from the entity's components, calculates ratio, and formats
// as a percentage string.
//
// Original: 0x14005F1F0.

// [7] 0x14005f1f0
void ConsumeProduceUI::OnUpdateLayout() {
    // Bail if the datasource entity is gone or marked-for-removal — otherwise the
    // IsDerivedFrom below runs on a dying entity (crash class 2026-07-21). Reliable
    // now that removals are one-frame-deferred (entitylist.cpp Step 0): the entity
    // is valid marked-but-alive memory whenever this per-frame slot fires.
    if (!SA_DialogEntityAlive(this)) return;

    // data38 = progress element InternalPtr
    void* progressElem = (void*)this->data38;
    if (!progressElem) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    // Find component via FUN_14000b6f0 (same FindComponent used in StorageCapacityUI)
    void* component = nullptr;
    fn_FindComponent_ConsumeProduce(entityInternal, &component);

    // Get sub-component at +0x18 of found component, check if ConsumeProduceComponent
    void* subComp = *(void**)((uintptr_t)component + 0x18);
    bool isCPComp = ::Object::IsDerivedFromNullSafe(subComp, ConsumeProduceDescription::TypeId());
    void* cpComp = isCPComp ? subComp : nullptr;

    // Calculate production ratio: 1.0 - currentTicks / totalTicks
    int currentTicks = *(int*)((uintptr_t)component + 0x68);
    int totalTicks = *(int*)((uintptr_t)cpComp + 0x48);
    float ratio = 1.0f - (float)currentTicks / (float)totalTicks;

    // Store ratio at progress element +0x128
    void* progPtr = (void*)this->data38;
    *(float*)((uintptr_t)progPtr + 0x128) = ratio;

    // Format as integer percentage: (int)(ratio * 100.0 + 0.5)
    int percent = (int)(ratio * 100.0f + 0.5f);
    System::String str = System::String::Format("%i", (uint64_t)(uint32_t)percent);

    // Set on progress element
    reinterpret_cast<UI::ProgressElement*>(progPtr)->SetProgressTextParameter(0, str);  // B.3.6 bridge
}

// ============================================================
// ConsumeProduceUI::Slot9 (vtable +0x48) — Combo selection handler
// ============================================================
// When the combo selection changes (param3==0x14, param4==22000),
// reads the selected product index from the UI combo element and
// writes it to the entity's component.
//
// Original: 0x14005F330.

// [9] 0x14005f330
void ConsumeProduceUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4,
                                      int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 != 0x14 || param4 != 22000) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    // Find component
    void* component = nullptr;
    fn_FindComponent_ConsumeProduce(entityInternal, &component);

    // Read selection from combo element (data30 InternalPtr) at +0x118
    void* comboElem = (void*)this->data30;
    uint32_t selection = *(uint32_t*)((uintptr_t)comboElem + 0x118);

    // Write to found component at +0x50
    *(uint32_t*)((uintptr_t)component + 0x50) = selection;
}
