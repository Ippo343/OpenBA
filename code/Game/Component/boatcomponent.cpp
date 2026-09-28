// code/Game/Component/boatcomponent.cpp
// Clean reimplementation of BoatComponent lifecycle methods.
//
// BoatComponent (0x98 bytes) manages fishing boat state:
// entity refs, position vectors, route waypoints, dock ID.
// Secondary vtable sub-object at +0x78 (ControllerComponent pattern).
//
// Vtable at 0x140158f40. Non-trivial override:
//   Shutdown (0x1400465d0) -- 95-instruction complex serialization
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/BoatComponent.h"
#include "Component/CharacterComponent.h"
#include "Component/ModelComponent.h"
#include "Component/ResidenceComponent.h"
#include "Component/coll_bridge.h"
#include "object/TypeInfo.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// FUN_14000d030 -- resize Vector3 array. ENDGAME B.3.2 step 2: this IS
// Collection::Array<Vector3>::SetSize — Persist's call site goes straight through
// the GameColl bridge. (The free BoatComponent_ResizeVec3Array copy and its
// reader-less 0x14000d030 row went at the B.3.8 close.)

// ============================================================================
// ENDGAME B.3.8 MI-A — BoatComponent as a REAL class (BoatComponent.h).
//
// Before: a 48-slot hand-built `BoatComponent_LiveVtable` filled at every
// create (FillTypedVtable + InstallController), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<BoatCtrlTag> table stamped into raw bytes at +0x78.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/BoatComponent.h"

// ── Controller state machine (boatcomponent_states.cpp) ──────────────────────────
// BoatComponent is a state-machine component in exactly TradeComponent's shape, and the
// geometry is read off its retail vtable 0x140158f40 rather than guessed:
//   slot 16 = 0x140003730 (`XOR EAX,EAX; RET`)  => the machine starts in state 0
//   slot 17 = 0x140003720 (`LEA RAX,[RCX+0x78]`) => member at +0x78, back-pointer at +0x90
// The state table is built ONCE at static-init time, as retail's static initializer
// does (B38 D6/D7).
extern "C" void BoatCtrl_PopulateTable();
static const bool s_boatStateTableBuilt = (BoatCtrl_PopulateTable(), true);

// 0x140046800. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// +0x28..+0x68 = 0, +0x70 = -1 (0x14004685c: the route index, "no waypoint yet" —
// the route builder resets it to 0 with the first point, boatcomponent_route.cpp);
// `_controller(this)` is the member's back-pointer at +0x90. No flag OR (0x80).
BoatComponent::BoatComponent()
    : data28(0), data30(0), data38(0), data40(0), data48(0), _pad4C{}, data50(0), data58(0),
      _pad5C{}, data60(0), data68(0), data70(-1), _pad74{}, _controller(this) {}

// 0x140046770: the route waypoint array (+0x60 data, +0x68 count/capacity) is freed
// and zeroed; the member's and the bases' dtors (restamps only) and ~Internal are
// the compiler's chain.
BoatComponent::~BoatComponent() {
    Object::detail::InternalMemFree(reinterpret_cast<void*>(data60));
    data60 = 0;
    data68 = 0;
}

// create 0x1400036e0: MemAlloc(0x98, 8), then the ctor.
void* BoatComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BoatComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BoatComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* BoatComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BoatComponent());
}

// [3] 0x1400465d0: Component::Persist (a direct CALL 0x1401189b0), an empty
// Serializer scope (format compat), then one scope carrying: the +0x28 pointer
// (validated against ti 0x1401c7cf0 = ResidenceComponent, the tree's old name for
// it was "ComponentEntity"), the +0x40 Vector3, the +0x4c float, the +0x30 pointer
// (ModelComponent, ti 0x1401c7a50), the +0x38 pointer (CharacterComponent, ti
// 0x1401c7078), the +0x50 Vector3, the +0x70 route index and the +0x60 Vector3
// route array (count at +0x68, resized on a load). A pointer whose object fails
// its IsDerivedFrom check is cleared.
void BoatComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);         // B.3.8 bridge
    auto persistPtr = [&ser](uint64_t& field, uint32_t typeId) {
        reinterpret_cast<Object::InternalBasePtr*>(&field)->Serialize(ser);   // B.3.8 bridge (the field is a raw InternalBasePtr)
        void* obj = reinterpret_cast<void*>(field);
        if (obj != nullptr && !::Object::IsDerivedFromNullSafe(obj, typeId))
            field = 0;
    };
    persistPtr(data28, ResidenceComponent::TypeId());
    reinterpret_cast<Math::Vector3*>(&data40)->Serialize(&ser);                        // B.3.8 bridge (+0x40 is a Vector3)
    ser.Serialize(*reinterpret_cast<float*>(&_pad4C), nullptr);                        // +0x4c float
    persistPtr(data30, ModelComponent::TypeId());
    persistPtr(data38, CharacterComponent::TypeId());
    reinterpret_cast<Math::Vector3*>(&data50)->Serialize(&ser);                        // B.3.8 bridge (+0x50 is a Vector3)
    ser.Serialize(*reinterpret_cast<int*>(&data70), nullptr);
    uint32_t count = *reinterpret_cast<uint32_t*>(&data68);
    ser.Serialize(*reinterpret_cast<int*>(&count), nullptr);
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // B.3.2 bridge: exe 0x14000d030 IS Collection::Array<Vector3>::SetSize(n).
        GameColl::AsArray<GameColl::Vec3>(&data60).SetSize(static_cast<int>(count));
    }
    uint8_t* route = reinterpret_cast<uint8_t*>(data60);
    if (static_cast<int>(count) > 0 && route) {
        for (uint32_t i = 0; i < count; i++, route += 12)   // sizeof(Vector3) = 12
            reinterpret_cast<Math::Vector3*>(route)->Serialize(&ser);   // B.3.8 bridge
    }
}

DEFINE_EXE_TYPE(BoatComponent, "ControllerComponent")
