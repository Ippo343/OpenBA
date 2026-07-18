// code/Game/Component/createpathdescription.cpp
// Clean reimplementation of CreatePathDescription (entity description block).
//
// CreatePathDescription (0x20 bytes) — path-drawing placement tool. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x140159210. Parent "Description".
//
// Factory  : MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x140047a70): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140047ab0): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140004d30): External::~External + free.
//
// Field knowledge harvested from engine/reimpl/createpathdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kCreatePathDescriptionVtable = 0x140159210;

static void CreatePathDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreatePathDescriptionVtable);
    *(void**)self = s_vtable;
}

void CreatePathDescription_Serialize(void* self, void* stream) {
    (void)self;
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serializer_Dtor(serializer);
}

void* CreatePathDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCreatePathDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreatePathDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreatePathDescription");
    return cached;
}

static int   CreatePathDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   CreatePathDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreatePathDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreatePathDescription_CreateInPlace(void* memory);

static void* g_CreatePathDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreatePathDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreatePathDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreatePathDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreatePathDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreatePathDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreatePathDescriptionVtable() {
    RegisterExeAddr(kCreatePathDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreatePathDescription_Vtable));
}

extern "C" void* CreatePathDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    CreatePathDescription_Init(p);
    return p;
}

static void* CreatePathDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreatePathDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreatePathDescription, "Description",
              &CreatePathDescription_Factory, &CreatePathDescription_CreateInPlace);

