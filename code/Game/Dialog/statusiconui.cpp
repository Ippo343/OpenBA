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

// By-name StatusIconComponent finder, defined in the STANDALONE promotion block at the
// bottom of this file. StatusIconUI_Startup calls it as a fallback because the exe
// component finder (Entity_FindComponent_MeshInstance, 0x140002f90) keys off the
// StatusIconComponent type-id .rdata global DAT_1401c7e08, which is UNFILLED (0xFFFFFFFF)
// in the standalone image and therefore matches no component for any entity.
static void* StatusIconUI_FindStatusIconComp(void* entity);

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

    // Determine if icons should be visible based on page state
    bool iconsActive = true;

    void* parentController = *(void**)((uintptr_t)self + 0x10);
    if (parentController != nullptr) {
        // Check if parent is a DialogController. Guard the type-id deref: in standalone the
        // exe .rdata type-id globals are null (page-check then defaults iconsActive=true,
        // which is the correct "show icons per active bits" behaviour for a single-view
        // panel).
        bool isDC = g_TypeId_DialogController &&
                    fn_IsDerivedFrom(parentController, *g_TypeId_DialogController);
        void* dcPtr = isDC ? parentController : nullptr;

        // Check if controller's dialog element (+0x20) is null
        if (dcPtr && *(void**)((uintptr_t)dcPtr + 0x20) == nullptr) {
            // Get PageToggleUI controller from dialog instance
            void* dialogInst = *(void**)((uintptr_t)self + 0x18);
            uint64_t ctrlResult = 0;
            fn_DialogInstance_GetController(dialogInst, &ctrlResult, 0);

            void* pageToggle = nullptr;
            if (ctrlResult != 0) {
                void* ctrlPtr = (void*)ctrlResult;
                bool isPageToggle = g_TypeId_PageToggleUI &&
                                    fn_IsDerivedFrom(ctrlPtr, *g_TypeId_PageToggleUI);
                pageToggle = isPageToggle ? ctrlPtr : nullptr;
            }

            uint8_t ptBuf[8];
            fn_InternalBasePtr_Ctor(ptBuf, pageToggle);
            void* ptInternal = *(void**)ptBuf;

            // GetActivePage is just *(int*)(pageElement + 0xC0)
            // PageToggleUI stores its page element at +0x40
            if (ptInternal != nullptr) {
                void* pageElem = *(void**)((uintptr_t)ptInternal + 0x40);
                if (pageElem != nullptr) {
                    int activePage = *(int*)((uintptr_t)pageElem + 0xC0);
                    // Icons are active on pages 2 and 3 (iVar3 - 2U <= 1)
                    iconsActive = (activePage >= 2 && activePage <= 3);
                }
            }
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
    // DialogController + PageToggleUI feed the (cosmetic) page-gating branch. If they don't
    // resolve, Startup's guards default iconsActive=true (icons shown per the active bits),
    // which is the correct single-view behaviour — so these are best-effort, not required.
    if (!g_TypeId_DialogController) { s_dc = ::Object::ResolveTypeIdByNameLazy("DialogController"); if (s_dc) g_TypeId_DialogController = &s_dc; }
    if (!g_TypeId_PageToggleUI)     { s_pt = ::Object::ResolveTypeIdByNameLazy("PageToggleUI");     if (s_pt) g_TypeId_PageToggleUI = &s_pt; }

    static bool s_logged = false;
    if (!s_logged && std::getenv("BANISHED_DIALOG_TRACE")) {
        s_logged = true;
        fprintf(stderr, "[StatusIconUI] globals: CE=%p DC=%p PageToggle=%p\n",
                (void*)g_TypeId_ComponentEntity, (void*)g_TypeId_DialogController,
                (void*)g_TypeId_PageToggleUI);
        fflush(stderr);
    }
}

// +0x20 setup: cache the "icons" group child elements into self+0x30 (array) / +0x38 (count).
void W_Serialize(void* self) { EnsureGlobals(); if (self) StatusIconUI_Serialize(reinterpret_cast<StatusIconUI*>(self)); }

// +0x38: the visibility update. Lazily runs Serialize once if the setup slot didn't populate
// the icon array (self+0x30 null), so the per-frame path is robust even if idx4 never fired.
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !g_TypeId_ComponentEntity) return;
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

