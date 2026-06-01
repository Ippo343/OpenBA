// code/Game/Component/highlightdescription.cpp
// Clean reimplementation of HighlightDescription (Foundation C).
//
// HighlightDescription (0x40 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x140159a10 (exe symbol PTR_HighlightComponent_vslot16_140159a10).
// BaseTypeInfo @ 0x1401c4448, parent "Description", file version 2.
//
// Field layout (from HighlightDescription_Factory @ exe and Serialize @ 0x14004c9d0):
//   +0x20  void*  groupArray   (dyn-array, 0x10 stride)
//   +0x28  int32  groupCount
//   +0x30  External* ref0       (ExternalPtr; FUN_140007140)
//   +0x38  External* ref1       (ExternalPtr; FUN_140007140)
//
// Group element (stride 0x10):
//   elem+0x00 : void*  refArray   (dyn-array of External*, 0x08 stride)
//   elem+0x08 : int32  refCount
//
// Serialize order per group (exe 0x14004c9d0):
//   1. refCount (int, elem+8)
//   2. resize refArray (elem+0)            — fresh-load alloc+zero
//   3. serialize each External* ref        — fn_Array_Serialize_External_GraphicsMesh
// Tail: ref0 ExternalPtr (+0x30), ref1 ExternalPtr (+0x38) via fn_ExternalPtr_Serialize3.
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable = 0x140159a10;

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

extern "C" void* HighlightDescription_Ctor_Impl(void* mem);

extern "C" void* HighlightDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    *(uint64_t*)(s + 0x20) = 0;   // groupArray
    *(uint64_t*)(s + 0x28) = 0;   // groupCount + cap
    *(uint64_t*)(s + 0x30) = 0;   // ref0
    *(uint64_t*)(s + 0x38) = 0;   // ref1
    return mem;
}

// Serialize (vtable +0x28, exe 0x14004c9d0)
extern "C" void HighlightDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int ||
        !fn_Array_Serialize_External_GraphicsMesh || !fn_ExternalPtr_Serialize3) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    g_fn_Serialize_Int(ser, s + 0x28, nullptr);            // groupCount
    int count = *(int32_t*)(s + 0x28);

    bool loading = (*(int32_t*)(*(uintptr_t*)ser + 8) == 0);
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(s + 0x20, count, 0x10));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x10)     = 0;             // refArray
            *(uint64_t*)(e + i * 0x10 + 8) = 0;             // refCount + cap
        }
    } else {
        e = *(uint8_t**)(s + 0x20);
    }

    for (int i = 0; i < count; ++i, e += 0x10) {
        g_fn_Serialize_Int(ser, e + 8, nullptr);            // refCount (elem+8)
        int refCount = *(int32_t*)(e + 8);
        if (loading) {
            uint8_t* r = reinterpret_cast<uint8_t*>(AllocArray(e, refCount, 8));
            for (int j = 0; j < refCount; ++j) *(uint64_t*)(r + j * 8) = 0;
        }
        fn_Array_Serialize_External_GraphicsMesh(ser, refCount, *(void**)e);
    }

    fn_ExternalPtr_Serialize3(s + 0x30, ser);              // ref0 (+0x30)
    fn_ExternalPtr_Serialize3(s + 0x38, ser);              // ref1 (+0x38)

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// Dtor (vtable +0x00, exe 0x140006fe0)
extern "C" void* HighlightDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t* e = *(uint8_t**)(s + 0x20);
    int count = *(int32_t*)(s + 0x28);
    if (e && fn_MemFreeStatic) {
        for (int i = 0; i < count; ++i) {
            void* ref = *(void**)(e + i * 0x10);
            if (ref) fn_MemFreeStatic(ref);
        }
        fn_MemFreeStatic(e);
    }
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x140006f70)
extern "C" void HighlightDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    HighlightDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x140006fd0)
extern "C" void* HighlightDescription_Slot8(void* self) {
    if (!self) return nullptr;
    HighlightDescription_Ctor_Impl(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HighlightDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HighlightDescription");
    return cached;
}
static int  HighlightDescription_Ver(void*)        { return 2; }
static int  HighlightDescription_Ret0(void*)       { return 0; }
static void* HighlightDescription_RetNull(void*, ...) { return nullptr; }

static void* g_HighlightDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HighlightDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&HighlightDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&HighlightDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&HighlightDescription_Ver),        // +0x18 GetVersion (2)
    reinterpret_cast<void*>(&HighlightDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&HighlightDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&HighlightDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&HighlightDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&HighlightDescription_Slot8),      // +0x40 Slot8
    reinterpret_cast<void*>(&HighlightDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&HighlightDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterHighlightDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_HighlightDescription_Vtable));
}

extern "C" void* HighlightDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    return HighlightDescription_Ctor_Impl(p);
}
extern "C" void* HighlightDescription_CreateInPlace(void* mem) {
    return HighlightDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(HighlightDescription, "Description",
              &HighlightDescription_Factory, &HighlightDescription_CreateInPlace);
#endif // STANDALONE_EXE
