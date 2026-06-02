// code/Game/Component/storagedisplaydescription.cpp
// Clean reimplementation of StorageDisplayDescription (Foundation C).
//
// StorageDisplayDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x14015a800 (exe symbol PTR_StorageDisplayComponent_vslot16_14015a800).
// BaseTypeInfo @ 0x1401c4988, parent "Description", file version 1.
//
// No own fields beyond the 0x20 External base. Serialize @ 0x140056a80 only
// constructs/destructs the Serializer (no field reads) — a header/marker block.
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable = 0x14015a800;
} // namespace

extern "C" void* StorageDisplayDescription_Ctor_Impl(void* mem);

extern "C" void* StorageDisplayDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    return mem;
}

// Serialize (vtable +0x28, exe 0x140056a80) — no fields.
extern "C" void StorageDisplayDescription_Serialize(void* self, void* stream) {
    (void)self;
    if (!g_fn_Serializer_Ctor) return;
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// Dtor (vtable +0x00, exe 0x14000c8e0)
extern "C" void* StorageDisplayDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x14000c880)
extern "C" void StorageDisplayDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    StorageDisplayDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x14000c8d0)
extern "C" void* StorageDisplayDescription_Slot8(void* self) {
    if (!self) return nullptr;
    StorageDisplayDescription_Ctor_Impl(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t StorageDisplayDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageDisplayDescription");
    return cached;
}
static int  StorageDisplayDescription_Ver(void*)        { return 1; }
static int  StorageDisplayDescription_Ret0(void*)       { return 0; }
static void* StorageDisplayDescription_RetNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* StorageDisplayDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "StorageDisplayComponent"); }

static void* g_StorageDisplayDescription_Vtable[11] = {
    reinterpret_cast<void*>(&StorageDisplayDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&StorageDisplayDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&StorageDisplayDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&StorageDisplayDescription_Ver),        // +0x18 GetVersion (1)
    reinterpret_cast<void*>(&StorageDisplayDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&StorageDisplayDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&StorageDisplayDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&StorageDisplayDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&StorageDisplayDescription_GetComponentTypeId),      // +0x40 Slot8
    reinterpret_cast<void*>(&StorageDisplayDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&StorageDisplayDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterStorageDisplayDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_StorageDisplayDescription_Vtable));
}

extern "C" void* StorageDisplayDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    return StorageDisplayDescription_Ctor_Impl(p);
}
extern "C" void* StorageDisplayDescription_CreateInPlace(void* mem) {
    return StorageDisplayDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(StorageDisplayDescription, "Description",
              &StorageDisplayDescription_Factory, &StorageDisplayDescription_CreateInPlace);
#endif // STANDALONE_EXE
