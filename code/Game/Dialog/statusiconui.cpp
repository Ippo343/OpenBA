// engine/src/ui/statusiconui.cpp
// Clean reimplementation of StatusIconUI game logic.
//
// StatusIconUI (0x40 bytes) — status icon grid panel.
// Extends DialogController. data30 = icon element array (ptr),
// data38 = icon element count.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/DialogInstance.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "Object/factory_registration.h"
#include <cstddef>
#include "Component/StatusIconComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/EntityUI.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/StatusIconUIConfig.h"   // X::TypeId() (B.3.8 callers)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)

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
// Resolved by NAME here rather than through the mislabelled TypeId_* globals, which are
// left untouched because engine_funcs.h:4585 still consumes TypeId_PageToggleUI.
static uint32_t s_tidStatusIconUIConfig = 0;
static uint32_t s_tidEntityUI = 0;

namespace statusiconui_sa {

void EnsureGlobals() {
    static uint32_t s_dc = 0, s_pt = 0;
    // ENDGAME B.3.9 CG2: TypeId_ComponentEntity (cell 0x1401c5d70) -> ComponentEntity::
    // TypeId(); the class static always answers, so the lazy-fill of the global retires.
    // These two are the REAL types the page gate needs (see the note at the top of this
    // file). The gate is not cosmetic: without it the title-bar mount of the icons ribbon
    // draws on the main info page and the stop-work icon appears twice.
    if (!s_tidStatusIconUIConfig) s_tidStatusIconUIConfig = StatusIconUIConfig::TypeId();
    if (!s_tidEntityUI)           s_tidEntityUI           = EntityUI::TypeId();

    // REMOVED — these two lazy by-name fills were actively harmful, not merely redundant.
    // Neither 0x1401c56e0 nor 0x1401c4f00 is RegisterExeAddr'd, so engine_funcs.cpp's
    // ResolveAddr leaves both globals null; THIS was the only code in the tree that ever
    // made them non-null, and it filled them by NAME with genuinely different classes than
    // the .rdata slots they are documented as. `*TypeId_DialogController` now has no
    // readers at all, and the sole reader of `*TypeId_PageToggleUI`
    // (engine_funcs.h, Toolbar_CenterOnEntity_Impl) actually wants **EntityUI** — retail
    // FUN_140103040 compares against DWORD_1401c4f00, whose registrar is
    // RegisterType_EntityUI. Feeding it the real PageToggleUI id (a different class, whose
    // own slot is 0x1401c5408) made its IsDerivedFrom always fail, silently disabling
    // camera-follow for every "go to entity" click. That consumer now resolves EntityUI
    // for itself, so filling these globals here would only re-poison it.
    (void)s_dc; (void)s_pt;

}



}  // namespace statusiconui_sa

// ============================================================
// StatusIconUI::Serialize (vtable +0x20) — Cache icon child elements
// ============================================================
// Finds the "icons" container element, gets its child count,
// resizes the internal array, and copies all child InternalPtrs
// into the array for fast access during Startup.
//
// Original: 0x14007b270.

// [4] 0x14007b270
void StatusIconUI::OnCreated() {
    statusiconui_sa::EnsureGlobals();
    if (!SA_DialogEntityAlive(this)) return;
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find "icons" container element by name
    System::String nameBuf("icons");

    uint64_t findResult = 0;
    // fn_FindElement: member with non-trivial return: RCX=this, RDX=retval, R8=name
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameBuf));  // B.3.6 bridge

    // Dereference InternalPtr result to get raw Element*
    // The original does IsDerivedFrom check here, but we skip it
    // since FindElement by name always returns a valid element.
    void* rawElem = (void*)findResult;
    uint8_t iconsBuf[8];
    new (iconsBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    void* iconsElem = *(void**)iconsBuf;

    // Get child count
    int childCount = reinterpret_cast<const UI::Element*>(iconsElem)->ChildCount();  // B.3.6 bridge

    // Resize the array at self+0x30 to hold childCount entries.
    // B.3.2: Collection::Array<T>::SetSize — retail 0x1400032e0, stride 8 align 8,
    // new tail zeroed. Element is the 8-byte InternalPtr<Element> copied in below.
    GameColl::ArrayAt<GameColl::Ptr8>(this, 0x30).SetSize(childCount);

    // Copy each child's InternalPtr (8 bytes) into the array
    int storedCount = *(int*)((uintptr_t)this + 0x38);
    for (int i = 0; i < storedCount; i++) {
        // Element::Child returns InternalPtr<Element> via non-trivial return
        uint64_t childResult = 0;
        ::new (&childResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(iconsElem)->Child(i));  // B.3.6 bridge

        // Copy 8-byte InternalPtr into array slot
        uint64_t* arrayBase = *(uint64_t**)((uintptr_t)this + 0x30);
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

// [7] 0x14007b0a0
void StatusIconUI::OnUpdateLayout() {
    statusiconui_sa::EnsureGlobals();
    if (!SA_DialogEntityAlive(this)) return;
    if (*reinterpret_cast<uint64_t*>((char*)this + 0x30) == 0)
        StatusIconUI::OnCreated();
    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
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
    void* config = *(void**)((uintptr_t)this + 0x10);
    if (config != nullptr) {
        bool isCfg = s_tidStatusIconUIConfig &&
                     fn_IsDerivedFrom(config, s_tidStatusIconUIConfig);
        void* cfgPtr = isCfg ? config : nullptr;

        // Retail reads *(char*)(cfg+0x20) — the `_showOnMainPage` BOOL — and enters the
        // page restriction when it is FALSE. (This was previously coded as a
        // *pointer*-null test on +0x20, which is a different question entirely.)
        if (cfgPtr && *(uint8_t*)((uintptr_t)cfgPtr + 0x20) == 0) {
            void* dialogInst = *(void**)((uintptr_t)this + 0x18);
            uint64_t ctrlResult = 0;
            // Third arg is a TYPE FILTER. Retail (0x14007b14b) is
            //   MOV R8D,[0x1401c4f00]   ; EntityUI type id
            //   CALL [0x140151768]      ; DialogInstance::GetController
            // Ghidra renders this call with two arguments and hides the filter, which
            // is how a 0 got here. With 0 the call returns nothing, the EntityUI narrow
            // below can never fire, and the page gate silently falls through to its
            // default — the same shape as the tutorial-panel bug in tutorial_wave.cpp.
            ::new (&ctrlResult) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialogInst)->GetController(s_tidEntityUI));  // B.3.6 bridge

            void* entityUI = nullptr;
            if (ctrlResult != 0) {
                void* ctrlPtr = (void*)ctrlResult;
                if (s_tidEntityUI && fn_IsDerivedFrom(ctrlPtr, s_tidEntityUI))
                    entityUI = ctrlPtr;
            }

            uint8_t ptBuf[8];
            new (ptBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityUI));   // B.3.3 bridge
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

    // Loop through icon elements, show/hide based on bitfield
    int iconCount = *(int*)((uintptr_t)this + 0x38);
    uint64_t* iconArray = *(uint64_t**)((uintptr_t)this + 0x30);
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
            reinterpret_cast<UI::Element*>(iconElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        } else {
            // Hide icon
            reinterpret_cast<UI::Element*>(iconElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge
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

// By-name StatusIconComponent finder (forward-declared at the top of this file). The exe
// finder Entity_FindComponent_MeshInstance (0x140002f90) compares each component's GetTypeId
// against DAT_1401c7e08 (the StatusIconComponent type-id), which is 0xFFFFFFFF (unfilled) in
// the standalone image → it matches nothing. Resolving the type-id from the live factory
// tree by name matches the established brick pattern (cf. StatusIcon_FindCompByName) and has
// no shared-global dependency. Entity component array at entity+0x30 (ptr) / +0x38 (count),
// each slot an InternalBasePtr. Returns null if the entity carries no StatusIconComponent.
static void* StatusIconUI_FindStatusIconComp(void* entity) {
    if (!entity || !fn_IsDerivedFrom) return nullptr;
    uint32_t want = StatusIconComponent::TypeId();
    if (want == 0 || want == 0xFFFFFFFFu) return nullptr;
    uintptr_t e = (uintptr_t)entity;
    int count = *(int*)(e + 0x38);
    int64_t arr = *(int64_t*)(e + 0x30);
    for (int i = 0; i < count; i++) {
        void** ibp = (void**)(arr + (int64_t)i * 8);
        void* comp = *ibp;
        if (!comp) continue;
        uint32_t tid = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (tid == want)
            return fn_IsDerivedFrom(comp, want) ? comp : nullptr;
    }
    return nullptr;
}




// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void StatusIconUI::OnUpdate(const System::Time& /*time*/) {
    StatusIconUI::OnUpdateLayout();
}

// [0] 0x140014370: ~StatusIconUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* StatusIconUI::DeletingDtor(unsigned flags) {
    this->~StatusIconUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140014180: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* StatusIconUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StatusIconUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StatusIconUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* StatusIconUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StatusIconUI());
}

DEFINE_EXE_VTABLE(StatusIconUI, "_ZTV12StatusIconUI")
DEFINE_EXE_TYPE(StatusIconUI, "UI::DialogController")

