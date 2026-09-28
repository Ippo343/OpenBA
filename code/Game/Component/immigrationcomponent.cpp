// code/Game/Component/immigrationcomponent.cpp
// Clean reimplementation of ImmigrationComponent::Shutdown from Application-x64.exe.
//
// ImmigrationComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages immigration/nomad arrival events.
//
// Vtable at 0x140159b30:
//   [3] Shutdown = 0x14004e4b0 -- complex serialization with entity array loop
//
// Ghidra decompilation:
//   Component_Serialize(param_1, param_2)
//   Serializer(local_20, param_2, 0); ~Serializer(local_20)  // empty controller block
//   Serializer(local_30, param_2, 0)
//   Serialize(local_30, param_1+0x50, nullptr)  // field50 (int)
//   local_res8 = *(param_1+0x60)                // array count
//   Serialize(local_30, local_res8, nullptr)
//   if loading: DynArray_Resize(param_1+0x58, count)
//   for each element in array:
//     InternalBasePtr::Serialize(element, serializer)
//     if ptr != null && !IsDerivedFrom(ptr, ComponentEntity): clear ptr
//   Time::Serialize(param_1+0x48, serializer)
//   ~Serializer(local_30)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/ImmigrationComponent.h"
#include "Entity/ComponentEntity.h"
#include "Component/coll_bridge.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

// FUN_1400032e0 -- dynamic array resize; this file DEFINES its body as
// ImmigrationComp_DynArrayResize below (kept, still registered for other
// callers) — Persist's own call site goes straight through the bridge.

// ============================================================
// Reimpl of FUN_1400032e0 (Component reg wave "people") — shared DynArray<ptr> resize.
// ENDGAME B.3.2 step 2: this IS Collection::Array<void*>::SetSize(n) — the old
// hand-written body's copy path did `*slot = 0;` immediately before assigning
// the copied 8-byte value into the same slot; that default-construct-then-
// assign is exactly what SetSizeCore's byte-copy `assign` does for a
// zero-default element and is unobservable (the write is overwritten before
// anything reads it), so the bridge call below is byte-for-byte identical.
// Non-static: still registered by reg_component_people.cpp for other callers,
// so this function is KEPT (not deleted/unregistered) — only its body changed.
// ============================================================
void ImmigrationComp_DynArrayResize(int64_t* param_1, int newCount) {
    GameColl::AsArray<GameColl::Ptr8>(param_1).SetSize(newCount);
}

// ============================================================================
// ENDGAME B.3.8 MI-A — ImmigrationComponent as a REAL class (ImmigrationComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES + CONTROLLER_INSTALL_SERIALIZE (a 48-slot
// table re-filled at every create), `Inert_ScalarDtor` in slot 0 (only `~Internal`), the
// ControllerLive::Dispatcher<ImmigrationCtrlTag> table stamped into raw bytes at +0x28.
// After: the compiler's 18-slot vtable, a real constructor and destructor
// chain, the controller a member object (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"
#include "Component/ImmigrationComponent.h"

// The nomad-arrival state machine (states 2/0xf/0x10/0x11,
// immigrationcomponent_states.cpp). Node at +0x28 (slot 17 0x140004080), default
// state 2 (slot 16 0x1400026c0). The state table is built ONCE at static-init time,
// as retail's static initializer does (B38 D6/D7).
extern "C" void ImmigrationCtrl_PopulateTable();
static const bool s_immigrationStateTableBuilt = (ImmigrationCtrl_PopulateTable(), true);

// 0x14004e640. Component() is the CALL 0x1401188b0; the stamps are the compiler's;
// `_controller(this)` is the member's back-pointer; +0x48..+0x60 = 0. No flag OR.
ImmigrationComponent::ImmigrationComponent()
    : _controller(this), field48(nullptr), field50(0), _pad54{}, field58(nullptr), field60(nullptr) {}

// 0x14004e5d0: the group array (+0x58 data, +0x60 count/capacity) is freed and
// zeroed; the member's and the bases' dtors (restamps only) and ~Internal are the
// compiler's chain.
ImmigrationComponent::~ImmigrationComponent() {
    Object::detail::InternalMemFree(field58);
    field58 = nullptr;
    field60 = nullptr;
}

// create 0x140007a20: MemAlloc(0x68, 8), then the ctor.
void* ImmigrationComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ImmigrationComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ImmigrationComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* ImmigrationComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ImmigrationComponent());
}

// [3] 0x14004e4b0: Component::Persist (a direct CALL 0x1401189b0), an empty
// Serializer scope (the controller block), then one scope carrying field50, the
// +0x58 entity array (count, resize on a load, each InternalBasePtr — cleared when
// it is not a ComponentEntity) and the Time at +0x48.
void ImmigrationComponent::Persist(void* stream) {
    Component::Persist(stream);
    { IO::Serializer scope(*reinterpret_cast<IO::Stream*>(stream), 0); }   // B.3.8 bridge (stream is void* in Internal::Persist)
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);         // B.3.8 bridge
    ser.Serialize(*reinterpret_cast<int*>(&field50), nullptr);
    int arrayCount = *reinterpret_cast<int*>(&field60);
    ser.Serialize(arrayCount, nullptr);
    // "Am I loading?" is the STREAM's direction (Serializer -> stream +8), not the
    // Serializer's own +8 (m_encoding): see
    // reference_serializer_plus8_is_encoding_not_loading_flag.
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        // B.3.2 bridge: exe 0x1400032e0 IS Collection::Array<void*>::SetSize(n).
        GameColl::AsArray<GameColl::Ptr8>(&field58).SetSize(arrayCount);
    }
    uint8_t* element = static_cast<uint8_t*>(field58);
    for (int64_t remaining = arrayCount; remaining > 0; --remaining, element += 8) {
        reinterpret_cast<Object::InternalBasePtr*>(element)->Serialize(ser);   // B.3.8 bridge (the array is raw bytes)
        void* obj = *reinterpret_cast<void**>(element);
        if (obj != nullptr && !::Object::IsDerivedFromNullSafe(obj, ComponentEntity::TypeId()))
            *reinterpret_cast<uint64_t*>(element) = 0;
    }
    reinterpret_cast<System::Time*>(&field48)->Serialize(ser);   // B.3.8 bridge (field48 is a Time)
}

DEFINE_EXE_TYPE(ImmigrationComponent, "ControllerComponent")
