// code/Game/Component/storagedescription.cpp
// Clean reimplementation of StorageDescription (Foundation C).
//
// StorageDescription (0x40 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x1401647a0 (exe symbol PTR_StorageComponent_vslot16_1401647a0).
// BaseTypeInfo @ 0x1401c7eb0, parent "Description", file version 2.
//
// Field layout (from StorageDescription_Factory @ exe and Serialize @ 0x14012c9a0):
//   +0x20  bool   flagA               (default 0)
//   +0x21  bool   flagB               (default 0)
//   +0x24  int32  pickupValue         (default 0)
//   +0x28  int32  rawMaterialFlags    (enum; serialized via serializer vtable+0x98)
//   +0x30  void*  volumeArray         (dyn-array of 0x10-stride entries)
//   +0x38  int32  volumeCount
//
// The +0x30 array element layout (stride 0x10):
//   elem+0x00 : External* ref  (ExternalPtr; serialized via fn_ExternalPtr_Serialize2)
//   elem+0x08 : int32          (serialized as int)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

namespace {
constexpr uintptr_t kVtable = 0x1401647a0;

// The Serializer enum dispatch (serializer vtable +0x98): void(self, int* field, const char*).
void SerializeEnum(uint8_t* serializerBuf, void* field) {
    void* serObj = *(void**)(serializerBuf + 8);
    if (!serObj) return;
    typedef void (*fn_t)(void* self, void* field, const char* name);
    void* vt = *(void**)serObj;
    fn_t fn = *(fn_t*)((uintptr_t)vt + 0x98);
    if (fn) fn(serObj, field, nullptr);
}

// Fresh-load alloc for a dyn-array header {ptr@0,count@8,cap@0xC}; returns buffer.
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

extern "C" void StorageDescription_Serialize(void* self, void* stream);
extern "C" void* StorageDescription_Ctor_Impl(void* mem);

// ============================================================
// Ctor (factory body): External base init + vtable + defaults
// ============================================================
extern "C" void* StorageDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);

    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    *(uint16_t*)(s + 0x20) = 0;   // flagA, flagB
    *(uint64_t*)(s + 0x24) = 0;   // pickupValue (+0x24), enum (+0x28), pad
    *(uint64_t*)(s + 0x30) = 0;   // volumeArray
    *(uint64_t*)(s + 0x38) = 0;   // volumeCount + cap
    return mem;
}

// ============================================================
// Serialize (vtable +0x28, exe 0x14012c9a0)
// ============================================================
extern "C" void StorageDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !fn_ExternalPtr_Serialize2) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t ser[24];
    g_fn_Serializer_Ctor(ser, stream, 0);

    SerializeEnum(ser, s + 0x28);                          // rawMaterialFlags (enum)
    g_fn_Serialize_Bool(ser, s + 0x20, nullptr);           // flagA
    g_fn_Serialize_Bool(ser, s + 0x21, nullptr);           // flagB
    g_fn_Serialize_Int(ser, s + 0x24, nullptr);            // pickupValue

    g_fn_Serialize_Int(ser, s + 0x38, nullptr);            // volumeCount
    int count = *(int32_t*)(s + 0x38);

    bool loading = (*(int32_t*)(*(uintptr_t*)ser + 8) == 0);
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(s + 0x30, count, 0x10));
        for (int i = 0; i < count; ++i) {
            *(uint64_t*)(e + i * 0x10)       = 0;          // External* ref
            *(uint64_t*)(e + i * 0x10 + 8)   = 0;          // int (+pad)
        }
    } else {
        e = *(uint8_t**)(s + 0x30);
    }
    for (int i = 0; i < count; ++i, e += 0x10) {
        g_fn_Serialize_Int(ser, e + 8, nullptr);           // int field (elem+8)
        fn_ExternalPtr_Serialize2(e, ser);                 // External* ref (elem+0)
    }

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// ============================================================
// Dtor (vtable +0x00, exe 0x140038cf0)
// ============================================================
extern "C" void* StorageDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_MemFreeStatic && *(void**)(s + 0x30)) fn_MemFreeStatic(*(void**)(s + 0x30));
    *(uint64_t*)(s + 0x30) = 0;
    *(uint64_t*)(s + 0x38) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// Cleanup (vtable +0x30, exe 0x140038c80): dtor(flags=0) then re-init
// ============================================================
extern "C" void StorageDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    StorageDescription_Ctor_Impl(self);
}

// ============================================================
// Slot8 (vtable +0x40, exe 0x140038ce0): reinit-in-place
// ============================================================
extern "C" void* StorageDescription_Slot8(void* self) {
    if (!self) return nullptr;
    StorageDescription_Ctor_Impl(self);
    return self;
}

// ============================================================
// STANDALONE scaffolding
// ============================================================
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t StorageDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StorageDescription");
    return cached;
}
static int  StorageDescription_Ver(void*)        { return 2; }   // file version 2
static int  StorageDescription_Ret0(void*)       { return 0; }
static void* StorageDescription_RetNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* StorageDescription_GetComponentTypeId(void*, void* out) {
    return ::Object::WriteComponentTypeId(out, "StorageComponent");
}

static void* g_StorageDescription_Vtable[11] = {
    reinterpret_cast<void*>(&StorageDescription_Dtor),       // +0x00 Dtor
    reinterpret_cast<void*>(&StorageDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&StorageDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&StorageDescription_Ver),        // +0x18 GetVersion (2)
    reinterpret_cast<void*>(&StorageDescription_Ret0),       // +0x20 Compile (shared, unused on load)
    reinterpret_cast<void*>(&StorageDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&StorageDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&StorageDescription_Ret0),       // +0x38 GetDesc (shared, unused on load)
    reinterpret_cast<void*>(&StorageDescription_GetComponentTypeId), // +0x40 GetComponentTypeId
    reinterpret_cast<void*>(&StorageDescription_RetNull),    // +0x48 over-read guard
    reinterpret_cast<void*>(&StorageDescription_RetNull),    // +0x50 over-read guard
};

extern "C" void RegisterStorageDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_StorageDescription_Vtable));
}

extern "C" void* StorageDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    return StorageDescription_Ctor_Impl(p);
}
extern "C" void* StorageDescription_CreateInPlace(void* mem) {
    return StorageDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(StorageDescription, "Description",
              &StorageDescription_Factory, &StorageDescription_CreateInPlace);
