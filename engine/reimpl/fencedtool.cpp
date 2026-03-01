// engine/reimpl/fencedtool.cpp
// Reimplementation of FencedTool vtable methods from Application-x64.exe.
//
// FencedTool (0x70 bytes), vtable at 0x1401605d0.
// Extends DragTool (0x38 bytes).
// Factory at 0x1400268d0, Ctor at 0x1400dd070.
//
// Vtable layout (15 slots):
//   +0x00: 0x140026930 — Slot0 Dtor (hooked)
//   +0x08: 0x1400268c0 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x1400268b0 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400dcd50 — Slot4 Startup (hooked)
//   +0x28: 0x1400dccc0 — Slot5 Shutdown2 (hooked)
//   +0x30: 0x1400dcbe0 — Slot6 OnKey (hooked)
//   +0x38: 0x1400dcad0 — Slot7 Serialize (hooked)
//   +0x40: 0x1400dc440 — Slot8 Draw (hooked)
//   +0x48: 0x1400dcd10 — Slot9 Shutdown (hooked)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140026920 — Slot11 (12 bytes, RIP-relative MOV — unsafe for trampoline)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x1400dae00 — Slot13 CalcPath (hooked)
//   +0x70: 0x1400db490 — Slot14 CalcCost (hooked)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookFencedToolDtor;
typedef void* (*fn_FencedToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_FencedToolDtor(void* self, uint32_t flags) {
    g_lastHook = "FencedTool::FencedToolDtor";
    return ((fn_FencedToolDtor_t)g_hookFencedToolDtor.trampoline)(self, flags);
}

// --- Slot4: Startup ---
static HookInfo g_hookFencedToolStartup;
typedef void (*fn_FencedToolStartup_t)(void* self);
static void Reimpl_FencedToolStartup(void* self) {
    g_lastHook = "FencedTool::FencedToolStartup";
    ((fn_FencedToolStartup_t)g_hookFencedToolStartup.trampoline)(self);
}

// --- Slot5: Shutdown2 ---
static HookInfo g_hookFencedToolShutdown2;
typedef void (*fn_FencedToolShutdown2_t)(void* self);
static void Reimpl_FencedToolShutdown2(void* self) {
    g_lastHook = "FencedTool::FencedToolShutdown2";
    ((fn_FencedToolShutdown2_t)g_hookFencedToolShutdown2.trampoline)(self);
}

// --- Slot6: OnKey ---
static HookInfo g_hookFencedToolOnKey;
typedef void (*fn_FencedToolOnKey_t)(void* self);
static void Reimpl_FencedToolOnKey(void* self) {
    g_lastHook = "FencedTool::FencedToolOnKey";
    ((fn_FencedToolOnKey_t)g_hookFencedToolOnKey.trampoline)(self);
}

// --- Slot7: Serialize ---
static HookInfo g_hookFencedToolSerialize;
typedef void (*fn_FencedToolSerialize_t)(void* self);
static void Reimpl_FencedToolSerialize(void* self) {
    g_lastHook = "FencedTool::FencedToolSerialize";
    ((fn_FencedToolSerialize_t)g_hookFencedToolSerialize.trampoline)(self);
}

// --- Slot8: Draw ---
static HookInfo g_hookFencedToolDraw;
typedef void (*fn_FencedToolDraw_t)(void* self, void* context, char param);
static void Reimpl_FencedToolDraw(void* self, void* context, char param) {
    g_lastHook = "FencedTool::FencedToolDraw";
    ((fn_FencedToolDraw_t)g_hookFencedToolDraw.trampoline)(self, context, param);
}

// --- Slot9: Shutdown ---
static HookInfo g_hookFencedToolShutdown;
typedef void (*fn_FencedToolShutdown_t)(void* self);
static void Reimpl_FencedToolShutdown(void* self) {
    g_lastHook = "FencedTool::FencedToolShutdown";
    ((fn_FencedToolShutdown_t)g_hookFencedToolShutdown.trampoline)(self);
}

// --- Slot13: CalcPath ---
static HookInfo g_hookFencedToolCalcPath;
typedef uint64_t (*fn_FencedToolCalcPath_t)(void* self, int x, int z);
static uint64_t Reimpl_FencedToolCalcPath(void* self, int x, int z) {
    g_lastHook = "FencedTool::FencedToolCalcPath";
    return ((fn_FencedToolCalcPath_t)g_hookFencedToolCalcPath.trampoline)(self, x, z);
}

// --- Slot14: CalcCost ---
static HookInfo g_hookFencedToolCalcCost;
typedef uint64_t (*fn_FencedToolCalcCost_t)(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d);
static uint64_t Reimpl_FencedToolCalcCost(void* self, int x, int z, uint32_t* a, int* b, int* c, int* d) {
    g_lastHook = "FencedTool::FencedToolCalcCost";
    return ((fn_FencedToolCalcCost_t)g_hookFencedToolCalcCost.trampoline)(self, x, z, a, b, c, d);
}

void InstallFencedToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026930);
    InstallHook(target, (void*)Reimpl_FencedToolDtor, &g_hookFencedToolDtor, 19);
#ifdef _DEBUG
    printf("[FencedTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolDtor() { RemoveHook(&g_hookFencedToolDtor); }

void InstallFencedToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dcd50);
    InstallHook(target, (void*)Reimpl_FencedToolStartup, &g_hookFencedToolStartup, 18);
#ifdef _DEBUG
    printf("[FencedTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolStartup() { RemoveHook(&g_hookFencedToolStartup); }

void InstallFencedToolShutdown2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dccc0);
    InstallHook(target, (void*)Reimpl_FencedToolShutdown2, &g_hookFencedToolShutdown2, 12);
#ifdef _DEBUG
    printf("[FencedTool] Shutdown2 hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolShutdown2() { RemoveHook(&g_hookFencedToolShutdown2); }

void InstallFencedToolOnKey() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dcbe0);
    InstallHook(target, (void*)Reimpl_FencedToolOnKey, &g_hookFencedToolOnKey, 13);
#ifdef _DEBUG
    printf("[FencedTool] OnKey hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolOnKey() { RemoveHook(&g_hookFencedToolOnKey); }

void InstallFencedToolSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dcad0);
    InstallHook(target, (void*)Reimpl_FencedToolSerialize, &g_hookFencedToolSerialize, 19);
#ifdef _DEBUG
    printf("[FencedTool] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolSerialize() { RemoveHook(&g_hookFencedToolSerialize); }

void InstallFencedToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dc440);
    InstallHook(target, (void*)Reimpl_FencedToolDraw, &g_hookFencedToolDraw, 15);
#ifdef _DEBUG
    printf("[FencedTool] Draw hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolDraw() { RemoveHook(&g_hookFencedToolDraw); }

void InstallFencedToolShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dcd10);
    InstallHook(target, (void*)Reimpl_FencedToolShutdown, &g_hookFencedToolShutdown, 12);
#ifdef _DEBUG
    printf("[FencedTool] Shutdown hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolShutdown() { RemoveHook(&g_hookFencedToolShutdown); }

void InstallFencedToolCalcPath() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dae00);
    InstallHook(target, (void*)Reimpl_FencedToolCalcPath, &g_hookFencedToolCalcPath, 12);
#ifdef _DEBUG
    printf("[FencedTool] CalcPath hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolCalcPath() { RemoveHook(&g_hookFencedToolCalcPath); }

void InstallFencedToolCalcCost() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400db490);
    InstallHook(target, (void*)Reimpl_FencedToolCalcCost, &g_hookFencedToolCalcCost, 13);
#ifdef _DEBUG
    printf("[FencedTool] CalcCost hooked at %p\n", (void*)target);
#endif
}
void RemoveFencedToolCalcCost() { RemoveHook(&g_hookFencedToolCalcCost); }
