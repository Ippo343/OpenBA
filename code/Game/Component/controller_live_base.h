#pragma once
// code/Game/Component/controller_live_base.h
// Phase 3 / Keystone D step 2 — ControllerComponent live bring-up for the
// standalone exe.
//
// A ControllerComponent (the 22 building-behaviour components: BuildComponent,
// StorageLocationComponent, WorkPlaceComponent, ClearAreaComponent, …) carries
// an *embedded state node* inside itself. The StateMachineComponent on the same
// entity links these nodes into its state-node chain and ticks them every frame.
//
// The linking contract (recovered from StateMachineComponent_Startup 0x14012b6a0):
// for each owner component the SM calls two virtuals through the controller's
// PRIMARY vtable:
//   * GetDefaultState  (vtable +0x80 / slot 16) -> a small int (default state id),
//     stored into the SM's stateFlags byte at +0x52.
//   * CreateState      (vtable +0x88 / slot 17) -> pointer to the embedded state
//     node (the original is just `LEA RAX,[RCX+nodeOff]; RET`, no allocation).
// The returned node is then linked: node+0x08 = parent, prev+0x10 = node.
//
// The embedded node's own vtable (node+0x00) is the controller's *secondary*
// vtable and implements the state-node interface the SM's Tick walks:
//   slot 0   (+0x00) Step(input)->int        (orig: searches a global
//                                              state-type table -> index / -1)
//   slot 3   (+0x18) Enter(sm)->char
//   slot 4   (+0x20) Update(sm)->char
//   slot 5   (+0x28) OnState(sm,a,b)->void
//   slot 6   (+0x30) Transition(sm,time)->char
//   slot 7   (+0x38) Transition(sm,time)->char
//
// In standalone the original Step would read the empty state-type table, return
// -1, and drive Tick's parent-walk into a null deref. So this brings the node up
// CRASH-SAFE: a shared node vtable whose Step returns 0 (>=0, so Tick never
// parent-walks) and whose Enter/Update/Transition return 1 (non-zero, so Tick's
// fall-through loops exit immediately). That exercises the SM's *calling
// mechanism* end-to-end over a real linked node without needing the full
// state-type registration — the deep per-controller state behaviour is deferred.
//
// Default (env off) => the component stays inert (slots 16/17 not installed, no
// node vtable written) => byte-for-byte unchanged run, exactly like
// COMPONENT_INERT_FACTORY.

#include <cstdint>
#include <cstddef>

namespace ControllerLive {

// Shared, crash-safe state-node vtable (see file header). Filled once on first
// call and reused by every live controller's embedded node.
void** NodeVtable();

}  // namespace ControllerLive

// CONTROLLER_LIVE_FACTORY — like COMPONENT_INERT_FACTORY, but when EnvVar (or the
// global BANISHED_LIVE_COMPONENTS) is set it graduates the per-type vtable's
// GetDefaultState (slot 16) and CreateState (slot 17) to real implementations and
// installs the shared crash-safe node vtable into the embedded node at
// `self + NodeOff`, so the entity's StateMachineComponent can link and tick it.
//
//   Size      = the _Factory MemAlloc size (component byte size)
//   Flags     = the _Ctor `OR EAX,0xNN` capability flags written to +0x20
//   NodeOff   = embedded state-node offset (CreateState's `LEA RAX,[RCX+NodeOff]`)
//   DefState  = GetDefaultState's constant return (the default state id)
//   EnvVar    = per-component opt-in env var
//
// Requires "Object/factory_registration.h" and "Component/component_factory_base.h".
#define CONTROLLER_LIVE_FACTORY(Name, Size, Flags, NodeOff, DefState, EnvVar)    \
    static uint32_t Name##_LiveGetTypeId(void*) {                               \
        static uint32_t cached = 0;                                             \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);             \
        return cached;                                                          \
    }                                                                           \
    static int Name##_GetDefaultState(void*) { return (DefState); }            \
    static void* Name##_CreateState(void* self) {                              \
        return reinterpret_cast<uint8_t*>(self) + (NodeOff);                    \
    }                                                                           \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];      \
    static void Name##_InstallLive(void* mem) {                                \
        if (!::ComponentFactory::LiveEnabled(EnvVar)) return;                  \
        Name##_LiveVtable[16] = reinterpret_cast<void*>(&Name##_GetDefaultState); \
        Name##_LiveVtable[17] = reinterpret_cast<void*>(&Name##_CreateState);  \
        if (mem)                                                               \
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + (NodeOff)) \
                = reinterpret_cast<void*>(::ControllerLive::NodeVtable());      \
    }                                                                           \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),         \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_InstallLive(p);                                                 \
        return p;                                                             \
    }                                                                           \
    static void* Name##_CreateInPlace(void* mem) {                            \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),     \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_InstallLive(mem);                                               \
        return mem;                                                           \
    }
