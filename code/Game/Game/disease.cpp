// code/Game/Game/disease.cpp
// Disease (Object::External, 0x48 bytes) — disease resource definition.
//
// Loaded during the in-game world build (worldgen / climate setup) as an
// External resource referenced by an ExternalList. In standalone it was a
// NullFactory stub → factory returned NULL → Runtime's External::Load called
// IsDerivedFrom on the NULL result → AV @Runtime+0xC039A (the "BuildTerrain
// hang" was actually this crash, masked by the shutdown-deadlock in
// LdrShutdownProcess). This file provides the real factory + faithful Serialize.
//
// Decompiled READ-ONLY from Application-x64.exe:
//   RegisterType  0x14014cb00 (mislabeled ClimateGenerator_StaticInit):
//                 BaseTypeInfo("Disease","Object::External", abstract=NULL,
//                              factory=0x14008fd0, vslot=ClimateGenerator_vslot18)
//   Factory       0x14008fd0: MemAlloc(0x48,8); External::External; vtable 0x14015e190;
//                 +0x20=0 +0x28=0 +0x30=1.0f +0x34=0 +0x38=1.0f +0x3c=10.0f +0x40=0
//   Vtable        0x14015e190: +0x00 Dtor=0x140018f60, +0x10 GetTypeId,
//                 +0x18 GetVersion=0x140093390 (mov eax,2), +0x28 Serialize=0x1400932d0
//   Serialize     0x1400932d0 stream order:
//                 +0x20 ExternalPtr<StringTable> (FUN_14000bf40),
//                 +0x28 System::String,
//                 +0x30/+0x34/+0x38/+0x3c/+0x40 floats
//   Dtor          0x140018f60: String::~String(+0x28); External::~External; cond free.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kDiseaseVtable = 0x14015e190;

// ============================================================
// Local serializer function pointers (resolved lazily; standalone fallbacks)
// ============================================================
static void  (*s_fn_Serializer_Ctor)(void* self, void* stream, int version) = nullptr;
static void  (*s_fn_Serializer_Dtor)(void* self) = nullptr;
static void  (*s_fn_Serialize_Float)(void* ser, void* field, const char* name) = nullptr;
static void  (*s_fn_String_Serialize)(void* str, void* serializer) = nullptr;
static void  (*s_fn_String_Dtor)(void* str) = nullptr;
static void  (*s_fn_ExternalPtr_Serialize2)(void* field, void* serializer) = nullptr;
static bool  s_funcsInited = false;

static void Disease_InitFuncs() {
    if (s_funcsInited) return;
    s_funcsInited = true;

    uintptr_t iat;
    // Serializer ctor/dtor + float serialize via the exe IAT dispatch slots
    iat = ResolveAddr(Module::Exe, 0x140152ad8);
    if (iat) s_fn_Serializer_Ctor = *reinterpret_cast<void(**)(void*, void*, int)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152ad0);
    if (iat) s_fn_Serializer_Dtor = *reinterpret_cast<void(**)(void*)>(iat);
    iat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);

    s_fn_String_Serialize = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)g_fn_String_Serialize);
    s_fn_String_Dtor      = reinterpret_cast<void(*)(void*)>((uintptr_t)fn_String_Dtor);
    s_fn_ExternalPtr_Serialize2 = reinterpret_cast<void(*)(void*, void*)>(
        ResolveAddr(Module::Exe, 0x14000bf40));

    // Standalone fallbacks: the exe IAT slots resolve to NULL when the original
    // exe isn't mapped. Fall back to the Runtime-resolved engine_funcs globals so
    // the SAME Serialize works in both modes.
    if (!s_fn_Serializer_Ctor) s_fn_Serializer_Ctor = reinterpret_cast<void(*)(void*, void*, int)>((uintptr_t)g_fn_Serializer_Ctor);
    if (!s_fn_Serializer_Dtor) s_fn_Serializer_Dtor = reinterpret_cast<void(*)(void*)>((uintptr_t)g_fn_Serializer_Dtor);
    if (!s_fn_Serialize_Float) s_fn_Serialize_Float = reinterpret_cast<void(*)(void*, void*, const char*)>((uintptr_t)fn_Serializer_Serialize_Float);
    if (!s_fn_ExternalPtr_Serialize2) s_fn_ExternalPtr_Serialize2 = reinterpret_cast<void(*)(void*, void*)>((uintptr_t)fn_ExternalPtr_BuildSerialize);
}

// ============================================================
// Disease::Init — mirrors factory 0x14008fd0
// ============================================================
static void Disease_Init(void* self) {
    if (fn_External_External) fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kDiseaseVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int64_t*)(s + 0x20) = 0;          // ExternalPtr<StringTable> (weak)
    *(int64_t*)(s + 0x28) = 0;          // System::String (empty COW)
    *(float*)(s + 0x30) = 1.0f;
    *(float*)(s + 0x34) = 0.0f;
    *(float*)(s + 0x38) = 1.0f;
    *(float*)(s + 0x3c) = 10.0f;
    *(float*)(s + 0x40) = 0.0f;
}

// ============================================================
// Disease::Serialize (vtable +0x28, exe 0x1400932d0)
// ============================================================
void Disease_Serialize(void* self, void* stream) {
    Disease_InitFuncs();
    if (!s_fn_Serializer_Ctor || !s_fn_Serializer_Dtor) return;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);

    uint8_t serializer[24];
    s_fn_Serializer_Ctor(serializer, stream, 0);
    if (s_fn_ExternalPtr_Serialize2) s_fn_ExternalPtr_Serialize2((void*)(s + 0x20), serializer);
    if (s_fn_String_Serialize)       s_fn_String_Serialize((void*)(s + 0x28), serializer);
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, (float*)(s + 0x30), nullptr);
        s_fn_Serialize_Float(serializer, (float*)(s + 0x34), nullptr);
        s_fn_Serialize_Float(serializer, (float*)(s + 0x38), nullptr);
        s_fn_Serialize_Float(serializer, (float*)(s + 0x3c), nullptr);
        s_fn_Serialize_Float(serializer, (float*)(s + 0x40), nullptr);
    }
    s_fn_Serializer_Dtor(serializer);
}

// ============================================================
// Disease::Dtor (vtable +0x00, exe 0x140018f60)
// ============================================================
void* Disease_Dtor(void* self, uint32_t flags) {
    Disease_InitFuncs();
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (s_fn_String_Dtor) s_fn_String_Dtor((void*)(s + 0x28));
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t Disease_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("Disease");
    return cached;
}
static int   Disease_Return2(void* /*self*/) { return 2; }   // GetVersion = 2
static int   Disease_Return0(void* /*self*/) { return 0; }
static void* Disease_ReturnNull(void*, ...)  { return nullptr; }
static void* Disease_CreateInPlace(void* mem);

static void* g_Disease_Vtable[9] = {
    reinterpret_cast<void*>(&Disease_Dtor),         // +0x00 Dtor
    reinterpret_cast<void*>(&Disease_ReturnNull),   // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&Disease_GetTypeId),    // +0x10 GetTypeId (used by IsDerivedFrom)
    reinterpret_cast<void*>(&Disease_Return2),      // +0x18 GetVersion=2
    reinterpret_cast<void*>(&Disease_Return0),      // +0x20 Compile thunk
    reinterpret_cast<void*>(&Disease_Serialize),    // +0x28 Serialize
    reinterpret_cast<void*>(&Disease_Return0),      // +0x30
    reinterpret_cast<void*>(&Disease_Return0),      // +0x38
    reinterpret_cast<void*>(&Disease_CreateInPlace),// +0x40
};

extern "C" void RegisterDiseaseVtable() {
    RegisterExeAddr(kDiseaseVtable, reinterpret_cast<uintptr_t>(g_Disease_Vtable));
}

extern "C" void* Disease_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    Disease_Init(p);
    return p;
}
static void* Disease_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    Disease_Init(mem);
    return mem;
}

REGISTER_TYPE(Disease, "Object::External", &Disease_Factory, &Disease_CreateInPlace);
#endif
