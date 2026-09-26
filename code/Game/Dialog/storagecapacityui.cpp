// engine/src/ui/storagecapacityui.cpp
// Clean reimplementation of StorageCapacityUI game logic.
//
// StorageCapacityUI (0x38 bytes) — storage capacity progress bar.
// Extends DialogController. data30 = progress element InternalPtr.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ProgressElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)

// ============================================================
// StorageCapacityUI::Serialize (vtable +0x20) — Find progress element
// ============================================================
// Finds the ProgressElement in the dialog by type ID and stores it
// at self+0x30. Sets the progress element's max scale to 1.0.
//
// Original: 0x14007b600.

void StorageCapacityUI_Serialize(StorageCapacityUI* self) {
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find ProgressElement by type
    uint32_t typeId = g_TypeId_ProgressElement ? *g_TypeId_ProgressElement : 0;
    uint64_t findResult = 0;
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(typeId));  // B.3.6 bridge

    // Dereference InternalPtr to get raw Element*, type check, wrap in InternalBasePtr
    void* rawElem = (void*)findResult;
    bool valid = fn_IsDerivedFrom(rawElem, typeId);
    void* checkedElem = valid ? rawElem : nullptr;

    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checkedElem));   // B.3.3 bridge
    self->data30 = *(uint64_t*)basePtrBuf;

    // Set progress max scale at element +0x124 to 1.0f
    void* progElem = (void*)self->data30;
    *(float*)((uintptr_t)progElem + 0x124) = 1.0f;
}

// ============================================================
// StorageCapacityUI::Startup (vtable +0x38) — Update progress display
// ============================================================
// Queries the entity's storage component for current count and capacity,
// calculates the fill ratio, and updates the progress bar with a
// percentage string.
//
// Original: 0x14007b4d0.

void StorageCapacityUI_Startup(StorageCapacityUI* self) {
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe. Reliable now that removals are one-frame-deferred
    // (entitylist.cpp Step 0) — the entity is valid marked-but-alive memory here.
    if (!SA_DialogEntityAlive(self)) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    void* entityInternal = *(void**)entityBuf;

    // Find StorageComponent via FUN_140006270
    void* component = nullptr;
    fn_FindComponent_Storage(entityInternal, &component);
    // SA safety: a partially-materialized storage entity can resolve a null-wrapped
    // component IBP. Skip crash-safe.
    if (!component) return;

    // Query current count
    int count = fn_StorageComponent_QueryCount(component);

    // Get capacity from component +0x38
    int capacity = *(int*)((uintptr_t)component + 0x38);

    // Calculate fill ratio
    float ratio;
    if (capacity == 0) {
        ratio = 0.0f;
    } else {
        ratio = (float)count / (float)capacity;
    }

    // Calculate display percentage: (int)(ratio * 100.0 + 0.5)
    float displayVal = ratio * 100.0f + 0.5f;

    // Store ratio at progress element +0x128
    void* progElem = (void*)self->data30;
    // SA safety: Serialize only caches data30 when the dialog template actually
    // contains a ProgressElement. Skip if it did not.
    if (!progElem) return;
    *(float*)((uintptr_t)progElem + 0x128) = ratio;

    // Format percentage string
    System::String str = System::String::Format("%i", (uint64_t)(uint32_t)(int)displayVal);

    // Set on progress element
    reinterpret_cast<UI::ProgressElement*>(progElem)->SetProgressTextParameter(0, str);  // B.3.6 bridge
}

// ============================================================
// STANDALONE promotion — real StorageCapacityUI controller vtable
// ============================================================
// A storage building's info panel (StorageBarn / StockpileYard) contains a
// ProgressElement bound to a StorageCapacityUI controller that shows the "X%"
// fill number. In standalone this type was a StubNop (all slots no-op, see
// ui_dialog_stub_factories.cpp — that stub registration is REMOVED there to avoid
// a duplicate REGISTER_TYPE), so Serialize (find the progress element) and Startup
// (compute count/capacity and set the % text) never ran and the number never
// appeared. This installs a real per-class vtable wiring the two reimplemented
// methods above, following the StorageUI/ResidenceUI promotion pattern.
//
// Vtable offsets (verified from the exe StorageCapacityUI vtable @ 0x14015d950):
//   +0x00 (idx0) ScalarDtor  (0x140014450 — trivial: reset vtable + free)
//   +0x10 (idx2) GetTypeId   (0x1400143d0)
//   +0x20 (idx4) Serialize   (0x14007b600) — find ProgressElement, cache at +0x30
//   +0x38 (idx7) Startup     (0x14007b4d0) — compute % and set the progress text
//   +0x40 (idx8)             — base DialogController::OnUpdate (per-frame). We drive
//                the % refresh here so it stays live as the barn fills/drains: run
//                Serialize once (until data30 is cached), then Startup every frame.
#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "dialog_controller_vtable.h"

namespace storagecapacityui_sa {

constexpr int kSlots = 48;
static void* g_vt[kSlots];

void StubNop(void*, ...) {}

uint32_t GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("StorageCapacityUI");
    return c;
}

void* Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    *reinterpret_cast<void**>(self) = g_vt;
    // data30 (+0x30) is an InternalBasePtr value, not an owned heap block — nothing
    // to free (mirrors the exe StorageCapacityUI_ScalarDtor, which only frees self).
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// The exe-.rdata type-id globals Serialize/Startup dereference are null in standalone.
// Lazy-fill from the live runtime type registry (valid after Object::Factory::Startup).
void EnsureGlobals() {
    static uint32_t s_prog = 0, s_ce = 0;
    if (!g_TypeId_ProgressElement) {
        s_prog = ::Object::ResolveTypeIdByNameLazy("ProgressElement");
        if (s_prog) g_TypeId_ProgressElement = &s_prog;
    }
    if (!g_TypeId_ComponentEntity) {
        s_ce = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ce) g_TypeId_ComponentEntity = &s_ce;
    }
    // g_TypeId_StorageComponent_FC (used by fn_FindComponent_Storage) is filled by
    // engine_funcs init in standalone.
}

bool Ready() {
    return g_TypeId_ProgressElement && *g_TypeId_ProgressElement
        && g_TypeId_ComponentEntity && *g_TypeId_ComponentEntity
        && g_TypeId_StorageComponent_FC && *g_TypeId_StorageComponent_FC;
}

static void Trace(const char* what, void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    fprintf(stderr, "[StorageCapacityUI] %s self=%p ready=%d data30(+0x30)=%llx\n",
            what, self, Ready() ? 1 : 0,
            self ? (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x30) : 0ull);
    fflush(stderr);
}

// idx4 (+0x20): Serialize — locate the ProgressElement, cache it at +0x30.
void W_Serialize(void* self) {
    EnsureGlobals();
    if (!self || !Ready()) { Trace("Serialize SKIP", self); return; }
    Trace("Serialize", self);
    StorageCapacityUI_Serialize(reinterpret_cast<StorageCapacityUI*>(self));
}

// idx7 (+0x38): Startup — compute the fill % and push it to the progress text.
void W_Startup(void* self) {
    EnsureGlobals();
    if (!self || !Ready()) return;
    StorageCapacityUI_Startup(reinterpret_cast<StorageCapacityUI*>(self));
}

// idx8 (+0x40): per-frame. Cache the progress element once (data30 == 0), then keep
// the % live by recomputing every frame.
void W_OnUpdate(void* self, void* /*time*/) {
    EnsureGlobals();
    if (!self || !Ready()) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) {
        StorageCapacityUI_Serialize(reinterpret_cast<StorageCapacityUI*>(self));
    }
    StorageCapacityUI_Startup(reinterpret_cast<StorageCapacityUI*>(self));
}

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

// Wave 10D lane VTFID: retail's StorageCapacityUI vtable INHERITS the base
// entry in the slots it does not override; ours held a file-local
// no-op. Fill exactly those slots (mask read from the exe), and only
// where this file has not deliberately overridden them.
DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "StorageCapacityUI", &StubNop);

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

}  // namespace storagecapacityui_sa

REGISTER_TYPE(StorageCapacityUI, "UI::DialogController",
              &storagecapacityui_sa::Factory, &storagecapacityui_sa::InPlace);

