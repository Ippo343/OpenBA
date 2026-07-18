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

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(UIComponent, 0x38, 0x80)
REGISTER_TYPE(UIComponent, "Component",
              &UIComponent_Factory, &UIComponent_CreateInPlace);
