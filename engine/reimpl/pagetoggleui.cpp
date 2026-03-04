// engine/reimpl/pagetoggleui.cpp
// Reimplementation of PageToggleUI vtable methods from Application-x64.exe.
//
// PageToggleUI (0x48 bytes), vtable at 0x14015cfb0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400131C0 — Dtor (scalar deleting destructor)
//   +0x20: 0x140075850 — Slot4 (shutdown)
//   +0x38: 0x140075780 — Slot7 (update handler)
//   +0x48: 0x140075710 — Slot9 (draw/message handler)
//
// Unhookable (too small):
//   +0x08: 0x140013150 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140013140 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x1400759A0 — 3 bytes, Slot3 (Startup, ret 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
    g_lastHook = "PageToggleUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, PageToggleUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (finds checkbox, pages element, restores active page)
#include "Dialog/ui_panels.h"
static void Reimpl_Slot4(void* self) {
    g_lastHook = "PageToggleUI::Slot4";
    PageToggleUI_Slot4((PageToggleUI*)self);
}

// Slot7 — REIMPLEMENTED (enables/disables checkbox based on page state)
static void Reimpl_Slot7(void* self) {
    g_lastHook = "PageToggleUI::Slot7";
    PageToggleUI_Slot7((PageToggleUI*)self);
}

// Slot9 — REIMPLEMENTED (toggles page 0/4 on checkbox click)
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "PageToggleUI::Slot9";
    (void)param5; (void)param6;
    PageToggleUI_Slot9((PageToggleUI*)self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPageToggleUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400131C0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[PageToggleUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemovePageToggleUIDtor() { RemoveHook(&g_hookDtor); }

void InstallPageToggleUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075850);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[PageToggleUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemovePageToggleUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallPageToggleUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075780);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[PageToggleUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemovePageToggleUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallPageToggleUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075710);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 14);
#ifdef _DEBUG
    printf("[PageToggleUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemovePageToggleUISlot9() { RemoveHook(&g_hookSlot9); }
