// code/Game/Dialog/upgradeui.cpp
// Clean reimplementation of UpgradeUI game logic.
//
// UpgradeUI (0x38 bytes) — upgrade panel controller.
// Extends DialogController (0x30 base).
// +0x30 = button element (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ImageElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Object/factory_registration.h"
#include "Dialog/UpgradeUIConfig.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "runtime/types/ui/ButtonElement.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/ToolbarDescription.h"   // X::TypeId() (B.3.9 CG2)

namespace upgradeui_sa {

// ENDGAME B.3.9 CG2: TypeId_UpgradeUIConfig -> UpgradeUIConfig::TypeId(),
// TypeId_ComponentEntity -> ComponentEntity::TypeId(), TypeId_HelpButtonElem
// (shares its initialiser exref 0x1401514f8 with TypeId_ButtonElement) ->
// UI::ButtonElement::TypeId(), TypeId_ImageElement -> UI::ImageElement::TypeId(),
// TypeId_ToolbarDescription_FC -> ToolbarDescription::TypeId(). The class statics
// always answer, so the lazy-fill retires.
void EnsureGlobals() {
}

bool Ready() {
    return fn_IsDerivedFrom && fn_InternalBasePtr_Ctor;
}

// Serialize is only safe when the bound config is an UpgradeUIConfig with a
// non-null target template at +0x20 (the stub config zero-fills it until the
// real UpgradeUIConfig port lands).
bool ConfigOk(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* cfg = *reinterpret_cast<void**>(s + 0x10);
    if (!cfg || !fn_IsDerivedFrom(cfg, UpgradeUIConfig::TypeId())) return false;
    return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cfg) + 0x20) != nullptr;
}



}  // namespace upgradeui_sa

// ============================================================
// UpgradeUI::Serialize (vtable +0x20) — Slot4
// ============================================================
// Finds the upgrade button element, creates a sprite from the
// ToolbarDescription on the upgrade target template (UpgradeUIConfig+0x20),
// and sets the sprite on the button's child image element.
//
// Original: 0x140086d90
// [4] 0x140086d90
void UpgradeUI::OnCreated() {
    upgradeui_sa::EnsureGlobals();
    if (!upgradeui_sa::Ready()) return;
    if (!upgradeui_sa::ConfigOk(this)) {
        return;
    }
    uintptr_t s = (uintptr_t)this;

    // Get config from DialogController +0x10
    void* desc = *(void**)(s + 0x10);

    // Check if config is an UpgradeUIConfig (its +0x20 holds the upgrade
    // target ComponentEntity template)
    bool isUpgrade = fn_IsDerivedFrom(desc, UpgradeUIConfig::TypeId());
    void* upgradeDesc = isUpgrade ? desc : nullptr;

    // CheckPtr on the InternalBasePtr at +0x20 before reading
    void* elem = *(void**)(s + 0x20);

    // Check if element is a ButtonElement (exref 0x1401514f8)
    bool isButton = fn_IsDerivedFrom(elem, UI::ButtonElement::TypeId());
    void* checkedElem = isButton ? elem : nullptr;

    // Properly construct InternalBasePtr at +0x30 (button reference)
    new ((void*)(s + 0x30)) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedElem));   // B.3.3 bridge

    // Set button element ID to 33000
    void* button = *(void**)(s + 0x30);
    reinterpret_cast<UI::Element*>(button)->SetElementId(33000);  // B.3.6 bridge

    // Get button's child at index 4 (the ImageElement)
    button = *(void**)(s + 0x30);
    uint8_t childBuf[16];
    ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(button)->Child(4));  // B.3.6 bridge

    // Check if child is an ImageElement type
    void* childElem = *(void**)childBuf;
    bool isImage = fn_IsDerivedFrom(childElem, UI::ImageElement::TypeId());
    void* checkedImage = isImage ? childElem : nullptr;

    // Construct an IBP for the image element
    uint8_t imageIBP[8];
    new (imageIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checkedImage));   // B.3.3 bridge

    // Get the target ComponentEntity template from UpgradeUIConfig+0x20
    uintptr_t subDesc = *(uintptr_t*)((uintptr_t)upgradeDesc + 0x20);

    // Find ToolbarDescription in the target template's component list
    void* buildReqComp = nullptr;
    if (subDesc && *(int*)(subDesc + 0x30) > 0) {
        int compCount = *(int*)(subDesc + 0x30);
        uintptr_t compArray = *(uintptr_t*)(subDesc + 0x28);

        for (int i = 0; i < compCount; i++) {
            void* comp = *(void**)(compArray + (uintptr_t)i * 8);
            if (!comp) continue;

            // Call component vtable[2] to get type ID
            uint32_t typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge

            if (typeId == ToolbarDescription::TypeId()) {
                void* checked = fn_IsDerivedFrom(comp, ToolbarDescription::TypeId())
                    ? comp : nullptr;
                buildReqComp = checked;
                break;
            }
        }
    }

    // SA-guard: retail data guarantees a ToolbarDescription on the upgrade
    // target template; skip the icon when the standalone template lacks one.
    if (!buildReqComp) return;

    // Create a Sprite from the ToolbarDescription using proper constructor
    uintptr_t brc = (uintptr_t)buildReqComp;
    uint8_t sprite[16];
    ::new (sprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(brc + 0x20))), *reinterpret_cast<const System::String*>(brc + 0x28));  // B.3.6 bridge

    // Set sprite on the image element
    void* imageElem = *(void**)imageIBP;
    reinterpret_cast<UI::ImageElement*>(imageElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprite));  // B.3.6 bridge

    // Set button's user data to the target template
    void* subDescBase = *(void**)((uintptr_t)upgradeDesc + 0x20);
    button = *(void**)(s + 0x30);
    reinterpret_cast<UI::Element*>(button)->SetUserData(reinterpret_cast<Object::Base*>(subDescBase));  // B.3.6 bridge
}

// ============================================================
// UpgradeUI::OnRemove (vtable +0x48) — Slot9
// ============================================================
// On button click (ID 33000), finds DestroyComponent on the entity,
// copies the UpgradeDescription reference to it, and starts the upgrade.
//
// Original: 0x140086c90
// [9] 0x140086c90
void UpgradeUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t eventType, int32_t eventId,
                               int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    void* p2 = element;
    upgradeui_sa::EnsureGlobals();
    if (!p2 || !upgradeui_sa::Ready()) return;
    uintptr_t s = (uintptr_t)this;

    // Only handle button clicks (msg 0x12) with ID 33000
    if (eventType != 0x12 || eventId != 33000)
        return;

    // Verify the event source matches our stored button element
    void* button = *(void**)(s + 0x30);
    void* eventSource = *(void**)p2;
    if (eventSource != button)
        return;

    // Get the data source entity from DialogController +0x28
    void* dataSource = *(void**)(s + 0x28);
    if (!dataSource)
        return;

    // Don't start an upgrade on a building that is already dead / marked-for-removal
    // (the panel can linger one marked-but-alive frame under the removal deferral —
    // entitylist.cpp Step 0 — during which a click must not re-arm its DestroyComponent).
    if (!SA_DialogEntityAlive(this))
        return;

    // Check if data source is a ComponentEntity
    if (!fn_IsDerivedFrom(dataSource, ComponentEntity::TypeId()))
        return;

    // Find DestroyComponent on the entity
    uint8_t compBuf[16];
    fn_FindComponent_Destroy(dataSource, compBuf);
    void* destroyComp = *(void**)compBuf;
    if (!destroyComp) {
        std::fprintf(stderr, "[UpgradeUI] OnRemove: no DestroyComponent on "
                     "entity %p — upgrade aborted\n", dataSource);
        std::fflush(stderr);
        return;
    }

    // Get the config from DialogController +0x10
    void* desc = *(void**)(s + 0x10);
    if (!desc)
        return;

    // Check if config is an UpgradeUIConfig
    if (!fn_IsDerivedFrom(desc, UpgradeUIConfig::TypeId()))
        return;

    // Set the upgrade target on DestroyComponent
    // destroy+0x60 = config+0x20 (the target ComponentEntity template)
    *(uint64_t*)((uintptr_t)destroyComp + 0x60) = *(uint64_t*)((uintptr_t)desc + 0x20);

    // Start the upgrade: pump the BuildState state machine. 0x14011dfa0 is the
    // generic driver ("StartUpgrade" was a misnomer); the disasm-verified port
    // is RemovalCalc_StateMachine_dfa0 (keystone_a_removalcalcpath.cpp),
    // reached through the address registry.
    typedef void (*fn_SMDriver_t)(void* destroyComp);
    fn_SMDriver_t smDriver =
        reinterpret_cast<fn_SMDriver_t>(ResolveAddr(Module::Exe, 0x14011dfa0));
    std::fprintf(stderr, "[UpgradeUI] OnRemove: target=%p written to "
                 "destroyComp=%p +0x60, pumping SM (driver=%p)\n",
                 *(void**)((uintptr_t)desc + 0x20), destroyComp,
                 (void*)smDriver);
    std::fflush(stderr);
    if (smDriver) smDriver(destroyComp);
}

// ============================================================
// Standalone promotion — real UpgradeUI vtable (2026-07-20)
// ============================================================
// Retail vtable @ 0x14015df00 (11 slots): overrides 0,1,2,4,7,9. Slot 7
// (0x140086d80) is a 3-byte RET 0 no-op, slots 3,5,6,8,10 are inherited
// UI::DialogController base thunks. Standalone mirrors the residenceui_sa
// pattern: Slot4 (Serialize, icon + button wiring) is driven once from slot 8
// (OnUpdate) as a self-heal if the framework never calls slot 4; slot 9 is the
// click handler (eventType 0x12, element id 33000 → write the upgrade target
// template into DestroyComponent+0x60 and pump the state machine).
// Replaces the UI_DIALOG_STUB(UpgradeUI, ...) no-op registration.



// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void UpgradeUI::OnUpdate(const System::Time& /*time*/) {
    upgradeui_sa::EnsureGlobals();
    if (!upgradeui_sa::Ready()) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) UpgradeUI::OnCreated();
}


// [0] 0x140016d50: ~UpgradeUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* UpgradeUI::DeletingDtor(unsigned flags) {
    this->~UpgradeUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140016b30: MemAlloc(0x38, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* UpgradeUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(UpgradeUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) UpgradeUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* UpgradeUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) UpgradeUI());
}

DEFINE_EXE_TYPE(UpgradeUI, "UI::DialogController")
