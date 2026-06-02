// code/Game/Component/orcharddescription.cpp
// OrchardDescription (entity description block) — orchard farming parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a210. Parent "Description".
// Size 0x30. GetVersion=3.
//
// Factory 0x14014b440-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   int  @ +0x20 = 5, int @ +0x24 = 2, int @ +0x28 = 2 (8-byte store -> +0x2c=0)
// Serialize (vtable +0x28, 0x140052300), stream order:
//   int   @ +0x24
//   int   @ +0x28
//   float @ +0x2c
//   enum  @ +0x20   (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kOrchardDescriptionVtable = 0x14015a210;

static void OrchardDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kOrchardDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 5;
    *(int32_t*)(s + 0x24) = 2;
    *(int32_t*)(s + 0x28) = 2;
    *(int32_t*)(s + 0x2c) = 0;
}

void OrchardDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 1);
    g_fn_Serialize_Int(serializer, s + 0x24, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x28, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x2c), nullptr);
    SerializerEncodingVCall(serializer, 0x98, s + 0x20);   // enum +0x20
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* OrchardDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kOrchardDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t OrchardDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("OrchardDescription");
    return cached;
}
static int   OrchardDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   OrchardDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* OrchardDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* OrchardDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* OrchardDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "OrchardComponent"); }

static void* g_OrchardDescription_Vtable[11] = {
    reinterpret_cast<void*>(&OrchardDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&OrchardDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&OrchardDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&OrchardDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&OrchardDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&OrchardDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&OrchardDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&OrchardDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&OrchardDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&OrchardDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&OrchardDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterOrchardDescriptionVtable() {
    RegisterExeAddr(kOrchardDescriptionVtable, reinterpret_cast<uintptr_t>(g_OrchardDescription_Vtable));
}
extern "C" void* OrchardDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    OrchardDescription_Init(p);
    return p;
}
static void* OrchardDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    OrchardDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(OrchardDescription, "Description",
              &OrchardDescription_Factory, &OrchardDescription_CreateInPlace);
#endif
