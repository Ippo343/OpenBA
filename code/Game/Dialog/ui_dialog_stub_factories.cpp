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
#include "object/Internal.h"
#include <new>
// ENDGAME B.3.8 PNL: the DialogController classes this file registers.
#include "Dialog/AttendanceUI.h"
#include "Dialog/BuildRequirementUI.h"
#include "Dialog/BuildUI.h"
#include "Dialog/CemeteryUI.h"
#include "Dialog/ConsumeProduceUI.h"
#include "Dialog/HelpUI.h"
#include "Dialog/HotKeyToolTipUI.h"
#include "Dialog/ImmigrationUI.h"
#include "Dialog/MapUI.h"
#include "Dialog/ModBrowseUI.h"
#include "Dialog/NoiseUI.h"
#include "Dialog/PageToggleUI.h"
#include "Dialog/ProductionUI.h"
#include "Dialog/ProfessionUI.h"
#include "Dialog/ResourceLimitUI.h"
#include "Dialog/SpriteCaptureUI.h"
#include "Dialog/TitleDescToolTipUI.h"
#include "Dialog/ToolUI.h"
#include "Dialog/TradeUI.h"

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

extern void BuildRequirementUI_Slot4(BuildRequirementUI* self);
namespace uistub_BuildRequirementUI {
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
}  // namespace uistub_BuildRequirementUI

// [4] 0x14005e7c0. The real reimpl unconditionally dereferences these globals;
// if any failed to resolve, skip rather than fault (standalone-only).
void BuildRequirementUI::OnCreated() {
    uistub_BuildRequirementUI::EnsureTypeIds();
    if (!g_TypeId_ComponentDescription_FC || !g_TypeId_ComponentEntity
        || !g_TypeId_Element || !g_TypeId_ToolbarDescription_FC
        || !g_TypeId_ResourceComponent || !g_TypeId_RawMaterialDescription) return;
    BuildRequirementUI_Slot4(this);
}

// [0] 0x140010860: ~BuildRequirementUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* BuildRequirementUI::DeletingDtor(unsigned flags) {
    this->~BuildRequirementUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400107f0: MemAlloc(0x60, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* BuildRequirementUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BuildRequirementUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BuildRequirementUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* BuildRequirementUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BuildRequirementUI());
}

DEFINE_EXE_VTABLE(BuildRequirementUI, "_ZTV18BuildRequirementUI")
DEFINE_EXE_TYPE(BuildRequirementUI, "UI::DialogController")

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
}  // namespace uistub_HotKeyToolTipUI

// [4] 0x14006a980.
void HotKeyToolTipUI::OnCreated() {
    uistub_HotKeyToolTipUI::EnsureTypeIds();
    if (!g_TypeId_Element) return;
    Game::HotKeyToolTipUI_Slot4(this);
}

// [0] 0x140011e70: ~HotKeyToolTipUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* HotKeyToolTipUI::DeletingDtor(unsigned flags) {
    this->~HotKeyToolTipUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011e20: MemAlloc(0x30, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* HotKeyToolTipUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HotKeyToolTipUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HotKeyToolTipUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* HotKeyToolTipUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HotKeyToolTipUI());
}

DEFINE_EXE_VTABLE(HotKeyToolTipUI, "_ZTV15HotKeyToolTipUI")
DEFINE_EXE_TYPE(HotKeyToolTipUI, "UI::DialogController")

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

extern void ConsumeProduceUI_Slot4(ConsumeProduceUI* self);
extern void ConsumeProduceUI_Slot7(ConsumeProduceUI* self);
extern void ConsumeProduceUI_Slot9(ConsumeProduceUI* self, uint64_t p2, int p3, int p4);

extern void CemeteryUI_Slot4(CemeteryUI* self);
extern void CemeteryUI_Slot7(CemeteryUI* self);
extern void AttendanceUI_Slot4(AttendanceUI* self);
extern void AttendanceUI_Slot7(AttendanceUI* self);
namespace uistub_AttendanceUI {
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
}  // namespace uistub_AttendanceUI

// [4] 0x14005dd40 — find the ProgressElement, set its maximum (capacity).
void AttendanceUI::OnCreated() {
    uistub_AttendanceUI::EnsureTypeIds();
    if (!uistub_AttendanceUI::Ready()) return;
    AttendanceUI_Slot4(this);
}
// [7] 0x14005dc50 — per-frame: current value + text parameter 0.
void AttendanceUI::OnUpdateLayout() {
    uistub_AttendanceUI::EnsureTypeIds();
    if (!uistub_AttendanceUI::Ready()) return;
    AttendanceUI_Slot7(this);
}

// [0] 0x140010560: ~AttendanceUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* AttendanceUI::DeletingDtor(unsigned flags) {
    this->~AttendanceUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400104b0: MemAlloc(0x38, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* AttendanceUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AttendanceUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AttendanceUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* AttendanceUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AttendanceUI());
}

DEFINE_EXE_VTABLE(AttendanceUI, "_ZTV12AttendanceUI")
DEFINE_EXE_TYPE(AttendanceUI, "UI::DialogController")

// ---- DialogController family ------------------------------------------------
// CemeteryUI: PROMOTED from a bare UI_DIALOG_STUB 2026-08-23. The panel bodies in
// code/Game/Dialog/cemeteryui.cpp were fully ported but wired into NOTHING -- the stub
// vtable left slots 4 and 7 as no-ops, so neither ProgressElement text parameter was
// ever set and the panel rendered a literal "  of   graves" with both numbers blank.
// Same shape as AttendanceUI directly above.
namespace uistub_CemeteryUI {
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
}  // namespace uistub_CemeteryUI

// [4] 0x14005f090.
void CemeteryUI::OnCreated() {
    uistub_CemeteryUI::EnsureTypeIds();
    if (!uistub_CemeteryUI::Ready()) return;
    CemeteryUI_Slot4(this);
}
// [7] 0x14005ef50.
void CemeteryUI::OnUpdateLayout() {
    uistub_CemeteryUI::EnsureTypeIds();
    if (!uistub_CemeteryUI::Ready()) return;
    CemeteryUI_Slot7(this);
}

// [0] 0x1400109b0: ~CemeteryUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* CemeteryUI::DeletingDtor(unsigned flags) {
    this->~CemeteryUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140010900: MemAlloc(0x38, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* CemeteryUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CemeteryUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CemeteryUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* CemeteryUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CemeteryUI());
}

DEFINE_EXE_VTABLE(CemeteryUI, "_ZTV10CemeteryUI")
DEFINE_EXE_TYPE(CemeteryUI, "UI::DialogController")
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
}  // namespace uistub_ConsumeProduceUI

// [4] 0x14005f3d0 — also reached BY ADDRESS (engine_funcs.cpp:1306 through the
// reg_code_lane2.cpp door), which calls ConsumeProduceUI_Slot4 directly.
void ConsumeProduceUI::OnCreated() {
    uistub_ConsumeProduceUI::EnsureTypeIds();
    if (!uistub_ConsumeProduceUI::Ready()) return;
    ConsumeProduceUI_Slot4(this);
}
// [7] 0x14005f1f0.
void ConsumeProduceUI::OnUpdateLayout() {
    uistub_ConsumeProduceUI::EnsureTypeIds();
    if (!uistub_ConsumeProduceUI::Ready()) return;
    ConsumeProduceUI_Slot7(this);
}
// [9] 0x14005f330. The body takes the element pointer's ADDRESS as a qword.
void ConsumeProduceUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a,
                                      int32_t b, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/,
                                      void* /*userData*/) {
    uistub_ConsumeProduceUI::EnsureTypeIds();
    if (!uistub_ConsumeProduceUI::Ready()) return;
    ConsumeProduceUI_Slot9(this, reinterpret_cast<uint64_t>(element), a, b);
}

// [0] 0x140010b90: ~ConsumeProduceUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ConsumeProduceUI::DeletingDtor(unsigned flags) {
    this->~ConsumeProduceUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140010ae0: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ConsumeProduceUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ConsumeProduceUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ConsumeProduceUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ConsumeProduceUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ConsumeProduceUI());
}

DEFINE_EXE_VTABLE(ConsumeProduceUI, "_ZTV16ConsumeProduceUI")
DEFINE_EXE_TYPE(ConsumeProduceUI, "UI::DialogController")
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
// ---- HelpUI: the slot bodies and their standalone guards live in helpui.cpp.




// [0] 0x140011cf0: ~HelpUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* HelpUI::DeletingDtor(unsigned flags) {
    this->~HelpUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011ca0: MemAlloc(0x78, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* HelpUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HelpUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HelpUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* HelpUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HelpUI());
}

DEFINE_EXE_VTABLE(HelpUI, "_ZTV6HelpUI")
DEFINE_EXE_TYPE(HelpUI, "UI::DialogController")
// HotKeyToolTipUI promoted to a real Slot4 reimpl above (was a no-op stub).
// ---- ImmigrationUI: the slot bodies and their standalone guards live in immigrationui.cpp.




// [0] 0x140011fd0: ~ImmigrationUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ImmigrationUI::DeletingDtor(unsigned flags) {
    this->~ImmigrationUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140011ee0: MemAlloc(0x50, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ImmigrationUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ImmigrationUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ImmigrationUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ImmigrationUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ImmigrationUI());
}

DEFINE_EXE_VTABLE(ImmigrationUI, "_ZTV13ImmigrationUI")
DEFINE_EXE_TYPE(ImmigrationUI, "UI::DialogController")
// ---- MapUI: the slot bodies and their standalone guards live in mapui.cpp.



// [0] 0x1400124d0: ~MapUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* MapUI::DeletingDtor(unsigned flags) {
    this->~MapUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140012240: MemAlloc(0x50, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* MapUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MapUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MapUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* MapUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MapUI());
}

DEFINE_EXE_VTABLE(MapUI, "_ZTV5MapUI")
DEFINE_EXE_TYPE(MapUI, "UI::DialogController")
// ---- ModBrowseUI / NoiseUI: ENDGAME B.3.8 PNL — the real classes -------------
// Until B.3.8 these two were UI_DIALOG_STUB rows: the reflected factory built a
// 48-slot all-nop object (no base ctor, no base dtor) that was the ONLY
// registration of either name (B38 D4). The factory now builds the real class.
// Neither class's own retail bodies are wired yet: ModBrowseUI has none past
// its `RET 0` slot 3; NoiseUI's slot 4 / slot 9 bodies are ported in
// noiseui.cpp (NoiseUI_Slot4 / NoiseUI_Slot9) but were never reached through
// the stub, so they stay PORT GAPs here and wiring them is its own titled
// commit.

// [4] PORT GAP (retail 0x1400714c0; ported as NoiseUI_Slot4, unwired) — the
// tree's stub held a no-op; kept. B38 D3.2.
void NoiseUI::OnCreated() {}
// [9] PORT GAP (retail 0x1400714a0; ported as NoiseUI_Slot9, unwired) — kept a no-op.
void NoiseUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t /*a*/,
                             int32_t /*b*/, int32_t /*c*/, int32_t /*d*/, int32_t /*e*/,
                             void* /*userData*/) {}

// [0] 0x140012720: ~ModBrowseUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ModBrowseUI::DeletingDtor(unsigned flags) {
    this->~ModBrowseUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400126d0: MemAlloc(0x30, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ModBrowseUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ModBrowseUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ModBrowseUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ModBrowseUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ModBrowseUI());
}

DEFINE_EXE_VTABLE(ModBrowseUI, "_ZTV11ModBrowseUI")
DEFINE_EXE_TYPE(ModBrowseUI, "UI::DialogController")

// [0] 0x140012da0: ~NoiseUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* NoiseUI::DeletingDtor(unsigned flags) {
    this->~NoiseUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140012d30: MemAlloc(0x70, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* NoiseUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NoiseUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) NoiseUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* NoiseUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) NoiseUI());
}

DEFINE_EXE_VTABLE(NoiseUI, "_ZTV7NoiseUI")
DEFINE_EXE_TYPE(NoiseUI, "UI::DialogController")
// ---- PageToggleUI: the slot bodies and their standalone guards live in pagetoggleui.cpp.




// [0] 0x1400131c0: ~PageToggleUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* PageToggleUI::DeletingDtor(unsigned flags) {
    this->~PageToggleUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140013160: MemAlloc(0x48, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* PageToggleUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PageToggleUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PageToggleUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* PageToggleUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PageToggleUI());
}

DEFINE_EXE_VTABLE(PageToggleUI, "_ZTV12PageToggleUI")
DEFINE_EXE_TYPE(PageToggleUI, "UI::DialogController")

// ---- ProductionUI: the slot bodies and their standalone guards live in productionui.cpp.



// [0] 0x1400132b0: ~ProductionUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ProductionUI::DeletingDtor(unsigned flags) {
    this->~ProductionUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140013230: MemAlloc(0x50, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ProductionUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ProductionUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ProductionUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ProductionUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ProductionUI());
}

DEFINE_EXE_VTABLE(ProductionUI, "_ZTV12ProductionUI")
DEFINE_EXE_TYPE(ProductionUI, "UI::DialogController")
// ---- ProfessionUI: the slot bodies and their standalone guards live in professionui.cpp.




// [0] 0x140013450: ~ProfessionUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ProfessionUI::DeletingDtor(unsigned flags) {
    this->~ProfessionUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140013400: MemAlloc(0x78, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ProfessionUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ProfessionUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ProfessionUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ProfessionUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ProfessionUI());
}

DEFINE_EXE_VTABLE(ProfessionUI, "_ZTV12ProfessionUI")
DEFINE_EXE_TYPE(ProfessionUI, "UI::DialogController")
// ---- RadiusUI: PROMOTED to the real reimpl (2026-08-15) ---------------------
// Moved to code/Game/Dialog/radiusui_promote.cpp, which carries the REGISTER_TYPE
// (never delete a stub REGISTER_TYPE — some are the only filler of a live type-id
// global). The stub's 48 no-op slots left idx5/idx6 dead, so the coverage-radius
// overlay never appeared on any radius building's info panel. It also zeroed +0x30,
// which the real Factory seeds to -1.

// ---- ResourceLimitUI: the slot bodies and their standalone guards live in resourcelimitui.cpp.




// [0] 0x1400138d0: ~ResourceLimitUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ResourceLimitUI::DeletingDtor(unsigned flags) {
    this->~ResourceLimitUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400136b0: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ResourceLimitUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ResourceLimitUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ResourceLimitUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ResourceLimitUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ResourceLimitUI());
}

DEFINE_EXE_VTABLE(ResourceLimitUI, "_ZTV15ResourceLimitUI")
DEFINE_EXE_TYPE(ResourceLimitUI, "UI::DialogController")
// ---- SpriteCaptureUI: ENDGAME B.3.8 PNL — the real class --------------------
// Was a UI_DIALOG_STUB row (48-slot all-nop object, the only registration of
// the name; B38 D4). Its slot 4 / 8 bodies are ported in spritecaptureui.cpp
// (SpriteCaptureUI_Serialize / SpriteCaptureUI_OnCreate) but were never reached
// through the stub; they stay PORT GAPs (no-ops, as the stub had them) and
// wiring them is its own titled commit.

// [4] PORT GAP (retail 0x140079640; ported as SpriteCaptureUI_Serialize, unwired).
void SpriteCaptureUI::OnCreated() {}
// [8] PORT GAP (retail 0x1400793a0; ported as SpriteCaptureUI_OnCreate, unwired).
void SpriteCaptureUI::OnUpdate(const System::Time& /*time*/) {}
// [9] PORT GAP (retail 0x140079620; not ported).
void SpriteCaptureUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/,
                                     int32_t /*a*/, int32_t /*b*/, int32_t /*c*/,
                                     int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {}

// [0] 0x140013fc0: ~SpriteCaptureUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* SpriteCaptureUI::DeletingDtor(unsigned flags) {
    this->~SpriteCaptureUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140013d10: MemAlloc(0x70, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* SpriteCaptureUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SpriteCaptureUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) SpriteCaptureUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* SpriteCaptureUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) SpriteCaptureUI());
}

DEFINE_EXE_VTABLE(SpriteCaptureUI, "_ZTV15SpriteCaptureUI")
DEFINE_EXE_TYPE(SpriteCaptureUI, "UI::DialogController")
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


// [0] 0x140014510: ~TitleDescToolTipUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* TitleDescToolTipUI::DeletingDtor(unsigned flags) {
    this->~TitleDescToolTipUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400144c0: MemAlloc(0x30, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* TitleDescToolTipUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TitleDescToolTipUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TitleDescToolTipUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* TitleDescToolTipUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TitleDescToolTipUI());
}

DEFINE_EXE_VTABLE(TitleDescToolTipUI, "_ZTV18TitleDescToolTipUI")
DEFINE_EXE_TYPE(TitleDescToolTipUI, "UI::DialogController")
// ---- ToolUI: the slot bodies and their standalone guards live in toolui.cpp.




// [0] 0x1400145f0: ~ToolUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* ToolUI::DeletingDtor(unsigned flags) {
    this->~ToolUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140014580: MemAlloc(0x58, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* ToolUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ToolUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ToolUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* ToolUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ToolUI());
}

DEFINE_EXE_VTABLE(ToolUI, "_ZTV6ToolUI")
DEFINE_EXE_TYPE(ToolUI, "UI::DialogController")
// ---- TownHallUI: PROMOTED to real SA vtable (townhallui.cpp) — the town-hall
//      General statistics / Professions / Production / Inventory / Graph /
//      Agriculture panel now populates from the full reimplementation. ----
// ---- TradeUI: the slot bodies and their standalone guards live in tradeui.cpp.








// [0] 0x140016020: ~TradeUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* TradeUI::DeletingDtor(unsigned flags) {
    this->~TradeUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140015f80: MemAlloc(0x148, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* TradeUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TradeUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TradeUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* TradeUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TradeUI());
}

DEFINE_EXE_VTABLE(TradeUI, "_ZTV7TradeUI")
DEFINE_EXE_TYPE(TradeUI, "UI::DialogController")
// UpgradeUI: PROMOTED to a real per-class standalone vtable in
// code/Game/Dialog/upgradeui.cpp (wires Slot4/8/9 — the wood→stone upgrade
// button; click writes the target template into DestroyComponent+0x60 and
// pumps the BuildState state machine). Stub registration removed to avoid a
// duplicate REGISTER_TYPE(UpgradeUI).
// ---- WorkPlaceUI: PROMOTED to real SA vtable (workplaceui.cpp) — the construction
//      workplace spinner panel now self-hides once a building is complete (its
//      profession clears), so finished houses/barns/piles show only center/pin/close
//      + their residence/storage content instead of a stale worker up/down spinner. ----
// ---- BuildUI: the slot bodies and their standalone guards live in buildui.cpp.




// [0] 0x140020a90: ~BuildUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* BuildUI::DeletingDtor(unsigned flags) {
    this->~BuildUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400209e0: MemAlloc(0x50, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* BuildUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BuildUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BuildUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* BuildUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BuildUI());
}

DEFINE_EXE_VTABLE(BuildUI, "_ZTV7BuildUI")
DEFINE_EXE_TYPE(BuildUI, "UI::DialogController")
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
#include "Dialog/ToolbarDialog.h"
#include "Dialog/ModUploadDialog.h"
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
DEFINE_EXE_TYPE(ModUploadDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(ScenarioDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(SaveLoadDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)
DEFINE_EXE_TYPE(ToolbarDialog, "UI::DialogInstance")   // was a UI_DIALOG_STUB twin (D4)

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

