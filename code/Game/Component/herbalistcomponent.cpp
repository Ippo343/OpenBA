// code/Game/Component/herbalistcomponent.cpp
// Clean reimplementation of HerbalistComponent::Shutdown from Application-x64.exe.
//
// HerbalistComponent (0x50 bytes) is a ResourceProduceComponent subclass that
// manages herbalist buildings. No additional fields beyond ResourceProduceComponent.
//
// Vtable at 0x140159860. Only non-trivial override:
//   Shutdown (0x14004b200) -- calls parent Shutdown + empty serializer block.
//
// Ghidra decompilation:
//   ResourceProduceComponent_Controller_Slot13(param_1, param_2)
//   Serializer(local, param_2, 0); ~Serializer(local)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/herbalistcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>

// ============================================================
// Resolved helpers
// ============================================================

// ResourceProduceComponent::Shutdown (parent) -- at 0x14012b4a0
// This already calls Component_Serialize + empty controller block + serialize produceFlag.
typedef void (*fn_ResourceProduceComponent_Shutdown_t)(void* self, void* stream);
static fn_ResourceProduceComponent_Shutdown_t s_fn_ParentShutdown = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    uintptr_t slot;

    s_fn_ParentShutdown = reinterpret_cast<fn_ResourceProduceComponent_Shutdown_t>(
        ResolveAddr(Module::Exe, 0x14012b4a0));

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// HerbalistComponent::Shutdown (vtable slot [3], 0x14004b200)
// ============================================================
// Calls parent ResourceProduceComponent::Shutdown, then creates
// an empty serializer block (no additional data to serialize).

void HerbalistComponent_Shutdown(HerbalistComponent* self, void* stream) {
    EnsureHelpers();

    // Call parent ResourceProduceComponent::Shutdown
    if (s_fn_ParentShutdown) {
        s_fn_ParentShutdown(self, stream);
    }

    // Empty serializer block (subclass compat)
    alignas(8) uint8_t serializer[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void HerbalistComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[HerbalistComponent] Funcs resolved: ParentShutdown=%p\n",
           (void*)s_fn_ParentShutdown);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// must resolve to a C++ factory here. Produces an inert-but-valid HerbalistComponent
// (size 0x50, +0x20 capability flags 0x80, Object::Internal base + shared stub
// vtable, zeroed body). Size from HerbalistComponent_Factory MemAlloc; flags from the
// Component_Ctor (+0x20=0x80) chain OR'd with the leaf ctor bits.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

// Promoted off COMPONENT_INERT_FACTORY to a live ControllerComponent.
//
// This component backs the GATHERER'S HUT (GathererShelter.rsc carries
// HerbalistDescription — there is no separate GathererComponent) as well as the
// herbalist. While inert, the shared ResourceProduce state machine never ran, so
// slot 18 never minted a HerbalistWork and the hut collected nothing.
//
// Size 0x50 / Flags 0x80 from HerbalistComponent_Factory (0x140006b40): it MemAllocs
// 0x50 and adds no ctor bits beyond the Component_Ctor chain's 0x80 (contrast Hunt,
// which ORs bit 8). NodeOff 0x28 / DefState 2 come from the shared
// ResourceProduceComponent ctor — see fishingcomponent.cpp for the byte derivation.
//
// Retail's HerbalistComponent vtable overrides slot 18 ONLY; 19/20/21 stay the base
// no-ops, so all production happens citizen-side in HerbalistWork.
//
// DEFAULT-ON via the default-on table (graduated 2026-08-17); the env var is now a
// kill-switch (=0). The positive run: seed 12345, demo hut staffed 4/4 and accepting in
// both arms — OFF left the hut parked with GATHERABLE CLIMBING 15 -> 26 (the resources
// were there and nobody was picking them); ON drained it 15 -> 2 and added resource
// types the OFF arm never produced. Full A/B in component_factory_base.cpp.
#include "Component/controller_live_base.h"
#include "Component/controller_state_table.h"

extern "C" void** RPCtrl_NodeVtable();      // resourceproducecomponent_states.cpp
extern "C" void   RPCtrl_PopulateTable();
extern "C" uintptr_t* HerbalistComponent_CreateWork(void*, uintptr_t*);   // slot 18
extern "C" uint8_t ResourceProduceComponent_CommandMask(void*);  // slot 13
extern "C" void    ResourceProduceComponent_Command(void*);      // slot 14

static void HerbalistCtrl_PopulateAndWireHooks();

CONTROLLER_LIVE_FACTORY_STATES(HerbalistComponent, 0x50, 0x80, 0x28, 2,
                               RPCtrl_NodeVtable, HerbalistCtrl_PopulateAndWireHooks)

static void HerbalistCtrl_PopulateAndWireHooks() {
    // From inside InstallLive only — FillTypedVtable erases static-init slot writes.
    HerbalistComponent_LiveVtable[18] =
        reinterpret_cast<void*>(&HerbalistComponent_CreateWork);
    // Slots 13/14 are the panel command path, shared by every
    // ResourceProduceComponent subclass (retail 0x14012b520 / 0x14012b5a0).
    // Without these the dialog harness measured every button on this panel dead:
    // both slots still held the inert stub.
    HerbalistComponent_LiveVtable[13] = reinterpret_cast<void*>(&ResourceProduceComponent_CommandMask);
    HerbalistComponent_LiveVtable[14] = reinterpret_cast<void*>(&ResourceProduceComponent_Command);
    // Slot 3 is Shutdown. The body has been ported in this file since the original
    // wave, but nothing ever wrote it into the live vtable, so the slot-coverage
    // harness measured it as still holding the inert stub — "ported but unwired",
    // the trap this codebase has hit repeatedly. Wire it here, inside InstallLive.
    HerbalistComponent_LiveVtable[3] = reinterpret_cast<void*>(&HerbalistComponent_Shutdown);
    RPCtrl_PopulateTable();
}
REGISTER_TYPE(HerbalistComponent, "ResourceProduceComponent",
              &HerbalistComponent_Factory, &HerbalistComponent_CreateInPlace);
