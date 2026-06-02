// code/Game/Component/diseasedescription.cpp
// DiseaseDescription (entity description block) — disease outbreak parameters.
// Extends Object::External (0x20 base). Vtable 0x140159310. Parent "Description".
// Size 0x48. GetVersion=1.
//
// Factory 0x14014ab80-area (MemAlloc 0x48 + base init + vtable; no explicit field
// inits — fields left as fresh-alloc zero).
// Serialize (vtable +0x28, 0x140048e40), stream order:
//   int   @ +0x20,  int   @ +0x24
//   float @ +0x28,  float @ +0x2c,  float @ +0x30,  float @ +0x34
//   float @ +0x38,  float @ +0x3c
//   int   @ +0x40
// GetVersion (vtable +0x18, 0x140048f20): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kDiseaseDescriptionVtable = 0x140159310;

static void DiseaseDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kDiseaseDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    // fresh-alloc zero for +0x20..+0x47 (exe ctor sets no explicit defaults)
    for (int o = 0x20; o < 0x48; o += 4) *(int32_t*)(s + o) = 0;
}

void DiseaseDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Int(serializer, s + 0x20, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x24, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x28), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x2c), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x30), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x34), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x38), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x3c), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x40, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* DiseaseDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kDiseaseDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t DiseaseDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("DiseaseDescription");
    return cached;
}
static int   DiseaseDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion=1
static int   DiseaseDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* DiseaseDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* DiseaseDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* DiseaseDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "DiseaseComponent"); }

static void* g_DiseaseDescription_Vtable[11] = {
    reinterpret_cast<void*>(&DiseaseDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&DiseaseDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&DiseaseDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&DiseaseDescription_Stub_Return1),     // +0x18 GetVersion=1
    reinterpret_cast<void*>(&DiseaseDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&DiseaseDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&DiseaseDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&DiseaseDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&DiseaseDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&DiseaseDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&DiseaseDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterDiseaseDescriptionVtable() {
    RegisterExeAddr(kDiseaseDescriptionVtable, reinterpret_cast<uintptr_t>(g_DiseaseDescription_Vtable));
}
extern "C" void* DiseaseDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    DiseaseDescription_Init(p);
    return p;
}
static void* DiseaseDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    DiseaseDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(DiseaseDescription, "Description",
              &DiseaseDescription_Factory, &DiseaseDescription_CreateInPlace);
#endif
