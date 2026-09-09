// engine/src/ui/statusiconui.cpp
// Clean reimplementation of StatusIconUI game logic.
//
// StatusIconUI (0x40 bytes) — status icon grid panel.
// Extends DialogController. data30 = icon element array (ptr),
// data38 = icon element count.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// By-name StatusIconComponent finder, defined in the STANDALONE promotion block at the
// bottom of this file. StatusIconUI_Startup calls it as a fallback because the exe
// component finder (Entity_FindComponent_MeshInstance, 0x140002f90) keys off the
// StatusIconComponent type-id .rdata global DAT_1401c7e08, which is UNFILLED (0xFFFFFFFF)
// in the standalone image and therefore matches no component for any entity.
static void* StatusIconUI_FindStatusIconComp(void* entity);

// Type ids for the two guards in Startup. BOTH were previously wrong, and the two errors
// compounded: the outer one made the page gate unreachable, and the inner one would have
// kept it broken even after the outer was fixed.
//
//   self+0x10 is a StatusIconUIConfig — retail reads DAT_1401c56e0, and get_xrefs_to
//   confirms that slot is written by StatusIconUIConfig_Register (0x14014c865). It is NOT
//   DialogController; engine_funcs.h:608 mislabels 0x1401c56e0 as such.
//
//   The dialog's controller 0 is an EntityUI — retail reads DAT_1401c4f00, written by
//   RegisterType_EntityUI (0x14014bf65). It is NOT PageToggleUI; that name is a legacy
//   misnomer already flagged in code/Game/Player/edittool.cpp:288 and used correctly in
//   edittool_select.cpp:151, but the correction never reached this file.
//
// Resolved by NAME here rather than through the mislabelled g_TypeId_* globals, which are
// left untouched because engine_funcs.h:4585 still consumes g_TypeId_PageToggleUI.
static uint32_t s_tidStatusIconUIConfig = 0;
static uint32_t s_tidEntityUI = 0;

// ============================================================
// StatusIconUI::Serialize (vtable +0x20) — Cache icon child elements
// ============================================================
// Finds the "icons" container element, gets its child count,
// resizes the internal array, and copies all child InternalPtrs
// into the array for fast access during Startup.
//
// Original: 0x14007b270.

void StatusIconUI_Serialize(StatusIconUI* self) {
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find "icons" container element by name
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "icons");

    uint64_t findResult = 0;
    // fn_FindElement: member with non-trivial return: RCX=this, RDX=retval, R8=name
    fn_FindElement(dialogElem, &findResult, nameBuf);

    // Dereference InternalPtr result to get raw Element*
    // The original does IsDerivedFrom check here, but we skip it
    // since FindElement by name always returns a valid element.
    void* rawElem = (void*)findResult;
    uint8_t iconsBuf[8];
    fn_InternalBasePtr_Ctor(iconsBuf, rawElem);
    void* iconsElem = *(void**)iconsBuf;

    fn_String_Dtor(nameBuf);

    // Get child count
    int childCount = fn_Element_ChildCount(iconsElem);

    // Resize the array at self+0x30 to hold childCount entries
    // FUN_1400032e0(arrayBase, newCount) — sets up the array
    fn_ExeArrayResize((void*)((uintptr_t)self + 0x30), childCount);

    // Copy each child's InternalPtr (8 bytes) into the array
    int storedCount = *(int*)((uintptr_t)self + 0x38);
    for (int i = 0; i < storedCount; i++) {
        // Element::Child returns InternalPtr<Element> via non-trivial return
        uint64_t childResult = 0;
        fn_Element_Child(iconsElem, &childResult, i);

        // Copy 8-byte InternalPtr into array slot
        uint64_t* arrayBase = *(uint64_t**)((uintptr_t)self + 0x30);
        arrayBase[i] = childResult;
    }
}

// ============================================================
// StatusIconUI::Startup (vtable +0x38) — Update icon visibility
// ============================================================
// Reads a bitfield from the MeshInstance component (+0xF0),
// checks the active page of the PageToggleUI controller,
// and shows/hides each icon element accordingly.
//
// Original: 0x14007b0a0.

void StatusIconUI_Startup(StatusIconUI* self) {
    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find the StatusIconComponent (the exe finder is named "MeshInstance" but it keys
    // off type-id DAT_1401c7e08 = StatusIconComponent; its +0xF0 word is the status-icon
    // active bitfield — the SAME bits that drive the 3D warning billboards).
    void* meshComp = nullptr;
    fn_FindComponent_MeshInstance(entityInternal, &meshComp);
    // Standalone: the exe finder's type-id global is unfilled → returns null for every
    // entity. Resolve the StatusIconComponent by name instead so the panel reads live bits.
    if (!meshComp) meshComp = StatusIconUI_FindStatusIconComp(entityInternal);

    // Determine if icons should be visible based on page state.
    //
    // A workplace-building dialog mounts this SAME "icons" ribbon TWICE (see
    // ToolKit/resource/Template/Pasture.rsc:262-273): once into "userGroup1" (the
    // content area, bottom right) with NO config, and once into "userTitle2" (the
    // TITLE BAR) with a config — e.g. CropField.rsc:256 `StatusIconUIConfig
    // titleBuildStatus { bool _showOnMainPage = false; }`. The config's +0x20 byte is
    // the ONLY thing that keeps the title-bar copy off the main info page, so getting
    // this gate wrong renders the stop-work icon twice.
    bool iconsActive = true;

    // self+0x10 is the optional per-mount StatusIconUIConfig, NOT a DialogController.
    void* config = *(void**)((uintptr_t)self + 0x10);
    if (config != nullptr) {
        bool isCfg = s_tidStatusIconUIConfig &&
                     fn_IsDerivedFrom(config, s_tidStatusIconUIConfig);
        void* cfgPtr = isCfg ? config : nullptr;

        // Retail reads *(char*)(cfg+0x20) — the `_showOnMainPage` BOOL — and enters the
        // page restriction when it is FALSE. (This was previously coded as a
        // *pointer*-null test on +0x20, which is a different question entirely.)
        if (cfgPtr && *(uint8_t*)((uintptr_t)cfgPtr + 0x20) == 0) {
            void* dialogInst = *(void**)((uintptr_t)self + 0x18);
            uint64_t ctrlResult = 0;
            // Third arg is a TYPE FILTER. Retail (0x14007b14b) is
            //   MOV R8D,[0x1401c4f00]   ; EntityUI type id
            //   CALL [0x140151768]      ; DialogInstance::GetController
            // Ghidra renders this call with two arguments and hides the filter, which
            // is how a 0 got here. With 0 the call returns nothing, the EntityUI narrow
            // below can never fire, and the page gate silently falls through to its
            // default — the same shape as the tutorial-panel bug in tutorial_wave.cpp.
            fn_DialogInstance_GetController(dialogInst, &ctrlResult, s_tidEntityUI);

            void* entityUI = nullptr;
            if (ctrlResult != 0) {
                void* ctrlPtr = (void*)ctrlResult;
                if (s_tidEntityUI && fn_IsDerivedFrom(ctrlPtr, s_tidEntityUI))
                    entityUI = ctrlPtr;
            }

            uint8_t ptBuf[8];
            fn_InternalBasePtr_Ctor(ptBuf, entityUI);
            void* ptInternal = *(void**)ptBuf;

            // Retail: iconsActive = (GetActivePage(ctrl+0x40) - 2U <= 1), i.e. the page
            // is pageBuild(2) or pageDestroy(3) per StandardDialog.rsc:64-74. Once this
            // gate is entered retail ALWAYS recomputes the flag — it never leaves it true.
            // GetActivePage is just *(int*)(pageElement + 0xC0).
            //
            // SA guard: retail derefs ctrl+0x40 unconditionally and would fault here. We
            // fall back to FALSE (hidden), not true — the config has explicitly said "not
            // on the main page", and the main page is what is open by default. Defaulting
            // this to `true` is precisely what hid the duplicate icon for this function's
            // entire life.
            iconsActive = false;
            if (ptInternal != nullptr) {
                void* pageElem = *(void**)((uintptr_t)ptInternal + 0x40);
                if (pageElem != nullptr) {
                    int activePage = *(int*)((uintptr_t)pageElem + 0xC0);
                    iconsActive = (activePage >= 2 && activePage <= 3);
                }
            }
        }
    }

    // Per-instance gate diagnostic. Reports the OUTCOME at every exit, including the
    // paths that SKIP the gate — a trace that only prints when the gate runs cannot
    // distinguish "gate said show" from "gate never ran", which is exactly the
    // ambiguity that made the first fix attempt unverifiable.
    if (std::getenv("BANISHED_DIALOG_TRACE")) {
        static int s_n = 0;
        if (s_n < 24) {
            ++s_n;
            void* cfg = *(void**)((uintptr_t)self + 0x10);
            const char* why = "gate ran";
            if (!cfg)                          why = "SKIP: no config at +0x10 (content mount?)";
            else if (!s_tidStatusIconUIConfig) why = "SKIP: StatusIconUIConfig tid unresolved";
            else if (!fn_IsDerivedFrom(cfg, s_tidStatusIconUIConfig))
                                               why = "SKIP: config not derived from StatusIconUIConfig";
            else if (*(uint8_t*)((uintptr_t)cfg + 0x20) != 0)
                                               why = "SKIP: _showOnMainPage=true";
            fprintf(stderr,
                "[StatusIconUI] self=%p cfg=%p cfgTid=%u entityUITid=%u showOnMain=%d "
                "iconsActive=%d icons=%d bits=0x%x  <- %s\n",
                (void*)self, cfg, s_tidStatusIconUIConfig, s_tidEntityUI,
                cfg ? (int)*(uint8_t*)((uintptr_t)cfg + 0x20) : -1,
                (int)iconsActive, *(int*)((uintptr_t)self + 0x38),
                meshComp ? *(uint32_t*)((uintptr_t)meshComp + 0xF0) : 0u, why);
            fflush(stderr);
        }
    }

    // Loop through icon elements, show/hide based on bitfield
    int iconCount = *(int*)((uintptr_t)self + 0x38);
    uint64_t* iconArray = *(uint64_t**)((uintptr_t)self + 0x30);
    uint32_t bitMask = 1;

    for (int i = 0; i < iconCount; i++) {
        void* iconElem = (void*)iconArray[i];

        // Read the status-icon active bitfield from StatusIconComponent+0xF0. Guard the
        // deref: if no StatusIconComponent was found (a partial-world entity, or a building
        // without one in standalone), treat all bits as clear → every icon is hidden, which
        // is the correct "no active warnings" appearance (and never faults).
        uint32_t bitfield = meshComp ? *(uint32_t*)((uintptr_t)meshComp + 0xF0) : 0;
        bool bitSet = (bitfield & bitMask) != 0;

        if (bitSet && iconsActive) {
            // Show icon
            fn_Element_SetFlagsOnOff(iconElem, 0, 1);
        } else {
            // Hide icon
            fn_Element_SetFlagsOnOff(iconElem, 1, 0);
        }

        // Rotate bit mask left
        bitMask = (bitMask << 1) | (bitMask >> 31);
    }
}

// ============================================================
// STANDALONE promotion — real StatusIconUI controller vtable
// ============================================================
// StatusIconUI is the sub-controller that owns the "icons" group in an entity info
// panel's title bar (the imageStatusHome/Food/Cold/Job/Tool/Disease icons for a citizen,
// and the equivalent construction/warning icons for a building). In standalone it was a
// StubNop (ui_dialog_stub_factories.cpp), so NEITHER its Serialize (+0x20 — caches the
// icon child elements) NOR its visibility update (+0x38 — shows/hides each icon per the
// StatusIconComponent's +0xF0 active-bit) ever ran. With nothing toggling them, every icon
// stayed at its .rsc-default visible state → ALL icons showed at once regardless of the
// entity's real status. This block installs a real per-class vtable that:
//   +0x20 (idx4)  W_Serialize  — cache the "icons" group children (runs once at setup)
//   +0x38 (idx7)  W_Startup    — the exe's real visibility-update slot (kept for fidelity)
//   +0x40 (idx8)  W_OnUpdate   — the ONLY slot the framework dispatches per frame
//                                (DialogInstance::OnUpdate, Runtime 0x180115400); it drives
//                                the visibility update every frame so the icons track state.
// Mirrors the CitizenUI / EntityUI promotion pattern (idx8 → the per-frame content slot).
//
// The StatusIconComponent's +0xF0 bits are populated by the citizen/building status logic
// (StatusIcon_Toggle in code/Game/Component/statusiconcomponent.cpp). When those bits are
// live the panel reflects real status; when they are all clear (a healthy citizen, or a
// standalone entity whose status logic hasn't populated them) the guarded read yields 0 and
// every icon is hidden — the correct "no active warnings" appearance, and never "all shown".
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>
#include "dialog_controller_vtable.h"

// By-name StatusIconComponent finder (forward-declared at the top of this file). The exe
// finder Entity_FindComponent_MeshInstance (0x140002f90) compares each component's GetTypeId
// against DAT_1401c7e08 (the StatusIconComponent type-id), which is 0xFFFFFFFF (unfilled) in
// the standalone image → it matches nothing. Resolving the type-id from the live factory
// tree by name matches the established brick pattern (cf. StatusIcon_FindCompByName) and has
// no shared-global dependency. Entity component array at entity+0x30 (ptr) / +0x38 (count),
// each slot an InternalBasePtr. Returns null if the entity carries no StatusIconComponent.
static void* StatusIconUI_FindStatusIconComp(void* entity) {
    if (!entity || !fn_IsDerivedFrom) return nullptr;
    uint32_t want = ::Object::GetTypeIdByName("StatusIconComponent");
    if (want == 0 || want == 0xFFFFFFFFu) return nullptr;
    uintptr_t e = (uintptr_t)entity;
    int count = *(int*)(e + 0x38);
    int64_t arr = *(int64_t*)(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = (void**)(arr + (int64_t)i * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(ibp);
        void* comp = *ibp;
        if (!comp) continue;
        typedef uint32_t (*GetTypeId_t)(void*);
        uint32_t tid = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)comp + 0x10)))(comp);
        if (tid == want)
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
    }
    return nullptr;
}

namespace statusiconui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("StatusIconUI");
    return c;
}
void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void EnsureGlobals() {
    static uint32_t s_ce = 0, s_dc = 0, s_pt = 0;
    // ComponentEntity is required — Startup casts the panel's data source (+0x28) to it.
    if (!g_TypeId_ComponentEntity) { s_ce = ::Object::ResolveTypeIdByNameLazy("ComponentEntity"); if (s_ce) g_TypeId_ComponentEntity = &s_ce; }
    // These two are the REAL types the page gate needs (see the note at the top of this
    // file). The gate is not cosmetic: without it the title-bar mount of the icons ribbon
    // draws on the main info page and the stop-work icon appears twice.
    if (!s_tidStatusIconUIConfig) s_tidStatusIconUIConfig = ::Object::ResolveTypeIdByNameLazy("StatusIconUIConfig");
    if (!s_tidEntityUI)           s_tidEntityUI           = ::Object::ResolveTypeIdByNameLazy("EntityUI");

    // REMOVED — these two lazy by-name fills were actively harmful, not merely redundant.
    // Neither 0x1401c56e0 nor 0x1401c4f00 is RegisterExeAddr'd, so engine_funcs.cpp's
    // ResolveAddr leaves both globals null; THIS was the only code in the tree that ever
    // made them non-null, and it filled them by NAME with genuinely different classes than
    // the .rdata slots they are documented as. `*g_TypeId_DialogController` now has no
    // readers at all, and the sole reader of `*g_TypeId_PageToggleUI`
    // (engine_funcs.h, Toolbar_CenterOnEntity_Impl) actually wants **EntityUI** — retail
    // FUN_140103040 compares against DWORD_1401c4f00, whose registrar is
    // RegisterType_EntityUI. Feeding it the real PageToggleUI id (a different class, whose
    // own slot is 0x1401c5408) made its IsDerivedFrom always fail, silently disabling
    // camera-follow for every "go to entity" click. That consumer now resolves EntityUI
    // for itself, so filling these globals here would only re-poison it.
    (void)s_dc; (void)s_pt;

    static bool s_logged = false;
    if (!s_logged && std::getenv("BANISHED_DIALOG_TRACE")) {
        s_logged = true;
        // Print the VALUES, not just the pointers: a resolve that returns 0 is the
        // documented "registered but filled with zero" failure, and a pointer-only log
        // cannot distinguish it from a good resolve.
        fprintf(stderr, "[StatusIconUI] globals: CE=%p cfgTid=%u entityUITid=%u\n",
                (void*)g_TypeId_ComponentEntity, s_tidStatusIconUIConfig, s_tidEntityUI);
        fflush(stderr);
    }
}

// +0x20 setup: cache the "icons" group child elements into self+0x30 (array) / +0x38 (count).
void W_Serialize(void* self) {
    EnsureGlobals();
    if (!self || !SA_DialogEntityAlive(self)) return;
    StatusIconUI_Serialize(reinterpret_cast<StatusIconUI*>(self));
}

// +0x38: the visibility update. Lazily runs Serialize once if the setup slot didn't populate
// the icon array (self+0x30 null), so the per-frame path is robust even if idx4 never fired.
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !g_TypeId_ComponentEntity || !SA_DialogEntityAlive(self)) return;
    if (*reinterpret_cast<uint64_t*>((char*)self + 0x30) == 0)
        StatusIconUI_Serialize(reinterpret_cast<StatusIconUI*>(self));
    StatusIconUI_Startup(reinterpret_cast<StatusIconUI*>(self));
}

// +0x40 per-frame (the only slot DialogInstance::OnUpdate dispatches each frame) → drive the
// visibility update so the icons continuously track the entity's live StatusIconComponent bits.
void W_OnUpdate(void* self, void* /*time*/) { W_Startup(self); }

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Serialize);
        g_vt[7] = reinterpret_cast<void*>(&W_Startup);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
    }
} g_init;

// Wave 10D lane VTFID: retail's StatusIconUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "StatusIconUI", &StubNop);

void Zero(void* p) { for (size_t o = 0; o < (size_t)0x40; o += 8) *reinterpret_cast<uint64_t*>((char*)p + o) = 0; }
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)0x40, 8);
    if (!p) return nullptr;
    Zero(p); *reinterpret_cast<void**>(p) = g_vt; return p;
}
void* InPlace(void* m) { if (!m) return nullptr; Zero(m); *reinterpret_cast<void**>(m) = g_vt; return m; }

}  // namespace statusiconui_sa

REGISTER_TYPE(StatusIconUI, "UI::DialogController",
              &statusiconui_sa::Factory, &statusiconui_sa::InPlace);

