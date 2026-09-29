// code/Game/Component/mapdescription.cpp
// Clean reimplementation of MapDescription (entity description block).
//
// MapDescription (0x38 bytes) — minimap / map-overlay parameters for a building.
// Extends Object::External (0x20 base). Vtable at 0x140163b20.
// Parent type "Description".
//
// Factory  : 0x140035760  (MemAlloc 0x38 + ctor sub_4670 0x140124670)
// CreateInPlace: 0x1400357a0
// Ctor defaults (from MapDescription_sub_4670):
//   int    @ +0x20 = 2          (enum: overlay type)
//   String @ +0x28 = 0          (empty)
//   short  @ +0x30 = 0
//   byte   @ +0x32 = 0
//   int    @ +0x34 = 0xff000000 (ARGB Color, opaque black)
// Serialize (vtable +0x28, 0x140124560), stream order:
//   enum   @ +0x20   (serializer-encoding vtable +0x98 dispatch)
//   String @ +0x28
//   bool   @ +0x30
//   bool   @ +0x31
//   bool   @ +0x32
//   Color  @ +0x34   (System::Color::Serialize, 4-byte ARGB)
// GetVersion (vtable +0x18, 0x140124610): mov eax,1; ret -> file version 1.
// Dtor (vtable +0x00, 0x1400357e0): vtable reset + String::~String(+0x28) +
//   External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (MapDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163b20) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/MapDescription.h"
#include "Component/MapComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <new>

// ctor (inlined into create 0x140035760 / createInPlace 0x1400357a0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
MapDescription::MapDescription()
    : Description(),
      _cellType(2),
      _claimOnAdd(false),
      _noOccupancy(false),
      _showOnMap(false) {
    _color._value = 0xff000000u;
}

// [5] 0x140124560: stream order as retail.
bool MapDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_cellType, nullptr);
    _footprint.Serialize(&ser);
    ser.Serialize(_claimOnAdd, nullptr);
    ser.Serialize(_noOccupancy, nullptr);
    ser.Serialize(_showOnMap, nullptr);
    _color.Serialize(ser);
    return true;
}

// [6] 0x1400357b0: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* MapDescription::Reconstruct() {
    this->~MapDescription();
    return new (static_cast<void*>(this)) MapDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void MapDescription::Destruct() { this->~MapDescription(); }

// [8] 0x1400357d0: MapComponent's type id through the hidden return slot.
Object::Type* MapDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = MapComponent::TypeId();
    return ret;
}

// [9] 0x1400357a0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* MapDescription::Construct() {
    return new (static_cast<void*>(this)) MapDescription();
}

// create 0x140035760: MemAlloc(0x38, 8), then the (inlined) ctor.
void* MapDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(MapDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) MapDescription());
}
// createInPlace 0x1400357a0.
void* MapDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) MapDescription());
}

DEFINE_EXE_TYPE(MapDescription, "Description")
