// engine/reimpl/removaltool.cpp
// Reimplementation of RemovalTool vtable methods from Application-x64.exe.
//
// RemovalTool (0x40 bytes), vtable at 0x140161de0.
// Extends DragTool (0x38 bytes).
// Factory at 0x14002e7a0, Ctor at 0x140104cc0.
//
// Vtable layout (15 slots):
//   +0x00: 0x14002e7f0 — Slot0 Dtor (hooked)
//   +0x08: 0x14002e790 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002e780 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x140104bd0 — Slot4 Startup (hooked)
//   +0x28: 0x140104bc0 — Slot5 (3 bytes RET, unhookable)
//   +0x30: 0x1400fc6b0 — Slot6 OnKey (shared with DragTool — hooked there)
//   +0x38: 0x140104bb0 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x140104050 — Slot8 Draw (hooked)
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unsafe for trampoline)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x1401035e0 — Slot13 CalcPath (hooked)
//   +0x70: 0x140103ef0 — Slot14 CalcCost (hooked)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookRemovalToolDtor;
typedef void* (*fn_RemovalToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_RemovalToolDtor(void* self, uint32_t flags) {
    return ((fn_RemovalToolDtor_t)g_hookRemovalToolDtor.trampoline)(self, flags);
}

// --- Slot4: Startup ---
static HookInfo g_hookRemovalToolStartup;
typedef void (*fn_RemovalToolStartup_t)(void* self);
static void Reimpl_RemovalToolStartup(void* self) {
    ((fn_RemovalToolStartup_t)g_hookRemovalToolStartup.trampoline)(self);
}

// --- Slot8: Draw ---
static HookInfo g_hookRemovalToolDraw;
typedef void (*fn_RemovalToolDraw_t)(void* self, void* context, char param);
static void Reimpl_RemovalToolDraw(void* self, void* context, char param) {
    ((fn_RemovalToolDraw_t)g_hookRemovalToolDraw.trampoline)(self, context, param);
}

// --- Slot13: CalcPath ---
static HookInfo g_hookRemovalToolCalcPath;
typedef uint64_t (*fn_RemovalToolCalcPath_t)(void* self, int x, int z);
static uint64_t Reimpl_RemovalToolCalcPath(void* self, int x, int z) {
    return ((fn_RemovalToolCalcPath_t)g_hookRemovalToolCalcPath.trampoline)(self, x, z);
}

// --- Slot14: CalcCost ---
static HookInfo g_hookRemovalToolCalcCost;
typedef uint64_t (*fn_RemovalToolCalcCost_t)(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d);
static uint64_t Reimpl_RemovalToolCalcCost(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d) {
    return ((fn_RemovalToolCalcCost_t)g_hookRemovalToolCalcCost.trampoline)(self, x, z, a, b, c, d);
}

void InstallRemovalToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002e7f0);
    InstallHook(target, (void*)Reimpl_RemovalToolDtor, &g_hookRemovalToolDtor, 19);
#ifdef _DEBUG
    printf("[RemovalTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveRemovalToolDtor() { RemoveHook(&g_hookRemovalToolDtor); }

void InstallRemovalToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140104bd0);
    InstallHook(target, (void*)Reimpl_RemovalToolStartup, &g_hookRemovalToolStartup, 15);
#ifdef _DEBUG
    printf("[RemovalTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveRemovalToolStartup() { RemoveHook(&g_hookRemovalToolStartup); }

void InstallRemovalToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140104050);
    InstallHook(target, (void*)Reimpl_RemovalToolDraw, &g_hookRemovalToolDraw, 15);
#ifdef _DEBUG
    printf("[RemovalTool] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveRemovalToolDraw() { RemoveHook(&g_hookRemovalToolDraw); }

void InstallRemovalToolCalcPath() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401035e0);
    InstallHook(target, (void*)Reimpl_RemovalToolCalcPath, &g_hookRemovalToolCalcPath, 13);
#ifdef _DEBUG
    printf("[RemovalTool] CalcPath hooked at %p\n", (void*)target);
#endif
}
void RemoveRemovalToolCalcPath() { RemoveHook(&g_hookRemovalToolCalcPath); }

void InstallRemovalToolCalcCost() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140103ef0);
    InstallHook(target, (void*)Reimpl_RemovalToolCalcCost, &g_hookRemovalToolCalcCost, 15);
#ifdef _DEBUG
    printf("[RemovalTool] CalcCost hooked at %p\n", (void*)target);
#endif
}
void RemoveRemovalToolCalcCost() { RemoveHook(&g_hookRemovalToolCalcCost); }
