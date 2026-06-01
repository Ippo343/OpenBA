// code/Game/Component/valuedescription.cpp
// ValueDescription (entity description block) — resource value/trade parameters.
// Extends Object::External (0x20 base). Vtable 0x14015add0. Parent "Description".
// Size 0x50. GetVersion=3 (slot +0x18 @ 0x14005ce10: mov eax,3; ret).
//
// Ctor defaults (ValueDescription_Factory @ 0x14000faa0):
//   ExternalPtr @ +0x20 = 0   (sprite/icon ref)
//   String      @ +0x28 = 0   (name)
//   ExternalPtr @ +0x30 = 0
//   String      @ +0x38 = 0
//   int         @ +0x40 = 0
//   int         @ +0x44 = 100 (baseValue)
//   int         @ +0x48 = 0   (weight) [factory writes 8 bytes @+0x44 = 100]
// Serialize (vtable +0x28, 0x14005cd60), stream order:
//   ExternalPtr @ +0x20  (FUN_14000bd30 -> fn_ExternalPtr_BuildSerialize)
//   String      @ +0x28
//   ExternalPtr @ +0x30  (FUN_14000bf40 -> fn_ExternalPtr_BuildSerialize)
//   String      @ +0x38
//   int         @ +0x40
//   int         @ +0x44
//   int         @ +0x48
// ToolKit fields: sprite / name / baseValue / weight.
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kValueDescriptionVtable = 0x14015add0;

static void ValueDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kValueDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;   // String
    *(uint64_t*)(s + 0x30) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x38) = 0;   // String
    *(int32_t*) (s + 0x40) = 0;
    *(int32_t*) (s + 0x44) = 100; // baseValue
    *(int32_t*) (s + 0x48) = 0;   // weight
}

void ValueDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_String_Serialize || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    g_fn_String_Serialize(s + 0x28, serializer);           // String      +0x28
    fn_ExternalPtr_BuildSerialize(s + 0x30, serializer);   // ExternalPtr +0x30
    g_fn_String_Serialize(s + 0x38, serializer);           // String      +0x38
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x44, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x48, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* ValueDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kValueDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_String_Dtor) { fn_String_Dtor(s + 0x28); fn_String_Dtor(s + 0x38); }
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ValueDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ValueDescription");
    return cached;
}
static int   ValueDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   ValueDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* ValueDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* ValueDescription_CreateInPlace(void* memory);

static void* g_ValueDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ValueDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&ValueDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&ValueDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&ValueDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&ValueDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&ValueDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&ValueDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&ValueDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&ValueDescription_Stub_ReturnNull), // +0x40
    reinterpret_cast<void*>(&ValueDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&ValueDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterValueDescriptionVtable() {
    RegisterExeAddr(kValueDescriptionVtable, reinterpret_cast<uintptr_t>(g_ValueDescription_Vtable));
}
extern "C" void* ValueDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x50, 8);
    if (!p) return nullptr;
    ValueDescription_Init(p);
    return p;
}
static void* ValueDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ValueDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(ValueDescription, "Description",
              &ValueDescription_Factory, &ValueDescription_CreateInPlace);
#endif
