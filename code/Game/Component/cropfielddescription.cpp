// code/Game/Component/cropfielddescription.cpp
// CropFieldDescription (entity description block) — crop-field farming parameters.
// Extends Object::External (0x20 base). Vtable 0x140162f50. Parent "Description".
// Size 0x28. GetVersion=3.
//
// Factory 0x14014ed10-area (MemAlloc 0x28 + vtable + defaults). Defaults:
//   enum @ +0x20 = 5   (8-byte store -> +0x20=5, +0x24=0)
// Serialize (vtable +0x28, 0x14011b270), stream order:
//   float @ +0x24
//   enum  @ +0x20   (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kCropFieldDescriptionVtable = 0x140162f50;

static void CropFieldDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kCropFieldDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 5;   // enum (farming type)
    *(int32_t*)(s + 0x24) = 0;
}

void CropFieldDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 1);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x24), nullptr);
    SerializerEncodingVCall(serializer, 0x98, s + 0x20);   // enum +0x20
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* CropFieldDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCropFieldDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CropFieldDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CropFieldDescription");
    return cached;
}
static int   CropFieldDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   CropFieldDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* CropFieldDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* CropFieldDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* CropFieldDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "CropFieldComponent"); }

static void* g_CropFieldDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CropFieldDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&CropFieldDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&CropFieldDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&CropFieldDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&CropFieldDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&CropFieldDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&CropFieldDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&CropFieldDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&CropFieldDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&CropFieldDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CropFieldDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterCropFieldDescriptionVtable() {
    RegisterExeAddr(kCropFieldDescriptionVtable, reinterpret_cast<uintptr_t>(g_CropFieldDescription_Vtable));
}
extern "C" void* CropFieldDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    CropFieldDescription_Init(p);
    return p;
}
static void* CropFieldDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CropFieldDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(CropFieldDescription, "Description",
              &CropFieldDescription_Factory, &CropFieldDescription_CreateInPlace);
#endif
