// code/Game/Component/wildanimalcomponent.cpp
// Clean reimplementation of WildAnimalComponent lifecycle methods.
//
// WildAnimalComponent (0x30 bytes) -- simple Component subclass.
// Holds a single entity reference at +0x28 (the wild animal entity).
//
// Vtable at 0x140164af0:
//   [3] Shutdown = 0x14012e300 -- serialization with InternalBasePtr + IsDerivedFrom
//   [7] OnCreate = 0x14012e2b0 -- registers self with animal's component list
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

// FUN_140002cc0 -- add InternalBasePtr to component list at entity+0x38
typedef void (*fn_AddToComponentList_t)(void* listBase, void* ibpPtr);
static fn_AddToComponentList_t s_fn_AddToComponentList = nullptr;

// Object::InternalBasePtr::CheckPtr
typedef void (*fn_CheckPtr_t)(void* ptrField);
static fn_CheckPtr_t s_fn_CheckPtr = nullptr;

// Object::InternalBasePtr::Ctor(retbuf, rawPtr) -- creates IBP from raw pointer
typedef void (*fn_IBP_Ctor_t)(void* retbuf, void* rawPtr);
static fn_IBP_Ctor_t s_fn_IBP_Ctor = nullptr;

// Type ID: DAT_1401c7820 -- WildAnimal type for IsDerivedFrom validation
static uint32_t* s_TypeId_WildAnimal = nullptr;

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

    // FUN_140002cc0 -- add to component list
    s_fn_AddToComponentList = reinterpret_cast<fn_AddToComponentList_t>(
        ResolveAddr(Module::Exe, 0x140002cc0));

    // Type ID
    s_TypeId_WildAnimal = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7820));

    s_helpersResolved = true;
}

// ============================================================
// WildAnimalComponent::Shutdown (vtable slot [3], 0x14012e300)
// ============================================================
// Serializes Component base, then InternalBasePtr at +0x28.
// After deserialization, validates the pointer with IsDerivedFrom
// and nulls it out if the type check fails (save compatibility).
//
// Original pseudocode:
//   Component_Serialize(self, stream)
//   IO::Serializer::Serializer(local, stream, 0)
//   Object::InternalBasePtr::Serialize(&self->data28, local)
//   if (self->data28 != 0 && !IsDerivedFrom(self->data28, WildAnimalTypeId))
//       self->data28 = 0
//   IO::Serializer::~Serializer(local)

void WildAnimalComponent_Shutdown(WildAnimalComponent* self, void* stream) {
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
        if (s_fn_IsDerivedFrom_dispatch && s_TypeId_WildAnimal) {
            bool valid = s_fn_IsDerivedFrom_dispatch(
                reinterpret_cast<void*>(self->data28), *s_TypeId_WildAnimal);
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
// WildAnimalComponent::OnCreate (vtable slot [7], 0x14012e2b0)
// ============================================================
// If data28 (entity ref at +0x28) is non-null:
//   1. Creates InternalBasePtr from owner (self)
//   2. CheckPtr on +0x28 entity
//   3. Adds owner to entity's component list at +0x38 via FUN_140002cc0
//
// Original pseudocode:
//   if (self->data28 != 0) {
//       InternalBasePtr ibp;
//       InternalBasePtr::Ctor(&ibp, self);
//       CheckPtr(&self->data28);
//       FUN_140002cc0(self->data28 + 0x38, &ibp);
//   }

void WildAnimalComponent_OnCreate(WildAnimalComponent* self) {
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

        // Add self to entity's component list at entity+0x38
        if (s_fn_AddToComponentList) {
            uintptr_t entity = static_cast<uintptr_t>(self->data28);
            s_fn_AddToComponentList(
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
           "AddToList=%p TypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IBP_Serialize,
           (void*)s_fn_AddToComponentList,
           (void*)s_TypeId_WildAnimal);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid WildAnimalComponent: size 0x30 + Object::Internal base + shared stub
// vtable + capability flags +0x20=0x82 (from WildAnimalComponent_Ctor) + zeroed body.
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(WildAnimalComponent, 0x30, 0x82)
REGISTER_TYPE(WildAnimalComponent, "Component",
              &WildAnimalComponent_Factory, &WildAnimalComponent_CreateInPlace);
