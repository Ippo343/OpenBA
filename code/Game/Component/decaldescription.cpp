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
#include "engine_funcs.h"
#include <cstdint>
#include <new>

namespace {
// {data, count, cap} header: set count = cap = n and allocate n * stride (8-aligned).
void* AllocArray(void* header, int count, int stride) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    *(int32_t*)(h + 8) = count;
    *(int32_t*)(h + 0xc) = count;
    void* buf = (count > 0 && fn_MemAlloc && g_pMemAllocSingleton)
                ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)count * (uint32_t)stride, 8)
                : nullptr;
    *(void**)h = buf;
    return buf;
}
}  // namespace

// ctor (inlined into create 0x140033820 / createInPlace 0x140033860):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
DecalDescription::DecalDescription()
    : Description(),
      _sizeA(1.0f),
      _flagA(false),
      _flagB(false),
      _refs(nullptr),
      _refCount(0),
      _refCap(0),
      _quads(nullptr),
      _quadCount(0),
      _quadCap(0),
      _sizeB(1.0f) {}

// dtor (slot 0 0x1400338a0 minus the stamps): frees the quad and ref arrays.
// The compiler then chains ~Description -> ~External.
DecalDescription::~DecalDescription() {
    if (fn_MemFreeStatic) {
        if (_quads) fn_MemFreeStatic(_quads);   // quadArray
        if (_refs)  fn_MemFreeStatic(_refs);    // refArray
    }
    _refs = nullptr;  _refCount = 0;  _refCap = 0;
    _quads = nullptr; _quadCount = 0; _quadCap = 0;
}

// [0] 0x1400338a0: ~DecalDescription -> ~Description -> ~External, MemFree on &1.
Object::Base* DecalDescription::DeletingDtor(unsigned flags) {
    this->~DecalDescription();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// [5] 0x14011b980: stream order as retail.
bool DecalDescription::Serialize(void* stream) {
    if (!fn_Array_Serialize_External_GraphicsMesh) return true;   // standalone: the door is unwired
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_sizeA, nullptr);   // sizeA
    
    // --- refArray (External* pool) at +0x28 ---
    ser.Serialize(_refCount, nullptr);   // refCount
    int refCount = _refCount;
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    if (loading) {
        uint8_t* r = reinterpret_cast<uint8_t*>(AllocArray(&_refs, refCount, 8));
        for (int j = 0; j < refCount; ++j) *(uint64_t*)(r + j * 8) = 0;
    }
    fn_Array_Serialize_External_GraphicsMesh(&ser, refCount, _refs);
    
    ser.Serialize(_sizeB, nullptr);   // sizeB
    
    // --- quadArray (4-float entries) at +0x38 ---
    ser.Serialize(_quadCount, nullptr);   // quadCount
    int quadCount = _quadCount;
    uint8_t* q;
    if (loading) {
        q = reinterpret_cast<uint8_t*>(AllocArray(&_quads, quadCount, 0x10));
        for (int i = 0; i < quadCount; ++i) {
            float* f = (float*)(q + i * 0x10);
            f[0] = 0.0f; f[1] = 0.0f; f[2] = 1.0f; f[3] = 1.0f;   // default quad
        }
    } else {
        q = static_cast<uint8_t*>(_quads);
    }
    for (int i = 0; i < quadCount; ++i, q += 0x10) {
        ser.Serialize(*reinterpret_cast<float*>(q + 0x0), nullptr);
        ser.Serialize(*reinterpret_cast<float*>(q + 0x4), nullptr);
        ser.Serialize(*reinterpret_cast<float*>(q + 0x8), nullptr);
        ser.Serialize(*reinterpret_cast<float*>(q + 0xc), nullptr);
    }
    ser.Serialize(_flagA, nullptr);
    ser.Serialize(_flagB, nullptr);
    return true;
}

// [6] 0x140033870: DeletingDtor(0) (destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* DecalDescription::Reconstruct() {
    DeletingDtor(0);
    return new (static_cast<void*>(this)) DecalDescription();
}

// [7] 0x140001640: retail's shared body, a virtual DeletingDtor(0) (D9 #10).
void DecalDescription::Destruct() { DeletingDtor(0); }

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

DEFINE_EXE_VTABLE(DecalDescription, "_ZTV16DecalDescription")
DEFINE_EXE_TYPE(DecalDescription, "Description")
