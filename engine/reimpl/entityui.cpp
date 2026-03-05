// engine/reimpl/entityui.cpp
// Reimplementation of EntityUI vtable methods from Application-x64.exe.
//
// EntityUI (0x48 bytes), vtable at 0x14015bb90.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x1400112D0 — Dtor (scalar deleting destructor)
//   +0x18: 0x1400672A0 — Slot3 (3-byte RET, unhookable)
//   +0x20: 0x1400670D0 — Slot4 (setup/refresh)
//   +0x30: 0x140067090 — Slot6 (handler)
//   +0x38: 0x140066D80 — Slot7 (update handler)
//   +0x40: 0x140066AD0 — Slot8 (additional handler)
//   +0x48: 0x140067050 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140011260 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140011250 — 7 bytes, Slot2 (Cleanup getter)
//   +0x18: 0x1400672A0 — 3 bytes, Slot3 (RET 0)
//
// Stubs (base class no-ops):
//   +0x28: Slot5, +0x50: Slot10

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
static HookInfo g_hookSlot6;
static HookInfo g_hookSlot7;
static HookInfo g_hookSlot8;
static HookInfo g_hookSlot9;

// ============================================================
// Passthrough wrappers
// ============================================================
typedef void* (*fn_Dtor_t)(void* self, uint32_t flags);
static void* Reimpl_Dtor(void* self, uint32_t flags) {
    g_lastHook = "EntityUI::Dtor";
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, EntityUIAddr::Vtable);
    ((DialogController*)self)->vtable = s_vtable;
    fn_DialogController_Dtor(self);
    if (flags & 1) fn_MemFreeStatic(self);
    return self;
}

// Slot4 — REIMPLEMENTED (entity panel setup)
typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "EntityUI::Slot4";
    EntityUI_Slot4((EntityUI*)self);
}

// Slot6 — REIMPLEMENTED (removes entity from tracking set)
static void Reimpl_Slot6(void* self) {
    g_lastHook = "EntityUI::Slot6";
    EntityUI_Slot6((EntityUI*)self);
}

// Slot7 — REIMPLEMENTED (page switching logic)
typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "EntityUI::Slot7";
    EntityUI_Slot7((EntityUI*)self);
}

// Slot8 — REIMPLEMENTED (input handler: mouse position, key 5 entity select)
typedef void (*fn_Slot8_t)(void* self);
static void Reimpl_Slot8(void* self) {
    g_lastHook = "EntityUI::Slot8";
    EntityUI_Slot8((EntityUI*)self);
}

// Slot9 — REIMPLEMENTED (button dispatch: close/center/pin)
typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "EntityUI::Slot9";
    EntityUI_Slot9((EntityUI*)self, (void*)param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallEntityUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400112D0);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[EntityUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUIDtor() { RemoveHook(&g_hookDtor); }

void InstallEntityUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400670D0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 14);
#ifdef _DEBUG
    printf("[EntityUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallEntityUISlot6() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140067090);
    InstallHook(target, (void*)Reimpl_Slot6, &g_hookSlot6, 13);
#ifdef _DEBUG
    printf("[EntityUI] Slot6 hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUISlot6() { RemoveHook(&g_hookSlot6); }

void InstallEntityUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140066D80);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 14);
#ifdef _DEBUG
    printf("[EntityUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallEntityUISlot8() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140066AD0);
    InstallHook(target, (void*)Reimpl_Slot8, &g_hookSlot8, 14);
#ifdef _DEBUG
    printf("[EntityUI] Slot8 hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUISlot8() { RemoveHook(&g_hookSlot8); }

void InstallEntityUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140067050);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 19);
#ifdef _DEBUG
    printf("[EntityUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveEntityUISlot9() { RemoveHook(&g_hookSlot9); }
