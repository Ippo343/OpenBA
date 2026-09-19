// code/Game/Component/workdescription.cpp
// Clean reimplementation of WorkDescription (entity description block).
//
// WorkDescription (0x38 bytes) — work-cost parameters for a job.
// Extends Object::External (0x20 base). Vtable at 0x140164c60.
// Parent type "Description".  (ToolKit: WorkDescription = work cost.)
//
// Factory  : 0x140039ed0  (MemAlloc 0x38 + ctor sub_e9b0 0x14012e9b0)
// CreateInPlace: 0x140039f10
// Ctor defaults (from WorkDescription_sub_e9b0):
//   bool @ +0x20 = 0
//   int  @ +0x24 = 0
//   int  @ +0x28 = 1
//   int  @ +0x2c = 100
//   int  @ +0x30 = 1
//   bool @ +0x34 = 0
// Serialize (vtable +0x28, 0x14012e8b0) — NOTE the non-monotonic field order:
//   int  @ +0x24
//   int  @ +0x28
//   int  @ +0x2c
//   bool @ +0x34
//   int  @ +0x30
//   bool @ +0x20
// GetVersion (vtable +0x18, 0x14012e960): mov eax,4; ret -> file version 4.
// Dtor (vtable +0x00, 0x140039f50): vtable reset + External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kWorkDescriptionVtable = 0x140164c60;

static void WorkDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kWorkDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint8_t*)(s + 0x20) = 0;   // bool
    *(int32_t*)(s + 0x24) = 0;   // int
    *(int32_t*)(s + 0x28) = 1;   // int
    *(int32_t*)(s + 0x2c) = 100; // int
    *(int32_t*)(s + 0x30) = 1;   // int
    *(uint8_t*)(s + 0x34) = 0;   // bool
}

// ============================================================
// WorkDescription_Serialize (vtable +0x28, 0x14012e8b0)
// ============================================================
void WorkDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Int || !g_fn_Serialize_Bool) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x28), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x34), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x20), nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// WorkDescription_Dtor (vtable +0x00, 0x140039f50)
// ============================================================
void* WorkDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kWorkDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t WorkDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WorkDescription");
    return cached;
}

static int   WorkDescription_VtblStub_Return4(void* /*self*/) { return 4; } // GetVersion
static int   WorkDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* WorkDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* WorkDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* WorkDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "WorkComponent"); }

static void* g_WorkDescription_Vtable[11] = {
    reinterpret_cast<void*>(&WorkDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&WorkDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&WorkDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&WorkDescription_VtblStub_Return4),   // +0x18 GetVersion
    reinterpret_cast<void*>(&WorkDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&WorkDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&WorkDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&WorkDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&WorkDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&WorkDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&WorkDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterWorkDescriptionVtable() {
    RegisterExeAddr(kWorkDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_WorkDescription_Vtable));
}

extern "C" void* WorkDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    WorkDescription_Init(p);
    return p;
}

static void* WorkDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    WorkDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(WorkDescription, "Description",
              &WorkDescription_Factory, &WorkDescription_CreateInPlace);

