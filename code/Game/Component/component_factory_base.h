#pragma once
// Shared inert-component factory infrastructure for the standalone exe
// (Foundation D — component instantiation cascade).
//
// In standalone the original exe is not mapped, so Object::Factory::CreateObject
// (called by ComponentEntity_CtorFromDescription while instantiating an entity
// from its template) must resolve to a C++ factory. Without one the component
// type stays NullFactory and CtorFromDescription gets a null component — the
// entity spawns but is empty.
//
// Every component factory here produces a binary-valid but *inert* component:
//   * Object::Internal base (fn_ObjectInternal_Ctor)
//   * a single shared 48-slot vtable: slot +0x00 = a real scalar-deleting dtor,
//     every behavior slot = a safe no-op stub returning 0. (In standalone the
//     real methods are delegate-to-NULL — their code/Game reimpls ResolveAddr the
//     unmapped exe — so they cannot be wired yet. Behavior fills in later as each
//     method gets a true standalone reimpl, at which point the type graduates to
//     its own real vtable.)
//   * the real Component capability flags at +0x20 (so the entity registers in the
//     correct update/draw buckets), and a zeroed body.
//
// Per the spawn-chain analysis (CtorFromDescription -> RefreshEntityFlags ->
// StartupComponents(vtable +0x20) -> RegisterEntity -> OnAddedPass(vtable +0x30)),
// no component vtable slot other than the stubs/dtor is exercised during spawn,
// so the shared stub vtable is spawn-safe for every component type.
//
// Each component's .cpp adds, under #ifdef STANDALONE_EXE:
//   COMPONENT_INERT_FACTORY(XxxComponent, /*size*/0xNN, /*flags*/0xMM)
//   REGISTER_TYPE(XxxComponent, "Parent", &XxxComponent_Factory, &XxxComponent_CreateInPlace);
// where size = the _Factory MemAlloc size and flags = the _Ctor's `OR EAX,0xNN`
// value (the +0x20 capability flags). Then remove the matching
// REGISTER_NULL_FACTORY line from factory_registrations_bulk.cpp.

#include <cstdint>
#include <cstddef>

namespace ComponentFactory {

// The address every inert vtable slot carries. A component slot equal to this has NOT
// been reimplemented, whatever the source tree suggests — see food_placement_harness.
void* InertStubSlot();
void* BaseSerializeSlot();   // slot-[3] base-only Serialize (inventory dump probe)

// Slot count for an inert component vtable. 48 slots is comfortable headroom over
// the largest real component vtable, so any in-range virtual call lands on a stub
// rather than reading past the array.
constexpr size_t kInertVtableSlots = 48;

// In-place init of an inert component at `self` (already-allocated `size` bytes):
// Object::Internal base, shared stub vtable, +0x20 = flags, zeroed body.
void  InitInert(void* self, size_t size, uint32_t flags);

// Allocate (`size` bytes, 8-align) + InitInert. Returns nullptr on alloc failure.
void* MakeInert(size_t size, uint32_t flags);

// Foundation E — typed variants. Identical to the above, but install a per-type
// vtable (`vtable`, a kInertVtableSlots-element array owned by the caller/macro)
// whose slot +0x10 (GetTypeId) is `getTypeIdFn`. The find-loops in the start-town
// spawn pools (structures/citizens/resources/environment) locate a component on a
// freshly-instantiated entity by calling its vtable+0x10 and comparing the result
// to the target type id; the shared all-stubs vtable returns 0 there, so no
// component is ever found and the loop writes through a null IBP. A real per-type
// GetTypeId makes those loops resolve our inert components. The vtable is filled
// once on first use (from the shared stub table) and reused for every instance of
// that type. `getTypeIdFn` must be a `uint32_t(*)(void*)` (returns the id in EAX).
void  InitInertTyped(void* self, size_t size, uint32_t flags,
                     void** vtable, void* getTypeIdFn);
void* MakeInertTyped(size_t size, uint32_t flags,
                     void** vtable, void* getTypeIdFn);

}  // namespace ComponentFactory

// Emits XxxComponent_Factory()/XxxComponent_CreateInPlace() for REGISTER_TYPE.
// Each component gets its own vtable so slot +0x10 returns the component's real
// type id (via Object::GetTypeIdByName, cached) — required for the spawn-pool
// component find-loops. Every other slot stays an inert stub. Requires
// "Object/factory_registration.h" to be included for ::Object::GetTypeIdByName.
#define COMPONENT_INERT_FACTORY(Name, Size, Flags)                             \
    static uint32_t Name##_InertGetTypeId(void*) {                             \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_InertVtable[::ComponentFactory::kInertVtableSlots];    \
    extern "C" void* Name##_Factory(void) {                                    \
        return ::ComponentFactory::MakeInertTyped((Size), (Flags),             \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
        return mem;                                                            \
    }

// COMPONENT_INERT_FACTORY_SER — COMPONENT_INERT_FACTORY plus UNCONDITIONAL
// stream-serializer wiring (vtable slot [3], +0x18). Serialize round-trip is
// not an opt-in feature (save/load wave 2026-08-28): without this, the class
// falls through to the base-only Component_SerializeReimpl and its per-instance
// fields silently reset on save/load. SerFn signature:
// `void SerFn(Name* self, void* stream)` (the Ghidra-misnamed *_Shutdown body).
#define COMPONENT_INERT_FACTORY_SER(Name, Size, Flags, SerFn)                  \
    static uint32_t Name##_InertGetTypeId(void*) {                             \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_InertVtable[::ComponentFactory::kInertVtableSlots];    \
    static void Name##_SerVSlot(void* s, void* stream) {                       \
        SerFn(reinterpret_cast<Name*>(s), stream);                             \
    }                                                                          \
    static void Name##_FinishInertVtableSer() {                                \
        Name##_InertVtable[3] = reinterpret_cast<void*>(&Name##_SerVSlot);     \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
        Name##_FinishInertVtableSer();                                         \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
        Name##_FinishInertVtableSer();                                         \
        return mem;                                                            \
    }

// COMPONENT_LIVE_FACTORY — the GrowthComponent bring-up pattern, generalized
// (Track C / Phase 1). Like COMPONENT_INERT_FACTORY, but it graduates the
// per-type vtable's per-tick Update slot — vtable +0x48 / index 9, the slot the EntityList bucket
// round-robin actually calls — to the component's real Update. The Update fn must
// have signature `void UpdateFn(Name* self, int64_t* deltaTime)`.
//
// IMPORTANT: a live Update on an inert-spawned component crashes unless the body
// is initialized (the inert factory zeroes it). Either the Update must early-out
// on null body fields (GUARD), or use COMPONENT_LIVE_FACTORY_S to also route the
// Startup slot to a body-init fn. See growthcomponent.cpp for the worked example.
#define COMPONENT_LIVE_FACTORY(Name, Size, Flags, UpdateFn)                    \
    static uint32_t Name##_LiveGetTypeId(void*) {                              \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];     \
    static void Name##_UpdateVSlot(void* s, void* t) {                         \
        UpdateFn(reinterpret_cast<Name*>(s), reinterpret_cast<int64_t*>(t));   \
    }                                                                          \
    static void Name##_FinishLiveVtable() {                                    \
        Name##_LiveVtable[9] =                                                 \
            reinterpret_cast<void*>(&Name##_UpdateVSlot);                      \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return mem;                                                            \
    }

// As COMPONENT_LIVE_FACTORY_S, but routes ONLY the Startup slot (vtable +0x20 /
// index 4), leaving the per-tick Update slot (index 9) an inert no-op stub. Use
// to populate a component's body at spawn — so sibling components can read it —
// WITHOUT running its own (heavy / not-yet-ported) Update. The body-init fn is
// `void StartupFn(Name* self)`.
#define COMPONENT_LIVE_FACTORY_STARTUP(Name, Size, Flags, StartupFn)           \
    static uint32_t Name##_LiveGetTypeId(void*) {                              \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];     \
    static void Name##_StartupVSlot(void* s) {                                 \
        StartupFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                          \
    static void Name##_FinishLiveVtable() {                                    \
        Name##_LiveVtable[4] =                                                 \
            reinterpret_cast<void*>(&Name##_StartupVSlot);                     \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return mem;                                                            \
    }

// As COMPONENT_LIVE_FACTORY_STARTUP, but routes ONLY the OnAdded slot (vtable
// +0x30 / index 6 — dispatched by ComponentEntity_OnAddedPass at register time,
// for components whose Component+0x20 bit 0 is set). Use to run a component's
// register-with-systems / one-shot setup at spawn WITHOUT running its per-tick
// Update. The OnAdded fn is `void OnAddedFn(Name* self)`.
#define COMPONENT_LIVE_FACTORY_ONADDED(Name, Size, Flags, OnAddedFn)           \
    static uint32_t Name##_LiveGetTypeId(void*) {                              \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];     \
    static void Name##_OnAddedVSlot(void* s) {                                 \
        OnAddedFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                          \
    static void Name##_FinishLiveVtable() {                                    \
        Name##_LiveVtable[6] =                                                 \
            reinterpret_cast<void*>(&Name##_OnAddedVSlot);                     \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return mem;                                                            \
    }

// As COMPONENT_LIVE_FACTORY, but also routes the Startup slot (vtable +0x20 /
// index 4 — dispatched by ComponentEntity_StartupComponents at spawn) to a
// body-init fn `void StartupFn(Name* self)`. Use when the live Update needs the
// body populated from the entity/description before the first tick.
#define COMPONENT_LIVE_FACTORY_S(Name, Size, Flags, UpdateFn, StartupFn)        \
    static uint32_t Name##_LiveGetTypeId(void*) {                              \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];     \
    static void Name##_UpdateVSlot(void* s, void* t) {                         \
        UpdateFn(reinterpret_cast<Name*>(s), reinterpret_cast<int64_t*>(t));   \
    }                                                                          \
    static void Name##_StartupVSlot(void* s) {                                 \
        StartupFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                          \
    static void Name##_FinishLiveVtable() {                                    \
        Name##_LiveVtable[4] =                                                 \
            reinterpret_cast<void*>(&Name##_StartupVSlot);                     \
        Name##_LiveVtable[9] =                                                 \
            reinterpret_cast<void*>(&Name##_UpdateVSlot);                      \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return mem;                                                            \
    }

// As COMPONENT_LIVE_FACTORY_S, plus the POST-UPDATE slot (vtable +0x50 / index 10 —
// dispatched by EntityList::PostUpdateNodeHandler on every component whose flags carry
// bit 4). That slot is where a component reacts to its own Update having changed
// something, and it shares the component's env var because the pair is one feature: the
// Update sets the bit, the post-update consumes it and clears it again. GrowthComponent
// is the worked example — Update accumulates growth, slot 10 eases the model's visual
// scale toward it. The PostUpdate fn is `void PostUpdateFn(Name* self)`.
#define COMPONENT_LIVE_FACTORY_SP(Name, Size, Flags, UpdateFn, StartupFn,         \
                                  PostUpdateFn)                                   \
    static uint32_t Name##_LiveGetTypeId(void*) {                                 \
        static uint32_t cached = 0;                                               \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);               \
        return cached;                                                            \
    }                                                                             \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];        \
    static void Name##_UpdateVSlot(void* s, void* t) {                            \
        UpdateFn(reinterpret_cast<Name*>(s), reinterpret_cast<int64_t*>(t));      \
    }                                                                             \
    static void Name##_StartupVSlot(void* s) {                                    \
        StartupFn(reinterpret_cast<Name*>(s));                                    \
    }                                                                             \
    static void Name##_PostUpdateVSlot(void* s) {                                 \
        PostUpdateFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                             \
    static void Name##_FinishLiveVtable() {                                       \
        Name##_LiveVtable[4] =                                                    \
            reinterpret_cast<void*>(&Name##_StartupVSlot);                        \
        Name##_LiveVtable[9] =                                                    \
            reinterpret_cast<void*>(&Name##_UpdateVSlot);                         \
        Name##_LiveVtable[10] =                                                   \
            reinterpret_cast<void*>(&Name##_PostUpdateVSlot);                     \
    }                                                                             \
    extern "C" void* Name##_Factory(void) {                                       \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),             \
            Name##_LiveVtable,                                                    \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                      \
        Name##_FinishLiveVtable();                                                \
        return p;                                                                 \
    }                                                                             \
    static void* Name##_CreateInPlace(void* mem) {                                \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),         \
            Name##_LiveVtable,                                                    \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                      \
        Name##_FinishLiveVtable();                                                \
        return mem;                                                               \
    }

// As COMPONENT_LIVE_FACTORY_S, plus the OnAdded slot (vtable +0x30 / index 6 —
// dispatched by ComponentEntity_OnAddedPass at EntityList_RegisterEntity time)
// through its own macro parameter. The
// slot-6 install has to live in FinishLiveVtable like the others: InitInertTyped
// re-stubs the shared vtable on every instance creation, so anything installed
// from a per-instance body would be erased by the next component's creation.
#define COMPONENT_LIVE_FACTORY_SOA(Name, Size, Flags, UpdateFn,               \
                                   StartupFn, OnAddedFn)                       \
    static uint32_t Name##_LiveGetTypeId(void*) {                              \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_LiveVtable[::ComponentFactory::kInertVtableSlots];     \
    static void Name##_UpdateVSlot(void* s, void* t) {                         \
        UpdateFn(reinterpret_cast<Name*>(s), reinterpret_cast<int64_t*>(t));   \
    }                                                                          \
    static void Name##_StartupVSlot(void* s) {                                 \
        StartupFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                          \
    static void Name##_OnAddedVSlot(void* s) {                                 \
        OnAddedFn(reinterpret_cast<Name*>(s));                                 \
    }                                                                          \
    static void Name##_FinishLiveVtable() {                                    \
        Name##_LiveVtable[4] =                                                 \
            reinterpret_cast<void*>(&Name##_StartupVSlot);                     \
        Name##_LiveVtable[9] =                                                 \
            reinterpret_cast<void*>(&Name##_UpdateVSlot);                      \
        Name##_LiveVtable[6] =                                                 \
            reinterpret_cast<void*>(&Name##_OnAddedVSlot);                     \
    }                                                                          \
    extern "C" void* Name##_Factory(void) {                                    \
        void* p = ::ComponentFactory::MakeInertTyped((Size), (Flags),          \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return p;                                                              \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                             \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),      \
            Name##_LiveVtable,                                                 \
            reinterpret_cast<void*>(&Name##_LiveGetTypeId));                   \
        Name##_FinishLiveVtable();                                             \
        return mem;                                                            \
    }

// ---------------------------------------------------------------------------
// COMPONENT_INSTALL_SERIALIZE — wire a component's OWN stream Serialize into
// vtable slot 3 (+0x18) for any factory macro that does not do it itself.
//
// Of the factory macros in this header only COMPONENT_INERT_FACTORY_SER fills
// slot 3; every other one leaves the generic BASE-ONLY Component::Serialize
// there. For a component whose retail slot 3 reads more than the base, that is a
// stream desync, not a missing feature: the record in the save is longer than we
// read, so every object deserialized after it comes off misaligned bytes. Our own
// saves cannot catch it (the save leg skips the same bytes); only a retail
// authored save can. Serializer wiring is therefore never behind a live/env gate.
//
// Use after the factory macro and register the _Ser entry points:
//   COMPONENT_INSTALL_SERIALIZE(FooComponent, FooComponent_Shutdown)
//   REGISTER_TYPE(FooComponent, "Component",
//                 &FooComponent_Factory_Ser, &FooComponent_CreateInPlace_Ser);
// ---------------------------------------------------------------------------
#define COMPONENT_INSTALL_SERIALIZE(Name, SerBody)                              \
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
