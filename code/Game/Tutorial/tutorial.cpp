// code/Game/Tutorial/tutorial.cpp
// Clean reimplementation of Tutorial vtable methods.
//
// Tutorial (0x40 bytes), vtable at 0x140160740.
// Extends Object::External (0x20 bytes base).
//
// Fields:
//   +0x20: void* setup       — ExternalPtr to TutorialSetup
//   +0x28: void* steps       — array of ExternalPtr to TutorialStep
//   +0x30: int64  stepCount  — number of steps (count + capacity packed)
//   +0x38: int32  currentStep — current step index (-1 = none)
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Tutorial/tutorial_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include <cstdint>

// ============================================================
// Local function pointers (resolved lazily)
// ============================================================

// Serializer::Ctor -- dispatch slot 0x140152ad8
typedef void (*fn_Serializer_Ctor_t)(void* self, void* stream, int version);
static fn_Serializer_Ctor_t s_fn_Serializer_Ctor = nullptr;

// Serializer::Dtor -- dispatch slot 0x140152ad0
typedef void (*fn_Serializer_Dtor_t)(void* self);
static fn_Serializer_Dtor_t s_fn_Serializer_Dtor = nullptr;

// Serializer::Serialize(int*) -- dispatch slot 0x140152ac8
typedef void (*fn_Serializer_SerializeInt_t)(void* serializer, void* intPtr, const char* name);
static fn_Serializer_SerializeInt_t s_fn_Serializer_SerializeInt = nullptr;

// DynArray resize helper (0x140001bb0) -- resize 8-byte element array
typedef void (*fn_DynArrayResize_t)(void* arrayField, int newCount);
static fn_DynArrayResize_t s_fn_DynArrayResize = nullptr;

// Tutorial_helper_6fb0 (0x140026fb0) -- serialize array of ExternalPtr<TutorialStep>
typedef void (*fn_SerializeStepArray_t)(void* serializer, int count, void* arrayData, void* p4);
static fn_SerializeStepArray_t s_fn_SerializeStepArray = nullptr;

// FUN_140002340 -- serialize single ExternalPtr<TutorialSetup>
typedef void (*fn_ExternalPtrSerialize_Setup_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtrSerialize_Setup_t s_fn_ExternalPtrSerialize_Setup = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;
    s_resolved = true;

    void** slot;

    // Serializer dispatch slots
    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad8));
    if (slot && *slot) s_fn_Serializer_Ctor = reinterpret_cast<fn_Serializer_Ctor_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ad0));
    if (slot && *slot) s_fn_Serializer_Dtor = reinterpret_cast<fn_Serializer_Dtor_t>(*slot);

    slot = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140152ac8));
    if (slot && *slot) s_fn_Serializer_SerializeInt = reinterpret_cast<fn_Serializer_SerializeInt_t>(*slot);

    // Direct exe functions
    s_fn_DynArrayResize = reinterpret_cast<fn_DynArrayResize_t>(
        ResolveAddr(Module::Exe, 0x140001bb0));
    s_fn_SerializeStepArray = reinterpret_cast<fn_SerializeStepArray_t>(
        ResolveAddr(Module::Exe, 0x140026fb0));
    s_fn_ExternalPtrSerialize_Setup = reinterpret_cast<fn_ExternalPtrSerialize_Setup_t>(
        ResolveAddr(Module::Exe, 0x140002340));
}

// ============================================================
// Tutorial::Dtor (vtable +0x00, 0x140026f50)
// ============================================================
// Scalar deleting destructor.
// 1. Set vtable to Tutorial
// 2. Free steps array at +0x28
// 3. Zero +0x28 and +0x30
// 4. Call External::~External(self)
// 5. If (flags & 1), free self
// 6. Return self
void* Tutorial_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Free the steps array (+0x28 data, +0x30 size, +0x34 capacity).
    // ENDGAME B.3.2 step 2: this WAS an inlined Collection::Array<T>::~Array —
    // MemFree the buffer (null-guarded on both sides), zero _data, zero the
    // _size/_capacity qword. Retail destroys no element here (the 8-byte
    // ExternalPtr<TutorialStep> entries have no destroy arm), so the trait-derived
    // Array<void*> row (destroy == nullptr) is that body exactly.
    // B.3.2 bridge: Tutorial is still an untyped exe struct.
    reinterpret_cast<Collection::Array<void*>*>(s + 0x28)->~Array();

    // Call External::~External
    if (fn_External_Dtor) {
        fn_External_Dtor(self);
    }

    // Conditional free
    if ((flags & 1) != 0) {
        if (fn_MemFreeStatic) {
            fn_MemFreeStatic(self);
        }
    }

    return self;
}

// ============================================================
// Tutorial::Cleanup (vtable +0x30, 0x140026f00)
// ============================================================
// Cleanup and reinitialize.
// 1. Call self->vtable[0](self, 0) — Dtor with flags=0 (cleanup only, no free)
// 2. Call External::External(self) — reinitialize base
// 3. Set vtable to Tutorial
// 4. Zero all Tutorial fields
void* Tutorial_Cleanup(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Call Dtor(self, 0) through vtable — this cleans up derived class fields too
    typedef void* (*fn_ScalarDtor_t)(void* self, uint32_t flags);
    uintptr_t vtablePtr = *(uintptr_t*)s;
    fn_ScalarDtor_t dtor = *(fn_ScalarDtor_t*)vtablePtr;
    dtor(self, 0);

    // Reinitialize External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Zero Tutorial fields
    *(uint64_t*)(s + 0x20) = 0;  // setup
    *(uint64_t*)(s + 0x28) = 0;  // steps array
    *(uint64_t*)(s + 0x30) = 0;  // step count
    *(int32_t*)(s + 0x38) = -1;  // current step index

    return self;
}

// ============================================================
// Tutorial::Ctor (vtable +0x40, 0x140026eb0)
// ============================================================
// Constructor.
// 1. Null check self
// 2. Call External::External(self) — initialize base
// 3. Set vtable to Tutorial
// 4. Zero all Tutorial fields
void* Tutorial_Ctor(void* self) {
    if (!self) return nullptr;

    uintptr_t s = (uintptr_t)self;

    // Initialize External base
    if (fn_External_External) {
        fn_External_External(self);
    }

    // Set vtable to Tutorial
    uintptr_t vtable = ResolveAddr(Module::Exe, TutorialAddr::Vtable);
    *(uintptr_t*)s = vtable;

    // Zero Tutorial fields
    *(uint64_t*)(s + 0x20) = 0;  // setup
    *(uint64_t*)(s + 0x28) = 0;  // steps array
    *(uint64_t*)(s + 0x30) = 0;  // step count
    *(int32_t*)(s + 0x38) = -1;  // current step index

    return self;
}

// ============================================================
// Tutorial::Serialize (vtable +0x28, 0x1400dead0)
// ============================================================
// Serializes all Tutorial fields using a Serializer wrapper.
// Fields serialized:
//   1. int stepCount at +0x30
//   2. (on load) resize steps array at +0x28
//   3. ExternalPtr<TutorialStep> array via helper (0x140026fb0)
//   4. ExternalPtr<TutorialSetup> at +0x20 via helper (0x140002340)
//   5. int currentStep at +0x38
void Tutorial_Serialize(void* self, void* stream) {
    EnsureResolved();

    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (s_fn_Serializer_Ctor) {
        s_fn_Serializer_Ctor(serializer, stream, 0);
    } else {
        return;
    }

    // 1. Serialize step count (+0x30)
    int32_t localCount = *reinterpret_cast<int32_t*>(s + 0x30);
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, &localCount, nullptr);
    }

    // 2. On load: resize steps array
    uintptr_t serPtr = *reinterpret_cast<uintptr_t*>(serializer);
    if (*reinterpret_cast<int32_t*>(serPtr + 8) == 0) {
        // Reading mode — resize array to match deserialized count
        if (s_fn_DynArrayResize) {
            s_fn_DynArrayResize(reinterpret_cast<void*>(s + 0x28), localCount);
        }
    }

    // 3. Serialize steps array (ExternalPtr<TutorialStep>[])
    void* stepsData = *reinterpret_cast<void**>(s + 0x28);
    if (s_fn_SerializeStepArray) {
        s_fn_SerializeStepArray(serializer, localCount, stepsData, nullptr);
    }

    // 4. Serialize setup ExternalPtr at +0x20
    if (s_fn_ExternalPtrSerialize_Setup) {
        s_fn_ExternalPtrSerialize_Setup(reinterpret_cast<void*>(s + 0x20), serializer);
    }

    // 5. Serialize currentStep at +0x38
    if (s_fn_Serializer_SerializeInt) {
        s_fn_Serializer_SerializeInt(serializer, reinterpret_cast<void*>(s + 0x38), nullptr);
    }

    // Destroy Serializer
    if (s_fn_Serializer_Dtor) {
        s_fn_Serializer_Dtor(serializer);
    }
}

// ============================================================
// Standalone factory + vtable — External-resource family (factory cascade).
// Vtable @ 0x140160740 (9 slots, Object::External base), size 0x40, parent "Object::External".
// GetFileVersion (+0x18) = 2. MusicList pattern (musiclist.cpp).
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t Tutorial_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("Tutorial");
    return c;
}
static int Tutorial_GetFileVersion(void*) { return 2; }

static void* g_Tutorial_Vtable[9] = {
    (void*)&Tutorial_Dtor,            // +0x00 Dtor
    nullptr,                      // +0x08 (base thunk)
    (void*)&Tutorial_GetTypeId,       // +0x10 GetTypeId
    (void*)&Tutorial_GetFileVersion,  // +0x18 GetFileVersion
    nullptr,                      // +0x20 (shared base thunk 0x14003a912)
    (void*)&Tutorial_Serialize,       // +0x28 Serialize
    (void*)&Tutorial_Cleanup,         // +0x30 Cleanup
    nullptr,                      // +0x38 (base 0x140001640)
    (void*)&Tutorial_Ctor,              // +0x40 Ctor/CreateInPlace
};

extern "C" void RegisterTutorialVtable() {
    RegisterExeAddr(0x140160740, (uintptr_t)g_Tutorial_Vtable);
}
extern "C" void* Tutorial_CreateInPlace(void* mem) {
    return Tutorial_Ctor(mem);
}
extern "C" void* Tutorial_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    return p ? Tutorial_Ctor(p) : nullptr;
}
REGISTER_TYPE(Tutorial, "Object::External", &Tutorial_Factory, &Tutorial_CreateInPlace);
