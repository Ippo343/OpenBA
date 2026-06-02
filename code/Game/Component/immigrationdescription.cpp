// code/Game/Component/immigrationdescription.cpp
// ImmigrationDescription (entity description block) — immigration/arrival parameters.
// Extends Object::External (0x20 base). Vtable 0x140159bc0. Parent "Description".
// Size 0x68. GetVersion=2.
//
// Factory 0x14014b000-area (MemAlloc 0x68 + ctor sub_e9a0). Ctor defaults:
//   ExternalPtr @ +0x20 = 0
//   int         @ +0x28 = 0x50 (80)
//   External*-array @ +0x30 = 0 (count @ +0x38, cap @ +0x3c)
//   float @ +0x40 = 0.05  (0x3d4ccccd),  float @ +0x44 = 0.15 (0x3e19999a)
//   float @ +0x48 = 24.0  (0x41c00000),  float @ +0x4c = 48.0 (0x42400000)
//   int   @ +0x50 = 1,  int @ +0x54 = 3,  int @ +0x58 = 7,  int @ +0x5c = 10
//   float @ +0x60 = 1.0,  int @ +0x64 = 6
// Serialize (vtable +0x28, 0x14004e7f0), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   int         @ +0x28
//   int count   @ +0x38 -> resize +0x30 array -> per-element External FindOrLoad
//                          (FUN_1400049c0 == Desc_LoadExternalArray)
//   float @ +0x40,  float @ +0x44,  float @ +0x48,  float @ +0x4c
//   int   @ +0x50,  int   @ +0x54
//   float @ +0x60
//   int   @ +0x64
// GetVersion (vtable +0x18, 0x14004e930): mov eax,2; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);

static const uintptr_t kImmigrationDescriptionVtable = 0x140159bc0;

static inline int ID_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void ImmigrationDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kImmigrationDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;          // ExternalPtr
    *(int32_t*) (s + 0x28) = 0x50;       // 80
    *(uint64_t*)(s + 0x30) = 0;          // External*-array ptr
    *(uint64_t*)(s + 0x38) = 0;          // count + cap
    *(float*)   (s + 0x40) = 0.05f;
    *(float*)   (s + 0x44) = 0.15f;
    *(float*)   (s + 0x48) = 24.0f;
    *(float*)   (s + 0x4c) = 48.0f;
    *(int32_t*) (s + 0x50) = 1;
    *(int32_t*) (s + 0x54) = 3;
    *(int32_t*) (s + 0x58) = 7;
    *(int32_t*) (s + 0x5c) = 10;
    *(float*)   (s + 0x60) = 1.0f;
    *(int32_t*) (s + 0x64) = 6;
}

void ImmigrationDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);     // ExternalPtr +0x20
    g_fn_Serialize_Int(serializer, s + 0x28, nullptr);       // int         +0x28

    // External*-array @ +0x30 (count @ +0x38, cap @ +0x3c, stride 8)
    g_fn_Serialize_Int(serializer, s + 0x38, nullptr);
    int n = *(int32_t*)(s + 0x38);
    if (ID_SerStreamErr(serializer) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        *(void**)(s + 0x30) = buf;
        *(int32_t*)(s + 0x3c) = n;
        if (buf) for (int i = 0; i < n; ++i) ((void**)buf)[i] = nullptr;
    }
    Desc_LoadExternalArray(serializer, n, *(void**)(s + 0x30));

    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x40), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x44), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x48), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x4c), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x50, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x54, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x60), nullptr);
    g_fn_Serialize_Int(serializer, s + 0x64, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* ImmigrationDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kImmigrationDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    void* arr = *(void**)(s + 0x30);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + 0x30) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ImmigrationDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ImmigrationDescription");
    return cached;
}
static int   ImmigrationDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion=2
static int   ImmigrationDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* ImmigrationDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* ImmigrationDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* ImmigrationDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "ImmigrationComponent"); }

static void* g_ImmigrationDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ImmigrationDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&ImmigrationDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_Return2),     // +0x18 GetVersion=2
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&ImmigrationDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&ImmigrationDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&ImmigrationDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&ImmigrationDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterImmigrationDescriptionVtable() {
    RegisterExeAddr(kImmigrationDescriptionVtable, reinterpret_cast<uintptr_t>(g_ImmigrationDescription_Vtable));
}
extern "C" void* ImmigrationDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    if (!p) return nullptr;
    ImmigrationDescription_Init(p);
    return p;
}
static void* ImmigrationDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ImmigrationDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(ImmigrationDescription, "Description",
              &ImmigrationDescription_Factory, &ImmigrationDescription_CreateInPlace);
#endif
