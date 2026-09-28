// code/Game/Component/pickingdescription.cpp
// Clean reimplementation of PickingDescription (Foundation C).
//
// PickingDescription (0x40 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x1401640c0 (exe symbol PTR_PickingComponent_vslot16_1401640c0).
// BaseTypeInfo @ 0x1401c7c48, parent "Description", file version 4.
//
// Field layout (from PickingDescription_Factory @ exe and Serialize @ 0x140128d00):
//   +0x20  void*  groupArray   (dyn-array, 0x10 stride)
//   +0x28  int32  groupCount
//   +0x30  External* meshRef   (ExternalPtr; FUN_140007140)
//   +0x38  bool   enabled
//
// Group element (stride 0x10):
//   elem+0x00 : void*  refArray   (dyn-array of External*, 0x08 stride)
//   elem+0x08 : int32  refCount
//
// Serialize order per group (exe 0x140128d00):
//   1. refCount (int, elem+8)
//   2. resize refArray (elem+0)            — fresh-load alloc+zero
//   3. serialize each External* ref        — fn_Array_Serialize_External_GraphicsMesh
// Tail: meshRef ExternalPtr (+0x30) via fn_ExternalPtr_Serialize3, then bool (+0x38).
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (PickingDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x1401640c0) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/PickingDescription.h"
#include "Component/PickingComponent.h"
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

// ctor (inlined into create 0x140036e00 / createInPlace 0x140036e70):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
PickingDescription::PickingDescription()
    : Description(),
      _groups(nullptr),
      _groupCount(0),
      _groupCap(0),
      _meshRef(nullptr),
      _enabled(false) {}

// dtor (slot 0 0x140036f40 minus the stamps): frees every group's ref array, then the group array.
// The compiler then chains ~Description -> ~External.
PickingDescription::~PickingDescription() {
    uint8_t* e = static_cast<uint8_t*>(_groups);
    int count = _groupCount;
    if (e) {
        for (int i = 0; i < count; ++i) {
            void* ref = *(void**)(e + i * 0x10);
            if (ref) System::MemoryAllocator::MemFree(ref);
        }
        System::MemoryAllocator::MemFree(e);
    }
    _groups = nullptr;
    _groupCount = 0;
    _groupCap = 0;
}

// [5] 0x140128d00: stream order as retail.
bool PickingDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_groupCount, nullptr);   // groupCount
    int count = _groupCount;
    
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(&_groups, count, 0x10));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x10)     = 0;             // refArray
            *(uint64_t*)(e + i * 0x10 + 8) = 0;             // refCount + cap
        }
    } else {
        e = static_cast<uint8_t*>(_groups);
    }
    
    for (int i = 0; i < count; ++i, e += 0x10) {
        ser.Serialize(*reinterpret_cast<int*>(e + 8), nullptr);   // refCount (elem+8)
        int refCount = *(int32_t*)(e + 8);
        if (loading) {
            uint8_t* r = reinterpret_cast<uint8_t*>(AllocArray(e, refCount, 8));
            for (int j = 0; j < refCount; ++j) *(uint64_t*)(r + j * 8) = 0;
        }
        Reimpl_Array_Serialize_External_GraphicsMesh(&ser, refCount, *(void**)e);
    }
    Reimpl_ExternalPtr_Serialize_Typed_7140(&_meshRef, &ser);
    ser.Serialize(_enabled, nullptr);
    return true;
}

// [6] 0x140036ed0: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* PickingDescription::Reconstruct() {
    this->~PickingDescription();
    return new (static_cast<void*>(this)) PickingDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void PickingDescription::Destruct() { this->~PickingDescription(); }

// [8] 0x140036f30: PickingComponent's type id through the hidden return slot.
Object::Type* PickingDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = PickingComponent::TypeId();
    return ret;
}

// [9] 0x140036e70: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* PickingDescription::Construct() {
    return new (static_cast<void*>(this)) PickingDescription();
}

// create 0x140036e00: MemAlloc(0x40, 8), then the (inlined) ctor.
void* PickingDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PickingDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PickingDescription());
}
// createInPlace 0x140036e70.
void* PickingDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PickingDescription());
}

DEFINE_EXE_TYPE(PickingDescription, "Description")
