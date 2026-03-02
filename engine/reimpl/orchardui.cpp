// engine/reimpl/orchardui.cpp
// Reimplementation of OrchardUI vtable methods from Application-x64.exe.
//
// OrchardUI (0x48 bytes), vtable at 0x14015cf30.
// Extends SelectResourceUI (which extends DialogController, 0x30 base).
// 14-slot vtable (11 base + 3 SelectResourceUI extensions).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400130E0 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400755B0 — Slot4 (shutdown)
//   +0x38: 0x1400754B0 — Slot7 (update handler)
//   +0x40: 0x140078D30 — Slot8 (SelectResourceUI handler, shared with PastureUI)
//   +0x48: 0x140078D80 — Slot9 (draw/message handler)
//   +0x58: 0x140075380 — Slot11 (SelectResourceUI extension)
//   +0x60: 0x140075430 — Slot12 (SelectResourceUI extension)
//
// Unhookable (too small):
//   +0x08: 0x140012FE0 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140012FD0 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14003A95A — 6 bytes, Slot3 (thunk to Runtime)
//   +0x68: 0x140075420 — 6 bytes, Slot13 (mov eax,1; ret)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x50: Slot10

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
static HookInfo g_hookSlot11;
static HookInfo g_hookSlot12;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "OrchardUI::Dtor";
    static void* s_vtable = nullptr;
    static void* s_baseVtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, OrchardUIAddr::Vtable);
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, CropUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    ((DialogController*)self)->vtable = s_baseVtable;  // SelectResourceUI base
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "OrchardUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

// Slot7 — REIMPLEMENTED (updates growth progress bar)
#include "ui/ui_panels.h"
static void Reimpl_Slot7(void* self) {
    g_lastHook = "OrchardUI::Slot7";
    OrchardUI_Slot7((OrchardUI*)self);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "OrchardUI::Slot8";
    ((fn_Slot8_t)g_hookSlot8.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "OrchardUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4, param5, param6);
}

// Slot11 — REIMPLEMENTED (sets crop/resource type on component)
static void Reimpl_Slot11(void* self, int64_t param2) {
    g_lastHook = "OrchardUI::Slot11";
    OrchardUI_Slot11((OrchardUI*)self, param2);
}

// Slot12 — REIMPLEMENTED (gets crop/resource type from component)
static void* Reimpl_Slot12(void* self, void* param2) {
    g_lastHook = "OrchardUI::Slot12";
    return OrchardUI_Slot12((OrchardUI*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallOrchardUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400130E0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[OrchardUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUIDtor() { RemoveHook(&g_hookDtor); }

void InstallOrchardUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400755B0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[OrchardUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallOrchardUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400754B0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 17);
#ifdef _DEBUG
    printf("[OrchardUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallOrchardUISlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078D30);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[OrchardUI] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot8() { RemoveHook(&g_hookSlot8); }

void InstallOrchardUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078D80);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 19);
#ifdef _DEBUG
    printf("[OrchardUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot9() { RemoveHook(&g_hookSlot9); }

void InstallOrchardUISlot11() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075380);
    InstallHook(target, (void*)Reimpl_Slot11, &g_hookSlot11, 15);
#ifdef _DEBUG
    printf("[OrchardUI] Slot11 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot11() { RemoveHook(&g_hookSlot11); }

void InstallOrchardUISlot12() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140075430);
    InstallHook(target, (void*)Reimpl_Slot12, &g_hookSlot12, 17);
#ifdef _DEBUG
    printf("[OrchardUI] Slot12 hooked at %p\n", (void*)target);
#endif
}
void RemoveOrchardUISlot12() { RemoveHook(&g_hookSlot12); }
