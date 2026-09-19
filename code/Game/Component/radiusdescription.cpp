// code/Game/Component/radiusdescription.cpp
// Clean reimplementation of RadiusDescription (entity description block).
//
// RadiusDescription (0x30 bytes) — radius-of-effect descriptor.
// Extends Object::External (0x20 base). Vtable at 0x14015a580. Parent "Description".
//   +0x20: ExternalPtr  (default null)
//   +0x28: int          (radius, default 0x1e = 30)
//
// Factory  : MemAlloc 0x30 + External ctor + vtable + field defaults.
// Serialize (vtable +0x28, 0x140055530), stream order:
//   int         @ +0x28
//   ExternalPtr @ +0x20   (fn_ExternalPtr_Serialize3)
// GetVersion (vtable +0x18, 0x140055590): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x14000bb10): External::~External + free.
//
// Field knowledge harvested from engine/reimpl/radiusdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kRadiusDescriptionVtable = 0x14015a580;

static void RadiusDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kRadiusDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;       // ExternalPtr (null)
    *(uint32_t*)(s + 0x28) = 0x1e;    // int radius = 30
}

void RadiusDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Int || !fn_ExternalPtr_Serialize3) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // int +0x28 // B.3.4 bridge
    fn_ExternalPtr_Serialize3(s + 0x20, serializer);     // ExternalPtr +0x20
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

void* RadiusDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kRadiusDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t RadiusDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("RadiusDescription");
    return cached;
}

static int   RadiusDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   RadiusDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* RadiusDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* RadiusDescription_CreateInPlace(void* memory);

static void* g_RadiusDescription_Vtable[11] = {
    reinterpret_cast<void*>(&RadiusDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&RadiusDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&RadiusDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&RadiusDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&RadiusDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterRadiusDescriptionVtable() {
    RegisterExeAddr(kRadiusDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_RadiusDescription_Vtable));
}

extern "C" void* RadiusDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    RadiusDescription_Init(p);
    return p;
}

static void* RadiusDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    RadiusDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(RadiusDescription, "Description",
              &RadiusDescription_Factory, &RadiusDescription_CreateInPlace);

