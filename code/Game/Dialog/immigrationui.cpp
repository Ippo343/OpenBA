// engine/src/ui/immigrationui.cpp
// Clean reimplementation of ImmigrationUI game logic.
//
// ImmigrationUI (0x50 bytes) — immigration allow/deny panel.
// Extends DialogController. Fields:
//   +0x30 textNone, +0x38 textNomad (InternalBasePtr)
//   +0x40 buttonAllow, +0x48 buttonDeny (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdio>
#include "Object/factory_registration.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/ImmigrationComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Component/StateMachineComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    System::String nameBuf(name);

    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameBuf));  // B.3.6 bridge

    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    return *(uint64_t*)basePtrBuf;
}

// ---- ImmigrationUI: PROMOTED to a real SA vtable (immigrationui.cpp) ----------
// The Town Hall "Nomads" tab: Dialog/Immigration.rsc is inserted into the empty
// nomadTab GroupDescription and driven by ImmigrationUI (see Template/TownHall.rsc
// _controllers). The generic stub left every slot a no-op, so the tab showed the
// .rsc placeholder text and dead Allow/Deny buttons. Wiring Slot4 (+0x20 setup —
// finds textNone/textNomad/buttonAllow/buttonDeny, sets button ids), Slot7 (+0x38
// update — finds the ImmigrationComponent on the town-hall entity, enables the
// buttons + shows the nomad-request text when a group is waiting), and Slot9
// (+0x48 event — Allow=39000/Deny=0x9859 dispatch to the nomad camp's StateMachine)
// makes the tab live. Construction stays the stub alloc+zero+vtable; the framework
// fills the base fields (dialog +0x18, datasource entity +0x28) afterward — the
// same shape as the ResourceLimitUI/StatusBarUI promotions. The ported Slot7/Slot9
// degrade to the "no nomads" state if the entity/component is unbound in standalone
// (see immigrationui.cpp guards), so a missing ImmigrationComponent can't fault.
namespace immigrationui_sa {
    // ImmigrationUI Slot7/Slot9 dereference these exe-.rdata type-id globals (null in
    // standalone): the entity IsA check (ComponentEntity — the reimpl already guards a
    // null id), fn_FindComponent_Immigration (ImmigrationComponent), and
    // fn_FindComponent_BuildState (DAT_1401c7d98 = StateMachineComponent; the
    // engine_funcs label now matches, see
    // [[project_townhallui_promoted]]). Fill from the live factory tree post-Engine::
    // Startup; Entity_FindComponentByExeTypeId derefs the id ptr, so it must be live.
    // ENDGAME B.3.9: ComponentEntity / ImmigrationComponent / StateMachineComponent (was
    // TypeId_StateMachineComponent_FC) all have a ported TypeId() now, so the
    // EnsureGlobals() lazy-cache is dead; only the game-world singleton check stays.
    bool Ready() {
        return g_pGameWorldSingleton && *g_pGameWorldSingleton;
    }
    // stderr, not stdout: the perf build strips printf, and the Immigrants wave's
    // harness runs (tools/stress_run.sh) only keep standalone_error.log.
    void Trace(const char* slot, void* self) {
        static int n=0; if (n>=16) return; ++n;
        void* ent = self ? *reinterpret_cast<void**>((uintptr_t)self+0x28) : nullptr;
        fprintf(stderr, "[ImmigrationUI] %s self=%p entity(+0x28)=%p dlg(+0x18)=%p\n", slot, self, ent,
                self ? *reinterpret_cast<void**>((uintptr_t)self+0x18) : nullptr);
        fflush(stderr);
    }
}

// ============================================================
// ImmigrationUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds textNone, textNomad, buttonAllow, and buttonDeny
// elements in the dialog. Stores as InternalBasePtr and sets
// element IDs on the buttons.
//
// Original: 0x14006B330.

// [4] 0x14006b330
void ImmigrationUI::OnCreated() {
    immigrationui_sa::Trace("Slot4", this);
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find text elements
    this->data30 = FindElemAsBasePtr(dialogElem, "textNone");
    this->data38 = FindElemAsBasePtr(dialogElem, "textNomad");

    // Find button elements
    this->data40 = FindElemAsBasePtr(dialogElem, "buttonAllow");
    this->data48 = FindElemAsBasePtr(dialogElem, "buttonDeny");

    // Set element IDs on buttons
    reinterpret_cast<UI::Element*>(this->data40)->SetElementId(39000);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(this->data48)->SetElementId(0x9859);  // B.3.6 bridge
}

// ============================================================
// ImmigrationUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Finds ImmigrationComponent, checks build state to determine
// if nomads are available. Enables/disables buttons accordingly,
// shows/hides text, formats immigration count + world name.
//
// Original: 0x14006B130.

// [7] 0x14006b130
void ImmigrationUI::OnUpdateLayout() {
    if (!immigrationui_sa::Ready()) return;
    immigrationui_sa::Trace("Slot7", this);
    uintptr_t s = (uintptr_t)this;

    // Get entity at +0x28, check IsA ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* checkedEntity = nullptr;
    if (::Object::IsDerivedFromNullSafe(entity, entityTypeId)) {
        checkedEntity = entity;
    }

    // Wrap in InternalBasePtr for FindComponent
    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    // Find ImmigrationComponent
    uint64_t immigComp = 0;
    fn_FindComponent_Immigration((void*)entityBasePtr, &immigComp);

    // Standalone safety: retail always finds the ImmigrationComponent on the
    // town-hall entity, so it never guards. In standalone the dialog's entity
    // (+0x28) or the component may be unbound; degrade to the "no nomads" state
    // (buttons disabled, textNone shown) instead of dereferencing NULL.
    if (immigComp == 0) {
        void* bAllow = (void*)*(uint64_t*)(s + 0x40);
        void* bDeny  = (void*)*(uint64_t*)(s + 0x48);
        if (bAllow) reinterpret_cast<UI::Element*>(bAllow)->Enable(false);  // B.3.6 bridge: Element slot 8 Enable
        if (bDeny)  reinterpret_cast<UI::Element*>(bDeny)->Enable(false);  // B.3.6 bridge: Element slot 8 Enable
        // Show textNone (+0x30), hide textNomad (+0x38).
        reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x38))->SetFlagsOnOff(1, 0);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x30))->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        return;
    }

    // Read immigration count at comp+0x60
    uint32_t immigCount = *(uint32_t*)(immigComp + 0x60);

    // Get sub-entity from ImmigrationComponent+0x10 (InternalBasePtr field)
    void* subEntity = (void*)*(uint64_t*)(immigComp + 0x10);

    // Find BuildStateComponent on sub-entity
    uint64_t buildStateComp = 0;
    fn_FindComponent_BuildState(subEntity, &buildStateComp);

    // Check build state: byte at +0x52, if negative use +0x50, compare to 0x11.
    // Guard the sub-entity's BuildState too (unbuilt/absent nomad camp in SA).
    bool buildComplete = false;
    if (*(uint64_t*)&buildStateComp != 0) {
        char state = *(char*)(*(uint64_t*)&buildStateComp + 0x52);
        if (state < 0) {
            state = *(char*)(*(uint64_t*)&buildStateComp + 0x50);
        }
        buildComplete = (state == 0x11);
    }

    // Enable/disable buttons at +0x40 and +0x48 via vtable[8] (SetEnabled)
    void* buttonAllow = (void*)*(uint64_t*)(s + 0x40);
    reinterpret_cast<UI::Element*>(buttonAllow)->Enable(buildComplete);  // B.3.6 bridge: Element slot 8 Enable

    void* buttonDeny = (void*)*(uint64_t*)(s + 0x48);
    reinterpret_cast<UI::Element*>(buttonDeny)->Enable(buildComplete);  // B.3.6 bridge: Element slot 8 Enable

    // Show/hide text elements based on build state
    // If build complete: show textNomad (+0x38), hide textNone (+0x30)
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x38))->SetFlagsOnOff(
        (uint32_t)!buildComplete, (uint32_t)buildComplete);  // B.3.6 bridge
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x30))->SetFlagsOnOff(
        (uint32_t)buildComplete, (uint32_t)!buildComplete);  // B.3.6 bridge

    // Format immigration count as "%i" and set on textNomad parameter 0
    System::String countStr = System::String::Format("%i", (uint64_t)immigCount);
    reinterpret_cast<UI::TextElement*>(*(uint64_t*)(s + 0x38))->SetParameter(0, countStr);  // B.3.6 bridge

    // Copy world name from worldSingleton+0x28, set as parameter 1
    void* worldSingleton = *g_pGameWorldSingleton;
    System::String worldNameStr(
        *reinterpret_cast<const System::String*>((uintptr_t)worldSingleton + 0x28));  // B.3.1 bridge
    reinterpret_cast<UI::TextElement*>(*(uint64_t*)(s + 0x38))->SetParameter(1, worldNameStr);  // B.3.6 bridge
}

// ============================================================
// ImmigrationUI::Slot9 (vtable +0x48) — Button handler
// ============================================================
// Handles Allow (id=39000) and Deny (id=0x9859) button clicks.
// Gets ImmigrationComponent, finds its sub-entity's BuildState,
// then calls a StateMachine vtable method with mode 7 (allow)
// or 8 (deny) to accept/reject nomads.
//
// Original: 0x14006B300.

// [9] 0x14006b300
void ImmigrationUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t msg, int32_t id,
                                   int32_t /*param5*/, int32_t /*param6*/, int32_t /*e*/, void* /*userData*/) {
    if (!immigrationui_sa::Ready()) return;
    immigrationui_sa::Trace("Slot9", this);
    uintptr_t s = (uintptr_t)this;

    if (msg != 0x12) return;

    // Determine action mode: 7 = allow, 8 = deny
    int mode;
    if (id == 39000) {
        mode = 7;
    } else if (id == 0x9859) {
        mode = 8;
    } else {
        return;
    }

    // Get entity from +0x28, check IsA ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* checkedEntity = nullptr;
    if (::Object::IsDerivedFromNullSafe(entity, entityTypeId)) {
        checkedEntity = entity;
    }

    // Find ImmigrationComponent
    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    uint64_t immigComp = 0;
    fn_FindComponent_Immigration((void*)entityBasePtr, &immigComp);
    if (immigComp == 0) return;   // no component bound (standalone safety)

    // Get sub-entity from ImmigrationComponent+0x10

    // Find BuildState component on sub-entity at +0x10
    uint64_t buildState = 0;
    fn_FindComponent_BuildState(*(void**)(immigComp + 0x10), &buildState);

    // Read the BuildState pointer value
    uintptr_t bsPtr = *(uintptr_t*)&buildState;
    if (bsPtr == 0) return;

    // Get StateMachine sub-object at BuildState+0x48
    void* smObj = *(void**)(bsPtr + 0x48);
    if (!smObj) return;
    void** smVtable = *(void***)smObj;

    // Call vtable[2] (offset 0x10): RequestAction(self, &controllerSelf, currentState, mode)
    typedef void (*fn_RequestAction_t)(void* self, void* controllerSelf, int currentState, int mode);
    int currentState = (int)*(char*)(bsPtr + 0x51);
    ((fn_RequestAction_t)smVtable[2])(smObj, (void*)(bsPtr + 0x48), currentState, mode);
}
