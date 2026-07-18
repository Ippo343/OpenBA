// code/Game/Component/marketlocationcomponent.cpp
// Clean reimplementation of MarketLocationComponent lifecycle methods from Application-x64.exe.
//
// MarketLocationComponent is a ControllerComponent subclass (0x48 bytes) with no
// additional fields. Two non-trivial vtable overrides:
//   Shutdown (0x14004efa0) — Component_Serialize + two empty Serializer blocks
//   OnCreate (0x14004ef60) — register entity with game world entity list
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/marketlocationcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base Shutdown) at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// FUN_140002cc0 — register entity with world entity list
typedef void (*fn_RegisterEntity_t)(void* listPtr, void* entityPtr);
static fn_RegisterEntity_t s_fn_RegisterEntity = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_fn_RegisterEntity = reinterpret_cast<fn_RegisterEntity_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_initialized = true;
}

// ============================================================
// MarketLocationComponent::Shutdown (vtable slot [3], 0x14004efa0)
// ============================================================
// Calls Component_Serialize (base), then creates/destroys two empty
// Serializer blocks (version 0 each). These exist for binary format
// compatibility (versioned sub-sections with no fields).
//
// Original disassembly:
//   CALL Component_Serialize (0x1401189b0)
//   CALL Serializer_Ctor(local1, stream, 0)
//   CALL Serializer_Dtor(local1)
//   CALL Serializer_Ctor(local2, stream, 0)
//   CALL Serializer_Dtor(local2)

void MarketLocationComponent_Shutdown(MarketLocationComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Two empty serializer blocks (version 0)
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        uint8_t serializerBuf1[24];
        s_fn_Serializer_Ctor(serializerBuf1, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf1);

        uint8_t serializerBuf2[24];
        s_fn_Serializer_Ctor(serializerBuf2, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf2);
    }
}

// ============================================================
// MarketLocationComponent::OnCreate (vtable slot [7], 0x14004ef60)
// ============================================================
// Registers the owning entity with the game world's market location list.
// Gets entity ptr from self->owner (+0x10), then registers at
// (gameState->+0xf0)+0x10 list.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]          ; entity ptr from owner
//   LEA RDX,[RSP+0x30]          ; stack InternalBasePtr
//   MOV [RSP+0x30],RAX          ; store entity ptr
//   MOV RAX,[DAT_1401c3dd8]     ; game state singleton
//   MOV RCX,[RAX+0xf0]          ; world manager
//   ADD RCX,0x10                ; market location entity list
//   CALL FUN_140002cc0           ; register entity

void MarketLocationComponent_OnCreate(MarketLocationComponent* self) {
    EnsureHelpers();

    if (!s_fn_RegisterEntity || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
        return;

    // Build an InternalBasePtr on the stack with the owner entity
    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    // Get the world manager from game state
    uintptr_t gameState = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr + 0x10 (market location entity list)
    s_fn_RegisterEntity((void*)(worldMgr + 0x10), &entityPtr);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void MarketLocationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[MarketLocationComponent] Funcs resolved: Component_Serialize=%p "
           "Ctor=%p Dtor=%p RegisterEntity=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor, (void*)s_fn_RegisterEntity);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// size 0x48 (MarketLocationComponent_Factory MemAlloc), flags 0x82 (Ctor
// 0x14004f060: Component base sets +0x20=0x80, leaf OR 0x2 => 0x82).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(MarketLocationComponent, 0x48, 0x82)
REGISTER_TYPE(MarketLocationComponent, "ControllerComponent",
              &MarketLocationComponent_Factory, &MarketLocationComponent_CreateInPlace);
