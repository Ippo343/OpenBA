// code/Game/Component/huntcomponent.cpp
// Clean reimplementation of HuntComponent::Update from Application-x64.exe.
//
// HuntComponent (0x58 bytes) extends ResourceProduceComponent. The Update
// reads the terrain speed ratio from the entity's components, scales the
// delta time by that ratio, and accumulates the result into the hunt
// progress accumulator at +0x50.
//
// Original: 0x140122740
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/huntcomponent_funcs.h"
#include "object/Internal.h"
#include "object/TypeInfo.h"
#include "Component/WorkComponent.h"
#include "Component/WorkPlaceComponent.h"
#include "Entity/ComponentEntity.h"
#include "Tutorial/exe_bodies.h"    // WorkComponent_CalcMaxWorkers
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "system/Timer.h"   // ENDGAME B.3.9 SS2: System::Timer::_secondsPerTick

// ============================================================
// Helpers, called by name (B3R):
//   0x140002e10 FindComponent<WorkPlaceComponent>
//   0x140003050 FindComponent<WorkComponent>
//   0x14012e5c0 WorkComponent_CalcMaxWorkers — the description-derived, footprint-scaled
//     worker capacity, cached at workComp+0x30. HuntComponent::Update reuses that same cache
//     as its speed divisor (the old name here was "ComputeTerrainSpeed"). The single body
//     lives in code/Game/Tutorial/tutorial_base.cpp.
// ============================================================

// ============================================================
// HuntComponent::Shutdown — vtable slot 3, retail 0x140122820
// ============================================================
// Retail:
//   ResourceProduceComponent::Shutdown(self, stream)    (0x14012b4a0, a direct call)
//   Serializer s(stream, 0)
//   System::Time::Serialize(self+0x50, s)               — the hunt-progress accumulator
//   ~Serializer()
//
// +0x50 is the 8-byte Time the slot-9 Update advances, so without this the lodge's
// accumulated hunt progress is silently dropped across a save/load.
void HuntComponent::Persist(IO::Stream& stream) {
    ResourceProduceComponent::Persist(stream);

    IO::Serializer ser(stream, 0);
    huntProgress.Serialize(ser);
}

// NOTE on slot 9: HuntComponent::Update is implemented above (0x140122740). Hunt is
// the only ResourceProduceComponent subclass that overrides slot 9, and its ctor
// (0x1401228e0) ORs capability bit 8 into +0x20 precisely to opt into per-frame
// update ticking; Fishing and Herbalist do neither. The slot receives the Time by
// VALUE (an 8-byte tick count in a register): the base declares that register as
// `void*`, so its bits are the tick count.
void HuntComponent::Update(void* time) {
    const int64_t deltaTime = static_cast<int64_t>(reinterpret_cast<intptr_t>(time));   // LEFT: SIG Component::Update (Component.h): the elapsed ticks travel in the pointer slot (see above)

    // The owner's workplace: its worker count is the speed numerator
    WorkPlaceComponent* workplace = _entity->FindComponent<WorkPlaceComponent>();   // 0x140002e10
    int workerCount = workplace->workerCount;

    // The work component of the workplace's owner: its cached worker capacity
    WorkComponent* work = workplace->_entity->FindComponent<WorkComponent>();   // 0x140003050

    // Lazily compute the capacity divisor if not cached
    int capacity = work->workState;
    if (capacity == 0) {
        capacity = WorkComponent_CalcMaxWorkers(reinterpret_cast<uintptr_t>(work));   // LEFT: SIG WorkComponent_CalcMaxWorkers (Tutorial/exe_bodies.h, wave X)
        work->workState = capacity;
    }

    // Convert delta time to seconds (inline Time::AsSeconds)
    float deltaSeconds = static_cast<float>(
        static_cast<double>(deltaTime) * ::System::Timer::_secondsPerTick);

    // Scale by the staffing ratio and convert back to ticks
    float scaledSeconds = deltaSeconds * (static_cast<float>(workerCount) /
                                           static_cast<float>(capacity));
    int64_t scaledTicks = static_cast<int64_t>(
        static_cast<double>(scaledSeconds) / ::System::Timer::_secondsPerTick);

    // Accumulate into the hunt progress
    huntProgress._ticks += scaledTicks;
}

// ============================================================================
// ENDGAME B.3.8 MI-B — HuntComponent as a REAL class (HuntComponent.h).
//
// Before: CONTROLLER_LIVE_FACTORY_STATES (48-slot table filled per create, slots
// 3/9/13/14/18 written from InstallLive, Inert_ScalarDtor in slot 0, the shared RP
// dispatcher stamped at +0x28, flags 0x88 from the factory). After: the compiler's
// 22-slot vtable (13/14/16/17 and 19-21 inherited from ResourceProduceComponent), a
// real ctor/dtor chain, the controller the base's member (B38_RULES D1, D2, D6).
// ============================================================================
#include "Object/factory_registration.h"

extern "C" uintptr_t* HuntComponent_CreateWork(void*, uintptr_t*);   // resourceproduce_creatework.cpp

// 0x1401228e0: the RP ctor (CALL 0x14012b3d0), the compiler's stamp,
// `MOV qword [RBX+0x50],0`, then `_flags |= 8` (0x80 | 8 = 0x88).
HuntComponent::HuntComponent() : ResourceProduceComponent(), huntProgress{0} {
    OrFlagsInCtor(0x8);
}

// create 0x140034f30: MemAlloc(0x58, 8), then the ctor.
void* HuntComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HuntComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HuntComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* HuntComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HuntComponent());
}

uintptr_t* HuntComponent::CreateWork(uintptr_t* out) { return HuntComponent_CreateWork(this, out); }  // [18] 0x1401226a0

DEFINE_EXE_TYPE(HuntComponent, "ResourceProduceComponent")
