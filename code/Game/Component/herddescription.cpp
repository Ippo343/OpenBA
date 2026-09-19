// code/Game/Component/herddescription.cpp
// HerdDescription (entity description block) — wild-animal herd parameters.
// Extends Object::External (0x20 base). Vtable 0x140163700. Parent "Description".
// Size 0x40. GetVersion=2.
//
// Factory 0x14014f190-area (MemAlloc 0x40 + vtable + zero). Serialize (vtable
// +0x28, 0x1401221e0), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   int   @ +0x28,  int   @ +0x2c
//   float @ +0x30,  float @ +0x34
//   int   @ +0x38,  float @ +0x3c
// Decompiled READ-ONLY from Application-x64.exe.
//
// Part of the PopulationDeer vertical slice (Foundation C keystone).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kHerdDescriptionVtable = 0x140163700;

static void HerdDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kHerdDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr (null)
    *(int32_t*) (s + 0x28) = 0;
    *(int32_t*) (s + 0x2c) = 0;
    *(float*)   (s + 0x30) = 0.0f;
    *(float*)   (s + 0x34) = 0.0f;
    *(int32_t*) (s + 0x38) = 0;
    *(float*)   (s + 0x3c) = 0.0f;
}

void HerdDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalPtr_BuildSerialize) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);     // ExternalPtr +0x20
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x30), nullptr); // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x34), nullptr); // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x38), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x3c), nullptr); // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* HerdDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kHerdDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HerdDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HerdDescription");
    return cached;
}
static int   HerdDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   HerdDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* HerdDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* HerdDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* HerdDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "HerdComponent"); }

static void* g_HerdDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HerdDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&HerdDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&HerdDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&HerdDescription_Stub_Return2),     // +0x18 GetVersion=2
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&HerdDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&HerdDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&HerdDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&HerdDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterHerdDescriptionVtable() {
    RegisterExeAddr(kHerdDescriptionVtable, reinterpret_cast<uintptr_t>(g_HerdDescription_Vtable));
}
extern "C" void* HerdDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    HerdDescription_Init(p);
    return p;
}
static void* HerdDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    HerdDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(HerdDescription, "Description",
              &HerdDescription_Factory, &HerdDescription_CreateInPlace);
