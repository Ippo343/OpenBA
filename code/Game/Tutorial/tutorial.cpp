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

#include "Tutorial/Tutorial.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "collection/Collection.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Exe helpers, resolved PER CALL (ENDGAME B.3.8, B6-5: the loader thread
// serializes tutorial resources; no first-call cache)
// ============================================================

// DynArray resize helper (0x140001bb0) -- resize 8-byte element array
typedef void (*fn_DynArrayResize_t)(void* arrayField, int newCount);
static fn_DynArrayResize_t DynArrayResizeFn() {
    return reinterpret_cast<fn_DynArrayResize_t>(ResolveAddr(Module::Exe, 0x140001bb0));
}

// Tutorial_helper_6fb0 (0x140026fb0) -- serialize array of ExternalPtr<TutorialStep>
typedef void (*fn_SerializeStepArray_t)(void* serializer, int count, void* arrayData, void* p4);
static fn_SerializeStepArray_t SerializeStepArrayFn() {
    return reinterpret_cast<fn_SerializeStepArray_t>(ResolveAddr(Module::Exe, 0x140026fb0));
}

// FUN_140002340 -- serialize the single ExternalPtr at +0x20
typedef void (*fn_ExternalPtrSerialize_Setup_t)(void* fieldPtr, void* serializer);
static fn_ExternalPtrSerialize_Setup_t ExternalPtrSerializeSetupFn() {
    return reinterpret_cast<fn_ExternalPtrSerialize_Setup_t>(ResolveAddr(Module::Exe, 0x140002340));
}

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400deb90 (inlined in create 0x140026e50 / createInPlace 0x140026eb0):
// External(), the stamp (the compiler's), then the fields.
Tutorial::Tutorial()
    : setup(nullptr), steps(nullptr), stepCount(0), currentStep(-1), _pad3C{} {}

// The dtor inside [0] 0x140026f50: free the steps array (+0x28 data, +0x30
// size, +0x34 capacity) — an inlined Collection::Array<T>::~Array (MemFree
// the buffer, zero _data, zero the _size/_capacity qword); no element is
// destroyed (8-byte ExternalPtr<TutorialStep> entries). The compiler then
// runs ~External.
Tutorial::~Tutorial() {
    reinterpret_cast<Collection::Array<void*>*>(&steps)->~Array();   // B.3.2 bridge
}

// [0] 0x140026f50: ~Tutorial, MemFree on &1.
Object::Base* Tutorial::DeletingDtor(unsigned flags) {
    this->~Tutorial();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// ============================================================
// Tutorial::Reconstruct [6] 0x140026f00 / Destruct [7] 0x140001640
// ============================================================
// DeletingDtor(0) (destroy in place), then the ctor on `this`.
Object::External* Tutorial::Reconstruct() {
    DeletingDtor(0);
    return ::new (static_cast<void*>(this)) Tutorial();
}
void Tutorial::Destruct() { DeletingDtor(0); }

// ============================================================
// Tutorial::Construct (vtable +0x40, 0x140026eb0)
// ============================================================
Object::External* Tutorial::Construct() {
    return ::new (static_cast<void*>(this)) Tutorial();
}

// ============================================================
// Tutorial::Serialize (vtable +0x28, 0x1400dead0)
// ============================================================
// Fields serialized:
//   1. int stepCount at +0x30
//   2. (on load) resize steps array at +0x28
//   3. ExternalPtr<TutorialStep> array via helper (0x140026fb0)
//   4. the ExternalPtr at +0x20 via helper (0x140002340)
//   5. int currentStep at +0x38
bool Tutorial::Serialize(void* stream) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);

    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // 1. Serialize step count (+0x30)
    int32_t localCount = *reinterpret_cast<int32_t*>(s + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&localCount), nullptr);   // B.3.4 bridge

    // 2. On load: resize steps array
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        if (auto resize = DynArrayResizeFn()) {
            resize(reinterpret_cast<void*>(s + 0x28), localCount);
        }
    }

    // 3. Serialize steps array (ExternalPtr<TutorialStep>[])
    void* stepsData = *reinterpret_cast<void**>(s + 0x28);
    if (auto serializeSteps = SerializeStepArrayFn()) {
        serializeSteps(serializer, localCount, stepsData, nullptr);
    }

    // 4. Serialize the ExternalPtr at +0x20
    if (auto serializeSetup = ExternalPtrSerializeSetupFn()) {
        serializeSetup(reinterpret_cast<void*>(s + 0x20), serializer);
    }

    // 5. Serialize currentStep at +0x38
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(currentStep, nullptr);   // B.3.4 bridge

    // Destroy Serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// create 0x140026e50: MemAlloc(0x40, 8), then the (inlined) ctor.
void* Tutorial::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Tutorial), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) Tutorial());
}
// createInPlace 0x140026eb0.
void* Tutorial::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) Tutorial());
}

DEFINE_EXE_TYPE(Tutorial, "Object::External")
