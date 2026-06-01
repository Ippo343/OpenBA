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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable = 0x140163c20;

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
} // namespace

extern "C" void* ModelDescription_Ctor_Impl(void* mem);

extern "C" void* ModelDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    *(uint64_t*)(s + 0x20) = 0;   // meshGroupArray
    *(uint64_t*)(s + 0x28) = 0;   // meshGroupCount + cap
    *(uint64_t*)(s + 0x30) = 0;   // lod0, lod1
    *(uint8_t*)(s + 0x38)  = 0;   // castShadow
    return mem;
}

// Serialize (vtable +0x28, exe 0x140124dc0)
extern "C" void ModelDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !fn_ExternalPtr_Serialize_GraphicsMesh || !fn_Array_Serialize_External_GraphicsMesh) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    g_fn_Serialize_Int(ser, s + 0x28, nullptr);            // meshGroupCount
    int count = *(int32_t*)(s + 0x28);

    bool loading = (*(int32_t*)(*(uintptr_t*)ser + 8) == 0);
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(s + 0x20, count, 0x18));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x18)        = 0;          // mesh
            *(uint64_t*)(e + i * 0x18 + 8)    = 0;          // materialArray
            *(uint64_t*)(e + i * 0x18 + 0x10) = 0;          // materialCount + cap
        }
    } else {
        e = *(uint8_t**)(s + 0x20);
    }

    for (int i = 0; i < count; ++i, e += 0x18) {
        fn_ExternalPtr_Serialize_GraphicsMesh(e, ser);      // mesh ref (elem+0)

        g_fn_Serialize_Int(ser, e + 0x10, nullptr);         // materialCount (elem+0x10)
        int matCount = *(int32_t*)(e + 0x10);

        if (loading) {
            uint8_t* m = reinterpret_cast<uint8_t*>(AllocArray(e + 8, matCount, 8));
            for (int j = 0; j < matCount; ++j) *(uint64_t*)(m + j * 8) = 0;
        }
        // serialize materialArray of External* (elem+8 base)
        fn_Array_Serialize_External_GraphicsMesh(ser, matCount, *(void**)(e + 8));
    }

    g_fn_Serialize_Int(ser, s + 0x30, nullptr);            // lod0
    g_fn_Serialize_Int(ser, s + 0x34, nullptr);            // lod1
    g_fn_Serialize_Bool(ser, s + 0x38, nullptr);           // castShadow

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// Dtor (vtable +0x00, exe 0x140035a70)
extern "C" void* ModelDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    // Free each MeshGroup's material sub-array, then the MeshGroup array.
    uint8_t* e = *(uint8_t**)(s + 0x20);
    int count = *(int32_t*)(s + 0x28);
    if (e && fn_MemFreeStatic) {
        for (int i = 0; i < count; ++i) {
            void* mat = *(void**)(e + i * 0x18 + 8);
            if (mat) fn_MemFreeStatic(mat);
        }
        fn_MemFreeStatic(e);
    }
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x140035a00)
extern "C" void ModelDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    ModelDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x140035a60)
extern "C" void* ModelDescription_Slot8(void* self) {
    if (!self) return nullptr;
    ModelDescription_Ctor_Impl(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ModelDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ModelDescription");
    return cached;
}
static int  ModelDescription_Ver(void*)        { return 1; }
static int  ModelDescription_Ret0(void*)       { return 0; }
static void* ModelDescription_RetNull(void*, ...) { return nullptr; }

static void* g_ModelDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ModelDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&ModelDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ModelDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&ModelDescription_Ver),        // +0x18 GetVersion (1)
    reinterpret_cast<void*>(&ModelDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&ModelDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&ModelDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&ModelDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&ModelDescription_Slot8),      // +0x40 Slot8
    reinterpret_cast<void*>(&ModelDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&ModelDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterModelDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_ModelDescription_Vtable));
}

extern "C" void* ModelDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    return ModelDescription_Ctor_Impl(p);
}
extern "C" void* ModelDescription_CreateInPlace(void* mem) {
    return ModelDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(ModelDescription, "Description",
              &ModelDescription_Factory, &ModelDescription_CreateInPlace);
#endif // STANDALONE_EXE
