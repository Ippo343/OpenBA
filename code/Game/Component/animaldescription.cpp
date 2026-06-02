// code/Game/Component/animaldescription.cpp
// AnimalDescription (entity description block) — animal/wildlife parameters.
// Extends Object::External. Vtable 0x140162190. Parent "Description". Size 0x58.
// GetVersion=2. (Transitively pulled by PopulationDeer via the deer entity.)
//
// Ctor defaults (FUN_1401071e0): int@0x20=5, int@0x24=4, float@0x28=1.0,
//   float@0x2c=0, float@0x30=1.0, float@0x34=0.5, float@0x38=0.5, bool@0x3c=1,
//   float@0x40=4.0, External*-array@0x48 (count@0x50, cap@0x54).
// Serialize (vtable +0x28, 0x140107050), stream order:
//   int count@0x50 -> resize +0x48 array -> per-element External FindOrLoad
//   (FUN_1400049c0 == the shared Desc_LoadExternalArray loop), then
//   int@0x20, int@0x24, float@0x28, float@0x2c, float@0x30, float@0x34,
//   float@0x38, bool@0x3c.
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

extern "C" void Desc_LoadExternalArray(void* serializer, int count, void* arrayBase);

static const uintptr_t kAnimalDescriptionVtable = 0x140162190;

static inline int AD_SerStreamErr(void* serBuf) {
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void AnimalDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kAnimalDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 5;
    *(int32_t*)(s + 0x24) = 4;
    *(float*)  (s + 0x28) = 1.0f;
    *(float*)  (s + 0x2c) = 0.0f;
    *(float*)  (s + 0x30) = 1.0f;
    *(float*)  (s + 0x34) = 0.5f;
    *(float*)  (s + 0x38) = 0.5f;
    *(uint8_t*)(s + 0x3c) = 1;
    *(float*)  (s + 0x40) = 4.0f;
    *(uint64_t*)(s + 0x48) = 0;   // External*-array ptr
    *(uint64_t*)(s + 0x50) = 0;   // count + cap
}

void AnimalDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_Serialize_Bool || !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // External*-array @ +0x48 (count @ +0x50, stride 8)
    g_fn_Serialize_Int(serializer, s + 0x50, nullptr);
    int n = *(int32_t*)(s + 0x50);
    if (AD_SerStreamErr(serializer) == 0 && n > 0 && fn_MemAlloc && g_pMemAllocSingleton) {
        void* buf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)n * 8, 8);
        *(void**)(s + 0x48) = buf;
        *(int32_t*)(s + 0x54) = n;
        if (buf) for (int i = 0; i < n; ++i) ((void**)buf)[i] = nullptr;
    }
    Desc_LoadExternalArray(serializer, n, *(void**)(s + 0x48));

    g_fn_Serialize_Int(serializer, s + 0x20, nullptr);
    g_fn_Serialize_Int(serializer, s + 0x24, nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x28), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x2c), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x30), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x34), nullptr);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x38), nullptr);
    g_fn_Serialize_Bool(serializer, s + 0x3c, nullptr);
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* AnimalDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kAnimalDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    void* arr = *(void**)(s + 0x48);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + 0x48) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AnimalDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AnimalDescription");
    return cached;
}
static int   AnimalDescription_Stub_Return2(void* /*self*/) { return 2; }
static int   AnimalDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* AnimalDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* AnimalDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AnimalDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AnimalComponent"); }

static void* g_AnimalDescription_Vtable[11] = {
    reinterpret_cast<void*>(&AnimalDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&AnimalDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&AnimalDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&AnimalDescription_Stub_Return2),    // +0x18 GetVersion=2
    reinterpret_cast<void*>(&AnimalDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&AnimalDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&AnimalDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&AnimalDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&AnimalDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&AnimalDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&AnimalDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterAnimalDescriptionVtable() {
    RegisterExeAddr(kAnimalDescriptionVtable, reinterpret_cast<uintptr_t>(g_AnimalDescription_Vtable));
}
extern "C" void* AnimalDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x58, 8);
    if (!p) return nullptr;
    AnimalDescription_Init(p);
    return p;
}
static void* AnimalDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    AnimalDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(AnimalDescription, "Description",
              &AnimalDescription_Factory, &AnimalDescription_CreateInPlace);
#endif
