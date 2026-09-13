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

#include "Component/storagelocationcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
// Because the StorageBarn seeds the SM with BuildComponent's default state (1, the
// first-linked controller), this node — the SM chain TAIL — gets Step(1) which is not
// in its table (returns -1), so Tick walks up to BuildComponent's node (Step(1)->0):
// the real multi-controller parent-walk.
//
// BOTH enter handlers are now REAL (2026-07-27). They were left null on the belief
// that they were "exe code unmapped in standalone"; they are not — both are ported:
//   * state 2 enter = helper_d230 (0x14012d230) = TownRegistry_RegisterStorageLocation,
//     the townMgr+0x00 APPEND. Verified to be the state-2 enter and NOTHING else: the
//     function's only xrefs are .pdata plus SetupCrossRefs and its inlined twin
//     impl_d410, and it appears in no vtable. So "entity becomes a town storage
//     location" IS "this component's state machine entered state 2".
//   * state 3 enter = helper_d1f0 (0x14012d1f0) = the matching ERASE
//     (DynArray8_EraseByValue over the same *(gw+0xf0) list, i.e. 0x140002cc0 which we
//     ship as CemeteryListErase_Reimpl). state 3 = "no longer a storage location".
// Both take the component (the enter dispatcher passes node+0x18 = the owning
// component) and read the entity from comp+0x10, which is exactly the argument
// TownRegistry_RegisterStorageLocation already expects.
namespace { struct StorageCtrlTag {}; }
using StorageCtrlDispatch = ::ControllerLive::Dispatcher<StorageCtrlTag>;

extern "C" void TownRegistry_RegisterStorageLocation(void* storageLocationComp);
extern "C" void CemeteryListErase_Reimpl(void* listBase, void* ptrValue);

// state-2 ENTER — port of StorageLocationDescription_helper_d230.
//
// RETURN 0, NOT 1. Retail d230 ends `return uVar1 & 0xffffffffffffff00` — AL is
// explicitly zeroed. The return value is StateMachine::Tick's walk-stop bit: the
// enter walk runs tail->parent through EVERY controller node while handlers
// return 0, and stops at the first non-zero (ClearArea's 77f0 returns 1 by
// design; Storage/WPC/Residence return 0). Returning 1 here stopped the barn's
// state-2 walk at this node, starving WPC's edf0 (so the completed barn stayed
// registered as an available Builder workplace, capacity 4, and absorbed the
// first 4 builders town-wide) and ClearArea's state-2 event-1 broadcast.
static char StorageLoc_State2Enter(void* data, void* /*sm*/) {
    if (!data) return 0;
    TownRegistry_RegisterStorageLocation(data);
    return 0;
}

// state-3 ENTER — port of StorageLocationDescription_helper_d1f0. Retail:
//   local[0] = *(comp+0x10); DynArray8_EraseByValue(*(gw+0xf0), &local[0]);
// The list base is the town manager itself (townMgr+0x00 is the array head), which is
// the same object the state-2 append grows.
// Returns 0 for the same reason as State2Enter above (retail d1f0 zeroes AL too).
static char StorageLoc_State3Enter(void* data, void* /*sm*/) {
    if (!data) return 0;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 0;
    void* townMgr = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf0);
    if (!townMgr) return 0;
    uint64_t entityRef = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(data) + 0x10);
    CemeteryListErase_Reimpl(townMgr, &entityRef);
    return 0;
}

static void StorageLocationComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = StorageCtrlDispatch::table;
    if (t.count != 0) return;  // idempotent
    TableRegisterState(t, 2, reinterpret_cast<void*>(&StorageLoc_State2Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);
    TableRegisterState(t, 3, reinterpret_cast<void*>(&StorageLoc_State3Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
}
static void** StorageLocationComponent_NodeVtable() { return StorageCtrlDispatch::Vtable(); }

CONTROLLER_LIVE_FACTORY_STATES(StorageLocationComponent, 0x48, 0x82, 0x28, 2,
                               "BANISHED_LIVE_STORAGECTRL",
                               StorageLocationComponent_NodeVtable,
                               StorageLocationComponent_PopulateStateTable)
CONTROLLER_INSTALL_SERIALIZE(StorageLocationComponent,
                             StorageLocationComponent_Shutdown)
REGISTER_TYPE(StorageLocationComponent, "ControllerComponent",
              &StorageLocationComponent_Factory_Ser,
              &StorageLocationComponent_CreateInPlace_Ser);
