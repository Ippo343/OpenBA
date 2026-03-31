// code/Game/Component/cropfieldcomponent.cpp
// Clean reimplementation of CropFieldComponent lifecycle methods from Application-x64.exe.
//
// CropFieldComponent (0x68 bytes) extends ControllerComponent with crop flags
// and entity reference arrays for field tiles.
//
// Vtable at 0x140162ec0. One non-trivial hookable override:
//   Shutdown (0x14011ae70) — Component_Serialize + empty serializer + versioned block
//                            with entity ref validation, crop flags, IBP array, entity list
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Component/cropfieldcomponent_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Resolved helpers
// ============================================================

// Component_Serialize at 0x1401189b0
typedef void (*fn_Component_Serialize_t)(void* self, void* stream);
static fn_Component_Serialize_t s_fn_Component_Serialize = nullptr;

// IO::Serializer ctor/dtor via dispatch slots
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// IO::Serializer::Serialize(this, bool*, name)
typedef void (*fn_Serializer_SerializeBool_t)(void* self, bool* value, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// IO::Serializer::Serialize(this, int*, name)
typedef void (*fn_Serializer_SerializeInt_t)(void* self, int* value, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// FUN_14011aa20 — validate entity references
typedef void (*fn_ValidateEntityRefs_t)(uintptr_t self);
static fn_ValidateEntityRefs_t s_fn_ValidateEntityRefs = nullptr;

// FUN_140002340 — serialize InternalBasePtr array
typedef void (*fn_SerializeIBPArray_t)(void* arrayPtr, void* serializer);
static fn_SerializeIBPArray_t s_fn_SerializeIBPArray = nullptr;

// CropFieldComponent_helper_35b0 — resize entity list
typedef void (*fn_ResizeEntityList_t)(void* arrayPtr, int newSize);
static fn_ResizeEntityList_t s_fn_ResizeEntityList = nullptr;

// CropFieldComponent_helper_36c0 — serialize entity list entries
typedef void (*fn_SerializeEntityList_t)(void* serializer, int count, uintptr_t arrayPtr);
static fn_SerializeEntityList_t s_fn_SerializeEntityList = nullptr;

static bool s_initialized = false;

static void EnsureHelpers() {
    if (s_initialized) return;

    s_fn_Component_Serialize = reinterpret_cast<fn_Component_Serialize_t>(
        ResolveAddr(Module::Exe, 0x1401189b0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x140152ad8);
    if (slot) s_fn_Serializer_Ctor = *reinterpret_cast<fn_Serializer_Ctor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ad0);
    if (slot) s_fn_Serializer_Dtor = *reinterpret_cast<fn_Serializer_Dtor_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152a58);
    if (slot) s_fn_Serializer_SerializeBool = *reinterpret_cast<fn_Serializer_SerializeBool_t*>(slot);

    slot = ResolveAddr(Module::Exe, 0x140152ac8);
    if (slot) s_fn_Serializer_SerializeInt = *reinterpret_cast<fn_Serializer_SerializeInt_t*>(slot);

    s_fn_ValidateEntityRefs = reinterpret_cast<fn_ValidateEntityRefs_t>(
        ResolveAddr(Module::Exe, 0x14011aa20));

    s_fn_SerializeIBPArray = reinterpret_cast<fn_SerializeIBPArray_t>(
        ResolveAddr(Module::Exe, 0x140002340));

    s_fn_ResizeEntityList = reinterpret_cast<fn_ResizeEntityList_t>(
        ResolveAddr(Module::Exe, 0x1400035b0));

    s_fn_SerializeEntityList = reinterpret_cast<fn_SerializeEntityList_t>(
        ResolveAddr(Module::Exe, 0x1400036c0));

    s_initialized = true;
}

// ============================================================
// CropFieldComponent::Shutdown (vtable slot [3], 0x14011ae70)
// ============================================================
// Complex entity reference serialization:
//   1. Component_Serialize (base)
//   2. Empty serializer (v0, immediately destroyed)
//   3. Main serializer (v0):
//      a. FUN_14011aa20 — validate entity refs
//      b. Serialize cropFlags as 2 bools (+0x48, +0x49)
//      c. FUN_140002340 — serialize IBP array at +0x50
//      d. Serialize entity count from +0x60
//      e. On load: resize entity list at +0x58
//      f. CropFieldComponent_helper_36c0 — serialize entity list entries

void CropFieldComponent_Shutdown(CropFieldComponent* self, void* stream) {
    EnsureHelpers();

    // Base Component serialization
    if (s_fn_Component_Serialize) {
        s_fn_Component_Serialize(self, stream);
    }

    // Empty serializer block (version 0, immediately destroyed)
    uint8_t emptySerializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(emptySerializer, stream, 0);
    }
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(emptySerializer);
    }

    // Main serializer block (version 0)
    uint8_t serializer[16];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    }

    // Validate entity references
    if (s_fn_ValidateEntityRefs) {
        s_fn_ValidateEntityRefs((uintptr_t)self);
    }

    // Serialize cropFlags as two bools (+0x48 and +0x49)
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer, (bool*)((uintptr_t)self + 0x48), nullptr);
        s_fn_Serializer_SerializeBool(serializer, (bool*)((uintptr_t)self + 0x49), nullptr);
    }

    // Serialize InternalBasePtr array at +0x50
    if (s_fn_SerializeIBPArray) {
        s_fn_SerializeIBPArray((void*)((uintptr_t)self + 0x50), serializer);
    }

    // Serialize entity count from +0x60
    int entityCount = *(int*)((uintptr_t)self + 0x60);
    int countBuf = entityCount;
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, &countBuf, nullptr);
    }

    // On load (serializer mode == 0), resize entity list
    if (*(int*)((uintptr_t)serializer + 8) == 0) {
        if (s_fn_ResizeEntityList) {
            s_fn_ResizeEntityList((void*)((uintptr_t)self + 0x58), countBuf);
        }
    }

    // Serialize entity list entries
    if (s_fn_SerializeEntityList) {
        uintptr_t entityArray = *(uintptr_t*)((uintptr_t)self + 0x58);
        s_fn_SerializeEntityList(serializer, countBuf, entityArray);
    }

    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Init: resolve function pointers
// ============================================================
void CropFieldComponent_InitFuncs() {
    EnsureHelpers();
#ifdef _DEBUG
    printf("[CropFieldComponent] Funcs resolved: Component_Serialize=%p ValidateEntityRefs=%p "
           "SerializeIBPArray=%p ResizeEntityList=%p SerializeEntityList=%p\n",
           (void*)s_fn_Component_Serialize, (void*)s_fn_ValidateEntityRefs,
           (void*)s_fn_SerializeIBPArray, (void*)s_fn_ResizeEntityList,
           (void*)s_fn_SerializeEntityList);
#endif
}
