// src/game/Component/componentdescription.cpp
// Foundation C keystone — ComponentDescription (the entity-template container).
//
// ComponentDescription (0x38 bytes) extends Object::External. It is the top-level
// type of every entity Template (Template\Citizen.rsc etc.). Layout:
//   +0x00 vtable (0x140162a00)
//   +0x08 Object::External base
//   +0x20 int   category (default 6)
//   +0x28 void* subDescArray  — array of sub-description External* (stride 8)
//   +0x30 int   subDescCount
//   +0x34 int   subDescCap
//
// Loading a template: Factory creates a ComponentDescription, Serialize reads the
// category + sub-description count, resizes the array, then loads each sub-
// description as its own External resource via ExternalFactory::FindOrLoad
// (FUN_140032090). Each sub-description (CitizenDescription, ModelDescription, …)
// has its own factory/Serialize — see the other Foundation C files.
//
// Reimplemented for standalone (the original 0x14014ea90 / 0x140118af0 / 0x140032090
// aren't mapped). Mirrors mapgenerator.cpp (deserialize) + statgoal.cpp (scaffolding)
// + serialize_struct60b_serialize.cpp (the proven External-array FindOrLoad body).

#include "Component/ComponentDescription.h"
#include "Component/Description.h"   // the SerializeExternalArray<Description> instantiation
#include "object/TypeInfo.h"
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <new>
#include "system/MemoryAllocator.h"
#include "Component/exe_bodies.h"
#include "Object/NamedExternalRef.h"   // ResolveExternalByName_Impl's record


// ---- internal: retail's `*(int*)(*(serializer) + 8) == 0` is the stream's direction
// word — IO::Serializer::GetDirection (0x180006210) — 0 = reading.
static inline bool Reading(IO::Serializer& ser) {
    return static_cast<int>(ser.GetDirection()) == 0;
}


// ============================================================
// ComponentDescription — ENDGAME B.3.8 K1: a REAL class (ComponentDescription.h)
// with a compiler-generated 9-slot vtable. The hand-built 11-slot table (slot 8
// null, slot 9 past retail's end holding createInPlace) and its
// RegisterExeAddr(0x140162a00) row are gone: nothing in the tree reads that
// address back (B38_RULES D5).
// ============================================================
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"

// ctor (inlined into create 0x140031f30 / createInPlace 0x140031f90):
// `??0External@Object@@`, the stamp (the compiler's), then the defaults.
ComponentDescription::ComponentDescription()
    : Object::External(),
      _category(6),                 // default category
      _subDescs(nullptr),
      _subDescCount(0),
      _subDescCap(0) {}

// dtor (slot 0 0x140032030 minus the stamps): free the sub-description pointer
// array (the sub-descriptions themselves are owned by the ExternalFactory cache,
// not freed here). The compiler then runs ~External.
ComponentDescription::~ComponentDescription() {
    if (_subDescs) ::System::MemoryAllocator::MemFree(_subDescs);
    _subDescs = nullptr;
    _subDescCount = 0;
}

// [5] 0x140118af0.
bool ComponentDescription::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);

    // category enum (+0x20) through the UINT door. `serBuf + 8` was the
    // ReaderWriter, not an Encoding, and vtable +0x98 is rw_vtbl::kScalarUInt.
    ser.Serialize(_category, nullptr);   // 0x98 = uint door

    // sub-description count (+0x30) + fresh array resize (+0x28, stride 8)
    ser.Serialize(_subDescCount, nullptr);
    int count = _subDescCount;
    if (Reading(ser) && count > 0 && ::System::MemoryAllocator::_singleton) {
        _subDescs = static_cast<Description**>(::System::MemoryAllocator::MemAlloc((uint64_t)count * 8, 8));
        _subDescCap = count;
        if (_subDescs) for (int i = 0; i < count; ++i) _subDescs[i] = nullptr;
    }

    // load each sub-description: 0x140118b65 CALL 0x140032090, the ExternalPtr array
    // template's instantiation over the Description cell 0x1401c72e0 (B8 RES, row 1040).
    Object::SerializeExternalArray<Description>(ser, count, _subDescs);
    return true;
}

// [6] 0x140031fe0: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* ComponentDescription::Reconstruct() {
    this->~ComponentDescription();
    return new (static_cast<void*>(this)) ComponentDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void ComponentDescription::Destruct() { this->~ComponentDescription(); }

// [8] 0x140031f90: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* ComponentDescription::Construct() {
    return new (static_cast<void*>(this)) ComponentDescription();
}

// create 0x140031f30: MemAlloc(0x38, 8), then the (inlined) ctor.
::Object::Base* ComponentDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ComponentDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ComponentDescription());
}
// createInPlace 0x140031f90.
::Object::Base* ComponentDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ComponentDescription());
}

DEFINE_EXE_TYPE(ComponentDescription, "Object::External")

// moved from engine_funcs.h (ENDGAME B.3.9 TR): reads X::TypeId() of the cell's class.
//
// 0x14001b1d0 resolves a by-name ComponentDescription reference (Object/NamedExternalRef.h):
// the template's name, the ExternalFactory generation it was last resolved at, and the
// cached result.
void NamedExternalRef::ResolveDescription() {
    // 0x14001b1ec-0x14001b1f6: the factory singleton is read with no test (B8 RES, row 1064).
    ::Object::ExternalFactory* factory = ::Object::ExternalFactory::_singleton;   // ENDGAME B.3.9 SS2 (merged)

    // Check generation counter: if matches, cached value is still valid
    if (generation == factory->_generation) return;

    // Convert String to Filename, look up in factory; the Filename dies right after
    // the find (0x14001b223), before the type test.
    // ??0Filename@System@@QEAA@AEBVString@1@@Z  retail 0x1800c6360
    ::Object::External* found;
    {
        System::Filename filename(name);
        found = factory->FindAs<::Object::External>(filename);
    }

    // Keep the result only if it is a ComponentDescription (cell 0x1401c7318):
    // 0x14001b229 tests the find's result, then IsDerivedFrom (slot 0x140152a98).
    ::Object::External* result = nullptr;
    if (found && found->IsDerivedFrom(ComponentDescription::TypeId())) {
        result = found;
    }

    // Cache result, then the generation RE-READ after the find (0x14001b24d).
    resolved   = result;
    generation = factory->_generation;
}
