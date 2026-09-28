// code/Game/Component/particledescription.cpp
// Clean reimplementation of ParticleDescription (Foundation C).
//
// ParticleDescription (0x38 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x14015a310 (exe symbol PTR_ParticleComponent_vslot16_14015a310).
// BaseTypeInfo @ 0x1401c4758, parent "Description", file version 7.
//
// Field layout (from ParticleDescription_Factory @ exe and Serialize @ 0x140053460):
//   +0x20  External* materialRef  (ExternalPtr<Pathfinding::PointList>; FUN_14000ab20
//          type-checks it against ?_typeInfo@PointList@Pathfinding@@, B37_RULES 6.2 —
//          it is not a material; the name is historical)
//   +0x28  void*  emitterArray    (dyn-array of emitter entries, 0x28 stride)
//   +0x30  int32  emitterCount
//
// Emitter element (stride 0x28):
//   elem+0x00 : External* ref   (ExternalPtr; FUN_14000a910)
//   elem+0x08 : String          (8 bytes; String::Serialize)
//   elem+0x10 : bool  b0        (default 0)
//   elem+0x11 : bool  b1        (default 1)
//   elem+0x12 : bool  b2        (default 1)
//   elem+0x13 : bool  b3        (default 0)
//   elem+0x18 : Time  t0        (default 0.0)   — serialized SECOND
//   elem+0x20 : Time  t1        (default 1.0)   — serialized FIRST
//
// Serialize order per emitter (exe 0x140053460):
//   1. ref ExternalPtr (elem+0)
//   2. String (elem+8)
//   3. b0,b1,b2,b3 (elem+0x10..0x13)
//   4. Time t1 (elem+0x20), then Time t0 (elem+0x18)
// Tail: materialRef ExternalPtr (+0x20).
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (ParticleDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x14015a310) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ParticleDescription.h"
#include "Component/ParticleComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "system/String.h"
#include "system/Value.h"
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

void InitTime(void* field, float seconds) {
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(field), seconds);
}
}  // namespace

// ctor (inlined into create 0x14000a3c0 / createInPlace 0x14000a430):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ParticleDescription::ParticleDescription()
    : Description(),
      _materialRef(nullptr),
      _emitters(nullptr),
      _emitterCount(0),
      _emitterCap(0) {}

// dtor (slot 0 0x14000a500 minus the stamps): destroys each emitter's String, then frees the emitter array.
// The compiler then chains ~Description -> ~External.
ParticleDescription::~ParticleDescription() {
    uint8_t* e = static_cast<uint8_t*>(_emitters);
    int count = _emitterCount;
    if (e) {
        for (int i = 0; i < count; ++i) reinterpret_cast<System::String*>(e + i * 0x28 + 0x08)->~String();  // B.3.1 bridge
        System::MemoryAllocator::MemFree(e);
    }
    _emitters = nullptr;
    _emitterCount = 0;
    _emitterCap = 0;
}

// [5] 0x140053460: stream order as retail.
bool ParticleDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_emitterCount, nullptr);   // emitterCount
    int count = _emitterCount;
    
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(&_emitters, count, 0x28));
        for (int i = 0; i < count; ++i) {
            uint8_t* el = e + i * 0x28;
            *(uint64_t*)(el + 0x00) = 0;       // ref
            ::new (static_cast<void*>(el + 0x08)) System::String();  // B.3.1 bridge: String
            *(uint8_t*)(el + 0x10) = 0;
            *(uint8_t*)(el + 0x11) = 1;
            *(uint8_t*)(el + 0x12) = 1;
            *(uint8_t*)(el + 0x13) = 0;
            InitTime(el + 0x18, 0.0f);          // t0 default 0.0
            InitTime(el + 0x20, 1.0f);          // t1 default 1.0 (g_fUVScale)
        }
    } else {
        e = static_cast<uint8_t*>(_emitters);
    }
    
    for (int i = 0; i < count; ++i, e += 0x28) {
        Reimpl_ExternalPtr_Serialize_Typed_7140(e + 0x00, &ser);                                 // ref (elem+0)
        reinterpret_cast<System::String*>(e + 0x08)->Serialize(&ser);             // B.3.1 bridge: String (elem+8)
        ser.Serialize(*reinterpret_cast<bool*>(e + 0x10), nullptr);
        ser.Serialize(*reinterpret_cast<bool*>(e + 0x11), nullptr);
        ser.Serialize(*reinterpret_cast<bool*>(e + 0x12), nullptr);
        ser.Serialize(*reinterpret_cast<bool*>(e + 0x13), nullptr);
        reinterpret_cast<System::Time*>(e + 0x20)->Serialize(ser);                // t1 (elem+0x20) FIRST   // B.3.4b bridge
        reinterpret_cast<System::Time*>(e + 0x18)->Serialize(ser);                // t0 (elem+0x18) SECOND  // B.3.4b bridge
    }
    
    Reimpl_ExternalPtr_Serialize_Typed_7140(&_materialRef, &ser);   // materialRef (+0x20)
    return true;
}

// [6] 0x14000a490: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* ParticleDescription::Reconstruct() {
    this->~ParticleDescription();
    return new (static_cast<void*>(this)) ParticleDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void ParticleDescription::Destruct() { this->~ParticleDescription(); }

// [8] 0x14000a4f0: ParticleComponent's type id through the hidden return slot.
Object::Type* ParticleDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ParticleComponent::TypeId();
    return ret;
}

// [9] 0x14000a430: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* ParticleDescription::Construct() {
    return new (static_cast<void*>(this)) ParticleDescription();
}

// create 0x14000a3c0: MemAlloc(0x38, 8), then the (inlined) ctor.
void* ParticleDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ParticleDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ParticleDescription());
}
// createInPlace 0x14000a430.
void* ParticleDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ParticleDescription());
}

DEFINE_EXE_TYPE(ParticleDescription, "Description")
