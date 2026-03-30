// code/Game/Component/destroycomponent.cpp
// Clean reimplementation of DestroyComponent lifecycle methods from Application-x64.exe.
//
// DestroyComponent (0x68 bytes) -- ControllerComponent subclass.
// Manages building demolition/destruction process.
// Fields: field48 (Time), destroyFlags (uint16 +0x50), field51 (bool),
//         field54 (uint32), field58 (ptr), field60 (entity ref / mesh ID).
//
// Vtable at 0x140163120. Two non-trivial overrides:
//   Shutdown  (0x14011DB50) -- serialize all destroy state fields
//   Serialize (0x14011DB00) -- OnCreate: copy mesh ref from building data
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h -- portable for standalone exe.

#include "Component/destroycomponent_funcs.h"
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

// IO::Serializer::Serializer(stream, version) -- dispatch slot at 0x140152ad8
typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// IO::Serializer::~Serializer() -- dispatch slot at 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(int*) -- dispatch slot at 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// IO::Serializer::Serialize(bool*) -- dispatch slot at 0x140152a58
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// System::Time::Serialize -- dispatch slot at 0x140152aa8
typedef void (*fn_Time_Serialize_t)(void* time, void* serializer);
static fn_Time_Serialize_t s_fn_Time_Serialize = nullptr;

// FUN_140002340 -- serialize InternalBasePtr (field60)
typedef void (*fn_SerializeInternalBasePtr_t)(void* ptrField, void* serializer);
static fn_SerializeInternalBasePtr_t s_fn_SerializeInternalBasePtr = nullptr;

// IsDerivedFrom -- dispatch slot at 0x140152a98
typedef bool (*fn_IsDerivedFrom_dispatch_t)(void* self, uint32_t typeId);
static fn_IsDerivedFrom_dispatch_t s_fn_IsDerivedFrom_d = nullptr;

// Type ID for building component type (DAT_1401c7628)
static uint32_t* s_pBuildingCompTypeId = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_SerializeInternalBasePtr = reinterpret_cast<fn_SerializeInternalBasePtr_t>(
        ResolveAddr(Module::Exe, 0x140002340));
    s_pBuildingCompTypeId = reinterpret_cast<uint32_t*>(
        ResolveAddr(Module::Exe, 0x1401c7628));

    uintptr_t slot;

    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152aa8);
    if (slot) s_fn_Time_Serialize = reinterpret_cast<fn_Time_Serialize_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    slot = ResolveAddr(Module::Exe, 0x140152a98);
    if (slot) s_fn_IsDerivedFrom_d = reinterpret_cast<fn_IsDerivedFrom_dispatch_t>(
        *reinterpret_cast<uintptr_t*>(slot));

    s_initialized = true;
}

// ============================================================
// DestroyComponent::Shutdown (vtable slot [3], 0x14011DB50)
// ============================================================
// Full serialization of destroy state:
//   1. Component_Serialize
//   2. Empty serializer block (controller compat)
//   3. Serializer(version=0): field54 (int), field58 (int),
//      field51 (bool), field60 (InternalBasePtr via FUN_140002340),
//      destroyFlags at +0x50 (bool), field48 (Time)

void DestroyComponent_Shutdown(DestroyComponent* self, void* stream) {
    EnsureHelpers();

    // Call base Component_Serialize
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (controller format compat)
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor && s_fn_Serializer_Dtor) {
        s_fn_Serializer_Ctor(serializer1, stream, 0);
        s_fn_Serializer_Dtor(serializer1);
    }

    // Serialize destroy state fields
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer2, stream, 0);
    }

    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer2, (int*)&self->field54, nullptr);
        s_fn_Serializer_SerializeInt(serializer2, (int*)((uintptr_t)self + 0x58), nullptr);
    }

    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer2, (bool*)((uintptr_t)self + 0x51), nullptr);
    }

    if (s_fn_SerializeInternalBasePtr) {
        s_fn_SerializeInternalBasePtr((void*)((uintptr_t)self + 0x60), serializer2);
    }

    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer2, (bool*)&self->destroyFlags, nullptr);
    }

    if (s_fn_Time_Serialize) {
        s_fn_Time_Serialize((void*)&self->field48, serializer2);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer2);
    }
}

// ============================================================
// DestroyComponent::Serialize (vtable slot [4], 0x14011DB00)
// ============================================================
// Copies mesh reference from building data. Reads _unk18,
// does IsDerivedFrom check, copies 8 bytes from building+0x20
// into field60.

void DestroyComponent_Serialize(DestroyComponent* self) {
    EnsureHelpers();

    void* baseObj = self->_unk18;
    if (!s_fn_IsDerivedFrom_d || !s_pBuildingCompTypeId) return;

    bool isDerived = s_fn_IsDerivedFrom_d(baseObj, *s_pBuildingCompTypeId);
    void* buildingData = isDerived ? baseObj : nullptr;

    if (buildingData) {
        // Copy mesh reference from building data +0x20 into field60
        *(uint64_t*)&self->field60 = *(uint64_t*)((uintptr_t)buildingData + 0x20);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void DestroyComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[DestroyComponent] Funcs resolved: Serialize=%p IsDerivedFrom=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_IsDerivedFrom_d);
#endif
}
