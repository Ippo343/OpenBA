// code/Game/Component/fishingcomponent.cpp
// Clean reimplementation of FishingComponent lifecycle methods from Application-x64.exe.
//
// FishingComponent (0x58 bytes) extends ResourceProduceComponent with:
//   +0x50: uint8  fishingFlag  (serialized as bool)
//   +0x54: float  fishingRate  (serialized)
//
// Vtable at 0x140163230. One non-trivial hookable override:
//   Shutdown (0x14011f050) — ResourceProduceComponent::Shutdown + own serializer
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/fishingcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent::Shutdown (base) at 0x14012b4a0
typedef void (*fn_ResourceProduceShutdown_t)(void* self, void* stream);
static fn_ResourceProduceShutdown_t s_fn_ResourceProduceShutdown = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IO::Serializer::Serialize(this, float*, name)
typedef void (*fn_Serializer_SerializeFloat_t)(void* self, float* value, const char* name);
static fn_Serializer_SerializeFloat_t s_fn_Serializer_SerializeFloat = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_ResourceProduceShutdown = reinterpret_cast<fn_ResourceProduceShutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a60);
    if (slot) s_fn_Serializer_SerializeFloat = *reinterpret_cast<fn_Serializer_SerializeFloat_t*>(slot);

    s_initialized = true;
}

// ============================================================
// FishingComponent::Shutdown (vtable slot [3], 0x14011f050)
// ============================================================
// ResourceProduceComponent::Shutdown(self, stream)
// Serializer s(stream, 0)
// s.Serialize(&self[0x50], null)  // bool: fishingFlag
// s.Serialize(&self[0x54], null)  // float: fishingRate
// ~Serializer()

void FishingComponent_Shutdown(FishingComponent* self, void* stream) {
    EnsureHelpers();

    // Call base ResourceProduceComponent::Shutdown
    if (s_fn_ResourceProduceShutdown) {
        s_fn_ResourceProduceShutdown(self, stream);
    }

    // Serialize FishingComponent's own fields
    alignas(8) uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    }

    if (s_fn_Serializer_SerializeBool) {
        bool* pFishingFlag = reinterpret_cast<bool*>(&self->fishingFlag);
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*pFishingFlag, nullptr);   // B.3.4 bridge
    }

    if (s_fn_Serializer_SerializeFloat) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->fishingRate, nullptr);   // B.3.4 bridge
    }

    if (s_fn_Serializer_Dtor) {
        reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void FishingComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[FishingComponent] Funcs resolved: ResourceProduceShutdown=%p "
           "Serializer_Ctor=%p SerializeBool=%p SerializeFloat=%p\n",
           (void*)s_fn_ResourceProduceShutdown, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_SerializeBool, (void*)s_fn_Serializer_SerializeFloat);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
//
// Promoted off COMPONENT_INERT_FACTORY to a live ControllerComponent. While inert,
// the embedded state node at comp+0x28 was never installed and never linked into the
// entity's StateMachineComponent, so the shared ResourceProduce state machine never
// ran: state 13 was never entered, slot 18 (CreateWork) was never called, and no
// FishingWork was ever posted to the reservation ledger. FishingWork itself has been
// fully ported and vtable-registered for some time — it simply had no producer. That
// is the whole "fishing dock does nothing" symptom.
//
// Macro arguments, byte-verified against retail rather than inferred:
//   Size 0x58  — FishingComponent_Factory (0x140034350) MemAllocs 0x58.
//   Flags 0x80 — the Component_Ctor chain ORs 0x80 into +0x20; the Fishing factory
//                adds no further bits (contrast HuntComponent, which ORs bit 8).
//   NodeOff 0x28 — ResourceProduceComponent_Ctor (0x14012b3d0) writes
//                vtable_ResourceProduceComponent_Controller to +0x28 and self to
//                +0x40 (= NodeOff+0x18); primary slot 17 (CreateState) is the shared
//                0x140004080 = `LEA RAX,[RCX+0x28]; RET`.
//   DefState 2 — primary slot 16 (GetDefaultState) is the shared 0x1400026c0 =
//                `MOV EAX,2; RET`.
//
// DEFAULT-ON via the default-on table (graduated 2026-08-17); the env var is now a
// kill-switch (=0). The positive-outcome run the old note asked for: at seed 12345 with
// the demo dock staffed 4/4 in both arms, OFF left registered(+0x50)=0 rate(+0x54)=0.000
// and no fish resource type in town storage at all; ON gave registered=1 rate=1.444 and
// the fish type climbing 161 -> 374 over 6000 frames. A clean run still proves nothing
// here, which is why the assertion is the rate and the fish count, not the absence of a
// crash. See the block in component_factory_base.cpp for the full A/B.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uintptr_t* FishingComponent_CreateWork(void*, uintptr_t*);  // slot 18
extern "C" uint8_t ResourceProduceComponent_CommandMask(void*);  // slot 13
extern "C" void    ResourceProduceComponent_Command(void*);      // slot 14

// Fishing is the only subclass in this wave that overrides the whole production-hook
// block: 19/20 (enter/exit producing) and 21 (the per-frame rate tick).
extern "C" char  FishingComponent_OnEnterProducing(void* self);   // 0x14011ee80
extern "C" char  FishingComponent_OnExitProducing(void* self);    // 0x14011ee50
extern "C" char  FishingComponent_ProduceTick(void* self, void* time);  // 0x14011ec10

// Slots 18..21 must be written from INSIDE InstallLive: MakeInertTyped ->
// FillTypedVtable rewrites slots 1..N with the inert stub on the first factory call,
// so anything written at static-init is silently erased. The macro calls PopulateFn
// from InstallLive AFTER FillTypedVtable has run, which is the one surviving place.
static void FishingCtrl_PopulateAndWireHooks();

CONTROLLER_LIVE_FACTORY_STATES(FishingComponent, 0x58, 0x80, 0x28, 2,
                               RPCtrl_NodeVtable, FishingCtrl_PopulateAndWireHooks)

static void FishingCtrl_PopulateAndWireHooks() {
    FishingComponent_LiveVtable[18] = reinterpret_cast<void*>(&FishingComponent_CreateWork);
    // Slots 13/14 are the panel command path, shared by every
    // ResourceProduceComponent subclass (retail 0x14012b520 / 0x14012b5a0).
    // Without these the dialog harness measured every button on this panel dead:
    // both slots still held the inert stub.
    FishingComponent_LiveVtable[13] = reinterpret_cast<void*>(&ResourceProduceComponent_CommandMask);
    FishingComponent_LiveVtable[14] = reinterpret_cast<void*>(&ResourceProduceComponent_Command);
    // Slot 3 is Shutdown. The body has been ported in this file since the original
    // wave, but nothing ever wrote it into the live vtable, so the slot-coverage
    // harness measured it as still holding the inert stub — "ported but unwired",
    // the trap this codebase has hit repeatedly. Wire it here, inside InstallLive.
    FishingComponent_LiveVtable[3] = reinterpret_cast<void*>(&FishingComponent_Shutdown);
    FishingComponent_LiveVtable[19] = reinterpret_cast<void*>(&FishingComponent_OnEnterProducing);
    FishingComponent_LiveVtable[20] = reinterpret_cast<void*>(&FishingComponent_OnExitProducing);
    FishingComponent_LiveVtable[21] = reinterpret_cast<void*>(&FishingComponent_ProduceTick);
    RPCtrl_PopulateTable();
}
REGISTER_TYPE(FishingComponent, "ResourceProduceComponent",
              &FishingComponent_Factory, &FishingComponent_CreateInPlace);
