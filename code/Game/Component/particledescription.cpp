// code/Game/Component/particledescription.cpp
// Clean reimplementation of ParticleDescription (Foundation C).
//
// ParticleDescription (0x38 bytes) — extends Object::External (0x20 base).
// Vtable @ 0x14015a310 (exe symbol PTR_ParticleComponent_vslot16_14015a310).
// BaseTypeInfo @ 0x1401c4758, parent "Description", file version 7.
//
// Field layout (from ParticleDescription_Factory @ exe and Serialize @ 0x140053460):
//   +0x20  External* materialRef  (ExternalPtr; FUN_14000ab20)
//   +0x28  void*  emitterArray    (dyn-array of emitter entries, 0x28 stride)
//   +0x30  int32  emitterCount
//
// Emitter element (stride 0x28):
//   elem+0x00 : External* ref   (ExternalPtr; FUN_14000a910)
//   elem+0x08 : String          (8 bytes; String::Serialize)
//   elem+0x10 : bool  b0        (default 0)
//   elem+0x11 : bool  b1        (default 1)
//   elem+0x12 : bool  b2        (default 1)
//   elem+0x13 : bool  b3        (default 0)
//   elem+0x18 : Time  t0        (default 0.0)   — serialized SECOND
//   elem+0x20 : Time  t1        (default 1.0)   — serialized FIRST
//
// Serialize order per emitter (exe 0x140053460):
//   1. ref ExternalPtr (elem+0)
//   2. String (elem+8)
//   3. b0,b1,b2,b3 (elem+0x10..0x13)
//   4. Time t1 (elem+0x20), then Time t0 (elem+0x18)
// Tail: materialRef ExternalPtr (+0x20).
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "system/String.h"
#include <new>
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>

namespace {
constexpr uintptr_t kVtable = 0x14015a310;

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

// Init a Time field (8 bytes) to `seconds` via the Runtime Time ctor.
void InitTime(void* field, float seconds) {
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(field, seconds);
    } else {
        *(uint64_t*)field = 0;
    }
}
} // namespace

extern "C" void* ParticleDescription_Ctor_Impl(void* mem);

extern "C" void* ParticleDescription_Ctor_Impl(void* mem) {
    if (!mem) return nullptr;
    if (fn_External_External) fn_External_External(mem);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)mem = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(mem);
    *(uint64_t*)(s + 0x20) = 0;   // materialRef
    *(uint64_t*)(s + 0x28) = 0;   // emitterArray
    *(uint64_t*)(s + 0x30) = 0;   // emitterCount + cap
    return mem;
}

// Serialize (vtable +0x28, exe 0x140053460)
extern "C" void ParticleDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !g_fn_Time_Serialize || !fn_ExternalPtr_Serialize3) {
        return;
    }
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t ser[24];
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);            // emitterCount   // B.3.4 bridge
    int count = *(int32_t*)(s + 0x30);

    bool loading = (*(int32_t*)(*(uintptr_t*)ser + 8) == 0);
    uint8_t* e;
    if (loading) {
        e = reinterpret_cast<uint8_t*>(AllocArray(s + 0x28, count, 0x28));
        for (int i = 0; i < count; ++i) {
            uint8_t* el = e + i * 0x28;
            *(uint64_t*)(el + 0x00) = 0;       // ref
            ::new (static_cast<void*>(el + 0x08)) System::String();  // B.3.1 bridge: String
            *(uint8_t*)(el + 0x10) = 0;
            *(uint8_t*)(el + 0x11) = 1;
            *(uint8_t*)(el + 0x12) = 1;
            *(uint8_t*)(el + 0x13) = 0;
            InitTime(el + 0x18, 0.0f);          // t0 default 0.0
            InitTime(el + 0x20, 1.0f);          // t1 default 1.0 (g_fUVScale)
        }
    } else {
        e = *(uint8_t**)(s + 0x28);
    }

    for (int i = 0; i < count; ++i, e += 0x28) {
        fn_ExternalPtr_Serialize3(e + 0x00, ser);           // ref (elem+0)
        reinterpret_cast<System::String*>(e + 0x08)->Serialize(ser);               // B.3.1 bridge: String (elem+8)
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(e + 0x10), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(e + 0x11), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(e + 0x12), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(*reinterpret_cast<bool*>(e + 0x13), nullptr);   // B.3.4 bridge
        g_fn_Time_Serialize(e + 0x20, ser);                 // t1 (elem+0x20) FIRST
        g_fn_Time_Serialize(e + 0x18, ser);                 // t0 (elem+0x18) SECOND
    }

    fn_ExternalPtr_Serialize3(s + 0x20, ser);              // materialRef (+0x20)

    if (g_fn_Serializer_Dtor) reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

// Dtor (vtable +0x00, exe 0x14000a500)
extern "C" void* ParticleDescription_Dtor(void* self, uint32_t flags) {
    if (!self) return self;
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kVtable);
    *(void**)self = s_vtable;

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uint8_t* e = *(uint8_t**)(s + 0x28);
    int count = *(int32_t*)(s + 0x30);
    if (e) {
        for (int i = 0; i < count; ++i) reinterpret_cast<System::String*>(e + i * 0x28 + 0x08)->~String();  // B.3.1 bridge
        if (fn_MemFreeStatic) fn_MemFreeStatic(e);
    }
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;

    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Cleanup (vtable +0x30, exe 0x14000a490)
extern "C" void ParticleDescription_Cleanup(void* self) {
    typedef void* (*fn_dtor_t)(void*, uint32_t);
    fn_dtor_t dtor = *(fn_dtor_t*)(*(uintptr_t*)self);
    if (dtor) dtor(self, 0);
    ParticleDescription_Ctor_Impl(self);
}

// Slot8 (vtable +0x40, exe 0x14000a4f0)
extern "C" void* ParticleDescription_Slot8(void* self) {
    if (!self) return nullptr;
    ParticleDescription_Ctor_Impl(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ParticleDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ParticleDescription");
    return cached;
}
static int  ParticleDescription_Ver(void*)        { return 7; }
static int  ParticleDescription_Ret0(void*)       { return 0; }
static void* ParticleDescription_RetNull(void*, ...) { return nullptr; }

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* ParticleDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "ParticleComponent"); }

static void* g_ParticleDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ParticleDescription_Dtor),       // +0x00
    reinterpret_cast<void*>(&ParticleDescription_RetNull),    // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&ParticleDescription_GetTypeId),  // +0x10 GetTypeId
    reinterpret_cast<void*>(&ParticleDescription_Ver),        // +0x18 GetVersion (7)
    reinterpret_cast<void*>(&ParticleDescription_Ret0),       // +0x20 Compile
    reinterpret_cast<void*>(&ParticleDescription_Serialize),  // +0x28 Serialize
    reinterpret_cast<void*>(&ParticleDescription_Cleanup),    // +0x30 Cleanup
    reinterpret_cast<void*>(&ParticleDescription_Ret0),       // +0x38 GetDesc
    reinterpret_cast<void*>(&ParticleDescription_GetComponentTypeId),      // +0x40 Slot8
    reinterpret_cast<void*>(&ParticleDescription_RetNull),    // +0x48 guard
    reinterpret_cast<void*>(&ParticleDescription_RetNull),    // +0x50 guard
};

extern "C" void RegisterParticleDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_ParticleDescription_Vtable));
}

extern "C" void* ParticleDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return ParticleDescription_Ctor_Impl(p);
}
extern "C" void* ParticleDescription_CreateInPlace(void* mem) {
    return ParticleDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(ParticleDescription, "Description",
              &ParticleDescription_Factory, &ParticleDescription_CreateInPlace);
