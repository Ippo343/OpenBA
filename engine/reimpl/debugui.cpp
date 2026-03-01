// engine/reimpl/debugui.cpp
// Reimplementation of DebugUI vtable methods from Application-x64.exe.
//
// DebugUI (0x130 bytes), vtable at 0x14015b870.
// Extends DialogController (0x30 base). Has Ctor at 0x140064630.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010F10 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400619E0 — Slot4 (setup/refresh)
//   +0x30: 0x1400645F0 — Slot6 (3-byte RET, unhookable)
//   +0x38: 0x140061280 — Slot7 (update handler)
//   +0x40: 0x1400610C0 — Slot8 (additional handler)
//   +0x48: 0x140060BC0 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140010EB0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140010EA0 — 7 bytes, Slot2 (Cleanup getter)
//   +0x30: 0x1400645F0 — 3 bytes, Slot6 (RET 0)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x50: Slot10

#include "ui/Panels.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookDtor;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDebugUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010F10);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[DebugUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveDebugUIDtor() { RemoveHook(&g_hookDtor); }

void InstallDebugUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400619E0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[DebugUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveDebugUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallDebugUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140061280);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 15);
#ifdef _DEBUG
    printf("[DebugUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveDebugUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallDebugUISlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400610C0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 19);
#ifdef _DEBUG
    printf("[DebugUI] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveDebugUISlot8() { RemoveHook(&g_hookSlot8); }

void InstallDebugUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140060BC0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[DebugUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveDebugUISlot9() { RemoveHook(&g_hookSlot9); }
