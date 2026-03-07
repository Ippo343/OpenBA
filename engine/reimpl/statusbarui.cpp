// engine/reimpl/statusbarui.cpp
// Reimplementation of StatusBarUI vtable methods from Application-x64.exe.
//
// StatusBarUI (0x1D0 bytes), vtable at 0x14015d810. Largest panel.
// Extends DialogController (0x30 base). Has Ctor at 0x14007af20.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140014130 — ScalarDtor(self, flags) [15b prologue]
//   +0x28: 0x14007a3d0 — Draw(self) [607 instrs, 15b prologue]
//   +0x38: 0x1400798d0 — Startup(self) [493 instrs, 15b prologue]
//   +0x48: 0x14007a3a0 — OnRemove(self, ?, eventType, eventId) [13b prologue]
//
// Unhookable (too small or thunk):
//   +0x08: 0x1400140d0 — GetDescription, 8 bytes
//   +0x10: 0x1400140c0 — GetTypeId, 7 bytes
//   +0x18: 0x14003a95a — thunk (6b JMP)
//   +0x20: 0x14003a984 — thunk (6b JMP)
//   +0x30: 0x14007a3c0 — 3 bytes (RET 0)
//   +0x40: 0x14003a96c — thunk (6b JMP)
//   +0x50: 0x14003a978 — thunk (6b JMP)

#include "ui/Panels.h"
#include "Dialog/ui_panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookScalarDtor;
static HookInfo g_hookDraw;
static HookInfo g_hookStartup;
static HookInfo g_hookOnRemove;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
static void* Reimpl_ScalarDtor(void* self, uint32_t flags) {
    g_lastHook = "StatusBarUI::ScalarDtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, StatusBarUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x138));
    *(uint64_t*)(s + 0x138) = 0;
    *(uint64_t*)(s + 0x140) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x128));
    *(uint64_t*)(s + 0x128) = 0;
    *(uint64_t*)(s + 0x130) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x118));
    *(uint64_t*)(s + 0x118) = 0;
    *(uint64_t*)(s + 0x120) = 0;
    fn_MemFreeStatic(*(void**)(s + 0x108));
    *(uint64_t*)(s + 0x108) = 0;
    *(uint64_t*)(s + 0x110) = 0;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Draw_t)(void* self);
static void Reimpl_Draw(void* self) {
    g_lastHook = "StatusBarUI::Draw";
    // REIMPLEMENTED — call clean code
    StatusBarUI_Draw((StatusBarUI*)self);
}

typedef void (*fn_Startup_t)(void* self);
static void Reimpl_Startup(void* self) {
    g_lastHook = "StatusBarUI::Startup";
    // REIMPLEMENTED — call clean code
    StatusBarUI_Startup((StatusBarUI*)self);
}

typedef void (*fn_OnRemove_t)(void* self, void* param2, int eventType, int eventId);
static void Reimpl_OnRemove(void* self, void* param2, int eventType, int eventId) {
    g_lastHook = "StatusBarUI::OnRemove";
    ((fn_OnRemove_t)g_hookOnRemove.trampoline)(self, param2, eventType, eventId);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallStatusBarUIScalarDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140014130);
    InstallHook(target, (void*)Reimpl_ScalarDtor, &g_hookScalarDtor, 15);
#ifdef _DEBUG
    printf("[StatusBarUI] ScalarDtor hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusBarUIScalarDtor() { RemoveHook(&g_hookScalarDtor); }

void InstallStatusBarUIDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007a3d0);
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 18);
#ifdef _DEBUG
    printf("[StatusBarUI] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusBarUIDraw() { RemoveHook(&g_hookDraw); }

void InstallStatusBarUIStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400798d0);
    InstallHook(target, (void*)Reimpl_Startup, &g_hookStartup, 15);
#ifdef _DEBUG
    printf("[StatusBarUI] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusBarUIStartup() { RemoveHook(&g_hookStartup); }

void InstallStatusBarUIOnRemove() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14007a3a0);
    InstallHook(target, (void*)Reimpl_OnRemove, &g_hookOnRemove, 13);
#ifdef _DEBUG
    printf("[StatusBarUI] OnRemove hooked at %p\n", (void*)target);
#endif
}
void RemoveStatusBarUIOnRemove() { RemoveHook(&g_hookOnRemove); }
