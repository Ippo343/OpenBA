// engine/reimpl/controlui.cpp
// Reimplementation of ControlUI vtable methods from Application-x64.exe.
//
// ControlUI (0x40 bytes), vtable at 0x14015b380.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010E20 — Dtor (scalar deleting destructor)
//   +0x20: 0x14005F9B0 — Slot4 (setup/refresh)
//   +0x38: 0x14005F870 — Slot7 (update handler)
//   +0x48: 0x14005F7F0 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140010BF0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140010BE0 — 7 bytes, Slot2 (Cleanup getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
    g_lastHook = "ControlUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ControlUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (finds checkbox, sub-component, sets state)
#include "Dialog/ui_panels.h"
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ControlUI::Slot4";
    ControlUI_Slot4((ControlUI*)self);
}

// Slot7 — REIMPLEMENTED (updates checkbox from component)
static void Reimpl_Slot7(void* self) {
    g_lastHook = "ControlUI::Slot7";
    ControlUI_Slot7((ControlUI*)self);
}

// Slot9 — REIMPLEMENTED (toggles component state on checkbox click)
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "ControlUI::Slot9";
    ControlUI_Slot9((ControlUI*)self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallControlUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010E20);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ControlUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveControlUIDtor() { RemoveHook(&g_hookDtor); }

void InstallControlUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F9B0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[ControlUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveControlUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallControlUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F870);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[ControlUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveControlUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallControlUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F7F0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 16);
#ifdef _DEBUG
    printf("[ControlUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveControlUISlot9() { RemoveHook(&g_hookSlot9); }
