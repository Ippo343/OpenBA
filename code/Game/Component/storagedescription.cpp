// code/Game/Component/storagedescription.cpp
// Clean reimplementation of StorageDescription (Foundation C).
//
// StorageDescription (0x40 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x1401647a0 (exe symbol PTR_StorageComponent_vslot16_1401647a0).
// BaseTypeInfo @ 0x1401c7eb0, parent "Description", file version 2.
//
// Field layout (from StorageDescription_Factory @ exe and Serialize @ 0x14012c9a0):
//   +0x20  bool   flagA               (default 0)
//   +0x21  bool   flagB               (default 0)
//   +0x24  int32  pickupValue         (default 0)
//   +0x28  int32  rawMaterialFlags    (enum; serialized via serializer vtable+0x98)
//   +0x30  void*  volumeArray         (dyn-array of 0x10-stride entries)
//   +0x38  int32  volumeCount
//
// The +0x30 array element layout (stride 0x10):
//   elem+0x00 : External* ref  (ExternalPtr; serialized via fn_ExternalPtr_Serialize2)
//   elem+0x08 : int32          (serialized as int)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (StorageDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401647a0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/StorageDescription.h"
#include "Component/StorageComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <new>

namespace {
// {data, count, cap} header: set count = cap = n and allocate n * stride (8-aligned).
void* AllocArray(void* header, int count, int stride) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    *(int32_t*)(h + 8) = count;
    *(int32_t*)(h + 0xc) = count;
    void* buf = (count > 0)
                ? System::MemoryAllocator::MemAlloc((uint64_t)count * (uint32_t)stride, 8)
                : nullptr;
    *(void**)h = buf;
    return buf;
}
}  // namespace

// ctor (inlined into create 0x140038bb0 / createInPlace 0x140038c20):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
StorageDescription::StorageDescription()
    : Description(),
      _flagA(false),
      _flagB(false),
      _pickupValue(0),
      _rawMaterialFlags(0),
      _volumes(nullptr),
      _volumeCount(0),
      _volumeCap(0) {}

// dtor (slot 0 0x140038cf0 minus the stamps): frees the volume array.
// The compiler then chains ~Description -> ~External.
StorageDescription::~StorageDescription() {
    if (_volumes) System::MemoryAllocator::MemFree(_volumes);
    _volumes = nullptr;
    _volumeCount = 0;
    _volumeCap = 0;
}

// [5] 0x14012c9a0: stream order as retail.
bool StorageDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_rawMaterialFlags, nullptr);   // rawMaterialFlags (enum, uint door)
    ser.Serialize(_flagA, nullptr);
    ser.Serialize(_flagB, nullptr);
    ser.Serialize(_pickupValue, nullptr);
    
    ser.Serialize(_volumeCount, nullptr);   // volumeCount
    int count = _volumeCount;
    
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(&_volumes, count, 0x10));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x10)       = 0;          // External* ref
            *(uint64_t*)(e + i * 0x10 + 8)   = 0;          // int (+pad)
        }
    } else {
        e = static_cast<uint8_t*>(_volumes);
    }
    for (int i = 0; i < count; ++i, e += 0x10) {
        ser.Serialize(*reinterpret_cast<int*>(e + 8), nullptr);   // int field, elem+8
        Reimpl_ExternalPtr_Serialize_StringTable(e, &ser);                        // External* ref (elem+0)
    }
    return true;
}

// [6] 0x140038c80: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* StorageDescription::Reconstruct() {
    this->~StorageDescription();
    return new (static_cast<void*>(this)) StorageDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void StorageDescription::Destruct() { this->~StorageDescription(); }

// [8] 0x140038ce0: StorageComponent's type id through the hidden return slot.
Object::Type* StorageDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = StorageComponent::TypeId();
    return ret;
}

// [9] 0x140038c20: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* StorageDescription::Construct() {
    return new (static_cast<void*>(this)) StorageDescription();
}

// create 0x140038bb0: MemAlloc(0x40, 8), then the (inlined) ctor.
void* StorageDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(StorageDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) StorageDescription());
}
// createInPlace 0x140038c20.
void* StorageDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) StorageDescription());
}

DEFINE_EXE_TYPE(StorageDescription, "Description")
