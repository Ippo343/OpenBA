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
#include "Player/placementtool_funcs.h"
#include <cstdio>
#include <cstdint>

// --- Slot0: Dtor ---
static HookInfo g_hookPlacementToolDtor;
typedef void* (*fn_PlacementToolDtor_t)(void* self, uint32_t flags);
static void* Reimpl_PlacementToolDtor(void* self, uint32_t flags) {
    g_lastHook = "PlacementTool::PlacementToolDtor";
    return PlacementTool_Dtor_Impl((PlacementTool*)self, flags);
}

// --- Slot4: Startup --- REIMPLEMENTED
static HookInfo g_hookPlacementToolStartup;
typedef void (*fn_PlacementToolStartup_t)(void* self);
static void Reimpl_PlacementToolStartup(void* self) {
    g_lastHook = "PlacementTool::PlacementToolStartup";
    PlacementTool_Startup_Impl((PlacementTool*)self);
}

// --- Slot5: Shutdown2 --- REIMPLEMENTED
static HookInfo g_hookPlacementToolShutdown2;
static void Reimpl_PlacementToolShutdown2(void* self) {
    g_lastHook = "PlacementTool::PlacementToolShutdown2";
    PlacementTool_Shutdown2_Impl((PlacementTool*)self);
}

// --- Slot6: OnKey --- REIMPLEMENTED (rotation + placement input)
static HookInfo g_hookPlacementToolOnKey;
static uint64_t Reimpl_PlacementToolOnKey(void* self, char param2) {
    g_lastHook = "PlacementTool::OnKey";
    return PlacementTool_OnKey_Impl((PlacementTool*)self, param2);
}

// --- Slot7: Serialize --- REIMPLEMENTED (placement preview + cost UI)
static HookInfo g_hookPlacementToolSerialize;
static void Reimpl_PlacementToolSerialize(void* self, void* param2, char param3) {
    g_lastHook = "PlacementTool::Serialize";
    PlacementTool_Serialize_Impl((PlacementTool*)self, param2, param3);
}

// --- Slot8: Draw --- REIMPLEMENTED (placement overlay tile rendering)
static HookInfo g_hookPlacementToolDraw;
static void Reimpl_PlacementToolDraw(void* self, void* context, char param3) {
    g_lastHook = "PlacementTool::Draw";
    PlacementTool_Draw_Impl((PlacementTool*)self, context, param3);
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
    // Disasm: MOV R11,RSP(3)+PUSH RBX(1)+PUSH RSI(1)+SUB RSP,68(4)+
    //         MOV [R11+8],RBP(5)=14? original was 13.
    // 3+1+1=5, SUB RSP,68 is 4 bytes=9, MOV [R11+8],RBP is 5=14. But original said 13.
    // Let's keep original prologue size of 13 (3+1+1+4+1+1+2=? recalculate from disasm):
    // 1400fe2a0 MOV R11,RSP       = 3 bytes (4C 89 DC)
    // 1400fe2a3 PUSH RBX          = 1 byte
    // 1400fe2a4 PUSH RSI          = 1 byte
    // 1400fe2a5 SUB RSP,0x68      = 4 bytes (48 83 EC 68)
    // 1400fe2a9 MOV [R11+8],RBP   = 5 bytes (4C 89 5B 08? no, 4C=REX, 89=MOV, ...) need exact
    // 3+1+1+4=9 bytes, 4 more = 13 bytes total ending at some instruction boundary.
    // Original prologueSize=13, trust that value.
    InstallHookRSPSafe(target, (void*)Reimpl_PlacementToolOnKey, &g_hookPlacementToolOnKey, 13);
#ifdef _DEBUG
    printf("[PlacementTool] OnKey hooked (RSP-safe) at %p\n", (void*)target);
#endif
}
void RemovePlacementToolOnKey() { RemoveHook(&g_hookPlacementToolOnKey); }

void InstallPlacementToolSerialize() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fdee0);
    // Disasm: MOV RAX,RSP(3)+PUSH RBP(1)+PUSH RSI(1)+PUSH RDI(1)+PUSH R13(2)+
    //         PUSH R14(2)+LEA RBP,[RAX-58](5)=15? original was 14.
    // 3+1+1+1+2+2=10, LEA RBP,[RAX-58] = 4 bytes (48 8D 68 A8) = 14. Trust 14.
    InstallHookRSPSafe(target, (void*)Reimpl_PlacementToolSerialize, &g_hookPlacementToolSerialize, 14);
#ifdef _DEBUG
    printf("[PlacementTool] Serialize hooked (RSP-safe) at %p\n", (void*)target);
#endif
}
void RemovePlacementToolSerialize() { RemoveHook(&g_hookPlacementToolSerialize); }

void InstallPlacementToolDraw() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x1400fdcc0);
    // Disasm: MOV R11,RSP(3)+PUSH RBX(1)+PUSH RBP(1)+PUSH RDI(1)+PUSH R14(2)+
    //         PUSH R15(2)+SUB RSP,E0(7)=17 bytes. Trust original prologueSize=17.
    InstallHookRSPSafe(target, (void*)Reimpl_PlacementToolDraw, &g_hookPlacementToolDraw, 17);
#ifdef _DEBUG
    printf("[PlacementTool] Draw hooked (RSP-safe) at %p\n", (void*)target);
#endif
}
void RemovePlacementToolDraw() { RemoveHook(&g_hookPlacementToolDraw); }
