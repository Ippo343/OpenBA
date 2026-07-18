// code/Game/Component/destroydescription.cpp
// Clean reimplementation of DestroyDescription (entity sub-description).
//
// DestroyDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x1401631b0.  GetVersion = 5.
//
// Field layout (from Factory + Serialize FUN_14011de70 + dtor FUN_140033d70):
//   +0x20: void* _descRef        ExternalPtr (Description) field (default 0)
//   +0x28: int32 _val0           (default 1)
//   +0x2c: int32 _val1           (default 1)
//   +0x30: int32 _val2           (default 0)
//
// Serialize (FUN_14011de70) stream order:
//   1. ExternalPtr serialize (+0x20)   FUN_140002340
//   2. int (+0x28)
//   3. int (+0x2c)
//   4. int (+0x30)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable  = 0x1401631b0;
constexpr int       kVersion = 5;
}

static void* DestroyDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;   // _descRef
    *(uint32_t*)(s + 0x28) = 1;   // _val0
    *(uint32_t*)(s + 0x2c) = 1;   // _val1
    *(uint32_t*)(s + 0x30) = 0;   // _val2 (serialized but not factory-set; zeroed)
    return mem;
}

// Serialize (vtable +0x28, FUN_14011de70)
void DestroyDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // 1. ExternalPtr (+0x20) — see ClearAreaDescription note re: typeId trust.
    fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);
    // 2..4. three ints
    g_fn_Serialize_Int(serializer, (void*)(s + 0x28), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x2c), nullptr);
    g_fn_Serialize_Int(serializer, (void*)(s + 0x30), nullptr);

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, FUN_140033d70) — scalar fields only, no heap.
void* DestroyDescription_Dtor(void* self, uint32_t flags) {
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DestroyDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DestroyDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* DestroyDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "DestroyComponent"); }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&DestroyDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&DestroyDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),              // +0x18 (=5)
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x20 Compile
    reinterpret_cast<void*>(&DestroyDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                 // +0x38 GetDesc
    reinterpret_cast<void*>(&DestroyDescription_GetComponentTypeId),              // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),              // +0x50 guard
};

extern "C" void RegisterDestroyDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* DestroyDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return DestroyDescription_Ctor_Impl(p);
}
extern "C" void* DestroyDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return DestroyDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(DestroyDescription, "Description",
              &DestroyDescription_Factory, &DestroyDescription_CreateInPlace);
