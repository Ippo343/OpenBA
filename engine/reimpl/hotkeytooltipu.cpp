// engine/reimpl/hotkeytooltipu.cpp
// Reimplementation of HotKeyToolTipUI vtable methods from Application-x64.exe.
//
// HotKeyToolTipUI (0x30 bytes), vtable at 0x14015bfc0.
// Extends DialogController (0x30 base). No subclass fields.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140011E70 — Dtor (scalar deleting destructor)
//   +0x20: 0x14006A980 — Slot4 (shutdown)
//
// Unhookable (too small):
//   +0x08: 0x140011E10 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140011E00 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14006AB80 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x38: Slot7, +0x40: Slot8, +0x48: Slot9, +0x50: Slot10

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

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "HotKeyToolTipUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "HotKeyToolTipUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallHotKeyToolTipUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011E70);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[HotKeyToolTipUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveHotKeyToolTipUIDtor() { RemoveHook(&g_hookDtor); }

void InstallHotKeyToolTipUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14006A980);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[HotKeyToolTipUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveHotKeyToolTipUISlot4() { RemoveHook(&g_hookSlot4); }
