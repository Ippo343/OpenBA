// engine/reimpl/pathtool.cpp
// Reimplementation of PathTool vtable methods from Application-x64.exe.
//
// PathTool (0x60 bytes), vtable at 0x140160680.
// Extends Tool (0x28 bytes).
// Factory at 0x140026b10, Ctor at 0x1400de9e0.
//
// Vtable layout (13 slots):
//   +0x00: 0x140026b70 — Slot0 Dtor (hooked) — REIMPLEMENTED
//   +0x08: 0x140026b00 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x140026af0 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400de940 — Slot4 (3 bytes RET, unhookable)
//   +0x28: 0x1400de930 — Slot5 (5 bytes JMP rel32, unhookable)
//   +0x30: 0x1400de6b0 — Slot6 OnKey (hooked) — REIMPLEMENTED
//   +0x38: 0x1400dd700 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x1400dd3b0 — Slot8 Draw (hooked) — REIMPLEMENTED
//   +0x48: 0x1400de920 — Slot9 (3 bytes, unhookable — returns false)
//   +0x50: 0x1400de8d0 — Slot10 Serialize (hooked) — REIMPLEMENTED
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400de910 — Slot12 (6 bytes, unhookable — returns 4)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Player/pathtool_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor — REIMPLEMENTED ---
static HookInfo g_hookPathToolDtor;
static void* Reimpl_PathToolDtor(void* self, uint32_t flags) {
    g_lastHook = "PathTool::PathToolDtor";
    return PathTool_Dtor((PathTool*)self, flags);
}

// --- Slot6: OnKey — REIMPLEMENTED ---
static HookInfo g_hookPathToolOnKey;
static uint64_t Reimpl_PathToolOnKey(void* self, char key) {
    g_lastHook = "PathTool::PathToolOnKey";
    return PathTool_OnKey((PathTool*)self, key);
}

// --- Slot8: Draw --- REIMPLEMENTED
static HookInfo g_hookPathToolDraw;
static void Reimpl_PathToolDraw(void* self, void* context) {
    g_lastHook = "PathTool::PathToolDraw";
    PathTool_Draw((PathTool*)self, context);
}

// --- Slot10: Serialize — REIMPLEMENTED ---
static HookInfo g_hookPathToolSerialize;
static bool Reimpl_PathToolSerialize(void* self, uint64_t* param2) {
    g_lastHook = "PathTool::PathToolSerialize";
    return PathTool_Serialize((PathTool*)self, param2);
}

void InstallPathToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140026b70);
    InstallHook(target, (void*)Reimpl_PathToolDtor, &g_hookPathToolDtor, 12);
#ifdef _DEBUG
    printf("[PathTool] Dtor hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePathToolDtor() { RemoveHook(&g_hookPathToolDtor); }

void InstallPathToolOnKey() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400de6b0);
    InstallHook(target, (void*)Reimpl_PathToolOnKey, &g_hookPathToolOnKey, 15);
#ifdef _DEBUG
    printf("[PathTool] OnKey hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePathToolOnKey() { RemoveHook(&g_hookPathToolOnKey); }

void InstallPathToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400dd3b0);
    InstallHook(target, (void*)Reimpl_PathToolDraw, &g_hookPathToolDraw, 12);
#ifdef _DEBUG
    printf("[PathTool] Draw hooked at %p (trampoline)\n", (void*)target);
#endif
}
void RemovePathToolDraw() { RemoveHook(&g_hookPathToolDraw); }

void InstallPathToolSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400de8d0);
    InstallHook(target, (void*)Reimpl_PathToolSerialize, &g_hookPathToolSerialize, 14);
#ifdef _DEBUG
    printf("[PathTool] Serialize hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemovePathToolSerialize() { RemoveHook(&g_hookPathToolSerialize); }
