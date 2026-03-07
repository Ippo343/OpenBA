// engine/reimpl/captureboxelement.cpp
// Reimplementation of CaptureBoxElement vtable methods from Application-x64.exe.
//
// CaptureBoxElement (0xe0 bytes), vtable at 0x14015d440.
// Extends UI::Element.
//
// Element vtable (12 slots):
//   +0x00: 0x140014070 — Slot0 Dtor(self, flags)
//   +0x20: 0x140078fb0 — Slot4 OnCreate(self)
//   +0x28: 0x140078ee0 — Slot5 ComputeLayout(self, int, int, int, int)
//   +0x30: 0x140078da0 — Slot6 Draw(self, SpriteContext*, byte)
//
// Unhookable (too small):
//   +0x08: 0x140013ea0 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x140013e90 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14003a9b4 — 6 bytes, Slot3 JMP thunk
//   +0x38: 0x14003a9ba — 6 bytes, Slot7 JMP thunk
//   +0x40: 0x14003a9c0 — 6 bytes, Slot8 JMP thunk
//   +0x48: 0x14003a9c6 — 6 bytes, Slot9 JMP thunk
//   +0x50: 0x14003a9d2 — 6 bytes, Slot10 JMP thunk
//   +0x58: 0x14003a9cc — 6 bytes, Slot11 JMP thunk
//
// Second vtable (Description) at +0x60 in the object mirrors CaptureBoxDescription vtable.

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookOnCreate;
static HookInfo g_hookComputeLayout;
static HookInfo g_hookDraw;

// ============================================================
// Reimplemented functions
// ============================================================

// Dtor: set vtable, Element::~Element, conditionally free
// From decompile: sets vtable 0x14015d440, UI::Element::~Element(self), conditional free
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CaptureBoxElement::Dtor";
    static void* vtable = nullptr;
    if (!vtable) vtable = (void*)ResolveAddr(Module::Exe, 0x14015d440);
    *(void**)self = vtable;
    fn_Element_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// OnCreate: if bit 27 of self+0x34 is set (initial-size-pending flag),
// write 1 to self+0x60 and self+0x64 (initial width/height = 1),
// then clear bit 27 from self+0x34.
// Disabled because the prologue contains a short Jcc at byte 10 (SHORT_JCC bug).
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "CaptureBoxElement::OnCreate";
    uint32_t flags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x34);
    if ((flags >> 27) & 1u) {
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x60) = 1;
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x64) = 1;
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x34) = flags & ~(1u << 27);
    }
}

// ComputeLayout: Element::ComputeLayout base, GetAlignedPosition, write rect fields
// From decompile: identical pattern to FrustumOutlineElement::ComputeLayout
static void Reimpl_ComputeLayout(void* self, int x, int y, int w, int h) {
    g_lastHook = "CaptureBoxElement::ComputeLayout";
    uintptr_t s = (uintptr_t)self;
    fn_Element_ComputeLayout(self, x, y, w, h);
    int outX[2], outY, outW, outH[2];
    fn_Element_GetAlignedPosition(self, x, y, w, h, 0, 0, outX, &outY, &outW, outH, false, false);
    *(int*)(s + 0x68) = outX[0];
    *(int*)(s + 0x70) = outX[0] + outW;
    *(int*)(s + 0x6c) = outY;
    *(int*)(s + 0x74) = outY + outH[0];
}

// Draw: Element::Draw base, compute color, draw 4 lines as a rect outline
// From decompile: reads color from +0x38, checks bit 0x40000 of +0x34 for half-alpha,
// draws left/right edges (+0xd0/+0xd8 for x, +0xd4/+0xdc for y range),
// draws top/bottom edges (+0xd4/+0xdc for y, +0xd0/+0xd8 for x range)
static void Reimpl_Draw(void* self, void* spriteCtx, uint8_t alpha) {
    g_lastHook = "CaptureBoxElement::Draw";
    uintptr_t s = (uintptr_t)self;
    fn_Element_Draw(self, spriteCtx, alpha);
    uint32_t packedColor = *(uint32_t*)(s + 0x38);
    if (*(uint32_t*)(s + 0x34) & 0x40000u) {
        alpha = alpha >> 1;
    }
    uint32_t color =
        (((packedColor >> 8 & 0xffff0000u) * (uint32_t)alpha ^ packedColor) & 0xffffffu) ^
        ((packedColor >> 8 & 0xff0000u) * (uint32_t)alpha);
    void* sprite = (void*)(s + 0xc0);
    // Draw rect outline: top edge, bottom edge, left edge, right edge
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        *(int*)(s + 0xd0), *(int*)(s + 0xd4),
        *(int*)(s + 0xd8), *(int*)(s + 0xd4), color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        *(int*)(s + 0xd0), *(int*)(s + 0xdc),
        *(int*)(s + 0xd8), *(int*)(s + 0xdc), color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        *(int*)(s + 0xd0), *(int*)(s + 0xd4),
        *(int*)(s + 0xd0), *(int*)(s + 0xdc), color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        *(int*)(s + 0xd8), *(int*)(s + 0xd4),
        *(int*)(s + 0xd8), *(int*)(s + 0xdc), color);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCaptureBoxElementDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014070);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[CaptureBoxElement] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxElementDtor() { RemoveHook(&g_hookDtor); }

void InstallCaptureBoxElementOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078fb0);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[CaptureBoxElement] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxElementOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallCaptureBoxElementComputeLayout() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078ee0);
    InstallHook(target, (void*)Reimpl_ComputeLayout, &g_hookComputeLayout, 12);
#ifdef _DEBUG
    printf("[CaptureBoxElement] ComputeLayout hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxElementComputeLayout() { RemoveHook(&g_hookComputeLayout); }

void InstallCaptureBoxElementDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078da0);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 15);
#ifdef _DEBUG
    printf("[CaptureBoxElement] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveCaptureBoxElementDraw() { RemoveHook(&g_hookDraw); }
