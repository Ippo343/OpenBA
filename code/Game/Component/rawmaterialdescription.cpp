// code/Game/Component/rawmaterialdescription.cpp
// RawMaterialDescription (entity description block) — resource/material type.
// Extends Object::External (0x20 base). Vtable 0x1401641f0. Parent "Description".
// Size 0x60. GetVersion=8 (slot +0x18 @ 0x140129630: mov eax,8; ret).
//
// Ctor defaults (RawMaterialDescription_CopyCtor):
//   ExternalPtr @ +0x20 = 0, ExternalPtr @ +0x28 = 0   (sprite / icon refs)
//   String      @ +0x30 = 0, String @ +0x38 = 0        (name / plural)
//   int         @ +0x40 = 1, +0x44 = 1, +0x48 = 1       (flags / stack / etc.)
//   float       @ +0x50 = 0.33 (0x3ea8f5c3)
//   byte/bool   @ +0x58 = 0
//   int/enum    @ +0x5c = 0
// Serialize (vtable +0x28, 0x140129520), stream order:
//   ExternalPtr @ +0x20  (FUN_14000bd30 -> fn_ExternalPtr_BuildSerialize)
//   ExternalPtr @ +0x28  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   String      @ +0x30
//   String      @ +0x38
//   int @ +0x40, int @ +0x44, int @ +0x48, int @ +0x4c
//   enum @ +0x5c (encoding vcall +0x98)
//   float @ +0x50
//   int @ +0x54
//   bool @ +0x58
// ToolKit fields: RawMaterialFlags / stack / value / weight.
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kRawMaterialDescriptionVtable = 0x1401641f0;

static void RawMaterialDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kRawMaterialDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;           // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;           // ExternalPtr
    *(uint64_t*)(s + 0x30) = 0;           // String
    *(uint64_t*)(s + 0x38) = 0;           // String
    *(int32_t*) (s + 0x40) = 1;
    *(int32_t*) (s + 0x44) = 1;
    *(int32_t*) (s + 0x48) = 1;
    *(int32_t*) (s + 0x4c) = 0;
    *(uint32_t*)(s + 0x50) = 0x3ea8f5c3u; // float ~0.33
    *(int32_t*) (s + 0x54) = 0;
    *(uint8_t*) (s + 0x58) = 0;
    *(int32_t*) (s + 0x5c) = 0;
}

void RawMaterialDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_Serialize_Bool || !g_fn_String_Serialize || !fn_Serializer_Serialize_Float ||
        !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    fn_ExternalPtr_BuildSerialize(s + 0x28, serializer);   // ExternalPtr +0x28
    g_fn_String_Serialize(s + 0x30, serializer);           // String      +0x30
    g_fn_String_Serialize(s + 0x38, serializer);           // String      +0x38
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x44, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x48, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x4c, nullptr);
    SerializerEncodingVCall(serializer, 0x98, s + 0x5c);   // enum +0x5c
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x50), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x54, nullptr);
    g_fn_Serialize_Bool(serializer, s + 0x58, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* RawMaterialDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kRawMaterialDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_String_Dtor) { fn_String_Dtor(s + 0x30); fn_String_Dtor(s + 0x38); }
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RawMaterialDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("RawMaterialDescription");
    return cached;
}
static int   RawMaterialDescription_Stub_Return8(void* /*self*/) { return 8; } // GetVersion
static int   RawMaterialDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* RawMaterialDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* RawMaterialDescription_CreateInPlace(void* memory);

static void* g_RawMaterialDescription_Vtable[11] = {
    reinterpret_cast<void*>(&RawMaterialDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&RawMaterialDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_Return8),    // +0x18 GetVersion=8
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&RawMaterialDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_ReturnNull), // +0x40
    reinterpret_cast<void*>(&RawMaterialDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&RawMaterialDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterRawMaterialDescriptionVtable() {
    RegisterExeAddr(kRawMaterialDescriptionVtable, reinterpret_cast<uintptr_t>(g_RawMaterialDescription_Vtable));
}
extern "C" void* RawMaterialDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    if (!p) return nullptr;
    RawMaterialDescription_Init(p);
    return p;
}
static void* RawMaterialDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    RawMaterialDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(RawMaterialDescription, "Description",
              &RawMaterialDescription_Factory, &RawMaterialDescription_CreateInPlace);
#endif
