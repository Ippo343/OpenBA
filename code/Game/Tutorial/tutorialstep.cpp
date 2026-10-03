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

#include "Tutorial/TutorialStep.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "system/String.h"
#include "collection/Collection.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include <new>

// ============================================================
// Exe helpers, called by name: 0x14000bf40 ExternalPtr<StringTable>::Serialize
// (Reimpl_ExternalPtr_Serialize_StringTable) and 0x140001bb0 the DynArray resize
// (Reimpl_Array_Resize_Stride08), Game/exe_bodies.h; the three ExternalPtrArray
// serializers (TutBase_Serialize*Array), Tutorial/exe_bodies.h.
// ============================================================

// ============================================================
// Construction / destruction (ENDGAME B.3.8 block GOAL)
// ============================================================
// ctor 0x1400e3480: External() (the base's real ctor), the stamp (the
// compiler's), then every field zeroed (+0x20 .. +0x68).
TutorialStep::TutorialStep()
    : completed(false), _pad21{}, stringTable(nullptr), titleStr(nullptr), descStr(nullptr),
      images(nullptr), imageCount(0), imageCapacity(0),
      setups(nullptr), setupCount(0), setupCapacity(0),
      goals(nullptr), goalCount(0), goalCapacity(0) {}

// The dtor body 0x1400e33f0: free the goals (+0x60), setups (+0x50) and images
// (+0x40) arrays in retail's order — each an inlined Collection::Array<T>::~Array
// (MemFree the buffer, zero _data, zero the _size/_capacity qword; no element
// destroy for the 8-byte ExternalPtr<T> entries) — then ~String on the
// description (+0x38) and the title (+0x30). The compiler runs ~External.
TutorialStep::~TutorialStep() {
    reinterpret_cast<Collection::Array<void*>*>(&goals)->~Array();    // B.3.2 bridge
    reinterpret_cast<Collection::Array<void*>*>(&setups)->~Array();   // B.3.2 bridge
    reinterpret_cast<Collection::Array<void*>*>(&images)->~Array();   // B.3.2 bridge
    DescText().~String();
    TitleText().~String();
}

// ============================================================
// TutorialStep::Reconstruct [6] 0x140028ca0 / Destruct [7] / Construct [8]
// ============================================================
// Reconstruct: `~T()` (slot 0, flags 0) (the tree called the dtor body directly — the
// same effect), then the ctor 0x1400e3480 on `this`.
Object::External* TutorialStep::Reconstruct() {
    this->~TutorialStep();
    return ::new (static_cast<void*>(this)) TutorialStep();
}
void TutorialStep::Destruct() { this->~TutorialStep(); }
Object::External* TutorialStep::Construct() {                     // 0x140028c90
    return ::new (static_cast<void*>(this)) TutorialStep();
}

// ============================================================
// TutorialStep::Serialize (vtable +0x28, 0x1400e32a0)
// ============================================================
// Fields serialized:
//   1. bool at +0x20 (completed)
//   2. ExternalPtr<StringTable> at +0x28
//   3. String at +0x30 (title)
//   4. String at +0x38 (description)
//   5. int count at +0x48, then DynArray resize +0x40, then array serialize (images)
//   6. int count at +0x58, then DynArray resize +0x50, then array serialize (setups)
//   7. int count at +0x68, then DynArray resize +0x60, then array serialize (goals)
// LEFT H5: the two Strings are raw storage (the ctor stores 0 inline, the dtor
// destroys them by hand).
System::String& TutorialStep::TitleText() {
    return *reinterpret_cast<System::String*>(&titleStr);
}
System::String& TutorialStep::DescText() {
    return *reinterpret_cast<System::String*>(&descStr);
}

bool TutorialStep::Serialize(IO::Stream& stream) {
    // The serializer.
    IO::Serializer serializer(stream, 0);

    // 1. Serialize bool at +0x20 (completed flag)
    serializer.Serialize(completed, nullptr);

    // 2. Serialize ExternalPtr<StringTable> at +0x28
    Reimpl_ExternalPtr_Serialize_StringTable(&stringTable, &serializer);   // 0x14000bf40

    // 3. / 4. Serialize the title (+0x30) and description (+0x38) Strings
    TitleText().Serialize(&serializer);
    DescText().Serialize(&serializer);

    // 5. / 6. / 7. The three ExternalPtr arrays: count, resize on load, elements.
    auto leg = [&serializer](auto& data, int32_t count,
                             void (*serializeArray)(void* serializer, int count, void* arrayData, void* p4)) {
        int32_t localCount = count;
        serializer.Serialize(localCount, nullptr);
        // On load (encoding mode 0): resize the array
        if (static_cast<int>(serializer.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
            Reimpl_Array_Resize_Stride08(&data, localCount);   // 0x140001bb0
        }
        serializeArray(&serializer, localCount, data, nullptr);
    };
    leg(images, imageCount, &TutBase_SerializeImageArray);   // images  0x140028cf0
    leg(setups, setupCount, &TutBase_SerializeSetupArray);   // setups  0x140028f40
    leg(goals,  goalCount,  &TutBase_SerializeGoalArray);    // goals   0x140029190
    return true;
}

void* TutorialStep::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TutorialStep), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) TutorialStep());
}
// createInPlace 0x140028c90.
void* TutorialStep::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) TutorialStep());
}

DEFINE_EXE_TYPE(TutorialStep, "Object::External")
