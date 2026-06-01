// code/Game/Component/spawndescription.cpp
// SpawnDescription (entity description block) — spawn-table parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a6f0. Parent "Description".
// Size 0x30. GetVersion=2.
//
// Layout: a growable array of 16-byte spawn entries.
//   void* array @ +0x20, int count @ +0x28, int capacity @ +0x2c
//   element { ExternalPtr @ +0x00 (8B), float @ +0x08, float @ +0x0c }
// Factory 0x14014b740-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   array @ +0x20 = 0, count/cap @ +0x28 = 0 (8-byte store)
// Serialize (vtable +0x28, 0x1400560b0), stream order:
//   int count @ +0x28
//   if stream-ok: resize array @ +0x20 to count (exe helper FUN_14000c520)
//   per element [0..count): ExternalPtr(elem+0) via fn_ExternalPtr_BuildSerialize
//     (FUN_140002340), float(elem+8), float(elem+0xc)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kSpawnDescriptionVtable = 0x14015a6f0;
// Exe-internal spawn-array resize helper (resizes the 16-byte-stride vector at
// +0x20). NEEDS standalone address registration (see report).
static const uintptr_t kSpawnArrayResize = 0x14000c520;
typedef void (*SpawnArrayResize_t)(void* arrayHeader, int newCount);

static inline int Spawn_SerStreamErr(void* serBuf) {
    // serializer[0] -> stream; stream+8 = error/status int (0 == OK)
    return *reinterpret_cast<int*>(
        reinterpret_cast<uint8_t*>(*reinterpret_cast<void**>(serBuf)) + 8);
}

static void SpawnDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kSpawnDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // array ptr
    *(uint64_t*)(s + 0x28) = 0;   // count + capacity
}

void SpawnDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_Serializer_Serialize_Float || !fn_ExternalPtr_BuildSerialize) return;
    SpawnArrayResize_t resize =
        (SpawnArrayResize_t)ResolveAddr(Module::Exe, kSpawnArrayResize);

    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    int count = *(int32_t*)(s + 0x28);
    g_fn_Serialize_Int(serializer, &count, nullptr);
    if (Spawn_SerStreamErr(serializer) == 0 && resize) {
        resize(s + 0x20, count);   // header at +0x20 {ptr, count, cap}
    }
    if (count > 0) {
        uint8_t* elem = *(uint8_t**)(s + 0x20);
        for (int i = 0; i < count && elem; ++i, elem += 0x10) {
            fn_ExternalPtr_BuildSerialize(elem + 0x00, serializer);
            fn_Serializer_Serialize_Float(serializer, (float*)(elem + 0x08), nullptr);
            fn_Serializer_Serialize_Float(serializer, (float*)(elem + 0x0c), nullptr);
        }
    }
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* SpawnDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kSpawnDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    void* arr = *(void**)(s + 0x20);
    if (arr && fn_MemFreeStatic) fn_MemFreeStatic(arr);
    *(void**)(s + 0x20) = nullptr;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t SpawnDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("SpawnDescription");
    return cached;
}
static int   SpawnDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   SpawnDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* SpawnDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* SpawnDescription_CreateInPlace(void* memory);

static void* g_SpawnDescription_Vtable[11] = {
    reinterpret_cast<void*>(&SpawnDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&SpawnDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&SpawnDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&SpawnDescription_Stub_Return2),    // +0x18 GetVersion=2
    reinterpret_cast<void*>(&SpawnDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&SpawnDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&SpawnDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&SpawnDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&SpawnDescription_Stub_ReturnNull), // +0x40
    reinterpret_cast<void*>(&SpawnDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&SpawnDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterSpawnDescriptionVtable() {
    RegisterExeAddr(kSpawnDescriptionVtable, reinterpret_cast<uintptr_t>(g_SpawnDescription_Vtable));
}
extern "C" void* SpawnDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    SpawnDescription_Init(p);
    return p;
}
static void* SpawnDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    SpawnDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(SpawnDescription, "Description",
              &SpawnDescription_Factory, &SpawnDescription_CreateInPlace);
#endif
