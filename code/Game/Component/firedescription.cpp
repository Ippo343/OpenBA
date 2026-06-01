// code/Game/Component/firedescription.cpp
// FireDescription (entity description block) — fire disaster/spread parameters.
// Extends Object::External (0x20 base). Vtable 0x140159400. Parent "Description".
// Size 0x50. GetVersion=4.
//
// Factory 0x14014ac00-area (MemAlloc 0x50 + ctor sub_9d60). Ctor defaults:
//   float @ +0x20 = 1.0,  (+0x24 = 0)
//   int   @ +0x28 = 1,    (+0x2c = 0)
//   float @ +0x30 = 0.25, float @ +0x34 = 0.5
//   int   @ +0x38 = 8,    int   @ +0x3c = 100,  int @ +0x40 = 50 (0x32)
//   float @ +0x44 = 1.0,  float @ +0x48 = 1.0
// Serialize (vtable +0x28, 0x140049c00), stream order (note non-monotone):
//   float @ +0x20,  float @ +0x24
//   int   @ +0x28,  int   @ +0x2c
//   float @ +0x30,  float @ +0x34
//   int   @ +0x3c
//   float @ +0x44,  float @ +0x48
//   int   @ +0x38,  int   @ +0x40
// GetVersion (vtable +0x18, 0x140049d10): mov eax,4; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kFireDescriptionVtable = 0x140159400;

static void FireDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kFireDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)  (s + 0x20) = 1.0f;
    *(float*)  (s + 0x24) = 0.0f;
    *(int32_t*)(s + 0x28) = 1;
    *(int32_t*)(s + 0x2c) = 0;
    *(float*)  (s + 0x30) = 0.25f;
    *(float*)  (s + 0x34) = 0.5f;
    *(int32_t*)(s + 0x38) = 8;
    *(int32_t*)(s + 0x3c) = 100;
    *(int32_t*)(s + 0x40) = 50;
    *(float*)  (s + 0x44) = 1.0f;
    *(float*)  (s + 0x48) = 1.0f;
    *(int32_t*)(s + 0x4c) = 0;
}

void FireDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x20), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x24), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x28, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x2c, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x30), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x34), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x3c, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x44), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x48), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x38, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* FireDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kFireDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FireDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FireDescription");
    return cached;
}
static int   FireDescription_Stub_Return4(void* /*self*/) { return 4; } // GetVersion=4
static int   FireDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* FireDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* FireDescription_CreateInPlace(void* memory);

static void* g_FireDescription_Vtable[11] = {
    reinterpret_cast<void*>(&FireDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&FireDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&FireDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&FireDescription_Stub_Return4),     // +0x18 GetVersion=4
    reinterpret_cast<void*>(&FireDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&FireDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&FireDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&FireDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&FireDescription_Stub_ReturnNull),  // +0x40
    reinterpret_cast<void*>(&FireDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&FireDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterFireDescriptionVtable() {
    RegisterExeAddr(kFireDescriptionVtable, reinterpret_cast<uintptr_t>(g_FireDescription_Vtable));
}
extern "C" void* FireDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 8);
    if (!p) return nullptr;
    FireDescription_Init(p);
    return p;
}
static void* FireDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    FireDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(FireDescription, "Description",
              &FireDescription_Factory, &FireDescription_CreateInPlace);
#endif
