// code/Game/Component/createdragdescription.cpp
// Clean reimplementation of CreateDragDescription (entity description block).
//
// CreateDragDescription (0x40 bytes) — drag-to-build placement tool parameters.
// Extends Object::External (0x20 base). Vtable at 0x140162ca0.
// Parent type "Description".
//
// Factory  : 0x140032970  (MemAlloc 0x40 + ctor 0x140119860)
// CreateInPlace: 0x1400329b0
// Ctor defaults (from CreateDragDescription_Ctor 0x140119860):
//   int  @ +0x20 = 0   (enum: drag axis/shape)
//   bool @ +0x24 = 0
//   int  @ +0x28 = 1
//   int  @ +0x2c = 1
//   int  @ +0x30 = 1
//   int  @ +0x34 = 1
//   void*@ +0x38 = 0   (ExternalRef: created-resource description, weak)
// Serialize (vtable +0x28, 0x140119750), stream order:
//   enum        @ +0x20   (serializer-encoding vtable +0x98 dispatch)
//   int         @ +0x28
//   int         @ +0x2c
//   int         @ +0x30
//   int         @ +0x34
//   bool        @ +0x24
//   ExternalRef @ +0x38   (FUN_140002340)
// GetVersion (vtable +0x18, 0x140119810): mov eax,2; ret -> file version 2.
// Dtor (vtable +0x00, 0x1400329e0): vtable reset + External::~External + free
//   (the +0x38 ExternalRef is a weak pointer, not owned — no per-field dtor).
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

static const uintptr_t kCreateDragDescriptionVtable = 0x140162ca0;

static void CreateDragDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCreateDragDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 0;   // enum
    *(uint8_t*)(s + 0x24) = 0;   // bool
    *(int32_t*)(s + 0x28) = 1;   // int
    *(int32_t*)(s + 0x2c) = 1;   // int
    *(int32_t*)(s + 0x30) = 1;   // int
    *(int32_t*)(s + 0x34) = 1;   // int
    *(uint64_t*)(s + 0x38) = 0;  // ExternalRef (weak)
}

// ============================================================
// CreateDragDescription_Serialize (vtable +0x28, 0x140119750)
// ============================================================
void CreateDragDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)
        ->Serialize(*reinterpret_cast<unsigned int*>(s + 0x20), nullptr);   // B.3.4 bridge   // enum +0x20
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge   // int  +0x28
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge   // int  +0x2c
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge   // int  +0x30
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x34), nullptr);   // B.3.4 bridge   // int  +0x34
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x24), nullptr);  // B.3.4 bridge   // bool +0x24
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x38, serializer);// ref  +0x38
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// CreateDragDescription_Dtor (vtable +0x00, 0x1400329e0)
// ============================================================
void* CreateDragDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kCreateDragDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CreateDragDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CreateDragDescription");
    return cached;
}

static int   CreateDragDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   CreateDragDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CreateDragDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CreateDragDescription_CreateInPlace(void* memory);

static void* g_CreateDragDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CreateDragDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&CreateDragDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&CreateDragDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_Return0),   // +0x30 Draw/Cleanup
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_ReturnNull),// +0x40 Slot8
    reinterpret_cast<void*>(&CreateDragDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CreateDragDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCreateDragDescriptionVtable() {
    RegisterExeAddr(kCreateDragDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CreateDragDescription_Vtable));
}

extern "C" void* CreateDragDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x40, 8);
    if (!p) return nullptr;
    CreateDragDescription_Init(p);
    return p;
}

static void* CreateDragDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CreateDragDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CreateDragDescription, "Description",
              &CreateDragDescription_Factory, &CreateDragDescription_CreateInPlace);

