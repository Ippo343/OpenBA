// code/Game/Component/absorbresourcedescription.cpp
// Clean reimplementation of AbsorbResourceDescription (Object::External "Description").
//
// AbsorbResourceDescription (0x20 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140162090.
//
// Carries NO serialized fields. Factory (0x14014e450 area) allocates 0x20 bytes,
// runs External base ctor, sets vtable. Serialize (0x140105d50) just ctor+dtor a
// Serializer (reads nothing). GetVersion (0x140105d90) = 1.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {
constexpr uintptr_t kVtableAddr    = 0x140162090;
constexpr uintptr_t kSerializeAddr = 0x140105d50;
constexpr uint32_t  kFileVersion   = 1;
} // namespace

static void* AbsorbResourceDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(mem) = reinterpret_cast<void*>(vt);
    return mem;
}

void AbsorbResourceDescription_Serialize(void* self, void* stream) {
    (void)self;
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

void* AbsorbResourceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(self) = reinterpret_cast<void*>(vt);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AbsorbResourceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AbsorbResourceDescription");
    return cached;
}

static void* AbsorbResourceDescription_Dtor_Tramp(void* self, uint32_t flags) {
    return AbsorbResourceDescription_Dtor(self, flags);
}
static void  AbsorbResourceDescription_Serialize_Tramp(void* self, void* stream) {
    AbsorbResourceDescription_Serialize(self, stream);
}
static int   AbsorbResourceDescription_VtblReturn1(void* /*self*/) { return (int)kFileVersion; }
static int   AbsorbResourceDescription_VtblReturn0(void* /*self*/) { return 0; }
static void* AbsorbResourceDescription_VtblReturnNull(void* /*self*/, ...) { return nullptr; }

extern "C" void* AbsorbResourceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return AbsorbResourceDescription_Ctor_Impl(memory);
}

static void  AbsorbResourceDescription_Cleanup_Tramp(void* self) {
    typedef void* (*dtor_t)(void*, uint32_t);
    dtor_t d = *reinterpret_cast<dtor_t*>(*reinterpret_cast<uintptr_t*>(self));
    if (d) d(self, 0);
    AbsorbResourceDescription_Ctor_Impl(self);
}
static void* AbsorbResourceDescription_Slot8_Tramp(void* self) {
    if (!self) return nullptr;
    AbsorbResourceDescription_Ctor_Impl(self);
    return self;
}

// Vtable @ 0x140162090 (Object::External, verified via read_memory):
//   +0x00 Dtor 0x14002eb90 | +0x08 0x14002ea70 | +0x10 GetTypeId 0x14002ea60
//   +0x18 GetVersion 0x140105d90 (mov eax,1) | +0x20 Compile 0x14003a912
//   +0x28 Serialize 0x140105d50 (empty) | +0x30 Cleanup 0x14002eb30
//   +0x38 GetDesc 0x140001640 | +0x40 Slot8 0x14002eb80 | +0x48 0x14002eae0
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AbsorbResourceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AbsorbResourceComponent"); }

static void* g_AbsorbResourceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&AbsorbResourceDescription_Dtor_Tramp),      // +0x00
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturnNull),  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&AbsorbResourceDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturn1),     // +0x18 GetVersion
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturn0),     // +0x20 Compile
    reinterpret_cast<void*>(&AbsorbResourceDescription_Serialize_Tramp), // +0x28
    reinterpret_cast<void*>(&AbsorbResourceDescription_Cleanup_Tramp),   // +0x30
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturn0),     // +0x38 GetDesc
    reinterpret_cast<void*>(&AbsorbResourceDescription_GetComponentTypeId),     // +0x40
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturnNull),  // +0x48 guard
    reinterpret_cast<void*>(&AbsorbResourceDescription_VtblReturnNull),  // +0x50 guard
};

extern "C" void RegisterAbsorbResourceDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_AbsorbResourceDescription_Vtable));
}

extern "C" void* AbsorbResourceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    return AbsorbResourceDescription_Ctor_Impl(p);
}

REGISTER_TYPE(AbsorbResourceDescription, "Description",
              &AbsorbResourceDescription_Factory, &AbsorbResourceDescription_CreateInPlace);

