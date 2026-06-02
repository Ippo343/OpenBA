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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable = 0x140163050;

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

extern "C" void* DecalDescription_Ctor_Impl(void* mem);

extern "C" void* DecalDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    *(float*)(s + 0x20)   = 1.0f;  // sizeA
    *(uint16_t*)(s + 0x24) = 0;    // flagA, flagB
    *(uint64_t*)(s + 0x28) = 0;    // refArray
    *(uint64_t*)(s + 0x30) = 0;    // refCount + cap
    *(uint64_t*)(s + 0x38) = 0;    // quadArray
    *(uint64_t*)(s + 0x40) = 0;    // quadCount + cap
    *(float*)(s + 0x48)   = 1.0f;  // sizeB
    return mem;
}

// Serialize (vtable +0x28, exe 0x14011b980)
extern "C" void DecalDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !fn_Serializer_Serialize_Float || !fn_Array_Serialize_External_GraphicsMesh) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    fn_Serializer_Serialize_Float(ser, (float*)(s + 0x20), nullptr);   // sizeA

    // --- refArray (External* pool) at +0x28 ---
    g_fn_Serialize_Int(ser, s + 0x30, nullptr);            // refCount
    int refCount = *(int32_t*)(s + 0x30);
    bool loading = (*(int32_t*)(*(uintptr_t*)ser + 8) == 0);
    if (loading) {
        uint8_t* r = reinterpret_cast<uint8_t*>(AllocArray(s + 0x28, refCount, 8));
        for (int j = 0; j < refCount; ++j) *(uint64_t*)(r + j * 8) = 0;
    }
    fn_Array_Serialize_External_GraphicsMesh(ser, refCount, *(void**)(s + 0x28));

    fn_Serializer_Serialize_Float(ser, (float*)(s + 0x48), nullptr);   // sizeB

    // --- quadArray (4-float entries) at +0x38 ---
    g_fn_Serialize_Int(ser, s + 0x40, nullptr);            // quadCount
    int quadCount = *(int32_t*)(s + 0x40);
    uint8_t* q;
    if (loading) {
        q = reinterpret_cast<uint8_t*>(AllocArray(s + 0x38, quadCount, 0x10));
        for (int i = 0; i < quadCount; ++i) {
            float* f = (float*)(q + i * 0x10);
            f[0] = 0.0f; f[1] = 0.0f; f[2] = 1.0f; f[3] = 1.0f;   // default quad
        }
    } else {
        q = *(uint8_t**)(s + 0x38);
    }
    for (int i = 0; i < quadCount; ++i, q += 0x10) {
        fn_Serializer_Serialize_Float(ser, (float*)(q + 0x0), nullptr);
        fn_Serializer_Serialize_Float(ser, (float*)(q + 0x4), nullptr);
        fn_Serializer_Serialize_Float(ser, (float*)(q + 0x8), nullptr);
        fn_Serializer_Serialize_Float(ser, (float*)(q + 0xc), nullptr);
    }

    g_fn_Serialize_Bool(ser, s + 0x24, nullptr);           // flagA
    g_fn_Serialize_Bool(ser, s + 0x25, nullptr);           // flagB

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// Dtor (vtable +0x00, exe 0x1400338a0)
extern "C" void* DecalDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_MemFreeStatic) {
        if (*(void**)(s + 0x38)) fn_MemFreeStatic(*(void**)(s + 0x38));   // quadArray
        if (*(void**)(s + 0x28)) fn_MemFreeStatic(*(void**)(s + 0x28));   // refArray
    }
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;
    *(uint64_t*)(s + 0x40) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x140033870)
extern "C" void DecalDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    DecalDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x140033890)
extern "C" void* DecalDescription_Slot8(void* self) {
    if (!self) return nullptr;
    DecalDescription_Ctor_Impl(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DecalDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DecalDescription");
    return cached;
}
static int  DecalDescription_Ver(void*)        { return 4; }
static int  DecalDescription_Ret0(void*)       { return 0; }
static void* DecalDescription_RetNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* DecalDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "DecalComponent"); }

static void* g_DecalDescription_Vtable[11] = {
    reinterpret_cast<void*>(&DecalDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&DecalDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&DecalDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&DecalDescription_Ver),        // +0x18 GetVersion (4)
    reinterpret_cast<void*>(&DecalDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&DecalDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&DecalDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&DecalDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&DecalDescription_GetComponentTypeId),      // +0x40 Slot8
    reinterpret_cast<void*>(&DecalDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&DecalDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterDecalDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_DecalDescription_Vtable));
}

extern "C" void* DecalDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 8);
    if (!p) return nullptr;
    return DecalDescription_Ctor_Impl(p);
}
extern "C" void* DecalDescription_CreateInPlace(void* mem) {
    return DecalDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(DecalDescription, "Description",
              &DecalDescription_Factory, &DecalDescription_CreateInPlace);
#endif // STANDALONE_EXE
