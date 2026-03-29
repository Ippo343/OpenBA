// engine/reimpl/bridgetool.cpp
// Reimplementation of BridgeTool vtable methods from Application-x64.exe.
//
// BridgeTool (0x40 bytes), vtable at 0x140161b00.
// Extends DragTool (0x38 bytes).
// Factory at 0x14002d7a0, Ctor at 0x1400fb890.
//
// Vtable layout (15 slots):
//   +0x00: 0x14002d7f0 — Slot0 Dtor (hooked) — REIMPLEMENTED
//   +0x08: 0x14002d790 — Slot1 GetTypeInfo (8 bytes, unhookable)
//   +0x10: 0x14002d780 — Slot2 GetTypeId (7 bytes, unhookable)
//   +0x18: 0x14003a95a — Slot3 JMP thunk (6 bytes, unhookable)
//   +0x20: 0x1400fb7a0 — Slot4 Startup (hooked) — REIMPLEMENTED
//   +0x28: 0x1400fb790 — Slot5 (3 bytes RET, unhookable)
//   +0x30: 0x1400fc6b0 — Slot6 OnKey (shared with DragTool — hooked there)
//   +0x38: 0x1400fb2c0 — Slot7 (3 bytes RET, unhookable)
//   +0x40: 0x1400fadc0 — Slot8 Draw (JNZ rel32 in prologue — unsafe for trampoline)
//   +0x48: 0x1400fc640 — Slot9 Shutdown (JZ rel8 in prologue — unsafe for trampoline)
//   +0x50: 0x1400268a0 — Slot10 (3 bytes, unhookable — returns false)
//   +0x58: 0x140004950 — Slot11 (10 bytes, unhookable)
//   +0x60: 0x1400fc9d0 — Slot12 (6 bytes, unhookable — returns 5)
//   +0x68: 0x1400f9cd0 — Slot13 CalcPath (hooked) — trampoline passthrough
//   +0x70: 0x1400facf0 — Slot14 CalcCost (hooked) — REIMPLEMENTED

#include "BridgeTool.h"
#include "Player/bridgetool_funcs.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Slot0: Dtor — REIMPLEMENTED
// ============================================================
static HookInfo g_hookBridgeToolDtor;
static void* Reimpl_BridgeToolDtor(void* self, uint32_t flags) {
    g_lastHook = "BridgeTool::BridgeToolDtor";
    return BridgeTool_Dtor((BridgeTool*)self, flags);
}

// ============================================================
// Slot4: Startup — REIMPLEMENTED
// ============================================================
static HookInfo g_hookBridgeToolStartup;
static void Reimpl_BridgeToolStartup(void* self) {
    g_lastHook = "BridgeTool::BridgeToolStartup";
    BridgeTool_Startup((BridgeTool*)self);
}

// ============================================================
// Slot13: CalcPath — REIMPLEMENTED (trampoline passthrough)
// ============================================================
// Clean C++ wrapper in code/Game/Player/bridgetool.cpp documents the function's
// full logic: linked list segment allocation, entity copy-ctor, component lookups
// (MapComponent, ModelComponent, MultiObjectComponent, PickingComponent),
// transform operations, picking scene registration, and entity list management.
//
// Full reimpl blocked on ~20 untyped helper functions. Uses RSP-safe naked JMP
// passthrough because InstallHookRSPSafe's trampoline expects to be JMP'd to
// (not CALL'd) for correct MOV RAX,RSP compensation.
static HookInfo g_hookBridgeToolCalcPath;
REIMPL_RSP_PASSTHROUGH(BridgeToolCalcPath, g_hookBridgeToolCalcPath, "BridgeTool::CalcPath")

// ============================================================
// Slot14: CalcCost — REIMPLEMENTED
// ============================================================
static HookInfo g_hookBridgeToolCalcCost;
static uint64_t Reimpl_BridgeToolCalcCost(void* self, int x, int z,
                                           uint32_t* a, int* b, int* c, int* d) {
    g_lastHook = "BridgeTool::BridgeToolCalcCost";
    return BridgeTool_CalcCost((BridgeTool*)self, x, z, a, b, c, d);
}

// ============================================================
// Install/Remove functions
// ============================================================

void InstallBridgeToolDtor() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14002d7f0);
    InstallHook(target, (void*)Reimpl_BridgeToolDtor, &g_hookBridgeToolDtor, 19);
#ifdef _DEBUG
    printf("[BridgeTool] Dtor hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemoveBridgeToolDtor() { RemoveHook(&g_hookBridgeToolDtor); }

void InstallBridgeToolStartup() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fb7a0);
    InstallHook(target, (void*)Reimpl_BridgeToolStartup, &g_hookBridgeToolStartup, 15);
#ifdef _DEBUG
    printf("[BridgeTool] Startup hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemoveBridgeToolStartup() { RemoveHook(&g_hookBridgeToolStartup); }

void InstallBridgeToolCalcPath() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400f9cd0);
    // prologueSize=13: MOV RAX,RSP(3) + MOV [RAX+10],EDX(3) + MOV [RAX+8],RCX(4) = 10
    //   + PUSH RBP(1) + PUSH RSI(1) + PUSH RDI(1) = 13 bytes (3 complete instructions past 10)
    InstallHookRSPSafe(target, (void*)Reimpl_BridgeToolCalcPath, &g_hookBridgeToolCalcPath, 13);
#ifdef _DEBUG
    printf("[BridgeTool] CalcPath hooked (RSP-safe passthrough) at %p\n", (void*)target);
#endif
}
void RemoveBridgeToolCalcPath() { RemoveHook(&g_hookBridgeToolCalcPath); }

void InstallBridgeToolCalcCost() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400facf0);
    InstallHook(target, (void*)Reimpl_BridgeToolCalcCost, &g_hookBridgeToolCalcCost, 12);
#ifdef _DEBUG
    printf("[BridgeTool] CalcCost hooked at %p (REIMPLEMENTED)\n", (void*)target);
#endif
}
void RemoveBridgeToolCalcCost() { RemoveHook(&g_hookBridgeToolCalcCost); }
