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

#include <cstdint>
#include <cstddef>

namespace ControllerLive {

// Shared, crash-safe state-node vtable (see file header). Filled once on first
// call and reused by every live controller's embedded node.
void** NodeVtable();

}  // namespace ControllerLive

// CONTROLLER_LIVE_FACTORY — like COMPONENT_INERT_FACTORY, but it graduates the
// per-type vtable's GetDefaultState (slot 16) and CreateState (slot 17) to real
// implementations and
// installs the shared crash-safe node vtable into the embedded node at
// `self + NodeOff`, so the entity's StateMachineComponent can link and tick it.
//
//   Size      = the _Factory MemAlloc size (component byte size)
//   Flags     = the _Ctor `OR EAX,0xNN` capability flags written to +0x20
//   NodeOff   = embedded state-node offset (CreateState's `LEA RAX,[RCX+NodeOff]`)
//   DefState  = GetDefaultState's constant return (the default state id)
//
// Requires "Object/factory_registration.h" and "Component/component_factory_base.h".
#define CONTROLLER_LIVE_FACTORY(Name, Size, Flags, NodeOff, DefState)            \
    static uint32_t Name##_LiveGetTypeId(void*) {                               \
        static uint32_t cached = 0;                                             \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);             \
        return cached;                                                          \
    }                                                                           \
    static int Name##_GetDefaultState(void*) { return (DefState); }            \
    static void* Name##_CreateState(void* self) {                              \
        return reinterpret_cast<uint8_t*>(self) + (NodeOff);                    \
    }                                                                           \
    /* Slot 1 = GetTypeInfo. Retail returns a per-class .rdata global sitting 8  \
       bytes after the type-id global (FishingComponent: id 0x1401c7660, info    \
       0x1401c7668). Our BaseTypeInfoStorage IS that object, so its address is   \
       the faithful answer. This was the ONE slot the coverage harness found     \
       stubbed on all six food classes, so it is filled here once, in the shared \
       macro, rather than six times over. */                                     \
    static void* Name##_LiveGetTypeInfo(void*) {                               \
        static void* cached = nullptr;                                         \
        if (!cached) cached = ::Object::GetTypeInfoByName(#Name);              \
        return cached;                                                         \
    }                                                                           \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];      \
    static void Name##_InstallLive(void* mem) {                                \
        Name##_LiveVtable[1]  = reinterpret_cast<void*>(&Name##_LiveGetTypeInfo); \
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

// CONTROLLER_LIVE_FACTORY_STATES — as CONTROLLER_LIVE_FACTORY, but installs the
// REAL per-controller state-type-table dispatcher node vtable instead of the shared
// crash-safe stub, and populates that controller's state table (Phase 3 step 3).
//   NodeVtableFn : `void**(void)` returning the controller's dispatcher vtable
//                  (e.g. ControllerLive::Dispatcher<Tag>::Vtable()).
//   PopulateFn   : `void(void)` that fills the controller's StateTable once
//                  (idempotent — replicates <X>Description_Setup's sub_f500/f620).
// Requires "Object/factory_registration.h", "Component/component_factory_base.h"
// and "Component/controller_state_table.h".
#define CONTROLLER_LIVE_FACTORY_STATES(Name, Size, Flags, NodeOff, DefState,     \
                                       NodeVtableFn, PopulateFn)                 \
    static uint32_t Name##_LiveGetTypeId(void*) {                               \
        static uint32_t cached = 0;                                             \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);             \
        return cached;                                                          \
    }                                                                           \
    static int Name##_GetDefaultState(void*) { return (DefState); }            \
    static void* Name##_CreateState(void* self) {                              \
        return reinterpret_cast<uint8_t*>(self) + (NodeOff);                    \
    }                                                                           \
    /* Slot 1 = GetTypeInfo. Retail returns a per-class .rdata global sitting 8  \
       bytes after the type-id global (FishingComponent: id 0x1401c7660, info    \
       0x1401c7668). Our BaseTypeInfoStorage IS that object, so its address is   \
       the faithful answer. This was the ONE slot the coverage harness found     \
       stubbed on all six food classes, so it is filled here once, in the shared \
       macro, rather than six times over. */                                     \
    static void* Name##_LiveGetTypeInfo(void*) {                               \
        static void* cached = nullptr;                                         \
        if (!cached) cached = ::Object::GetTypeInfoByName(#Name);              \
        return cached;                                                         \
    }                                                                           \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];      \
    static void Name##_InstallLive(void* mem) {                                \
        Name##_LiveVtable[1]  = reinterpret_cast<void*>(&Name##_LiveGetTypeInfo); \
        Name##_LiveVtable[16] = reinterpret_cast<void*>(&Name##_GetDefaultState); \
        Name##_LiveVtable[17] = reinterpret_cast<void*>(&Name##_CreateState);  \
        (PopulateFn)();                                                        \
        if (mem) {                                                             \
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + (NodeOff)) \
                = reinterpret_cast<void*>((NodeVtableFn)());                    \
            /* node+0x18 = the owning component (the real ctor sets this via   \
               selfPtr @ comp+NodeOff+0x18). The exe enter/update/transition   \
               dispatcher tail-calls `MOV RCX,[node+0x18]; JMP handler`, so a  \
               real handler needs this back-pointer to receive its component. */\
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(mem) + (NodeOff) + 0x18) \
                = mem;                                                         \
        }                                                                      \
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

// ---------------------------------------------------------------------------
// CONTROLLER_INSTALL_SERIALIZE — wire a controller's OWN stream Serialize into
// vtable slot 3 (+0x18).
//
// Neither CONTROLLER_LIVE_FACTORY nor CONTROLLER_LIVE_FACTORY_STATES touches
// slot 3, so a controller that has a ported Serialize body in its own file still
// gets the generic BASE-ONLY Component::Serialize from the inert fill. That is
// not a missing feature, it is a stream desync: retail's body reads the base
// record PLUS its own Serializer scopes, and an "empty" scope still occupies 16
// bytes in the save encoding. Reading fewer bytes than the file holds corrupts
// every object deserialized after it, and our own saves cannot catch it because
// the save leg skips exactly the same bytes — only a retail-authored save does.
// (Found via the packaged tutorial save: StorageLocationComponent's record is 82
// bytes and the base-only fill consumed 50.)
//
// Serializer wiring is deliberately NOT behind a live/env gate — see the same
// note on WorkComponent's slot 3.
//
// Use directly after the factory macro, and register the _Ser entry points:
//   CONTROLLER_INSTALL_SERIALIZE(FooComponent, FooComponent_Shutdown)
//   REGISTER_TYPE(FooComponent, "ControllerComponent",
//                 &FooComponent_Factory_Ser, &FooComponent_CreateInPlace_Ser);
// ---------------------------------------------------------------------------
#define CONTROLLER_INSTALL_SERIALIZE(Name, SerBody)                              \
    static void Name##_SerSlot3(void* s, void* stream) {                        \
        SerBody(reinterpret_cast<Name*>(s), stream);                            \
    }                                                                           \
    extern "C" void* Name##_Factory_Ser(void) {                                 \
        void* p = Name##_Factory();                                             \
        Name##_LiveVtable[3] = reinterpret_cast<void*>(&Name##_SerSlot3);       \
        return p;                                                               \
    }                                                                           \
    static void* Name##_CreateInPlace_Ser(void* mem) {                          \
        void* p = Name##_CreateInPlace(mem);                                    \
        Name##_LiveVtable[3] = reinterpret_cast<void*>(&Name##_SerSlot3);       \
        return p;                                                               \
    }
