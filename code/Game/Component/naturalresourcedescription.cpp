// code/Game/Component/naturalresourcedescription.cpp
// Clean reimplementation of NaturalResourceDescription (Object::External "Description").
//
// NaturalResourceDescription (0x88 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140163e90.  GetVersion = 4.
//
// Field layout (from ctor NaturalResourceDescription_sub_6010 @ 0x140126010 +
// Serialize FUN_140125e10 @ 0x140125e10):
//   +0x20 ExternalPtr<BuildingDescription>  (FUN_140002340 serialize)
//   +0x28 float (1.0)   +0x2c float (4.0)   +0x30 float (1.0)  (+0x34 float, high dword of +0x30 write = 0)
//   +0x38 float (0.5)   (+0x3c float, high dword of +0x38 write = 0)
//   +0x40 float (16.0)  +0x44 float (0.95)
//   +0x48 bool (0)
//   +0x4c float (1.0)   +0x50 float (1.0)   +0x54 float (1.0)  (+0x58 float, high dword of +0x54 write = 0)
//   +0x5c int (1)       (+0x60 float, high dword of +0x5c write = 0)
//   +0x64 bool (0)
//   +0x68 Time (1.0)    +0x70 Time (1.0)
//   +0x78 ExternalRef<Profession>  (FUN_140018020 serialize)
//   +0x80 int/enum (0xb = 11)
//
// Serialize stream order (EXACT, from FUN_140125e10):
//   ExternalPtr(+0x20); float +0x28,+0x2c,+0x30,+0x34,+0x38; bool +0x48;
//   float +0x4c,+0x50,+0x54,+0x58; int +0x5c; float +0x3c,+0x40,+0x44; bool +0x64;
//   Time +0x68; Time +0x70; enum(serializer vtbl +0x98) +0x80; ExternalRef +0x78;
//   float +0x60.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).
//
// ENDGAME B.3.8 K1: a REAL class (NaturalResourceDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163e90) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/NaturalResourceDescription.h"
#include "Component/NaturalResourceComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstring>
#include <cstdint>
#include <new>

namespace {
// The retail ctor stores raw IEEE bit patterns; this keeps them exact.
inline float FloatBits(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

// ctor (inlined into create 0x1400365d0 / createInPlace 0x140036610):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
NaturalResourceDescription::NaturalResourceDescription()
    : Description(),
      _building(nullptr),
      _f28(1.0f),
      _f2c(4.0f),
      _f30(1.0f),
      _f34(0.0f),
      _f38(0.5f),
      _f3c(0.0f),
      _f40(16.0f),
      _f44(FloatBits(0x3f733333)),
      _b48(false),
      _f4c(1.0f),
      _f50(1.0f),
      _f54(1.0f),
      _f58(0.0f),
      _i5c(1),
      _f60(0.0f),
      _b64(false),
      _profession(nullptr),
      _u80(0xb) {
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(&_time68, 1.0f);
        fn_Time_Ctor_Float(&_time70, 1.0f);
    } else {
        _time68._ticks = 0;
        _time70._ticks = 0;
    }
}

// [0] 0x140036650: ~NaturalResourceDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* NaturalResourceDescription::DeletingDtor(unsigned flags) {
    this->~NaturalResourceDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x140125e10: stream order as retail.
bool NaturalResourceDescription::Serialize(void* stream) {
    if (!fn_ExternalPtr_BuildSerialize) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    fn_ExternalPtr_BuildSerialize(&_building, &ser);   // +0x20 ExternalPtr<BuildingDescription>
    ser.Serialize(_f28, nullptr);
    ser.Serialize(_f2c, nullptr);
    ser.Serialize(_f30, nullptr);
    ser.Serialize(_f34, nullptr);
    ser.Serialize(_f38, nullptr);
    ser.Serialize(_b48, nullptr);
    ser.Serialize(_f4c, nullptr);
    ser.Serialize(_f50, nullptr);
    ser.Serialize(_f54, nullptr);
    ser.Serialize(_f58, nullptr);
    ser.Serialize(_i5c, nullptr);
    ser.Serialize(_f3c, nullptr);
    ser.Serialize(_f40, nullptr);
    ser.Serialize(_f44, nullptr);
    ser.Serialize(_b64, nullptr);
    _time68.Serialize(ser);
    _time70.Serialize(ser);
    ser.Serialize(_u80, nullptr);   // enum through the UINT door
    {
        // +0x78 ExternalRef<Profession> (exe 0x140018020, resolved by address).
        typedef void (*fn_ExternalRefSerialize_t)(void* refField, void* serializer);
        fn_ExternalRefSerialize_t f =
            reinterpret_cast<fn_ExternalRefSerialize_t>(ResolveAddr(Module::Exe, 0x140018020));
        if (f) f(&_profession, &ser);
    }
    ser.Serialize(_f60, nullptr);
    return true;
}

// [6] 0x140036620: DeletingDtor(0) (destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* NaturalResourceDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) NaturalResourceDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void NaturalResourceDescription::Destruct() { DeletingDtor(0); }

// [8] 0x140036640: NaturalResourceComponent's type id through the hidden return slot.
Object::Type* NaturalResourceDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = NaturalResourceComponent::TypeId();
    return ret;
}

// [9] 0x140036610: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* NaturalResourceDescription::Construct() {
    return new (static_cast<void*>(this)) NaturalResourceDescription();
}

// create 0x1400365d0: MemAlloc(0x88, 8), then the (inlined) ctor.
void* NaturalResourceDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(NaturalResourceDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) NaturalResourceDescription());
}
// createInPlace 0x140036610.
void* NaturalResourceDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) NaturalResourceDescription());
}

DEFINE_EXE_TYPE(NaturalResourceDescription, "Description")
