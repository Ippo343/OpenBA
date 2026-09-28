// code/Game/Component/citizendescription.cpp
// Clean reimplementation of CitizenDescription (Object::External "Description").
//
// CitizenDescription (0x148 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140162600.  GetVersion = 17 (0x11).
//
// This is the field-heavy citizen tuning block (Template\Citizen.rsc). Layout +
// defaults come from ctor FUN_140114e70 @ 0x140114e70; serialize stream order
// from CitizenComponent_OnCreate (Serialize) @ 0x140114900.
//
// Special members:
//   +0x20,+0x28,+0x30  ExternalRef<Profession> (FUN_140018020, single ptr each)
//   +0xdc..+0xe7       float[3] bulk-serialized (count=3, slot 0x140151ea8)
//   +0x138 / +0x140    dynamic External* array (ptr / count): resize via
//                      FUN_140001bb0, element-serialize via FUN_14001bcc0
//
// Serialize stream order (EXACT, from 0x140114900):
//   float: +0x44, +0x6c,+0x70,+0x74,+0x78,+0x7c,+0x80,+0x84,+0x88,+0x8c,+0x90,
//          +0x38,+0x3c,+0x40, +0xc0, +0xd8;
//   floatArray3(+0xdc);
//   float: +0x48,+0x4c,+0x50,+0x54,+0x58,+0x5c,+0x60, +0xc4,+0xd4,+0xc8,+0xcc,+0xd0,
//          +0xe8,+0xec,+0xf0,+0xf4;
//   int +0x110;
//   int countTmp=+0x140 (serialize); if loading resize(+0x138,count); refArray(+0x138,count);
//   float +0x114; int +0x118; float +0x120,+0x11c,+0x128,+0x124,+0x130;
//   float +0xa4; int +0xa8,+0xac; float +0xa0; int +0xb0; float +0xb4,+0xb8; int +0xbc;
//   ExternalRef +0x20; ExternalRef +0x28; ExternalRef +0x30;
//   float +0x64,+0x68; int +0xf8,+0xfc,+0x100,+0x104,+0x108; float +0x10c.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).
//
// ENDGAME B.3.8 K1: a REAL class (CitizenDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140162600) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/CitizenDescription.h"
#include "Component/CitizenComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Component/coll_bridge.h"
#include <cstring>
#include <cstdint>
#include <new>
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Game/exe_bodies.h"

namespace {
// The retail ctor stores raw IEEE bit patterns; this keeps them exact.
inline float FloatBits(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

// ctor (inlined into create 0x140030170 / createInPlace 0x1400301b0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
CitizenDescription::CitizenDescription()
    : Description(),
      _f38(FloatBits(0x41700000)),
      _f3c(FloatBits(0x40000000)),
      _f40(FloatBits(0x3991a2b4)),
      _f44(FloatBits(0x40000000)),
      _f48(FloatBits(0x40000000)),
      _f4c(FloatBits(0x40400000)),
      _f50(FloatBits(0x40000000)),
      _f54(FloatBits(0x41000000)),
      _f58(FloatBits(0x44610000)),
      _f5c(FloatBits(0x44e10000)),
      _f60(FloatBits(0x45a8c000)),
      _f64(FloatBits(0x3c23d70a)),
      _f68(FloatBits(0x3dcccccd)),
      _f6c(FloatBits(0x40c00000)),
      _f70(FloatBits(0x41200000)),
      _f74(FloatBits(0x41000000)),
      _f78(FloatBits(0x40400000)),
      _f7c(FloatBits(0x3f800000)),
      _f80(FloatBits(0x3f800000)),
      _f84(FloatBits(0x3f800000)),
      _f88(FloatBits(0x3f800000)),
      _f8c(FloatBits(0x3f800000)),
      _f90(FloatBits(0x3f800000)),
      _f94(FloatBits(0x3f800000)),
      _f98(FloatBits(0x3f000000)),
      _f9c(FloatBits(0x41400000)),
      _fa0(FloatBits(0x3e800000)),
      _fa4(FloatBits(0x41200000)),
      _ia8(4),
      _iac(12),
      _ib0(3),
      _fb4(FloatBits(0x3f19999a)),
      _fb8(FloatBits(0x3e800000)),
      _ibc(20),
      _fc0(FloatBits(0x42c80000)),
      _fc4(FloatBits(0x42c80000)),
      _fc8(FloatBits(0x425c0000)),
      _fcc(FloatBits(0x42960000)),
      _fd0(FloatBits(0x42c80000)),
      _fd4(FloatBits(0x41c80000)),
      _fd8(FloatBits(0x3dcccccd)),
      _fe8(FloatBits(0x42480000)),
      _fec(FloatBits(0x42960000)),
      _ff0(FloatBits(0x42c80000)),
      _ff4(FloatBits(0x42400000)),
      _if8(60),
      _ifc(60),
      _i100(2000),
      _i104(6),
      _i108(200),
      _f10c(FloatBits(0x42340000)),
      _i110(100),
      _f114(FloatBits(0x40a00000)),
      _i118(200),
      _f11c(FloatBits(0x40800000)),
      _f120(FloatBits(0x40a00000)),
      _f124(FloatBits(0x40a00000)),
      _f128(FloatBits(0x40000000)),
      _f12c(FloatBits(0x3c23d70a)),
      _f130(FloatBits(0x42700000)),
      _refs(nullptr),
      _refCount(0),
      _refCap(0) {
    _professions[0] = _professions[1] = _professions[2] = nullptr;
    _f3dc[0] = _f3dc[1] = _f3dc[2] = FloatBits(0x41200000);
}

// dtor (slot 0 0x1400301f0 minus the stamps): the tree's slot 0 skipped ~External; the compiler chain now runs it (B38 D2 — this commit's behaviour change)
// The compiler then chains ~Description -> ~External.
CitizenDescription::~CitizenDescription() {
    // Free the dynamic External* array buffer (entries are non-owning ptrs).
    if (_refs) {
        System::MemoryAllocator::MemFree(_refs);
        _refs = nullptr;
        _refCount = 0;
        _refCap = 0;
    }
}

// [5] 0x140114900: stream order as retail.
bool CitizenDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_f44, nullptr);
    ser.Serialize(_f6c, nullptr);
    ser.Serialize(_f70, nullptr);
    ser.Serialize(_f74, nullptr);
    ser.Serialize(_f78, nullptr);
    ser.Serialize(_f7c, nullptr);
    ser.Serialize(_f80, nullptr);
    ser.Serialize(_f84, nullptr);
    ser.Serialize(_f88, nullptr);
    ser.Serialize(_f8c, nullptr);
    ser.Serialize(_f90, nullptr);
    ser.Serialize(_f38, nullptr);
    ser.Serialize(_f3c, nullptr);
    ser.Serialize(_f40, nullptr);
    ser.Serialize(_fc0, nullptr);
    ser.Serialize(_fd8, nullptr);
    ser.Serialize(3, _f3dc, nullptr);   // float[3] bulk at +0xdc
    ser.Serialize(_f48, nullptr);
    ser.Serialize(_f4c, nullptr);
    ser.Serialize(_f50, nullptr);
    ser.Serialize(_f54, nullptr);
    ser.Serialize(_f58, nullptr);
    ser.Serialize(_f5c, nullptr);
    ser.Serialize(_f60, nullptr);
    ser.Serialize(_fc4, nullptr);
    ser.Serialize(_fd4, nullptr);
    ser.Serialize(_fc8, nullptr);
    ser.Serialize(_fcc, nullptr);
    ser.Serialize(_fd0, nullptr);
    ser.Serialize(_fe8, nullptr);
    ser.Serialize(_fec, nullptr);
    ser.Serialize(_ff0, nullptr);
    ser.Serialize(_ff4, nullptr);
    ser.Serialize(_i110, nullptr);
    {
        // dynamic External* array (count at +0x140, data at +0x138)
        int32_t count = _refCount;
        ser.Serialize(count, nullptr);   // local temp, as retail
        if (static_cast<int>(ser.GetDirection()) == 0) {   // loading
            // B.3.2 bridge: exe 0x140001bb0 IS Collection::Array<void*>::SetSize(n).
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(&_refs)).SetSize(count);
        }
        // 0x14001bcc0 takes three arguments: its prologue spills R9 to the home slot
        // [RBP+0x58], which the body only reuses as a local after writing it, so the
        // old door's 4th nullptr was never read.
        Reimpl_Array_Serialize_External_GraphicsMesh(&ser, count, _refs);
    }
    ser.Serialize(_f114, nullptr);
    ser.Serialize(_i118, nullptr);
    ser.Serialize(_f120, nullptr);
    ser.Serialize(_f11c, nullptr);
    ser.Serialize(_f128, nullptr);
    ser.Serialize(_f124, nullptr);
    ser.Serialize(_f130, nullptr);
    ser.Serialize(_fa4, nullptr);
    ser.Serialize(_ia8, nullptr);
    ser.Serialize(_iac, nullptr);
    ser.Serialize(_fa0, nullptr);
    ser.Serialize(_ib0, nullptr);
    ser.Serialize(_fb4, nullptr);
    ser.Serialize(_fb8, nullptr);
    ser.Serialize(_ibc, nullptr);
    {
        // ExternalRef<Profession> x3 at +0x20/+0x28/+0x30 (0x140018020)
        Reimpl_ExternalPtr_Serialize_StringTable(&_professions[0], &ser);
        Reimpl_ExternalPtr_Serialize_StringTable(&_professions[1], &ser);
        Reimpl_ExternalPtr_Serialize_StringTable(&_professions[2], &ser);
    }
    ser.Serialize(_f64, nullptr);
    ser.Serialize(_f68, nullptr);
    ser.Serialize(_if8, nullptr);
    ser.Serialize(_ifc, nullptr);
    ser.Serialize(_i100, nullptr);
    ser.Serialize(_i104, nullptr);
    ser.Serialize(_i108, nullptr);
    ser.Serialize(_f10c, nullptr);
    return true;
}

// [6] 0x1400301c0: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* CitizenDescription::Reconstruct() {
    this->~CitizenDescription();
    return new (static_cast<void*>(this)) CitizenDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void CitizenDescription::Destruct() { this->~CitizenDescription(); }

// [8] 0x1400301e0: CitizenComponent's type id through the hidden return slot.
Object::Type* CitizenDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = CitizenComponent::TypeId();
    return ret;
}

// [9] 0x1400301b0: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* CitizenDescription::Construct() {
    return new (static_cast<void*>(this)) CitizenDescription();
}

// create 0x140030170: MemAlloc(0x148, 8), then the (inlined) ctor.
void* CitizenDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(CitizenDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) CitizenDescription());
}
// createInPlace 0x1400301b0.
void* CitizenDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) CitizenDescription());
}

DEFINE_EXE_TYPE(CitizenDescription, "Description")
