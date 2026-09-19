// code/Game/Component/livestockdescription.cpp
// LivestockDescription (entity description block) — livestock parameters
// (byproduct / createMonths).
// Extends Object::External (0x20 base). Vtable 0x140163a30. Parent "Description".
// Size 0x30. GetVersion=1.
//
// Factory 0x14014f310-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   (8)   @ +0x20 = 0    (ExternalPtr byproduct, null)
//   enum  @ +0x28 = 5
//   float @ +0x2c = 1.0f (0x3f800000)
// Serialize (vtable +0x28, 0x140123f60), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   float @ +0x2c
//   enum  @ +0x28        (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kLivestockDescriptionVtable = 0x140163a30;

static void LivestockDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kLivestockDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr (byproduct, null)
    *(int32_t*) (s + 0x28) = 5;   // enum
    *(float*)   (s + 0x2c) = 1.0f;
}

void LivestockDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalPtr_BuildSerialize) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x2c), nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(serializer)->GetVersion() > 0)   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x28), nullptr);   // B.3.4 bridge (enum +0x28, version-gated)
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* LivestockDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kLivestockDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t LivestockDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("LivestockDescription");
    return cached;
}
static int   LivestockDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   LivestockDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* LivestockDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* LivestockDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* LivestockDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "LivestockComponent"); }

static void* g_LivestockDescription_Vtable[11] = {
    reinterpret_cast<void*>(&LivestockDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&LivestockDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&LivestockDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&LivestockDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&LivestockDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&LivestockDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&LivestockDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&LivestockDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&LivestockDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&LivestockDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&LivestockDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterLivestockDescriptionVtable() {
    RegisterExeAddr(kLivestockDescriptionVtable, reinterpret_cast<uintptr_t>(g_LivestockDescription_Vtable));
}
extern "C" void* LivestockDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    LivestockDescription_Init(p);
    return p;
}
static void* LivestockDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    LivestockDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(LivestockDescription, "Description",
              &LivestockDescription_Factory, &LivestockDescription_CreateInPlace);
