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
