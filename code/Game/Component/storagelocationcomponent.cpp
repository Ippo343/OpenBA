// code/Game/Component/storagelocationcomponent.cpp
// Clean reimplementation of StorageLocationComponent lifecycle methods from Application-x64.exe.
//
// StorageLocationComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Registers entity as a storage location in the game world.
//
// Vtable at 0x140164880. Two non-trivial overrides:
//   Shutdown  (0x14012d2a0) — Component_Serialize + two empty Serializer blocks
//   OnCreate  (0x14012d270) — register entity with game world storage location list.
//             (Its standalone port StorageLocationComponent_OnCreate had no caller and
//             was deleted in B3R G-CompD; the live registration is the state-2 enter
//             below, TownRegistry_RegisterStorageLocation.)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/StorageLocationComponent.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <new>

// ===== ENDGAME B.3.8 MI-B — StorageLocationComponent as a REAL class =====
// (StorageLocationComponent.h). Before: CONTROLLER_LIVE_FACTORY_STATES +
// CONTROLLER_INSTALL_SERIALIZE (48-slot table filled per create, Inert_ScalarDtor
// in slot 0, Dispatcher<StorageCtrlTag> stamped into raw bytes at +0x28). After: the
// compiler's 18-slot vtable, a real ctor/dtor chain, the controller a member object
// (B38_RULES D1, D2, D6).
#include "Object/factory_registration.h"
#include "Component/controller_state_table.h"
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
// BOTH enter handlers are REAL (2026-07-27):
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
    if (GameUpdateLoop::_singleton == nullptr) return 0;
    void* townMgr = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton) + 0xf0);
    if (!townMgr) return 0;
    uint64_t entityRef = *reinterpret_cast<uint64_t*>(
        reinterpret_cast<uintptr_t>(data) + 0x10);
    CemeteryListErase_Reimpl(townMgr, &entityRef);
    return 0;
}

// Built ONCE at static-init time, as retail's SetupCrossRefs does (B38 D6/D7).
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
static const bool s_storageStateTableBuilt = (StorageLocationComponent_PopulateStateTable(), true);

// 0x14012d360. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; then `_flags |= 2` (0x82).
StorageLocationComponent::StorageLocationComponent() : _controller(this) {
    OrFlagsInCtor(0x2);
}

// create 0x140038df0: MemAlloc(0x48, 8), then the ctor.
void* StorageLocationComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageLocationComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageLocationComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* StorageLocationComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageLocationComponent());
}

// ============================================================
// StorageLocationComponent::Persist (vtable slot [3], 0x14012d2a0)
// ============================================================
// Component_Serialize + two empty Serializer blocks (version 0).
//
// Original disassembly:
//   CALL Component_Serialize(self, stream)
//   Serializer_Ctor(local1, stream, 0)
//   Serializer_Dtor(local1)
//   Serializer_Ctor(local2, stream, 0)
//   Serializer_Dtor(local2)

void StorageLocationComponent::Persist(void* stream) {
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    alignas(8) uint8_t serializerBuf1[24];
    ::new (static_cast<void*>(serializerBuf1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf1)->~Serializer();   // B.3.4 bridge

    alignas(8) uint8_t serializerBuf2[24];
    ::new (static_cast<void*>(serializerBuf2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializerBuf2)->~Serializer();   // B.3.4 bridge
}

DEFINE_EXE_TYPE(StorageLocationComponent, "ControllerComponent")
