// code/Game/Component/clearareadescription.cpp
// Clean reimplementation of ClearAreaDescription (entity sub-description).
//
// ClearAreaDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140162760.  GetVersion = 6.
//
// Field layout (from Factory + Serialize FUN_1401180f0 + dtor FUN_140031060):
//   +0x20: void* _descRef        ExternalPtr (Description) field (default 0)
//   +0x28: bool  _flag0          (default 1)
//   +0x29: bool  _flag1          (default 0)
//   +0x2a: bool  _flag2          (default 1)
//   +0x2b: bool  _flag3          (default 0)
//   +0x2c: bool  _flag4          (default 1)
//   +0x2d: bool  _flag5          (default 0)
//   +0x30: int32 _value          (default -1)
//
// Serialize (FUN_1401180f0) stream order:
//   1. ExternalPtr serialize (+0x20)        FUN_140002340
//   2..7. bool x6 (+0x28..+0x2d)
//   8. int (+0x30)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable  = 0x140162760;
constexpr int       kVersion = 6;
}

static void* ClearAreaDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;            // _descRef
    *(uint32_t*)(s + 0x28) = 0x00010001;   // _flag0..3 = 1,0,1,0
    *(uint16_t*)(s + 0x2c) = 0x0001;       // _flag4,5  = 1,0
    *(uint32_t*)(s + 0x30) = 0xffffffff;   // _value = -1
    return mem;
}

// Serialize (vtable +0x28, FUN_1401180f0)
void ClearAreaDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // 1. ExternalPtr (+0x20). Same stream layout as the StringTable ExternalPtr
    //    helper (FUN_14000bf40); only the validated typeId differs (trusted in
    //    standalone via IsDerivedFromOrTrust).
    fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);

    // 2..7. six bools
    for (int i = 0; i < 6; ++i) {
        g_fn_Serialize_Bool(serializer, (void*)(s + 0x28 + i), nullptr);
    }
    // 8. int (+0x30)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x30), nullptr);

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, FUN_140031060) — scalar fields only, no heap.
void* ClearAreaDescription_Dtor(void* self, uint32_t flags) {
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ClearAreaDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ClearAreaDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* ClearAreaDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "ClearAreaComponent"); }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&ClearAreaDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ClearAreaDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),                // +0x18 (=6)
    reinterpret_cast<void*>(&Vtbl_Return0),                   // +0x20 Compile
    reinterpret_cast<void*>(&ClearAreaDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                   // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                   // +0x38 GetDesc
    reinterpret_cast<void*>(&ClearAreaDescription_GetComponentTypeId),                // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                // +0x50 guard
};

extern "C" void RegisterClearAreaDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* ClearAreaDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return ClearAreaDescription_Ctor_Impl(p);
}
extern "C" void* ClearAreaDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return ClearAreaDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(ClearAreaDescription, "Description",
              &ClearAreaDescription_Factory, &ClearAreaDescription_CreateInPlace);
