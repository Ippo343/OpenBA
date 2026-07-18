// code/Game/Component/selectdescription.cpp
// Clean reimplementation of SelectDescription (entity description block).
//
// SelectDescription (0x48 bytes) — selection-tool descriptor.
// Extends Object::External (0x20 base). Vtable at 0x14015a5f0. Parent "Description".
//   +0x20: ExternalPtr  (default null)
//   +0x28: String       (default empty)
//   +0x30: ExternalPtr  (default null)
//   +0x38: String       (default empty)
//   +0x40: enum/int     (default 0)
//
// Factory  : MemAlloc 0x48 + External ctor + vtable + field defaults (all zero).
// Serialize (vtable +0x28, 0x140055630), stream order:
//   ExternalPtr @ +0x20  (fn_ExternalPtr_Serialize)
//   String      @ +0x28
//   ExternalPtr @ +0x30  (fn_ExternalPtr_Serialize2)
//   String      @ +0x38
//   enum        @ +0x40  (serializer-encoding vtable +0x98 dispatch)
// GetVersion (vtable +0x18, 0x1400556c0): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x14000bcc0): ~String(+0x38) + ~String(+0x28) +
//   External::~External + free.
//
// Field knowledge harvested from engine/reimpl/selectdescription.cpp (DLL-mode reimpl).
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kSelectDescriptionVtable = 0x14015a5f0;

static void SelectDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kSelectDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;   // String
    *(uint64_t*)(s + 0x30) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x38) = 0;   // String
    *(uint32_t*)(s + 0x40) = 0;   // enum
}

void SelectDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_String_Serialize ||
        !fn_ExternalPtr_Serialize || !fn_ExternalPtr_Serialize2) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_Serialize(s + 0x20, serializer);          // ExternalPtr +0x20
    g_fn_String_Serialize(s + 0x28, serializer);             // String      +0x28
    fn_ExternalPtr_Serialize2(s + 0x30, serializer);         // ExternalPtr +0x30
    g_fn_String_Serialize(s + 0x38, serializer);             // String      +0x38
    SerializerEncodingVCall(serializer, 0x98, s + 0x40);     // enum        +0x40
    g_fn_Serializer_Dtor(serializer);
}

void* SelectDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kSelectDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_String_Dtor) {
        fn_String_Dtor(s + 0x38);   // String +0x38
        fn_String_Dtor(s + 0x28);   // String +0x28
    }
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t SelectDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("SelectDescription");
    return cached;
}

static int   SelectDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   SelectDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* SelectDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* SelectDescription_CreateInPlace(void* memory);

static void* g_SelectDescription_Vtable[11] = {
    reinterpret_cast<void*>(&SelectDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&SelectDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&SelectDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&SelectDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&SelectDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&SelectDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&SelectDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&SelectDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&SelectDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&SelectDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&SelectDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterSelectDescriptionVtable() {
    RegisterExeAddr(kSelectDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_SelectDescription_Vtable));
}

extern "C" void* SelectDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 8);
    if (!p) return nullptr;
    SelectDescription_Init(p);
    return p;
}

static void* SelectDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    SelectDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(SelectDescription, "Description",
              &SelectDescription_Factory, &SelectDescription_CreateInPlace);

