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
UI_DIALOG_STUB(EventLogUI,          "UI::DialogController", 0x80);
UI_DIALOG_STUB(HelpUI,              "UI::DialogController", 0x78);
// HotKeyToolTipUI promoted to a real Slot4 reimpl above (was a no-op stub).
UI_DIALOG_STUB(ImmigrationUI,       "UI::DialogController", 0x50);
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
UI_DIALOG_STUB(ProfessionUI,        "UI::DialogController", 0x78);
UI_DIALOG_STUB(RadiusUI,            "UI::DialogController", 0x38);
UI_DIALOG_STUB(ResourceLimitUI,     "UI::DialogController", 0x40);
UI_DIALOG_STUB(SpriteCaptureUI,     "UI::DialogController", 0x70);
UI_DIALOG_STUB(StatusBarUI,         "UI::DialogController", 0x1D0);
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
UI_DIALOG_STUB(TownHallUI,          "UI::DialogController", 0x1E8);
UI_DIALOG_STUB(TradeUI,             "UI::DialogController", 0x148);
UI_DIALOG_STUB(UpgradeUI,           "UI::DialogController", 0x38);
// ---- WorkPlaceUI: PROMOTED to real SA vtable (workplaceui.cpp) — the construction
//      workplace spinner panel now self-hides once a building is complete (its
//      profession clears), so finished houses/barns/piles show only center/pin/close
//      + their residence/storage content instead of a stale worker up/down spinner. ----
UI_DIALOG_STUB(BuildUI,             "UI::DialogController", 0x50);
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
UI_DIALOG_STUB(EventNotifyDialog,   "UI::DialogInstance",   0xF0);
UI_DIALOG_STUB(EventPopupDialog,    "UI::DialogInstance",   0xC0);
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
UI_DIALOG_STUB(ResourceLimitUIConfig, "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(StatusIconUIConfig,    "UI::DialogControllerConfig", 0x28);
UI_DIALOG_STUB(UpgradeUIConfig,       "UI::DialogControllerConfig", 0x28);

// ---- External-based tutorial dialogs (parents are abstract External bases) --
UI_DIALOG_STUB(TutorialGoalDialog,  "TutorialGoal",         0x40);
UI_DIALOG_STUB(TutorialSetupDialog, "TutorialSetup",        0x78);

#undef UI_DIALOG_STUB

#endif  // STANDALONE_EXE
