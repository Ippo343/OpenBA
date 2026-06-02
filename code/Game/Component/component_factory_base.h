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
#define COMPONENT_INERT_FACTORY(Name, Size, Flags)                              \
    static uint32_t Name##_InertGetTypeId(void*) {                             \
        static uint32_t cached = 0;                                            \
        if (cached == 0) cached = ::Object::GetTypeIdByName(#Name);            \
        return cached;                                                         \
    }                                                                          \
    static void* Name##_InertVtable[::ComponentFactory::kInertVtableSlots];    \
    extern "C" void* Name##_Factory(void) {                                    \
        return ::ComponentFactory::MakeInertTyped((Size), (Flags),            \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
    }                                                                          \
    static void* Name##_CreateInPlace(void* mem) {                            \
        if (mem) ::ComponentFactory::InitInertTyped(mem, (Size), (Flags),     \
            Name##_InertVtable,                                                \
            reinterpret_cast<void*>(&Name##_InertGetTypeId));                  \
        return mem;                                                           \
    }
