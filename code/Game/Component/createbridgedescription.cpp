// code/Game/Component/createbridgedescription.cpp
// Clean reimplementation of CreateBridgeDescription (entity description block).
//
// CreateBridgeDescription (0x60 bytes) — bridge-building placement tool.
// Extends Object::External (0x20 base). Vtable at 0x140162c30. Parent "Description".
//   +0x20: enum   +0x24: enum   +0x28: enum   +0x2c: enum   +0x30: enum
//   +0x34: int
//   +0x38: ExternalPtr[5] (inline fixed array, 5 * 8 bytes -> +0x38..+0x60)
//
// Factory  : MemAlloc 0x60 + External ctor + vtable. (Ctor zero-inits all fields.)
// Serialize (vtable +0x28, 0x1401195d0 CreateBridgeDescription_Slot5), stream order:
//   enum  @ +0x20  (encoding vtable +0x98)
//   enum  @ +0x28  (encoding vtable +0x98)
//   enum  @ +0x2c  (encoding vtable +0x98)
//   enum  @ +0x24  (encoding vtable +0x98)
//   enum  @ +0x30  (encoding vtable +0x98)
//   int   @ +0x34
//   ExternalRefArray(count=5) @ +0x38  (FUN_1400049c0 / fn_ExternalRefArray_Serialize)
// GetVersion (vtable +0x18, 0x1401196a0): mov eax,2; ret -> version 2.
// Dtor (vtable +0x00, 0x1400328f0): External::~External + free.
//
// NOTE: fn_ExternalRefArray_Serialize resolves to exe FUN_1400049c0 — null in
// standalone until 0x1400049c0 is registered (REPORTED to orchestrator). The 4th
// param (param4) is a dead init inside FUN_1400049c0, so nullptr is safe.
//
// Field knowledge harvested from engine/reimpl/createbridgedescription.cpp (DLL-mode reimpl).
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kCreateBridgeDescriptionVtable = 0x140162c30;

static void CreateBridgeDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreateBridgeDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    // 0x20..0x60 zero-initialized (5 enums, int, 5 inline ExternalPtrs)
    for (int off = 0x20; off < 0x60; off += 8) *(uint64_t*)(s + off) = 0;
}

void CreateBridgeDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Int ||
        !fn_ExternalRefArray_Serialize) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    SerializerEncodingVCall(serializer, 0x98, s + 0x20);   // enum +0x20
    SerializerEncodingVCall(serializer, 0x98, s + 0x28);   // enum +0x28
    SerializerEncodingVCall(serializer, 0x98, s + 0x2c);   // enum +0x2c
    SerializerEncodingVCall(serializer, 0x98, s + 0x24);   // enum +0x24
    SerializerEncodingVCall(serializer, 0x98, s + 0x30);   // enum +0x30
    g_fn_Serialize_Int(serializer, s + 0x34, nullptr);     // int  +0x34
    fn_ExternalRefArray_Serialize(serializer, 5, s + 0x38, nullptr); // ExtPtr[5] +0x38
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* CreateBridgeDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCreateBridgeDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateBridgeDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreateBridgeDescription");
    return cached;
}

static int   CreateBridgeDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   CreateBridgeDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreateBridgeDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreateBridgeDescription_CreateInPlace(void* memory);

static void* g_CreateBridgeDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreateBridgeDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreateBridgeDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreateBridgeDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreateBridgeDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreateBridgeDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreateBridgeDescriptionVtable() {
    RegisterExeAddr(kCreateBridgeDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreateBridgeDescription_Vtable));
}

extern "C" void* CreateBridgeDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x60, 8);
    if (!p) return nullptr;
    CreateBridgeDescription_Init(p);
    return p;
}

static void* CreateBridgeDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreateBridgeDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreateBridgeDescription, "Description",
              &CreateBridgeDescription_Factory, &CreateBridgeDescription_CreateInPlace);

#endif // STANDALONE_EXE
