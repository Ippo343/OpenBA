// code/Game/Dialog/upgradeui.cpp
// Clean reimplementation of UpgradeUI game logic.
//
// UpgradeUI (0x38 bytes) — upgrade panel controller.
// Extends DialogController (0x30 base).
// +0x30 = button element (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// UpgradeUI::Serialize (vtable +0x20) — Slot4
// ============================================================
// Finds the upgrade button element, creates a sprite from the
// ToolbarDescription on the upgrade target template (UpgradeUIConfig+0x20),
// and sets the sprite on the button's child image element.
//
// Original: 0x140086d90
void UpgradeUI_Serialize(UpgradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get config from DialogController +0x10
    void* desc = *(void**)(s + 0x10);

    // Check if config is an UpgradeUIConfig (its +0x20 holds the upgrade
    // target ComponentEntity template)
    bool isUpgrade = fn_IsDerivedFrom(desc, *g_TypeId_UpgradeUIConfig);
    void* upgradeDesc = isUpgrade ? desc : nullptr;

    // CheckPtr on the InternalBasePtr at +0x20 before reading
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    void* elem = *(void**)(s + 0x20);

    // Check if element is a ButtonElement (exref 0x1401514f8)
    bool isButton = fn_IsDerivedFrom(elem, *g_TypeId_HelpButtonElem);
    void* checkedElem = isButton ? elem : nullptr;

    // Properly construct InternalBasePtr at +0x30 (button reference)
    fn_InternalBasePtr_Ctor((void*)(s + 0x30), checkedElem);

    // Set button element ID to 33000
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    void* button = *(void**)(s + 0x30);
    fn_Element_SetElementId(button, 33000);

    // Get button's child at index 4 (the ImageElement)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    button = *(void**)(s + 0x30);
    uint8_t childBuf[16];
    fn_Element_Child(button, childBuf, 4);

    // Check if child is an ImageElement type
    fn_InternalBasePtr_CheckPtr(childBuf);
    void* childElem = *(void**)childBuf;
    bool isImage = fn_IsDerivedFrom(childElem, *g_TypeId_ImageElement);
    void* checkedImage = isImage ? childElem : nullptr;

    // Construct an IBP for the image element
    uint8_t imageIBP[8];
    fn_InternalBasePtr_Ctor(imageIBP, checkedImage);

    // Get the target ComponentEntity template from UpgradeUIConfig+0x20
    uintptr_t subDesc = *(uintptr_t*)((uintptr_t)upgradeDesc + 0x20);

    // Find ToolbarDescription in the target template's component list
    void* buildReqComp = nullptr;
    if (subDesc && *(int*)(subDesc + 0x30) > 0) {
        int compCount = *(int*)(subDesc + 0x30);
        uintptr_t compArray = *(uintptr_t*)(subDesc + 0x28);

        for (int i = 0; i < compCount; i++) {
            void* comp = *(void**)(compArray + (uintptr_t)i * 8);
            if (!comp) continue;

            // Call component vtable[2] to get type ID
            void** compVtable = *(void***)comp;
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            uint32_t typeId = ((fn_GetTypeId_t)compVtable[2])(comp);

            if (typeId == *g_TypeId_ToolbarDescription_FC) {
                void* checked = fn_IsDerivedFrom(comp, *g_TypeId_ToolbarDescription_FC)
                    ? comp : nullptr;
                buildReqComp = checked;
                break;
            }
        }
    }

    // SA-guard: retail data guarantees a ToolbarDescription on the upgrade
    // target template; skip the icon when the standalone template lacks one.
    if (!buildReqComp) return;

    // Create a Sprite from the ToolbarDescription using proper constructor
    uintptr_t brc = (uintptr_t)buildReqComp;
    uint8_t sprite[16];
    fn_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));

    // Set sprite on the image element
    fn_InternalBasePtr_CheckPtr(imageIBP);
    void* imageElem = *(void**)imageIBP;
    fn_ImageElement_SetSprite(imageElem, sprite);

    // Set button's user data to the target template
    void* subDescBase = *(void**)((uintptr_t)upgradeDesc + 0x20);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    button = *(void**)(s + 0x30);
    fn_Element_SetUserData(button, subDescBase);
}

// ============================================================
// UpgradeUI::OnRemove (vtable +0x48) — Slot9
// ============================================================
// On button click (ID 33000), finds DestroyComponent on the entity,
// copies the UpgradeDescription reference to it, and starts the upgrade.
//
// Original: 0x140086c90
void UpgradeUI_OnRemove(UpgradeUI* self, void* p2, int eventType, int eventId) {
    uintptr_t s = (uintptr_t)self;

    // Only handle button clicks (msg 0x12) with ID 33000
    if (eventType != 0x12 || eventId != 33000)
        return;

    // Verify the event source matches our stored button element
    void* button = *(void**)(s + 0x30);
    void* eventSource = *(void**)p2;
    if (eventSource != button)
        return;

    // Get the data source entity from DialogController +0x28
    void* dataSource = *(void**)(s + 0x28);
    if (!dataSource)
        return;

    // Don't start an upgrade on a building that is already dead / marked-for-removal
    // (the panel can linger one marked-but-alive frame under the removal deferral —
    // entitylist.cpp Step 0 — during which a click must not re-arm its DestroyComponent).
    if (!SA_DialogEntityAlive(self))
        return;

    // Check if data source is a ComponentEntity
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity))
        return;

    // Find DestroyComponent on the entity
    uint8_t compBuf[16];
    fn_FindComponent_Destroy(dataSource, compBuf);
    void* destroyComp = *(void**)compBuf;
    if (!destroyComp) {
        std::fprintf(stderr, "[UpgradeUI] OnRemove: no DestroyComponent on "
                     "entity %p — upgrade aborted\n", dataSource);
        std::fflush(stderr);
        return;
    }

    // Get the config from DialogController +0x10
    void* desc = *(void**)(s + 0x10);
    if (!desc)
        return;

    // Check if config is an UpgradeUIConfig
    if (!fn_IsDerivedFrom(desc, *g_TypeId_UpgradeUIConfig))
        return;

    // Set the upgrade target on DestroyComponent
    // destroy+0x60 = config+0x20 (the target ComponentEntity template)
    *(uint64_t*)((uintptr_t)destroyComp + 0x60) = *(uint64_t*)((uintptr_t)desc + 0x20);

    // Start the upgrade: pump the BuildState state machine. 0x14011dfa0 is the
    // generic driver ("StartUpgrade" was a misnomer); the disasm-verified port
    // is RemovalCalc_StateMachine_dfa0 (keystone_a_removalcalcpath.cpp),
    // reached through the address registry.
    typedef void (*fn_SMDriver_t)(void* destroyComp);
    fn_SMDriver_t smDriver =
        reinterpret_cast<fn_SMDriver_t>(ResolveAddr(Module::Exe, 0x14011dfa0));
    std::fprintf(stderr, "[UpgradeUI] OnRemove: target=%p written to "
                 "destroyComp=%p +0x60, pumping SM (driver=%p)\n",
                 *(void**)((uintptr_t)desc + 0x20), destroyComp,
                 (void*)smDriver);
    std::fflush(stderr);
    if (smDriver) smDriver(destroyComp);
}

// ============================================================
// Standalone promotion — real UpgradeUI vtable (2026-07-20)
// ============================================================
// Retail vtable @ 0x14015df00 (11 slots): overrides 0,1,2,4,7,9. Slot 7
// (0x140086d80) is a 3-byte RET 0 no-op, slots 3,5,6,8,10 are inherited
// UI::DialogController base thunks. Standalone mirrors the residenceui_sa
// pattern: Slot4 (Serialize, icon + button wiring) is driven once from slot 8
// (OnUpdate) as a self-heal if the framework never calls slot 4; slot 9 is the
// click handler (eventType 0x12, element id 33000 → write the upgrade target
// template into DestroyComponent+0x60 and pump the state machine).
// Replaces the UI_DIALOG_STUB(UpgradeUI, ...) no-op registration.
#include "Object/factory_registration.h"
#include "dialog_controller_vtable.h"

namespace upgradeui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

// Standalone-local fallbacks for type-id globals that resolve NULL.
static uint32_t s_cfgTid = 0;   // UpgradeUIConfig
static uint32_t s_ceTid  = 0;   // ComponentEntity

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("UpgradeUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    // +0x30 is an unowned button reference — nothing to free.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

void EnsureGlobals() {
    if (!g_TypeId_UpgradeUIConfig || !*g_TypeId_UpgradeUIConfig) {
        s_cfgTid = ::Object::ResolveTypeIdByNameLazy("UpgradeUIConfig");
        if (s_cfgTid) g_TypeId_UpgradeUIConfig = &s_cfgTid;
    }
    if (!g_TypeId_ComponentEntity) {
        s_ceTid = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ceTid) g_TypeId_ComponentEntity = &s_ceTid;
    }
}

bool Ready() {
    return g_TypeId_UpgradeUIConfig && *g_TypeId_UpgradeUIConfig
        && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
        && g_TypeId_HelpButtonElem && *g_TypeId_HelpButtonElem
        && g_TypeId_ImageElement && *g_TypeId_ImageElement
        && g_TypeId_ToolbarDescription_FC && *g_TypeId_ToolbarDescription_FC
        && fn_IsDerivedFrom && fn_InternalBasePtr_Ctor
        && fn_InternalBasePtr_CheckPtr;
}

// Serialize is only safe when the bound config is an UpgradeUIConfig with a
// non-null target template at +0x20 (the stub config zero-fills it until the
// real UpgradeUIConfig port lands).
bool ConfigOk(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* cfg = *reinterpret_cast<void**>(s + 0x10);
    if (!cfg || !fn_IsDerivedFrom(cfg, *g_TypeId_UpgradeUIConfig)) return false;
    return *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cfg) + 0x20) != nullptr;
}

static bool Trace() {
    static const bool t = [] {
        const char* e = std::getenv("BANISHED_DESTROY_TRACE");
        return e && e[0] && e[0] != '0';
    }();
    return t;
}

// idx4 (+0x20): bind the upgrade button, set its icon from the target
// template's ToolbarDescription.
void W_Serialize(void* self) {
    EnsureGlobals();
    if (!self || !Ready()) return;
    if (!ConfigOk(self)) {
        if (Trace()) {
            static int n = 0;
            if (n++ < 3) {
                uintptr_t s = reinterpret_cast<uintptr_t>(self);
                void* cfg = *reinterpret_cast<void**>(s + 0x10);
                std::fprintf(stderr, "[UpgradeUI] Serialize blocked: cfg=%p "
                             "isCfg=%d target(+0x20)=%p\n", cfg,
                             cfg ? (int)fn_IsDerivedFrom(cfg, *g_TypeId_UpgradeUIConfig) : -1,
                             cfg ? *reinterpret_cast<void**>(
                                 reinterpret_cast<uintptr_t>(cfg) + 0x20) : nullptr);
                std::fflush(stderr);
            }
        }
        return;
    }
    UpgradeUI_Serialize(reinterpret_cast<UpgradeUI*>(self));
    if (Trace()) {
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        std::fprintf(stderr, "[UpgradeUI] Serialize OK: button(+0x30)=%llx\n",
                     (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x30));
        std::fflush(stderr);
    }
}

// idx8 (+0x40): per-frame self-heal — run Serialize once if the button ref at
// +0x30 was never populated.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready()) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) W_Serialize(self);
}

// idx9 (+0x48): click dispatch (self, &elementPtr, msg, id, x, y).
void W_OnRemove(void* self, void* p2, int msg, int id, int /*mx*/, int /*my*/) {
    EnsureGlobals();
    if (!self || !p2 || !Ready()) return;
    if (Trace() && msg == 0x12) {
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        std::fprintf(stderr, "[UpgradeUI] click msg=0x%x id=%d src=%p "
                     "button(+0x30)=%llx\n", msg, id, *reinterpret_cast<void**>(p2),
                     (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x30));
        std::fflush(stderr);
    }
    UpgradeUI_OnRemove(reinterpret_cast<UpgradeUI*>(self), p2, msg, id);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0] = reinterpret_cast<void*>(&Dtor);
        g_vt[2] = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4] = reinterpret_cast<void*>(&W_Serialize);
        g_vt[8] = reinterpret_cast<void*>(&W_OnUpdate);
        g_vt[9] = reinterpret_cast<void*>(&W_OnRemove);
    }
} g_init;

// Wave 10D lane VTFID: retail's UpgradeUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "UpgradeUI", &StubNop);

void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x38); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x38), 8);
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

}  // namespace upgradeui_sa

REGISTER_TYPE(UpgradeUI, "UI::DialogController",
              &upgradeui_sa::Factory, &upgradeui_sa::InPlace);
