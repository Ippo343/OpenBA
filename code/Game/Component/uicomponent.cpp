// code/Game/Component/uicomponent.cpp
// Clean reimplementation of UIComponent vtable no-op slots.
//
// UIComponent (0x38 bytes), vtable at 0x1401649f0.
// Extends Component. Manages UI overlay data for entities.
//
// Non-trivial overrides (Shutdown, Serialize) are already reimplemented
// in engine/reimpl/uicomponent.cpp. This file covers the remaining
// vtable slots which are all no-ops in the original binary.
//
// Vtable layout:
//   +0x00: Slot0  — Dtor (0x140039760, compiler-generated, SKIP)
//   +0x08: Slot1  — GetDescription (0x140039540, type system, SKIP)
//   +0x10: Slot2  — GetTypeId (0x140039530, type system, SKIP)
//   +0x18: Slot3  — Shutdown (0x14012DD00, reimplemented in reimpl/)
//   +0x20: Slot4  — Serialize (0x14012DCA0, reimplemented in reimpl/)
//   +0x28: Slot5  — Draw (0x140118A60, RET 0 no-op)
//   +0x30: Slot6  — (0x140118820, RET 0 no-op)
//   +0x38: Slot7  — (0x140118830, RET 0 no-op)
//   +0x40: Slot8  — OnRemove (0x140118A70, RET 0 no-op)
//   +0x48: Slot9  — (0x140118A80, RET 0 no-op)
//   +0x50: Slot10 — (0x140118A90, RET 0 no-op)
//   +0x58: Slot11 — (0x140118AA0, RET 0 no-op)
//   +0x60: Slot12 — (0x140118AB0, RET 0 no-op)
//   +0x68: Slot13 — (0x140118AC0, XOR EAX,EAX; RET = return 0)
//   +0x70: Slot14 — (0x140118AD0, RET 0 no-op)
//   +0x78: Slot15 — (0x140118AE0, RET 0 no-op)

#include "UIComponent.h"
#include <cstdint>

// ============================================================
// Slot5 (vtable +0x28): Draw — no-op
// Original: 0x140118A60 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Draw(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot6 (vtable +0x30): no-op
// Original: 0x140118820 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot6(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot7 (vtable +0x38): no-op
// Original: 0x140118830 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot7(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot8 (vtable +0x40): OnRemove — no-op
// Original: 0x140118A70 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_OnRemove(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot9 (vtable +0x48): no-op
// Original: 0x140118A80 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot9(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot10 (vtable +0x50): no-op
// Original: 0x140118A90 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot10(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot11 (vtable +0x58): no-op
// Original: 0x140118AA0 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot11(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot12 (vtable +0x60): no-op
// Original: 0x140118AB0 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot12(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot13 (vtable +0x68): return 0
// Original: 0x140118AC0 (4 bytes, XOR EAX,EAX; RET, unhookable)
// ============================================================
int UIComponent_Slot13(UIComponent* self) {
    (void)self;
    return 0;
}

// ============================================================
// Slot14 (vtable +0x70): no-op
// Original: 0x140118AD0 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot14(UIComponent* self) {
    (void)self;
}

// ============================================================
// Slot15 (vtable +0x78): no-op
// Original: 0x140118AE0 (3 bytes, RET 0, unhookable)
// ============================================================
void UIComponent_Slot15(UIComponent* self) {
    (void)self;
}

// ============================================================================
// ENDGAME B.3.8 VIS — UIComponent as a REAL class (UIComponent.h).
//
// Before: COMPONENT_INERT_FACTORY_SER — a 48-slot table filled per create,
// Inert_ScalarDtor in slot 0 (only ~Internal).
// After: the compiler's 16-slot vtable and a real ctor/dtor chain
// (B38_RULES D1, D2); slot bodies are the ones the old table installed.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/UIComponent.h"

extern "C" void UIComponent_Serialize(void* self, void* stream);

// 0x14012de00: Component() is the CALL 0x1401188b0; the 0x1401649f0 stamp is
// the compiler's; +0x28 = +0x30 = 0; flags |= 0x80.
UIComponent::UIComponent() : uiData0(nullptr), uiData1(nullptr) {
    OrFlagsInCtor(0x80);
}

// [3] 0x14012dd00 — UIComponent's +0x28..+0x37 is a real growable byte array;
// the base-only Component::Persist would drop it on save/load.
void UIComponent::Persist(void* stream) { UIComponent_Serialize(this, stream); }

// [4] 0x14012dca0. PORT GAP (retail 0x14012dca0) — B38 D9: retail has its own
// Startup body; the tree's table never installed one (inert stub). The inert
// behaviour is kept here; porting it is its own titled commit.
void UIComponent::Startup() {}

// [0] 0x140039760: ~UIComponent (0x14012dda0; its array free is a PORT GAP, see the header), MemFree on &1.
Object::Base* UIComponent::DeletingDtor(unsigned flags) {
    this->~UIComponent();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140039550: MemAlloc(0x38, 8), then the ctor.
void* UIComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(UIComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) UIComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* UIComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) UIComponent());
}

DEFINE_EXE_VTABLE(UIComponent, "_ZTV11UIComponent")
DEFINE_EXE_TYPE(UIComponent, "Component")
