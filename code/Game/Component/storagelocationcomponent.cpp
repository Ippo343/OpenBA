// code/Game/Component/storagelocationcomponent.cpp
// Clean reimplementation of StorageLocationComponent lifecycle methods from Application-x64.exe.
//
// StorageLocationComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Registers entity as a storage location in the game world.
//
// Vtable at 0x140164880. Two non-trivial overrides:
//   Shutdown  (0x14012d2a0) — Component_Serialize + two empty Serializer blocks
//   OnCreate  (0x14012d270) — register entity with game world storage location list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/storagelocationcomponent_funcs.h"
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

// DAT_1401c3dd8 — game state singleton pointer
static void** s_pGameState = nullptr;

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

    s_pGameState = reinterpret_cast<void**>(
        ResolveAddr(Module::Exe, 0x1401c3dd8));

    s_initialized = true;
}

// ============================================================
// StorageLocationComponent::Shutdown (vtable slot [3], 0x14012d2a0)
// ============================================================
// Component_Serialize + two empty Serializer blocks (version 0).
//
// Original disassembly:
//   CALL Component_Serialize(self, stream)
//   Serializer_Ctor(local1, stream, 0)
//   Serializer_Dtor(local1)
//   Serializer_Ctor(local2, stream, 0)
//   Serializer_Dtor(local2)

void StorageLocationComponent_Shutdown(StorageLocationComponent* self, void* stream) {
    EnsureHelpers();

    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    uint8_t serializerBuf1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializerBuf1, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf1);

        uint8_t serializerBuf2[24];
        s_fn_Serializer_Ctor(serializerBuf2, stream, 0);
        s_fn_Serializer_Dtor(serializerBuf2);
    }
}

// ============================================================
// StorageLocationComponent::OnCreate (vtable slot [7], 0x14012d270)
// ============================================================
// Registers the owning entity with the game world's storage location list.
// Gets entity ptr from self+0x10, registers on gameState->+0xf0 list.
//
// Original disassembly:
//   MOV RAX,[RCX+0x10]        ; entity ptr
//   MOV RCX,[0x1401c3dd8]     ; game state singleton
//   MOV RCX,[RCX+0xf0]        ; world manager
//   LEA RDX,[RAX+0x28]        ; entity chain ptr (entity+0x28)
//   CALL FUN_140002cc0         ; register entity

void StorageLocationComponent_OnCreate(StorageLocationComponent* self) {
    EnsureHelpers();

    if (!s_fn_RegisterEntity || !s_pGameState) return;

    uint64_t entityPtr = (uint64_t)(uintptr_t)self->owner;

    uintptr_t gameState = (uintptr_t)*s_pGameState;
    uintptr_t worldMgr = *(uintptr_t*)(gameState + 0xf0);

    // Register entity at worldMgr + 0x00 (storage location entity list)
    s_fn_RegisterEntity((void*)worldMgr, &entityPtr);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void StorageLocationComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[StorageLocationComponent] Funcs resolved: Component_Serialize=%p "
           "RegisterEntity=%p pGameState=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_RegisterEntity,
           (void*)s_pGameState);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"
// Phase 3 / Keystone D step 3 — live ControllerComponent with the REAL state-type
// table + dispatcher node virtuals. Embedded state node at +0x28 (CreateState
// 0x140004080 = LEA RAX,[RCX+0x28]); GetDefaultState (0x1400026c0) returns 2. When
// BANISHED_LIVE_STORAGECTRL is set the entity's StateMachineComponent links this
// node and ticks it through the dispatcher vtable below.
//
// Storage state graph (recovered from StorageLocationDescription_SetupCrossRefs ->
// sub_9040/sub_9150, 0x14014fc70): two states 2 (default), 3.
//   state 2: enter=helper_d230  ; transitions -> {3}
//   state 3: enter=helper_d1f0  ; transitions -> {6, 4, 5}
// (state-ids 6/4/5 are transition targets only — not registered states here.)
// Handlers are exe code unmapped in standalone -> populated null (graceful no-op);
// the ids + transitions are real. Because the StorageBarn seeds the SM with
// BuildComponent's default state (1, the first-linked controller), this node — the
// SM chain TAIL — gets Step(1) which is not in its table (returns -1), so Tick walks
// up to BuildComponent's node (Step(1)->0): the real multi-controller parent-walk.
namespace { struct StorageCtrlTag {}; }
using StorageCtrlDispatch = ::ControllerLive::Dispatcher<StorageCtrlTag>;

static void StorageLocationComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = StorageCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 2, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static void** StorageLocationComponent_NodeVtable() { return StorageCtrlDispatch::Vtable(); }

CONTROLLER_LIVE_FACTORY_STATES(StorageLocationComponent, 0x48, 0x82, 0x28, 2,
                               "BANISHED_LIVE_STORAGECTRL",
                               StorageLocationComponent_NodeVtable,
                               StorageLocationComponent_PopulateStateTable)
REGISTER_TYPE(StorageLocationComponent, "ControllerComponent",
              &StorageLocationComponent_Factory, &StorageLocationComponent_CreateInPlace);
#endif // STANDALONE_EXE
