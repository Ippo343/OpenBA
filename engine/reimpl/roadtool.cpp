// engine/reimpl/roadtool.cpp
// Reimplementation of RoadTool vtable methods from Application-x64.exe.
//
// RoadTool (0x48 bytes), vtable at 0x140161e70.
// Extends DragTool (0x38 bytes).
// Factory at 0x14002e870, Ctor at 0x1401057d0.
//
// Vtable layout (15 slots):
//   +0x00: 0x14002e8c0 — Slot0 Dtor (hooked) — REIMPLEMENTED
//   +0x08: 0x14002e860 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002e850 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1401056e0 — Slot4 Startup (hooked) — REIMPLEMENTED
//   +0x28: 0x1401056d0 — Slot5 (3 bytes RET, unhookable)
//   +0x30: 0x140105690 — Slot6 OnKey (CALL rel32 in prologue — unhookable)
//   +0x38: 0x140105680 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x140105190 — Slot8 Draw (hooked) — trampoline (calls base DragTool::Draw)
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unhookable)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x140104d30 — Slot13 CalcPath (CALL rel32 in prologue — unhookable)
//   +0x70: 0x140104fe0 — Slot14 CalcCost (CALL rel32 in prologue — unhookable)

#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "Player/roadtool_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Slot0: Dtor — REIMPLEMENTED
// ============================================================
static HookInfo g_hookRoadToolDtor;
static void* Reimpl_RoadToolDtor(void* self, uint32_t flags) {
    g_lastHook = "RoadTool::RoadToolDtor";
    return RoadTool_Dtor((RoadTool*)self, flags);
}

// ============================================================
// Slot4: Startup — REIMPLEMENTED
// ============================================================
static HookInfo g_hookRoadToolStartup;
static void Reimpl_RoadToolStartup(void* self) {
    g_lastHook = "RoadTool::RoadToolStartup";
    RoadTool_Startup((RoadTool*)self);
}

// ============================================================
// Slot8: Draw — trampoline passthrough
// ============================================================
// Draw calls the shared DragTool::Draw (complex render logic with dynamic
// arrays) then reads KeyMap modifier flag. Kept as trampoline until the
// base DragTool::Draw is fully reimplemented.
static HookInfo g_hookRoadToolDraw;
typedef void (*fn_RoadToolDraw_t)(void* self, void* context, char param);
static void Reimpl_RoadToolDraw(void* self, void* context, char param) {
    g_lastHook = "RoadTool::RoadToolDraw";
    ((fn_RoadToolDraw_t)g_hookRoadToolDraw.trampoline)(self, context, param);
}

// ============================================================
// Install/Remove functions
// ============================================================

void InstallRoadToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002e8c0);
    InstallHook(target, (void*)Reimpl_RoadToolDtor, &g_hookRoadToolDtor, 19);
#ifdef _DEBUG
    printf("[RoadTool] Dtor hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemoveRoadToolDtor() { RemoveHook(&g_hookRoadToolDtor); }

void InstallRoadToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1401056e0);
    InstallHook(target, (void*)Reimpl_RoadToolStartup, &g_hookRoadToolStartup, 15);
#ifdef _DEBUG
    printf("[RoadTool] Startup hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemoveRoadToolStartup() { RemoveHook(&g_hookRoadToolStartup); }

void InstallRoadToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x140105190);
    InstallHook(target, (void*)Reimpl_RoadToolDraw, &g_hookRoadToolDraw, 15);
#ifdef _DEBUG
    printf("[RoadTool] Draw hooked at %p (trampoline)\n", (void*)target);
#endif
}
void RemoveRoadToolDraw() { RemoveHook(&g_hookRoadToolDraw); }
