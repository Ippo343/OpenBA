// engine/reimpl/buildui.cpp
// Reimplementation of BuildUI vtable methods from Application-x64.exe.
//
// BuildUI (0x50 bytes), vtable at 0x14015f210.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140020A90 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400B1F40 — Slot4 (setup/refresh)
//   +0x38: 0x1400B1A50 — Slot7 (update handler)
//   +0x48: 0x1400B1EA0 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x1400209D0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x1400209C0 — 7 bytes, Slot2 (Cleanup getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x30: Slot6, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookDtor;
static HookInfo g_hookSlot4;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "BuildUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, BuildUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0x30));
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (populates build storage table)
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "BuildUI::Slot4";
    BuildUI_Slot4((BuildUI*)self);
}

// Slot7 — REIMPLEMENTED (updates build progress + materials)
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "BuildUI::Slot7";
    BuildUI_Slot7((BuildUI*)self);
}

// Slot9 — REIMPLEMENTED (toggles workplace enabled state)
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "BuildUI::Slot9";
    BuildUI_Slot9((BuildUI*)self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallBuildUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020A90);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[BuildUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildUIDtor() { RemoveHook(&g_hookDtor); }

void InstallBuildUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B1F40);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[BuildUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallBuildUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B1A50);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 13);
#ifdef _DEBUG
    printf("[BuildUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallBuildUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B1EA0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 14);
#ifdef _DEBUG
    printf("[BuildUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveBuildUISlot9() { RemoveHook(&g_hookSlot9); }
