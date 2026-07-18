// code/Game/Component/quarrydescription.cpp
// QuarryDescription (entity description block) — quarry extraction parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a510. Parent "Description".
// Size 0x30. GetVersion=4 (slot +0x18 @ 0x140055390: mov eax,4; ret).
//
// Ctor defaults (QuarryDescription_Factory):
//   ExternalPtr @ +0x20 = 0        (product material ref)
//   float       @ +0x28 = -1.5  (0xbfc00000)  (min depth)
//   float       @ +0x2c = -7.0  (0xc0e00000)  (max depth)
// Serialize (vtable +0x28, 0x140055320), stream order:
//   float       @ +0x2c
//   float       @ +0x28
//   ExternalPtr @ +0x20  (FUN_14000b7b0 -> fn_ExternalPtr_BuildSerialize)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kQuarryDescriptionVtable = 0x14015a510;

static void QuarryDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kQuarryDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;           // ExternalPtr
    *(uint32_t*)(s + 0x28) = 0xbfc00000u; // float -1.5
    *(uint32_t*)(s + 0x2c) = 0xc0e00000u; // float -7.0
}

void QuarryDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_Serializer_Serialize_Float || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x2c), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x28), nullptr);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    g_fn_Serializer_Dtor(serializer);
}

void* QuarryDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kQuarryDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t QuarryDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("QuarryDescription");
    return cached;
}
static int   QuarryDescription_Stub_Return4(void* /*self*/) { return 4; } // GetVersion
static int   QuarryDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* QuarryDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* QuarryDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* QuarryDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "QuarryComponent"); }

static void* g_QuarryDescription_Vtable[11] = {
    reinterpret_cast<void*>(&QuarryDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&QuarryDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&QuarryDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&QuarryDescription_Stub_Return4),    // +0x18 GetVersion=4
    reinterpret_cast<void*>(&QuarryDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&QuarryDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&QuarryDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&QuarryDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&QuarryDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&QuarryDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&QuarryDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterQuarryDescriptionVtable() {
    RegisterExeAddr(kQuarryDescriptionVtable, reinterpret_cast<uintptr_t>(g_QuarryDescription_Vtable));
}
extern "C" void* QuarryDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    QuarryDescription_Init(p);
    return p;
}
static void* QuarryDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    QuarryDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(QuarryDescription, "Description",
              &QuarryDescription_Factory, &QuarryDescription_CreateInPlace);
