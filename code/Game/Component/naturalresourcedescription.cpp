// code/Game/Component/naturalresourcedescription.cpp
// Clean reimplementation of NaturalResourceDescription (Object::External "Description").
//
// NaturalResourceDescription (0x88 bytes) — extends Object::External (0x20 base).
// Vtable at 0x140163e90.  GetVersion = 4.
//
// Field layout (from ctor NaturalResourceDescription_sub_6010 @ 0x140126010 +
// Serialize FUN_140125e10 @ 0x140125e10):
//   +0x20 ExternalPtr<BuildingDescription>  (FUN_140002340 serialize)
//   +0x28 float (1.0)   +0x2c float (4.0)   +0x30 float (1.0)  (+0x34 float, high dword of +0x30 write = 0)
//   +0x38 float (0.5)   (+0x3c float, high dword of +0x38 write = 0)
//   +0x40 float (16.0)  +0x44 float (0.95)
//   +0x48 bool (0)
//   +0x4c float (1.0)   +0x50 float (1.0)   +0x54 float (1.0)  (+0x58 float, high dword of +0x54 write = 0)
//   +0x5c int (1)       (+0x60 float, high dword of +0x5c write = 0)
//   +0x64 bool (0)
//   +0x68 Time (1.0)    +0x70 Time (1.0)
//   +0x78 ExternalRef<Profession>  (FUN_140018020 serialize)
//   +0x80 int/enum (0xb = 11)
//
// Serialize stream order (EXACT, from FUN_140125e10):
//   ExternalPtr(+0x20); float +0x28,+0x2c,+0x30,+0x34,+0x38; bool +0x48;
//   float +0x4c,+0x50,+0x54,+0x58; int +0x5c; float +0x3c,+0x40,+0x44; bool +0x64;
//   Time +0x68; Time +0x70; enum(serializer vtbl +0x98) +0x80; ExternalRef +0x78;
//   float +0x60.
//
// Decompiled from Application-x64.exe (read-only Ghidra recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

namespace {
constexpr uintptr_t kVtableAddr    = 0x140163e90;
constexpr uintptr_t kSerializeAddr = 0x140125e10;
constexpr uint32_t  kFileVersion   = 4;

// FUN_140018020 — ExternalRef<Profession> serialize (exe-internal; reads bool +
// String name then ExternalFactory::FindOrLoad → resolves to a Profession*).
// REPORTED for engine_funcs.h; resolved here via ResolveAddr (standalone needs it
// registered in address_registry.cpp).
typedef void (*fn_ExternalRefSerialize_t)(void* refField, void* serializer);

inline void SetF(void* base, uintptr_t off, uint32_t bits) {
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(base) + off) = bits;
}
} // namespace

static void* NaturalResourceDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(mem) = reinterpret_cast<void*>(vt);

    uint8_t* p = reinterpret_cast<uint8_t*>(mem);
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;          // ExternalPtr (null)
    SetF(mem, 0x28, 0x3f800000);  // 1.0
    SetF(mem, 0x2c, 0x40800000);  // 4.0
    SetF(mem, 0x30, 0x3f800000);  // 1.0   (8-byte write → +0x34 = 0)
    SetF(mem, 0x34, 0x00000000);
    SetF(mem, 0x38, 0x3f000000);  // 0.5   (8-byte write → +0x3c = 0)
    SetF(mem, 0x3c, 0x00000000);
    SetF(mem, 0x40, 0x41800000);  // 16.0
    SetF(mem, 0x44, 0x3f733333);  // 0.95
    *(p + 0x48) = 0;              // bool
    SetF(mem, 0x4c, 0x3f800000);  // 1.0
    SetF(mem, 0x50, 0x3f800000);  // 1.0
    SetF(mem, 0x54, 0x3f800000);  // 1.0   (8-byte write → +0x58 = 0)
    SetF(mem, 0x58, 0x00000000);
    *reinterpret_cast<uint64_t*>(p + 0x5c) = 1;          // int +0x5c = 1 (8-byte write → +0x60 = 0)
    *(p + 0x64) = 0;              // bool
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(p + 0x68, 1.0f);
        fn_Time_Ctor_Float(p + 0x70, 1.0f);
    } else {
        *reinterpret_cast<uint64_t*>(p + 0x68) = 0;
        *reinterpret_cast<uint64_t*>(p + 0x70) = 0;
    }
    *reinterpret_cast<uint64_t*>(p + 0x78) = 0;          // ExternalRef<Profession> (null)
    *reinterpret_cast<int32_t*>(p + 0x80) = 0xb;         // enum default 11
    return mem;
}

void NaturalResourceDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_Serializer_Serialize_Float || !g_fn_Serialize_Bool ||
        !g_fn_Serialize_Int || !fn_ExternalPtr_BuildSerialize || !g_fn_Time_Serialize) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // +0x20 ExternalPtr<BuildingDescription>
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x34), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x38), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x48), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x4c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x50), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x54), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x58), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x5c), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x3c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x40), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x44), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x64), nullptr);   // B.3.4 bridge

    // +0x68, +0x70 System::Time::Serialize
    g_fn_Time_Serialize(s + 0x68, serializer);
    g_fn_Time_Serialize(s + 0x70, serializer);

    // +0x80 enum through the UINT door (the same dispatch StatGoal uses).
    // `serializer + 8` was the READERWRITER, not an Encoding, and vtable +0x98
    // is rw_vtbl::kScalarUInt.
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x80), nullptr);   // B.3.4 bridge  (0x98 = uint door)

    // +0x78 ExternalRef<Profession> (exe FUN_140018020). Resolved via ResolveAddr;
    // works once 0x140018020 is registered.
    {
        fn_ExternalRefSerialize_t f =
            reinterpret_cast<fn_ExternalRefSerialize_t>(ResolveAddr(Module::Exe, 0x140018020));
        if (f) f(s + 0x78, serializer);
    }

    // +0x60 float
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x60), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

void* NaturalResourceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kVtableAddr);
    if (vt) *reinterpret_cast<void**>(self) = reinterpret_cast<void*>(vt);
    // Fields are scalar + ExternalPtr/ExternalRef raw pointers + Time (POD).
    // The ExternalPtr/Ref hold non-owning pointers; nothing to free here.
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// ============================================================
// STANDALONE_EXE: Factory + Vtable + REGISTER_TYPE
// ============================================================

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t NaturalResourceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("NaturalResourceDescription");
    return cached;
}

static void* NaturalResourceDescription_Dtor_Tramp(void* self, uint32_t flags) {
    return NaturalResourceDescription_Dtor(self, flags);
}
static void  NaturalResourceDescription_Serialize_Tramp(void* self, void* stream) {
    NaturalResourceDescription_Serialize(self, stream);
}
static int   NaturalResourceDescription_VtblReturn4(void* /*self*/) { return (int)kFileVersion; }
static int   NaturalResourceDescription_VtblReturn0(void* /*self*/) { return 0; }
static void* NaturalResourceDescription_VtblReturnNull(void* /*self*/, ...) { return nullptr; }

extern "C" void* NaturalResourceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    return NaturalResourceDescription_Ctor_Impl(memory);
}

static void  NaturalResourceDescription_Cleanup_Tramp(void* self) {
    typedef void* (*dtor_t)(void*, uint32_t);
    dtor_t d = *reinterpret_cast<dtor_t*>(*reinterpret_cast<uintptr_t*>(self));
    if (d) d(self, 0);
    NaturalResourceDescription_Ctor_Impl(self);
}
static void* NaturalResourceDescription_Slot8_Tramp(void* self) {
    if (!self) return nullptr;
    NaturalResourceDescription_Ctor_Impl(self);
    return self;
}

// Vtable @ 0x140163e90 (Object::External, verified via read_memory):
//   +0x00 Dtor 0x140036650 | +0x08 0x1400365c0 | +0x10 GetTypeId 0x1400365b0
//   +0x18 GetVersion 0x140125fc0 (mov eax,4) | +0x20 Compile 0x14003a912
//   +0x28 Serialize 0x140125e10 | +0x30 Cleanup 0x140036620
//   +0x38 GetDesc 0x140001640 | +0x40 Slot8 0x140036640 | +0x48 0x140036610
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* NaturalResourceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "NaturalResourceComponent"); }

static void* g_NaturalResourceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&NaturalResourceDescription_Dtor_Tramp),      // +0x00
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturnNull),  // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&NaturalResourceDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturn4),     // +0x18 GetVersion
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturn0),     // +0x20 Compile
    reinterpret_cast<void*>(&NaturalResourceDescription_Serialize_Tramp), // +0x28
    reinterpret_cast<void*>(&NaturalResourceDescription_Cleanup_Tramp),   // +0x30
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturn0),     // +0x38 GetDesc
    reinterpret_cast<void*>(&NaturalResourceDescription_GetComponentTypeId),     // +0x40
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturnNull),  // +0x48 guard
    reinterpret_cast<void*>(&NaturalResourceDescription_VtblReturnNull),  // +0x50 guard
};

extern "C" void RegisterNaturalResourceDescriptionVtable() {
    RegisterExeAddr(kVtableAddr, reinterpret_cast<uintptr_t>(g_NaturalResourceDescription_Vtable));
}

extern "C" void* NaturalResourceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x88, 8);
    if (!p) return nullptr;
    return NaturalResourceDescription_Ctor_Impl(p);
}

REGISTER_TYPE(NaturalResourceDescription, "Description",
              &NaturalResourceDescription_Factory, &NaturalResourceDescription_CreateInPlace);

