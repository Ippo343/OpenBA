// engine/src/ui/controlui.cpp
// Clean reimplementation of ControlUI panel methods.
//
// ControlUI (0x40 bytes): Building enable/disable toggle panel.
// Layout: +0x30 = checkbox element (InternalBasePtr)
//         +0x38 = sub-component (InternalBasePtr) found via component array search

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot4: Find checkbox element, find sub-component by type, set initial state.
void ControlUI_Slot4(ControlUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Check datasource component type
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
    void* component = nullptr;
    if (fn_IsDerivedFrom(datasource, controlTypeId)) {
        component = datasource;
    }
    // SA guard: retail reads cfg+0x20 / cfg+0x24 unconditionally. If the config is
    // unbound or not a ControlUIConfig there is no command and no target type, so
    // there is nothing this button could drive.
    if (!component) return;

    // Check dialog element type, store at +0x30
    uint32_t elemBaseTypeId = g_TypeId_UIElementBase ? *g_TypeId_UIElementBase : 0;
    void* dialogElem = (void*)*(uint64_t*)(s + 0x20);
    void* checkedElem = nullptr;
    if (fn_IsDerivedFrom(dialogElem, elemBaseTypeId)) {
        checkedElem = dialogElem;
    }
    uint64_t stored = 0;
    fn_InternalBasePtr_Ctor(&stored, checkedElem);
    *(uint64_t*)(s + 0x30) = stored;

    // Set element ID 50000 (0xC350). SA guard: retail assumes the .rsc always binds
    // an element here; a dialog that lacks it would take SetElementId(null).
    if (*(uint64_t*)(s + 0x30))
        fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 50000);

    // Get entity, find sub-component via component type array search
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    int subTypeKey = *(int*)((uintptr_t)component + 0x24);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
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

    // If element at +0x30 IsA CheckboxElement, set initial checkbox state
    uint32_t checkboxTypeId = g_TypeId_CheckboxElement ? *g_TypeId_CheckboxElement : 0;
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        void** vtable = *(void***)subCompPtr;
        // vtable[13] (offset 0x68): query enabled state with key from component+0x20
        typedef uint32_t (*fn_QueryState_t)(void*, int);
        uint32_t value = ((fn_QueryState_t)vtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));

        // Validate element again for safe access
        void* checkboxElem = nullptr;
        if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
            checkboxElem = elemAt30;
        }
        // Set checkbox state: bit 1 of return value
        *(char*)((uintptr_t)checkboxElem + 0xd8) = (char)((value >> 1) & 1);
    }
}

// Slot7: Update checkbox state from component, set enabled/disabled.
void ControlUI_Slot7(ControlUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get datasource component
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
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
    uint32_t checkboxTypeId = g_TypeId_CheckboxElement ? *g_TypeId_CheckboxElement : 0;
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        // Query current state from sub-component vtable[13]
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        void** subVtable = *(void***)subCompPtr;
        typedef uint32_t (*fn_QueryState_t)(void*, int);
        uint32_t value = ((fn_QueryState_t)subVtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
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
    void** subVtable = *(void***)subCompPtr;
    typedef uint32_t (*fn_QueryState_t)(void*, int);
    uint32_t enabledBits = ((fn_QueryState_t)subVtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
    bool enabled = (enabledBits & 1) != 0;

    void* elem = (void*)*(uint64_t*)(s + 0x30);
    if (!elem) return;
    void** elemVtable = *(void***)elem;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)elemVtable[8])(elem, enabled);
}

// Slot9: On checkbox click (event 0x12, id 50000), toggle component state.
// param2 is a POINTER — must deref to compare against stored element ptr.
// Original asm: CMP [RDI],RAX where RDI=param2, RAX=*(self+0x30)
void ControlUI_Slot9(ControlUI* self, uint64_t* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x12 && param4 == 50000) {
        // CheckPtr on stored element before comparison
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
        // Verify: *param2 must match stored element pointer at +0x30
        if (*param2 == *(uint64_t*)(s + 0x30)) {
            // Get datasource component
            void* datasource = (void*)*(uint64_t*)(s + 0x10);
            uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
            void* component = nullptr;
            if (fn_IsDerivedFrom(datasource, controlTypeId)) {
                component = datasource;
            }
            if (!component) return;   // SA guard: no config => no command to send

            // CheckPtr on sub-component before vtable call
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
            // Call vtable[14] (offset 0x70) on sub-component to toggle state
            void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
            if (!subCompPtr) return;  // SA guard: entity lacks the target component
            void** subVtable = *(void***)subCompPtr;
            typedef void (*fn_ToggleState_t)(void*, int);
            ((fn_ToggleState_t)subVtable[14])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
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

#include "Object/factory_registration.h"
#include "address_registry.h"
#include "resolve.h"
#include <cstdio>
#include <cstdlib>

namespace controlui_sa {

constexpr uintptr_t kVtable = 0x14015b380;
constexpr int       kSlots  = 48;

static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("ControlUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Report Slot4's INPUTS as well as its outputs. The first version of this printed only
// +0x30/+0x38 — which are Slot4's OUTPUTS — so when the type-id guard rejected every call
// before writing them, the trace read as "the framework never bound +0x20/+0x28" and sent
// the investigation at the binding path, which was working fine all along. Always print
// the thing the guard actually tests.
static void Trace(const char* what, void* self) {
    if (!std::getenv("BANISHED_CONTROLUI_TRACE")) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* cfg = self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x10)) : nullptr;
    uint32_t wantTid = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
    std::fprintf(stderr,
        "[ControlUI] %s self=%p | IN: cfgTid=%s(%u) cfg(+0x10)=%p elem(+0x20)=%p entity(+0x28)=%p"
        " | OUT: elem(+0x30)=%p target(+0x38)=%p cmd=%d\n",
        what, self,
        g_TypeId_ControlComponent ? (wantTid ? "ok" : "ZERO") : "NULLPTR", wantTid, cfg,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x20)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x28)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x30)) : nullptr,
        self ? reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x38)) : nullptr,
        cfg ? *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(cfg) + 0x20) : -1);
    std::fflush(stderr);
}

// idx4 (+0x20): setup — stamp element id 50000, resolve the target component.
void W_Slot4(void* self) {
    if (!self) return;
    ControlUI_Slot4(reinterpret_cast<ControlUI*>(self));
    Trace("Slot4", self);
}

// idx7 (+0x38): per-frame enable / checked sync.
void W_Slot7(void* self) {
    if (!self) return;
    ControlUI_Slot7(reinterpret_cast<ControlUI*>(self));
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

// idx9 (+0x48): element events — the actual click.
void W_Slot9(void* self, uint64_t* p2, int p3, int p4) {
    if (!self) return;
    if (p3 == 0x12 && p4 == 50000) Trace("Slot9(click)", self);
    ControlUI_Slot9(reinterpret_cast<ControlUI*>(self), p2, p3, p4);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Slot4);   // OnCreated      (AddController)
        g_vt[7] = reinterpret_cast<void*>(&W_Slot7);   // OnUpdateLayout (per frame)
        g_vt[9] = reinterpret_cast<void*>(&W_Slot9);   // OnElementEvent (clicks)
        // idx8 left inherited/no-op on purpose — see the note above W_Slot9.
    }
} g_init;

// ControlUI object size 0x40 (+0x30 element IBP, +0x38 target component).
void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x40); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x40), 8);
    if (!p) return nullptr;
    Zero(p);
    *reinterpret_cast<void**>(p) = g_vt;
    return p;
}
void* InPlace(void* m) {
    if (!m) return nullptr;
    Zero(m);
    *reinterpret_cast<void**>(m) = g_vt;
    return m;
}

}  // namespace controlui_sa

extern "C" void RegisterControlUIVtable() {
    RegisterExeAddr(controlui_sa::kVtable,
                    reinterpret_cast<uintptr_t>(controlui_sa::g_vt));
}

REGISTER_TYPE(ControlUI, "UI::DialogController",
              &controlui_sa::Factory, &controlui_sa::InPlace);
