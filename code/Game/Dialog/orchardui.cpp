// engine/src/ui/orchardui.cpp
// Clean reimplementation of OrchardUI game logic.
//
// OrchardUI (0x48 bytes) — orchard crop selection and growth display.
// Extends SelectResourceUI > DialogController.
// +0x30 = combo element (SelectResourceUI base), +0x38 = yield progress element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"   // Object::ResolveTypeIdByNameLazy
#include <cstdint>
#include <cstdlib>
#include <cstdio>

// Retail's Slot4 filters the "yield" FindElement result through
// IsDerivedFrom(elem, *g_TypeId_ProgressElement) before storing it at +0x38.
// That exe-.rdata slot (0x1401515b8) is filled ONLY by
// standalone/reg_element_typeids.cpp, which is gated on BANISHED_LIVE_ELEMENT_TIDS
// and is NOT in kDefaultOnFlags — so on the default path it is null, and
// IsDerivedFrom(elem, 0) is always false. That would leave +0x38 null and turn the
// unguarded `+0x124 = 1.0f` write two lines later into a null-deref crash.
// See [[reference-gated-element-typeid-disables-controller]] — the same null field
// that made every pasture ControlUI button inert.
//
// Resolve a PRIVATE copy by name rather than publishing into the shared global:
// other dialogs read it and currently rely on the null as a no-op. Prefer the
// registry value when the gate IS on so the two paths can never disagree.
// Same idiom as controlui.cpp / pagetoggleui.cpp / productionui.cpp.
static uint32_t OrchardUI_ProgressElementTypeId() {
    if (g_TypeId_ProgressElement && *g_TypeId_ProgressElement)
        return *g_TypeId_ProgressElement;          // gate on: use the real slot
    static uint32_t s_local = 0;
    if (!s_local) s_local = ::Object::ResolveTypeIdByNameLazy("ProgressElement");
    return s_local;
}

// ============================================================
// OrchardUI::Slot4 (vtable +0x20) — Initialize yield progress + combo
// ============================================================
// Finds the "yield" progress element, stores at +0x38, sets initial
// progress value to 1.0f, then calls SelectResourceUI::Slot4 to
// initialize the combo element.
//
// Original: 0x1400755B0.

void OrchardUI_Slot4(OrchardUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "yield" progress element in parent element at +0x20
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "yield");

    void* parentElem = *(void**)(s + 0x20);
    uint64_t findResult = 0;
    fn_FindElement(parentElem, &findResult, nameBuf);
    fn_String_Dtor(nameBuf);

    // Store yield element at +0x38, faithful to retail: the find result is kept only
    // if it really is a ProgressElement (see the type-id note at the top of the file —
    // this check used to be skipped, which is safe but unfaithful).
    void* rawElem = (void*)findResult;
    void* checked = fn_IsDerivedFrom(rawElem, OrchardUI_ProgressElementTypeId())
                        ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    fn_InternalBasePtr_Ctor(&basePtrVal, checked);
    *(uint64_t*)(s + 0x38) = basePtrVal;

    // Set initial progress value to 1.0f (100%).
    // Retail writes this unguarded — Orchard.rsc always carries a "yield" progress
    // element, so the deref is load-bearing there. SA guard: the panel template is
    // reached through the shared StandardDialog mount, so a dialog whose layout has
    // no "yield" would fault here where retail never can. Skip the write, don't fake it.
    if (*(uint64_t*)(s + 0x38))
        *(float*)(*(uint64_t*)(s + 0x38) + 0x124) = 1.0f;

    // Initialize combo via base class
    SelectResourceUI_Slot4(self);
}

// ============================================================
// OrchardUI::Slot7 (vtable +0x38) — Update growth progress
// ============================================================
// Gets the OrchardComponent from the entity, queries growth progress
// as a float ratio, updates the yield progress bar with percentage text,
// then enables/disables the combo based on item count.
//
// Original: 0x1400754B0.

void OrchardUI_Slot7(OrchardUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe. Same guard PastureUI_Slot7 carries, same reason.
    if (!SA_DialogEntityAlive(self)) return;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);

    // SA guard (retail unguarded): an entity without an OrchardComponent, or the
    // combo/yield elements not seated yet because Slot4 has not run for this
    // instance. Retail cannot reach either state — the panel is only mounted on
    // Orchard.rsc and Slot4 always precedes the first tick.
    if (!orchardComp) return;
    if (!*(uint64_t*)(s + 0x30) || !*(uint64_t*)(s + 0x38)) return;

    // Get average growth ratio (0.0 - 1.0)
    float growth = fn_OrchardGetGrowth((void*)orchardComp);

    // Set progress bar value directly at element+0x128
    void* yieldElem = (void*)*(uint64_t*)(s + 0x38);
    *(float*)((uintptr_t)yieldElem + 0x128) = growth;

    // Format percentage text: (int)(growth * 100.0f + 0.5f)
    int percent = (int)(growth * 100.0f + 0.5f);
    uint8_t textBuf[16];
    fn_String_Format((char*)textBuf, "%i", (uint64_t)(uint32_t)percent);

    // Set progress text parameter
    fn_ProgressElement_SetProgressTextParameter(yieldElem, 0, textBuf);
    fn_String_Dtor(textBuf);

    // Enable/disable combo based on item count (SelectResourceUI base logic)
    void* combo = (void*)*(uint64_t*)(s + 0x30);
    int itemCount = fn_ComboElement_GetItemCount(combo);
    void** vtable = *(void***)combo;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable[8])(combo, itemCount > 0);
}

// ============================================================
// OrchardUI::Slot11 (vtable +0x58) — Set resource type
// ============================================================
// Sets the selected crop/resource type on the OrchardComponent.
// If changing from no resource (0) to a valid resource, calls
// vtable[14](comp, 2) to initialize production.
//
// Original: 0x140075380.

void OrchardUI_Slot11(OrchardUI* self, int64_t param2) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);


    // SA guard (retail unguarded): fires from a combo event, which can only be
    // routed to an entity that has an OrchardComponent in retail.
    if (!orchardComp) return;

    // If currently no resource set (comp+0x60 == 0) and new resource is valid,
    // call initialization via vtable[14](comp, 2)
    int64_t currentResource = *(int64_t*)(orchardComp + 0x60);
    if (currentResource == 0 && param2 != 0) {
        void** vtable = *(void***)(void*)orchardComp;
        typedef void (*fn_InitProduction_t)(void*, int);
        ((fn_InitProduction_t)vtable[14])((void*)orchardComp, 2);
    }

    // Set the resource type
    *(int64_t*)(orchardComp + 0x60) = param2;
}

// ============================================================
// OrchardUI::Slot12 (vtable +0x60) — Get resource type
// ============================================================
// Returns the current crop/resource type from the OrchardComponent
// at comp+0x60 via the output parameter.
//
// Original: 0x140075430.

void* OrchardUI_Slot12(OrchardUI* self, void* param2) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);

    // Return resource type at comp+0x60 (SA guard: see Slot11)
    *(uint64_t*)param2 = orchardComp ? *(uint64_t*)(orchardComp + 0x60) : 0;
    return param2;
}

// ============================================================
// Slot13 (vtable +0x68): GetCategoryFilter — returns 1 (orchard category)
// Original: 0x140075420 (6 bytes, MOV EAX,1; RET, unhookable)
// ============================================================
int OrchardUI_Slot13(OrchardUI* self) {
    (void)self;
    return 1;
}

// ============================================================
// STANDALONE promotion — real OrchardUI controller vtable
// ============================================================
// OrchardUI was a UI_DIALOG_STUB (all-no-op), so every port above was dead code:
// the orchard panel's tree-type combo stayed empty and the yield bar never moved.
// Follows pastureui.cpp exactly — same class family, same base.
//
// ⚠ Vtable address corrected 2026-08-09 by a byte-wise read. Three tables in this
// family were being confused, and decomp/application.md had two of them wrong:
//   0x14015cf30 = vtable_OrchardUI          (doc said 0x14015d330 — WRONG)
//   0x14015f610 = vtable_PastureUI          (doc said 0x14015d330 — WRONG)
//   0x14015f270 = vtable_CropUI
//   0x14015d330 = vtable_SelectResourceUI, the ABSTRACT base. Ghidra labels it
//                 `vtable_CropUI`, which is a misnomer; its _purecall (0x14003addc)
//                 entries at idx11/12/13 are simply the three pure virtuals
//                 (SetResource / GetResource / GetCategoryFilter).
//
// Retail vtable @ 0x14015cf30 (14 slots), read byte-wise:
//   idx3,5,6,10 -> the shared 5-6 byte micro-rets at 0x14003a95a-0x14003a978,
//                  for which the generic StubNop is already byte-faithful.
//   idx4  (+0x20) 0x1400755b0 Slot4  — find "yield" ProgressElement, set 100%, base setup
//   idx7  (+0x38) 0x1400754b0 Slot7  — per-frame: growth % into the yield bar
//   idx8  (+0x40) 0x140078d30        — SelectResourceUI::Slot8 (combo refresh)
//   idx9  (+0x48) 0x140078d80        — SelectResourceUI::Slot9 (combo selection)
//   idx11 (+0x58) 0x140075380 Slot11 — set resource type on OrchardComponent
//   idx12 (+0x60) 0x140075430 Slot12 — get current type
//   idx13 (+0x68) 0x140075420 Slot13 — category filter = 1 (orchard)
// That closes the work list at the vtable boundary: every real slot is covered.
//
// Retail's Slot4/Slot7 tail-call 0x140078be0 / 0x140078cb0 — Ghidra names those
// `CropUI_Serialize` / `CropUI_OnCreate`, but they are idx4/idx7 of the abstract
// base table above, i.e. SelectResourceUI's own. Slot7's tail is exactly the
// "enable the combo iff it has items" logic already inlined in OrchardUI_Slot7.
#include "Object/factory_registration.h"
#include <cstdlib>
#include <cstdio>
#include "dialog_controller_vtable.h"

namespace orchardui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("OrchardUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

static void Trace(const char* what, void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    fprintf(stderr, "[OrchardUI] %s self=%p combo(+0x30)=%llx yield(+0x38)=%llx\n",
            what, self,
            self ? (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x30) : 0ull,
            self ? (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x38) : 0ull);
    fflush(stderr);
}

// idx4 (+0x20): setup. Needs the DialogInstance (+0x18) the framework seats before
// dispatch; the element finds inside are guarded.
void W_Slot4(void* self) {
    if (!self || !*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x18)) {
        Trace("Slot4 SKIP (no DialogInstance)", self);
        return;
    }
    OrchardUI_Slot4(reinterpret_cast<OrchardUI*>(self));
    Trace("Slot4", self);
}
// idx7 (+0x38): per-frame growth sync (SA_DialogEntityAlive-guarded in the port).
void W_Slot7(void* self) {
    if (!self) return;
    OrchardUI_Slot7(reinterpret_cast<OrchardUI*>(self));
}
// idx8 (+0x40): base combo refresh, plus the same standalone-only self-heal
// pastureui.cpp carries — re-run Slot4 if the yield IBP (+0x38) is still null.
void W_Slot8(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (!SA_DialogEntityAlive(self)) return;
    if (!*reinterpret_cast<void**>(s + 0x38)) {
        W_Slot4(self);
        if (!*reinterpret_cast<void**>(s + 0x38)) return;   // dialog genuinely lacks it
    }
    if (*reinterpret_cast<void**>(s + 0x30))
        SelectResourceUI_Slot8(self);
}
// idx9 (+0x48): combo selection -> Slot11. Base class slot, unlike PastureUI which
// overrides it to also handle its slider.
void W_Slot9(void* self, void* p2, int p3, int p4) {
    if (!self) return;
    Trace("Slot9(event)", self);
    SelectResourceUI_Slot9(self, p2, p3, p4);
}

struct Init {
    Init() {
        for (int i = 0; i < kSlots; ++i) g_vt[i] = reinterpret_cast<void*>(&StubNop);
        g_vt[0]  = reinterpret_cast<void*>(&Dtor);
        g_vt[2]  = reinterpret_cast<void*>(&GetTypeId);
        g_vt[4]  = reinterpret_cast<void*>(&W_Slot4);
        g_vt[7]  = reinterpret_cast<void*>(&W_Slot7);
        g_vt[8]  = reinterpret_cast<void*>(&W_Slot8);
        g_vt[9]  = reinterpret_cast<void*>(&W_Slot9);
        g_vt[11] = reinterpret_cast<void*>(&OrchardUI_Slot11);
        g_vt[12] = reinterpret_cast<void*>(&OrchardUI_Slot12);
        g_vt[13] = reinterpret_cast<void*>(&OrchardUI_Slot13);
    }
} g_init;

// Wave 10D lane VTFID: retail's OrchardUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "OrchardUI", &StubNop);

// OrchardUI object size 0x48 (+0x30 combo IBP, +0x38 yield IBP, +0x40 spare).
void Zero(void* p) {
    for (size_t o = 0; o < static_cast<size_t>(0x48); o += 8)
        *reinterpret_cast<uint64_t*>(static_cast<char*>(p) + o) = 0;
}
void* Factory() {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(0x48), 8);
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

}  // namespace orchardui_sa

REGISTER_TYPE(OrchardUI, "SelectResourceUI",
              &orchardui_sa::Factory, &orchardui_sa::InPlace);
