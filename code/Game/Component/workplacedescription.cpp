// code/Game/Component/workplacedescription.cpp
// Clean reimplementation of WorkPlaceDescription (entity description block).
//
// WorkPlaceDescription (0x30 bytes) — workplace job slots (two Profession refs).
// Extends Object::External (0x20 base). Vtable at 0x140164dc0.
// Parent type "Description".
//
// Factory  : 0x14003a090  (MemAlloc 0x30 + ctor)  fields +0x20=0, +0x28=0
// CreateInPlace: 0x14003a100
// Serialize (vtable +0x28, 0x14012f420):
//   ExternalPtr<Profession> @ +0x20   (FUN_140018020, typeId TypeInfo_Profession)
//   ExternalPtr<Profession> @ +0x28   (FUN_140018020)
//   Both are 8-byte resolved External* fields stored in place.
// GetVersion (vtable +0x18, 0x14012f480): mov eax,2; ret -> file version 2.
// Dtor (vtable +0x00, 0x14003a1c0): vtable reset + External::~External + free
//   (the two +0x20/+0x28 fields are weak External* refs, not owned — no dtor).
//
// STANDALONE: the exe's per-field ExternalPtr serialize (FUN_140018020) is not
// mapped. We reuse the proven generic ExternalPtr serialize reimpl
// (Reimpl_ExternalPtr_Serialize_StringTable in code/Game/Game/serialize_externals.cpp).
// The typeId only drives an IsDerivedFrom validation that
// Object::IsDerivedFromOrTrust makes a no-op in standalone, so the body is
// stream-identical regardless of the (Profession vs StringTable) typeId source.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>

// Generic single-ExternalPtr serialize (exported from serialize_externals.cpp).
extern "C" void Reimpl_ExternalPtr_Serialize_StringTable(void* refField, void* serializer);

static const uintptr_t kWorkPlaceDescriptionVtable = 0x140164dc0;

static void WorkPlaceDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kWorkPlaceDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;  // ExternalPtr<Profession> (resolved External*)
    *(uint64_t*)(s + 0x28) = 0;  // ExternalPtr<Profession>
}


// ============================================================
// WorkPlaceDescription_Serialize (vtable +0x28, 0x14012f420)
// ============================================================
void WorkPlaceDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x20, serializer);  // Profession ref
    Reimpl_ExternalPtr_Serialize_StringTable(s + 0x28, serializer);  // Profession ref


    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// WorkPlaceDescription_Dtor (vtable +0x00, 0x14003a1c0)
// ============================================================
void* WorkPlaceDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kWorkPlaceDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t WorkPlaceDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WorkPlaceDescription");
    return cached;
}

static int   WorkPlaceDescription_VtblStub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   WorkPlaceDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* WorkPlaceDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* WorkPlaceDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* WorkPlaceDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "WorkPlaceComponent"); }

static void* g_WorkPlaceDescription_Vtable[11] = {
    reinterpret_cast<void*>(&WorkPlaceDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&WorkPlaceDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return2),   // +0x18 GetVersion
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&WorkPlaceDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&WorkPlaceDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&WorkPlaceDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&WorkPlaceDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterWorkPlaceDescriptionVtable() {
    RegisterExeAddr(kWorkPlaceDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_WorkPlaceDescription_Vtable));
}

extern "C" void* WorkPlaceDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    WorkPlaceDescription_Init(p);
    return p;
}

static void* WorkPlaceDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    WorkPlaceDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(WorkPlaceDescription, "Description",
              &WorkPlaceDescription_Factory, &WorkPlaceDescription_CreateInPlace);

