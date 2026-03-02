// engine/reimpl/destroyui.cpp
// Reimplementation of DestroyUI vtable methods from Application-x64.exe.
//
// DestroyUI (0x78 bytes), vtable at 0x14015bb00.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140011200 — Dtor (scalar deleting destructor)
//   +0x20: 0x140066150 — Slot4 (setup/refresh)
//   +0x38: 0x140065B00 — Slot7 (update handler)
//   +0x48: 0x140066080 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140011120 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140011110 — 7 bytes, Slot2 (Cleanup getter)
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
// Reimplemented wrappers
// ============================================================

// Dtor (scalar deleting destructor) — REIMPLEMENTED
// Original: 0x140011200. Sets vtable, calls base dtor, conditional free.
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "DestroyUI::Dtor";

    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, DestroyUIAddr::Vtable);

    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// Slot4 (setup/refresh) — PASSTHROUGH (finds 8 named elements, sets IDs, component checks)
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "DestroyUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot7 (update handler) — PASSTHROUGH (complex visibility logic, progress bars, storage queries)
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "DestroyUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

// Slot9 (notification handler) — PASSTHROUGH (button handlers: reclaim/rebuild/destroy)
typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "DestroyUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallDestroyUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140011200);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[DestroyUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyUIDtor() { RemoveHook(&g_hookDtor); }

void InstallDestroyUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140066150);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 16);
#ifdef _DEBUG
    printf("[DestroyUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallDestroyUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140065B00);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 16);
#ifdef _DEBUG
    printf("[DestroyUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallDestroyUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140066080);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[DestroyUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveDestroyUISlot9() { RemoveHook(&g_hookSlot9); }
