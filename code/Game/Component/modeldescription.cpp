// code/Game/Component/modeldescription.cpp
// Clean reimplementation of ModelDescription (Foundation C).
//
// ModelDescription (0x40 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x140163c20 (exe symbol PTR_ModelComponent_vslot16_140163c20).
// BaseTypeInfo @ 0x1401c7a88, parent "Description", file version 1.
//
// Field layout (from ModelDescription_Factory @ exe and Serialize @ 0x140124dc0):
//   +0x20  void*  meshGroupArray  (dyn-array of MeshGroup, 0x18 stride)
//   +0x28  int32  meshGroupCount
//   +0x30  int32  lod0            (int)
//   +0x34  int32  lod1            (int)
//   +0x38  bool   castShadow      (bool)
//
// MeshGroup element (stride 0x18):
//   elem+0x00 : External* mesh           (ExternalPtr<GraphicsMesh>; FUN_140035b40)
//   elem+0x08 : void*     materialArray  (dyn-array of External*, 0x08 stride)
//   elem+0x10 : int32     materialCount
//
// Serialize order per MeshGroup (exe 0x140124dc0):
//   1. mesh ExternalPtr (elem+0)         — fn_ExternalPtr_Serialize_GraphicsMesh
//   2. materialCount (int, elem+0x10)
//   3. resize materialArray (elem+8)     — fresh-load: alloc+zero
//   4. serialize each material External* — fn_Array_Serialize_External_GraphicsMesh
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).
//
// ENDGAME B.3.8 K1: a REAL class (ModelDescription.h) with a compiler-generated vtable;
// the hand-built 11-slot table and its RegisterExeAddr(0x140163c20) row are gone
// (nothing in the tree read that address back — B38_RULES D5).

#include "Component/ModelDescription.h"
#include "Component/ModelComponent.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "io/Serializer.h"
#include "Object/factory_registration.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Game/exe_bodies.h"   // Reimpl_ExternalPtr_Serialize_GraphicsMesh, Reimpl_Array_Serialize_External_GraphicsMesh
#include <cstdint>
#include <new>

namespace {
// {data, count, cap} header: set count = cap = n and allocate n * stride (8-aligned).
void* AllocArray(void* header, int count, int stride) {
    uint8_t* h = reinterpret_cast<uint8_t*>(header);
    *(int32_t*)(h + 8) = count;
    *(int32_t*)(h + 0xc) = count;
    void* buf = (count > 0 && System::MemoryAllocator::_singleton)
                ? System::MemoryAllocator::MemAlloc((uint64_t)count * (uint32_t)stride, 8)
                : nullptr;
    *(void**)h = buf;
    return buf;
}
}  // namespace

// ctor (inlined into create 0x140035930 / createInPlace 0x1400359a0):
// `??0External@Object@@` (Description()), the stamps (the compiler's), then
// the defaults.
ModelDescription::ModelDescription()
    : Description(),
      _meshGroups(nullptr),
      _meshGroupCount(0),
      _meshGroupCap(0),
      _lod0(0),
      _lod1(0),
      _castShadow(false) {}

// dtor (slot 0 0x140035a70 minus the stamps): frees every MeshGroup's material array, then the group array.
// The compiler then chains ~Description -> ~External.
ModelDescription::~ModelDescription() {
    // Free each MeshGroup's material sub-array, then the MeshGroup array.
    uint8_t* e = static_cast<uint8_t*>(_meshGroups);
    int count = _meshGroupCount;
    if (e) {
        for (int i = 0; i < count; ++i) {
            void* mat = *(void**)(e + i * 0x18 + 8);
            if (mat) System::MemoryAllocator::MemFree(mat);
        }
        System::MemoryAllocator::MemFree(e);
    }
    _meshGroups = nullptr;
    _meshGroupCount = 0;
    _meshGroupCap = 0;
}

// [5] 0x140124dc0: stream order as retail.
bool ModelDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)
    ser.Serialize(_meshGroupCount, nullptr);   // meshGroupCount
    int count = _meshGroupCount;
    
    bool loading = (static_cast<int>(ser.GetDirection()) == 0);   // IO::Serializer::GetDirection, retail 0x180006210
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(&_meshGroups, count, 0x18));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x18)        = 0;          // mesh
            *(uint64_t*)(e + i * 0x18 + 8)    = 0;          // materialArray
            *(uint64_t*)(e + i * 0x18 + 0x10) = 0;          // materialCount + cap
        }
    } else {
        e = static_cast<uint8_t*>(_meshGroups);
    }
    
    for (int i = 0; i < count; ++i, e += 0x18) {
        Reimpl_ExternalPtr_Serialize_GraphicsMesh(e, &ser);      // mesh ref (elem+0)
    
        ser.Serialize(*reinterpret_cast<int*>(e + 0x10), nullptr);   // materialCount, elem+0x10
        int matCount = *(int32_t*)(e + 0x10);
    
        if (loading) {
            uint8_t* m = reinterpret_cast<uint8_t*>(AllocArray(e + 8, matCount, 8));
            for (int j = 0; j < matCount; ++j) *(uint64_t*)(m + j * 8) = 0;
        }
        // serialize materialArray of External* (elem+8 base)
        Reimpl_Array_Serialize_External_GraphicsMesh(&ser, matCount, *(void**)(e + 8));
    }
    ser.Serialize(_lod0, nullptr);
    ser.Serialize(_lod1, nullptr);
    ser.Serialize(_castShadow, nullptr);
    return true;
}

// [6] 0x140035a00: `this->~T()` (retail: slot 0, flags 0 -- destroy in place), then the ctor again on
// `this`. The hand-built table's Cleanup trampoline already did this.
Object::External* ModelDescription::Reconstruct() {
    this->~ModelDescription();
    return new (static_cast<void*>(this)) ModelDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void ModelDescription::Destruct() { this->~ModelDescription(); }

// [8] 0x140035a60: ModelComponent's type id through the hidden return slot.
Object::Type* ModelDescription::GetComponentType(Object::Type* ret) const {
    ret->_type = ModelComponent::TypeId();
    return ret;
}

// [9] 0x1400359a0: placement-new on `this`, the same body as createInPlace (D9 #10).
Object::External* ModelDescription::Construct() {
    return new (static_cast<void*>(this)) ModelDescription();
}

// create 0x140035930: MemAlloc(0x40, 8), then the (inlined) ctor.
void* ModelDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ModelDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) ModelDescription());
}
// createInPlace 0x1400359a0.
void* ModelDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) ModelDescription());
}

DEFINE_EXE_TYPE(ModelDescription, "Description")
