// code/Game/Component/storagelocationdescription.cpp
// Clean reimplementation of StorageLocationDescription (Foundation C).
//
// StorageLocationDescription (0x28 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x140164910 (exe symbol PTR_StorageLocationComponent_vslot18_140164910).
// BaseTypeInfo @ 0x1401c7f20, parent "Description", file version 2.
//
// Field layout (from StorageLocationDescription_Factory @ exe and Serialize @ 0x14012d470):
//   +0x20  int32  locationType  (enum; serialized via serializer vtable+0x98)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {
constexpr uintptr_t kVtable = 0x140164910;

// The enum field, through the UINT door. `serializerBuf + 8` was the
// READERWRITER (IO::Serializer::_readerWriter), not an Encoding, and its
// vtable +0x98 is rw_vtbl::kScalarUInt.
void SerializeEnum(uint8_t* serializerBuf, void* field) {
    reinterpret_cast<IO::Serializer*>(serializerBuf)->Serialize(*reinterpret_cast<unsigned int*>(field), nullptr);   // B.3.4 bridge  (0x98 = uint door)
}
} // namespace

extern "C" void* StorageLocationDescription_Ctor_Impl(void* mem);

extern "C" void* StorageLocationDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    *(uint32_t*)((uint8_t*)mem + 0x20) = 0;   // locationType
    return mem;
}

// Serialize (vtable +0x28, exe 0x14012d470)
extern "C" void StorageLocationDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    SerializeEnum(ser, s + 0x20);                 // locationType (enum)
    if (g_fn_Serializer_Dtor) reinterpret_cast<IO::Serializer*>(ser)->~Serializer();              // B.3.4 bridge
}

// Dtor (vtable +0x00, exe 0x140038f60)
extern "C" void* StorageLocationDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x140038f00)
extern "C" void StorageLocationDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    StorageLocationDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x140038f50)
extern "C" void* StorageLocationDescription_Slot8(void* self) {
    if (!self) return nullptr;
    StorageLocationDescription_Ctor_Impl(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t StorageLocationDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageLocationDescription");
    return cached;
}
static int  StorageLocationDescription_Ver(void*)        { return 2; }
static int  StorageLocationDescription_Ret0(void*)       { return 0; }
static void* StorageLocationDescription_RetNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* StorageLocationDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "StorageLocationComponent"); }

static void* g_StorageLocationDescription_Vtable[11] = {
    reinterpret_cast<void*>(&StorageLocationDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&StorageLocationDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&StorageLocationDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&StorageLocationDescription_Ver),        // +0x18 GetVersion (2)
    reinterpret_cast<void*>(&StorageLocationDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&StorageLocationDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&StorageLocationDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&StorageLocationDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&StorageLocationDescription_GetComponentTypeId),      // +0x40 Slot8
    reinterpret_cast<void*>(&StorageLocationDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&StorageLocationDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterStorageLocationDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_StorageLocationDescription_Vtable));
}

extern "C" void* StorageLocationDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    return StorageLocationDescription_Ctor_Impl(p);
}
extern "C" void* StorageLocationDescription_CreateInPlace(void* mem) {
    return StorageLocationDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(StorageLocationDescription, "Description",
              &StorageLocationDescription_Factory, &StorageLocationDescription_CreateInPlace);
