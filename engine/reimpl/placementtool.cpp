// engine/reimpl/placementtool.cpp
// Reimplementation of PlacementTool vtable methods from Application-x64.exe.
//
// PlacementTool (0x70 bytes), vtable at 0x140161d20.
// Extends Tool (0x28 bytes).
// Factory at 0x14002dc40, Ctor at 0x1400fe850.
//
// Vtable layout (13 slots):
//   +0x00: 0x14002dc90 — Slot0 Dtor (hooked)
//   +0x08: 0x14002dc30 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002dc20 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400fe580 — Slot4 Startup (hooked)
//   +0x28: 0x1400fe4e0 — Slot5 Shutdown2 (hooked)
//   +0x30: 0x1400fe2a0 — Slot6 OnKey (hooked)
//   +0x38: 0x1400fdee0 — Slot7 Serialize (hooked)
//   +0x40: 0x1400fdcc0 — Slot8 Draw (hooked)
//   +0x48: 0x1400fe4d0 — Slot9 (3 bytes, unhookable — returns false)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140026920 — Slot11 (12 bytes, RIP-relative MOV — unsafe for trampoline)
//   +0x60: 0x1400fe800 — Slot12 (6 bytes, unhookable — returns 5)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookPlacementToolDtor;
typedef void* (*fn_PlacementToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_PlacementToolDtor(void* self, uint32_t flags) {
    g_lastHook = "PlacementTool::PlacementToolDtor";
    return ((fn_PlacementToolDtor_t)g_hookPlacementToolDtor.trampoline)(self, flags);
}

// --- Slot4: Startup ---
static HookInfo g_hookPlacementToolStartup;
typedef void (*fn_PlacementToolStartup_t)(void* self);
static void Reimpl_PlacementToolStartup(void* self) {
    g_lastHook = "PlacementTool::PlacementToolStartup";
    ((fn_PlacementToolStartup_t)g_hookPlacementToolStartup.trampoline)(self);
}

// --- Slot5: Shutdown2 ---
static HookInfo g_hookPlacementToolShutdown2;
typedef void (*fn_PlacementToolShutdown2_t)(void* self);
static void Reimpl_PlacementToolShutdown2(void* self) {
    g_lastHook = "PlacementTool::PlacementToolShutdown2";
    ((fn_PlacementToolShutdown2_t)g_hookPlacementToolShutdown2.trampoline)(self);
}

// --- Slot6: OnKey ---
static HookInfo g_hookPlacementToolOnKey;
typedef uint64_t (*fn_PlacementToolOnKey_t)(void* self, char key);
static uint64_t Reimpl_PlacementToolOnKey(void* self, char key) {
    g_lastHook = "PlacementTool::PlacementToolOnKey";
    return ((fn_PlacementToolOnKey_t)g_hookPlacementToolOnKey.trampoline)(self, key);
}

// --- Slot7: Serialize ---
static HookInfo g_hookPlacementToolSerialize;
typedef void (*fn_PlacementToolSerialize_t)(void* self);
static void Reimpl_PlacementToolSerialize(void* self) {
    g_lastHook = "PlacementTool::PlacementToolSerialize";
    ((fn_PlacementToolSerialize_t)g_hookPlacementToolSerialize.trampoline)(self);
}

// --- Slot8: Draw ---
static HookInfo g_hookPlacementToolDraw;
typedef void (*fn_PlacementToolDraw_t)(void* self);
static void Reimpl_PlacementToolDraw(void* self) {
    g_lastHook = "PlacementTool::PlacementToolDraw";
    ((fn_PlacementToolDraw_t)g_hookPlacementToolDraw.trampoline)(self);
}

void InstallPlacementToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002dc90);
    InstallHook(target, (void*)Reimpl_PlacementToolDtor, &g_hookPlacementToolDtor, 19);
#ifdef _DEBUG
    printf("[PlacementTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolDtor() { RemoveHook(&g_hookPlacementToolDtor); }

void InstallPlacementToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fe580);
    InstallHook(target, (void*)Reimpl_PlacementToolStartup, &g_hookPlacementToolStartup, 18);
#ifdef _DEBUG
    printf("[PlacementTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolStartup() { RemoveHook(&g_hookPlacementToolStartup); }

void InstallPlacementToolShutdown2() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fe4e0);
    InstallHook(target, (void*)Reimpl_PlacementToolShutdown2, &g_hookPlacementToolShutdown2, 12);
#ifdef _DEBUG
    printf("[PlacementTool] Shutdown2 hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolShutdown2() { RemoveHook(&g_hookPlacementToolShutdown2); }

void InstallPlacementToolOnKey() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fe2a0);
    InstallHook(target, (void*)Reimpl_PlacementToolOnKey, &g_hookPlacementToolOnKey, 13);
#ifdef _DEBUG
    printf("[PlacementTool] OnKey hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolOnKey() { RemoveHook(&g_hookPlacementToolOnKey); }

void InstallPlacementToolSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fdee0);
    InstallHook(target, (void*)Reimpl_PlacementToolSerialize, &g_hookPlacementToolSerialize, 14);
#ifdef _DEBUG
    printf("[PlacementTool] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolSerialize() { RemoveHook(&g_hookPlacementToolSerialize); }

void InstallPlacementToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fdcc0);
    InstallHook(target, (void*)Reimpl_PlacementToolDraw, &g_hookPlacementToolDraw, 17);
#ifdef _DEBUG
    printf("[PlacementTool] Draw hooked at %p\n", (void*)target);
#endif
}
void RemovePlacementToolDraw() { RemoveHook(&g_hookPlacementToolDraw); }
