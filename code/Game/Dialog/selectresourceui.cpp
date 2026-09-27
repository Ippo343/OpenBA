// engine/src/ui/selectresourceui.cpp
// Clean reimplementation of SelectResourceUI shared methods.
//
// SelectResourceUI is the abstract base for CropUI, OrchardUI, PastureUI.
// +0x28 = entity (ComponentEntity*)
// +0x30 = combo element (InternalBasePtr, set during Slot4 initialization).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Dialog/SelectResourceUI.h"
#include "Dialog/CropUI.h"
#include "engine_funcs.h"
#include "system/String.h"
#include "Object/factory_registration.h"
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
#include "Component/SelectDescription.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Object/exe_type.h"   // DEFINE_EXE_ABSTRACT_TYPE

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
// When combo is non-null, adds items to the combo and sets selectedIdx
// to the combo index matching the current resource type. When combo is
// null, just counts matching items.
//
// This pattern is shared by PopulateCombo (0x140078830) and
// GetResourceCount (0x140078a40).

static int SelectResourceUI_IterateResources(void* self, void* combo, int* selectedIndexOut) {
    uintptr_t s = (uintptr_t)self;
    int count = 0;
    int selectedIdx = -1;

    // Belt-and-braces: the .rdata type-id globals are null until seeded
    // (reg_pipeline_typeids.cpp registers 0x1401c48a8); lazy-resolve by name so
    // the combo populate never derefs a null pointer (StorageUI EnsureGlobals
    // pattern).
    static uint32_t s_sd = 0, s_ce = 0;
    if (!g_TypeId_SelectDescription) {
        s_sd = SelectDescription::TypeId();
        if (s_sd) g_TypeId_SelectDescription = &s_sd;
    }
    if (!g_TypeId_ComponentEntity) {
        s_ce = ComponentEntity::TypeId();
        if (s_ce) g_TypeId_ComponentEntity = &s_ce;
    }
    if (!g_TypeId_SelectDescription || !g_TypeId_ComponentEntity) return 0;

    // Get resource type list from game world
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t scenarioPtr = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    int numTypes = *(int*)(scenarioPtr + 0x128);
    uintptr_t typeArray = *(uintptr_t*)(scenarioPtr + 0x120);

    // Get current resource type via virtual call (vtable +0x60)
    // Returns the currently selected resource type pointer
    SelectResourceUI* const ui = reinterpret_cast<SelectResourceUI*>(self);   // B.3.8 bridge
    uint64_t currentResource = 0;
    if (combo) {
        ui->GetResource(&currentResource);                        // slot 12 (+0x60)
    }

    // Get category filter via virtual call (vtable +0x68)
    int categoryFilter = ui->GetCategory();                       // slot 13 (+0x68)

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
            int subTypeId = reinterpret_cast<Object::Base*>(subComp)->Type();  // B.3.8 bridge

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
// SelectResourceUI::PopulateCombo (0x140078830) — Fill combo
// ============================================================
// Clears the combo, iterates game world resource types, finds
// SelectDescription sub-components matching the category filter,
// and adds them as combo items with sprites and names.
// Sets the current selection to match the entity's current resource.
//
// Original: FUN_140078830 (was fn_SelectResourceUI_PopulateCombo).

void SelectResourceUI_PopulateCombo(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Get the selectResource combo element
    void* combo = *(void**)(s + 0x30);

    // Clear existing items
    reinterpret_cast<UI::ComboElement*>(combo)->RemoveAllItems();  // B.3.6 bridge

    // Populate combo and get selected index
    int selectedIdx = -1;
    SelectResourceUI_IterateResources(self, combo, &selectedIdx);

    // Set current selection if we found a match
    if (selectedIdx >= 0) {
        reinterpret_cast<UI::ComboElement*>(*(void**)(s + 0x30))->SetCurrentItem(selectedIdx);  // B.3.6 bridge
    }
}

// ============================================================
// SelectResourceUI::GetResourceCount (0x140078a40) — Count matching
// ============================================================
// Same iteration as PopulateCombo but just counts matching types.
// Used by Slot8 to detect when the combo needs refreshing.
//
// Original: FUN_140078a40 (was fn_SelectResourceUI_GetResourceCount).

int SelectResourceUI_GetResourceCount(void* self) {
    return SelectResourceUI_IterateResources(self, nullptr, nullptr);
}

// ============================================================
// SelectResourceUI::Slot4 (vtable +0x20) — Initialize combo element
// ============================================================
// Finds the "selectResource" combo element in the dialog, stores at +0x30,
// sets its element ID to 500, and populates it with resources.
//
// Original: 0x140078be0 (shared by CropUI, OrchardUI, PastureUI).

// [4] 0x140078be0.
void SelectResourceUI::OnCreated() {
    uintptr_t s = (uintptr_t)this;

    // Find "selectResource" combo element via DialogInstance at +0x18
    System::String nameBuf("selectResource");

    void* dialogInstance = *(void**)(s + 0x18);
    uint64_t findResult = 0;
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialogInstance)->FindElement(nameBuf));  // B.3.6 bridge

    // Store combo element at +0x30 (IsDerivedFrom check skipped -- null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    new (&basePtrVal) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    *(uint64_t*)(s + 0x30) = basePtrVal;

    // Set element ID to 500 (combo selection event ID)
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x30))->SetElementId(500);  // B.3.6 bridge

    // Populate combo with available resources
    SelectResourceUI_PopulateCombo(this);
}

// ============================================================
// SelectResourceUI::Slot8 (vtable +0x40) — Repopulate combo if count changed
// ============================================================
// Checks if the combo element's item count matches the expected resource count.
// If different, repopulates the combo with current resources.
//
// Original: 0x140078D30 (shared by OrchardUI and PastureUI).

// [8] 0x140078d30.
void SelectResourceUI::OnUpdate(const System::Time& /*time*/) {
    uintptr_t s = (uintptr_t)this;

    void* comboElem = *(void**)(s + 0x30);
    int currentCount = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemCount();  // B.3.6 bridge
    int expectedCount = SelectResourceUI_GetResourceCount(this);

    if (currentCount != expectedCount) {
        SelectResourceUI_PopulateCombo(this);
    }
}

// ============================================================
// SelectResourceUI::Slot9 (vtable +0x48) — Combo selection handler
// ============================================================
// On combo selection (msg=0x14, id=500), looks up the selected resource
// type from the game world's resource list and calls Slot11 (vtable +0x58)
// to apply it to the component.
//
// Original: 0x140078D80 (shared by CropUI and OrchardUI).

// [9] 0x140078d80 — the element pointer's address and (event, id) as the body reads them.
void SelectResourceUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4,
                                      int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (param3 != 0x14 || param4 != 500)
        return;

    uintptr_t s = (uintptr_t)this;
    void* comboElem = *(void**)(s + 0x30);

    // Get current selected item index from combo
    int currentItem = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetCurrentItem();  // B.3.6 bridge

    // Get the item data (resource index) for the current selection
    void* itemData = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemData(currentItem);  // B.3.6 bridge
    int resourceIndex = (int)(intptr_t)itemData;

    // Look up resource type from game world's entity list
    void* world = *g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)((uintptr_t)world + 0xf0);
    uintptr_t resourceArray = *(uintptr_t*)(entityList + 0x120);
    int64_t resourceType = *(int64_t*)(resourceArray + (int64_t)resourceIndex * 8);

    // Slot 11 (polymorphic -- sets resource type on component)
    SetResource(resourceType);   // slot 11, virtual (as retail)
}

// ============================================================================
// ENDGAME B.3.8 PNL — SelectResourceUI as a REAL abstract class
// (SelectResourceUI.h). Retail never builds one (create 0x140003730 returns
// 0); its table 0x14015d330 is what every leaf ctor stamps first and every
// leaf dtor restamps last. Every leaf overrides 4 and 7; slot 9 runs for
// CropUI (inherited, as in retail); 8 is overridden by all three in the tree.
// ============================================================================

// [0] 0x140013ca0: ~SelectResourceUI (restamp, ~DialogController), MemFree on &1.
Object::Base* SelectResourceUI::DeletingDtor(unsigned flags) {
    this->~SelectResourceUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [1] TypeName / [2] Type, the record and retail's abstract factory (create =
// the logging stub, in-place = null): ENDGAME B.3.9 RF.
DEFINE_EXE_ABSTRACT_TYPE(SelectResourceUI, "UI::DialogController")




