// code/Game/Component/growthdescription.cpp
// GrowthDescription (entity description block) — plant-growth parameters
// (temperature / soil / seasons).
// Extends Object::External (0x20 base). Vtable 0x140163610. Parent "Description".
// Size 0x68. GetVersion=3.
//
// Factory 0x14014f110-area (MemAlloc 0x68 + ctor sub_0b50). Ctor defaults:
//   float @ +0x20 = -1.0f      (0xbf800000)
//   int   @ +0x24 = 0
//   float @ +0x28 = 1.0f
//   bool  @ +0x2c = 0
//   (8)   @ +0x30 = 0
//   bool  @ +0x38 = 0
//   float @ +0x3c = 1.0f
//   float @ +0x40 = 1.0f
//   bool  @ +0x44 = 0
//   (8)   @ +0x48 = 0
//   (8)   @ +0x50 = 0
//   int   @ +0x58 = 3
//   int   @ +0x5c = 3   (8-byte store -> +0x60=0)
//   bool  @ +0x64 = 0
// Serialize (vtable +0x28, 0x140120980), stream order (note non-linear field order):
//   float@+0x20, float@+0x24, float@+0x28, bool@+0x2c, float@+0x30, float@+0x34,
//   bool@+0x38, float@+0x3c, float@+0x40, bool@+0x44, int@+0x4c, int@+0x50,
//   int@+0x48, float@+0x54, int@+0x58, int@+0x5c, float@+0x60, bool@+0x64
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kGrowthDescriptionVtable = 0x140163610;

static void GrowthDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kGrowthDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)  (s + 0x20) = -1.0f;
    *(int32_t*)(s + 0x24) = 0;
    *(float*)  (s + 0x28) = 1.0f;
    *(uint8_t*)(s + 0x2c) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    *(uint8_t*)(s + 0x38) = 0;
    *(float*)  (s + 0x3c) = 1.0f;
    *(float*)  (s + 0x40) = 1.0f;
    *(uint8_t*)(s + 0x44) = 0;
    *(uint64_t*)(s + 0x48) = 0;
    *(uint64_t*)(s + 0x50) = 0;
    *(int32_t*)(s + 0x58) = 3;
    *(int32_t*)(s + 0x5c) = 3;
    *(int32_t*)(s + 0x60) = 0;
    *(uint8_t*)(s + 0x64) = 0;
}

void GrowthDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x24), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x34), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x38), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x3c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x40), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x44), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x4c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x50), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x48), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x54), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x58), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x5c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x60), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x64), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* GrowthDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kGrowthDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t GrowthDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("GrowthDescription");
    return cached;
}
static int   GrowthDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   GrowthDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* GrowthDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* GrowthDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* GrowthDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "GrowthComponent"); }

static void* g_GrowthDescription_Vtable[11] = {
    reinterpret_cast<void*>(&GrowthDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&GrowthDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&GrowthDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&GrowthDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&GrowthDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&GrowthDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&GrowthDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&GrowthDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&GrowthDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&GrowthDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&GrowthDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterGrowthDescriptionVtable() {
    RegisterExeAddr(kGrowthDescriptionVtable, reinterpret_cast<uintptr_t>(g_GrowthDescription_Vtable));
}
extern "C" void* GrowthDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    if (!p) return nullptr;
    GrowthDescription_Init(p);
    return p;
}
static void* GrowthDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    GrowthDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(GrowthDescription, "Description",
              &GrowthDescription_Factory, &GrowthDescription_CreateInPlace);
