// engine/reimpl/edittool.cpp
// Reimplementation of EditTool vtable methods from Application-x64.exe.
//
// EditTool (0x38 bytes), vtable at 0x140161ca0.
// Extends Tool (0x28 bytes).
// Factory at 0x14002daa0, Ctor at 0x1400fd340.
//
// Vtable layout (13 slots):
//   +0x00: 0x14002db00 — Slot0 Dtor (hooked)
//   +0x08: 0x14002da90 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002da80 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400fd2f0 — Slot4 (3 bytes RET, unhookable)
//   +0x28: 0x1400fd2e0 — Slot5 (5 bytes JMP rel32, unhookable)
//   +0x30: 0x1400fd030 — Slot6 OnKey (hooked)
//   +0x38: 0x1400fcda0 — Slot7 Startup (hooked)
//   +0x40: 0x1400fcd90 — Slot8 (3 bytes RET, unhookable)
//   +0x48: 0x1400fd2d0 — Slot9 (3 bytes, unhookable — returns false)
//   +0x50: 0x1400fd290 — Slot10 Serialize (hooked)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x140105860 — Slot12 (6 bytes, unhookable — returns 1)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Player/edittool_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookEditToolDtor;
typedef void* (*fn_EditToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_EditToolDtor(void* self, uint32_t flags) {
    g_lastHook = "EditTool::EditToolDtor";
    return EditTool_Dtor_Impl((EditTool*)self, flags);
}

// --- Slot6: OnKey ---
static HookInfo g_hookEditToolOnKey;
static uint64_t Reimpl_EditToolOnKey(void* self, char key) {
    g_lastHook = "EditTool::EditToolOnKey";
    return EditTool_OnKey_Impl((EditTool*)self, key);
}

// --- Slot7: Startup ---
static HookInfo g_hookEditToolStartup;
static void Reimpl_EditToolStartup(void* self) {
    g_lastHook = "EditTool::EditToolStartup";
    EditTool_Startup_Impl((EditTool*)self);
}

// --- Slot10: Serialize ---
static HookInfo g_hookEditToolSerialize;
typedef bool (*fn_EditToolSerialize_t)(void* self, uint64_t* param_2);
static bool Reimpl_EditToolSerialize(void* self, uint64_t* param_2) {
    g_lastHook = "EditTool::EditToolSerialize";
    return EditTool_Serialize_Impl((EditTool*)self, param_2);
}

void InstallEditToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002db00);
    InstallHook(target, (void*)Reimpl_EditToolDtor, &g_hookEditToolDtor, 19);
#ifdef _DEBUG
    printf("[EditTool] Dtor hooked at %p\n", (void*)target);
#endif
}
void RemoveEditToolDtor() { RemoveHook(&g_hookEditToolDtor); }

void InstallEditToolOnKey() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fd030);
    InstallHook(target, (void*)Reimpl_EditToolOnKey, &g_hookEditToolOnKey, 15);
#ifdef _DEBUG
    printf("[EditTool] OnKey hooked at %p\n", (void*)target);
#endif
}
void RemoveEditToolOnKey() { RemoveHook(&g_hookEditToolOnKey); }

void InstallEditToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fcda0);
    InstallHook(target, (void*)Reimpl_EditToolStartup, &g_hookEditToolStartup, 12);
#ifdef _DEBUG
    printf("[EditTool] Startup hooked at %p\n", (void*)target);
#endif
}
void RemoveEditToolStartup() { RemoveHook(&g_hookEditToolStartup); }

void InstallEditToolSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fd290);
    InstallHook(target, (void*)Reimpl_EditToolSerialize, &g_hookEditToolSerialize, 14);
#ifdef _DEBUG
    printf("[EditTool] Serialize hooked at %p\n", (void*)target);
#endif
}
void RemoveEditToolSerialize() { RemoveHook(&g_hookEditToolSerialize); }
