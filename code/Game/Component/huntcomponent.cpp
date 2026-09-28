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
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "system/Timer.h"   // ENDGAME B.3.9 SS2: System::Timer::_secondsPerTick

// ============================================================
// Resolved helpers
// ============================================================

// FUN_140002e10: Find component on entity by type (terrain/character component)
typedef void* (*fn_FindComponent1_t)(int64_t entity, void* outPtr);
static fn_FindComponent1_t s_fn_FindComponent1 = nullptr;

// FUN_140003050: Find component on entity by type (map tile component)
typedef void* (*fn_FindComponent2_t)(int64_t entity, void* outPtr);
static fn_FindComponent2_t s_fn_FindComponent2 = nullptr;

// FUN_14012e5c0: WorkComponent::CalcMaxWorkers — the description-derived, footprint-scaled
// worker capacity, cached at workComp+0x30. HuntComponent::Update reuses that same cache as
// its speed divisor, which is why the old name here was "ComputeTerrainSpeed". The single
// body lives in code/Game/Tutorial/tutorial_base.cpp (WorkComponent_CalcMaxWorkers) and is
// registered ONCE, from standalone/reg_tutorial_base.cpp. A second, identically-behaving
// copy used to be registered from reg_component_nature.cpp; whichever batch ran last won.
typedef int (*fn_CalcMaxWorkers_t)(int64_t workComp);
static fn_CalcMaxWorkers_t s_fn_CalcMaxWorkers = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_FindComponent1 = reinterpret_cast<fn_FindComponent1_t>(
        ResolveAddr(Module::Exe, 0x140002e10));

    s_fn_FindComponent2 = reinterpret_cast<fn_FindComponent2_t>(
        ResolveAddr(Module::Exe, 0x140003050));

    s_fn_CalcMaxWorkers = reinterpret_cast<fn_CalcMaxWorkers_t>(
        ResolveAddr(Module::Exe, 0x14012e5c0));

    s_initialized = true;
}

// ============================================================
// HuntComponent::Update (vtable slot [9], 0x140122740)
// ============================================================
// Computes hunt progress: accumulates delta time scaled by the
// terrain speed ratio (base speed / terrain speed).

// ============================================================
// Init: resolve function pointers
// ============================================================
void HuntComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HuntComponent] Funcs resolved: FindComponent1=%p FindComponent2=%p "
           "CalcMaxWorkers=%p\n",
           (void*)s_fn_FindComponent1, (void*)s_fn_FindComponent2,
           (void*)s_fn_CalcMaxWorkers);
#endif
}

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
void HuntComponent::Persist(void* stream) {
    ResourceProduceComponent::Persist(stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(&huntData)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// NOTE on slot 9: HuntComponent::Update is implemented above (0x140122740). Hunt is
// the only ResourceProduceComponent subclass that overrides slot 9, and its ctor
// (0x1401228e0) ORs capability bit 8 into +0x20 precisely to opt into per-frame
// update ticking; Fishing and Herbalist do neither. The slot receives the Time by
// VALUE (an 8-byte tick count in a register): the base declares that register as
// `void*`, so its bits are the tick count.
void HuntComponent::Update(void* time) {
    const int64_t deltaTime = static_cast<int64_t>(reinterpret_cast<intptr_t>(time));   // the elapsed ticks travel in the pointer slot
    EnsureHelpers();

    if (!s_fn_FindComponent1 || !s_fn_FindComponent2 || !s_fn_CalcMaxWorkers)
        return;

    // Find the terrain/character component on our owner entity
    int64_t entityPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(this) + 0x10);

    int64_t terrainComponent = 0;
    s_fn_FindComponent1(entityPtr, &terrainComponent);

    // Read terrain base speed from component+0x60
    int terrainBaseSpeed = *reinterpret_cast<int*>(terrainComponent + 0x60);

    // Find the map tile component via terrainComponent's owner
    int64_t tileOwner = *reinterpret_cast<int64_t*>(terrainComponent + 0x10);
    int64_t tileComponentPtr = 0;
    void* result = s_fn_FindComponent2(tileOwner, &tileComponentPtr);

    // Dereference to get actual tile data
    int64_t tileData = *reinterpret_cast<int64_t*>(result);

    // Lazily compute the capacity divisor if not cached
    int terrainSpeed = *reinterpret_cast<int*>(tileData + 0x30);
    if (terrainSpeed == 0) {
        terrainSpeed = s_fn_CalcMaxWorkers(tileData);
        *reinterpret_cast<int*>(tileData + 0x30) = terrainSpeed;
    }

    // Convert delta time to seconds (inline Time::AsSeconds)
    float deltaSeconds = static_cast<float>(
        static_cast<double>(deltaTime) * ::System::Timer::_secondsPerTick);

    // Scale by terrain speed ratio and convert back to ticks
    float scaledSeconds = deltaSeconds * (static_cast<float>(terrainBaseSpeed) /
                                           static_cast<float>(terrainSpeed));
    int64_t scaledTicks = static_cast<int64_t>(
        static_cast<double>(scaledSeconds) / ::System::Timer::_secondsPerTick);

    // Accumulate into hunt progress at +0x50
    *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(this) + 0x50) += scaledTicks;
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
HuntComponent::HuntComponent() : ResourceProduceComponent(), huntData(nullptr) {
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
