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
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "collection/Collection.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Exe helpers, called by name: 0x140001bb0 DynArray resize (Reimpl_Array_Resize_Stride08)
// and 0x140002340 ExternalPtr serialize (Reimpl_ExternalPtr_Serialize_StringTable, the
// body registered there), Game/exe_bodies.h; 0x140026fb0 TutBase_SerializeStepArray,
// Tutorial/exe_bodies.h.
// ============================================================

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400deb90 (inlined in create 0x140026e50 / createInPlace 0x140026eb0):
// External(), the stamp (the compiler's), then the fields.
Tutorial::Tutorial()
    : setup(nullptr), steps(nullptr), stepCount(0), stepCapacity(0), currentStep(-1), _pad3C{} {}

// The dtor inside [0] 0x140026f50: free the steps array (+0x28 data, +0x30
// size, +0x34 capacity) — an inlined Collection::Array<T>::~Array (MemFree
// the buffer, zero _data, zero the _size/_capacity qword); no element is
// destroyed (8-byte ExternalPtr<TutorialStep> entries). The compiler then
// runs ~External.
Tutorial::~Tutorial() {
    reinterpret_cast<Collection::Array<void*>*>(&steps)->~Array();   // B.3.2 bridge
}

// ============================================================
// Tutorial::Reconstruct [6] 0x140026f00 / Destruct [7] 0x140001640
// ============================================================
// `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor on `this`.
Object::External* Tutorial::Reconstruct() {
    this->~Tutorial();
    return ::new (static_cast<void*>(this)) Tutorial();
}
void Tutorial::Destruct() { this->~Tutorial(); }

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
    // The serializer. LEFT: External::Serialize hands the stream in as void*
    // (the runtime base's signature).
    IO::Serializer serializer(*reinterpret_cast<IO::Stream*>(stream), 0);

    // 1. Serialize step count (+0x30)
    int32_t localCount = stepCount;
    serializer.Serialize(localCount, nullptr);

    // 2. On load: resize steps array
    if (static_cast<int>(serializer.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        Reimpl_Array_Resize_Stride08(&steps, localCount);   // 0x140001bb0
    }

    // 3. Serialize steps array (ExternalPtr<TutorialStep>[])
    TutBase_SerializeStepArray(&serializer, localCount, steps, nullptr);   // 0x140026fb0

    // 4. Serialize the ExternalPtr at +0x20
    Reimpl_ExternalPtr_Serialize_StringTable(&setup, &serializer);   // 0x140002340

    // 5. Serialize currentStep at +0x38
    serializer.Serialize(currentStep, nullptr);
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
