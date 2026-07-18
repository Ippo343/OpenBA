// code/Game/Tutorial/tutorialstep.cpp
// Clean reimplementation of TutorialStep vtable methods.
//
// TutorialStep (0x70 bytes) -- tutorial step definition.
// Vtable: 0x140160e90, extends Object::External (0x20 bytes base).
//
// Fields (from Ghidra decompilation):
//   +0x20: bool     completed      -- step completion flag
//   +0x28: void*    stringTable    -- ExternalPtr<StringTable>
//   +0x30: void*    titleStr       -- System::String
//   +0x38: void*    descStr        -- System::String
//   +0x40: void*    images         -- DynArray<ExternalPtr<TutorialImage>> data
//   +0x48: int32_t  imageCount     -- array count
//   +0x50: void*    setups         -- DynArray<ExternalPtr<TutorialSetup>> data
//   +0x58: int32_t  setupCount     -- array count
//   +0x60: void*    goals          -- DynArray<ExternalPtr<TutorialGoal>> data
//   +0x68: int32_t  goalCount      -- array count
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorialstep_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// ============================================================
// Local function pointers (resolved lazily, not in engine_funcs.h)
// ============================================================

// External::External() constructor -- dispatch slot 0x140151e98
typedef void (*fn_External_Ctor_t)(void* self);
static fn_External_Ctor_t s_fn_External_Ctor = nullptr;

// Serializer::Serialize(bool*) -- dispatch slot 0x140152a58
typedef void (*fn_Serializer_SerializeBool_t)(void* serializer, void* boolPtr, const char* name);
static fn_Serializer_SerializeBool_t s_fn_Serializer_SerializeBool = nullptr;

// Serializer::Serialize(int*) -- dispatch slot 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* serializer, void* intPtr, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// Serializer::Ctor -- dispatch slot 0x140152ad8
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// Serializer::Dtor -- dispatch slot 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// ExternalPtr<StringTable>::Serialize (0x14000bf40)
typedef void (*fn_ExternalPtrSerialize_StringTable_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtrSerialize_StringTable_t s_fn_ExternalPtrSerialize_StringTable = nullptr;

// String::Serialize -- dispatch slot 0x140151238
typedef void (*fn_StringSerialize_t)(void* stringPtr, void* serializer);
static fn_StringSerialize_t s_fn_StringSerialize = nullptr;

// DynArray resize helper (0x140001bb0)
typedef void (*fn_DynArrayResize_t)(void* arrayField, int newCount);
static fn_DynArrayResize_t s_fn_DynArrayResize = nullptr;

// ExternalPtrArray<TutorialImage>::Serialize (0x140028cf0)
typedef void (*fn_SerializeImageArray_t)(void* serializer, int count, void* arrayData, void* p4);
static fn_SerializeImageArray_t s_fn_SerializeImageArray = nullptr;

// ExternalPtrArray<TutorialSetup>::Serialize (0x140028f40)
typedef void (*fn_SerializeSetupArray_t)(void* serializer, int count, void* arrayData, void* p4);
static fn_SerializeSetupArray_t s_fn_SerializeSetupArray = nullptr;

// ExternalPtrArray<TutorialGoal>::Serialize (0x140029190)
typedef void (*fn_SerializeGoalArray_t)(void* serializer, int count, void* arrayData, void* p4);
static fn_SerializeGoalArray_t s_fn_SerializeGoalArray = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    // External::External() constructor
    void** slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151e98));
    if (slot && *slot) s_fn_External_Ctor = reinterpret_cast<fn_External_Ctor_t>(*slot);

    // Serializer dispatch slots
    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152a58));
    if (slot && *slot) s_fn_Serializer_SerializeBool = reinterpret_cast<fn_Serializer_SerializeBool_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ac8));
    if (slot && *slot) s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad8));
    if (slot && *slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad0));
    if (slot && *slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(*slot);

    // String::Serialize dispatch slot
    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140151238));
    if (slot && *slot) s_fn_StringSerialize = reinterpret_cast<fn_StringSerialize_t>(*slot);

    // Direct exe functions
    s_fn_ExternalPtrSerialize_StringTable = reinterpret_cast<fn_ExternalPtrSerialize_StringTable_t>(
        ResolveAddr(Module::Exe, 0x14000bf40));
    s_fn_DynArrayResize = reinterpret_cast<fn_DynArrayResize_t>(
        ResolveAddr(Module::Exe, 0x140001bb0));
    s_fn_SerializeImageArray = reinterpret_cast<fn_SerializeImageArray_t>(
        ResolveAddr(Module::Exe, 0x140028cf0));
    s_fn_SerializeSetupArray = reinterpret_cast<fn_SerializeSetupArray_t>(
        ResolveAddr(Module::Exe, 0x140028f40));
    s_fn_SerializeGoalArray = reinterpret_cast<fn_SerializeGoalArray_t>(
        ResolveAddr(Module::Exe, 0x140029190));
}

// ============================================================
// TutorialStep_DtorBody (0x1400e33f0)
// ============================================================
// The actual destructor body -- frees arrays, destroys strings,
// calls External::~External.
static void TutorialStep_DtorBody(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialStepAddr::Vtable);

    // Set vtable
    *reinterpret_cast<uintptr_t*>(s) = vtable;

    // Free goals array at +0x60, zero +0x60 and +0x68
    void* goalsData = *reinterpret_cast<void**>(s + 0x60);
    if (goalsData && fn_MemFreeStatic) fn_MemFreeStatic(goalsData);
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x68) = 0;

    // Free setups array at +0x50, zero +0x50 and +0x58
    void* setupsData = *reinterpret_cast<void**>(s + 0x50);
    if (setupsData && fn_MemFreeStatic) fn_MemFreeStatic(setupsData);
    *reinterpret_cast<uint64_t*>(s + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;

    // Free images array at +0x40, zero +0x40 and +0x48
    void* imagesData = *reinterpret_cast<void**>(s + 0x40);
    if (imagesData && fn_MemFreeStatic) fn_MemFreeStatic(imagesData);
    *reinterpret_cast<uint64_t*>(s + 0x40) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;

    // Destroy String at +0x38 (descStr)
    if (fn_String_Dtor) fn_String_Dtor(reinterpret_cast<void*>(s + 0x38));

    // Destroy String at +0x30 (titleStr)
    if (fn_String_Dtor) fn_String_Dtor(reinterpret_cast<void*>(s + 0x30));

    // Call External::~External
    if (fn_External_Dtor) fn_External_Dtor(self);
}

// ============================================================
// TutorialStep_Dtor -- Slot0 (vtable +0x00, 0x140028cc0)
// ============================================================
// Scalar destructor: calls DtorBody, optionally frees memory.
void* TutorialStep_Dtor(void* self, uint32_t flags) {
    TutorialStep_DtorBody(self);

    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) fn_MemFreeStatic(self);
    }

    return self;
}

// ============================================================
// TutorialStep_Init -- Slot8 (vtable +0x40, 0x140028c90 -> 0x1400e3480)
// ============================================================
// Constructor/initializer: calls External::External(), sets vtable, zeros all fields.
void* TutorialStep_Init(void* self) {
    EnsureResolved();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Call External base constructor
    if (s_fn_External_Ctor) s_fn_External_Ctor(self);

    // Set vtable to TutorialStep
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialStepAddr::Vtable);
    *reinterpret_cast<uintptr_t*>(s) = vtable;

    // Zero all TutorialStep fields (+0x20 through +0x68)
    *reinterpret_cast<uint8_t*>(s + 0x20) = 0;   // completed
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;   // stringTable
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;   // titleStr
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;   // descStr
    *reinterpret_cast<uint64_t*>(s + 0x40) = 0;   // images data
    *reinterpret_cast<uint64_t*>(s + 0x48) = 0;   // imageCount
    *reinterpret_cast<uint64_t*>(s + 0x50) = 0;   // setups data
    *reinterpret_cast<uint64_t*>(s + 0x58) = 0;   // setupCount
    *reinterpret_cast<uint64_t*>(s + 0x60) = 0;   // goals data
    *reinterpret_cast<uint64_t*>(s + 0x68) = 0;   // goalCount

    return self;
}

// ============================================================
// TutorialStep_Cleanup -- Slot6 (vtable +0x30, 0x140028ca0)
// ============================================================
// Cleanup: calls scalar dtor with flags=0 (destroy, no free), then re-initializes.
void TutorialStep_Cleanup(void* self) {
    // Call scalar dtor with flags=0: destroys fields without freeing self
    // In the original, this calls vtable[0](self, 0), which is the scalar dtor.
    // We call TutorialStep_DtorBody directly (same effect as flags=0 scalar dtor).
    TutorialStep_DtorBody(self);

    // Re-initialize: the original tail-calls TutorialStep_Ctor (0x1400e3480)
    TutorialStep_Init(self);
}

// ============================================================
// TutorialStep_Serialize -- Slot5 (vtable +0x28, 0x1400e32a0)
// ============================================================
// Serializes all TutorialStep fields using a Serializer wrapper.
// Fields serialized:
//   1. bool at +0x20 (completed)
//   2. ExternalPtr<StringTable> at +0x28
//   3. String at +0x30 (title)
//   4. String at +0x38 (description)
//   5. int count at +0x48, then DynArray resize +0x40, then array serialize (images)
//   6. int count at +0x58, then DynArray resize +0x50, then array serialize (setups)
//   7. int count at +0x68, then DynArray resize +0x60, then array serialize (goals)
void TutorialStep_Serialize(void* self, void* stream) {
    EnsureResolved();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    } else {
        return; // Can't serialize without a serializer
    }

    // 1. Serialize bool at +0x20 (completed flag)
    if (s_fn_Serializer_SerializeBool) {
        s_fn_Serializer_SerializeBool(serializer, reinterpret_cast<void*>(s + 0x20), nullptr);
    }

    // 2. Serialize ExternalPtr<StringTable> at +0x28
    if (s_fn_ExternalPtrSerialize_StringTable) {
        s_fn_ExternalPtrSerialize_StringTable(reinterpret_cast<void*>(s + 0x28), serializer);
    }

    // 3. Serialize String at +0x30 (title)
    if (s_fn_StringSerialize) {
        s_fn_StringSerialize(reinterpret_cast<void*>(s + 0x30), serializer);
    }

    // 4. Serialize String at +0x38 (description)
    if (s_fn_StringSerialize) {
        s_fn_StringSerialize(reinterpret_cast<void*>(s + 0x38), serializer);
    }

    // 5. Images array (+0x40 data, +0x48 count)
    {
        int32_t count = *reinterpret_cast<int32_t*>(s + 0x48);
        int32_t localCount = count;
        if (s_fn_Serializer_SerializeInt) {
            s_fn_Serializer_SerializeInt(serializer, &localCount, nullptr);
        }
        // On load: check if reading (serializer->encoding->mode == 0)
        uintptr_t serPtr = *reinterpret_cast<uintptr_t*>(serializer);
        if (*reinterpret_cast<int32_t*>(serPtr + 8) == 0) {
            // Reading: resize the array
            if (s_fn_DynArrayResize) {
                s_fn_DynArrayResize(reinterpret_cast<void*>(s + 0x40), localCount);
            }
        }
        // Serialize each element
        void* arrayData = *reinterpret_cast<void**>(s + 0x40);
        if (s_fn_SerializeImageArray) {
            s_fn_SerializeImageArray(serializer, localCount, arrayData, nullptr);
        }
    }

    // 6. Setups array (+0x50 data, +0x58 count)
    {
        int32_t count = *reinterpret_cast<int32_t*>(s + 0x58);
        int32_t localCount = count;
        if (s_fn_Serializer_SerializeInt) {
            s_fn_Serializer_SerializeInt(serializer, &localCount, nullptr);
        }
        uintptr_t serPtr = *reinterpret_cast<uintptr_t*>(serializer);
        if (*reinterpret_cast<int32_t*>(serPtr + 8) == 0) {
            if (s_fn_DynArrayResize) {
                s_fn_DynArrayResize(reinterpret_cast<void*>(s + 0x50), localCount);
            }
        }
        void* arrayData = *reinterpret_cast<void**>(s + 0x50);
        if (s_fn_SerializeSetupArray) {
            s_fn_SerializeSetupArray(serializer, localCount, arrayData, nullptr);
        }
    }

    // 7. Goals array (+0x60 data, +0x68 count)
    {
        int32_t count = *reinterpret_cast<int32_t*>(s + 0x68);
        int32_t localCount = count;
        if (s_fn_Serializer_SerializeInt) {
            s_fn_Serializer_SerializeInt(serializer, &localCount, nullptr);
        }
        uintptr_t serPtr = *reinterpret_cast<uintptr_t*>(serializer);
        if (*reinterpret_cast<int32_t*>(serPtr + 8) == 0) {
            if (s_fn_DynArrayResize) {
                s_fn_DynArrayResize(reinterpret_cast<void*>(s + 0x60), localCount);
            }
        }
        void* arrayData = *reinterpret_cast<void**>(s + 0x60);
        if (s_fn_SerializeGoalArray) {
            s_fn_SerializeGoalArray(serializer, localCount, arrayData, nullptr);
        }
    }

    // Destroy serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160e90 (9 slots, Object::External base), size 0x70, parent "Object::External".
// GetFileVersion (+0x18) = 1. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TutorialStep_GetTypeId_SA(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("TutorialStep");
    return c;
}
static int TutorialStep_GetFileVersion(void*) { return 1; }

static void* g_TutorialStep_Vtable[9] = {
    (void*)&TutorialStep_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&TutorialStep_GetTypeId_SA,       // +0x10 GetTypeId
    (void*)&TutorialStep_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&TutorialStep_Serialize,       // +0x28 Serialize
    (void*)&TutorialStep_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&TutorialStep_Init,              // +0x40 Ctor/CreateInPlace
};

extern "C" void RegisterTutorialStepVtable() {
    RegisterExeAddr(0x140160e90, (uintptr_t)g_TutorialStep_Vtable);
}
extern "C" void* TutorialStep_CreateInPlace(void* mem) {
    return TutorialStep_Init(mem);
}
extern "C" void* TutorialStep_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x70, 8);
    return p ? TutorialStep_Init(p) : nullptr;
}
REGISTER_TYPE(TutorialStep, "Object::External", &TutorialStep_Factory, &TutorialStep_CreateInPlace);
