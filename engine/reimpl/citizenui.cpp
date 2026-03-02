// engine/reimpl/citizenui.cpp
// Reimplementation of CitizenUI vtable methods from Application-x64.exe.
//
// CitizenUI (0xE8 bytes), vtable at 0x14015ba00.
// Extends DialogController (0x30 base). Has Ctor at 0x140065a40.
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010FD0 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400651A0 — Slot4 (setup/refresh)
//   +0x30: 0x140065130 — Slot6 (close/cleanup handler)
//   +0x38: 0x140064AE0 — Slot7 (update handler)
//   +0x48: 0x1400650F0 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140010F70 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140010F60 — 7 bytes, Slot2 (Cleanup getter)
//
// Stubs (base class no-ops):
//   +0x18: Slot3, +0x28: Slot5, +0x40: Slot8, +0x50: Slot10

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
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "CitizenUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, CitizenUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    uintptr_t s = (uintptr_t)self;
    fn_MemFreeStatic(*(void**)(s + 0xD0));
    *(uint64_t*)(s + 0xD0) = 0;
    *(uint64_t*)(s + 0xD8) = 0;
    fn_MemFreeStatic(*(void**)(s + 0xC0));
    *(uint64_t*)(s + 0xC0) = 0;
    *(uint64_t*)(s + 0xC8) = 0;
    fn_MemFreeStatic(*(void**)(s + 0xB0));
    *(uint64_t*)(s + 0xB0) = 0;
    *(uint64_t*)(s + 0xB8) = 0;
    fn_MemFreeStatic(*(void**)(s + 0xA0));
    *(uint64_t*)(s + 0xA0) = 0;
    *(uint64_t*)(s + 0xA8) = 0;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "CitizenUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot6 — REIMPLEMENTED (closes sub-dialog, clears reference)
#include "ui/ui_panels.h"
static void Reimpl_Slot6(void* self) {
    g_lastHook = "CitizenUI::Slot6";
    CitizenUI_Slot6((CitizenUI*)self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "CitizenUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "CitizenUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallCitizenUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010FD0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 15);
#ifdef _DEBUG
    printf("[CitizenUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveCitizenUIDtor() { RemoveHook(&g_hookDtor); }

void InstallCitizenUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400651A0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 14);
#ifdef _DEBUG
    printf("[CitizenUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveCitizenUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallCitizenUISlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140065130);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[CitizenUI] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveCitizenUISlot6() { RemoveHook(&g_hookSlot6); }

void InstallCitizenUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140064AE0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 16);
#ifdef _DEBUG
    printf("[CitizenUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveCitizenUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallCitizenUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400650F0);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[CitizenUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveCitizenUISlot9() { RemoveHook(&g_hookSlot9); }
