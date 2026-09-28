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
#include "Object/factory_registration.h"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.8 callers)
#include "Component/StorageComponent.h"   // X::TypeId() (B.3.9 CG2)

namespace storagecapacityui_sa {

// ENDGAME B.3.9 CG2: TypeId_ProgressElement -> UI::ProgressElement::TypeId(),
// TypeId_ComponentEntity -> ComponentEntity::TypeId(), TypeId_StorageComponent_FC
// -> StorageComponent::TypeId(). The class statics always answer, so the lazy-fill of
// the exe-.rdata globals retires.
void EnsureGlobals() {
}

bool Ready() {
    return true;
}

static void Trace(const char* what, void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    fprintf(stderr, "[StorageCapacityUI] %s self=%p ready=%d data30(+0x30)=%llx\n",
            what, self, Ready() ? 1 : 0,
            self ? (unsigned long long)*reinterpret_cast<uint64_t*>(s + 0x30) : 0ull);
    fflush(stderr);
}



}  // namespace storagecapacityui_sa

// ============================================================
// StorageCapacityUI::Serialize (vtable +0x20) — Find progress element
// ============================================================
// Finds the ProgressElement in the dialog by type ID and stores it
// at self+0x30. Sets the progress element's max scale to 1.0.
//
// Original: 0x14007b600.

// [4] 0x14007b600
void StorageCapacityUI::OnCreated() {
    storagecapacityui_sa::EnsureGlobals();
    if (!storagecapacityui_sa::Ready()) { storagecapacityui_sa::Trace("Serialize SKIP", this); return; }
    storagecapacityui_sa::Trace("Serialize", this);
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)this + 0x20);

    // Find ProgressElement by type
    uint32_t typeId = UI::ProgressElement::TypeId();
    uint64_t findResult = 0;
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElementType(typeId));  // B.3.6 bridge

    // Dereference InternalPtr to get raw Element*, type check, wrap in InternalBasePtr
    void* rawElem = (void*)findResult;
    bool valid = fn_IsDerivedFrom(rawElem, typeId);
    void* checkedElem = valid ? rawElem : nullptr;

    uint8_t basePtrBuf[8];
    new (basePtrBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checkedElem));   // B.3.3 bridge
    this->data30 = *(uint64_t*)basePtrBuf;

    // Set progress max scale at element +0x124 to 1.0f
    void* progElem = (void*)this->data30;
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

// [7] 0x14007b4d0
void StorageCapacityUI::OnUpdateLayout() {
    storagecapacityui_sa::EnsureGlobals();
    if (!storagecapacityui_sa::Ready()) return;
    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe. Reliable now that removals are one-frame-deferred
    // (entitylist.cpp Step 0) — the entity is valid marked-but-alive memory here.
    if (!SA_DialogEntityAlive(this)) return;

    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)this + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, ComponentEntity::TypeId());
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
    void* progElem = (void*)this->data30;
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




// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void StorageCapacityUI::OnUpdate(const System::Time& /*time*/) {
    storagecapacityui_sa::EnsureGlobals();
    if (!storagecapacityui_sa::Ready()) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (*reinterpret_cast<uint64_t*>(s + 0x30) == 0) {
        StorageCapacityUI::OnCreated();
    }
    StorageCapacityUI::OnUpdateLayout();
}

// [0] 0x140014450: ~StorageCapacityUI (restamp, ~DialogController, ~Internal — the
// compiler's chain), MemFree on &1.
Object::Base* StorageCapacityUI::DeletingDtor(unsigned flags) {
    this->~StorageCapacityUI();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x1400143f0: MemAlloc(0x38, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* StorageCapacityUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageCapacityUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageCapacityUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* StorageCapacityUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageCapacityUI());
}

DEFINE_EXE_TYPE(StorageCapacityUI, "UI::DialogController")

