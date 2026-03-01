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
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookOnCreate;
static HookInfo g_hookComputeLayout;
static HookInfo g_hookDraw;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_OnCreate_t)(void* self);
static void Reimpl_OnCreate(void* self) {
    ((fn_OnCreate_t)g_hookOnCreate.trampoline)(self);
}

typedef void (*fn_ComputeLayout_t)(void* self, int a, int b, int c, int d);
static void Reimpl_ComputeLayout(void* self, int a, int b, int c, int d) {
    ((fn_ComputeLayout_t)g_hookComputeLayout.trampoline)(self, a, b, c, d);
}

typedef void (*fn_Draw_t)(void* self, void* spriteCtx, uint8_t alpha);
static void Reimpl_Draw(void* self, void* spriteCtx, uint8_t alpha) {
    ((fn_Draw_t)g_hookDraw.trampoline)(self, spriteCtx, alpha);
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
