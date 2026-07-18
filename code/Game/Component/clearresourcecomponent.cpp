// code/Game/Component/clearresourcecomponent.cpp
// Clean reimplementation of ClearResourceComponent lifecycle methods.
//
// ClearResourceComponent (0x48 bytes) -- ControllerComponent subclass with no extra fields.
// Handles clearing resources from tiles during construction.
//
// Vtable at 0x140162840. Non-trivial override:
//   Shutdown (0x1401185E0) -- Component_Serialize + two empty serializer blocks
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/clearresourcecomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

static bool s_initialized = false;

// ============================================================
// ClearResourceComponent::Shutdown (vtable slot [3], 0x1401185E0)
// ============================================================
// Same pattern as AbsorbResourceComponent: Component_Serialize + two empty blocks.
void ClearResourceComponent_Shutdown(ClearResourceComponent* self, void* stream) {
    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // First empty serializer block (version 0)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Second empty serializer block (version 0)
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void ClearResourceComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    s_initialized = true;

#ifdef _DEBUG
    printf("[ClearResourceComponent] Funcs resolved: CompSerialize=%p Ctor=%p Dtor=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_Serializer_Ctor,
           (void*)s_fn_Serializer_Dtor);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
#include "Component/controller_live_base.h"

// The clear-job's self-destruct controller — this is what clears the clearing ICON
// once the resource is harvested (the DecalComponent's terrain decal is released by
// the +0x38 detach that entity removal dispatches). State table + the state-2 enter
// handler (a port of 0x140118560) live in clearresourcecomponent_states.cpp.
//
// Shape recovered from the exe (ClearResourceComponent_Ctor 0x1401186a0 + the
// primary vtable at 0x140162840):
//   size 0x48 / flags 0x80 (plain ControllerComponent — the SM ticks the node, so
//     no self-tick bit)
//   node at +0x28 (slot 17 CreateState 0x140004080 = `LEA RAX,[RCX+0x28]; RET`),
//     back-pointer node+0x18 == comp+0x40 (the ctor's `*(param_1+0x40) = param_1`)
//   default state 2 (slot 16 GetDefaultState 0x1400026c0 = `MOV EAX,2; RET`)
//
// ⚠️ The default state of 2 is faithful but must never be the one the SM adopts —
// state 2 is the destroy. It is safe only because StateMachineComponent_Startup
// (0x14012b6a0) reads GetDefaultState from the FIRST ControllerComponent only, and
// `cleararea` (default 0) precedes `clearresource` in every template that uses this
// component (Template/Clear.rsc, Template/ClearR.rsc — both verified). That is also
// why the env gate below is BANISHED_LIVE_CLEARCTRL and not a var of its own: with
// `cleararea` inert, this component would be the first LIVE controller our
// standalone linker sees and every clear-job would delete itself on creation.
extern "C" void** ClearResourceCtrl_NodeVtable();
extern "C" void   ClearResourceCtrl_PopulateTable();

CONTROLLER_LIVE_FACTORY_STATES(ClearResourceComponent, 0x48, 0x80, 0x28, 2,
                               "BANISHED_LIVE_CLEARCTRL",
                               ClearResourceCtrl_NodeVtable,
                               ClearResourceCtrl_PopulateTable)
REGISTER_TYPE(ClearResourceComponent, "ControllerComponent",
              &ClearResourceComponent_Factory, &ClearResourceComponent_CreateInPlace);
