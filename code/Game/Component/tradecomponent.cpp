// code/Game/Component/tradecomponent.cpp
// Clean reimplementation of TradeComponent lifecycle methods from Application-x64.exe.
//
// TradeComponent is the largest ControllerComponent variant (0xa8 bytes) with
// trade state, item arrays, and a controller sub-object at +0x88.
// Three non-trivial vtable overrides:
//   Shutdown (0x14005bd50) — PASSTHROUGH (too complex for safe reimpl)
//   Draw     (0x14005c1d0) — add entity to trade pending list + call setup
//   OnCreate (0x14005c190) — register entity with world trade list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/tradecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

// FUN_140003110 — add entity to world pending list
typedef void (*fn_AddToPending_t)(void* listPtr, int newCount, void* entityPtr);
static fn_AddToPending_t s_fn_AddToPending = nullptr;

// FUN_14005c220 — trade component setup (called after adding to pending list)
typedef void (*fn_TradeSetup_t)(void* self);
static fn_TradeSetup_t s_fn_TradeSetup = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_fn_AddToPending = reinterpret_cast<fn_AddToPending_t>(
        ResolveAddr(Module::Exe, 0x140003110));

    s_fn_TradeSetup = reinterpret_cast<fn_TradeSetup_t>(
        ResolveAddr(Module::Exe, 0x14005c220));

    s_initialized = true;
}

// ============================================================
// TradeComponent::Draw (vtable slot [6], 0x14005c1d0)
// ============================================================
// Adds the owning entity to the world trade pending list, then
// calls TradeSetup to initialize trade state.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   MOV RBX,RCX                 ; save self
//   LEA RDX,[RSP+0x20]          ; stack InternalBasePtr
//   MOV [RSP+0x20],RAX          ; store entity ptr
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; world manager
//   MOV EDX,[RCX+0x38]          ; current count
//   ADD EDX,1                   ; newCount = count + 1
//   ADD RCX,0x30                ; trade pending list
//   LEA R8,[RSP+0x20]           ; entity ptr
//   CALL FUN_140003110           ; add to pending list
//   MOV RCX,RBX                 ; self
//   CALL FUN_14005c220           ; trade setup

void TradeComponent_Draw(TradeComponent* self) {
    EnsureHelpers();

    if (!s_fn_AddToPending || !s_fn_TradeSetup ||
        !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Add to trade pending list at worldMgr+0x30
    int currentCount = *(int*)(worldMgr + 0x38);
    s_fn_AddToPending((void*)(worldMgr + 0x30), currentCount + 1, &entityPtr);

    // Call trade setup
    s_fn_TradeSetup(self);
}

// ============================================================
// TradeComponent::OnCreate (vtable slot [7], 0x14005c190)
// ============================================================
// Registers the owning entity with the world trade entity list.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   LEA RDX,[RSP+0x08]          ; stack InternalBasePtr
//   MOV [RSP+0x08],RAX          ; store entity ptr
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; world manager
//   ADD RCX,0x30                ; trade entity list
//   CALL FUN_140002cc0           ; register entity

void TradeComponent_OnCreate(TradeComponent* self) {
    EnsureHelpers();

    if (!s_fn_RegisterEntity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr+0x30 (trade entity list)
    s_fn_RegisterEntity((void*)(worldMgr + 0x30), &entityPtr);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void TradeComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[TradeComponent] Funcs resolved: RegisterEntity=%p AddToPending=%p "
           "TradeSetup=%p\n",
           (void*)s_fn_RegisterEntity, (void*)s_fn_AddToPending,
           (void*)s_fn_TradeSetup);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0xa8 (TradeComponent_Factory MemAlloc), flags 0x83 (Ctor 0x14005c430:
// Component base sets +0x20=0x80, leaf OR 0x3 => 0x83).
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(TradeComponent, 0xa8, 0x83)
REGISTER_TYPE(TradeComponent, "ControllerComponent",
              &TradeComponent_Factory, &TradeComponent_CreateInPlace);
#endif // STANDALONE_EXE
