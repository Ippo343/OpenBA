// engine/reimpl/helpui.cpp
// Reimplementation of HelpUI vtable methods from Application-x64.exe.
//
// HelpUI (0x78 bytes), vtable at 0x14015bf30.
// Extends DialogController (0x30 base). Has Ctor at 0x14006a920.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140011CF0 — Dtor (scalar deleting destructor)
//   +0x20: 0x14006A500 — Slot4 (setup/refresh)
//   +0x28: 0x14006A4F0 — Slot5 (3-byte RET, unhookable)
//   +0x38: 0x14006A230 — Slot7 (update handler)
//   +0x48: 0x14006A290 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140011C90 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140011C80 — 7 bytes, Slot2 (Cleanup getter)
//   +0x28: 0x14006A4F0 — 3 bytes, Slot5 (RET 0)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "HelpUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, HelpUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x68));
    *(uint64_t*)(s + 0x68) = 0;
    *(uint64_t*)(s + 0x70) = 0;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "HelpUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot7 — REIMPLEMENTED (enables/disables prev/next buttons)
#include "ui/ui_panels.h"
static void Reimpl_Slot7(void* self) {
    g_lastHook = "HelpUI::Slot7";
    HelpUI_Slot7((HelpUI*)self);
}

typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "HelpUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHelpUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011CF0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[HelpUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveHelpUIDtor() { RemoveHook(&g_hookDtor); }

void InstallHelpUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006A500);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[HelpUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveHelpUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallHelpUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006A230);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[HelpUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveHelpUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallHelpUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006A290);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 16);
#ifdef _DEBUG
    printf("[HelpUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveHelpUISlot9() { RemoveHook(&g_hookSlot9); }
