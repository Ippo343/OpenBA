// engine/reimpl/tradecomponent.cpp
// Reimplementation of TradeComponent lifecycle methods from Application-x64.exe.
//
// TradeComponent is the largest ControllerComponent variant (0xa8 bytes) with
// trade state, item arrays, and an unusual controller sub-object at +0x88.
// Three non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x14005bd50) — PASSTHROUGH: 174 instructions, complex nested
//     serialization of multiple arrays (storage items, trade goods, offers),
//     string serialization, entity refs, Time, and version migration.
//     Too risky for full reimpl (save corruption potential).
//   - Draw (0x14005c1d0) — add entity to trade pending list + call setup
//   - OnCreate (0x14005c190) — register entity with world trade list
//
// All other vtable slots are no-ops.

#include "TradeComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookDraw;
static HookInfo g_hookOnCreate;

// ============================================================
// Original function typedefs for passthrough
// ============================================================
typedef void (*fn_TradeComponent_Shutdown_t)(TradeComponent* self, void* stream,
                                              uint64_t param3, uint64_t param4);

// ============================================================
// Local helper function pointers
// ============================================================

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t fn_RegisterEntity = nullptr;

// FUN_140003110 — add entity to world pending list
typedef void (*fn_AddToPending_t)(void* listPtr, int newCount, void* entityPtr);
static fn_AddToPending_t fn_AddToPending = nullptr;

// FUN_14005c220 — trade component setup (called after adding to pending list)
typedef void (*fn_TradeSetup_t)(void* self);
static fn_TradeSetup_t fn_TradeSetup = nullptr;

// DAT_1401c3dd8 — game state singleton pointer
static void** g_pGameState = nullptr;

// ============================================================
// Shutdown (0x14005bd50) — PASSTHROUGH
// Extremely complex serialization: Component_Serialize + empty serializer +
// versioned serializer with:
//   - Storage item array (+0x50/+0x58) with per-item InternalBasePtr + 2 ints
//   - Trade ID (+0x2c)
//   - Trade goods array (+0x60) with InternalBasePtr
//   - Offer array (+0x30/+0x38) with String + count + resize + per-item serialize
//   - Third array (+0x40/+0x48) with 12-byte entries
//   - Trade state int (+0x28)
//   - Time at +0x78
//   - Bool at +0x71
//   - Entity ref at +0x80 with IsDerivedFrom validation
//   - Bool at +0x70
// ============================================================
static void Reimpl_Shutdown(TradeComponent* self, void* stream,
                             uint64_t param3, uint64_t param4) {
    g_lastHook = "TradeComponent::Shutdown";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[TradeComponent::Shutdown] self=%p tradeId=%d tradeState=%u (passthrough)\n",
               (void*)self, self->tradeId, self->tradeState);
        callCount++;
    }
#endif

    ((fn_TradeComponent_Shutdown_t)g_hookShutdown.trampoline)(self, stream, param3, param4);
}

// ============================================================
// Draw (0x14005c1d0)
// Adds the owning entity to the world trade pending list, then
// calls TradeSetup to initialize trade state.
//
// Original logic:
//   entityPtr = self->owner (+0x10)
//   worldMgr = gameState->+0xf0
//   AddToPending(worldMgr+0x30, *(worldMgr+0x38)+1, &entityPtr)
//   TradeSetup(self)
// ============================================================
static void Reimpl_Draw(TradeComponent* self) {
    g_lastHook = "TradeComponent::Draw";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[TradeComponent::Draw] self=%p tradeId=%d\n",
               (void*)self, self->tradeId);
        callCount++;
    }
#endif

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameState;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Add to trade pending list at worldMgr+0x30
    int currentCount = *(int*)(worldMgr + 0x38);
    fn_AddToPending((void*)(worldMgr + 0x30), currentCount + 1, &entityPtr);

    // Call trade setup
    fn_TradeSetup(self);
}

// ============================================================
// OnCreate (0x14005c190)
// Registers the owning entity with the world trade entity list.
// Similar to StorageLocationComponent::OnCreate but at offset +0x30.
// ============================================================
static void Reimpl_OnCreate(TradeComponent* self) {
    g_lastHook = "TradeComponent::OnCreate";
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[TradeComponent::OnCreate] self=%p tradeId=%d\n",
               (void*)self, self->tradeId);
        callCount++;
    }
#endif

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameState;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x30 (trade entity list)
    fn_RegisterEntity((void*)(worldMgr + 0x30), &entityPtr);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallTradeComponentShutdown() {
    uintptr_t target = ResolveAddr(Module::Exe, 0x14005bd50);
    // Prologue: PUSH RBP(1)+PUSH RSI(1)+PUSH RDI(1)+MOV RBP,RSP(3)+SUB RSP,0x50(4)+MOV qword,-0x2(8) = 18
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 19);
#ifdef _DEBUG
    printf("[TradeComponent] Shutdown hooked at %p (prologue=18, passthrough)\n", (void*)target);
#endif
}

void RemoveTradeComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallTradeComponentDraw() {
    fn_AddToPending = reinterpret_cast<fn_AddToPending_t>(
        ResolveAddr(Module::Exe, 0x140003110));
    fn_TradeSetup = reinterpret_cast<fn_TradeSetup_t>(
        ResolveAddr(Module::Exe, 0x14005c220));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    if (!fn_AddToPending || !fn_TradeSetup || !g_pGameState) {
#ifdef _DEBUG
        printf("[TradeComponent] ERROR: failed to resolve Draw helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005c1d0);
    // Prologue: PUSH RBX(2)+SUB RSP,0x20(4)+MOV RAX,[RCX+0x10](4)+MOV RBX,RCX(3) = 13
    InstallHook(target, (void*)Reimpl_Draw, &g_hookDraw, 13);
#ifdef _DEBUG
    printf("[TradeComponent] Draw hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveTradeComponentDraw() { RemoveHook(&g_hookDraw); }

void InstallTradeComponentOnCreate() {
    fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    if (!g_pGameState) {
        g_pGameState = reinterpret_cast<void**>(
            ResolveAddr(Module::Exe, 0x1401c3dd8));
    }

    if (!fn_RegisterEntity || !g_pGameState) {
#ifdef _DEBUG
        printf("[TradeComponent] ERROR: failed to resolve OnCreate helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14005c190);
    // Prologue: SUB RSP,0x28(4)+MOV RAX,[RCX+0x10](4)+LEA RDX,[RSP+0x30](5) = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[TradeComponent] OnCreate hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveTradeComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
