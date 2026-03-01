// engine/reimpl/consumeproduceui.cpp
// Reimplementation of ConsumeProduceUI vtable methods from Application-x64.exe.
//
// ConsumeProduceUI (0x40 bytes), vtable at 0x14015b2e0.
// Extends DialogController (0x30 base).
//
// Vtable overrides (exe-range functions):
//   +0x00: 0x140010B90 — Dtor (scalar deleting destructor)
//   +0x20: 0x14005F3D0 — Slot4 (setup/refresh)
//   +0x38: 0x14005F1F0 — Slot7 (update handler)
//   +0x48: 0x14005F330 — Slot9 (notification handler)
//
// Unhookable (too small):
//   +0x08: 0x140010AD0 — 8 bytes, Slot1 (Setup getter)
//   +0x10: 0x140010AC0 — 7 bytes, Slot2 (Cleanup getter)
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
    g_lastHook = "ConsumeProduceUI::Dtor";
    return ((fn_Dtor_t)g_hookDtor.trampoline)(self, flags);
}

typedef void (*fn_Slot4_t)(void* self);
static void Reimpl_Slot4(void* self) {
    g_lastHook = "ConsumeProduceUI::Slot4";
    ((fn_Slot4_t)g_hookSlot4.trampoline)(self);
}

typedef void (*fn_Slot7_t)(void* self);
static void Reimpl_Slot7(void* self) {
    g_lastHook = "ConsumeProduceUI::Slot7";
    ((fn_Slot7_t)g_hookSlot7.trampoline)(self);
}

typedef void (*fn_Slot9_t)(void* self, uint64_t param2, int param3, int param4);
static void Reimpl_Slot9(void* self, uint64_t param2, int param3, int param4) {
    g_lastHook = "ConsumeProduceUI::Slot9";
    ((fn_Slot9_t)g_hookSlot9.trampoline)(self, param2, param3, param4);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallConsumeProduceUIDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140010B90);
    InstallHook(target, (void*)Reimpl_Dtor, &g_hookDtor, 19);
#ifdef _DEBUG
    printf("[ConsumeProduceUI] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceUIDtor() { RemoveHook(&g_hookDtor); }

void InstallConsumeProduceUISlot4() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F3D0);
    InstallHook(target, (void*)Reimpl_Slot4, &g_hookSlot4, 13);
#ifdef _DEBUG
    printf("[ConsumeProduceUI] Slot4 hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceUISlot4() { RemoveHook(&g_hookSlot4); }

void InstallConsumeProduceUISlot7() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F1F0);
    InstallHook(target, (void*)Reimpl_Slot7, &g_hookSlot7, 17);
#ifdef _DEBUG
    printf("[ConsumeProduceUI] Slot7 hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceUISlot7() { RemoveHook(&g_hookSlot7); }

void InstallConsumeProduceUISlot9() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005F330);
    InstallHook(target, (void*)Reimpl_Slot9, &g_hookSlot9, 15);
#ifdef _DEBUG
    printf("[ConsumeProduceUI] Slot9 hooked at %p\n", (void*)target);
#endif
}
void RemoveConsumeProduceUISlot9() { RemoveHook(&g_hookSlot9); }
