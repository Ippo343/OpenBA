// code/Game/Dialog/cropui_promote.cpp
//
// CropUI: the three pure virtuals of the SelectResourceUI base, CropUI's own idx4/idx7,
// and the standalone vtable promotion.
//
// ⚠ The Slot4/Slot7/Slot8 bodies in cropui.cpp are the ABSTRACT BASE's
// (0x140078be0 / 0x140078cb0 / 0x140078d30), not CropUI's — the file's comments
// attribute them to CropUI, which is wrong. Read byte-wise from retail's vtable at
// 0x14015f270 (14 slots), CropUI's own table is:
//   idx4  (+0x20) 0x1400b2b50  CropUI_Startup — base idx4, then bind "yield"
//   idx7  (+0x38) 0x1400b2a50  CropUI_Update  — maturity into the bar, then base idx7
//   idx8  (+0x40) 0x140078d30  SelectResourceUI::Slot8 (combo refresh)
//   idx9  (+0x48) 0x140078d80  SelectResourceUI::Slot9 (combo selection -> idx11)
//   idx11 (+0x58) 0x1400b2920  set crop type on CropFieldComponent+0x50
//   idx12 (+0x60) 0x1400b29d0  get current crop type
//   idx13 (+0x68) 0x1400b29c0  category filter — bytes are `33 C0 C3`
//                              (XOR EAX,EAX; RET) = 0. Orchard is 1, pasture 2.
//
// WHY THIS FILE EXISTS: CropUI was the last member of the SelectResourceUI family still
// on UI_DIALOG_STUB (OrchardUI and PastureUI were promoted earlier), so every body in
// cropui.cpp was dead code — the combo was never bound, never given element id 500, and
// never populated, and idx9 was a no-op so a pick could not reach idx11. That is why the
// crop field's "?" button did nothing: the dropdown opened with zero rows and had no
// handler behind it. Structure follows orchardui.cpp exactly.

#include "Dialog/ui_panels.h"
#include "Dialog/cropui_funcs.h"
#include "system/Value.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Dialog/exe_bodies.h"
#include "system/String.h"
#include "Object/factory_registration.h"
#include "Work/exe_bodies.h"        // B3R: WorkAcc_FindComponent_5380
#include "Component/exe_bodies.h"   // B3R: CropFieldComponent_Maturity
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ProgressElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Component/Component.h"
#include "Entity/ComponentEntity.h"   // X::TypeId() (ENDGAME B.3.9)

namespace {

// FUN_140005380 = WorkAcc_FindComponent_5380, FUN_14011b410 =
// CropFieldComponent_Maturity: called by name (B3R).

// Resolve the CropFieldComponent behind this panel's entity. Mirrors the retail
// prologue shared by idx7/idx11/idx12: entity(+0x28) -> IsDerivedFrom(ComponentEntity)
// -> InternalBasePtr -> FUN_140005380.
void* FindComp(void* self) {
    if (!self) return nullptr;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* entity = *reinterpret_cast<void**>(s + 0x28);
    uint32_t entTid = ComponentEntity::TypeId();
    void* checked = ::Object::IsDerivedFromNullSafe(entity, entTid) ? entity : nullptr;
    uint64_t entIbp = 0;
    new (&entIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    uint64_t comp = 0;
    WorkAcc_FindComponent_5380(static_cast<uintptr_t>(entIbp), &comp);   // 0x140005380
    return reinterpret_cast<void*>(comp);
}

// ENDGAME B.3.9: UI::ProgressElement now has a ported TypeId() (a class static, always
// populated), so the gated-global-vs-private-resolve split collapses to the static.
uint32_t ProgressElementTypeId() {
    return UI::ProgressElement::TypeId();
}

}  // namespace

// ============================================================
// idx11 (+0x58) — retail 0x1400b2920. Set the crop type.
// ============================================================
// If the field currently has none and a real type is picked, retail dispatches the
// component's own vtable[14](comp, 2) BEFORE storing. Order matters: that handler reads
// +0x50 as "still unset" to decide this is a fresh planting.
// [11] 0x1400b2920
void CropUI::SetResource(int64_t newType) {
    void* comp = FindComp(this);

    // SA guard (retail unguarded): in retail a combo event can only be routed to an
    // entity that has a CropFieldComponent.
    if (!comp) return;

    int64_t cur = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50);
    if (cur == 0 && newType != 0) {
        reinterpret_cast<Component*>(comp)->Command(2);   // slot 14; B.3.8 bridge
    }
    *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50) = newType;
}

// idx12 (+0x60) — retail 0x1400b29d0. Current crop type via out-param.
// [12] 0x1400b29d0
void* CropUI::GetResource(void* out) {
    void* comp = FindComp(this);
    *reinterpret_cast<uint64_t*>(out) =
        comp ? *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(comp) + 0x50) : 0;
    return out;
}

// idx13 (+0x68) — retail 0x1400b29c0 is literally `XOR EAX,EAX; RET`.
// [13] 0x1400b29c0
int CropUI::GetCategory() {
    return 0;
}

// ============================================================
// STANDALONE promotion — real CropUI controller vtable
// ============================================================
// idx9 (+0x48) — combo selection -> idx11 — is SelectResourceUI's slot, inherited
// (retail's CropUI table holds the base's 0x140078d80); it was StubNop in the tree
// once, the slot that swallowed every pick on the "?" combo.

// [4] 0x1400b2b50
// idx4 (+0x20) — retail 0x1400b2b50: base idx4 (combo bind + element id 500 +
// populate), then find "yield" and seat it at +0x38 with progress 1.0f.
void CropUI::OnCreated() {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    // Needs the DialogInstance (+0x18) the framework seats before dispatch.
    if (!*reinterpret_cast<void**>(s + 0x18)) return;

    CropUI_Slot4(reinterpret_cast<CropUI*>(this));   // = 0x140078be0, the base's idx4

    uint64_t found = 0;
    {
        System::String nameStr("yield");
        void* parentElem = *reinterpret_cast<void**>(s + 0x20);
        ::new (&found) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameStr));  // B.3.6 bridge
    }

    void* raw = reinterpret_cast<void*>(found);
    void* checked = ::Object::IsDerivedFromNullSafe(raw, ProgressElementTypeId()) ? raw : nullptr;
    uint64_t ibp = 0;
    new (&ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    *reinterpret_cast<uint64_t*>(s + 0x38) = ibp;

    // Retail writes this unguarded (Crop.rsc always carries "yield"); guarded here for
    // the same reason orchardui.cpp is — the shared StandardDialog mount could reach a
    // layout without it, which retail cannot.
    if (*reinterpret_cast<uint64_t*>(s + 0x38))
        *reinterpret_cast<float*>(*reinterpret_cast<uint64_t*>(s + 0x38) + 0x124) = 1.0f;

}

// [7] 0x1400b2a50
// idx7 (+0x38) — retail 0x1400b2a50: mean maturity into the yield bar + percent text,
// then the base's idx7 (enable the combo iff it has items).
void CropUI::OnUpdateLayout() {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Self-heal, same as orchardui.cpp: Slot4 may not have run for this instance yet.
    if (!*reinterpret_cast<uint64_t*>(s + 0x38)) {
        CropUI::OnCreated();
        if (!*reinterpret_cast<uint64_t*>(s + 0x38)) return;
    }

    void* comp = FindComp(this);
    if (!comp) return;

    float maturity = CropFieldComponent_Maturity(comp);   // 0x14011b410

    void* yieldElem = reinterpret_cast<void*>(*reinterpret_cast<uint64_t*>(s + 0x38));
    *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(yieldElem) + 0x128) = maturity;

    int percent = static_cast<int>(maturity * 100.0f + 0.5f);
    {
        System::String textStr = System::String::Format("%i",
                         static_cast<uint64_t>(static_cast<uint32_t>(percent)));
        reinterpret_cast<UI::ProgressElement*>(yieldElem)->SetProgressTextParameter(0, textStr);  // B.3.6 bridge
    }

    if (*reinterpret_cast<uint64_t*>(s + 0x30))
        SelectResourceUI::OnUpdateLayout();   // = 0x140078cb0, the base's idx7
}

// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
// idx8 (+0x40) — base combo refresh, with the same null-combo self-heal.
void CropUI::OnUpdate(const System::Time& time) {
    if (!SA_DialogEntityAlive(this)) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (!*reinterpret_cast<uint64_t*>(s + 0x30)) {
        CropUI::OnCreated();
        if (!*reinterpret_cast<uint64_t*>(s + 0x30)) return;
    }
    SelectResourceUI::OnUpdate(time);
}




// create 0x140020b10: MemAlloc(0x60, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* CropUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CropUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CropUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* CropUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CropUI());
}

DEFINE_EXE_TYPE(CropUI, "SelectResourceUI")
