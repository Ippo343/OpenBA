// code/Game/Component/immigrationdescription.cpp
// ImmigrationDescription (entity description block) — immigration/arrival parameters.
// Extends Object::External (0x20 base). Vtable 0x140159bc0. Parent "Description".
// Size 0x68. GetVersion=2.
//
// Factory 0x14014b000-area (MemAlloc 0x68 + ctor sub_e9a0). Ctor defaults:
//   ExternalPtr @ +0x20 = 0
//   int         @ +0x28 = 0x50 (80)
//   External*-array @ +0x30 = 0 (count @ +0x38, cap @ +0x3c)
//   float @ +0x40 = 0.05  (0x3d4ccccd),  float @ +0x44 = 0.15 (0x3e19999a)
//   float @ +0x48 = 24.0  (0x41c00000),  float @ +0x4c = 48.0 (0x42400000)
//   int   @ +0x50 = 1,  int @ +0x54 = 3,  int @ +0x58 = 7,  int @ +0x5c = 10
//   float @ +0x60 = 1.0,  int @ +0x64 = 6
// Serialize (vtable +0x28, 0x14004e7f0), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   int         @ +0x28
//   int count   @ +0x38 -> resize +0x30 array -> per-element External FindOrLoad
//                          (FUN_1400049c0 == Desc_LoadExternalArray)
//   float @ +0x40,  float @ +0x44,  float @ +0x48,  float @ +0x4c
//   int   @ +0x50,  int   @ +0x54
//   float @ +0x60
//   int   @ +0x64
// GetVersion (vtable +0x18, 0x14004e930): mov eax,2; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).
//
// ENDGAME B.3.8 K1: a REAL class (ImmigrationDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140159bc0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ImmigrationDescription.h"
#include "Component/ImmigrationComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include <cstdint>
#include <new>

namespace {
// serializer+0x00 is the IO::Stream; stream+0x08 is its error/direction word
// (0 == ok on the read leg). The tree's check before it allocates an array.
inline int SerStreamErr(IO::Serializer& ser) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(&ser)) + 8);
}
}  // namespace

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);  // componentdescription.cpp

// ctor (inlined into create 0x140007a80 / createInPlace 0x140007ac0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ImmigrationDescription::ImmigrationDescription()
    : Description(),
      _ref20(nullptr),
      _i28(0x50),
      _refs(nullptr),
      _refCount(0),
      _refCap(0),
      _f40(0.05f),
      _f44(0.15f),
      _f48(24.0f),
      _f4c(48.0f),
      _i50(1),
      _i54(3),
      _i58(7),
      _i5c(10),
      _f60(1.0f),
      _i64(6) {}

// dtor (slot 0 0x140007b00 minus the stamps): frees the External* array buffer.
// The compiler then chains ~Description -> ~External.
ImmigrationDescription::~ImmigrationDescription() {
    if (_refs && fn_MemFreeStatic) fn_MemFreeStatic(_refs);
    _refs = nullptr;
}

// [0] 0x140007b00: ~ImmigrationDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* ImmigrationDescription::DeletingDtor(unsigned flags) {
    this->~ImmigrationDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14004e7f0: stream order as retail.
bool ImmigrationDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_ref20, &ser);
    ser.Serialize(_i28, nullptr);
    // External*-array @ +0x30 (count @ +0x38, cap @ +0x3c, stride 8)
    ser.Serialize(_refCount, nullptr);
    int n = _refCount;
    if (SerStreamErr(ser) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        _refs = buf;
        _refCap = n;
        if (buf) for (int i = 0; i < n; ++i) static_cast<void**>(buf)[i] = nullptr;
    }
    Desc_LoadExternalArray(&ser, n, _refs);
    ser.Serialize(_f40, nullptr);
    ser.Serialize(_f44, nullptr);
    ser.Serialize(_f48, nullptr);
    ser.Serialize(_f4c, nullptr);
    ser.Serialize(_i50, nullptr);
    ser.Serialize(_i54, nullptr);
    ser.Serialize(_f60, nullptr);
    ser.Serialize(_i64, nullptr);
    return true;
}

// [6] 0x140007ad0: retail destroys and rebuilds in place: a virtual DeletingDtor(0),
// then the ctor again on `this` (D9 #10).
Object::External* ImmigrationDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) ImmigrationDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void ImmigrationDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140007af0: ImmigrationComponent's type id through the hidden return slot.
Object::Type* ImmigrationDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ImmigrationComponent::TypeId();
    return ret;
}

// [9] 0x140007ac0: placement-new on `this` (folded with createInPlace). The old
// table held the same construction in this slot.
Object::External* ImmigrationDescription::Construct() {
    return new (static_cast<void*>(this)) ImmigrationDescription();
}

// create 0x140007a80: MemAlloc(0x68, 8), then the (inlined) ctor.
void* ImmigrationDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ImmigrationDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ImmigrationDescription());
}
// createInPlace 0x140007ac0.
void* ImmigrationDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ImmigrationDescription());
}

DEFINE_EXE_TYPE(ImmigrationDescription, "Description")
