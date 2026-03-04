// engine/reimpl/pastureui.cpp
// Reimplementation of PastureUI vtable methods from Application-x64.exe.
//
// PastureUI (0x40 bytes), vtable at 0x14015f610.
// Extends SelectResourceUI (which extends DialogController, 0x30 base).
// 14-slot vtable (11 base + 3 SelectResourceUI extensions).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140020EE0 — Dtor (scalar deleting destructor)
//   +0x20: 0x1400B5090 — Slot4 (shutdown)
//   +0x38: 0x1400B4E50 — Slot7 (update handler)
//   +0x40: 0x140078D30 — Slot8 (SelectResourceUI handler, shared with OrchardUI)
//   +0x48: 0x1400B4E20 — Slot9 (draw/message handler)
//   +0x58: 0x1400B4C40 — Slot11 (SelectResourceUI extension)
//   +0x60: 0x1400B4D00 — Slot12 (SelectResourceUI extension)
//
// Unhookable (too small):
//   +0x08: 0x140020E00 — 8 bytes, Slot1 (Cleanup getter)
//   +0x10: 0x140020DF0 — 7 bytes, Slot2 (RegisterType getter)
//   +0x18: 0x14003A95A — 6 bytes, Slot3 (thunk to Runtime)
//   +0x68: 0x1400B4CF0 — 6 bytes, Slot13 (mov eax,2; ret)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x30: Slot6, +0x50: Slot10

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
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;
static HookInfo g_hookSlot11;
static HookInfo g_hookSlot12;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "PastureUI::Dtor";
    static void* s_vtable = nullptr;
    static void* s_baseVtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, PastureUIAddr::Vtable);
    if (!s_baseVtable) s_baseVtable = (void*)ResolveAddr(Module::Exe, CropUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    ((DialogController*)self)->vtable = s_baseVtable;  // SelectResourceUI base
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (initialize combo + slider elements)
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "PastureUI::Slot4";
    PastureUI_Slot4((PastureUI*)self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "PastureUI::Slot7";
    PastureUI_Slot7((PastureUI*)self);
}

typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "PastureUI::Slot8";
    SelectResourceUI_Slot8(self);
}

// Slot9 — REIMPLEMENTED (combo selection + slider change handler)
typedef void (*fn_Slot9_t)(void* self, void* param2, int param3, int param4, int param5, int param6);
static void Reimpl_Slot9(void* self, void* param2, int param3, int param4, int param5, int param6) {
    g_lastHook = "PastureUI::Slot9";
    PastureUI_Slot9((PastureUI*)self, param2, param3, param4);
}

// Slot11 — REIMPLEMENTED (sets animal/resource type, updates slider)
#include "Dialog/ui_panels.h"
static void Reimpl_Slot11(void* self, int64_t param2) {
    g_lastHook = "PastureUI::Slot11";
    PastureUI_Slot11((PastureUI*)self, param2);
}

// Slot12 — REIMPLEMENTED (gets animal/resource type from component)
static void* Reimpl_Slot12(void* self, void* param2) {
    g_lastHook = "PastureUI::Slot12";
    return PastureUI_Slot12((PastureUI*)self, param2);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallPastureUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140020EE0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[PastureUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUIDtor() { RemoveHook(&g_hookDtor); }

void InstallPastureUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B5090);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 15);
#ifdef _DEBUG
    printf("[PastureUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallPastureUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B4E50);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 17);
#ifdef _DEBUG
    printf("[PastureUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallPastureUISlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140078D30);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 13);
#ifdef _DEBUG
    printf("[PastureUI] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot8() { RemoveHook(&g_hookSlot8); }

void InstallPastureUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B4E20);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 13);
#ifdef _DEBUG
    printf("[PastureUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot9() { RemoveHook(&g_hookSlot9); }

void InstallPastureUISlot11() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B4C40);
    InstallHook(target, (void*)Reimpl_Slot11, &g_hookSlot11, 16);
#ifdef _DEBUG
    printf("[PastureUI] Slot11 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot11() { RemoveHook(&g_hookSlot11); }

void InstallPastureUISlot12() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400B4D00);
    InstallHook(target, (void*)Reimpl_Slot12, &g_hookSlot12, 14);
#ifdef _DEBUG
    printf("[PastureUI] Slot12 hooked at %p\n", (void*)target);
#endif
}
void RemovePastureUISlot12() { RemoveHook(&g_hookSlot12); }
