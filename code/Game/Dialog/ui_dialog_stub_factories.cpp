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
// code/Game/Dialog/<type>.cpp (used by DLL-hook mode); this file only supplies
// the standalone construction/registration the exe ctors used to provide.

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

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
// composes the title (BuildRequirementComponent+0x38 name key), the icon, and the
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
    // reimpl's first `*g_TypeId_BuildComponent` would fault. Fill them from the live
    // Factory tree (post-Engine::Startup), like ToolbarStandalone_Init. Idempotent.
    // NB: g_TypeId_ResourceComponent's engine_funcs label is misleading — the id at
    // .rdata 0x1401c7040 is actually "BuildDescription"; and Object::Internal is
    // registered under the simple name "Internal".
    void EnsureTypeIds() {
        static uint32_t s_build=0, s_entity=0, s_internal=0, s_buildDesc=0, s_rawMat=0;
        if (!g_TypeId_BuildComponent)        { s_build=::Object::ResolveTypeIdByNameLazy("BuildComponent");        if(s_build) g_TypeId_BuildComponent=&s_build; }
        if (!g_TypeId_ComponentEntity)       { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");      if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_ResourceComponent)     { s_buildDesc=::Object::ResolveTypeIdByNameLazy("BuildDescription");  if(s_buildDesc) g_TypeId_ResourceComponent=&s_buildDesc; }
        if (!g_TypeId_RawMaterialDescription){ s_rawMat=::Object::ResolveTypeIdByNameLazy("RawMaterialDescription"); if(s_rawMat) g_TypeId_RawMaterialDescription=&s_rawMat; }
        if (!g_TypeId_ObjectInternal)        { s_internal=::Object::ResolveTypeIdByNameLazy("Internal");           if(s_internal) g_TypeId_ObjectInternal=&s_internal; }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        // The real reimpl unconditionally dereferences these globals; if any failed
        // to resolve, skip rather than fault.
        if (!self || !g_TypeId_BuildComponent || !g_TypeId_ComponentEntity
            || !g_TypeId_ObjectInternal || !g_TypeId_BuildRequirementComponent
            || !g_TypeId_ResourceComponent || !g_TypeId_RawMaterialDescription) return;
        BuildRequirementUI_Slot4(reinterpret_cast<BuildRequirementUI*>(self));
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));
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
    // The reimpl unconditionally dereferences g_TypeId_ObjectInternal; in
    // standalone it resolves from a raw exe .rdata addr -> null. Lazy-fill it
    // from the live Factory tree (post-Engine::Startup). NB: Object::Internal is
    // registered under the simple name "Internal". Idempotent.
    void EnsureTypeIds() {
        static uint32_t s_internal=0;
        if (!g_TypeId_ObjectInternal) {
            s_internal=::Object::ResolveTypeIdByNameLazy("Internal");
            if (s_internal) g_TypeId_ObjectInternal=&s_internal;
        }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !g_TypeId_ObjectInternal) return;
        Game::HotKeyToolTipUI_Slot4(self);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));
    struct Slot4Wire { Slot4Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); } } g_s4;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x30;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x30,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(HotKeyToolTipUI, "UI::DialogController",
              &uistub_HotKeyToolTipUI::Factory, &uistub_HotKeyToolTipUI::InPlace);

// ---- DialogController family ------------------------------------------------
UI_DIALOG_STUB(AttendanceUI,        "UI::DialogController", 0x38);
UI_DIALOG_STUB(CemeteryUI,          "UI::DialogController", 0x38);
UI_DIALOG_STUB(ConsumeProduceUI,    "UI::DialogController", 0x40);
UI_DIALOG_STUB(ControlUI,           "UI::DialogController", 0x40);
UI_DIALOG_STUB(DebugUI,             "DialogController",     0x130);
// CitizenUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/citizenui.cpp
// (wires Slot4/6/7/9 so the clicked-citizen panel shows name/age/profession/hearts).
UI_DIALOG_STUB(DestroyUI,           "UI::DialogController", 0x78);
// EntityUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/entityui.cpp
// (wires Slot4 so the entity panel gets its +0x30 entity ref → correct positioning).
// EventLogUI: PROMOTED to a real per-class standalone vtable in code/Game/Dialog/eventlogui.cpp
// (wires Slot5/7/9 + per-frame OnUpdate so the event-log panel populates from gameWorld+0x138).
UI_DIALOG_STUB(HelpUI,              "UI::DialogController", 0x78);
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
    // fn_FindComponent_BuildState (DAT_1401c7d98, whose true identity is
    // StateMachineComponent — the engine_funcs label is a historical mislabel, see
    // [[project_townhallui_promoted]]). Fill from the live factory tree post-Engine::
    // Startup; Entity_FindComponentByExeTypeId derefs the id ptr, so it must be live.
    void EnsureGlobals() {
        static uint32_t s_entity=0, s_immig=0, s_bstate=0;
        if (!g_TypeId_ComponentEntity)      { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");        if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_ImmigrationComponent) { s_immig =::Object::ResolveTypeIdByNameLazy("ImmigrationComponent");   if(s_immig)  g_TypeId_ImmigrationComponent=&s_immig; }
        if (!g_TypeId_BuildStateComponent)  { s_bstate=::Object::ResolveTypeIdByNameLazy("StateMachineComponent");  if(s_bstate) g_TypeId_BuildStateComponent=&s_bstate; }
    }
    bool Ready() {
        return g_TypeId_ComponentEntity && g_TypeId_ImmigrationComponent &&
               g_TypeId_BuildStateComponent &&
               g_pGameWorldSingleton && *g_pGameWorldSingleton;
    }
    void Trace(const char* slot, void* self) {
        if (!std::getenv("BANISHED_NOMAD_TRACE")) return;
        static int n=0; if (n>=16) return; ++n;
        void* ent = self ? *reinterpret_cast<void**>((uintptr_t)self+0x28) : nullptr;
        printf("[ImmigrationUI] %s self=%p entity(+0x28)=%p dlg(+0x18)=%p\n", slot, self, ent,
               self ? *reinterpret_cast<void**>((uintptr_t)self+0x18) : nullptr);
        fflush(stdout);
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
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
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
        if (!s_logged && std::getenv("BANISHED_MAP_TRACE")) {
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
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x50;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x50,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(MapUI, "UI::DialogController",
              &uistub_MapUI::Factory, &uistub_MapUI::InPlace);
UI_DIALOG_STUB(ModBrowseUI,         "UI::DialogController", 0x30);
UI_DIALOG_STUB(NoiseUI,             "UI::DialogController", 0x70);
UI_DIALOG_STUB(PageToggleUI,        "UI::DialogController", 0x48);
UI_DIALOG_STUB(ProductionUI,        "UI::DialogController", 0x50);
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
        if (!std::getenv("BANISHED_PROFESSION_TRACE")) return;
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
    struct Wire { Wire(){ g_vt[5]=reinterpret_cast<void*>(&Slot5);
                          g_vt[8]=reinterpret_cast<void*>(&Slot8);
                          g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x78;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x78,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ProfessionUI, "UI::DialogController",
              &uistub_ProfessionUI::Factory, &uistub_ProfessionUI::InPlace);
UI_DIALOG_STUB(RadiusUI,            "UI::DialogController", 0x38);
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
    // Dispatch-order -> resource index. The framework never binds the child
    // controller's datasource (self+0x10) in standalone, so the retail Slot4/7/9
    // (which read the resource index from config+0x20) would IsDerivedFrom(NULL)
    // and fault. Instead we number the 11 row controllers by the order the
    // framework sets them up, which equals the .rsc/table order == resource index
    // (Log=0..Textile=10). One registry per dialog build: a 12th distinct `self`
    // means a fresh build, so wrap back to 0.
    static void* g_selves[11];
    static int   g_count = 0;
    int IdxFor(void* self) {
        for (int i=0;i<g_count;i++) if (g_selves[i]==self) return i;
        if (g_count >= 11) { for (int i=0;i<11;i++) g_selves[i]=nullptr; g_count=0; }
        g_selves[g_count]=self;
        return g_count++;
    }
    void Trace(const char* slot, void* self, int idx) {
        if (!std::getenv("BANISHED_LIMIT_TRACE")) return;
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
    struct Wire { Wire(){ g_vt[4]=reinterpret_cast<void*>(&Slot4); g_vt[7]=reinterpret_cast<void*>(&Slot7); g_vt[9]=reinterpret_cast<void*>(&Slot9); } } g_wire;
    void Zero(void* p){ for(size_t o=0;o<(size_t)0x40;o+=8) *reinterpret_cast<uint64_t*>((char*)p+o)=0; }
    void* Factory(){ if(!fn_MemAlloc||!g_pMemAllocSingleton) return nullptr; void* p=fn_MemAlloc(g_pMemAllocSingleton,(uint64_t)0x40,8); if(!p) return nullptr; Zero(p); *reinterpret_cast<void**>(p)=g_vt; return p; }
    void* InPlace(void* m){ if(!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m)=g_vt; return m; }
}
REGISTER_TYPE(ResourceLimitUI, "UI::DialogController",
              &uistub_ResourceLimitUI::Factory, &uistub_ResourceLimitUI::InPlace);
UI_DIALOG_STUB(SpriteCaptureUI,     "UI::DialogController", 0x70);
// ---- StatusBarUI: PROMOTED to real SA vtable (statusbarui.cpp) — the toolbar
//      "town status" utility dialog (group 4, next to Map) now renders the town
//      name title + population/resources/weather/hearts/stars instead of zeros ----
// ---- StatusIconUI: PROMOTED to real SA vtable (statusiconui.cpp) — icons now
//      show/hide per StatusIconComponent +0xF0 state instead of all-at-once ----
// ---- StorageCapacityUI: PROMOTED to real SA vtable (storagecapacityui.cpp) — the
//      storage building's "X%" fill number now computes+renders instead of staying
//      blank (Serialize finds the ProgressElement, Startup sets count/capacity %). ----
UI_DIALOG_STUB(TitleDescToolTipUI,  "UI::DialogController", 0x30);
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
        // NB: g_TypeId_BuildComponent maps to exe .rdata 0x1401c7318 whose real
        // identity is ComponentDescription (verified: written by
        // ComponentDescription_Register), NOT BuildComponent — the engine_funcs
        // label is misleading. ToolUI::Slot4 branch-1 tests the datasource against
        // THIS id (a utility dialog's datasource is a ComponentDescription), so it
        // must be filled with the ComponentDescription type-id.
        if (!g_TypeId_BuildComponent)            { s_build =::Object::ResolveTypeIdByNameLazy("ComponentDescription");      if(s_build)  g_TypeId_BuildComponent=&s_build; }
        if (!g_TypeId_ComponentEntity)           { s_entity=::Object::ResolveTypeIdByNameLazy("ComponentEntity");           if(s_entity) g_TypeId_ComponentEntity=&s_entity; }
        if (!g_TypeId_BuildRequirementComponent) { s_breq  =::Object::ResolveTypeIdByNameLazy("BuildRequirementComponent"); if(s_breq)   g_TypeId_BuildRequirementComponent=&s_breq; }
        if (!g_TypeId_UIDescription)             { s_uidesc=::Object::ResolveTypeIdByNameLazy("UIDescription");             if(s_uidesc) g_TypeId_UIDescription=&s_uidesc; }
        if (!g_TypeId_TabElement)                { s_tab   =::Object::ResolveTypeIdByNameLazy("TabElement");                if(s_tab)    g_TypeId_TabElement=&s_tab; }
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        // The reimpl unconditionally dereferences these; skip rather than fault if
        // any failed to resolve.
        if (!self || !g_TypeId_BuildComponent || !g_TypeId_ComponentEntity
            || !g_TypeId_BuildRequirementComponent || !g_TypeId_UIDescription
            || !g_TypeId_TabElement) return;
        static bool s_logged=false;
        if(!s_logged && std::getenv("BANISHED_MAP_TRACE")){ s_logged=true;
            printf("[ToolUI::Slot4] dispatched self=%p ds(+0x28)=%p\n",
                   self, *reinterpret_cast<void**>((char*)self+0x28)); fflush(stdout); }
        ToolUI_Slot4(reinterpret_cast<ToolUI*>(self));
    }
    // Per-frame update (+0x40). Runtime DialogInstance::OnUpdate dispatches
    // (controller, Time&); the reimpl ignores the Time arg. Idle cost is two flag
    // reads (+0x41/+0x4D, both 0 unless Slot9 armed a drag). Guard the KeyMap
    // exref + the Runtime fns the drag path dereferences unconditionally.
    void Slot8(void* self, void* /*time*/) {
        if (!self || !g_pKeyMapObjectExref || !*g_pKeyMapObjectExref
            || !fn_KeyMap_GetValue || !fn_DialogInstance_SetUpperLeft
            || !g_pGameWorldSingleton || !*g_pGameWorldSingleton) return;
        ToolUI_Slot8(reinterpret_cast<ToolUI*>(self));
    }
    // OnElementEvent (+0x48). Dispatched with (self, &elementPtr, msg, id, x, y, ...);
    // the real handler closes the dialog on the buttonClose (id 1, msg 0x12).
    void Slot9(void* self, void* p2, int msg, int id, int mx, int my) {
        if (self) ToolUI_Slot9(reinterpret_cast<ToolUI*>(self), p2, msg, id, mx, my);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));
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
UI_DIALOG_STUB(TradeUI,             "UI::DialogController", 0x148);
UI_DIALOG_STUB(UpgradeUI,           "UI::DialogController", 0x38);
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
        if (!g_TypeId_PositionComponent)      { s_build=::Object::ResolveTypeIdByNameLazy("BuildComponent");         if(s_build) g_TypeId_PositionComponent=&s_build; }
        if (!g_TypeId_SMComponent)            { s_clear=::Object::ResolveTypeIdByNameLazy("ClearAreaComponent");      if(s_clear) g_TypeId_SMComponent=&s_clear; }
    }
    // Slot4 has run once self+0x40 (the "progress" element IBP) is populated.
    inline bool IsSetUp(void* self) { return self && *reinterpret_cast<uint64_t*>((uintptr_t)self + 0x40) != 0; }
    bool HardIdsReady() {
        return g_TypeId_ComponentEntity && g_TypeId_ResourceComponent && g_TypeId_RawMaterialDescription;
    }
    void Slot4(void* self) {
        EnsureTypeIds();
        if (!self || !HardIdsReady()) return;
        // Idempotent: the framework dispatches setup on attach; a re-drive would
        // append a second copy of the storage rows to self+0x30 (count never reset)
        // — exactly the ProfessionUI double-populate class. Skip if already set up.
        if (IsSetUp(self)) return;
        BuildUI_Slot4(reinterpret_cast<BuildUI*>(self));
#if defined(_DEBUG)
        if (const char* e=std::getenv("BANISHED_BUILDUI_TRACE"); e && e[0] && e[0]!='0') {
            static int n=0; if(n<8){++n;
                fprintf(stderr,"[BuildUI] Slot4 self=%p progress(+0x40)=%p rows(+0x30)=%p rowCount(+0x38)=%d\n",
                        self, *reinterpret_cast<void**>((uintptr_t)self+0x40),
                        *reinterpret_cast<void**>((uintptr_t)self+0x30),
                        *reinterpret_cast<int*>((uintptr_t)self+0x38)); fflush(stderr);
            }
        }
#endif
    }
    void Slot7(void* self) {
        EnsureTypeIds();
        // Per-frame update: the framework fires this every tick, including before
        // Slot4 populates (self+0x40 progress / self+0x30 rows still null). Guard.
        if (!IsSetUp(self) || !HardIdsReady()) return;
        BuildUI_Slot7(reinterpret_cast<BuildUI*>(self));
    }
    void Slot9(void* self, uint64_t p2, int p3, int p4) {
        EnsureTypeIds();
        if (!self || !g_TypeId_ComponentEntity) return;
        BuildUI_Slot9(reinterpret_cast<BuildUI*>(self), p2, p3, p4);
    }
    StubVtableInit g_init(g_vt, reinterpret_cast<void*>(&Dtor), reinterpret_cast<void*>(&GetTypeId));
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
        if (const char* e=std::getenv("BANISHED_BUILDUI_TRACE"); e && e[0] && e[0]!='0') {
            static int n=0; if(n<8){++n; fprintf(stderr,"[BuildUI] Factory -> %p\n",p); fflush(stderr);} }
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
UI_DIALOG_STUB(TutorialUI,          "UI::DialogController", 0x60);

// ---- SelectResourceUI subclasses (parent is the abstract SelectResourceUI) --
UI_DIALOG_STUB(OrchardUI,           "SelectResourceUI",     0x48);
UI_DIALOG_STUB(CropUI,              "SelectResourceUI",     0x60);
UI_DIALOG_STUB(PastureUI,           "SelectResourceUI",     0x40);

// ---- DialogInstance family --------------------------------------------------
UI_DIALOG_STUB(AchievementDialog,   "UI::DialogInstance",   0xD0);
UI_DIALOG_STUB(AutoSaveDialog,      "UI::DialogInstance",   0xD0);
// EventNotifyDialog / EventPopupDialog: PROMOTED to real per-class standalone vtables
// in code/Game/Dialog/eventnotifydialog.cpp / eventpopupdialog.cpp (ribbon + modal popup).
UI_DIALOG_STUB(FollowDialog,        "UI::DialogInstance",   0xD8);
UI_DIALOG_STUB(GoalDialog,          "UI::DialogInstance",   0xC8);
UI_DIALOG_STUB(MapParamsDialog,     "UI::DialogInstance",   0xF8);
UI_DIALOG_STUB(ModDetailsDialog,    "UI::DialogInstance",   0xC8);
UI_DIALOG_STUB(ModDialog,           "UI::DialogInstance",   0xD8);
UI_DIALOG_STUB(ModUploadDialog,     "UI::DialogInstance",   0xB8);
UI_DIALOG_STUB(ScenarioDialog,      "UI::DialogInstance",   0xE8);
UI_DIALOG_STUB(SaveLoadDialog,      "UI::DialogInstance",   0x160);
UI_DIALOG_STUB(ToolbarDialog,       "UI::DialogInstance",   0x108);
UI_DIALOG_STUB(TutorialHighlightDialog, "UI::DialogInstance", 0xF0);

// ---- DialogControllerConfig family (all 0x28) -------------------------------
UI_DIALOG_STUB(ControlUIConfig,       "UI::DialogControllerConfig", 0x28);
// ResourceLimitUIConfig stays a generic stub: standalone creates these config
// objects (Factory fires) but never runs their field-deserialize (Serialize) nor
// links them to the row controllers, so config+0x20 (resource index) is always 0
// and controller+0x10 (config) is always null. The resource index is therefore
// unrecoverable from the config in standalone — the ResourceLimitUI promotion
// above sources it from dispatch order (== .rsc/table order == resource index)
// instead. See the "Standalone entry points" note in
// code/Game/Dialog/resourcelimitui.cpp.
UI_DIALOG_STUB(ResourceLimitUIConfig, "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(StatusIconUIConfig,    "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(UpgradeUIConfig,       "UI::DialogControllerConfig", 0x28);

// ---- External-based tutorial dialogs (parents are abstract External bases) --
UI_DIALOG_STUB(TutorialGoalDialog,  "TutorialGoal",         0x40);
UI_DIALOG_STUB(TutorialSetupDialog, "TutorialSetup",        0x78);

#undef UI_DIALOG_STUB

#endif  // STANDALONE_EXE
