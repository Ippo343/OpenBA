// engine/src/ui/controlui.cpp
// Clean reimplementation of ControlUI panel methods.
//
// ControlUI (0x40 bytes): Building enable/disable toggle panel.
// Layout: +0x30 = checkbox element (InternalBasePtr)
//         +0x38 = sub-component (InternalBasePtr) found via component array search

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // Object::ResolveTypeIdByNameLazy
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "address_registry.h"
#include "resolve.h"
#include <cstdio>
#include <cstdlib>
#include "Component/Component.h"
#include "Entity/ComponentEntity.h"   // X::TypeId() (ENDGAME B.3.9)
#include "Dialog/ControlUIConfig.h"   // X::TypeId() (ENDGAME B.3.9)
#include "runtime/types/ui/ButtonElement.h"
#include "runtime/types/ui/CheckElement.h"

// ---- Element type ids: UI::ButtonElement and UI::CheckElement ----------------
// Retail reads these through exe .rdata exrefs 0x1401514f8 (ButtonElement, tested
// by Slot4 before storing the element at +0x30) and 0x1401516a8 (CheckElement,
// tested by Slot4/Slot7 before touching the checked byte at elem+0xd8). In
// standalone both live in the element-type-id table in
// standalone/reg_element_typeids.cpp, which is GATED on BANISHED_LIVE_ELEMENT_TIDS
// and therefore off by default — so g_TypeId_ButtonElement and
// g_TypeId_CheckboxElement are both normally null.
//
// MEASURED consequence before this fix (BANISHED_CONTROLUI_TRACE, demo pasture):
// all three pasture ControlUIs reported `elem(+0x30)=0` with a perfectly good
// `target(+0x38)`, because IsDerivedFrom(elem, 0) is always false. That null +0x30
// silently disabled everything this controller does — SetElementId(50000) was
// skipped (so Slot9 could never match a click), the +0xd8 checked byte was never
// written, and Slot7 returned at the `if (!elem)` guard before calling Enable().
// The four button predicates were correct all along; nothing ever applied them,
// which is why work/split/empty all sat at the element ctor default (enabled,
// unchecked) and read as "highlighted".
//
// ENDGAME B.3.9: UI::ButtonElement / UI::CheckElement now have a ported TypeId() (a
// class static, always populated), so the private-copy-vs-gated-global split above
// collapses to the static: both arms already answered the same id.
static uint32_t ControlUI_ButtonElementTypeId() {
    return UI::ButtonElement::TypeId();
}

static uint32_t ControlUI_CheckElementTypeId() {
    return UI::CheckElement::TypeId();
}

namespace controlui_sa {

// Report Slot4's INPUTS as well as its outputs. The first version of this printed only
// +0x30/+0x38 — which are Slot4's OUTPUTS — so when the type-id guard rejected every call
// before writing them, the trace read as "the framework never bound +0x20/+0x28" and sent
// the investigation at the binding path, which was working fine all along. Always print
// the thing the guard actually tests.
static void Trace(const char* what, void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* cfg = self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x10)) : nullptr;
    uint32_t wantTid = ControlUIConfig::TypeId();
    std::fprintf(stderr,
        "[ControlUI] %s self=%p | IN: cfgTid=%s(%u) cfg(+0x10)=%p elem(+0x20)=%p entity(+0x28)=%p"
        " | OUT: elem(+0x30)=%p target(+0x38)=%p cmd=%d\n",
        what, self,
        wantTid ? "ok" : "ZERO", wantTid, cfg,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x20)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x28)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x30)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x38)) : nullptr,
        cfg ? *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(cfg) + 0x20) : -1);
    std::fflush(stderr);
}


// idx7 (+0x38): per-frame enable / checked sync.
//
// Slot7 had NO trace of its own, which is why "is slot 7 even ticked for this
// panel?" stayed open through two investigations. It is: UI::UserInterface::Update
// (Runtime 0x1801213c0) -> DialogInstance::ComputeLayout (0x180113d90) -> vtable
// idx6 OnUpdateLayout (0x1801153a0) walks +0x78 and dispatches controller idx7
// every frame, with no dirty flag. Report the OUTCOME — the element state read back
// AFTER the writes — rather than the intent, and print in every arm so that silence
// can never read as success. Rate-limited because this is a per-frame path.
// The per-frame trace that used to follow the Slot7 body in the W_Slot7 wrapper.
void TraceSlot7(void* self) {
    static int s_left = 12;
    if (s_left <= 0) return;
    --s_left;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* cfg = reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x10));
    void* elem = reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x30));
    int cmd = cfg ? *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(cfg) + 0x20) : -1;
    if (!elem) {
        // The pre-fix state: correct predicates, nothing to apply them to.
        std::fprintf(stderr, "[ControlUI] Slot7 cmd=%d elem(+0x30)=NULL"
                     " -> no Enable, no checked write\n", cmd);
    } else {
        // elem+0x34 bit 0x800000 = disabled (UI::Element::Enable, Runtime 0x180116910).
        uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(elem) + 0x34);
        // +0xd8 is the checked byte of a CheckElement ONLY (CheckElement::IsChecked,
        // Runtime 0x180048c80; CheckDescription::CreateInstance allocates 0xe0). The
        // first version of this trace read it unconditionally and printed checked=50
        // for the two ButtonElement buttons — a read PAST the end of a plain button,
        // i.e. heap bytes reported as game state. Only the cmd=2 work button is a
        // CheckElement here, so gate the read on the same type id Slot7 gates its
        // write on.
        if (fn_IsDerivedFrom(elem, ControlUI_CheckElementTypeId())) {
            unsigned checked =
                *reinterpret_cast<unsigned char*>(reinterpret_cast<uintptr_t>(elem) + 0xd8);
            std::fprintf(stderr, "[ControlUI] Slot7 cmd=%d elem=%p disabled=%d checked=%u\n",
                         cmd, elem, (flags & 0x800000u) ? 1 : 0, checked);
        } else {
            std::fprintf(stderr, "[ControlUI] Slot7 cmd=%d elem=%p disabled=%d"
                         " checked=n/a (ButtonElement, no +0xd8)\n",
                         cmd, elem, (flags & 0x800000u) ? 1 : 0);
        }
    }
    std::fflush(stderr);
}

// idx8 (+0x40) is DELIBERATELY NOT OVERRIDDEN, matching retail.
// An earlier version of this file installed a composite here that re-ran Slot4 every
// frame until the target resolved. Its justifying comment — "idx8 is the only slot the
// framework ticks per frame" — was FALSE, and the retry was load-bearing for nothing:
//   * UI::DialogInstance::OnUpdateLayout (Runtime 0x1801153a0) walks this+0x78 and
//     dispatches vtable +0x38 = idx7, where W_Slot7 already sits. That is the per-frame
//     sync path.
//   * UI::DialogInstance::OnUpdate (Runtime 0x180115400) dispatches +0x40 = idx8 with a
//     `const Time&` that a void(*)(void*) composite silently dropped. Retail leaves idx8
//     inherited (a no-op) precisely because it has no per-controller work to do.
// Slot4 is called ONCE, by UI::DialogInstance::AddController (Runtime 0x1801133b0), which
// writes +0x10 config / +0x18 dialog / +0x20 element / +0x28 entity in four consecutive
// stores and then dispatches idx4. With ControlUIConfig's type id registered, that single
// call now succeeds, so there is nothing left for a retry to heal.

}  // namespace controlui_sa

// Slot4: Find checkbox element, find sub-component by type, set initial state.
// [4] 0x14005f9b0
void ControlUI::OnCreated() {
    uintptr_t s = (uintptr_t)this;

    // Check datasource component type
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = ControlUIConfig::TypeId();
    void* component = nullptr;
    if (fn_IsDerivedFrom(datasource, controlTypeId)) {
        component = datasource;
    }
    // SA guard: retail reads cfg+0x20 / cfg+0x24 unconditionally. If the config is
    // unbound or not a ControlUIConfig there is no command and no target type, so
    // there is nothing this button could drive.
    if (!component) return;

    // Check dialog element type, store at +0x30
    uint32_t elemBaseTypeId = ControlUI_ButtonElementTypeId();
    void* dialogElem = (void*)*(uint64_t*)(s + 0x20);
    void* checkedElem = nullptr;
    if (fn_IsDerivedFrom(dialogElem, elemBaseTypeId)) {
        checkedElem = dialogElem;
    }
    uint64_t stored = 0;
    new (&stored) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checkedElem));   // B.3.3 bridge
    *(uint64_t*)(s + 0x30) = stored;

    // Set element ID 50000 (0xC350). SA guard: retail assumes the .rsc always binds
    // an element here; a dialog that lacks it would take SetElementId(null).
    if (*(uint64_t*)(s + 0x30))
        reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x30))->SetElementId(50000);  // B.3.6 bridge

    // Get entity, find sub-component via component type array search
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    int subTypeKey = *(int*)((uintptr_t)component + 0x24);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* entityChecked = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        entityChecked = entity;
    }
    void* subComp = FindComponentByTypeId(entityChecked, subTypeKey);
    *(uint64_t*)(s + 0x38) = (uint64_t)subComp;
    // SA guard: the checkbox sync below dispatches subComp->vtable[13]. A selected
    // entity that simply lacks the target component is normal (the same .rsc button
    // template is reused across building types), so bail rather than fault.
    if (!subComp) return;

    // If element at +0x30 IsA CheckboxElement, set initial checkbox state.
    //
    // This guard is LOAD-BEARING, not defensive: +0xd8 is one byte PAST the end of a
    // plain ButtonElement. ButtonDescription::CreateInstance (Runtime 0x180111510)
    // allocates 0xd8; CheckDescription::CreateInstance (0x1801119c0) allocates 0xe0
    // precisely so the checked byte fits. On a pasture, empty(0)/split(1) are
    // ButtonElements and only work(2) is a CheckElement — relaxing this test to make a
    // highlight appear would be a 1-byte heap overrun on two of the three buttons.
    // It is also why Pasture_CanCommand (0x140127dd0) never sets bit 1 for cmd 0/1:
    // those elements have no checked byte to write.
    //
    // FAITHFULNESS DEVIATION (deliberate, documented rather than silent): retail guards
    // this with Object::Base::IsA (exe slot 0x140151410, 0x1800491c0), an EXACT-type
    // test (`return param_1 == GetTypeId();`), whereas we use fn_IsDerivedFrom. Identical
    // for the pasture — work is exactly CheckElement, empty/split are not derived from it
    // — but the two differ for any CheckElement SUBCLASS, and Runtime does ship
    // RadioElement. Revisit if a .rsc ever binds a check subclass here.
    uint32_t checkboxTypeId = ControlUI_CheckElementTypeId();
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        // vtable[13] (offset 0x68): query enabled state with key from component+0x20
        uint32_t value = reinterpret_cast<Component*>(subCompPtr)->CommandMask(*(int*)((uintptr_t)component + 0x20));  // slot 13; B.3.8 bridge

        // Validate element again for safe access
        void* checkboxElem = nullptr;
        if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
            checkboxElem = elemAt30;
        }
        // Set checkbox state: bit 1 of return value
        *(char*)((uintptr_t)checkboxElem + 0xd8) = (char)((value >> 1) & 1);
    }
    controlui_sa::Trace("Slot4", this);
}

// Slot7: Update checkbox state from component, set enabled/disabled.
// [7] 0x14005f870
void ControlUI::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    // Get datasource component
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = ControlUIConfig::TypeId();
    void* component = nullptr;
    if (fn_IsDerivedFrom(datasource, controlTypeId)) {
        component = datasource;
    }
    // SA guards, same reasoning as Slot4: no config => no command key; no target
    // component => nothing to query. Both are dispatched through unconditionally
    // in retail, which can rely on the .rsc binding always being complete.
    if (!component) return;
    if (!*(uint64_t*)(s + 0x38)) return;

    // Check if element IsA CheckboxElement
    uint32_t checkboxTypeId = ControlUI_CheckElementTypeId();
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        // Query current state from sub-component vtable[13]
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        uint32_t value = reinterpret_cast<Component*>(subCompPtr)->CommandMask(*(int*)((uintptr_t)component + 0x20));  // slot 13; B.3.8 bridge
        char newState = (char)((value >> 1) & 1);

        // Only update if state changed
        void* checkboxElem = nullptr;
        if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
            checkboxElem = elemAt30;
        }
        if (newState != *(char*)((uintptr_t)checkboxElem + 0xd8)) {
            void* checkboxElem2 = nullptr;
            if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
                checkboxElem2 = elemAt30;
            }
            *(char*)((uintptr_t)checkboxElem2 + 0xd8) = newState;
        }
    }

    // Set enabled/disabled via vtable[8] based on component's enabled state
    void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
    uint32_t enabledBits = reinterpret_cast<Component*>(subCompPtr)->CommandMask(*(int*)((uintptr_t)component + 0x20));  // slot 13; B.3.8 bridge
    bool enabled = (enabledBits & 1) != 0;

    void* elem = (void*)*(uint64_t*)(s + 0x30);
    if (!elem) return;
    reinterpret_cast<UI::Element*>(elem)->Enable(enabled);  // B.3.6 bridge: Element slot 8 Enable
    controlui_sa::TraceSlot7(this);
}

// Slot9: On checkbox click (event 0x12, id 50000), toggle component state.
// param2 is a POINTER — must deref to compare against stored element ptr.
// Original asm: CMP [RDI],RAX where RDI=param2, RAX=*(self+0x30)
// [9] 0x14005f7f0
void ControlUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t param3, int32_t param4,
                               int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    uint64_t* param2 = reinterpret_cast<uint64_t*>(element);
    if (param3 == 0x12 && param4 == 50000) controlui_sa::Trace("Slot9(click)", this);
    uintptr_t s = (uintptr_t)this;

    if (param3 == 0x12 && param4 == 50000) {
        // CheckPtr on stored element before comparison
        // Verify: *param2 must match stored element pointer at +0x30
        if (*param2 == *(uint64_t*)(s + 0x30)) {
            // Get datasource component
            void* datasource = (void*)*(uint64_t*)(s + 0x10);
            uint32_t controlTypeId = ControlUIConfig::TypeId();
            void* component = nullptr;
            if (fn_IsDerivedFrom(datasource, controlTypeId)) {
                component = datasource;
            }
            if (!component) return;   // SA guard: no config => no command to send

            // CheckPtr on sub-component before vtable call
            // Call vtable[14] (offset 0x70) on sub-component to toggle state
            void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
            if (!subCompPtr) return;  // SA guard: entity lacks the target component
            reinterpret_cast<Component*>(subCompPtr)->Command(*(int*)((uintptr_t)component + 0x20));  // slot 14; B.3.8 bridge
        }
    }
}

// ============================================================
// STANDALONE promotion — real ControlUI controller vtable
// ============================================================
// ControlUI was a UI_DIALOG_STUB (all-no-op), so every port above was dead code.
// These are the generic component-command buttons: on a pasture they are
// work / split / empty, all sharing element id 50000 and distinguished ONLY by
// the command int in ControlUIConfig+0x20 (work = 2, split = 1, empty = 0).
// With the stub in place Slot4 never stamped the element id and never resolved
// the target component, Slot7 never synced enable/checked state (which is why
// the buttons still LOOKED live — they kept their .rsc appearance), and Slot9
// landed on StubNop. The PastureComponent side (slots 13/14 -> Pasture_CanCommand /
// Pasture_Command) has been ported and live the whole time.
//
// Retail vtable @ 0x14015b380:
//   idx4 (+0x20) 0x14005f9b0 — setup   (Ghidra "ControlUI_Serialize"  — mislabeled)
//   idx7 (+0x38) 0x14005f870 — per-frame (Ghidra "ControlUI_OnCreate" — mislabeled)
//   idx9 (+0x48) 0x14005f7f0 — click   (Ghidra "ControlUI_Update"    — mislabeled)
// The Ghidra names are wrong on all three; the vtable INDEX is authoritative.
// (Ghidra's "ControlUI_Slot12/13/14/vslot17/vslot18" are ControlUIConfig methods.)
//
// Retail does NOT override idx8, but idx8 is the only slot the framework ticks
// per frame (see the note at pastureui.cpp:425), so — exactly as the PastureUI /
// StorageUI promotions do — idx8 here is a composite that self-heals Slot4 once
// and then drives Slot7.


// [0] 0x140010e20: ~ControlUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ControlUI::DeletingDtor(unsigned flags) {
    this->~ControlUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140010c00: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ControlUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ControlUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ControlUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ControlUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ControlUI());
}

DEFINE_EXE_VTABLE(ControlUI, "_ZTV9ControlUI")
DEFINE_EXE_TYPE(ControlUI, "UI::DialogController")
