// code/Game/Component/fruitdescription.cpp
// FruitDescription (entity description block) — fruit/produce parameters.
// Extends Object::External (0x20 base). Vtable 0x1401594d0. Parent "Description".
// Size 0x38. GetVersion=1.
//
// Factory 0x14014ac80-area (MemAlloc 0x38 + vtable + defaults). Defaults:
//   float @ +0x20 = 1.0f  (0x3f800000, 8-byte store -> +0x24=0)
//   int   @ +0x28 = 0
//   (8)   @ +0x30 = 0   (ExternalPtr, null)
// Serialize (vtable +0x28, 0x14004a1e0), stream order:
//   float @ +0x20
//   float @ +0x24
//   float @ +0x28
//   ExternalPtr @ +0x30  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kFruitDescriptionVtable = 0x1401594d0;

static void FruitDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kFruitDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)  (s + 0x20) = 1.0f;
    *(int32_t*)(s + 0x24) = 0;
    *(int32_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;   // ExternalPtr (null)
}

void FruitDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_Serializer_Serialize_Float || !fn_ExternalPtr_BuildSerialize) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x24), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x28), nullptr);   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x30, serializer);   // ExternalPtr +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* FruitDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kFruitDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FruitDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FruitDescription");
    return cached;
}
static int   FruitDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   FruitDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* FruitDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* FruitDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* FruitDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "FruitComponent"); }

static void* g_FruitDescription_Vtable[11] = {
    reinterpret_cast<void*>(&FruitDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&FruitDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&FruitDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&FruitDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&FruitDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&FruitDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&FruitDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&FruitDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&FruitDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&FruitDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&FruitDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterFruitDescriptionVtable() {
    RegisterExeAddr(kFruitDescriptionVtable, reinterpret_cast<uintptr_t>(g_FruitDescription_Vtable));
}
extern "C" void* FruitDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    FruitDescription_Init(p);
    return p;
}
static void* FruitDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    FruitDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(FruitDescription, "Description",
              &FruitDescription_Factory, &FruitDescription_CreateInPlace);
