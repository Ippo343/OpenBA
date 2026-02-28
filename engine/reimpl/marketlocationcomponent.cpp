// engine/reimpl/marketlocationcomponent.cpp
// Reimplementation of MarketLocationComponent lifecycle methods from Application-x64.exe.
//
// MarketLocationComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. Two non-trivial vtable overrides:
//
// Methods reimplemented:
//   - Shutdown (0x14004efa0) — Component_Serialize + two empty Serializer blocks
//   - OnCreate (0x14004ef60) — register entity with game world entity list
//
// All other vtable slots are no-ops.

#include "MarketLocationComponent.h"
#include "hooks.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>

// ============================================================
// Hook state
// ============================================================
static HookInfo g_hookShutdown;
static HookInfo g_hookOnCreate;

// ============================================================
// Local helper function pointers
// ============================================================

// Component_Serialize (base Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via IAT
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t fn_Serializer_Dtor = nullptr;

// FUN_140002cc0 — register entity with world entity list
// RCX = list ptr (from game state), RDX = InternalBasePtr*
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t fn_RegisterEntity = nullptr;

// DAT_1401c3dd8 — game state singleton pointer
static void** g_pGameState = nullptr;

// ============================================================
// Shutdown (0x14004efa0)
// Same pattern: Component_Serialize + two empty Serializer blocks
// ============================================================
static void Reimpl_Shutdown(MarketLocationComponent* self, void* stream) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[MarketLocationComponent::Shutdown] self=%p stream=%p\n",
               (void*)self, stream);
        callCount++;
    }
#endif

    fn_Component_Serialize(self, stream);

    uint8_t serializerBuf1[16];
    fn_Serializer_Ctor(serializerBuf1, stream, 0);
    fn_Serializer_Dtor(serializerBuf1);

    uint8_t serializerBuf2[16];
    fn_Serializer_Ctor(serializerBuf2, stream, 0);
    fn_Serializer_Dtor(serializerBuf2);
}

// ============================================================
// OnCreate (0x14004ef60)
// Registers the owning entity with the game world's market location list.
// Gets entity ptr from self+0x10, then calls register on
// (gameState->+0xf0)+0x10 list.
// ============================================================
static void Reimpl_OnCreate(MarketLocationComponent* self) {
#ifdef _DEBUG
    static int callCount = 0;
    if (callCount < 3) {
        printf("[MarketLocationComponent::OnCreate] self=%p owner=%p\n",
               (void*)self, self->owner);
        callCount++;
    }
#endif

    // Build an InternalBasePtr on the stack with the owner entity
    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    // Get the world manager from game state
    uintptr_t gameState = (uintptr_t)*g_pGameState;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr + 0x10 (market location entity list)
    fn_RegisterEntity((void*)(worldMgr + 0x10), &entityPtr);
}

// ============================================================
// Install / Remove
// ============================================================
void InstallMarketLocationComponentShutdown() {
    fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t iatCtor = ResolveAddr(Module::Exe, 0x140152ad8);
    uintptr_t iatDtor = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iatCtor) fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(iatCtor);
    if (iatDtor) fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(iatDtor);

    if (!fn_Component_Serialize || !fn_Serializer_Ctor || !fn_Serializer_Dtor) {
#ifdef _DEBUG
        printf("[MarketLocationComponent] ERROR: failed to resolve Shutdown helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004efa0);
    // Prologue: PUSH RBX(2) + SUB RSP,0x40(4) + MOV RBX,RDX(3) + CALL(5) = 14
    InstallHook(target, (void*)Reimpl_Shutdown, &g_hookShutdown, 14);
#ifdef _DEBUG
    printf("[MarketLocationComponent] Shutdown hooked at %p (prologue=14)\n", (void*)target);
#endif
}

void RemoveMarketLocationComponentShutdown() { RemoveHook(&g_hookShutdown); }

void InstallMarketLocationComponentOnCreate() {
    fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));
    g_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    if (!fn_RegisterEntity || !g_pGameState) {
#ifdef _DEBUG
        printf("[MarketLocationComponent] ERROR: failed to resolve OnCreate helpers\n");
#endif
        return;
    }

    uintptr_t target = ResolveAddr(Module::Exe, 0x14004ef60);
    // Prologue: SUB RSP,0x28(4) + MOV RAX,[RCX+0x10](4) + LEA RDX,[RSP+0x30](5) = 13
    InstallHook(target, (void*)Reimpl_OnCreate, &g_hookOnCreate, 13);
#ifdef _DEBUG
    printf("[MarketLocationComponent] OnCreate hooked at %p (prologue=13)\n", (void*)target);
#endif
}

void RemoveMarketLocationComponentOnCreate() { RemoveHook(&g_hookOnCreate); }
