// code/Game/Component/characterdescription.cpp
// Clean reimplementation of CharacterDescription (Object::External "Description").
//
// CharacterDescription (0x48 bytes) — extends Object::External (0x20 base).
// Vtable at 0x1401623f0.  GetVersion = 6.
//
// Field layout (from ctor FUN_140109f10 + Serialize 0x140109dd0):
//   +0x20 float  speedScale        (default 0.25  = 0x3e800000)
//   +0x24 float  _slowSpeed?       (default 0.0;  high dword of the 8-byte ctor write at +0x20)
//   +0x28 float  _slowSpeed        (default 0.85  = 0x3f59999a)
//   +0x2c float  _turnFilter?      (default 0.1   = 0x3dcccccd)
//   +0x30 float                    (default 1.0)
//   +0x34 float                    (default 1.0)
//   +0x38 float                    (default 1.0)
//   +0x3c bool                     (default 0)
//   +0x40 float                    (default 0.0)
//   +0x44 float                    (default -10.0 = 0xc1200000)
//
// Serialize stream order (exact, from 0x140109dd0):
//   +0x20,+0x24,+0x28,+0x2c,+0x30,+0x34,+0x38 (float), +0x3c (bool),
//   +0x40,+0x44 (float).
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtableAddr    = 0x1401623f0;
constexpr uintptr_t kSerializeAddr = 0x140109dd0;
constexpr uint32_t  kFileVersion   = 6;

inline void SetF(void* base, uintptr_t off, uint32_t bits) {
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + off) = bits;
}
} // namespace

static void* CharacterDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(mem) = reinterpret_cast<void*>(vt);

    // Field defaults (raw bit patterns, exact from ctor FUN_140109f10).
    SetF(mem, 0x20, 0x3e800000);  // 0.25   (8-byte write: low dword)
    SetF(mem, 0x24, 0x00000000);  // 0.0    (high dword of the +0x20 write)
    SetF(mem, 0x28, 0x3f59999a);  // 0.85
    SetF(mem, 0x2c, 0x3dcccccd);  // 0.1
    SetF(mem, 0x30, 0x3f800000);  // 1.0
    SetF(mem, 0x34, 0x3f800000);  // 1.0
    SetF(mem, 0x38, 0x3f800000);  // 1.0
    *(reinterpret_cast<uint8_t*>(mem) + 0x3c) = 0;  // bool
    SetF(mem, 0x40, 0x00000000);  // 0.0
    SetF(mem, 0x44, 0xc1200000);  // -10.0
    return mem;
}

void CharacterDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_Serializer_Serialize_Float || !g_fn_Serialize_Bool) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x20), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x24), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x28), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x2c), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x30), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x34), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x38), nullptr);
    g_fn_Serialize_Bool(serializer, s + 0x3c, nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x40), nullptr);
    fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + 0x44), nullptr);

    g_fn_Serializer_Dtor(serializer);
}

void* CharacterDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(self) = reinterpret_cast<void*>(vt);
    // All fields are scalar floats/bool — no heap-owning members beyond External base.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CharacterDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CharacterDescription");
    return cached;
}

static void* CharacterDescription_Dtor_Tramp(void* self, uint32_t flags) {
    return CharacterDescription_Dtor(self, flags);
}
static void  CharacterDescription_Serialize_Tramp(void* self, void* stream) {
    CharacterDescription_Serialize(self, stream);
}
static int   CharacterDescription_VtblReturn6(void* /*self*/) { return (int)kFileVersion; }
static int   CharacterDescription_VtblReturn0(void* /*self*/) { return 0; }
static void* CharacterDescription_VtblReturnNull(void* /*self*/, ...) { return nullptr; }

extern "C" void* CharacterDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return CharacterDescription_Ctor_Impl(memory);
}

static void  CharacterDescription_Cleanup_Tramp(void* self) {
    typedef void* (*dtor_t)(void*, uint32_t);
    dtor_t d = *reinterpret_cast<dtor_t*>(*reinterpret_cast<uintptr_t*>(self));
    if (d) d(self, 0);
    CharacterDescription_Ctor_Impl(self);
}
static void* CharacterDescription_Slot8_Tramp(void* self) {
    if (!self) return nullptr;
    CharacterDescription_Ctor_Impl(self);
    return self;
}

// Vtable @ 0x1401623f0 (Object::External, verified via read_memory):
//   +0x00 Dtor 0x14002fa40 | +0x08 0x14002f9b0 | +0x10 GetTypeId 0x14002f9a0
//   +0x18 GetVersion 0x140109ec0 (mov eax,6) | +0x20 Compile 0x14003a912
//   +0x28 Serialize 0x140109dd0 | +0x30 Cleanup 0x14002fa10
//   +0x38 GetDesc 0x140001640 | +0x40 Slot8 0x14002fa30 | +0x48 0x14002fa00
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* CharacterDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "CharacterComponent"); }

static void* g_CharacterDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CharacterDescription_Dtor_Tramp),      // +0x00
    reinterpret_cast<void*>(&CharacterDescription_VtblReturnNull),  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&CharacterDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&CharacterDescription_VtblReturn6),     // +0x18 GetVersion
    reinterpret_cast<void*>(&CharacterDescription_VtblReturn0),     // +0x20 Compile
    reinterpret_cast<void*>(&CharacterDescription_Serialize_Tramp), // +0x28
    reinterpret_cast<void*>(&CharacterDescription_Cleanup_Tramp),   // +0x30
    reinterpret_cast<void*>(&CharacterDescription_VtblReturn0),     // +0x38 GetDesc
    reinterpret_cast<void*>(&CharacterDescription_GetComponentTypeId),     // +0x40
    reinterpret_cast<void*>(&CharacterDescription_VtblReturnNull),  // +0x48 guard
    reinterpret_cast<void*>(&CharacterDescription_VtblReturnNull),  // +0x50 guard
};

extern "C" void RegisterCharacterDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_CharacterDescription_Vtable));
}

extern "C" void* CharacterDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    return CharacterDescription_Ctor_Impl(p);
}

REGISTER_TYPE(CharacterDescription, "Description",
              &CharacterDescription_Factory, &CharacterDescription_CreateInPlace);

