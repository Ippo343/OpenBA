// code/Game/Component/ambientemitterdescription.cpp
// Clean reimplementation of AmbientEmitterDescription (entity sub-description).
//
// AmbientEmitterDescription (0x38 bytes) — extends Object::External (0x20 base).
// Parent type name: "Description". Vtable @ 0x140158ae0.  GetVersion = 2.
//
// Field layout (from Factory + Serialize FUN_1400431b0 + dtor FUN_140001660):
//   +0x20: bool   _enabled            (default 1)
//   +0x28: void*  _emitters[]         ExternalPtr array buffer (default null)
//   +0x30: int32  _emitterCount       (default 0)  array {ptr@0x28,count@0x30,cap@0x34}
//   +0x34: int32  _emitterCap         (default 0)
//
// Serialize (FUN_1400431b0) stream order:
//   1. int   count                              -> +0x30
//   2. (load) resize ptr-array +0x28 to count   (FUN_140001bb0 stride 8)
//   3. per element: ExternalPtr serialize        (FUN_140001c70 loop)
//   4. bool  _enabled                            -> +0x20
//
// Decompiled from Application-x64.exe (READ-ONLY Ghidra).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace {
constexpr uintptr_t kVtable  = 0x140158ae0;
constexpr int       kVersion = 2;
}

static void* AmbientEmitterDescription_Ctor_Impl(void* mem) {
    if (fn_External_External) fn_External_External(mem);
    uintptr_t s = (uintptr_t)mem;
    *(void**)mem = (void*)ResolveAddr(Module::Exe, kVtable);
    *(uint8_t*)(s + 0x20)  = 1;    // _enabled
    *(uint64_t*)(s + 0x28) = 0;    // _emitters ptr
    *(uint64_t*)(s + 0x30) = 0;    // count + cap
    return mem;
}

// Serialize (vtable +0x28, FUN_1400431b0)
void AmbientEmitterDescription_Serialize(void* self, void* stream) {
    uintptr_t s = (uintptr_t)self;
    uint8_t serializer[24];
    if (!g_fn_Serializer_Ctor || !g_fn_Serialize_Int || !g_fn_Serialize_Bool ||
        !fn_ExternalPtr_Serialize2) {
        return;
    }
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // 1. count (+0x30)
    g_fn_Serialize_Int(serializer, (void*)(s + 0x30), nullptr);
    int count = *(int32_t*)(s + 0x30);

    // 2. on load (serializer version == 0), resize ptr-array (+0x28) to count
    void* serObj = *(void**)(serializer + 8);
    bool loading = serObj && (*(int*)((uintptr_t)serObj + 8) == 0);
    if (loading && fn_Array_Resize_Stride08) {
        fn_Array_Resize_Stride08((void*)(s + 0x28), count);
    }

    // 3. per-element ExternalPtr serialize (each slot is an External*)
    uint8_t* buf = *(uint8_t**)(s + 0x28);
    if (buf) {
        for (int i = 0; i < count; ++i) {
            fn_ExternalPtr_Serialize2(buf + (size_t)i * 8, serializer);
        }
    }

    // 4. _enabled (+0x20)
    g_fn_Serialize_Bool(serializer, (void*)(s + 0x20), nullptr);

    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// Dtor (vtable +0x00, FUN_140001660)
void* AmbientEmitterDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t s = (uintptr_t)self;
    *(void**)self = (void*)ResolveAddr(Module::Exe, kVtable);
    if (fn_MemFreeStatic) fn_MemFreeStatic(*(void**)(s + 0x28));  // free ptr-array buffer
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AmbientEmitterDescription_GetTypeId(void*) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AmbientEmitterDescription");
    return cached;
}
static int   Vtbl_GetVersion(void*) { return kVersion; }
static int   Vtbl_Return0(void*) { return 0; }
static void* Vtbl_ReturnNull(void*, ...) { return nullptr; }

// Vtable @ 0x140158ae0 (Object::External 9-slot layout). +0x18 GetVersion=2.
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AmbientEmitterDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AmbientEmitterComponent"); }

static void* g_Vtable[11] = {
    reinterpret_cast<void*>(&AmbientEmitterDescription_Dtor),      // +0x00
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                     // +0x08 GetTypeInfo
    reinterpret_cast<void*>(&AmbientEmitterDescription_GetTypeId), // +0x10
    reinterpret_cast<void*>(&Vtbl_GetVersion),                     // +0x18
    reinterpret_cast<void*>(&Vtbl_Return0),                        // +0x20 Compile
    reinterpret_cast<void*>(&AmbientEmitterDescription_Serialize), // +0x28
    reinterpret_cast<void*>(&Vtbl_Return0),                        // +0x30 Cleanup
    reinterpret_cast<void*>(&Vtbl_Return0),                        // +0x38 GetDesc
    reinterpret_cast<void*>(&AmbientEmitterDescription_GetComponentTypeId),                     // +0x40 Slot8
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                     // +0x48 guard
    reinterpret_cast<void*>(&Vtbl_ReturnNull),                     // +0x50 guard
};

extern "C" void RegisterAmbientEmitterDescriptionVtable() {
    RegisterExeAddr(kVtable, reinterpret_cast<uintptr_t>(g_Vtable));
}
extern "C" void* AmbientEmitterDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    return AmbientEmitterDescription_Ctor_Impl(p);
}
extern "C" void* AmbientEmitterDescription_CreateInPlace(void* mem) {
    if (!mem) return nullptr;
    return AmbientEmitterDescription_Ctor_Impl(mem);
}

REGISTER_TYPE(AmbientEmitterDescription, "Description",
              &AmbientEmitterDescription_Factory, &AmbientEmitterDescription_CreateInPlace);
#endif
