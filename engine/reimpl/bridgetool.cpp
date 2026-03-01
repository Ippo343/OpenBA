// engine/reimpl/bridgetool.cpp
// Reimplementation of BridgeTool vtable methods from Application-x64.exe.
//
// BridgeTool (0x40 bytes), vtable at 0x140161b00.
// Extends DragTool (0x38 bytes).
// Factory at 0x14002d7a0, Ctor at 0x1400fb890.
//
// Vtable layout (15 slots):
//   +0x00: 0x14002d7f0 — Slot0 Dtor (hooked)
//   +0x08: 0x14002d790 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002d780 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400fb7a0 — Slot4 Startup (hooked)
//   +0x28: 0x1400fb790 — Slot5 (3 bytes RET, unhookable)
//   +0x30: 0x1400fc6b0 — Slot6 OnKey (shared with DragTool — hooked there)
//   +0x38: 0x1400fb2c0 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x1400fadc0 — Slot8 Draw (JNZ rel32 in prologue — unsafe for trampoline)
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unsafe for trampoline)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x1400f9cd0 — Slot13 CalcPath (hooked)
//   +0x70: 0x1400facf0 — Slot14 CalcCost (hooked)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookBridgeToolDtor;
typedef void* (*fn_BridgeToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_BridgeToolDtor(void* self, uint32_t flags) {
    return ((fn_BridgeToolDtor_t)g_hookBridgeToolDtor.trampoline)(self, flags);
}

// --- Slot4: Startup ---
static HookInfo g_hookBridgeToolStartup;
typedef void (*fn_BridgeToolStartup_t)(void* self);
static void Reimpl_BridgeToolStartup(void* self) {
    ((fn_BridgeToolStartup_t)g_hookBridgeToolStartup.trampoline)(self);
}

// --- Slot13: CalcPath ---
static HookInfo g_hookBridgeToolCalcPath;
typedef uint64_t (*fn_BridgeToolCalcPath_t)(void* self, int x, int z);
static uint64_t Reimpl_BridgeToolCalcPath(void* self, int x, int z) {
    return ((fn_BridgeToolCalcPath_t)g_hookBridgeToolCalcPath.trampoline)(self, x, z);
}

// --- Slot14: CalcCost ---
static HookInfo g_hookBridgeToolCalcCost;
typedef uint64_t (*fn_BridgeToolCalcCost_t)(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d);
static uint64_t Reimpl_BridgeToolCalcCost(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d) {
    return ((fn_BridgeToolCalcCost_t)g_hookBridgeToolCalcCost.trampoline)(self, x, z, a, b, c, d);
}

void InstallBridgeToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002d7f0);
    InstallHook(target, (void*)Reimpl_BridgeToolDtor, &g_hookBridgeToolDtor, 19);
#ifdef _DEBUG
    printf("[BridgeTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveBridgeToolDtor() { RemoveHook(&g_hookBridgeToolDtor); }

void InstallBridgeToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fb7a0);
    InstallHook(target, (void*)Reimpl_BridgeToolStartup, &g_hookBridgeToolStartup, 15);
#ifdef _DEBUG
    printf("[BridgeTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveBridgeToolStartup() { RemoveHook(&g_hookBridgeToolStartup); }

void InstallBridgeToolCalcPath() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f9cd0);
    InstallHook(target, (void*)Reimpl_BridgeToolCalcPath, &g_hookBridgeToolCalcPath, 13);
#ifdef _DEBUG
    printf("[BridgeTool] CalcPath hooked at %p\n", (void*)target);
#endif
}
void RemoveBridgeToolCalcPath() { RemoveHook(&g_hookBridgeToolCalcPath); }

void InstallBridgeToolCalcCost() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400facf0);
    InstallHook(target, (void*)Reimpl_BridgeToolCalcCost, &g_hookBridgeToolCalcCost, 12);
#ifdef _DEBUG
    printf("[BridgeTool] CalcCost hooked at %p\n", (void*)target);
#endif
}
void RemoveBridgeToolCalcCost() { RemoveHook(&g_hookBridgeToolCalcCost); }
