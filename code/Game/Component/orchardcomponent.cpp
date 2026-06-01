// code/Game/Component/orchardcomponent.cpp
// Clean reimplementation of OrchardComponent lifecycle methods from Application-x64.exe.
//
// OrchardComponent (0x68 bytes) extends ControllerComponent with:
//   +0x48: uint8  orchardFlag, +0x4C: int32 orchardId, +0x50-0x60: arrays
//
// Vtable at 0x14015a180. One hooked override:
//   Shutdown (0x140051de0) — Complex entity ref serialization
//
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/orchardcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

typedef void* (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

typedef void (*fn_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_SerializeBool_t s_fn_SerializeBool = nullptr;

typedef void (*fn_SerializeInt_t)(void* self, int* value, const char* name);
static fn_SerializeInt_t s_fn_SerializeInt = nullptr;

// FUN_140051830 — validate entity references in orchard
typedef void (*fn_ValidateOrchardRefs_t)(void* self);
static fn_ValidateOrchardRefs_t s_fn_ValidateOrchardRefs = nullptr;

// FUN_140009f80 — resize entity ref array at +0x50
typedef void (*fn_ResizeRefArray_t)(void* arrayPtr, int newCount);
static fn_ResizeRefArray_t s_fn_ResizeRefArray = nullptr;

// FUN_14000a230 — serialize entity ref array entries
typedef void (*fn_SerializeRefEntries_t)(void* serializer, int count, void* arrayBase);
static fn_SerializeRefEntries_t s_fn_SerializeRefEntries = nullptr;

// FUN_140002340 — serialize IBP array (for +0x60)
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

static bool s_initialized = false;

// ============================================================
// OrchardComponent::Shutdown (0x140051de0)
// ============================================================
// Component_Serialize + empty controller serializer + second serializer:
//   validate refs, serialize orchardFlag, orchardId, entity ref array, resource array

void OrchardComponent_Shutdown(OrchardComponent* self, void* stream) {
    uintptr_t selfAddr = (uintptr_t)self;

    if (s_fn_Component_Serialize)
        s_fn_Component_Serialize(self, stream);

    // Empty controller serializer block
    uint8_t serializer1[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer1, stream, 0);
    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer1);

    // Main data serializer
    uint8_t serializer2[24];
    if (s_fn_Serializer_Ctor) s_fn_Serializer_Ctor(serializer2, stream, 0);

    // Validate entity references
    if (s_fn_ValidateOrchardRefs)
        s_fn_ValidateOrchardRefs((void*)selfAddr);

    // Serialize orchardFlag (bool at +0x48)
    if (s_fn_SerializeBool)
        s_fn_SerializeBool(serializer2, (bool*)(selfAddr + 0x48), nullptr);

    // Serialize orchardId (int at +0x4C)
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, (int*)(selfAddr + 0x4C), nullptr);

    // Serialize entity ref count from +0x58, resize +0x50 if loading
    int localCount = *(int*)(selfAddr + 0x58);
    if (s_fn_SerializeInt)
        s_fn_SerializeInt(serializer2, &localCount, nullptr);

    if (*(int*)((uintptr_t)stream + 8) == 0 && s_fn_ResizeRefArray) {
        s_fn_ResizeRefArray((void*)(selfAddr + 0x50), localCount);
    }

    // Serialize entity ref array entries
    if (s_fn_SerializeRefEntries)
        s_fn_SerializeRefEntries(serializer2, localCount, *(void**)(selfAddr + 0x50));

    // Serialize resource IBP array at +0x60
    if (s_fn_SerializeIBPArray)
        s_fn_SerializeIBPArray((void*)(selfAddr + 0x60), serializer2);

    if (s_fn_Serializer_Dtor) s_fn_Serializer_Dtor(serializer2);
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void OrchardComponent_InitFuncs() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));
    s_fn_ValidateOrchardRefs = reinterpret_cast<fn_ValidateOrchardRefs_t>(
        ResolveAddr(Module::Exe, 0x140051830));
    s_fn_ResizeRefArray = reinterpret_cast<fn_ResizeRefArray_t>(
        ResolveAddr(Module::Exe, 0x140009f80));
    s_fn_SerializeRefEntries = reinterpret_cast<fn_SerializeRefEntries_t>(
        ResolveAddr(Module::Exe, 0x14000a230));
    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_SerializeBool = *reinterpret_cast<fn_SerializeBool_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_SerializeInt = *reinterpret_cast<fn_SerializeInt_t*>(slot);

    s_initialized = true;
}

// ===== Standalone factory (Foundation D — component instantiation) =====
#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "Component/component_factory_base.h"
COMPONENT_INERT_FACTORY(OrchardComponent, 0x68, 0x80)
REGISTER_TYPE(OrchardComponent, "ControllerComponent",
              &OrchardComponent_Factory, &OrchardComponent_CreateInPlace);
#endif // STANDALONE_EXE
