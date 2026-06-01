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
#include <cstdint>

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
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);     // ExternalPtr +0x20
    g_fn_Serialize_Int(serializer, s + 0x28, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x2c, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x30), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x34), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x38, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x3c), nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* HerdDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kHerdDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
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

static void* g_HerdDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HerdDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&HerdDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&HerdDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&HerdDescription_Stub_Return2),     // +0x18 GetVersion=2
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&HerdDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&HerdDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&HerdDescription_Stub_ReturnNull),  // +0x40
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
#endif
