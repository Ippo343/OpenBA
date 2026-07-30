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
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize (base class) -- at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer::Serializer(stream, version) -- dispatch slot
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer()
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Object::InternalBasePtr::Serialize(this, Serializer*)
typedef void (*fn_IBP_Serialize_t)(void* self, void* serializer);
static fn_IBP_Serialize_t s_fn_IBP_Serialize = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// FUN_140002cc0 -- DynArray8_EraseByValue: remove an InternalBasePtr from the
// 8-byte-element list at entity+0x38. (Append is FUN_140003110, a different fn.)
typedef void (*fn_EraseFromComponentList_t)(void* listBase, void* ibpPtr);
static fn_EraseFromComponentList_t s_fn_EraseFromComponentList = nullptr;

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// Object::InternalBasePtr::Ctor(retbuf, rawPtr) -- creates IBP from raw pointer
typedef void (*fn_IBP_Ctor_t)(void* retbuf, void* rawPtr);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

// Type ID: DAT_1401c7820 -- HerdComponent (NOT "WildAnimal": factory_registrations
// .csv:268 maps this global to HerdComponent_RegisterType). The +0x28 back-ref is
// validated against it because it points at the herd, not at another wild animal.
static uint32_t* s_TypeId_HerdComponent = nullptr;

static bool s_helpersResolved = false;

static void EnsureHelpers() {
    if (s_helpersResolved) return;

    uintptr_t slot;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    // Dispatch slots
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // InternalBasePtr::Serialize -- Runtime export
    s_fn_IBP_Serialize = reinterpret_cast<fn_IBP_Serialize_t>(
        ResolveAddr(Module::Runtime, 0x1800c0210));

    // CheckPtr -- Runtime
    s_fn_CheckPtr = reinterpret_cast<fn_CheckPtr_t>(
        ResolveAddr(Module::Runtime, 0x1800c0290));

    // InternalBasePtr::Ctor -- global
    s_fn_IBP_Ctor = reinterpret_cast<fn_IBP_Ctor_t>(
        reinterpret_cast<uintptr_t>(fn_InternalBasePtr_Ctor));

    // FUN_140002cc0 -- erase from component list
    s_fn_EraseFromComponentList = reinterpret_cast<fn_EraseFromComponentList_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    // Type ID (HerdComponent)
    s_TypeId_HerdComponent = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7820));

    s_helpersResolved = true;
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

void WildAnimalComponent_Serialize(WildAnimalComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Create local IO::Serializer (24 bytes on stack)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Serialize InternalBasePtr at +0x28
    if (s_fn_IBP_Serialize) {
        s_fn_IBP_Serialize(reinterpret_cast<uint8_t*>(self) + 0x28, serializer);
    }

    // Post-deserialize type validation
    if (self->data28 != 0) {
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_HerdComponent) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                reinterpret_cast<void*>(self->data28), *s_TypeId_HerdComponent);
            if (!valid) {
                self->data28 = 0;
            }
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
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

void WildAnimalComponent_OnRemove(WildAnimalComponent* self) {
    EnsureHelpers();

    if (self->data28 != 0) {
        // Create InternalBasePtr from self (the component)
        uint64_t ibp = 0;
        if (s_fn_IBP_Ctor) {
            s_fn_IBP_Ctor(&ibp, self);
        }

        // CheckPtr on entity reference
        if (s_fn_CheckPtr) {
            s_fn_CheckPtr(reinterpret_cast<uint8_t*>(self) + 0x28);
        }

        // Erase self from the herd entity's component list at entity+0x38
        if (s_fn_EraseFromComponentList) {
            uintptr_t entity = static_cast<uintptr_t>(self->data28);
            s_fn_EraseFromComponentList(
                reinterpret_cast<void*>(entity + 0x38), &ibp);
        }
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void WildAnimalComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[WildAnimalComponent] Funcs resolved: Serialize=%p IBP_Serialize=%p "
           "EraseFromList=%p HerdTypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_EraseFromComponentList,
           (void*)s_TypeId_HerdComponent);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid WildAnimalComponent: size 0x30 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x82 (from WildAnimalComponent_Ctor) + zeroed body.
//
// LIVE slot 7 (OnRemove, retail 0x14012e2b0): erases this animal from its herd's
// member list (herd+0x30/+0x38). MANDATORY once deer can die — a dead deer that
// stays in the list leaves a dangling entry that HerdComponent_Update's migration
// pass then walks (StartMigration faulted on exactly that: WRITE null+0x38).
// Hand-rolled (no macro variant exposes slot 7); pattern copied from
// pasturecomponent.cpp's controller factory. Gated on any of the animal-wave
// flags that can put deer in the world.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

static uint32_t WildAnimalComponent_LiveGetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WildAnimalComponent");
    return cached;
}
static void* WildAnimalComponent_LiveVtable[::ComponentFactory::kInertVtableSlots];

static void WildAnimalComponent_OnRemoveVSlot(void* self) {
    WildAnimalComponent_OnRemove(reinterpret_cast<WildAnimalComponent*>(self));
}

// Re-run for every instance: InitInertTyped re-stubs the shared vtable each time.
static void WildAnimalComponent_InstallLive() {
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_HERD") &&
        !::ComponentFactory::LiveEnabled("BANISHED_LIVE_POPULATION") &&
        !::ComponentFactory::LiveEnabled("BANISHED_LIVE_ANIMAL_UPDATE") &&
        !::ComponentFactory::LiveEnabled("BANISHED_LIVE_ANIMALS"))
        return;
    WildAnimalComponent_LiveVtable[7] =
        reinterpret_cast<void*>(&WildAnimalComponent_OnRemoveVSlot);
}

extern "C" void* WildAnimalComponent_Factory(void) {
    void* p = ::ComponentFactory::MakeInertTyped(0x30, 0x82,
        WildAnimalComponent_LiveVtable,
        reinterpret_cast<void*>(&WildAnimalComponent_LiveGetTypeId));
    WildAnimalComponent_InstallLive();
    return p;
}
static void* WildAnimalComponent_CreateInPlace(void* mem) {
    if (mem) ::ComponentFactory::InitInertTyped(mem, 0x30, 0x82,
        WildAnimalComponent_LiveVtable,
        reinterpret_cast<void*>(&WildAnimalComponent_LiveGetTypeId));
    WildAnimalComponent_InstallLive();
    return mem;
}
REGISTER_TYPE(WildAnimalComponent, "Component",
              &WildAnimalComponent_Factory, &WildAnimalComponent_CreateInPlace);
