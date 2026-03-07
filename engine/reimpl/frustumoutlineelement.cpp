// engine/reimpl/frustumoutlineelement.cpp
// Reimplementation of FrustumOutlineElement vtable methods from Application-x64.exe.
//
// FrustumOutlineElement (0xd8 bytes), vtable at 0x14015c2a0.
// Extends UI::Element.
//
// Element vtable (12 slots):
//   +0x00: 0x140012560 — Slot0 Dtor(self, flags)
//   +0x20: 0x14006c290 — Slot4 OnCreate(self)
//   +0x28: 0x14006c1c0 — Slot5 ComputeLayout(self, int, int, int, int)
//   +0x30: 0x14006be90 — Slot6 Draw(self, SpriteContext*, byte)
//
// Unhookable (too small):
//   +0x08: 0x1400123f0 — 8 bytes, Slot1 GetTypeInfo
//   +0x10: 0x1400123e0 — 7 bytes, Slot2 GetTypeId
//   +0x18: 0x14003a9b4 — 6 bytes, Slot3 JMP thunk
//   +0x38: 0x14003a9ba — 6 bytes, Slot7 JMP thunk
//   +0x40: 0x14003a9c0 — 6 bytes, Slot8 JMP thunk
//   +0x48: 0x14003a9c6 — 6 bytes, Slot9 JMP thunk
//   +0x50: 0x14003a9d2 — 6 bytes, Slot10 JMP thunk
//   +0x58: 0x14003a9cc — 6 bytes, Slot11 JMP thunk
//
// Second vtable (Description) at +0x60 in the object mirrors FrustumOutlineDescription vtable.

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Vtable addresses (exe .rdata, resolved once at runtime)
// ============================================================
static const uintptr_t kVtable_FrustumOutlineElement = 0x14015c2a0;

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
// From decompile: sets vtable 0x14015c2a0, UI::Element::~Element(self), conditional free
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "FrustumOutlineElement::Dtor";
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable_FrustumOutlineElement);
    fn_Element_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// OnCreate: if bit 27 of self+0x34 is set (initial-size-pending flag),
// copy self+0xd0 (initial width) to self+0x60 and self+0xd4 (initial height) to self+0x64,
// then clear bit 27 from self+0x34.
// Disabled because the prologue contains a short Jcc at byte 10.
static void Reimpl_OnCreate(void* self) {
    g_lastHook = "FrustumOutlineElement::OnCreate";
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uint32_t flags = *reinterpret_cast<uint32_t*>(s + 0x34);
    if ((flags >> 27) & 1u) {
        *reinterpret_cast<uint32_t*>(s + 0x60) = *reinterpret_cast<uint32_t*>(s + 0xd0);
        *reinterpret_cast<uint32_t*>(s + 0x34) = flags & ~(1u << 27);
        *reinterpret_cast<uint32_t*>(s + 0x64) = *reinterpret_cast<uint32_t*>(s + 0xd4);
    }
}

// ComputeLayout: Element::ComputeLayout base, GetAlignedPosition, write rect fields
// From decompile: identical pattern to CaptureBoxElement::ComputeLayout
static void Reimpl_ComputeLayout(void* self, int x, int y, int w, int h) {
    g_lastHook = "FrustumOutlineElement::ComputeLayout";
    uintptr_t s = (uintptr_t)self;
    fn_Element_ComputeLayout(self, x, y, w, h);
    int outX[2], outY, outW, outH[2];
    fn_Element_GetAlignedPosition(self, x, y, w, h, 0, 0, outX, &outY, &outW, outH, false, false);
    *(int*)(s + 0x68) = outX[0];
    *(int*)(s + 0x70) = outX[0] + outW;
    *(int*)(s + 0x6c) = outY;
    *(int*)(s + 0x74) = outY + outH[0];
}

// Draw: calls Element::Draw base, projects camera frustum corners to screen-space,
// then draws 4 outline lines forming a quadrilateral.
// Disabled because the prologue contains MOV RAX/R11,RSP (bug class 3 RSP capture).
//
// Clean reimplementation — no trampoline needed.
//
// Element layout (relevant fields at FrustumOutlineElement offsets):
//   +0x34 = flags (bit 18 = 0x40000 → half-alpha mode)
//   +0x38 = packed RGB color (no alpha in low bits; alpha comes from the alpha param)
//   +0x68 = element screen x
//   +0x6c = element screen y
//   +0xc0 = Sprite (16 bytes, address passed to AddLine)
//   +0xd0 = element pixel width
//   +0xd4 = element pixel height
//
// fn_FrustumProject(outCorners): writes 4 Vec3s (x,y,z) as 12 floats — 4 screen-space
// corners of the camera frustum projected onto the UI plane. The quadrilateral edges
// drawn are: corner0→corner1, corner1→corner3, corner2→corner3, corner2→corner0.
//
// SetScissor ABI (verified from disassembly at 14006c056 / 14006c179):
//   SetScissor(this, ctx, x, y, w, h) — MSVC member, non-trivial Viewport return
//     RCX=this, RDX=&savedVP(retval), R8=x, R9=y, [RSP+0x20]=w, [RSP+0x28]=h
//   SetScissor(this, ctx, &restoreVP) — restore overload
//     RCX=this, RDX=&discardVP(retval), R8=&savedVP(input)
//
// AddLine ABI: RCX=this, RDX=sprite*, R8=x1, R9=y1, [RSP+0x20]=x2, [RSP+0x28]=y2, [RSP+0x30]=color
typedef void (*fn_SetScissor4_hidden_t)(void* self, void* retval, int x, int y, int w, int h);
typedef void (*fn_SetScissorVP_hidden_t)(void* self, void* retval, void* inputVP);

static void Reimpl_Draw(void* self, void* spriteCtx, uint8_t alpha) {
    g_lastHook = "FrustumOutlineElement::Draw";

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Call base UI::Element::Draw
    fn_Element_Draw(self, spriteCtx, alpha);

    // Half alpha if flag bit 18 set
    uint32_t packedColor = *reinterpret_cast<uint32_t*>(s + 0x38);
    if (*reinterpret_cast<uint32_t*>(s + 0x34) & 0x40000u) {
        alpha = alpha >> 1;
    }
    // Color formula from decompile: strips stored alpha, applies render alpha
    uint32_t color =
        (((packedColor >> 8 & 0xffff0000u) * (uint32_t)alpha ^ packedColor) & 0xffffffu) ^
        ((packedColor >> 8 & 0xff0000u) * (uint32_t)alpha);

    // Project frustum corners: 4 Vec3s (x,y,z) as 12 floats
    float corners[12];
    fn_FrustumProject(corners);

    int elemX = *reinterpret_cast<int*>(s + 0x68);
    int elemY = *reinterpret_cast<int*>(s + 0x6c);
    int elemW = *reinterpret_cast<int*>(s + 0xd0);
    int elemH = *reinterpret_cast<int*>(s + 0xd4);
    void* sprite = reinterpret_cast<void*>(s + 0xc0);

    // SetScissor to clip draw region, saving old viewport (16-byte retval)
    // ABI: RCX=spriteCtx, RDX=&savedVP, R8=elemX, R9=elemY, [RSP+0x20]=elemW, [RSP+0x28]=elemH
    uint8_t savedViewport[16];
    uint8_t discardViewport[16];
    auto pfnSetScissor4 = reinterpret_cast<fn_SetScissor4_hidden_t>(fn_SpriteContext_SetScissor4);
    pfnSetScissor4(spriteCtx, savedViewport, elemX, elemY, elemW, elemH);

    // Draw quadrilateral outline edges
    // corners[i*3] = x, corners[i*3+1] = y, corners[i*3+2] = z (ignored for 2D)
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        (int)corners[0] + elemX, (int)corners[1] + elemY,
        (int)corners[3] + elemX, (int)corners[4] + elemY, color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        (int)corners[3] + elemX, (int)corners[4] + elemY,
        (int)corners[9] + elemX, (int)corners[10] + elemY, color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        (int)corners[6] + elemX, (int)corners[7] + elemY,
        (int)corners[9] + elemX, (int)corners[10] + elemY, color);
    fn_SpriteContext_AddLine(spriteCtx, sprite,
        (int)corners[6] + elemX, (int)corners[7] + elemY,
        (int)corners[0] + elemX, (int)corners[1] + elemY, color);

    // Restore scissor. ABI: RCX=spriteCtx, RDX=&discardVP, R8=&savedVP
    auto pfnSetScissorVP = reinterpret_cast<fn_SetScissorVP_hidden_t>(fn_SpriteContext_SetScissorVP);
    pfnSetScissorVP(spriteCtx, discardViewport, savedViewport);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallFrustumOutlineElementDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140012560);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[FrustumOutlineElement] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineElementDtor() { RemoveHook(&g_hookDtor); }

void InstallFrustumOutlineElementOnCreate() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006c290);
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 12);
#ifdef _DEBUG
    printf("[FrustumOutlineElement] OnCreate hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineElementOnCreate() { RemoveHook(&g_hookOnCreate); }

void InstallFrustumOutlineElementComputeLayout() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006c1c0);
    InstallHook(target, (void*)Reimpl_ComputeLayout, &g_hookComputeLayout, 12);
#ifdef _DEBUG
    printf("[FrustumOutlineElement] ComputeLayout hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineElementComputeLayout() { RemoveHook(&g_hookComputeLayout); }

void InstallFrustumOutlineElementDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006be90);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 15);
#ifdef _DEBUG
    printf("[FrustumOutlineElement] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveFrustumOutlineElementDraw() { RemoveHook(&g_hookDraw); }
