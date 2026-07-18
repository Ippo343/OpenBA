// code/Game/Component/createroaddescription.cpp
// Clean reimplementation of CreateRoadDescription (entity description block).
//
// CreateRoadDescription (0x30 bytes) — road-drawing placement tool.
// Extends Object::External (0x20 base). Vtable at 0x140162df0. Parent "Description".
//   +0x20: bool         (default 0)
//   +0x28: ExternalPtr  (created-resource description ref, default null)
//
// Factory  : MemAlloc 0x30 + External ctor + vtable + field defaults.
// Serialize (vtable +0x28, 0x140119c80), stream order:
//   bool        @ +0x20
//   ExternalPtr @ +0x28   (fn_ExternalPtr_Serialize3)
// GetVersion (vtable +0x18, 0x140119ce0): mov eax,2; ret -> version 2.
// Dtor (vtable +0x00, 0x140032e50): External::~External + free.
//
// Field knowledge harvested from engine/reimpl/createroaddescription.cpp (DLL-mode reimpl).
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kCreateRoadDescriptionVtable = 0x140162df0;

static void CreateRoadDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreateRoadDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint8_t*) (s + 0x20) = 0;   // bool
    *(uint64_t*)(s + 0x28) = 0;   // ExternalPtr (weak)
}

void CreateRoadDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Bool || !fn_ExternalPtr_Serialize3) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Bool(serializer, s + 0x20, nullptr);   // bool        +0x20
    fn_ExternalPtr_Serialize3(s + 0x28, serializer);      // ExternalPtr +0x28
    g_fn_Serializer_Dtor(serializer);
}

void* CreateRoadDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCreateRoadDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateRoadDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreateRoadDescription");
    return cached;
}

static int   CreateRoadDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   CreateRoadDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreateRoadDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreateRoadDescription_CreateInPlace(void* memory);

static void* g_CreateRoadDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreateRoadDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreateRoadDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreateRoadDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreateRoadDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreateRoadDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreateRoadDescriptionVtable() {
    RegisterExeAddr(kCreateRoadDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreateRoadDescription_Vtable));
}

extern "C" void* CreateRoadDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    CreateRoadDescription_Init(p);
    return p;
}

static void* CreateRoadDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreateRoadDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreateRoadDescription, "Description",
              &CreateRoadDescription_Factory, &CreateRoadDescription_CreateInPlace);

