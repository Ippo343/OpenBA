// code/Game/Dialog/ui_dialog_stub_factories.cpp
//
// Dormant-safe standalone factories for the UI dialog family — the last family
// of the factory cascade (notes/FACTORY_CASCADE_145_BRIEF.md).
//
// These 58 dialog/controller/config types are all DORMANT in the current
// standalone path (menu + worldgen never instantiate them; verified 0
// NullFactory hits with BANISHED_GAME_PRELOAD). Each was a REGISTER_NULL_FACTORY
// stub in factory_registrations_bulk.cpp — a latent IsDerivedFrom(NULL) landmine
// that crashes only if the type is ever loaded/instantiated by name.
//
// Strategy (same dormant-safe policy as code/Game/Game/tornado.cpp and
// code/Game/Component/frustumoutlineelement.cpp): replace the NullFactory with a
// real factory that
//   - MemAlloc's the object at its TRUE exe size (decompiled from each
//     Factory_<Type> / <Type>_Factory in Application-x64.exe),
//   - zeroes it,
//   - installs a minimal per-type standalone vtable (slot0 ScalarDtor,
//     slot2 GetTypeId returning the registered type id, all other slots a nop).
// No base ctor (DialogInstance / DialogController / DialogControllerConfig /
// External) is run — every base field is left zero, which is correct because the
// object is never dispatched while dormant. The non-null return removes the
// landmine; GetTypeId reports the right id for any IsDerivedFrom query.
//
// NOTE: the exe subclass vtable (vtable_<Type>, full of exe->Runtime thunks that
// don't resolve in standalone) is intentionally NOT installed, so no
// RegisterExeAddr / address_registry edit is needed here. When the HUD bring-up
// (task #10) makes a specific dialog reachable, promote it out of this file into
// a full per-class standalone block wiring its reimplemented slot methods (see
// code/Game/Dialog/messagedialog.cpp for that pattern).
//
// The method bodies for most of these dialogs already exist in their own
// code/Game/Dialog/<type>.cpp; this file only supplies the standalone
// construction/registration the exe ctors used to provide.


#include "Object/factory_registration.h"
#include "options.h"
#include "engine_funcs.h"
#include "resolve.h"      // ResolveExport — HelpUI's Runtime BaseTypeInfo type-id heal
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "dialog_controller_vtable.h"

namespace {

// Generic no-op for unused vtable slots. Dormant dialogs are never dispatched,
// so any slot beyond Dtor (+0x00) / GetTypeId (+0x10) is unreached. Variadic so
// a stray call with any argument list lands here harmlessly.
void StubNop(void*, ...) {}

// The widest dialog vtable observed is well under this (DialogInstance's
// MessageDialog used 14 slots); 48 is safely generous. Out-of-range slots are
// never read for dormant types.
constexpr int kStubVtableSlots = 48;

// Fills a vtable array with StubNop, then overrides the two slots that matter.
struct StubVtableInit {
    StubVtableInit(void** vt, void* dtor, void* getTypeId) {
        for (int i = 0; i < kStubVtableSlots; ++i)
            vt[i] = reinterpret_cast<void*>(&StubNop);
        vt[0] = dtor;       // +0x00 ScalarDtor
        vt[2] = getTypeId;  // +0x10 GetTypeId
    }
};

}  // namespace

// One dormant-safe factory + per-type vtable per UI dialog.
#define UI_DIALOG_STUB(Name, Parent, Size)                                       \
    namespace uistub_##Name {                                                    \
        static void* g_vt[kStubVtableSlots];                                     \
        uint32_t GetTypeId(void*) {                                              \
            static uint32_t c = 0;                                               \
            if (!c) c = ::Object::GetTypeIdByName(#Name);                        \
            return c;                                                            \
        }                                                                        \
        void* Dtor(void* self, uint32_t flags) {                                 \
            if (!self) return self;                                              \
            *reinterpret_cast<void**>(self) = g_vt;                              \
            if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);         \
            return self;                                                         \
        }                                                                        \
        StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor),              \
                              reinterpret_cast<void*>(&GetTypeId));              \
        void Zero(void* p) {                                                     \
            for (size_t o = 0; o < static_cast<size_t>(Size); o += 8)            \
                *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;     \
        }                                                                        \
        void* Factory() {                                                        \
            if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;           \
            void* p = fn_MemAlloc(g_pMemAllocSingleton,                          \
                                  static_cast<uint64_t>(Size), 8);               \
            if (!p) return nullptr;                                              \
            Zero(p);                                                             \
            *reinterpret_cast<void**>(p) = g_vt;                                 \
            return p;                                                            \
        }                                                                        \
        void* InPlace(void* m) {                                                 \
            if (!m) return nullptr;                                              \
            Zero(m);                                                            \
            *reinterpret_cast<void**>(m) = g_vt;                                 \
            return m;                                                            \
        }                                                                        \
    }                                                                            \
    REGISTER_TYPE(Name, Parent, &uistub_##Name::Factory, &uistub_##Name::InPlace)

// ---- BuildRequirementUI: PROMOTED to the real reimpl (2026-06-28) ------------
// This dialog is constructed + its Slot4(Update +0x20) dispatched on hover of a
// build-tool button (verified live: valid dlgInst@+0x18 / datasource@+0x28). The
// generic stub left Slot4 a no-op, so the build tooltip showed "????????" for the
// name and "x x x" for the cost. Wiring Slot4 to the real BuildRequirementUI_Slot4
// composes the title (ToolbarDescription+0x38 name key), the icon, and the
// cost labels. Construction stays the stub alloc+zero+vtable; the engine fills the
// base fields afterward.
struct BuildRequirementUI;  // opaque; the reimpl casts internally
extern void BuildRequirementUI_Slot4(BuildRequirementUI* self);
namespace uistub_BuildRequirementUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("BuildRequirementUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // Lazy-fill the exe-.rdata type-id globals BuildRequirementUI_Slot4 dereferences.
    // In standalone these resolve from raw exe .rdata addresses → null, so the real
    // reimpl's first `*g_TypeId_ComponentDescription_FC` would fault. Fill them from the live
    // Factory tree (post-Engine::Startup), like ToolbarStandalone_Init. Idempotent.
    // NB: g_TypeId_ResourceComponent's engine_funcs label is misleading — the id at
    // .rdata 0x1401c7040 is actually "BuildDescription"; and Object::Internal is
    // registered under the simple name "Internal".
    void EnsureTypeIds() {
        static uint32_t s_build=0, s_entity=0, s_internal=0, s_buildDesc=0, s_rawMat=0;
        if (!g_TypeId_ComponentDescription_FC)        { s_build=::Object::ResolveTypeIdByNameLazy("BuildComponent");        if(s_build) g_TypeId_ComponentDescription_FC=&s_build; }
        if (!g_TypeId_ComponentEntity)       { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");      if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_ResourceComponent)     { s_buildDesc=::Object::ResolveTypeIdByNameLazy("BuildDescription");  if(s_buildDesc) g_TypeId_ResourceComponent=&s_buildDesc; }
        if (!g_TypeId_RawMaterialDescription){ s_rawMat=::Object::ResolveTypeIdByNameLazy("RawMaterialDescription"); if(s_rawMat) g_TypeId_RawMaterialDescription=&s_rawMat; }
        if (!g_TypeId_Element)        { s_internal=::Object::ResolveTypeIdByNameLazy("Internal");           if(s_internal) g_TypeId_Element=&s_internal; }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        // The real reimpl unconditionally dereferences these globals; if any failed
        // to resolve, skip rather than fault.
        if (!self || !g_TypeId_ComponentDescription_FC || !g_TypeId_ComponentEntity
            || !g_TypeId_Element || !g_TypeId_ToolbarDescription_FC
            || !g_TypeId_ResourceComponent || !g_TypeId_RawMaterialDescription) return;
        BuildRequirementUI_Slot4(reinterpret_cast<BuildRequirementUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's BuildRequirementUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "BuildRequirementUI", &StubNop);
    struct Slot4Wire { Slot4Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); } } g_s4;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x60;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x60,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(BuildRequirementUI, "UI::DialogController",
              &uistub_BuildRequirementUI::Factory, &uistub_BuildRequirementUI::InPlace);

// ---- HotKeyToolTipUI: PROMOTED to the real reimpl (2026-06-28) ---------------
// This controller is bound to the "textHotKey"/"textToolTip" TextElements of
// every toolbar button. Its Slot4 (Serialize, +0x20) reads the button's
// auto-assigned shortcut id (element+0xC0), resolves it through Input::KeyMap +
// the keyNames StringTable, and fills the hotkey text. The generic stub left
// Slot4 a no-op, so the buttons showed the .rsc placeholder "[]" (empty
// brackets). Wiring Slot4 to the real HotKeyToolTipUI_Slot4 restores the key
// display. Construction stays the stub alloc+zero+vtable; the engine fills the
// base fields (dialog +0x18, datasource +0x28) afterward. Same shape as the
// BuildRequirementUI promotion above.
namespace Game { void HotKeyToolTipUI_Slot4(void* self); }
namespace uistub_HotKeyToolTipUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("HotKeyToolTipUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // The reimpl unconditionally dereferences g_TypeId_Element; in
    // standalone it resolves from a raw exe .rdata addr -> null. Lazy-fill it
    // from the live Factory tree (post-Engine::Startup). NB: Object::Internal is
    // registered under the simple name "Internal". Idempotent.
    void EnsureTypeIds() {
        static uint32_t s_internal=0;
        if (!g_TypeId_Element) {
            s_internal=::Object::ResolveTypeIdByNameLazy("Internal");
            if (s_internal) g_TypeId_Element=&s_internal;
        }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !g_TypeId_Element) return;
        Game::HotKeyToolTipUI_Slot4(self);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's HotKeyToolTipUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "HotKeyToolTipUI", &StubNop);
    struct Slot4Wire { Slot4Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); } } g_s4;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x30;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x30,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(HotKeyToolTipUI, "UI::DialogController",
              &uistub_HotKeyToolTipUI::Factory, &uistub_HotKeyToolTipUI::InPlace);

// ---- AttendanceUI: PROMOTED to the real reimpl (2026-08-15) ------------------
// Symptom: the school panel read "students" with no number in front of it.
//
// Both bodies have been ported in code/Game/Dialog/attendanceui.cpp since that
// file was written, and both were wired into NOTHING — the generic stub's 48
// no-op slots. This is the "stub sweep is a WIRING job" shape again
// (project_next_stub_controller_sweep): a ported body is not a reached body.
//
// The count is not a label of its own; it is TEXT PARAMETER 0 of the panel's
// ProgressElement ("{0} students" in the .rsc). With idx7 a no-op the parameter
// was never set, so the element rendered its literal remainder — the number was
// not wrong, it was absent, which is exactly what "students" with nothing in
// front of it looks like.
//
// Slot indices verified from the vtable BYTES at 0x14015b070 (11 slots wide; the
// RTTI name string "Attendan..." starts immediately after idx10):
//   idx4 (+0x20) 0x14005dd40 Slot4 — find the ProgressElement, set its MAXIMUM
//                                    from AttendanceDescription+0x20 (capacity)
//   idx7 (+0x38) 0x14005dc50 Slot7 — per-frame: set current value + text param 0
// Every other slot is one of the shared 0x14003a9xx no-op thunks, so wiring
// these two is full behavioural coverage for this class.
struct AttendanceUI;   // opaque; the reimpl casts internally
struct ConsumeProduceUI;   // opaque; the reimpl casts internally
extern void ConsumeProduceUI_Slot4(ConsumeProduceUI* self);
extern void ConsumeProduceUI_Slot7(ConsumeProduceUI* self);
extern void ConsumeProduceUI_Slot9(ConsumeProduceUI* self, uint64_t p2, int p3, int p4);

struct CemeteryUI;     // opaque; the reimpl casts internally
extern void CemeteryUI_Slot4(CemeteryUI* self);
extern void CemeteryUI_Slot7(CemeteryUI* self);
extern void AttendanceUI_Slot4(AttendanceUI* self);
extern void AttendanceUI_Slot7(AttendanceUI* self);
namespace uistub_AttendanceUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("AttendanceUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // ProgressElement's type-id .rdata slot is null unless BANISHED_LIVE_ELEMENT_TIDS
    // is on, and IsDerivedFrom(elem, 0) is always false — which would leave +0x30 null
    // and turn Slot4's +0x124 write into a null-deref. Same private by-name resolve
    // orchardui.cpp and cropui_promote.cpp use, for the same reason. See
    // reference_gated_element_typeid_disables_controller.
    static uint32_t s_progress = 0;
    void EnsureTypeIds() {
        static uint32_t s_entity=0, s_attDesc=0, s_attComp=0;
        if (!g_TypeId_ProgressElement || !*g_TypeId_ProgressElement) {
            if (!s_progress) s_progress = ::Object::ResolveTypeIdByNameLazy("ProgressElement");
            if (s_progress) g_TypeId_ProgressElement = &s_progress;
        }
        if (!g_TypeId_ComponentEntity) {
            s_entity = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
            if (s_entity) g_TypeId_ComponentEntity = &s_entity;
        }
        if (!g_TypeId_AttendanceDescription || !*g_TypeId_AttendanceDescription) {
            s_attDesc = ::Object::ResolveTypeIdByNameLazy("AttendanceDescription");
            if (s_attDesc) g_TypeId_AttendanceDescription = &s_attDesc;
        }
        // fn_FindComponent_Attendance searches by this one. Its .rdata slot
        // (0x1401c3f40) went unregistered until 2026-08-15 — the 2026-07-19 audit
        // registered the Description 0x38 bytes later and missed the Component — so
        // the finder ran with a null id, matched nothing, and Slot4's faithful
        // `attendComp+0x18` read [0+0x18] the first time anyone clicked a school.
        // reg_economy_typeids.cpp now supplies the storage; this keeps a resolve
        // failure a skip rather than a repeat of that crash.
        if (!g_TypeId_AttendanceComponent || !*g_TypeId_AttendanceComponent) {
            s_attComp = ::Object::ResolveTypeIdByNameLazy("AttendanceComponent");
            if (s_attComp) g_TypeId_AttendanceComponent = &s_attComp;
        }
    }
    // Both reimpls dereference these globals unconditionally, as retail does. Skip
    // rather than fault if any failed to resolve — a standalone-only failure mode
    // (retail's .rdata is always populated), so the guard is here at the boundary
    // rather than inside the faithful bodies.
    bool Ready() {
        return g_TypeId_ProgressElement && *g_TypeId_ProgressElement
            && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
            && g_TypeId_AttendanceDescription && *g_TypeId_AttendanceDescription
            && g_TypeId_AttendanceComponent && *g_TypeId_AttendanceComponent;
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        AttendanceUI_Slot4(reinterpret_cast<AttendanceUI*>(self));
    }
    void Slot7(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        AttendanceUI_Slot7(reinterpret_cast<AttendanceUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's AttendanceUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "AttendanceUI", &StubNop);
    // ⚠ Declared AFTER g_init: StubVtableInit re-fills every slot with StubNop, so a
    // slot written before it runs is erased (project_next_stub_controller_sweep).
    struct SlotWire { SlotWire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                                  g_vt[7]=reinterpret_cast<void*>(&Slot7); } } g_sw;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x38;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x38,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(AttendanceUI, "UI::DialogController",
              &uistub_AttendanceUI::Factory, &uistub_AttendanceUI::InPlace);

// ---- DialogController family ------------------------------------------------
// CemeteryUI: PROMOTED from a bare UI_DIALOG_STUB 2026-08-23. The panel bodies in
// code/Game/Dialog/cemeteryui.cpp were fully ported but wired into NOTHING -- the stub
// vtable left slots 4 and 7 as no-ops, so neither ProgressElement text parameter was
// ever set and the panel rendered a literal "  of   graves" with both numbers blank.
// Same shape as AttendanceUI directly above.
namespace uistub_CemeteryUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) {
        static uint32_t c = 0;
        if (!c) c = ::Object::GetTypeIdByName("CemeteryUI");
        return c;
    }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self;
        *reinterpret_cast<void**>(self) = g_vt;
        if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
        return self;
    }
    static uint32_t s_cemComp = 0, s_tomb = 0;
    void EnsureTypeIds() {
        // Both are .rdata globals the faithful bodies dereference unconditionally.
        // 0x1401c4090 (CemeteryComponent) had no registered storage at all until the
        // town-service wave; the lazy resolve here is the same belt-and-braces the
        // AttendanceUI block carries, so a resolve failure is a skip, not a fault.
        if (!g_TypeId_CemeteryComponent || !*g_TypeId_CemeteryComponent) {
            s_cemComp = ::Object::ResolveTypeIdByNameLazy("CemeteryComponent");
            if (s_cemComp) g_TypeId_CemeteryComponent = &s_cemComp;
        }
        if (!g_TypeId_TombstoneComponent || !*g_TypeId_TombstoneComponent) {
            s_tomb = ::Object::ResolveTypeIdByNameLazy("TombstoneComponent");
            if (s_tomb) g_TypeId_TombstoneComponent = &s_tomb;
        }
    }
    bool Ready() {
        return g_TypeId_ProgressElement && *g_TypeId_ProgressElement
            && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
            && g_TypeId_CemeteryComponent && *g_TypeId_CemeteryComponent
            && g_TypeId_TombstoneComponent && *g_TypeId_TombstoneComponent;
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        CemeteryUI_Slot4(reinterpret_cast<CemeteryUI*>(self));
    }
    void Slot7(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        CemeteryUI_Slot7(reinterpret_cast<CemeteryUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's CemeteryUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "CemeteryUI", &StubNop);
    // Declared AFTER g_init on purpose: StubVtableInit re-fills every slot with StubNop,
    // so a slot written before it runs is erased (project_next_stub_controller_sweep).
    struct SlotWire { SlotWire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                                  g_vt[7]=reinterpret_cast<void*>(&Slot7); } } g_sw;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x38;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x38,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(CemeteryUI, "UI::DialogController",
              &uistub_CemeteryUI::Factory, &uistub_CemeteryUI::InPlace);
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
namespace uistub_ConsumeProduceUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) {
        static uint32_t c = 0;
        if (!c) c = ::Object::GetTypeIdByName("ConsumeProduceUI");
        return c;
    }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self;
        *reinterpret_cast<void**>(self) = g_vt;
        if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
        return self;
    }
    static uint32_t s_cpDesc = 0, s_combo = 0, s_rawMat = 0;
    void EnsureTypeIds() {
        // .rdata globals the faithful bodies dereference unconditionally. Note
        // g_TypeId_ConsumeProduceComponent is MISNAMED: it is bound to 0x1401c7388,
        // which is the ConsumeProduce*Description* id -- and that is correct, because
        // the object it guards is the description at component+0x18. Retail reads the
        // same global at 0x14005f445. Do not "fix" the cast to the component id.
        if (!g_TypeId_ConsumeProduceComponent || !*g_TypeId_ConsumeProduceComponent) {
            s_cpDesc = ::Object::ResolveTypeIdByNameLazy("ConsumeProduceDescription");
            if (s_cpDesc) g_TypeId_ConsumeProduceComponent = &s_cpDesc;
        }
        if (!g_TypeId_ComboElement || !*g_TypeId_ComboElement) {
            s_combo = ::Object::ResolveTypeIdByNameLazy("ComboElement");
            if (s_combo) g_TypeId_ComboElement = &s_combo;
        }
        if (!g_TypeId_RawMaterialDescription || !*g_TypeId_RawMaterialDescription) {
            s_rawMat = ::Object::ResolveTypeIdByNameLazy("RawMaterialDescription");
            if (s_rawMat) g_TypeId_RawMaterialDescription = &s_rawMat;
        }
    }
    bool Ready() {
        return g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
            && g_TypeId_ComboElement && *g_TypeId_ComboElement
            && g_TypeId_ProgressElement && *g_TypeId_ProgressElement
            && g_TypeId_ConsumeProduceComponent && *g_TypeId_ConsumeProduceComponent
            && g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription;
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        ConsumeProduceUI_Slot4(reinterpret_cast<ConsumeProduceUI*>(self));
    }
    void Slot7(void* self) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        ConsumeProduceUI_Slot7(reinterpret_cast<ConsumeProduceUI*>(self));
    }
    void Slot9(void* self, uint64_t p2, int p3, int p4) {
        EnsureTypeIds();
        if (!self || !Ready()) return;
        ConsumeProduceUI_Slot9(reinterpret_cast<ConsumeProduceUI*>(self), p2, p3, p4);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ConsumeProduceUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ConsumeProduceUI", &StubNop);
    // Declared AFTER g_init on purpose: StubVtableInit re-fills every slot with StubNop,
    // so a slot written before it runs is erased (project_next_stub_controller_sweep).
    struct SlotWire { SlotWire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                                  g_vt[7]=reinterpret_cast<void*>(&Slot7);
                                  g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_sw;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x40;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x40,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ConsumeProduceUI, "UI::DialogController",
              &uistub_ConsumeProduceUI::Factory, &uistub_ConsumeProduceUI::InPlace);
// ControlUI: PROMOTED to a real per-class standalone vtable in
// code/Game/Dialog/controlui.cpp (wires Slot4/7/8/9 so the generic
// component-command buttons — pasture work/split/empty, element id 50000,
// distinguished by ControlUIConfig+0x20 — actually reach the target component's
// vtable[14]). Stub registration removed to avoid a duplicate REGISTER_TYPE.
// DebugUI: PROMOTED to a real per-class standalone vtable in
// code/Game/Dialog/debugui.cpp (wires Slot4/7/8/9 so the dev debug/cheat panel
// links to live game data instead of rendering blank). Stub registration removed
// to avoid a duplicate REGISTER_TYPE(DebugUI).
// CitizenUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/citizenui.cpp
// (wires Slot4/6/7/9 so the clicked-citizen panel shows name/age/profession/hearts).
// DestroyUI: PROMOTED to a real per-class standalone vtable in
// code/Game/Dialog/destroyui.cpp (wires Slot4/7/8/9 — demolish/reclaim/rebuild
// panel with live progress bars). Stub registration removed to avoid a
// duplicate REGISTER_TYPE(DestroyUI).
// EntityUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/entityui.cpp
// (wires Slot4 so the entity panel gets its +0x30 entity ref → correct positioning).
// EventLogUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/eventlogui.cpp
// (wires Slot5/7/9 + per-frame OnUpdate so the event-log panel populates from gameWorld+0x138).
// ---- HelpUI: PROMOTED to the real reimpl (help-dialog bring-up 2026-07-31) ----
// The toolbar "help" utility dialog (Template\UtilityHelp.rsc -> StandardDialog.rsc,
// content Dialog/Help.rsc). The whole controller is already reimplemented in
// code/Game/Dialog/helpui.cpp, but the generic stub below left every slot a no-op,
// so the dialog opened with an empty topic combo and a blank document — the
// "controllers render NO content" follow-up recorded in GUI_PLAN.md Phase 4.
//
// Slot map, read out of the retail vtable at exe .rdata 0x14015bf30 (qword N =
// slot N), NOT from our labels:
//   slot 4 (+0x20) 0x14006a500 Slot4  — setup: find the 4 typed elements + the two
//                                       named buttons, populate the topic combo from
//                                       the help ExternalList, tail into LoadPage.
//   slot 5 (+0x28) 0x14006a4f0        — 3-byte RET no-op (left as StubNop).
//   slot 7 (+0x38) 0x14006a230 Slot7  — enable/disable back+forward per history index.
//   slot 9 (+0x48) 0x14006a290 Slot9  — combo change (msg 0x14), document link click
//                                       and back/forward (msg 0x12).
// Slot9's 7th parameter is the clicked link index: retail reads it from
// [RSP+0xb0] at 0x14006a2ed, which is arg 7 in this frame — hence the 7-arg wrapper.
//
// ── Construction deviation ───────────────────────────────────────────────────
// HelpUI_Ctor (0x14006a920) zeroes +0x30..+0x58 and +0x68/+0x70 but writes
// **-1** to the qword at +0x60, i.e. BOTH the current page index (+0x60) and the
// history index (+0x64) start at -1. A plain zero-fill is not equivalent: LoadPage
// truncates the history to (historyIndex + 1) entries, so a 0 index leaves one
// uninitialised history slot in front of the first real page and the back button
// then navigates to a garbage page id. Factory/InPlace stamp the -1 explicitly.
//
// ── Type-id dependency ───────────────────────────────────────────────────────
// Slot4 looks its elements up by type (ScrollElement/ComboElement/
// DocumentTextElement/ContainerElement, plus ButtonElement for the two named
// buttons) and checks the topic resources against StringTable. Those ids come from
// exe .rdata dispatch slots that standalone only fills when
// BANISHED_LIVE_ELEMENT_TIDS is set (standalone/reg_element_typeids.cpp). Rather
// than make the help dialog depend on that gate, EnsureTypeIds() points the globals
// straight at the Runtime DLL's own BaseTypeInfo exports — the same objects retail's
// name resolution would have written into those slots, so this is a no-op when the
// gate is on and a self-heal when it is off. A type-id that fails to resolve leaves
// the element null, and the reimpl's standalone guards then degrade to today's
// empty-panel behaviour instead of faulting.
struct HelpUI;
extern void HelpUI_Slot4(HelpUI* self);
extern void HelpUI_Slot7(HelpUI* self);
extern void HelpUI_Slot9(HelpUI* self, uint64_t param2, int msg, int id,
                         int param5, int param6, int linkIndex);
namespace uistub_HelpUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("HelpUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // Fill the element/StringTable type-id globals from the Runtime's BaseTypeInfo
    // data exports (first dword of each object is the id). These are Runtime-
    // registered types, so Object::GetTypeIdByName cannot see them — see the
    // "Why ResolveExport and not GetTypeIdByName" note in reg_element_typeids.cpp.
    // Idempotent; never overwrites a slot the gate already filled.
    void EnsureTypeIds() {
        struct Fill { uint32_t** global; const char* mangled; };
        const Fill fills[] = {
            { &g_TypeId_HelpFirstElem,     "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A" },
            { &g_TypeId_HelpComboElem,     "?_typeInfo@ComboElement@UI@@0VBaseTypeInfo@Object@@A" },
            { &g_TypeId_HelpDocTextElem,   "?_typeInfo@DocumentTextElement@UI@@0VBaseTypeInfo@Object@@A" },
            { &g_TypeId_HelpContainerElem, "?_typeInfo@ContainerElement@UI@@0VBaseTypeInfo@Object@@A" },
            { &g_TypeId_HelpButtonElem,    "?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A" },
            { &g_TypeId_StringTable,       "?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A" },
        };
        for (const Fill& f : fills) {
            if (*f.global) continue;
            *f.global = reinterpret_cast<uint32_t*>(ResolveExport(f.mangled));
        }
    }
    // Everything the wired slots dereference unconditionally.
    bool Ready() {
        return g_TypeId_HelpFirstElem && g_TypeId_HelpComboElem &&
               g_TypeId_HelpDocTextElem && g_TypeId_HelpContainerElem &&
               g_TypeId_HelpButtonElem && g_TypeId_StringTable &&
               fn_IsDerivedFrom && fn_InternalBasePtr_Ctor &&
               fn_ExternalList_GetResourceCount &&
               fn_MemAlloc && g_pMemAllocSingleton;
    }
    // Report the OUTCOME of the readiness test, once, naming what is missing.
    bool CheckOnce() {
        static int s_ok = -1;
        if (s_ok >= 0) return s_ok != 0;
        EnsureTypeIds();
        s_ok = Ready() ? 1 : 0;
        if (!s_ok) {
            fprintf(stderr, "[HelpUI] slots stay no-op — unresolved deps:%s%s%s%s%s%s\n",
                    g_TypeId_HelpFirstElem     ? "" : " ScrollElement",
                    g_TypeId_HelpComboElem     ? "" : " ComboElement",
                    g_TypeId_HelpDocTextElem   ? "" : " DocumentTextElement",
                    g_TypeId_HelpContainerElem ? "" : " ContainerElement",
                    g_TypeId_HelpButtonElem    ? "" : " ButtonElement",
                    g_TypeId_StringTable       ? "" : " StringTable");
            fflush(stderr);
        }
        return s_ok != 0;
    }
    void Trace(const char* slot, void* self) {
        static int n=0; if (n>=24) return; ++n;
        uintptr_t s=reinterpret_cast<uintptr_t>(self);
        void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
        void* topics = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw)+0xa0) : nullptr;
        printf("[HelpUI] %s self=%p root(+0x20)=%p scroll=%p doc=%p combo=%p back=%p fwd=%p "
               "cont=%p page=%d hist=%d/%d topics=%p n=%d\n", slot, self,
               *reinterpret_cast<void**>(s+0x20), *reinterpret_cast<void**>(s+0x30),
               *reinterpret_cast<void**>(s+0x38), *reinterpret_cast<void**>(s+0x40),
               *reinterpret_cast<void**>(s+0x48), *reinterpret_cast<void**>(s+0x50),
               *reinterpret_cast<void**>(s+0x58), *reinterpret_cast<int*>(s+0x60),
               *reinterpret_cast<int*>(s+0x64), *reinterpret_cast<int*>(s+0x70), topics,
               (topics && fn_ExternalList_GetResourceCount)
                   ? fn_ExternalList_GetResourceCount(topics) : -1);
        fflush(stdout);
    }
    void Slot4(void* self) {
        if (!self || !CheckOnce()) return;
        // Setup is not idempotent: a second run appends every topic to the combo
        // again and pushes a second history entry. The framework dispatches it once
        // per dialog build; skip if the element cache is already populated.
        if (*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self)+0x40)) {
            Trace("Slot4-skip(already set up)", self);
            return;
        }
        HelpUI_Slot4(reinterpret_cast<HelpUI*>(self));
        Trace("Slot4", self);
    }
    void Slot7(void* self) {
        if (!self || !CheckOnce()) return;
        HelpUI_Slot7(reinterpret_cast<HelpUI*>(self));
    }
    void Slot9(void* self, uint64_t p2, int msg, int id, int p5, int p6, int linkIndex) {
        if (!self || !CheckOnce()) return;
        Trace("Slot9", self);
        HelpUI_Slot9(reinterpret_cast<HelpUI*>(self), p2, msg, id, p5, p6, linkIndex);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's HelpUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "HelpUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                          g_vt[7]=reinterpret_cast<void*>(&Slot7);
                          g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    // Mirror HelpUI_Ctor: zero, then the -1 qword at +0x60 (page index AND history index).
    void Init(void* p) {
        for (size_t o=0;o<(size_t)0x78;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0;
        *reinterpret_cast<uint64_t*>((char*)p+0x60) = 0xffffffffffffffffull;
        *reinterpret_cast<void**>(p)=g_vt;
    }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x78,8); if(!p) return nullptr; Init(p); return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Init(m); return m; }
}
REGISTER_TYPE(HelpUI, "UI::DialogController",
              &uistub_HelpUI::Factory, &uistub_HelpUI::InPlace);
// HotKeyToolTipUI promoted to a real Slot4 reimpl above (was a no-op stub).
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
struct ImmigrationUI;
extern void ImmigrationUI_Slot4(ImmigrationUI* self);
extern void ImmigrationUI_Slot7(ImmigrationUI* self);
extern void ImmigrationUI_Slot9(ImmigrationUI* self, void* param2, int msg, int id,
                                int param5, int param6);
namespace uistub_ImmigrationUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("ImmigrationUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // ImmigrationUI Slot7/Slot9 dereference these exe-.rdata type-id globals (null in
    // standalone): the entity IsA check (ComponentEntity — the reimpl already guards a
    // null id), fn_FindComponent_Immigration (ImmigrationComponent), and
    // fn_FindComponent_BuildState (DAT_1401c7d98 = StateMachineComponent; the
    // engine_funcs label now matches, see
    // [[project_townhallui_promoted]]). Fill from the live factory tree post-Engine::
    // Startup; Entity_FindComponentByExeTypeId derefs the id ptr, so it must be live.
    void EnsureGlobals() {
        static uint32_t s_entity=0, s_immig=0, s_bstate=0;
        if (!g_TypeId_ComponentEntity)      { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");        if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_ImmigrationComponent) { s_immig =::Object::ResolveTypeIdByNameLazy("ImmigrationComponent");   if(s_immig)  g_TypeId_ImmigrationComponent=&s_immig; }
        if (!g_TypeId_StateMachineComponent_FC)  { s_bstate=::Object::ResolveTypeIdByNameLazy("StateMachineComponent");  if(s_bstate) g_TypeId_StateMachineComponent_FC=&s_bstate; }
    }
    bool Ready() {
        return g_TypeId_ComponentEntity && g_TypeId_ImmigrationComponent &&
               g_TypeId_StateMachineComponent_FC &&
               g_pGameWorldSingleton && *g_pGameWorldSingleton;
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
    void Slot4(void* self) {
        if (!self) return;
        Trace("Slot4", self);
        ImmigrationUI_Slot4(reinterpret_cast<ImmigrationUI*>(self));
    }
    void Slot7(void* self) {
        EnsureGlobals();
        if (!self || !Ready()) return;
        Trace("Slot7", self);
        ImmigrationUI_Slot7(reinterpret_cast<ImmigrationUI*>(self));
    }
    void Slot9(void* self, void* p2, int msg, int id, int p5, int p6) {
        EnsureGlobals();
        if (!self || !Ready()) return;
        Trace("Slot9", self);
        ImmigrationUI_Slot9(reinterpret_cast<ImmigrationUI*>(self), p2, msg, id, p5, p6);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ImmigrationUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ImmigrationUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; Trace("created", p); return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; Trace("created(inplace)", m); return m; }
}
REGISTER_TYPE(ImmigrationUI, "UI::DialogController",
              &uistub_ImmigrationUI::Factory, &uistub_ImmigrationUI::InPlace);
// ---- MapUI: PROMOTED to the real reimpl (map-render bring-up) ----------------
// The minimap dialog's controller. Slot4 (+0x20, SetupImage) builds a
// MaterialInstance + SpriteSheet from the minimap subsystem's terrain/overlay
// ImageBuffers (gameWorld+0x148 +0x08/+0x10) and binds them to the "imageMap"/
// "imageOverlay" elements. Slot9 (+0x48) handles minimap clicks → move camera.
// The generic stub left both no-ops so the map dialog opened blank. The GPU
// buffers are created by Minimap_CreateAndUpload (code/Game/Game/minimap_upload.cpp);
// SetupImage is standalone-guarded (skips if the buffer is null / vtables absent).
struct MapUI;
extern void MapUI_Slot4(MapUI* self);
extern void MapUI_Slot9(MapUI* self, void* param2, int msg, int id, int cx, int cy);
namespace uistub_MapUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("MapUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // SetupImage dereferences g_TypeId_Material / g_TypeId_ImageElement (both
    // resolve from raw exe .rdata/exref → null in standalone). Lazy-fill from the
    // live factory tree post-Engine::Startup. g_fUVScale has a 1.0 fallback in
    // mapui.cpp already. Idempotent.
    void EnsureTypeIds() {
        static uint32_t s_mat=0, s_img=0;
        if (!g_TypeId_Material)     { s_mat=::Object::ResolveTypeIdByNameLazy("Material");     if(s_mat) g_TypeId_Material=&s_mat; }
        if (!g_TypeId_ImageElement) { s_img=::Object::ResolveTypeIdByNameLazy("ImageElement"); if(s_img) g_TypeId_ImageElement=&s_img; }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        static bool s_logged = false;
        if (!s_logged && Opt().mapTrace) {
            s_logged = true;
            void* gw = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
            void* mapObj = gw ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x148) : nullptr;
            printf("[MapUI::Slot4] dispatched self=%p mat_tid=%p img_tid=%p "
                   "matVtbl=%p ssVtbl=%p terrBuf=%p ovrBuf=%p\n", self,
                   (void*)g_TypeId_Material, (void*)g_TypeId_ImageElement,
                   (void*)g_ModDialog_MaterialInstanceVtable,
                   (void*)g_ModDialog_SpriteSheetVtable,
                   mapObj ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mapObj) + 0x08) : nullptr,
                   mapObj ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mapObj) + 0x10) : nullptr);
            fflush(stdout);
        }
        if (self) MapUI_Slot4(reinterpret_cast<MapUI*>(self));
    }
    void Slot9(void* self, void* p2, int msg, int id, int cx, int cy) {
        if (self) MapUI_Slot9(reinterpret_cast<MapUI*>(self), p2, msg, id, cx, cy);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's MapUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "MapUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(MapUI, "UI::DialogController",
              &uistub_MapUI::Factory, &uistub_MapUI::InPlace);
UI_DIALOG_STUB(ModBrowseUI,         "UI::DialogController", 0x30);
UI_DIALOG_STUB(NoiseUI,             "UI::DialogController", 0x70);

// Wave 10D lane VTFID: the same inherited-slot fix as the hand-written blocks.
// These three go through the UI_DIALOG_STUB macro, whose body is shared with the
// UI::DialogInstance and UI::DialogControllerConfig types further down; those
// have a DIFFERENT base and a different vtable, so the registration is written
// out here per type rather than folded into the macro, where it would register
// twenty tables whose base this fix knows nothing about.
namespace {
struct StubMacroDlgCtrlReg {
    StubMacroDlgCtrlReg() {
        void* stub = reinterpret_cast<void*>(&StubNop);
        ::UIDialogControllerVtable::Register(uistub_ModBrowseUI::g_vt,
                                             kStubVtableSlots, "ModBrowseUI", stub);
        ::UIDialogControllerVtable::Register(uistub_NoiseUI::g_vt,
                                             kStubVtableSlots, "NoiseUI", stub);
    }
} g_stubMacroDlgCtrlReg;
}  // namespace
// ---- PageToggleUI: PROMOTED to the real reimpl (production-statistics button) --
// PageToggleUI drives the `checkProduction` CheckElement that Dialog/Production.rsc
// inserts into a workplace dialog's title bar (userTitle0). It is the ONLY thing
// that switches a StandardDialog between pageMain(0) and pageUser(4) — the page
// that holds the production-statistics content (Production.rsc:production4, driven
// by ProductionUI below). Retail bodies:
//   Slot4 (+0x20, Setup)  0x140075850 — FindElementType(CheckElement) -> +0x30,
//                         SetElementId(700), FindElement("pages") -> +0x38, then
//                         restore the saved page if the dialog is on page 0 or 4.
//   Slot7 (+0x38, Update) 0x140075780 — enable/disable the checkbox (vtable +0x40)
//                         depending on whether the active page is 0 or 4.
//   Slot9 (+0x48, Event)  0x140075710 — msg 0x12 + id 700 -> +0x40 = checked ? 4 : 0
//                         and PageElement::SetActivePage. It opens NO dialog.
//   Slot3 (+0x18)         0x1400759a0 is a 3-byte `RET 0` — left a no-op stub
//                         (see ui_panels.h:618).
// The generic stub made all three no-ops, so the button was inert: the id-700
// event never fired and clicking the control did nothing.
//
// HAZARD (retail-faithful, hence the gate below): Slot7 derefs the checkbox
// vtable with NO null check, so if Slot4's FindElementType misses, Slot7 faults.
// In standalone the checkbox type id is null by default — the exe reads it through
// .rdata exref 0x1401516a8, and that whole element-type-id table
// (standalone/reg_element_typeids.cpp) is gated on BANISHED_LIVE_ELEMENT_TIDS,
// which is NOT in the default-on table. With g_TypeId_CheckboxElement null,
// FindElementType(root, 0) can never match, +0x30 stays null, and Slot7 faults.
// pagetoggleui.cpp therefore resolves a PRIVATE copy of the id by name (the
// exref's import string, read out of exe .rdata at 0x1401b8ce8, is
// "?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A" — i.e. UI::CheckElement),
// without writing the shared global, and Ready() refuses to dispatch any slot
// until it is non-zero.
struct PageToggleUI;
extern void PageToggleUI_Slot4(PageToggleUI* self);
extern void PageToggleUI_Slot7(PageToggleUI* self);
extern void PageToggleUI_Slot9(PageToggleUI* self, void* param2, int param3, int param4);
extern bool PageToggleUI_TypeIdsReady();
namespace uistub_PageToggleUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("PageToggleUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // The CheckElement type id is resolved PRIVATELY inside pagetoggleui.cpp
    // (PageToggleUI_TypeIdsReady) — see the long note there. We must not fill the
    // shared g_TypeId_CheckboxElement here: reg_element_typeids.cpp owns that
    // table and holds it behind BANISHED_LIVE_ELEMENT_TIDS on purpose.
    // Every slot below dereferences the checkbox / "pages" element unguarded, and
    // Slot4 cannot populate either without the type id. Report the outcome once.
    bool Ready() {
        bool ok = PageToggleUI_TypeIdsReady();
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            fprintf(stderr, "[PageToggleUI] Ready=%d (CheckElement id resolved=%d)\n",
                    (int)ok, (int)PageToggleUI_TypeIdsReady());
            fflush(stderr);
        }
        return ok;
    }
    void Slot4(void* self) { if (self && Ready()) PageToggleUI_Slot4(reinterpret_cast<PageToggleUI*>(self)); }
    // Slot7 runs every frame, including before Slot4 has populated +0x30/+0x38.
    // Retail cannot observe that ordering (its Slot4 always succeeds first), so
    // the empty-cache test below is a STANDALONE-ONLY precondition, not a
    // behaviour change: with both caches populated this is retail verbatim.
    void Slot7(void* self) {
        if (!self || !Ready()) return;
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        if (!*reinterpret_cast<void**>(s + 0x30) || !*reinterpret_cast<void**>(s + 0x38)) return;
        PageToggleUI_Slot7(reinterpret_cast<PageToggleUI*>(self));
    }
    void Slot9(void* self, void* p2, int msg, int id, int, int) {
        if (!self || !Ready()) return;
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        if (!*reinterpret_cast<void**>(s + 0x30) || !*reinterpret_cast<void**>(s + 0x38)) return;
        PageToggleUI_Slot9(reinterpret_cast<PageToggleUI*>(self), p2, msg, id);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's PageToggleUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "PageToggleUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x48;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x48,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(PageToggleUI, "UI::DialogController",
              &uistub_PageToggleUI::Factory, &uistub_PageToggleUI::InPlace);

// ---- ProductionUI: PROMOTED to the real reimpl (production-statistics content) -
// ProductionUI drives Dialog/Production.rsc:production4 at pageUser — the page
// PageToggleUI above flips to. Retail bodies:
//   Slot4 (+0x20, Setup)  0x140075da0 — find "production"/"production3"/"production4",
//                         take child 0 (consumed) + child 1 (produced), and cache
//                         each row's {labelStorage, numberCount} pair into the two
//                         dynamic arrays at +0x30 / +0x40.
//   Slot7 (+0x38, Update) 0x140075a20 -> helper 0x140075a50 twice (list 0, list 1) —
//                         reads the entity's TrackerComponent and fills sprite/text/
//                         count per row, hiding the leftovers.
//   Slot3 (+0x18)         0x140076300 decompiles to a bare `return;` — left a no-op.
// The generic stub made both no-ops, so even once the page could be reached it was
// blank.
//
// HAZARD: Slot7 runs EVERY frame, including before Slot4 has populated the caches,
// and the helper derefs tracker+0x48 unguarded. The tracker id is fixed properly
// (standalone/reg_tracker_typeid.cpp registers .rdata 0x1401c4b10 and game_main
// fills it after Object::Factory::Startup); the remaining null-tracker case is
// guarded inside productionui.cpp with an explicit standalone rationale.
struct ProductionUI;
extern void ProductionUI_Slot4(ProductionUI* self);
extern void ProductionUI_Slot7(ProductionUI* self);
extern bool ProductionUI_TypeIdsReady();
namespace uistub_ProductionUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("ProductionUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // ComponentEntity (.rdata 0x1401c5d70) and RawMaterialDescription (0x1401c7cb8)
    // are already registered+filled, ungated, by reg_action_child_ctors2, and
    // TrackerComponent (0x1401c4b10) by reg_tracker_typeid.cpp. The two ELEMENT ids
    // are resolved privately inside productionui.cpp (ProductionUI_TypeIdsReady) —
    // deliberately NOT published into g_TypeId_LabelElement / g_TypeId_NumberElement,
    // which reg_element_typeids.cpp owns and keeps behind BANISHED_LIVE_ELEMENT_TIDS.
    bool Ready() {
        bool ok = g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity != 0
                  && g_TypeId_RawMaterialDescription && *g_TypeId_RawMaterialDescription != 0
                  && g_TypeId_TrackerComponent && *g_TypeId_TrackerComponent != 0
                  && ProductionUI_TypeIdsReady()
                  && g_pGameWorldSingleton;
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            fprintf(stderr, "[ProductionUI] Ready=%d entity=%u rawmat=%u tracker=%u elementIds=%d\n",
                    (int)ok,
                    g_TypeId_ComponentEntity        ? *g_TypeId_ComponentEntity        : 0u,
                    g_TypeId_RawMaterialDescription ? *g_TypeId_RawMaterialDescription : 0u,
                    g_TypeId_TrackerComponent       ? *g_TypeId_TrackerComponent       : 0u,
                    (int)ProductionUI_TypeIdsReady());
            fflush(stderr);
        }
        return ok;
    }
    void Slot4(void* self) { if (self && Ready()) ProductionUI_Slot4(reinterpret_cast<ProductionUI*>(self)); }
    // Standalone-only precondition (see PageToggleUI::Slot7 above): with no cached
    // rows there is nothing for the retail body to write, and self+0x30/+0x40 are
    // still null.
    void Slot7(void* self) {
        if (!self || !Ready()) return;
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        if (*reinterpret_cast<uint32_t*>(s + 0x38) == 0 &&
            *reinterpret_cast<uint32_t*>(s + 0x48) == 0) return;
        ProductionUI_Slot7(reinterpret_cast<ProductionUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ProductionUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ProductionUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ProductionUI, "UI::DialogController",
              &uistub_ProductionUI::Factory, &uistub_ProductionUI::InPlace);
// ---- ProfessionUI: PROMOTED to the real reimpl (profession-assignment bring-up) -
// The toolbar "professions" overlay (Template\UtilityProfessions.rsc). Its controller
// lists every profession with a worker-count spinner + a "goto" button. The real
// reimpl lives in code/Game/Dialog/professionui.cpp; the generic stub left every slot
// a no-op so the overlay opened empty. Wire the three real slots:
//   Slot5 (+0x28, setup)  — builds the profession table, spinner/number element
//                           caches, and the profession→index BST; self-calls Slot8.
//   Slot8 (+0x40, update) — recounts citizens per profession each frame and refreshes
//                           the spinner values + worker-slot numbers. DialogInstance::
//                           OnUpdate dispatches this every frame once the controller is
//                           attached — INCLUDING before Slot5 has populated the caches,
//                           at which point the reimpl would deref the null spinner array
//                           (self+0x40). Guard: skip until setup has run (+0x30 counts
//                           array + +0x40 spinner array both non-null).
//   Slot9 (+0x48, event)  — spinner change reassigns citizens between professions
//                           (uses the CitizenList Find/Request/Remove worker fns already
//                           registered for the Town Hall profession tab); button click
//                           centers the camera on a citizen of that profession.
// The worker-reassignment + type-id deps all resolve in standalone already (see
// reg_townhallui.cpp / reg_town_workplace_registry_reader.cpp / reg_economy_typeids.cpp);
// only NumberEditElement's type-id is unregistered — lazy-fill it so Slot5's spinner-
// child validation runs (the reimpl already guards a null id, so this is optional but
// tidy). Construction stays the stub alloc+zero+vtable; the framework fills the base
// dialog/datasource fields afterward — same shape as the MapUI/ImmigrationUI promotions.
struct ProfessionUI;
extern void ProfessionUI_Slot5(ProfessionUI* self);
extern void ProfessionUI_Slot8(ProfessionUI* self);
extern void ProfessionUI_Slot9(ProfessionUI* self, void* param2, int param3, int param4);
namespace uistub_ProfessionUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("ProfessionUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // NumberEditElement type-id resolves from a double-indirect exref (null in SA).
    // Slot5 validates a spinner child against it (already guarded, so optional). Fill
    // it from the live factory tree post-Engine::Startup. Idempotent.
    void EnsureTypeIds() {
        static uint32_t s_numEdit=0;
        if (!g_TypeId_NumberEditElement) {
            s_numEdit=::Object::ResolveTypeIdByNameLazy("NumberEditElement");
            if (s_numEdit) g_TypeId_NumberEditElement=&s_numEdit;
        }
    }
    void Trace(const char* slot, void* self) {
        static int n=0; if (n>=32) return; ++n;
        uintptr_t s=reinterpret_cast<uintptr_t>(self);
        uint32_t spinN = self?*reinterpret_cast<uint32_t*>(s+0x48):0;
        printf("[ProfessionUI] %s self=%p dlg(+0x18)=%p root(+0x20)=%p counts(+0x30)=%p "
               "spinners(+0x40)=%p n=%u numbers(+0x50)=%p n=%u\n", slot, self,
               self?*reinterpret_cast<void**>(s+0x18):nullptr,
               self?*reinterpret_cast<void**>(s+0x20):nullptr,
               self?*reinterpret_cast<void**>(s+0x30):nullptr,
               self?*reinterpret_cast<void**>(s+0x40):nullptr, spinN,
               self?*reinterpret_cast<void**>(s+0x50):nullptr,
               self?*reinterpret_cast<uint32_t*>(s+0x58):0);
        // Dump the per-profession citizen counts (+0x30 int array) so we can confirm
        // Slot8 computes real assignments, not all-zero placeholder rows.
        int32_t* counts = self?*reinterpret_cast<int32_t**>(s+0x30):nullptr;
        if (counts && spinN>0 && spinN<=64) {
            printf("[ProfessionUI]   counts[0..%u]:", spinN-1);
            for (uint32_t i=0;i<spinN;i++) printf(" %d", counts[i]);
            printf("\n");
        }
        fflush(stdout);
    }
    // Has Slot5 populated the caches yet? counts(+0x30) is allocated by the Slot5 tail
    // (ExeArrayResize2), spinners(+0x40) grown as each profession row is added.
    bool IsSetUp(void* self) {
        uintptr_t s=reinterpret_cast<uintptr_t>(self);
        return *reinterpret_cast<void**>(s+0x30)!=nullptr &&
               *reinterpret_cast<void**>(s+0x40)!=nullptr;
    }
    void Slot5(void* self) {
        if (!self) return;
        // Idempotent: the framework dispatches Slot5 once during dialog creation, which
        // populates the table + spinner/number caches + BST. A SECOND run (e.g. the
        // harness re-drive, or a stray framework re-dispatch) does NOT clear first —
        // it re-clones the table cells and appends to the +0x40/+0x50 arrays, doubling
        // them (n=20→40) while the counts array (+0x30) stays sized to profCount. The
        // subsequent Slot8 then walks 40 half-built spinner IBPs and faults. Skip if
        // setup already ran.
        if (IsSetUp(self)) { Trace("Slot5-skip(already set up)", self); return; }
        EnsureTypeIds();
        Trace("Slot5", self);
        ProfessionUI_Slot5(reinterpret_cast<ProfessionUI*>(self));
    }
    void Slot8(void* self) {
        if (!self || !IsSetUp(self)) return;   // OnUpdate fires this pre-setup — skip
        Trace("Slot8", self);
        ProfessionUI_Slot8(reinterpret_cast<ProfessionUI*>(self));
    }
    void Slot9(void* self, void* p2, int msg, int id, int p5, int p6) {
        (void)p5; (void)p6;
        if (!self || !IsSetUp(self)) return;
        Trace("Slot9", self);
        ProfessionUI_Slot9(reinterpret_cast<ProfessionUI*>(self), p2, msg, id);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ProfessionUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ProfessionUI", &StubNop);
    struct Wire { Wire(){ g_vt[5]=reinterpret_cast<void*>(&Slot5);
                          g_vt[8]=reinterpret_cast<void*>(&Slot8);
                          g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x78;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x78,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ProfessionUI, "UI::DialogController",
              &uistub_ProfessionUI::Factory, &uistub_ProfessionUI::InPlace);
// ---- RadiusUI: PROMOTED to the real reimpl (2026-08-15) ---------------------
// Moved to code/Game/Dialog/radiusui_promote.cpp, which carries the REGISTER_TYPE
// (never delete a stub REGISTER_TYPE — some are the only filler of a live type-id
// global). The stub's 48 no-op slots left idx5/idx6 dead, so the coverage-radius
// overlay never appeared on any radius building's info panel. It also zeroed +0x30,
// which the real Factory seeds to -1.

// ---- ResourceLimitUI: PROMOTED to the real reimpl (resource-limit bring-up) --
// Each row of the Town Hall "Limits" tab (Log/Stone/Iron/Herbs/Tools/Fuel/Food/
// Clothing/Alcohol/Coal/Textile) is a ResourceLimitUI child controller bound to a
// ResourceLimitUIConfig (resource index at config+0x20). Slot4 (+0x20, setup)
// finds the row's "editLimit" spinner and seeds it from the live per-resource
// limit at townMgr(gw+0xf0)+0x170[idx*0x20+8]; Slot7 (+0x38) keeps it synced;
// Slot9 (+0x48) writes an edited value back. The generic stub left every slot a
// no-op, so all limit spinners showed the .rsc default 0 even though the
// town-manager ctor (TradeComponent_impl_0a30) seeds real per-resource defaults
// (200 / 5000 / 100 / 50 / 500 / 1000 ...). Construction stays the stub
// alloc+zero+vtable; the framework fills the base fields (dialog +0x18,
// datasource +0x10, element root +0x20) afterward. Same shape as the
// BuildRequirementUI / MapUI promotions above.
// The standalone-only entry points (index supplied by dispatch order; see the
// "Standalone entry points" note in code/Game/Dialog/resourcelimitui.cpp).
struct ResourceLimitUI;
extern void ResourceLimitUI_Setup_SA(ResourceLimitUI* self, int resourceIndex);
extern void ResourceLimitUI_Sync_SA(ResourceLimitUI* self, int resourceIndex);
extern void ResourceLimitUI_Write_SA(ResourceLimitUI* self, void* param2, int msg, int id, int resourceIndex);
extern int  ResourceLimitUI_ReadLimit_SA(int resourceIndex);
namespace uistub_ResourceLimitUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("ResourceLimitUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // Resource index for this row. PREFERRED source: the bound config's +0x20,
    // exactly as retail Slot4/7/9 read it — ResourceLimitUIConfig is a real class
    // as of 2026-07-31 (code/Game/Dialog/resourcelimituiconfig.cpp) and its idx5
    // deserialize fills that field.
    //
    // The dispatch-order fallback below is what we used while the config was a
    // stub, and it is UNSOUND across dialog rebuilds: it keys on the controller
    // POINTER, so when reopening the dialog allocates a row controller at an
    // address reused from the previous build, the loop finds a stale match and
    // returns that row's old index — desyncing every row after it. That was the
    // user-visible "the food limit shows a different value each time I open and
    // close the Town Hall". It is kept only so a row whose config somehow did not
    // bind still shows plausible data instead of always reading Log, and it now
    // says so when it fires.
    static void* g_selves[11];
    static int   g_count = 0;
    int IdxForFallback(void* self) {
        for (int i=0;i<g_count;i++) if (g_selves[i]==self) return i;
        if (g_count >= 11) { for (int i=0;i<11;i++) g_selves[i]=nullptr; g_count=0; }
        g_selves[g_count]=self;
        return g_count++;
    }
    int IdxFor(void* self) {
        void* cfg = self ? *reinterpret_cast<void**>((uintptr_t)self + 0x10) : nullptr;
        if (cfg) {
            int idx = *reinterpret_cast<int*>((uintptr_t)cfg + 0x20);
            if (idx >= 0 && idx < 11) return idx;
        }
        int idx = IdxForFallback(self);
        return idx;
    }
    void Trace(const char* slot, void* self, int idx) {
        static int n=0; if (n>=24) return; ++n;
        void* cfg = self ? *reinterpret_cast<void**>((uintptr_t)self+0x10) : nullptr;
        int cfgIdx = cfg ? *reinterpret_cast<int*>((uintptr_t)cfg+0x20) : -1;
        printf("[ResourceLimitUI] %s self=%p cfg=%p cfg+0x20=%d dispIdx=%d liveLimit=%d\n",
               slot, self, cfg, cfgIdx, idx, ResourceLimitUI_ReadLimit_SA(idx));
        fflush(stdout);
    }
    void Slot4(void* self){ if(!self) return; int idx=IdxFor(self); Trace("Slot4",self,idx); ResourceLimitUI_Setup_SA(reinterpret_cast<ResourceLimitUI*>(self), idx); }
    void Slot7(void* self){ if(!self) return; int idx=IdxFor(self); ResourceLimitUI_Sync_SA(reinterpret_cast<ResourceLimitUI*>(self), idx); }
    void Slot9(void* self, void* p2, int msg, int id, int, int){ if(!self) return; int idx=IdxFor(self); Trace("Slot9",self,idx); ResourceLimitUI_Write_SA(reinterpret_cast<ResourceLimitUI*>(self), p2, msg, id, idx); }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ResourceLimitUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ResourceLimitUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x40;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x40,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ResourceLimitUI, "UI::DialogController",
              &uistub_ResourceLimitUI::Factory, &uistub_ResourceLimitUI::InPlace);
UI_DIALOG_STUB(SpriteCaptureUI,     "UI::DialogController", 0x70);

// Wave 10D lane VTFID — see the ModBrowseUI/NoiseUI note above.
namespace {
struct SpriteCaptureDlgCtrlReg {
    SpriteCaptureDlgCtrlReg() {
        ::UIDialogControllerVtable::Register(uistub_SpriteCaptureUI::g_vt,
                                             kStubVtableSlots, "SpriteCaptureUI",
                                             reinterpret_cast<void*>(&StubNop));
    }
} g_spriteCaptureDlgCtrlReg;
}  // namespace
// ---- StatusBarUI: PROMOTED to real SA vtable (statusbarui.cpp) — the toolbar
//      "town status" utility dialog (group 4, next to Map) now renders the town
//      name title + population/resources/weather/hearts/stars instead of zeros ----
// ---- StatusIconUI: PROMOTED to real SA vtable (statusiconui.cpp) — icons now
//      show/hide per StatusIconComponent +0xF0 state instead of all-at-once ----
// ---- StorageCapacityUI: PROMOTED to real SA vtable (storagecapacityui.cpp) — the
//      storage building's "X%" fill number now computes+renders instead of staying
//      blank (Serialize finds the ProgressElement, Startup sets count/capacity %). ----
// ---- TitleDescToolTipUI: PROMOTED — wiring only, the logic was already ported --
// titledesctooltipui.cpp has held a faithful TitleDescToolTipUI_Serialize (exe
// 0x14007b730, vtable +0x20 = idx4) since it was decompiled, and NOTHING ever wired
// it: the generic stub left every slot StubNop, so the tooltip rendered its .rsc
// placeholder text forever. No port work was needed here, only a wire.
//
// Ghidra vtable read (vtable_TitleDescToolTipUI @ 0x14015d9f0, 11 slots): idx4 is
// the ONLY real slot; every other entry is one of the shared 5-6 byte micro-returns
// clustered at 0x14003a95a-0x14003a97e, for which the generic StubNop is already
// byte-faithful. So idx4 closes the work list at the vtable boundary — there is no
// second slot waiting to be found.
//
// No readiness gate: unlike PageToggleUI/ProductionUI this reimpl reads no element
// type id (it finds "textTitle"/"textDesc" BY NAME) and touches no .rdata type-id
// global, so there is nothing that could be null at static-init time. It also has
// zero unguarded dereferences — it null-checks both +0x18 (dialog) and +0x28
// (datasource) before use — which is why this was chosen as the first wire of the
// sweep: if the wiring mechanism itself is wrong, this class fails visibly without
// being able to fault.
// Forward-declared rather than including ui_panels.h: this file deliberately does
// not pull in the panel headers (the other promotions forward-declare too), and a
// pointer to an incomplete type is all the call needs.
struct TitleDescToolTipUI;
void TitleDescToolTipUI_Serialize(TitleDescToolTipUI* self);

namespace uistub_TitleDescToolTipUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("TitleDescToolTipUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    void Slot4(void* self) {
        if (!self) return;
        ::TitleDescToolTipUI_Serialize(reinterpret_cast<TitleDescToolTipUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's TitleDescToolTipUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "TitleDescToolTipUI", &StubNop);
    // MUST be declared AFTER g_init: same TU, same namespace, both dynamic
    // initializers, so declaration order is guaranteed. Reversed, g_init's blanket
    // StubNop fill would run second and silently un-wire idx4 — the promotion would
    // look complete and be dead.
    struct Slot4Wire { Slot4Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); } } g_s4;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x30;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x30,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(TitleDescToolTipUI, "UI::DialogController",
              &uistub_TitleDescToolTipUI::Factory, &uistub_TitleDescToolTipUI::InPlace);
// ---- ToolUI: PROMOTED to the real Slot4 reimpl (map-render bring-up) ----------
// ToolUI is StandardDialog.rsc's controller — created for EVERY StandardDialog-
// based utility dialog (Map/Help/Goal/…). Its Slot4 (+0x20, Setup) reads the
// dialog's data source (a ComponentEntity → its description), finds the title/
// close/drag elements, and — crucially — searches the description for a
// UIDescription and calls UIDesc_AddControllers_Impl, which injects each of the
// UIDescription's sub-controllers (for UtilityMap: the MapUI controller +
// Dialog/Map.rsc:content = imageMap/imageOverlay/frustum) at their _insertAt
// element ("userGroup0") via DialogInstance::AddController6. The generic no-op
// stub left Slot4 dead, so NO utility dialog ever built its inner content — the
// minimap was never instantiated. Wiring Slot4 to the real reimpl runs the whole
// injection chain. Slot9 (+0x48, event) is now wired too so the close (X) button
// works, and Slot8 (+0x40, per-frame drag-follow) so the title-bar drag actually
// moves the dialog: Runtime DialogInstance::OnUpdate (0x180115400) calls every
// controller's vtable+0x40 with (controller, Time&) each frame; Slot9's msg=2
// drag-init only sets the flags, Slot8 does the SetUpperLeft follow + writes the
// new position into the utility-dialog position BST (gameSubsys+0x50).
struct ToolUI;
extern void ToolUI_Slot4(ToolUI* self);
extern void ToolUI_Slot8(ToolUI* self);
// Slot9 (+0x48) is ToolUI's OnElementEvent handler. It contains the buttonClose
// (element id 1, msg 0x12) -> UserInterface::CloseDialog logic that dismisses the
// dialog, plus focus (msg 8..10 -> BringDialogToFront) and drag-init (msg 2).
// DialogInstance::OnElementEvent (Runtime 0x180114ed0) dispatches every element
// event to each controller's vtable+0x48, so leaving this a no-op meant the map/
// Help/etc. dialogs' X button (and every close button) did nothing.
extern void ToolUI_Slot9(ToolUI* self, void* param2, int msg, int id, int mx, int my);
namespace uistub_ToolUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("ToolUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // ToolUI_Slot4 + UIDesc_AddControllers_Impl dereference these exe-.rdata type-id
    // globals (null in standalone). Lazy-fill from the live factory tree post-
    // Engine::Startup. Idempotent.
    void EnsureTypeIds() {
        static uint32_t s_build=0, s_entity=0, s_breq=0, s_uidesc=0, s_tab=0;
        // NB: g_TypeId_ComponentDescription_FC maps to exe .rdata 0x1401c7318 whose real
        // identity is ComponentDescription (verified: written by
        // ComponentDescription_Register; the engine_funcs label now matches).
        // ToolUI::Slot4 branch-1 tests the datasource against
        // THIS id (a utility dialog's datasource is a ComponentDescription), so it
        // must be filled with the ComponentDescription type-id.
        if (!g_TypeId_ComponentDescription_FC)            { s_build =::Object::ResolveTypeIdByNameLazy("ComponentDescription");      if(s_build)  g_TypeId_ComponentDescription_FC=&s_build; }
        if (!g_TypeId_ComponentEntity)           { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");           if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        // CORRECTED 2026-07-19: slot 0x1401c7f58's writer is ToolbarDescription_RegisterType;
        // the old resolve string "BuildRequirementComponent" (an invented name) is not in the
        // type registry, so this heal always returned 0 and silently never fired.
        if (!g_TypeId_ToolbarDescription_FC) { s_breq  =::Object::ResolveTypeIdByNameLazy("ToolbarDescription"); if(s_breq)   g_TypeId_ToolbarDescription_FC=&s_breq; }
        if (!g_TypeId_UIDescription)             { s_uidesc=::Object::ResolveTypeIdByNameLazy("UIDescription");             if(s_uidesc) g_TypeId_UIDescription=&s_uidesc; }
        if (!g_TypeId_TabElement)                { s_tab   =::Object::ResolveTypeIdByNameLazy("TabElement");                if(s_tab)    g_TypeId_TabElement=&s_tab; }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        // The reimpl unconditionally dereferences these; skip rather than fault if
        // any failed to resolve.
        if (!self || !g_TypeId_ComponentDescription_FC || !g_TypeId_ComponentEntity
            || !g_TypeId_ToolbarDescription_FC || !g_TypeId_UIDescription
            || !g_TypeId_TabElement) return;
        static bool s_logged=false;
        if(!s_logged && Opt().mapTrace){ s_logged=true;
            printf("[ToolUI::Slot4] dispatched self=%p ds(+0x28)=%p\n",
                   self, *reinterpret_cast<void**>((char*)self+0x28)); fflush(stdout); }
        ToolUI_Slot4(reinterpret_cast<ToolUI*>(self));
    }
    // Per-frame update (+0x40). Runtime DialogInstance::OnUpdate dispatches
    // (controller, Time&); the reimpl ignores the Time arg. Idle cost is two flag
    // reads (+0x41/+0x4D, both 0 unless Slot9 armed a drag). Guard the KeyMap
    // exref + the Runtime fns the drag path dereferences unconditionally.
    void Slot8(void* self, void* /*time*/) {
        if (!self || !g_pInputConfigSingletonExref || !*g_pInputConfigSingletonExref
            || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
        ToolUI_Slot8(reinterpret_cast<ToolUI*>(self));
    }
    // OnElementEvent (+0x48). Dispatched with (self, &elementPtr, msg, id, x, y, ...);
    // the real handler closes the dialog on the buttonClose (id 1, msg 0x12).
    void Slot9(void* self, void* p2, int msg, int id, int mx, int my) {
        if (self) ToolUI_Slot9(reinterpret_cast<ToolUI*>(self), p2, msg, id, mx, my);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's ToolUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "ToolUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                          g_vt[8]=reinterpret_cast<void*>(&Slot8);
                          g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x58;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x58,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ToolUI, "UI::DialogController",
              &uistub_ToolUI::Factory, &uistub_ToolUI::InPlace);
// ---- TownHallUI: PROMOTED to real SA vtable (townhallui.cpp) — the town-hall
//      General statistics / Professions / Production / Inventory / Graph /
//      Agriculture panel now populates from the full reimplementation. ----
// ---- TradeUI: PROMOTED to the real reimpl (Wave A), GATED on its exe leaves ----
// The trading-post panel (Dialog\Trade.rsc). ~2800 lines of real reimplementation
// already exist in code/Game/Dialog/tradeui.cpp but were unreachable: the generic
// UI_DIALOG_STUB below registered the type name to a zero-filled 0x148 stub with a
// no-op vtable, so opening a post gave a blank panel and every ported method was
// dead code. This promotion follows the ProfessionUI pattern above (own g_vt +
// REGISTER_TYPE; slots wired by index, never by exe address).
//
// Slot map (byte offset = slot * 8), taken from the per-function headers in
// tradeui.cpp, which were recorded against the retail vtable when each was ported:
//   slot 4  (+0x20) TradeUI_Serialize — MISNAMED: this is the full UI SETUP
//                   (element lookup + table building). It is the ProfessionUI-Slot5
//                   analogue and must run before any other slot touches the caches.
//   slot 5  (+0x28) TradeUI_Draw      — mark tables for layout recalc
//   slot 6  (+0x30) TradeUI_Slot6     — clear the building dirty flag
//   slot 7  (+0x38) TradeUI_Startup   — trade value calcs, warning text, button enable
//   slot 8  (+0x40) TradeUI_OnCreate  — per-frame refresh when the order count changes
//   slot 9  (+0x48) TradeUI_OnRemove  — event dispatcher (spinners/combos/buttons)
//   slot 10 (+0x50) TradeUI_Update    — serialize the tab/sort state
//
// ── WHY THE GATE ─────────────────────────────────────────────────────────────
// tradeui.cpp calls 11 exe-resolved leaf helpers that are NOT ported and NOT
// registered, so ResolveAddr returns 0 for every one of them:
//   0x14005a520 fn_Building_GetSellableCount    0x140016050 fn_DynArray_Clear_0x20
//   0x140059670 fn_Building_GetBuyEntries       0x140016130 fn_DynArray_Clear_0x18_Buy
//   0x140015fd0 fn_Building_GetOrderName        0x140016200 fn_DynArray_Clear_0x18
//   0x1400165d0 fn_IntroSort_ByComponent        0x1400162d0 fn_IntroSort_ByName
//   0x14000e1c0 fn_StorageTree_Insert           0x140059490 fn_Building_AddPurchaseOrder
//   0x140081420 fn_TradeUI_OnTradeButton (also needs Wave D — it IS the checkout)
// Eight of those are reachable from the slot-4 SETUP path alone (it tails into
// RefreshDisplay + RefreshSellTable + RefreshBuyTable), so wiring the real slots
// today would EXEC-of-0 on the very first dispatch — the panel would crash rather
// than render. Wave A is therefore "port + register those 11, THEN flip", not a
// one-line stub removal.
//
// Rather than leave that as a tripwire, the wiring is conditional: DepsReady()
// checks every one of them at first dispatch, and while any is null the slots stay
// no-ops (identical to the old stub) and a single loud line names what is missing.
// Once the 11 land, this promotion goes live with no further edit here.
struct TradeUI;
extern void TradeUI_Serialize(TradeUI* self);
extern void TradeUI_Draw(TradeUI* self);
extern void TradeUI_Slot6(TradeUI* self);
extern void TradeUI_Startup(TradeUI* self);
extern void TradeUI_OnCreate(TradeUI* self);
extern void TradeUI_OnRemove(TradeUI* self, void* param2, int eventType, int eventId);
extern void TradeUI_Update(TradeUI* self, void* stream, void* p3, int64_t p4);
namespace uistub_TradeUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("TradeUI"); return c; }
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self; *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // Every exe leaf the wired slots can reach. Null while unported/unregistered.
    bool DepsReady() {
        static int s_ok = -1;
        if (s_ok >= 0) return s_ok != 0;
        struct Dep { const char* name; const void* fn; };
        const Dep deps[] = {
            { "fn_Building_GetSellableCount (0x14005a520)",  (const void*)fn_Building_GetSellableCount },
            { "fn_Building_GetBuyEntries (0x140059670)",     (const void*)fn_Building_GetBuyEntries },
            { "fn_Building_GetOrderName (0x140015fd0)",      (const void*)fn_Building_GetOrderName },
            { "fn_Building_AddPurchaseOrder (0x140059490)",  (const void*)fn_Building_AddPurchaseOrder },
            { "fn_DynArray_Clear_0x20 (0x140016050)",        (const void*)fn_DynArray_Clear_0x20 },
            { "fn_DynArray_Clear_0x18_Buy (0x140016130)",    (const void*)fn_DynArray_Clear_0x18_Buy },
            { "fn_DynArray_Clear_0x18 (0x140016200)",        (const void*)fn_DynArray_Clear_0x18 },
            { "fn_IntroSort_ByComponent (0x1400165d0)",      (const void*)fn_IntroSort_ByComponent },
            { "fn_IntroSort_ByName (0x1400162d0)",           (const void*)fn_IntroSort_ByName },
            { "fn_StorageTree_Insert (0x14000e1c0)",         (const void*)fn_StorageTree_Insert },
            // NOTE: fn_TradeUI_OnTradeButton (0x140081420) is deliberately NOT in this
            // list. It is the Wave D checkout, reachable ONLY from the slot-9 buttonTrade
            // branch, which null-guards it at the call site (tradeui.cpp). Including it
            // here would mean the panel could not open until Wave D landed — the opposite
            // of the intent, since rendering the panel needs none of the checkout.
        };
        int missing = 0;
        for (const Dep& d : deps) if (!d.fn) ++missing;
        s_ok = (missing == 0) ? 1 : 0;
        if (missing) {
            std::fprintf(stderr, "[TradeUI] STAYING STUBBED — %d unresolved exe leaf(s); the "
                         "panel would EXEC-of-0 on its setup slot. Missing:\n", missing);
            for (const Dep& d : deps)
                if (!d.fn) std::fprintf(stderr, "[TradeUI]     %s\n", d.name);
            std::fprintf(stderr, "[TradeUI]   Port+register these, then TradeUI goes live with "
                         "no further change to ui_dialog_stub_factories.cpp.\n");
            std::fflush(stderr);
        }
        return s_ok != 0;
    }
    // Slot 4 builds the element caches (+0xC0..+0x140) every other slot dereferences.
    // Same hazard ProfessionUI documents: the framework dispatches the per-frame slots
    // before setup has run, so they must skip until the caches exist.
    bool IsSetUp(void* self) {
        uintptr_t s=reinterpret_cast<uintptr_t>(self);
        return *reinterpret_cast<void**>(s+0xC8)!=nullptr &&   // tableSell
               *reinterpret_cast<void**>(s+0xD0)!=nullptr;     // tableBuy
    }
    void Trace(const char* slot, void* self) {
        static int n=0; if (n>=32) return; ++n;
        uintptr_t s=reinterpret_cast<uintptr_t>(self);
        // stderr, NOT stdout — two independent reasons, both already project rules:
        //  1. `std::printf` does not survive the release build. log_strip.h defines
        //     `printf(...)` as `((void)0)`, so the QUALIFIED call expands to
        //     `std::((void)0)` and fails with "expected unqualified-id before '('".
        //     This single line broke `make standalone` for the whole tree.
        //  2. stdout STALLS in this exe (standalone_debug.log froze at 22KB on the splash
        //     while stderr kept growing all session), so a printf-based harness prints
        //     NOTHING and looks identical to one that never fired — which is exactly the
        //     symptom this trace was added to investigate.
        std::fprintf(stderr, "[TradeUI] %s self=%p tableSell(+0xC8)=%p tableBuy(+0xD0)=%p "
               "buttonTrade(+0xF0)=%p sell.n(+0x38)=%d buy.n(+0x48)=%d\n", slot, self,
               self?*reinterpret_cast<void**>(s+0xC8):nullptr,
               self?*reinterpret_cast<void**>(s+0xD0):nullptr,
               self?*reinterpret_cast<void**>(s+0xF0):nullptr,
               self?*reinterpret_cast<int32_t*>(s+0x38):0,
               self?*reinterpret_cast<int32_t*>(s+0x48):0);
        std::fflush(stderr);
    }
    // Slot 4 is idempotent-guarded for the same reason ProfessionUI's Slot5 is: a second
    // run re-clones table rows and appends to the element arrays instead of clearing.
    void Slot4(void* self) {
        if (!self || !DepsReady()) return;
        if (IsSetUp(self)) { Trace("Slot4-skip(already set up)", self); return; }
        Trace("Slot4-setup", self);
        TradeUI_Serialize(reinterpret_cast<TradeUI*>(self));
    }
    void Slot5(void* self) {
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        Trace("Slot5-draw", self); TradeUI_Draw(reinterpret_cast<TradeUI*>(self));
    }
    void Slot6(void* self) {
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        TradeUI_Slot6(reinterpret_cast<TradeUI*>(self));
    }
    void Slot7(void* self) {
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        Trace("Slot7-values", self); TradeUI_Startup(reinterpret_cast<TradeUI*>(self));
    }
    void Slot8(void* self) {
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        TradeUI_OnCreate(reinterpret_cast<TradeUI*>(self));
    }
    void Slot9(void* self, void* p2, int msg, int id, int p5, int p6) {
        (void)p5; (void)p6;
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        Trace("Slot9-event", self);
        TradeUI_OnRemove(reinterpret_cast<TradeUI*>(self), p2, msg, id);
    }
    void Slot10(void* self, void* stream, void* p3, int64_t p4) {
        if (!self || !DepsReady() || !IsSetUp(self)) return;
        TradeUI_Update(reinterpret_cast<TradeUI*>(self), stream, p3, p4);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's TradeUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "TradeUI", &StubNop);
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4);
                          g_vt[5]=reinterpret_cast<void*>(&Slot5);
                          g_vt[6]=reinterpret_cast<void*>(&Slot6);
                          g_vt[7]=reinterpret_cast<void*>(&Slot7);
                          g_vt[8]=reinterpret_cast<void*>(&Slot8);
                          g_vt[9]=reinterpret_cast<void*>(&Slot9);
                          g_vt[10]=reinterpret_cast<void*>(&Slot10); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x148;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x148,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(TradeUI, "UI::DialogController",
              &uistub_TradeUI::Factory, &uistub_TradeUI::InPlace);
// UpgradeUI: PROMOTED to a real per-class standalone vtable in
// code/Game/Dialog/upgradeui.cpp (wires Slot4/8/9 — the wood→stone upgrade
// button; click writes the target template into DestroyComponent+0x60 and
// pumps the BuildState state machine). Stub registration removed to avoid a
// duplicate REGISTER_TYPE(UpgradeUI).
// ---- WorkPlaceUI: PROMOTED to real SA vtable (workplaceui.cpp) — the construction
//      workplace spinner panel now self-hides once a building is complete (its
//      profession clears), so finished houses/barns/piles show only center/pin/close
//      + their residence/storage content instead of a stale worker up/down spinner. ----
// ---- BuildUI: PROMOTED to the real reimpl (2026-07-13) -----------------------
// The in-game "under construction" panel: a per-material storage table
// (delivered / required), a construction progress bar + percent, and a
// pause-construction button. It is a DialogController bound into a building's
// info dialog (created by EditTool select); the generic stub left every slot a
// no-op, so selecting a building being built showed the ordinary (empty) info
// panel with no materials/progress. Wiring Slot4 (setup +0x20), Slot7
// (per-frame update +0x38) and Slot9 (button event +0x48) to the real bodies in
// code/Game/Dialog/buildui.cpp populates it. Construction stays the stub
// alloc+zero+vtable; the engine fills the base fields (dialog +0x20 / datasource
// +0x28) afterward. Same shape as the BuildRequirementUI promotion above.
struct BuildUI;  // opaque; the reimpl casts internally
extern void BuildUI_Slot4(BuildUI* self);
extern void BuildUI_Slot7(BuildUI* self);
extern void BuildUI_Slot9(BuildUI* self, uint64_t param2, int param3, int param4);
namespace uistub_BuildUI {
    static void* g_vt[kStubVtableSlots];
    uint32_t GetTypeId(void*) { static uint32_t c=0; if(!c) c=::Object::GetTypeIdByName("BuildUI"); return c; }
    // Dtor: free the Slot4-allocated row array at self+0x30 (MemAlloc'd fresh on
    // every open), then the standard vtable-restore + conditional self-free.
    void* Dtor(void* self, uint32_t flags) {
        if (!self) return self;
        if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>((uintptr_t)self + 0x30));
        *reinterpret_cast<uint64_t*>((uintptr_t)self + 0x30) = 0;
        *reinterpret_cast<uint64_t*>((uintptr_t)self + 0x38) = 0;
        *reinterpret_cast<void**>(self)=g_vt;
        if ((flags&1)&&fn_MemFreeStatic) fn_MemFreeStatic(self); return self;
    }
    // Lazy-fill the exe-.rdata type-id globals the reimpl + its FindComponent
    // helpers key off. In standalone these resolve from raw exe .rdata → null;
    // fill from the live Factory tree (post-Engine::Startup), idempotent. The
    // FindComponent helpers null-check their type-id pointer (a still-null one is
    // a safe no-match, not a crash — Entity_FindComponentByExeTypeId:1139), so the
    // only HARD requirements are the three ids the reimpl derefs directly:
    // ComponentEntity / BuildDescription (g_TypeId_ResourceComponent) /
    // RawMaterialDescription. Position=BuildComponent (0x1401c7008) and
    // SM=ClearAreaComponent (0x1401c71c8) drive the resource/progress finds; the
    // rest (MapComponent/WorkPlaceComponent/WorkComponent/StorageComponent) are
    // already registered+filled by FillEconomyTypeIdGlobals.
    void EnsureTypeIds() {
        static uint32_t s_entity=0, s_buildDesc=0, s_rawMat=0, s_build=0, s_clear=0;
        if (!g_TypeId_ComponentEntity)        { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");        if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_ResourceComponent)      { s_buildDesc=::Object::ResolveTypeIdByNameLazy("BuildDescription");    if(s_buildDesc) g_TypeId_ResourceComponent=&s_buildDesc; }
        if (!g_TypeId_RawMaterialDescription) { s_rawMat=::Object::ResolveTypeIdByNameLazy("RawMaterialDescription"); if(s_rawMat) g_TypeId_RawMaterialDescription=&s_rawMat; }
        if (!g_TypeId_BuildComponent_FC)      { s_build=::Object::ResolveTypeIdByNameLazy("BuildComponent");         if(s_build) g_TypeId_BuildComponent_FC=&s_build; }
        if (!g_TypeId_ClearAreaComponent_FC)            { s_clear=::Object::ResolveTypeIdByNameLazy("ClearAreaComponent");      if(s_clear) g_TypeId_ClearAreaComponent_FC=&s_clear; }
    }
    // Slot4 has run once self+0x40 (the "progress" element IBP) is populated.
    inline bool IsSetUp(void* self) { return self && *reinterpret_cast<uint64_t*>((uintptr_t)self + 0x40) != 0; }
    bool HardIdsReady() {
        return g_TypeId_ComponentEntity && g_TypeId_ResourceComponent && g_TypeId_RawMaterialDescription;
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !HardIdsReady() || !SA_DialogEntityAlive(self)) return;
        // Idempotent: the framework dispatches setup on attach; a re-drive would
        // append a second copy of the storage rows to self+0x30 (count never reset)
        // — exactly the ProfessionUI double-populate class. Skip if already set up.
        if (IsSetUp(self)) return;
        BuildUI_Slot4(reinterpret_cast<BuildUI*>(self));
#if defined(_DEBUG)
#endif
    }
    void Slot7(void* self) {
        EnsureTypeIds();
        // Per-frame update: the framework fires this every tick, including before
        // Slot4 populates (self+0x40 progress / self+0x30 rows still null). Guard.
        if (!IsSetUp(self) || !HardIdsReady()) return;
        if (!SA_DialogEntityAlive(self)) return;
        BuildUI_Slot7(reinterpret_cast<BuildUI*>(self));
    }
    void Slot9(void* self, uint64_t p2, int p3, int p4) {
        EnsureTypeIds();
        if (!self || !g_TypeId_ComponentEntity || !SA_DialogEntityAlive(self)) return;
        BuildUI_Slot9(reinterpret_cast<BuildUI*>(self), p2, p3, p4);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));

// Wave 10D lane VTFID: retail's BuildUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kStubVtableSlots, "BuildUI", &StubNop);
    struct SlotWire { SlotWire(){
        g_vt[4]=reinterpret_cast<void*>(&Slot4);
        g_vt[7]=reinterpret_cast<void*>(&Slot7);
        g_vt[9]=reinterpret_cast<void*>(&Slot9);
    } } g_sw;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){
        if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr;
        void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr;
        Zero(p); *reinterpret_cast<void**>(p)=g_vt;
#if defined(_DEBUG)
#endif
        return p;
    }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(BuildUI, "UI::DialogController",
              &uistub_BuildUI::Factory, &uistub_BuildUI::InPlace);
// ---- ResidenceUI: PROMOTED to real SA vtable (residenceui.cpp) — a house panel now
//      lists its residents (name/gender/age/profession) from the ResidenceComponent
//      roster instead of staying empty. ----
// ---- StorageUI: PROMOTED to real SA vtable (storageui.cpp) — Inventory/Storage
//      tab now populates rows from the entity's StorageComponent instead of "0" ----

// ---- SelectResourceUI subclasses (parent is the abstract SelectResourceUI) --
// ---- OrchardUI: PROMOTED to real SA vtable (orchardui.cpp) — the orchard panel's
//      tree-type combo now populates and the yield bar tracks growth instead of
//      sitting empty at 0. ----
// ---- CropUI: PROMOTED to real SA vtable (cropui_promote.cpp) — it was the last
//      member of this family still stubbed, which is why the crop field's "?"
//      crop-select combo did nothing: idx4 never bound the element or stamped id
//      500, so the dropdown had zero rows, and idx9 was a no-op so a pick could
//      never reach idx11. ----
// ---- PastureUI: PROMOTED to real SA vtable (pastureui.cpp) — livestock-type
//      combo + animal-count slider now live; retires BANISHED_PASTURE_AUTOTYPE
//      as the only way to type a pasture. ----

// ENDGAME B.3.8 DLG: the real DialogInstance classes whose reflection record
// stays at this TU position (B38_RULES D4).
#include "Dialog/SaveLoadDialog.h"
#include "Dialog/FollowDialog.h"
#include "Dialog/ScenarioDialog.h"
#include "Dialog/GoalDialog.h"
#include "Dialog/AchievementDialog.h"
#include "Dialog/MapParamsDialog.h"
// ---- DialogInstance family --------------------------------------------------
DEFINE_EXE_TYPE(AchievementDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
// AutoSaveDialog: its UI_DIALOG_STUB twin was a DUPLICATE registration of the name
// (ENDGAME B.3.8 D4); the real class registers in autosavedialog.cpp.
// EventNotifyDialog / EventPopupDialog: PROMOTED to real per-class standalone vtables
// in code/Game/Dialog/eventnotifydialog.cpp / eventpopupdialog.cpp (ribbon + modal popup).
DEFINE_EXE_TYPE(FollowDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(GoalDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(MapParamsDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
UI_DIALOG_STUB(ModUploadDialog,     "UI::DialogInstance",   0xB8);
DEFINE_EXE_TYPE(ScenarioDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(SaveLoadDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
UI_DIALOG_STUB(ToolbarDialog,       "UI::DialogInstance",   0x108);

// ---- DialogControllerConfig family (all 0x28) -------------------------------
// ControlUIConfig: PROMOTED to the real config class in
// code/Game/Dialog/controluiconfig.cpp — its idx5 field-deserialize (retail
// 0x14005fb70) reads the command int (+0x20) and target component type (+0x24)
// that ControlUI needs. As a stub those stayed zero, so every ControlUI button
// read as command 0 against component type 0. Stub registration removed to
// avoid a duplicate REGISTER_TYPE.
// ResourceLimitUIConfig: PROMOTED to the real config class in
// code/Game/Dialog/resourcelimituiconfig.cpp (idx5 deserialize, retail 0x140077b10,
// fills +0x20 = this row's resource index). Stub registration removed to avoid a
// duplicate REGISTER_TYPE.
//
// CORRECTION (2026-07-31) to what stood here before: this comment used to claim
// that standalone "creates these config objects but never runs their
// field-deserialize, and controller+0x10 is always null", making the resource index
// unrecoverable — which is why ResourceLimitUI sourced it from dispatch order.
// That claim was FALSE. It described the symptom of the config being an all-no-op
// STUB (its idx5 was StubNop, so of course nothing was deserialized), not a missing
// framework binding. Promoting ControlUIConfig the same way immediately produced
// real values, proving the framework does create AND deserialize these configs.
// The dispatch-order workaround it justified was itself the cause of the Town Hall
// limits changing value across dialog open/close — see the IdxFor note above.
// StatusIconUIConfig: PROMOTED to the real config class in
// code/Game/Dialog/statusiconuiconfig.cpp. Its Serialize (vtable idx5, retail
// 0x14007b400) reads ONE bool byte (_showOnMainPage) off the resource stream;
// the generic stub's StubNop read zero, so the external-object load drifted and
// the title-bar mount's config never resolved — StatusIconUI saw config==NULL at
// +0x10 and could not suppress the duplicate stop-work icon. Exactly the same
// failure (and fix) as the ControlUIConfig / ResourceLimitUIConfig promotions
// noted above. Stub registration removed to avoid a duplicate REGISTER_TYPE.
// UpgradeUIConfig: PROMOTED to the real config class in
// code/Game/Dialog/upgradeuiconfig.cpp (Serialize deserializes the upgrade
// target ComponentEntity template into +0x20 — the zero-fill stub left it null,
// so the upgrade button could never resolve a target). Stub registration
// removed to avoid a duplicate REGISTER_TYPE(UpgradeUIConfig).

// ---- External-based tutorial dialogs (parents are abstract External bases) --

#undef UI_DIALOG_STUB

