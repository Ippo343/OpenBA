// code/Game/Component/multiobjectdescription.cpp
// Clean reimplementation of MultiObjectDescription (entity description block).
//
// MultiObjectDescription (0x20 bytes) — groups multiple sub-objects. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x140163d80. Parent "Description".
//
// Factory (MultiObjectDescription_Factory): MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x140125b50): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140125b90): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x140036060): External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kMultiObjectDescriptionVtable = 0x140163d80;

static void MultiObjectDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kMultiObjectDescriptionVtable);
    *(void**)self = s_vtable;
}

void MultiObjectDescription_Serialize(void* self, void* stream) {
    (void)self;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* MultiObjectDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kMultiObjectDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t MultiObjectDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MultiObjectDescription");
    return cached;
}

static int   MultiObjectDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   MultiObjectDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* MultiObjectDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* MultiObjectDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* MultiObjectDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "MultiObjectComponent"); }

static void* g_MultiObjectDescription_Vtable[11] = {
    reinterpret_cast<void*>(&MultiObjectDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&MultiObjectDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&MultiObjectDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&MultiObjectDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&MultiObjectDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&MultiObjectDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterMultiObjectDescriptionVtable() {
    RegisterExeAddr(kMultiObjectDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_MultiObjectDescription_Vtable));
}

extern "C" void* MultiObjectDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    MultiObjectDescription_Init(p);
    return p;
}

static void* MultiObjectDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MultiObjectDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(MultiObjectDescription, "Description",
              &MultiObjectDescription_Factory, &MultiObjectDescription_CreateInPlace);

