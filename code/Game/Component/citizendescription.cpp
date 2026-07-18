// code/Game/Component/citizendescription.cpp
// Clean reimplementation of CitizenDescription (Object::External "Description").
//
// CitizenDescription (0x148 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140162600.  GetVersion = 17 (0x11).
//
// This is the field-heavy citizen tuning block (Template\Citizen.rsc). Layout +
// defaults come from ctor FUN_140114e70 @ 0x140114e70; serialize stream order
// from CitizenComponent_OnCreate (Serialize) @ 0x140114900.
//
// Special members:
//   +0x20,+0x28,+0x30  ExternalRef<Profession> (FUN_140018020, single ptr each)
//   +0xdc..+0xe7       float[3] bulk-serialized (count=3, slot 0x140151ea8)
//   +0x138 / +0x140    dynamic External* array (ptr / count): resize via
//                      FUN_140001bb0, element-serialize via FUN_14001bcc0
//
// Serialize stream order (EXACT, from 0x140114900):
//   float: +0x44, +0x6c,+0x70,+0x74,+0x78,+0x7c,+0x80,+0x84,+0x88,+0x8c,+0x90,
//          +0x38,+0x3c,+0x40, +0xc0, +0xd8;
//   floatArray3(+0xdc);
//   float: +0x48,+0x4c,+0x50,+0x54,+0x58,+0x5c,+0x60, +0xc4,+0xd4,+0xc8,+0xcc,+0xd0,
//          +0xe8,+0xec,+0xf0,+0xf4;
//   int +0x110;
//   int countTmp=+0x140 (serialize); if loading resize(+0x138,count); refArray(+0x138,count);
//   float +0x114; int +0x118; float +0x120,+0x11c,+0x128,+0x124,+0x130;
//   float +0xa4; int +0xa8,+0xac; float +0xa0; int +0xb0; float +0xb4,+0xb8; int +0xbc;
//   ExternalRef +0x20; ExternalRef +0x28; ExternalRef +0x30;
//   float +0x64,+0x68; int +0xf8,+0xfc,+0x100,+0x104,+0x108; float +0x10c.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtableAddr    = 0x140162600;
constexpr uintptr_t kSerializeAddr = 0x140114900;
constexpr uint32_t  kFileVersion   = 17;

// exe-internal helpers (REPORTED for engine_funcs.h; resolved via ResolveAddr).
typedef void (*fn_ExternalRefSerialize_t)(void* refField, void* serializer);            // FUN_140018020
typedef void (*fn_RefArrayResize_t)(void* arrayHeader, int newCount);                   // FUN_140001bb0
typedef void (*fn_RefArraySerialize_t)(void* serializer, int count, void* base, void* p4); // FUN_14001bcc0
// IO::Serializer::Serialize(float[]) dispatch slot 0x140151ea8.
typedef void (*fn_SerFloatArray_t)(void* serializer, int count, float* base, const char* name);

inline void SetF(void* base, uintptr_t off, uint32_t bits) {
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + off) = bits;
}
inline void SetI(void* base, uintptr_t off, int32_t v) {
    *reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(base) + off) = v;
}
} // namespace

static void* CitizenDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(mem) = reinterpret_cast<void*>(vt);

    uint8_t* p = reinterpret_cast<uint8_t*>(mem);
    // ExternalRef<Profession> ptrs (null)
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x30) = 0;
    // float / int defaults (exact bit patterns from FUN_140114e70)
    SetF(mem, 0x38, 0x41700000); SetF(mem, 0x3c, 0x40000000); SetF(mem, 0x40, 0x3991a2b4);
    SetF(mem, 0x44, 0x40000000); SetF(mem, 0x48, 0x40000000); SetF(mem, 0x4c, 0x40400000);
    SetF(mem, 0x50, 0x40000000); SetF(mem, 0x54, 0x41000000); SetF(mem, 0x58, 0x44610000);
    SetF(mem, 0x5c, 0x44e10000); SetF(mem, 0x60, 0x45a8c000); SetF(mem, 0x64, 0x3c23d70a);
    SetF(mem, 0x68, 0x3dcccccd); SetF(mem, 0x6c, 0x40c00000); SetF(mem, 0x70, 0x41200000);
    SetF(mem, 0x74, 0x41000000); SetF(mem, 0x78, 0x40400000); SetF(mem, 0x7c, 0x3f800000);
    SetF(mem, 0x80, 0x3f800000); SetF(mem, 0x84, 0x3f800000); SetF(mem, 0x88, 0x3f800000);
    SetF(mem, 0x8c, 0x3f800000); SetF(mem, 0x90, 0x3f800000); SetF(mem, 0x94, 0x3f800000);
    SetF(mem, 0x98, 0x3f000000); SetF(mem, 0x9c, 0x41400000); SetF(mem, 0xa0, 0x3e800000);
    SetF(mem, 0xa4, 0x41200000); SetI(mem, 0xa8, 4); SetI(mem, 0xac, 0xc);
    SetI(mem, 0xb0, 3);          SetF(mem, 0xb4, 0x3f19999a); SetF(mem, 0xb8, 0x3e800000);
    SetI(mem, 0xbc, 0x14);       SetF(mem, 0xc0, 0x42c80000); SetF(mem, 0xc4, 0x42c80000);
    SetF(mem, 0xc8, 0x425c0000); SetF(mem, 0xcc, 0x42960000); SetF(mem, 0xd0, 0x42c80000);
    SetF(mem, 0xd4, 0x41c80000); SetF(mem, 0xd8, 0x3dcccccd);
    SetF(mem, 0xdc, 0x41200000); SetF(mem, 0xe0, 0x41200000); SetF(mem, 0xe4, 0x41200000);
    SetF(mem, 0xe8, 0x42480000); SetF(mem, 0xec, 0x42960000); SetF(mem, 0xf0, 0x42c80000);
    SetF(mem, 0xf4, 0x42400000); SetI(mem, 0xf8, 0x3c);       SetI(mem, 0xfc, 0x3c);
    SetI(mem, 0x100, 2000);      SetI(mem, 0x104, 6);         SetI(mem, 0x108, 200);
    SetF(mem, 0x10c, 0x42340000); SetI(mem, 0x110, 100);      SetF(mem, 0x114, 0x40a00000);
    SetI(mem, 0x118, 200);       SetF(mem, 0x11c, 0x40800000); SetF(mem, 0x120, 0x40a00000);
    SetF(mem, 0x124, 0x40a00000); SetF(mem, 0x128, 0x40000000); SetF(mem, 0x12c, 0x3c23d70a);
    SetF(mem, 0x130, 0x42700000);
    // dynamic External* array header at +0x138 (ptr) / +0x140 (count) / +0x144 (cap)
    *reinterpret_cast<uint64_t*>(p + 0x138) = 0;
    *reinterpret_cast<uint64_t*>(p + 0x140) = 0;  // count + cap zeroed
    return mem;
}

void CitizenDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_Serializer_Serialize_Float || !g_fn_Serialize_Int) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    auto F = [&](uintptr_t off) {
        fn_Serializer_Serialize_Float(serializer, reinterpret_cast<float*>(s + off), nullptr);
    };
    auto I = [&](uintptr_t off) {
        g_fn_Serialize_Int(serializer, s + off, nullptr);
    };

    F(0x44);
    F(0x6c); F(0x70); F(0x74); F(0x78); F(0x7c); F(0x80); F(0x84); F(0x88); F(0x8c); F(0x90);
    F(0x38); F(0x3c); F(0x40);
    F(0xc0);
    F(0xd8);

    // float[3] bulk at +0xdc (dispatch slot 0x140151ea8)
    {
        fn_SerFloatArray_t f = nullptr;
        uintptr_t slot = ResolveAddr(Module::Exe, 0x140151ea8);
        if (slot) f = *reinterpret_cast<fn_SerFloatArray_t*>(slot);
        if (f) f(serializer, 3, reinterpret_cast<float*>(s + 0xdc), nullptr);
        else { F(0xdc); F(0xe0); F(0xe4); }  // stream-equivalent fallback
    }

    F(0x48); F(0x4c); F(0x50); F(0x54); F(0x58); F(0x5c); F(0x60);
    F(0xc4); F(0xd4); F(0xc8); F(0xcc); F(0xd0);
    F(0xe8); F(0xec); F(0xf0); F(0xf4);

    I(0x110);

    // dynamic External* array (count at +0x140, ptr at +0x138)
    {
        int32_t count = *reinterpret_cast<int32_t*>(s + 0x140);
        g_fn_Serialize_Int(serializer, &count, nullptr);   // local temp (mirrors local_res8)
        // If loading (serializer version field == 0) resize the array to `count`.
        void* serObj = *reinterpret_cast<void**>(serializer);  // local_20[0]
        bool loading = serObj && (*reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(serObj) + 8) == 0);
        if (loading) {
            fn_RefArrayResize_t rz =
                reinterpret_cast<fn_RefArrayResize_t>(ResolveAddr(Module::Exe, 0x140001bb0));
            if (rz) rz(s + 0x138, count);
        }
        fn_RefArraySerialize_t sa =
            reinterpret_cast<fn_RefArraySerialize_t>(ResolveAddr(Module::Exe, 0x14001bcc0));
        if (sa) sa(serializer, count, *reinterpret_cast<void**>(s + 0x138), nullptr);
    }

    F(0x114);
    I(0x118);
    F(0x120); F(0x11c); F(0x128); F(0x124); F(0x130);
    F(0xa4);
    I(0xa8); I(0xac);
    F(0xa0);
    I(0xb0);
    F(0xb4); F(0xb8);
    I(0xbc);

    // ExternalRef<Profession> singletons at +0x20, +0x28, +0x30 (FUN_140018020)
    {
        fn_ExternalRefSerialize_t f =
            reinterpret_cast<fn_ExternalRefSerialize_t>(ResolveAddr(Module::Exe, 0x140018020));
        if (f) { f(s + 0x20, serializer); f(s + 0x28, serializer); f(s + 0x30, serializer); }
    }

    F(0x64); F(0x68);
    I(0xf8); I(0xfc); I(0x100); I(0x104); I(0x108);
    F(0x10c);

    g_fn_Serializer_Dtor(serializer);
}

void* CitizenDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(self) = reinterpret_cast<void*>(vt);
    // Free the dynamic External* array buffer at +0x138 (entries are non-owning ptrs).
    uint8_t* p = reinterpret_cast<uint8_t*>(self);
    void* buf = *reinterpret_cast<void**>(p + 0x138);
    if (buf && fn_MemFreeStatic) {
        fn_MemFreeStatic(buf);
        *reinterpret_cast<uint64_t*>(p + 0x138) = 0;
        *reinterpret_cast<uint64_t*>(p + 0x140) = 0;
    }
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CitizenDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CitizenDescription");
    return cached;
}

static void* CitizenDescription_Dtor_Tramp(void* self, uint32_t flags) {
    return CitizenDescription_Dtor(self, flags);
}
static void  CitizenDescription_Serialize_Tramp(void* self, void* stream) {
    CitizenDescription_Serialize(self, stream);
}
static int   CitizenDescription_VtblReturn17(void* /*self*/) { return (int)kFileVersion; }
static int   CitizenDescription_VtblReturn0(void* /*self*/) { return 0; }
static void* CitizenDescription_VtblReturnNull(void* /*self*/, ...) { return nullptr; }

extern "C" void* CitizenDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return CitizenDescription_Ctor_Impl(memory);
}

static void  CitizenDescription_Cleanup_Tramp(void* self) {
    typedef void* (*dtor_t)(void*, uint32_t);
    dtor_t d = *reinterpret_cast<dtor_t*>(*reinterpret_cast<uintptr_t*>(self));
    if (d) d(self, 0);
    CitizenDescription_Ctor_Impl(self);
}
static void* CitizenDescription_Slot8_Tramp(void* self) {
    if (!self) return nullptr;
    CitizenDescription_Ctor_Impl(self);
    return self;
}

// Vtable @ 0x140162600 (Object::External, verified via read_memory):
//   +0x00 Dtor 0x1400301f0 | +0x08 0x140030160 | +0x10 GetTypeId 0x140030150
//   +0x18 GetVersion 0x140114e00 (mov eax,17) | +0x20 Compile 0x14003a912
//   +0x28 Serialize 0x140114900 | +0x30 Cleanup 0x1400301c0
//   +0x38 GetDesc 0x140001640 | +0x40 Slot8 0x1400301e0 | +0x48 0x1400301b0
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* CitizenDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "CitizenComponent"); }

static void* g_CitizenDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CitizenDescription_Dtor_Tramp),      // +0x00
    reinterpret_cast<void*>(&CitizenDescription_VtblReturnNull),  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&CitizenDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&CitizenDescription_VtblReturn17),    // +0x18 GetVersion
    reinterpret_cast<void*>(&CitizenDescription_VtblReturn0),     // +0x20 Compile
    reinterpret_cast<void*>(&CitizenDescription_Serialize_Tramp), // +0x28
    reinterpret_cast<void*>(&CitizenDescription_Cleanup_Tramp),   // +0x30
    reinterpret_cast<void*>(&CitizenDescription_VtblReturn0),     // +0x38 GetDesc
    reinterpret_cast<void*>(&CitizenDescription_GetComponentTypeId),     // +0x40
    reinterpret_cast<void*>(&CitizenDescription_VtblReturnNull),  // +0x48 guard
    reinterpret_cast<void*>(&CitizenDescription_VtblReturnNull),  // +0x50 guard
};

extern "C" void RegisterCitizenDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_CitizenDescription_Vtable));
}

extern "C" void* CitizenDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x148, 8);
    if (!p) return nullptr;
    return CitizenDescription_Ctor_Impl(p);
}

REGISTER_TYPE(CitizenDescription, "Description",
              &CitizenDescription_Factory, &CitizenDescription_CreateInPlace);

