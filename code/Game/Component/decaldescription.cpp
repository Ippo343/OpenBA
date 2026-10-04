// code/Game/Component/decaldescription.cpp
// Clean reimplementation of DecalDescription (Foundation C).
//
// DecalDescription (0x50 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x140163050 (exe symbol PTR_DecalComponent_vslot16_140163050).
// BaseTypeInfo @ 0x1401c75b8, parent "Description", file version 4.
//
// Field layout (from DecalDescription ctor sub_bb80 @ exe and Serialize @ 0x14011b980):
//   +0x20  float  sizeA          (default 1.0)
//   +0x24  bool   flagA          (default 0)
//   +0x25  bool   flagB          (default 0)
//   +0x28  void*  refArray       (dyn-array of External*, 0x08 stride)
//   +0x30  int32  refCount
//   +0x38  void*  quadArray      (dyn-array of 4-float entries, 0x10 stride)
//   +0x40  int32  quadCount
//   +0x48  float  sizeB          (default 1.0)
//
// quad element (stride 0x10): 4 floats. Default {0, 0, 1.0, 1.0}.
//
// Serialize order (exe 0x14011b980):
//   1. float sizeA (+0x20)
//   2. refCount (int, +0x30); resize refArray (+0x28); serialize each External* ref
//   3. float sizeB (+0x48)
//   4. quadCount (int, +0x40); resize quadArray (+0x38); per quad serialize 4 floats
//   5. bool flagA (+0x24), bool flagB (+0x25)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (DecalDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163050) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/DecalDescription.h"
#include "Component/DecalComponent.h"   // B.3.8 CB1a (TypeId)
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "object/External.h"
#include "object/TypeInfo.h"
#include <cstdint>
#include <new>
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"

namespace {
// {data, count, cap} header: set count = cap = n and allocate n * stride (8-aligned).
// Stores in the old order: count (+8), capacity (+0xc), then data (+0).
template <class T>
T* AllocArray(T*& data, int32_t& countField, int32_t& capField, int count, int stride) {
    countField = count;
    capField = count;
    T* buf = (count > 0 && ::System::MemoryAllocator::_singleton)
             ? static_cast<T*>(::System::MemoryAllocator::MemAlloc((uint64_t)count * (uint32_t)stride, 8))
             : nullptr;
    data = buf;
    return buf;
}
}  // namespace

// ctor (inlined into create 0x140033820 / createInPlace 0x140033860):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
DecalDescription::DecalDescription()
    : Description(),
      _initialAlpha(1.0f),
      _tiled(false),
      _flagB(false),
      _materials(nullptr),
      _materialCount(0),
      _materialCap(0),
      _uvRects(nullptr),
      _uvRectCount(0),
      _uvRectCap(0),
      _sizeB(1.0f) {}

// dtor (slot 0 0x1400338a0 minus the stamps): frees the quad and ref arrays.
// The compiler then chains ~Description -> ~External.
DecalDescription::~DecalDescription() {
    if (_uvRects)   ::System::MemoryAllocator::MemFree(_uvRects);     // quadArray
    if (_materials) ::System::MemoryAllocator::MemFree(_materials);   // refArray
    _materials = nullptr; _materialCount = 0; _materialCap = 0;
    _uvRects = nullptr;   _uvRectCount = 0;   _uvRectCap = 0;
}

// [5] 0x14011b980: stream order as retail.
bool DecalDescription::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);
    ser.Serialize(_initialAlpha, nullptr);   // sizeA

    // --- refArray (External* pool) at +0x28 ---
    ser.Serialize(_materialCount, nullptr);   // refCount
    int refCount = _materialCount;
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    if (loading) {
        AllocArray(_materials, _materialCount, _materialCap, refCount, 8);
        for (int j = 0; j < refCount; ++j) _materials[j] = nullptr;
    }
    Reimpl_Array_Serialize_External_GraphicsMesh(&ser, refCount, _materials);

    ser.Serialize(_sizeB, nullptr);   // sizeB

    // --- quadArray (4-float entries) at +0x38 ---
    ser.Serialize(_uvRectCount, nullptr);   // quadCount
    int quadCount = _uvRectCount;
    if (loading) {
        AllocArray(_uvRects, _uvRectCount, _uvRectCap, quadCount, 0x10);
        for (int i = 0; i < quadCount; ++i)
            _uvRects[i] = DecalUVRect{0.0f, 0.0f, 1.0f, 1.0f};   // default quad
    }
    for (int i = 0; i < quadCount; ++i) {
        ser.Serialize(_uvRects[i].u0, nullptr);
        ser.Serialize(_uvRects[i].v0, nullptr);
        ser.Serialize(_uvRects[i].u1, nullptr);
        ser.Serialize(_uvRects[i].v1, nullptr);
    }
    ser.Serialize(_tiled, nullptr);
    ser.Serialize(_flagB, nullptr);
    return true;
}

// [6] 0x140033870: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* DecalDescription::Reconstruct() {
    this->~DecalDescription();
    return new (static_cast<void*>(this)) DecalDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void DecalDescription::Destruct() { this->~DecalDescription(); }

// [8] 0x140033890: DecalComponent's type id through the hidden return slot.
Object::Type* DecalDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = DecalComponent::TypeId();
    return ret;
}

// [9] 0x140033860: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* DecalDescription::Construct() {
    return new (static_cast<void*>(this)) DecalDescription();
}

// create 0x140033820: MemAlloc(0x50, 8), then the (inlined) ctor.
void* DecalDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(DecalDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) DecalDescription());
}
// createInPlace 0x140033860.
void* DecalDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) DecalDescription());
}

DEFINE_EXE_TYPE(DecalDescription, "Description")
