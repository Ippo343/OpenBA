// code/Game/Component/populationdescription.cpp
// PopulationDescription (entity description block) — wild-population spawner
// parameters (e.g. deer herds). Extends Object::External. Vtable 0x14015a3e0.
// Parent "Description". Size 0x58. GetVersion=3.
//
// Ctor defaults (PopulationDescription_sub_4940):
//   String @ +0x20 = 0 (empty)
//   int    @ +0x28 = 8,   int @ +0x2c = 0x20
//   enum   @ +0x30 = 0xe1f, enum @ +0x34 = 0
//   bool   @ +0x38 = 1
//   int    @ +0x3c = 4,   int @ +0x40 = 1,   int @ +0x44 = 1
//   bool   @ +0x48 = 0
//   ExternalPtr @ +0x50 = 0
// Serialize (vtable +0x28, 0x1400547e0), stream order:
//   int@0x28, int@0x2c, enum@0x30 (vtbl+0x98), enum@0x34 (vtbl+0x98),
//   bool@0x38, int@0x3c, int@0x40, int@0x44, bool@0x48,
//   ExternalPtr@0x50 (FUN_140002340), String@0x20.
// Decompiled READ-ONLY from Application-x64.exe.
//
// Part of the PopulationDeer vertical slice (Foundation C keystone).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kPopulationDescriptionVtable = 0x14015a3e0;

static void PopulationDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kPopulationDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;        // String (empty)
    *(int32_t*) (s + 0x28) = 8;
    *(int32_t*) (s + 0x2c) = 0x20;
    *(int32_t*) (s + 0x30) = 0xe1f;    // enum
    *(int32_t*) (s + 0x34) = 0;        // enum
    *(uint8_t*) (s + 0x38) = 1;
    *(int32_t*) (s + 0x3c) = 4;
    *(int32_t*) (s + 0x40) = 1;
    *(int32_t*) (s + 0x44) = 1;
    *(uint8_t*) (s + 0x48) = 0;
    *(uint64_t*)(s + 0x50) = 0;        // ExternalPtr (null)
}

void PopulationDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_Serialize_Bool || !g_fn_String_Serialize || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Int(serializer, s + 0x28, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x2c, nullptr);
    SerializerEncodingVCall(serializer, 0x98, s + 0x30);     // enum +0x30
    SerializerEncodingVCall(serializer, 0x98, s + 0x34);     // enum +0x34
    g_fn_Serialize_Bool(serializer, s + 0x38, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x3c, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x44, nullptr);
    g_fn_Serialize_Bool(serializer, s + 0x48, nullptr);
    fn_ExternalPtr_BuildSerialize(s + 0x50, serializer);     // ExternalPtr +0x50
    g_fn_String_Serialize(s + 0x20, serializer);             // String +0x20
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* PopulationDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kPopulationDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_String_Dtor) fn_String_Dtor(s + 0x20);
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PopulationDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PopulationDescription");
    return cached;
}
static int   PopulationDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   PopulationDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* PopulationDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* PopulationDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* PopulationDescription_GetComponentTypeId(void*, void* out) {
    return ::Object::WriteComponentTypeId(out, "PopulationComponent");
}

static void* g_PopulationDescription_Vtable[11] = {
    reinterpret_cast<void*>(&PopulationDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&PopulationDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&PopulationDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&PopulationDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&PopulationDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&PopulationDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&PopulationDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&PopulationDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&PopulationDescription_GetComponentTypeId), // +0x40 GetComponentTypeId
    reinterpret_cast<void*>(&PopulationDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&PopulationDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterPopulationDescriptionVtable() {
    RegisterExeAddr(kPopulationDescriptionVtable, reinterpret_cast<uintptr_t>(g_PopulationDescription_Vtable));
}
extern "C" void* PopulationDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    if (!p) return nullptr;
    PopulationDescription_Init(p);
    return p;
}
static void* PopulationDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    PopulationDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(PopulationDescription, "Description",
              &PopulationDescription_Factory, &PopulationDescription_CreateInPlace);
#endif
