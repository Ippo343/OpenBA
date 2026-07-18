// code/Game/Component/autopickupcomponent.cpp
// Clean reimplementation of AutoPickupComponent lifecycle methods.
//
// AutoPickupComponent (0x40 bytes) auto-collects dropped resources near buildings.
// Fields: +0x28=isEnabled (byte), +0x30=array ptr, +0x38=array count.
//
// Vtable at 0x140158df0. Non-trivial overrides:
//   Shutdown  (0x1400449d0) -- Serializes isEnabled bool + int array
//   Serialize (0x140044990) -- IsDerivedFrom check to init isEnabled from config
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/autopickupcomponent_funcs.h"
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

// IO::Serializer::Serialize(bool*, name) -- serializes a bool
typedef void (*fn_Serialize_Bool_t)(void* serializer, bool* value, const char* name);
static fn_Serialize_Bool_t s_fn_Serialize_Bool = nullptr;

typedef void (*fn_Serialize_Int_t)(void* serializer, int* value, const char* name);
static fn_Serialize_Int_t s_fn_Serialize_Int = nullptr;

// IO::Serializer::Serialize(count, array, name) -- bulk int array serialization
typedef void (*fn_Serialize_IntArray_t)(void* serializer, int count, int* array, const char* name);
static fn_Serialize_IntArray_t s_fn_Serialize_IntArray = nullptr;

// FUN_1400021d0(array_ptr_field, new_count) -- resize int array
typedef void (*fn_ResizeIntArray_t)(void* arrayField, int newCount);
static fn_ResizeIntArray_t s_fn_ResizeIntArray = nullptr;

// IsDerivedFrom dispatch slot
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* obj, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_dispatch = nullptr;

// Type ID for AutoPickupDescription at DAT_1401c3fe8
static uint32_t* s_TypeId_AutoPickupDesc = nullptr;

static bool s_initialized = false;

// ============================================================
// AutoPickupComponent::Shutdown (vtable slot [3], 0x1400449d0)
// ============================================================
// Serializes:
//   1. Component_Serialize base
//   2. Serializer(version=0): serialize isEnabled bool at +0x28
//   3. Read count from +0x38, serialize it
//   4. If loading: resize array at +0x30 via FUN_1400021d0
//   5. Bulk-serialize int array elements
void AutoPickupComponent_Shutdown(AutoPickupComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    uint8_t serializer[24];
    if (!s_fn_Serializer_Ctor) return;
    s_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize isEnabled bool at +0x28
    if (s_fn_Serialize_Bool)
        s_fn_Serialize_Bool(serializer, (bool*)(selfAddr + 0x28), nullptr);

    // Read count from +0x38, serialize it
    int localCount = *(int*)(selfAddr + 0x38);
    if (s_fn_Serialize_Int)
        s_fn_Serialize_Int(serializer, &localCount, nullptr);

    // If loading (stream mode == 0), resize array at +0x30
    uintptr_t serializerObj = *(uintptr_t*)serializer;
    if (*(int*)(serializerObj + 8) == 0) {
        if (s_fn_ResizeIntArray)
            s_fn_ResizeIntArray((void*)(selfAddr + 0x30), localCount);
    }

    // Bulk-serialize int array
    int* arrayPtr = *(int**)(selfAddr + 0x30);
    if (s_fn_Serialize_IntArray)
        s_fn_Serialize_IntArray(serializer, localCount, arrayPtr, nullptr);

    if (s_fn_Serializer_Dtor)
        s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// AutoPickupComponent::Serialize (vtable slot [4], 0x140044990)
// ============================================================
// Checks IsDerivedFrom on +0x18 with type DAT_1401c3fe8,
// reads byte at +0x20 from the derived object and stores in isEnabled (+0x28).
void AutoPickupComponent_Serialize(AutoPickupComponent* self) {
    uintptr_t selfAddr = (uintptr_t)self;

    void* obj = *(void**)(selfAddr + 0x18);
    void* config = nullptr;

    if (obj && s_fn_IsDerivedFrom_dispatch && s_TypeId_AutoPickupDesc) {
        bool derived = s_fn_IsDerivedFrom_dispatch(obj, *s_TypeId_AutoPickupDesc);
        if (derived) config = obj;
    }

    if (config) {
        *(uint8_t*)(selfAddr + 0x28) = *(uint8_t*)((uintptr_t)config + 0x20);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void AutoPickupComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serialize_Int = *reinterpret_cast<fn_Serialize_Int_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_dispatch = *reinterpret_cast<fn_IsDerivedFrom_dispatch_t*>(slot);

    // Serialize bool -- dispatch slot at 0x140152a58
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serialize_Bool = *reinterpret_cast<fn_Serialize_Bool_t*>(slot);

    // Bulk int array serialization -- dispatch slot at 0x140151278
    slot = ResolveAddr(Module::Exe, 0x140151278);
    if (slot) s_fn_Serialize_IntArray = *reinterpret_cast<fn_Serialize_IntArray_t*>(slot);

    s_fn_ResizeIntArray = reinterpret_cast<fn_ResizeIntArray_t>(
        ResolveAddr(Module::Exe, 0x1400021d0));

    s_TypeId_AutoPickupDesc = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c3fe8));

    s_initialized = true;

#ifdef _DEBUG
    printf("[AutoPickupComponent] Funcs resolved: CompSerialize=%p Resize=%p TypeId=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ResizeIntArray,
           (void*)s_TypeId_AutoPickupDesc);
#endif
}

// ===== Standalone factory (Foundation D — component instantiation) =====
// Inert-but-valid AutoPickupComponent so Object::Factory::CreateObject(typeId)
// resolves during ComponentEntity_CtorFromDescription instead of NullFactory.
// size 0x40 (AutoPickupComponent_Factory MemAlloc), flags 0x80 (Ctor 0x140044ae0:
// Component base sets +0x20=0x80, leaf BTS bit7 => 0x80).
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"

COMPONENT_INERT_FACTORY(AutoPickupComponent, 0x40, 0x80)
REGISTER_TYPE(AutoPickupComponent, "Component",
              &AutoPickupComponent_Factory, &AutoPickupComponent_CreateInPlace);
