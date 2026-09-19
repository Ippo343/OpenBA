// code/Game/Component/ambientenvironmentdescription.cpp
// Clean reimplementation of AmbientEnvironmentDescription (entity description block).
//
// AmbientEnvironmentDescription (0x38 bytes) — ambient-zone environment override.
// Extends Object::External (0x20 base). Vtable at 0x140158bf0. Parent "Description".
//   +0x20: void* dynamic entry-array base (0x28-byte / 5-qword elements)
//   +0x28: int   count   (+0x2c cap)
//   +0x30: float (default 0x42000000 = 32.0f)
//   +0x34: float (default 0x42340000 = 45.0f)
//   Each array element (0x28 bytes):
//     +0x00 enum   +0x08 ExternalPtr  +0x10 ExternalPtr  +0x18 ExternalPtr  +0x20 int
//
// Factory  : MemAlloc 0x38 + External ctor + vtable + defaults
//            (+0x20=0, +0x28=0, +0x30=32.0f, +0x34=45.0f).
// Serialize (vtable +0x28, 0x140043950), stream order:
//   int count @ +0x28
//   [if loading: FUN_140002540(+0x20, count)  resize 0x28-byte-element array]
//   for each element (stride 0x28):
//     enum        @ elem+0x00  (encoding vtable +0x98)
//     ExternalPtr @ elem+0x08  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
//     ExternalPtr @ elem+0x10  (FUN_1400019a0)
//     ExternalPtr @ elem+0x18  (FUN_1400019a0)
//     int         @ elem+0x20
//   float @ +0x30
//   float @ +0x34
// GetVersion (vtable +0x18, 0x140043a70): mov eax,2; ret -> version 2.
// Dtor (vtable +0x00, 0x140002100): External::~External + free.
//
// NOTE: exe helpers FUN_1400019a0 (single ambient ExternalPtr serialize) and
// FUN_140002540 (0x28-stride array resize) are NOT in engine_funcs.h and resolve via
// ResolveAddr(Module::Exe,..) — null in standalone until those addrs are registered
// (REPORTED to orchestrator). If unavailable, the array loop is skipped (the scalar
// count + trailing floats still serialize, but a non-empty entry array would desync —
// blocked on those two helpers being registered).
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kAmbientEnvironmentDescriptionVtable = 0x140158bf0;

// exe helper FUN_140002540 — resize 0x28-byte-element array {ptr@arr+0, count@arr+8, cap@arr+0xc}.
typedef void (*fn_AmbientArrayResize_t)(void* arrayBase, int newCount);
// exe helper FUN_1400019a0 — serialize a single ambient ExternalPtr field.
typedef void (*fn_AmbientExternalPtr_Serialize_t)(void* field, void* serializer);

static fn_AmbientArrayResize_t           s_fn_AmbientArrayResize = nullptr;
static fn_AmbientExternalPtr_Serialize_t s_fn_AmbientExternalPtr = nullptr;
static bool s_ambientHelpersResolved = false;

static void AmbientEnvironmentDescription_EnsureHelpers() {
    if (s_ambientHelpersResolved) return;
    s_fn_AmbientArrayResize =
        (fn_AmbientArrayResize_t)ResolveAddr(Module::Exe, 0x140002540);
    s_fn_AmbientExternalPtr =
        (fn_AmbientExternalPtr_Serialize_t)ResolveAddr(Module::Exe, 0x1400019a0);
    s_ambientHelpersResolved = true;
}

// ============================================================
// Reimpl of FUN_140002540 (grow/shrink the 0x28-byte-element entry array)
// ============================================================
// Resizes the array {ptr@arr+0, count@arr+8, cap@arr+0xc} (stride 0x28). On grow
// past capacity: cap = newCount (or newCount*3 when growing by exactly one from a
// non-empty array), MemAlloc(cap*0x28, 8), copy existing elements, zero-init the new
// tail with elem+0x20 = 1 default, free the old block. 1:1 port of the exe helper.
extern "C" void AmbientArrayResize_Reimpl(void* arrayBase, int newCount) {
    uint8_t* arr = reinterpret_cast<uint8_t*>(arrayBase);
    int oldCount = *(int*)(arr + 8);
    if (newCount == oldCount) return;

    void* oldPtr = *(void**)(arr + 0);
    *(int*)(arr + 8) = newCount;
    if (*(int*)(arr + 0xc) < newCount) {
        *(int*)(arr + 0xc) = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            *(int*)(arr + 0xc) = newCount * 3;
        }
        if (fn_MemAlloc && g_pMemAllocSingleton) {
            void* newPtr = fn_MemAlloc(g_pMemAllocSingleton,
                                       (uint64_t)(*(int*)(arr + 0xc)) * 0x28, 8);
            *(void**)(arr + 0) = newPtr;
        }
    }

    int start = 0;
    if (oldPtr == *(void**)(arr + 0)) start = oldCount;  // grown in place: keep existing

    uint8_t* base = *(uint8_t**)(arr + 0);
    for (int i = start; i < newCount; ++i) {
        uint8_t* dst = base + (intptr_t)i * 0x28;
        if (!dst) continue;
        if (i < oldCount) {
            // copy old element (5 qwords = 0x28 bytes)
            const uint8_t* src = reinterpret_cast<const uint8_t*>(oldPtr) + (intptr_t)i * 0x28;
            for (int q = 0; q < 5; ++q)
                *(uint64_t*)(dst + q * 8) = *(const uint64_t*)(src + q * 8);
        } else {
            // zero-init new element, +0x20 = 1 default
            *(uint32_t*)(dst + 0x00) = 0;
            *(uint64_t*)(dst + 0x08) = 0;
            *(uint64_t*)(dst + 0x10) = 0;
            *(uint64_t*)(dst + 0x18) = 0;
            *(uint32_t*)(dst + 0x20) = 1;
        }
    }

    if (oldPtr != *(void**)(arr + 0) && oldPtr && fn_MemFreeStatic) {
        fn_MemFreeStatic(oldPtr);
    }
}

static void AmbientEnvironmentDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kAmbientEnvironmentDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;            // array base
    *(uint64_t*)(s + 0x28) = 0;            // count (+cap)
    *(uint32_t*)(s + 0x30) = 0x42000000;   // float 32.0f
    *(uint32_t*)(s + 0x34) = 0x42340000;   // float 45.0f
}

void AmbientEnvironmentDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalPtr_BuildSerialize) {
        return;
    }
    AmbientEnvironmentDescription_EnsureHelpers();

    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    int count = *(int*)(s + 0x28);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(count, nullptr);     // int count +0x28   // B.3.4 bridge

    // On load: resize the 0x28-byte-element array at +0x20.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        if (s_fn_AmbientArrayResize) s_fn_AmbientArrayResize(s + 0x20, count);
    }

    // Per-element loop (requires both exe helpers; skipped if unavailable in standalone).
    if (count > 0 && s_fn_AmbientExternalPtr) {
        uint8_t* elem = *(uint8_t**)(s + 0x20);
        for (int i = 0; i < count; ++i, elem += 0x28) {
            reinterpret_cast<IO::Serializer*>(serializer)
                ->Serialize(*reinterpret_cast<unsigned int*>(elem + 0x00), nullptr);   // B.3.4 bridge  (enum)
            fn_ExternalPtr_BuildSerialize(elem + 0x08, serializer);  // ExternalPtr
            s_fn_AmbientExternalPtr(elem + 0x10, serializer);        // ExternalPtr
            s_fn_AmbientExternalPtr(elem + 0x18, serializer);        // ExternalPtr
            reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(elem + 0x20), nullptr);    // B.3.4 bridge  (int)
        }
    }

    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge  (float +0x30, 4-byte serialize)
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x34), nullptr);   // B.3.4 bridge  (float +0x34)

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* AmbientEnvironmentDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kAmbientEnvironmentDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_MemFreeStatic && *(void**)(s + 0x20)) {
        fn_MemFreeStatic(*(void**)(s + 0x20));   // dynamic entry array
    }
    *(uint64_t*)(s + 0x20) = 0;
    *(uint64_t*)(s + 0x28) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AmbientEnvironmentDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AmbientEnvironmentDescription");
    return cached;
}

static int   AmbientEnvironmentDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   AmbientEnvironmentDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* AmbientEnvironmentDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* AmbientEnvironmentDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AmbientEnvironmentDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AmbientEnvironmentComponent"); }

static void* g_AmbientEnvironmentDescription_Vtable[11] = {
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&AmbientEnvironmentDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterAmbientEnvironmentDescriptionVtable() {
    RegisterExeAddr(kAmbientEnvironmentDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_AmbientEnvironmentDescription_Vtable));
}

extern "C" void* AmbientEnvironmentDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    AmbientEnvironmentDescription_Init(p);
    return p;
}

static void* AmbientEnvironmentDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    AmbientEnvironmentDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(AmbientEnvironmentDescription, "Description",
              &AmbientEnvironmentDescription_Factory, &AmbientEnvironmentDescription_CreateInPlace);

