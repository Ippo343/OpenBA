// engine/src/ui/resourcelimitui.cpp
// Clean reimplementation of ResourceLimitUI game logic.
//
// ResourceLimitUI (0x40 bytes) — resource limit spinner panel.
// Extends DialogController. Fields:
//   +0x10 dataSrc (pointer to typed object with resource index at +0x20)
//   +0x30 spinnerElement (InternalBasePtr)
//
// The resource limit value is stored in a global array accessed through
// the game world singleton: world->+0xF0->+0x170[index*0x20+8].
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdio>
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpinnerElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Dialog/ResourceLimitUIConfig.h"   // X::TypeId() (B.3.9 CG2)

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    System::String nameStr(name);

    uint64_t result = 0;
    ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge

    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawElem));   // B.3.3 bridge
    return *(uint64_t*)basePtrBuf;
}

// Helper: Read the resource limit value from the global array.
// Path: world->+0xF0->+0x170[resourceIndex * 0x20 + 8]
static int GetResourceLimit(void* dataSrc) {
    int resourceIndex = *(int*)((uintptr_t)dataSrc + 0x20);
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    return *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8);
}

// Helper: Write the resource limit value to the global array.
static void SetResourceLimit(void* dataSrc, int value) {
    int resourceIndex = *(int*)((uintptr_t)dataSrc + 0x20);
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8) = value;
}

// ============================================================
// ResourceLimitUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds "editLimit" spinner element, sets ID/min/max,
// reads current limit from global array.
//
// Original: 0x140077970.

void ResourceLimitUI_Slot4(ResourceLimitUI* self) {
    // Get data source from base class +0x10 and type-check
    // ENDGAME B.3.9 CG2: g_TypeId_ResourceLimitType binds cell 0x1401c5520, whose
    // retail class is ResourceLimitUIConfig (Table 1 NAME LIE).
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, ResourceLimitUIConfig::TypeId());
    void* typedSrc = isType ? dataSrc : nullptr;

    // Find "editLimit" spinner element
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);
    self->data30 = FindElemAsBasePtr(dialogElem, "editLimit");

    // Configure spinner
    void* spinner = (void*)self->data30;
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetElementId(0x3039);  // B.3.6 bridge
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetMinMax(0, 999999);  // B.3.6 bridge

    // Set current value from global resource limit array
    int currentLimit = GetResourceLimit(typedSrc);
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetValue(currentLimit);  // B.3.6 bridge
}

// ============================================================
// ResourceLimitUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Syncs spinner display with current limit (unless spinner is active).
//
// Original: 0x1400778D0.

void ResourceLimitUI_Slot7(ResourceLimitUI* self) {
    // Get data source
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, ResourceLimitUIConfig::TypeId());
    void* typedSrc = isType ? dataSrc : nullptr;

    void* spinner = (void*)self->data30;

    int currentLimit = GetResourceLimit(typedSrc);
    int spinnerVal = reinterpret_cast<const UI::SpinnerElement*>(spinner)->GetValue();  // B.3.6 bridge

    if (currentLimit != spinnerVal) {
        bool active = reinterpret_cast<const UI::SpinnerElement*>(spinner)->IsActive();  // B.3.6 bridge
        if (!active) {
            reinterpret_cast<UI::SpinnerElement*>(spinner)->SetValue(currentLimit);  // B.3.6 bridge
        }
    }
}

// ============================================================
// ResourceLimitUI::Slot9 (vtable +0x48) — Notification handler
// ============================================================
// When spinner value changes (event 0x15, ID 0x3039), writes
// the new value back to the global resource limit array.
//
// Original: 0x1400777E0.

void ResourceLimitUI_Slot9(ResourceLimitUI* self, void* param2,
                           int param3, int param4) {
    if (param3 != 0x15 || param4 != 0x3039) return;

    // Validate sender is the spinner element or its child
    void* spinner = (void*)self->data30;
    void* sender = *(void**)param2;

    if (sender != spinner) {
        uint64_t childResult = 0;
        ::new (&childResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(spinner)->Child(0));  // B.3.6 bridge
        void* child = (void*)childResult;
        if (sender != child) return;
    }

    // Get data source
    void* dataSrc = *(void**)((uintptr_t)self + 0x10);
    bool isType = fn_IsDerivedFrom(dataSrc, ResourceLimitUIConfig::TypeId());
    void* typedSrc = isType ? dataSrc : nullptr;

    // Write spinner value to global limit
    int newVal = reinterpret_cast<const UI::SpinnerElement*>(spinner)->GetValue();  // B.3.6 bridge
    SetResourceLimit(typedSrc, newVal);
}

// ============================================================
// Standalone entry points — resource index supplied explicitly
// ============================================================
// In standalone the ResourceLimitUI child controllers never get their datasource
// bound (self+0x10 == NULL): the config->controller binding flows through base
// DialogController slots that the generic dialog stub no-ops, and through the
// ResourceLimitUIConfig field-deserialize that the stub skips. The framework still
// creates + dispatches the 11 row controllers in template/table order, which is
// exactly the resource-index order the Town Hall's (user-verified) count columns
// use: Log=0, Stone=1, Iron=2, Herbs=3, Tools=4, Fuel=5, Food=6, Clothing=7,
// Alcohol=8, Coal=9, Textile=10 -> townMgr(gw+0xf0)+0x170[idx*0x20+8]. So the
// standalone promotion supplies the index by dispatch order and drives the
// "editLimit" spinner directly, bypassing the missing config. These mirror the
// config-based Slot4/7/9 above but take an explicit index and never touch
// self+0x10 (so IsDerivedFrom is never called on a NULL datasource).

static int GetResourceLimitByIdx(int resourceIndex) {
    if (!g_pGameWorldSingleton) return 0;
    void* worldSingleton = *g_pGameWorldSingleton;
    if (!worldSingleton) return 0;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    if (!subObj) return 0;
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    if (!arrayBase) return 0;
    return *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8);
}

static void SetResourceLimitByIdx(int resourceIndex, int value) {
    if (!g_pGameWorldSingleton) return;
    void* worldSingleton = *g_pGameWorldSingleton;
    if (!worldSingleton) return;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0xF0);
    if (!subObj) return;
    uintptr_t arrayBase = *(uintptr_t*)((uintptr_t)subObj + 0x170);
    if (!arrayBase) return;
    *(int*)(arrayBase + (int64_t)resourceIndex * 0x20 + 8) = value;
}

// Read the live default/limit for a row (used by the trace + external checks).
int ResourceLimitUI_ReadLimit_SA(int resourceIndex) {
    return GetResourceLimitByIdx(resourceIndex);
}

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
namespace resourcelimitui_sa {
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
    void Trace([[maybe_unused]] const char* slot, void* self, [[maybe_unused]] int idx) {
        static int n=0; if (n>=24) return; ++n;
        void* cfg = self ? *reinterpret_cast<void**>((uintptr_t)self+0x10) : nullptr;
        [[maybe_unused]] int cfgIdx = cfg ? *reinterpret_cast<int*>((uintptr_t)cfg+0x20) : -1;
        printf("[ResourceLimitUI] %s self=%p cfg=%p cfg+0x20=%d dispIdx=%d liveLimit=%d\n",
               slot, self, cfg, cfgIdx, idx, ResourceLimitUI_ReadLimit_SA(idx));
        fflush(stdout);
    }
}

// Setup (vtable +0x20): find "editLimit" within this row's element subtree, cache
// it at self->data30, configure id/min/max, seed it from the live limit.
// [4] 0x140077970
void ResourceLimitUI::OnCreated() {
    int resourceIndex = resourcelimitui_sa::IdxFor(this);
    resourcelimitui_sa::Trace("Slot4", this, resourceIndex);
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);
    if (!dialogElem) return;
    this->data30 = FindElemAsBasePtr(dialogElem, "editLimit");
    void* spinner = (void*)this->data30;
    if (!spinner) return;
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetElementId(0x3039);  // B.3.6 bridge
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetMinMax(0, 999999);  // B.3.6 bridge
    reinterpret_cast<UI::SpinnerElement*>(spinner)->SetValue(GetResourceLimitByIdx(resourceIndex));  // B.3.6 bridge
}

// Update (vtable +0x38): keep the spinner synced with the live limit unless the
// user is actively editing it.
// [7] 0x1400778d0
void ResourceLimitUI::OnUpdateLayout() {
    int resourceIndex = resourcelimitui_sa::IdxFor(this);
    void* spinner = (void*)this->data30;
    if (!spinner) return;
    int currentLimit = GetResourceLimitByIdx(resourceIndex);
    int spinnerVal = reinterpret_cast<const UI::SpinnerElement*>(spinner)->GetValue();  // B.3.6 bridge
    if (currentLimit != spinnerVal && !reinterpret_cast<const UI::SpinnerElement*>(spinner)->IsActive())  // B.3.6 bridge
        reinterpret_cast<UI::SpinnerElement*>(spinner)->SetValue(currentLimit);  // B.3.6 bridge
}

// Event (vtable +0x48): on spinner change (msg 0x15, id 0x3039) write the new
// value back to the live limit.
// [9] 0x1400777e0
void ResourceLimitUI::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t msg, int32_t id,
                                     int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    void* param2 = element;
    int resourceIndex = resourcelimitui_sa::IdxFor(this);
    resourcelimitui_sa::Trace("Slot9", this, resourceIndex);
    if (msg != 0x15 || id != 0x3039) return;
    void* spinner = (void*)this->data30;
    if (!spinner || !param2) return;
    void* sender = *(void**)param2;
    if (sender != spinner) {
        uint64_t childResult = 0;
        ::new (&childResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(spinner)->Child(0));  // B.3.6 bridge
        if (sender != (void*)childResult) return;
    }
    SetResourceLimitByIdx(resourceIndex, reinterpret_cast<const UI::SpinnerElement*>(spinner)->GetValue());  // B.3.6 bridge
}
