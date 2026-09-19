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

void HuntComponent_Update(HuntComponent* self, int64_t deltaTime) {
    EnsureHelpers();

    if (!s_fn_FindComponent1 || !s_fn_FindComponent2 || !s_fn_CalcMaxWorkers ||
        !g_Timer_secondsPerTick)
        return;

    // Find the terrain/character component on our owner entity
    int64_t entityPtr = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

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
        static_cast<double>(deltaTime) * (*g_Timer_secondsPerTick));

    // Scale by terrain speed ratio and convert back to ticks
    float scaledSeconds = deltaSeconds * (static_cast<float>(terrainBaseSpeed) /
                                           static_cast<float>(terrainSpeed));
    int64_t scaledTicks = static_cast<int64_t>(
        static_cast<double>(scaledSeconds) / (*g_Timer_secondsPerTick));

    // Accumulate into hunt progress at +0x50
    *reinterpret_cast<int64_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x50) += scaledTicks;
}

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
//   ResourceProduceComponent::Shutdown(self, stream)    (0x14012b4a0)
//   Serializer s(stream, 0)
//   System::Time::Serialize(self+0x50, s)               — the hunt-progress accumulator
//   ~Serializer()
//
// +0x50 is the 8-byte Time the slot-9 Update advances, so without this the lodge's
// accumulated hunt progress is silently dropped across a save/load. It was the last
// slot on HuntComponent still holding the inert stub.
extern "C" void HuntComponent_ShutdownVSlot(void* selfP, void* stream) {
    static void  (*s_parentShutdown)(void*, void*) = nullptr;
    static bool s_res = false;
    if (!s_res) {
        s_parentShutdown = reinterpret_cast<void(*)(void*, void*)>(
            ResolveAddr(Module::Exe, 0x14012b4a0));
        s_res = true;
    }
    if (s_parentShutdown) s_parentShutdown(selfP, stream);

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<System::Time*>(reinterpret_cast<uint8_t*>(selfP) + 0x50)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// NOTE on slot 9: HuntComponent::Update is already implemented above
// (0x140122740). Hunt is the only ResourceProduceComponent subclass that overrides
// slot 9, and its ctor (0x1401228e0) ORs capability bit 8 into +0x20 precisely to opt
// into per-frame update ticking; Fishing and Herbalist do neither. The slot receives
// the Time by VALUE (an 8-byte tick count in a register), which is why the existing
// signature takes int64_t rather than a Time*.

// ===== Standalone factory (Foundation D — component instantiation) =====
//
// Promoted off COMPONENT_INERT_FACTORY to a live ControllerComponent, so the shared
// ResourceProduce state machine (resourceproducecomponent_states.cpp) actually runs
// and slot 18 mints + posts a HuntWork. HuntWork itself was already fully ported and
// vtable-registered — it just had no producer while the component stayed inert.
//
// Flags 0x88, not 0x80: HuntComponent_Ctor (0x1401228e0) ORs bit 8 into +0x20 on top
// of the Component_Ctor chain's 0x80, which is what opts this component into the
// per-frame Update above. Size 0x58 and NodeOff 0x28 / DefState 2 come from the shared
// ResourceProduceComponent ctor — see fishingcomponent.cpp for the byte derivation.
//
// DEFAULT-ON via the default-on table (graduated 2026-08-17); the env var is now a
// kill-switch (=0). The positive run: seed 12345, demo lodge staffed 3/3 and accepting
// in both arms — OFF held progress(+0x50) at exactly 0 for 7200 frames and town storage
// at 7 resource types; ON ticked progress 0 -> 9.57e16 and storage reached 11 types,
// four of which the OFF arm never had. Full A/B in component_factory_base.cpp.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uintptr_t* HuntComponent_CreateWork(void*, uintptr_t*);   // slot 18
extern "C" uint8_t ResourceProduceComponent_CommandMask(void*);  // slot 13
extern "C" void    ResourceProduceComponent_Command(void*);      // slot 14

// Slots must be written from inside InstallLive — FillTypedVtable erases anything
// written at static-init on the first factory call.
static void HuntCtrl_PopulateAndWireHooks();

CONTROLLER_LIVE_FACTORY_STATES(HuntComponent, 0x58, 0x88, 0x28, 2,
                               RPCtrl_NodeVtable, HuntCtrl_PopulateAndWireHooks)

static void HuntCtrl_PopulateAndWireHooks() {
    HuntComponent_LiveVtable[3]  = reinterpret_cast<void*>(&HuntComponent_ShutdownVSlot);
    HuntComponent_LiveVtable[9]  = reinterpret_cast<void*>(&HuntComponent_Update);
    HuntComponent_LiveVtable[18] = reinterpret_cast<void*>(&HuntComponent_CreateWork);
    // Slots 13/14 are the panel command path, shared by every
    // ResourceProduceComponent subclass (retail 0x14012b520 / 0x14012b5a0).
    // Without these the dialog harness measured every button on this panel dead:
    // both slots still held the inert stub.
    HuntComponent_LiveVtable[13] = reinterpret_cast<void*>(&ResourceProduceComponent_CommandMask);
    HuntComponent_LiveVtable[14] = reinterpret_cast<void*>(&ResourceProduceComponent_Command);
    // 19/20/21 deliberately left as the base no-ops: retail's HuntComponent vtable
    // keeps 0x140006b10 in all three.
    RPCtrl_PopulateTable();
}
REGISTER_TYPE(HuntComponent, "ResourceProduceComponent",
              &HuntComponent_Factory, &HuntComponent_CreateInPlace);
