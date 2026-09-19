// code/Game/Component/statusicondescription.cpp
// Clean reimplementation of StatusIconDescription (entity sub-description).
//
// StatusIconDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x1401646a0.  GetVersion = 1.
//
// Field layout (from Factory + Serialize FUN_14012c180 + dtor FUN_140038a60):
//   +0x20: void*  _descRef       ExternalPtr (Description) field (default 0)
//   +0x28: float  _scale         (default 1.0)
//   +0x2c: float  _scale2        (default 1.0, not serialized — runtime/cached)
//   +0x30: bool   _flag          (default 0; serialized)
//
// Serialize (FUN_14012c180) stream order:
//   1. ExternalPtr serialize (+0x20)   FUN_14000bd30
//   2. float (+0x28)
//   3. bool  (+0x30)
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {
constexpr uintptr_t kVtable  = 0x1401646a0;
constexpr int       kVersion = 1;
constexpr uint32_t  kOne_f   = 0x3f800000;  // 1.0f bit pattern
}

static void* StatusIconDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint64_t*)(s + 0x20) = 0;        // _descRef
    *(uint32_t*)(s + 0x28) = kOne_f;   // _scale
    *(uint32_t*)(s + 0x2c) = kOne_f;   // _scale2
    *(uint32_t*)(s + 0x30) = 0;        // _flag
    return mem;
}

// Serialize (vtable +0x28, FUN_14012c180)
void StatusIconDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    alignas(8) uint8_t serializer[24];
    if (!fn_ExternalPtr_Serialize2) {
        return;
    }
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // 1. ExternalPtr (+0x20) — see ClearAreaDescription note re: typeId trust.
    fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);
    // 2. float (+0x28)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x28), nullptr);   // B.3.4 bridge
    // 3. bool (+0x30)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x30), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// Dtor (vtable +0x00, FUN_140038a60) — scalar fields only, no heap.
void* StatusIconDescription_Dtor(void* self, uint32_t flags) {
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t StatusIconDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("StatusIconDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* StatusIconDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "StatusIconComponent"); }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&StatusIconDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                 // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&StatusIconDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),                 // +0x18 (=1)
    reinterpret_cast<void*>(&Vtbl_Return0),                    // +0x20 Compile
    reinterpret_cast<void*>(&StatusIconDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                    // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                    // +0x38 GetDesc
    reinterpret_cast<void*>(&StatusIconDescription_GetComponentTypeId),                 // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                 // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                 // +0x50 guard
};

extern "C" void RegisterStatusIconDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* StatusIconDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return StatusIconDescription_Ctor_Impl(p);
}
extern "C" void* StatusIconDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return StatusIconDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(StatusIconDescription, "Description",
              &StatusIconDescription_Factory, &StatusIconDescription_CreateInPlace);
