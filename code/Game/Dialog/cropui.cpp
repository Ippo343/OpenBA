// engine/src/ui/cropui.cpp
// Clean reimplementation of CropUI game logic.
//
// CropUI (0x60 bytes) — crop selection panel.
// Extends SelectResourceUI (abstract) > DialogController.
// data30 (internalBasePtr) = selectResource combo element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
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

// ============================================================
// SelectDescription layout (from Ghidra analysis):
//   +0x20: SpriteSheet (ExternalPtr, 8 bytes)
//   +0x28: SpriteName (System::String, 8 bytes)
//   +0x30: StringTable (ExternalPtr, 8 bytes)
//   +0x38: StringName (System::String, 8 bytes)
//   +0x40: categoryId (int)
// ============================================================

// ============================================================
// Resource type list in game world (from DAT_1401c3dd8 + 0xF0):
//   +0x120: resourceTypes array pointer (array of pointers)
//   +0x128: resourceTypes count (int)
//
// Each resource type entry:
//   +0x28: sub-components array pointer (array of pointers)
//   +0x30: sub-components count (int)
// ============================================================

// Helper: iterate game world resource types, find SelectDescription
// sub-components matching the category filter, and either count them
// or populate a combo box.
//
// When combo is non-null, adds items to the combo and returns the
// selected index. When combo is null, just counts matching items.
//
// This pattern is shared by CropUI_PopulateCombo (0x140078830)
// and CropUI_GetExpectedCount (0x140078a40).

static int CropUI_IterateResources(CropUI* self, void* combo, int* selectedIndexOut) {
    uintptr_t s = (uintptr_t)self;
    int count = 0;
    int selectedIdx = -1;

    // Get resource type list from game world
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t scenarioPtr = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    int numTypes = *(int*)(scenarioPtr + 0x128);
    uintptr_t typeArray = *(uintptr_t*)(scenarioPtr + 0x120);

    // Get current resource type via virtual call (vtable +0x60)
    // Returns the currently selected resource type pointer at local_80
    typedef void (*fn_GetCurrentResource_t)(void* self, void* outPtr);
    void** vtable = *(void***)self;
    fn_GetCurrentResource_t fnGetCurrent =
        reinterpret_cast<fn_GetCurrentResource_t>(vtable[0x60 / 8]);
    uint64_t currentResource = 0;
    fnGetCurrent(self, &currentResource);

    // Get category filter via virtual call (vtable +0x68)
    typedef int (*fn_GetCategoryFilter_t)(void* self);
    fn_GetCategoryFilter_t fnGetCategory =
        reinterpret_cast<fn_GetCategoryFilter_t>(vtable[0x68 / 8]);
    int categoryFilter = fnGetCategory(self);

    // Bail on a dead / marked-for-removal datasource before probing it (see Slot7);
    // returns "no resources" (0) for this iterate helper.
    if (!SA_DialogEntityAlive(self)) return 0;
    // Wrap entity at +0x28 in InternalBasePtr (base class requirement)
    void* entity = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge

    int comboIdx = 0;

    for (int i = 0; i < numTypes; i++) {
        uintptr_t resType = *(uintptr_t*)(typeArray + (uintptr_t)i * 8);

        // Iterate sub-components of this resource type
        int subCount = *(int*)(resType + 0x30);
        uintptr_t subArray = *(uintptr_t*)(resType + 0x28);

        for (int j = 0; j < subCount; j++) {
            uintptr_t subComp = *(uintptr_t*)(subArray + (uintptr_t)j * 8);

            // Check if sub-component's type matches SelectDescription
            // Virtual call at vtable+0x10 returns type ID
            typedef int (*GetTypeId_t)(void*);
            void** subVtable = *(void***)subComp;
            GetTypeId_t fnGetTypeId = reinterpret_cast<GetTypeId_t>(subVtable[0x10 / 8]);
            int subTypeId = fnGetTypeId((void*)subComp);

            if (subTypeId == (int)*g_TypeId_SelectDescription) {
                void* subObj = *(void**)(subArray + (uintptr_t)j * 8);
                bool isDerived = fn_IsDerivedFrom(subObj, *g_TypeId_SelectDescription);
                if (isDerived && subObj != nullptr) {
                    // Check category filter matches
                    int category = *(int*)((uintptr_t)subObj + 0x40);
                    if (category == categoryFilter) {
                        if (combo) {
                            // Create UI::String from SelectDescription name
                            uint8_t uiStrBuf[16];
                            ::new (uiStrBuf) UI::String(
                                            Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)subObj + 0x30))),
                                            *reinterpret_cast<const System::String*>((uintptr_t)subObj + 0x38));  // B.3.6 bridge
                            // Convert to System::String
                            System::String sysStr;
                            ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStrBuf)->operator System::String());  // B.3.6 bridge

                            // Create Sprite from SelectDescription icon
                            uint8_t spriteBuf[16];
                            ::new (spriteBuf) UI::Sprite(
                                          Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)subObj + 0x20))),
                                          *reinterpret_cast<const System::String*>((uintptr_t)subObj + 0x28));  // B.3.6 bridge

                            // Add item to combo with index as userData
                            reinterpret_cast<UI::ComboElement*>(combo)->AddItem(*reinterpret_cast<const UI::Sprite*>(spriteBuf), sysStr, (void*)(intptr_t)comboIdx);  // B.3.6 bridge

                            // Check if this is the currently selected resource
                            if (resType == currentResource) {
                                selectedIdx = count;
                            }
                        }
                        count++;
                    }
                }
                break;  // Found the SelectDescription for this type, move to next
            }
        }
        comboIdx++;
    }

    if (selectedIndexOut) *selectedIndexOut = selectedIdx;
    return count;
}

// ============================================================
// PopulateCombo (0x140078830): Fill combo with available crops
// ============================================================
// Clears the combo, iterates game world resource types, finds
// SelectDescription sub-components matching the crop category,
// and adds them as combo items with sprites and names.
// Sets the current selection to match the entity's current crop.
//
// Original: FUN_140078830

void CropUI_PopulateCombo(CropUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get the selectResource combo element
    void* comboIBP = (void*)(s + 0x30);
    void* combo = *(void**)comboIBP;

    // Clear existing items
    reinterpret_cast<UI::ComboElement*>(combo)->RemoveAllItems();  // B.3.6 bridge

    // Populate combo and get selected index
    int selectedIdx = -1;
    CropUI_IterateResources(self, combo, &selectedIdx);

    // Set current selection if we found a match
    if (selectedIdx >= 0) {
        reinterpret_cast<UI::ComboElement*>(*(void**)comboIBP)->SetCurrentItem(selectedIdx);  // B.3.6 bridge
    }
}

// ============================================================
// GetExpectedCount (0x140078a40): Count matching crop types
// ============================================================
// Same iteration as PopulateCombo but just counts matching types.
// Used by Slot8 to detect when the combo needs refreshing.
//
// Original: FUN_140078a40

int CropUI_GetExpectedCount(CropUI* self) {
    return CropUI_IterateResources(self, nullptr, nullptr);
}

// ============================================================
// CropUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds the "selectResource" combo element in the dialog,
// wraps it as InternalBasePtr and stores at +0x30.
// Sets element ID 500, then calls the populate combo helper
// to fill it with available crop types.
//
// Original: 0x140078BE0.

void CropUI_Slot4(CropUI* self) {
    // Get dialog instance from DialogController base
    void* dialogInst = *(void**)((uintptr_t)self + 0x18);

    // Find "selectResource" element
    System::String nameBuf("selectResource");

    uint8_t elemBuf[8] = {0};
    ::new (elemBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInst)->FindElement(nameBuf));  // B.3.6 bridge
    void* foundElem = *(void**)elemBuf;

    // Wrap in InternalBasePtr, store at +0x30
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(foundElem));   // B.3.3 bridge
    self->internalBasePtr = *(uint64_t*)basePtrBuf;

    // Set element ID 500
    reinterpret_cast<UI::Element*>(self->internalBasePtr)->SetElementId(500);  // B.3.6 bridge

    // Populate the combo with available crop types
    CropUI_PopulateCombo(self);
}

// ============================================================
// CropUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables or disables the selectResource combo based on whether
// it has any items. Uses virtual call at vtable+0x40 (SetEnabled).
//
// Original: 0x140078CB0.

void CropUI_Slot7(CropUI* self) {
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(self)) return;
    // Wrap entity in InternalBasePtr (base class requirement, not used directly)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge

    // Get selectResource combo element
    void* combo = (void*)self->internalBasePtr;

    // Enable combo if it has items, disable if empty
    int itemCount = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge

    // Virtual call: vtable[8] = SetEnabled(hasItems)
    reinterpret_cast<UI::Element*>(combo)->Enable(itemCount > 0);  // B.3.6 bridge: Element slot 8 Enable
}

// ============================================================
// CropUI::Slot8 (vtable +0x40) — Refresh check
// ============================================================
// Compares current combo item count with expected count.
// If they differ, repopulates the combo with available crop types.
//
// Original: 0x140078D30.

void CropUI_Slot8(CropUI* self) {
    void* combo = (void*)self->internalBasePtr;

    int currentCount = reinterpret_cast<const UI::ComboElement*>(combo)->GetItemCount();  // B.3.6 bridge
    int expectedCount = CropUI_GetExpectedCount(self);

    if (currentCount != expectedCount) {
        CropUI_PopulateCombo(self);
    }
}
