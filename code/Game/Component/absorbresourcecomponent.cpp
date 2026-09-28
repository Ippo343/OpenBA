// code/Game/Component/absorbresourcecomponent.cpp
// Clean reimplementation of AbsorbResourceComponent lifecycle methods.
//
// AbsorbResourceComponent (0x48 bytes) is a ControllerComponent subclass with no
// additional fields. Only one non-trivial vtable override:
//   Shutdown (0x140105c00) -- Component_Serialize + two empty Serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.

#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "resolve.h"
#include "Game/serialize_externals.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================================
// ENDGAME B.3.8 MI-A — AbsorbResourceComponent as a REAL class with its
// controller member (AbsorbResourceComponent.h, B38_RULES D9 #8).
//
// Before: COMPONENT_INERT_FACTORY_SER — a 48-slot inert table re-filled at every
// create, `Inert_ScalarDtor` in slot 0, and NO controller: slots 16/17 were inert
// stubs, so the entity's StateMachineComponent never linked this component. After:
// the compiler's 18-slot vtable, a real constructor and destructor chain, and the
// retail controller member at +0x28 over its one-state table.
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/AbsorbResourceComponent.h"

#include "Component/coll_bridge.h"
#include "Component/RawMaterialComponent.h"   // ENDGAME B.3.9 C1: 0x1401c7c80 cell class
#include "Component/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"

using AbsorbResourceCtrlDispatch = ::ControllerLive::Dispatcher<AbsorbResourceCtrlTag>;

// ============================================================
// State 0 per-frame handler — retail 0x140105950 (member column +0x18, TickAlt).
// ============================================================
// Every frame while the entity is in state 0, sweep the tiles of its footprint
// and pull every loose raw material lying there into the entity's own storage:
//
//   map  = FindComponent<MapComponent>(entity)            0x140004680 (tile rect)
//   stor = FindComponent<StorageComponent>(entity)        0x140006270
//   for y in [map.y, map.y + map.h), x in [map.x, map.x + map.w):
//       e = world grid cell (x, y)'s entity               (*(gw+0xf8): width @+0,
//                                                          cells @+0x80, stride 0x30,
//                                                          entity @ cell+8)
//       rm = first component of e whose Type() == tid(0x1401c7c80), then
//            IsDerivedFrom-confirmed, as every retail finder does
//       if (rm && StorageCanAdd(stor, e+0x28, rm+0x28))    0x14012cd80
//           StorageAdd(stor, e+0x28, rm+0x28)             0x14012d000
//           if (!(e+0x40 bit 10)) { MarkForRemoval(e)     0x1400c2ea0
//               append e to (gw+0xe8)+0xd0 }              0x140003110 SetSize(n+1, &e)
//   return 1
//
// Read from the disassembly (the decompile names this AbsorbResourceDescription_
// helper_ec70_sub_5950). The map/storage finds and the grid, storage and removal
// derefs are UNGUARDED in retail and stay so: retail relies on every entity
// carrying this component also carrying a MapComponent and a StorageComponent.
// The helpers are called directly: FloorComponent_FindResData 0x140004680 (the
// MapComponent find), WorkBase_FindComponent_6270 0x140006270 (StorageComponent),
// Pf_StorageCanAccept_cd80 0x14012cd80, ActRm_StorageAdd_d000 0x14012d000,
// ActHunt_MarkEntityRemoval_c2ea0 0x1400c2ea0.
extern "C" char AbsorbResource_State0Tick(void* compV, void* /*sm*/, void* /*time*/) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(compV);
    // ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7c80 is RawMaterialComponent's registered
    // type-id cell (INVENTORY_C Table 1), read here BY CELL, not by the old local's name.
    const uint32_t tidRawMaterial = RawMaterialComponent::TypeId();

    const int64_t entity = *reinterpret_cast<int64_t*>(comp + 0x10);
    uintptr_t map = 0;
    FloorComponent_FindResData(reinterpret_cast<void*>(entity), &map);
    void* storage = nullptr;
    WorkBase_FindComponent_6270(static_cast<uintptr_t>(entity), &storage);

    const int64_t y0 = *reinterpret_cast<int16_t*>(map + 0x2a);
    const int64_t x0 = *reinterpret_cast<int16_t*>(map + 0x28);
    const int64_t yEnd = static_cast<int>(y0) + *reinterpret_cast<int16_t*>(map + 0x2e);
    const int64_t xEnd = static_cast<int>(x0) + *reinterpret_cast<int16_t*>(map + 0x2c);
    const uintptr_t gameWorld = reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton);
    const int* grid = *reinterpret_cast<int* const*>(gameWorld + 0xf8);

    for (int64_t y = y0; y < yEnd; ++y) {
        for (int64_t x = x0; x < xEnd; ++x) {
            const int64_t cellIndex = static_cast<int64_t>(grid[0]) * y + x;
            const int64_t cell = *reinterpret_cast<const int64_t*>(grid + 0x20) + cellIndex * 0x30;
            int64_t cellEntity = *reinterpret_cast<int64_t*>(cell + 8);
            if (cellEntity == 0) continue;

            void* rawMaterial = nullptr;
            const int compCount = *reinterpret_cast<int*>(cellEntity + 0x38);
            const int64_t comps = *reinterpret_cast<int64_t*>(cellEntity + 0x30);
            for (int i = 0; i < compCount; ++i) {
                ::Component* c = *reinterpret_cast<::Component**>(comps + static_cast<int64_t>(i) * 8);
                if (c->Type() != tidRawMaterial) continue;
                if (::Object::IsDerivedFromNullSafe(c, tidRawMaterial)) rawMaterial = c;
                break;
            }
            int64_t rmIBP = 0;
            new (&rmIBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(rawMaterial));   // B.3.3 bridge
            if (rmIBP == 0) continue;

            void* item = *reinterpret_cast<void**>(cellEntity + 0x28);
            const int quantity = *reinterpret_cast<int*>(rmIBP + 0x28);
            if (!(Pf_StorageCanAccept_cd80(reinterpret_cast<int64_t>(storage),
                                           reinterpret_cast<int64_t>(item), quantity) & 0xff)) continue;
            ActRm_StorageAdd_d000(reinterpret_cast<uintptr_t>(storage), reinterpret_cast<int64_t>(item),
                                  *reinterpret_cast<int*>(rmIBP + 0x28));

            const int64_t entityList = *reinterpret_cast<int64_t*>(
                reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xe8);
            if (((*reinterpret_cast<uint32_t*>(cellEntity + 0x40) >> 10) & 1) != 0) continue;
            ActHunt_MarkEntityRemoval_c2ea0(reinterpret_cast<void*>(cellEntity));
            // B.3.2 bridge: exe 0x140003110 IS Collection::Array<void*>::SetSize(n, fill).
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(entityList + 0xd0))
                .SetSizeFill(*reinterpret_cast<int*>(entityList + 0xd8) + 1, &cellEntity);
        }
    }
    return 1;
}

// The retail table, transcribed from the static initializer 0x14014e4f0 ->
// 0x14002ec70: ONE state, id 0, with only the per-frame TickAlt column (+0x18)
// set (0x140105950 above); no enter, no update, no transition, no event. Built
// ONCE at static-init time, as retail's initializer does (B38 D6/D7).
static void AbsorbResourceComponent_PopulateStateTable() {
    using namespace ::ControllerLive;
    StateTable& t = AbsorbResourceCtrlDispatch::table;
    if (t.count != 0) return;                      // idempotent
    TableRegisterState(t, 0, nullptr, nullptr,
                       reinterpret_cast<void*>(&AbsorbResource_State0Tick), nullptr);
}
static const bool s_absorbResourceStateTableBuilt = (AbsorbResourceComponent_PopulateStateTable(), true);

// 0x140105cc0. Component() is the CALL 0x1401188b0; the 0x140162b80 and
// 0x140162000 stamps and the member's 0x14015f070 / 0x140161fb0 stamps are the
// compiler's; `_controller(this)` is the +0x40 back-pointer store and the +0x30 /
// +0x38 zeroes. No flag OR (0x80).
AbsorbResourceComponent::AbsorbResourceComponent() : _controller(this) {}

// 0x140105c60: restamp + ~Internal, no own work.
AbsorbResourceComponent::~AbsorbResourceComponent() = default;

// [3] 0x140105c00: Component::Persist plus TWO empty Serializer scopes — an
// empty scope still occupies 16 bytes in the save encoding. Retail calls
// 0x1401189b0 (Component::Persist) directly, not ControllerComponent's.
void AbsorbResourceComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge
}

// create 0x14002ea20: MemAlloc(0x48, 8), then the ctor.
void* AbsorbResourceComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(AbsorbResourceComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) AbsorbResourceComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* AbsorbResourceComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) AbsorbResourceComponent());
}

DEFINE_EXE_TYPE(AbsorbResourceComponent, "ControllerComponent")
