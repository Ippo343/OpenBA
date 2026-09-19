// code/Game/Component/terraformdescription.cpp
// TerraformDescription (entity description block) — terraform/landscaping parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a900. Parent "Description".
// Size 0x38. GetVersion=3.
//
// Factory 0x14014b840-area (MemAlloc 0x38). Ctor defaults: zero @ +0x20, +0x28, +0x30.
// Serialize (vtable +0x28, 0x1400576a0), stream order (interleaved ExtPtr/enum):
//   ExternalPtr @ +0x20  (FUN_14000b7b0 — single-ExternalPtr variant w/ type-check;
//                          stream-identical to fn_ExternalPtr_BuildSerialize)
//   enum        @ +0x30  (serializer encoding vtable +0x98)
//   ExternalPtr @ +0x28  (FUN_14000b7b0)
//   enum        @ +0x34  (serializer encoding vtable +0x98)
// GetVersion (vtable +0x18, 0x140057730): mov eax,3; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kTerraformDescriptionVtable = 0x14015a900;

static void TerraformDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kTerraformDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x30) = 0;   // two enums (+0x30, +0x34)
}

void TerraformDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !fn_ExternalPtr_BuildSerialize) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);     // ExternalPtr +0x20
    reinterpret_cast<IO::Serializer*>(serializer)
        ->Serialize(*reinterpret_cast<unsigned int*>(s + 0x30), nullptr);     // enum        +0x30   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x28, serializer);     // ExternalPtr +0x28
    reinterpret_cast<IO::Serializer*>(serializer)
        ->Serialize(*reinterpret_cast<unsigned int*>(s + 0x34), nullptr);     // enum        +0x34   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* TerraformDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kTerraformDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TerraformDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TerraformDescription");
    return cached;
}
static int   TerraformDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion=3
static int   TerraformDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* TerraformDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* TerraformDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* TerraformDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "TerraformComponent"); }

static void* g_TerraformDescription_Vtable[11] = {
    reinterpret_cast<void*>(&TerraformDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&TerraformDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&TerraformDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&TerraformDescription_Stub_Return3),     // +0x18 GetVersion=3
    reinterpret_cast<void*>(&TerraformDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&TerraformDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&TerraformDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&TerraformDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&TerraformDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&TerraformDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&TerraformDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterTerraformDescriptionVtable() {
    RegisterExeAddr(kTerraformDescriptionVtable, reinterpret_cast<uintptr_t>(g_TerraformDescription_Vtable));
}
extern "C" void* TerraformDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    TerraformDescription_Init(p);
    return p;
}
static void* TerraformDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    TerraformDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(TerraformDescription, "Description",
              &TerraformDescription_Factory, &TerraformDescription_CreateInPlace);
