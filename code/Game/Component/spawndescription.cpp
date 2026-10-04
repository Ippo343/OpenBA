// code/Game/Component/spawndescription.cpp
// SpawnDescription (entity description block) — spawn-table parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a6f0. Parent "Description".
// Size 0x30. GetVersion=2.
//
// Layout: a growable array of 16-byte spawn entries.
//   void* array @ +0x20, int count @ +0x28, int capacity @ +0x2c
//   element { ExternalPtr @ +0x00 (8B), float @ +0x08, float @ +0x0c }
// Factory 0x14014b740-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   array @ +0x20 = 0, count/cap @ +0x28 = 0 (8-byte store)
// Serialize (vtable +0x28, 0x1400560b0), stream order:
//   int count @ +0x28
//   if stream-ok: resize array @ +0x20 to count (exe helper FUN_14000c520)
//   per element [0..count): ExternalPtr(elem+0) via fn_ExternalPtr_BuildSerialize
//     (FUN_140002340), float(elem+8), float(elem+0xc)
// Decompiled READ-ONLY from Application-x64.exe.
//
// ENDGAME B.3.8 K1: a REAL class (SpawnDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015a6f0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/SpawnDescription.h"
#include "Component/SpawnComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "io/Stream.h"   // IO::Stream::_direction
#include "Object/factory_registration.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <new>

namespace {
// serializer+0x00 is the IO::Stream; stream+0x08 is its error/direction word
// (0 == ok on the read leg). The tree's check before it allocates an array.
inline int SerStreamErr(IO::Serializer& ser) {
    return ser._stream->_direction;
}
}  // namespace

// ctor (inlined into create 0x14000c1d0 / createInPlace 0x14000c230):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
SpawnDescription::SpawnDescription()
    : Description(),
      _entries(nullptr),
      _entryCount(0),
      _entryCap(0) {}

// dtor (slot 0 0x14000c2e0 minus the stamps): frees the entry array buffer.
// The compiler then chains ~Description -> ~External.
SpawnDescription::~SpawnDescription() {
    if (_entries) System::MemoryAllocator::MemFree(_entries);
    _entries = nullptr;
}

// [5] 0x1400560b0: stream order as retail.
bool SpawnDescription::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);
    int count = _entryCount;
    ser.Serialize(count, nullptr);
    if (SerStreamErr(ser) == 0) {
        // exe-internal 16-byte-stride array resize (0x14000c520)
        Reimpl_Array_Resize_Stride10_Spawn(&_entries, count);   // header at +0x20 {ptr, count, cap}
    }
    if (count > 0) {
        Entry* elem = _entries;
        for (int i = 0; i < count && elem; ++i, ++elem) {
            Reimpl_ExternalPtr_Serialize_StringTable(&elem->_template, &ser);   // 0x140002340
            ser.Serialize(elem->_minSpawnAge, nullptr);
            ser.Serialize(elem->_maxSpawnAge, nullptr);
        }
    }
    return true;
}

// [6] 0x14000c280: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* SpawnDescription::Reconstruct() {
    this->~SpawnDescription();
    return new (static_cast<void*>(this)) SpawnDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void SpawnDescription::Destruct() { this->~SpawnDescription(); }

// [8] 0x14000c2d0: SpawnComponent's type id through the hidden return slot.
Object::Type* SpawnDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = SpawnComponent::TypeId();
    return ret;
}

// [9] 0x14000c230: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* SpawnDescription::Construct() {
    return new (static_cast<void*>(this)) SpawnDescription();
}

// create 0x14000c1d0: MemAlloc(0x30, 8), then the (inlined) ctor.
void* SpawnDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(SpawnDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) SpawnDescription());
}
// createInPlace 0x14000c230.
void* SpawnDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) SpawnDescription());
}

DEFINE_EXE_TYPE(SpawnDescription, "Description")
