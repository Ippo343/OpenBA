// code/Game/Component/wildanimalcomponent.cpp
// Clean reimplementation of WildAnimalComponent lifecycle methods.
//
// WildAnimalComponent (0x30 bytes) -- simple Component subclass.
// Holds a back-reference at +0x28 to the HERD entity this wild animal belongs to.
//
// Vtable at 0x140164af0 (slot addresses read from .rdata):
//   [3] Serialize = 0x14012e300 -- Component base + the InternalBasePtr at +0x28,
//       then a post-load IsDerivedFrom validation against the HerdComponent id.
//   [7] OnRemove  = 0x14012e2b0 -- ERASES self from the herd entity's component
//       list at +0x38 (DynArray8_EraseByValue).
//
// NAMING CORRECTIONS (2026-07-28): this file previously called slot 3 "Shutdown"
// and slot 7 "OnCreate", and described FUN_140002cc0 as "add to component list".
// Ghidra names them WildAnimalComponent_Serialize / WildAnimalComponent_OnRemove,
// and FUN_140002cc0 is erase-by-value, not append (append is FUN_140003110). The
// emitted code was already correct -- only the labels were wrong. The real
// OnCreate is 0x14012e440 (a Stream overload that just brackets an IO::Serializer
// and returns 1); it is NOT in this vtable, so nothing here should route to it.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Component/wildanimalcomponent_funcs.h"
#include "Component/HerdComponent.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.4 caller batch G

// ============================================================
// Resolved helpers
// ============================================================

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// FUN_140002cc0 -- DynArray8_EraseByValue: remove an InternalBasePtr from the
// 8-byte-element list at entity+0x38. (Append is FUN_140003110, a different fn.)
typedef void (*fn_EraseFromComponentList_t)(void* listBase, void* ibpPtr);
static fn_EraseFromComponentList_t s_fn_EraseFromComponentList = nullptr;

// Object::InternalBasePtr::Ctor(retbuf, rawPtr) -- creates IBP from raw pointer
typedef void (*fn_IBP_Ctor_t)(void* retbuf, void* rawPtr);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    // Dispatch slots

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // InternalBasePtr::Ctor -- global
    s_fn_IBP_Ctor = reinterpret_cast<fn_IBP_Ctor_t>(
        reinterpret_cast<uintptr_t>(fn_InternalBasePtr_Ctor));

    // FUN_140002cc0 -- erase from component list
    s_fn_EraseFromComponentList = reinterpret_cast<fn_EraseFromComponentList_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    s_helpersResolved = true;
}

// 0x14012e300 door: registered by address (standalone/reg_animal_serialize.cpp).
void WildAnimalComponent_Serialize(WildAnimalComponent* self, void* stream) {
    self->WildAnimalComponent::Persist(stream);
}


// ============================================================
// WildAnimalComponent::OnRemove (vtable slot [7], 0x14012e2b0)
// ============================================================
// If data28 (the herd entity back-ref at +0x28) is non-null:
//   1. Creates an InternalBasePtr wrapping self
//   2. CheckPtr on the +0x28 back-ref
//   3. ERASES self from that entity's component list at +0x38 (FUN_140002cc0 =
//      DynArray8_EraseByValue), so the herd stops seeing this animal.
//
// Original pseudocode:
//   if (self->data28 != 0) {
//       InternalBasePtr ibp;
//       InternalBasePtr::Ctor(&ibp, self);
//       CheckPtr(&self->data28);
//       DynArray8_EraseByValue(self->data28 + 0x38, &ibp);
//   }

// Herd de-registration census. AdjustSize only spawns while members(+0x40) <
// target, and target is derived from that same count -- so if this slot never
// runs, dead deer stay counted, the target stays saturated, and the population
// drains with no regrowth. "OnRemove is installed" is not evidence it is
// dispatched; count both arms.
static long long g_war_calls = 0, g_war_noHerd = 0;

// ============================================================
// Init: resolve function pointers
// ============================================================
void WildAnimalComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[WildAnimalComponent] Funcs resolved: "
           "EraseFromList=%p HerdTypeId=%u\n",
           (void*)s_fn_EraseFromComponentList,
           HerdComponent::TypeId());
#endif
}

// ===== ENDGAME B.3.8 CIT — WildAnimalComponent as a REAL class (WildAnimalComponent.h) =====
// Before: a 48-slot hand-built WildAnimalComponent_LiveVtable filled at every
// create (slots 3 Serialize, 7 OnRemove), Inert_ScalarDtor in slot 0. After: the
// compiler's 16-slot vtable and a real ctor/dtor chain (B38_RULES D1, D2).
// Slot 7 erases the animal from its herd's member list — mandatory once deer can
// die (a dangling entry faults HerdComponent_Update's migration pass).
#include "Object/factory_registration.h"
#include "Component/WildAnimalComponent.h"

// 0x14012e3c0. Component() is the CALL 0x1401188b0; the 0x140164af0 stamp is
// the compiler's; +0x28 = 0; flags |= 0x02 (0x82).
WildAnimalComponent::WildAnimalComponent() : data28(0) {
    OrFlagsInCtor(0x02);
}

// ============================================================
// WildAnimalComponent::Serialize (vtable slot [3], 0x14012e300)
// ============================================================
// Serializes Component base, then the InternalBasePtr at +0x28.
// After deserialization, validates the pointer with IsDerivedFrom against the
// HerdComponent id and nulls it out if the check fails (save compatibility).
//
// Original pseudocode:
//   Component_Serialize(self, stream)
//   IO::Serializer::Serializer(local, stream, 0)
//   Object::InternalBasePtr::Serialize(&self->data28, local)
//   if (self->data28 != 0 && !IsDerivedFrom(self->data28, HerdComponentTypeId))
//       self->data28 = 0
//   IO::Serializer::~Serializer(local)

void WildAnimalComponent::Persist(void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    Component::Persist(stream);   // retail: CALL 0x1401189b0 (qualified)

    // Create local IO::Serializer (24 bytes on stack)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize InternalBasePtr at +0x28
    reinterpret_cast<Object::InternalBasePtr*>(&data28)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4 bridge

    // Post-deserialize type validation
    if (data28 != 0) {
        if (s_fn_IsDerivedFrom_dispatch) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                reinterpret_cast<void*>(data28), HerdComponent::TypeId());
            if (!valid) {
                data28 = 0;
            }
        }
    }

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}
void WildAnimalComponent::OnRemove() {
    EnsureHelpers();

    if (data28 == 0) ++g_war_noHerd;
    ++g_war_calls;
    const bool trace = (g_war_calls % 10) == 1;
    const uintptr_t herdDbg = static_cast<uintptr_t>(data28);
    const int before = herdDbg ? *reinterpret_cast<int*>(herdDbg + 0x40) : -1;

    if (data28 != 0) {
        // Create InternalBasePtr from self (the component)
        uint64_t ibp = 0;
        if (s_fn_IBP_Ctor) {
            s_fn_IBP_Ctor(&ibp, this);
        }

        // CheckPtr on entity reference

        // Erase self from the herd entity's component list at entity+0x38
        if (s_fn_EraseFromComponentList) {
            uintptr_t entity = static_cast<uintptr_t>(data28);
            s_fn_EraseFromComponentList(
                reinterpret_cast<void*>(entity + 0x38), &ibp);
        }
    }

    if (trace) {
        const int after = herdDbg ? *reinterpret_cast<int*>(herdDbg + 0x40) : -1;
        fprintf(stderr, "[WildAnimalRm] calls=%lld noHerd=%lld herd=%p members %d -> %d%s\n",
                g_war_calls, g_war_noHerd, reinterpret_cast<void*>(herdDbg),
                before, after, (before == after) ? "  ERASE-NO-OP" : "");
        fflush(stderr);
    }
}

// create 0x140039c20: MemAlloc(0x30, 8), then the ctor.
void* WildAnimalComponent::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(WildAnimalComponent), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) WildAnimalComponent());
}
// Retail's createInPlace is `Internal::CreateInPlaceObject` (returns null); the
// tree's in-place entry built the object, and that is kept (B38 B8-13).
void* WildAnimalComponent::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) WildAnimalComponent());
}

DEFINE_EXE_TYPE(WildAnimalComponent, "Component")
