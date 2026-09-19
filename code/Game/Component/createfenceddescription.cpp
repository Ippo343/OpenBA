// code/Game/Component/createfenceddescription.cpp
// Clean reimplementation of CreateFencedDescription (entity description block).
//
// CreateFencedDescription (0x78 bytes) — fence/wall placement tool.
// Extends Object::External (0x20 base). Vtable at 0x1401591a0. Parent "Description".
//   +0x20: enum (int)
//   +0x28: ExternalPtr[6] (inline fixed array, 6 * 8 bytes -> +0x28..+0x58)
//   +0x58: int   +0x5c: int   +0x60: int   +0x64: int
//   +0x68: bool
//   +0x70: ExternalPtr (single)
//
// Factory  : MemAlloc 0x78 + External ctor + vtable + field defaults (zero).
// Serialize (vtable +0x28, 0x1400478f0 CreateFencedDescription_Slot5), stream order:
//   enum  @ +0x20  (encoding vtable +0x98, 3-arg variant: (encoding, field, 0))
//   ExternalRefArray(count=6) @ +0x28  (FUN_1400049c0 / fn_ExternalRefArray_Serialize)
//   int   @ +0x58
//   int   @ +0x5c
//   int   @ +0x60
//   int   @ +0x64
//   bool  @ +0x68
//   ExternalPtr @ +0x70  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
// GetVersion (vtable +0x18, 0x1400479c0): mov eax,3; ret -> version 3.
// Dtor (vtable +0x00, 0x140004960): External::~External + free.
//
// NOTE: fn_ExternalRefArray_Serialize -> exe FUN_1400049c0, null in standalone until
// 0x1400049c0 is registered (REPORTED). param4 is a dead init -> nullptr safe.
//
// Field knowledge harvested from engine/reimpl/createfenceddescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kCreateFencedDescriptionVtable = 0x1401591a0;

// 3-arg encoding vcall variant: (**(*encoding + 0x98))(encoding, field, extra)
static inline void FencedEncodingVCall3(void* serializer, void* field, int extra) {
    void* encoding = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(serializer) + 8);
    if (!encoding) return;
    void* vtable = *reinterpret_cast<void**>(encoding);
    typedef void (*VFunc_t)(void*, void*, int);
    auto fn = *reinterpret_cast<VFunc_t*>(reinterpret_cast<uintptr_t>(vtable) + 0x98);
    fn(encoding, field, extra);
}

static void CreateFencedDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreateFencedDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint32_t*)(s + 0x20) = 0;   // enum
    for (int off = 0x28; off <= 0x50; off += 8) *(uint64_t*)(s + off) = 0; // ExtPtr[6]
    *(uint32_t*)(s + 0x58) = 0;
    *(uint32_t*)(s + 0x5c) = 0;
    *(uint32_t*)(s + 0x60) = 0;
    *(uint32_t*)(s + 0x64) = 0;
    *(uint8_t*) (s + 0x68) = 0;   // bool
    *(uint64_t*)(s + 0x70) = 0;   // ExternalPtr
}

void CreateFencedDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !g_fn_Serialize_Bool || !fn_ExternalRefArray_Serialize ||
        !fn_ExternalPtr_BuildSerialize) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    FencedEncodingVCall3(serializer, s + 0x20, 0);                  // enum +0x20
    fn_ExternalRefArray_Serialize(serializer, 6, s + 0x28, nullptr);// ExtPtr[6] +0x28
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x58), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x5c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x60), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x64), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x68), nullptr);   // B.3.4 bridge
    fn_ExternalPtr_BuildSerialize(s + 0x70, serializer);          // ExtPtr +0x70
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* CreateFencedDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCreateFencedDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateFencedDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreateFencedDescription");
    return cached;
}

static int   CreateFencedDescription_VtblStub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   CreateFencedDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreateFencedDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreateFencedDescription_CreateInPlace(void* memory);

static void* g_CreateFencedDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreateFencedDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreateFencedDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_Return3),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreateFencedDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreateFencedDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreateFencedDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreateFencedDescriptionVtable() {
    RegisterExeAddr(kCreateFencedDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreateFencedDescription_Vtable));
}

extern "C" void* CreateFencedDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8);
    if (!p) return nullptr;
    CreateFencedDescription_Init(p);
    return p;
}

static void* CreateFencedDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreateFencedDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreateFencedDescription, "Description",
              &CreateFencedDescription_Factory, &CreateFencedDescription_CreateInPlace);

