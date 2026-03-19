#pragma once
// Function declarations for deeply reimplemented UI Component methods.
// These are clean game logic functions for standalone exe.

#include "UIComponent.h"

// ============================================================
// UIComponent — vtable no-op slots
// ============================================================
// All slots below are no-ops in the original binary (RET 0 or XOR EAX,EAX; RET).
// Needed for standalone exe vtable completeness.

// Slot5 (vtable +0x28): Draw — no-op (0x140118A60)
void UIComponent_Draw(UIComponent* self);

// Slot6 (vtable +0x30): no-op (0x140118820)
void UIComponent_Slot6(UIComponent* self);

// Slot7 (vtable +0x38): no-op (0x140118830)
void UIComponent_Slot7(UIComponent* self);

// Slot8 (vtable +0x40): OnRemove — no-op (0x140118A70)
void UIComponent_OnRemove(UIComponent* self);

// Slot9 (vtable +0x48): no-op (0x140118A80)
void UIComponent_Slot9(UIComponent* self);

// Slot10 (vtable +0x50): no-op (0x140118A90)
void UIComponent_Slot10(UIComponent* self);

// Slot11 (vtable +0x58): no-op (0x140118AA0)
void UIComponent_Slot11(UIComponent* self);

// Slot12 (vtable +0x60): no-op (0x140118AB0)
void UIComponent_Slot12(UIComponent* self);

// Slot13 (vtable +0x68): return 0 (0x140118AC0)
int UIComponent_Slot13(UIComponent* self);

// Slot14 (vtable +0x70): no-op (0x140118AD0)
void UIComponent_Slot14(UIComponent* self);

// Slot15 (vtable +0x78): no-op (0x140118AE0)
void UIComponent_Slot15(UIComponent* self);
