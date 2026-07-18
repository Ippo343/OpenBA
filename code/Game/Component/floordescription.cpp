// code/Game/Component/floordescription.cpp
// FloorDescription (entity description block) — floor parameters.
// Extends Object::External (0x20 base). Vtable 0x1401633e0. Parent "Description".
// Size 0x30. GetVersion=1.
//
// Factory 0x14014f010-area (MemAlloc 0x30 + vtable + defaults). Defaults:
//   (8)  @ +0x20 = 0   (ExternalPtr, null)
//   bool @ +0x28 = 0
// Serialize (vtable +0x28, 0x14011f680), stream order:
//   ExternalPtr @ +0x20  (exe helper FUN_14000b7b0 — the reference-mode
//     External-pointer serialize; distinct from fn_ExternalPtr_BuildSerialize)
//   bool @ +0x28
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kFloorDescriptionVtable = 0x1401633e0;
// Exe-internal ExternalPtr (reference-mode) serialize helper. NEEDS standalone
// address registration (see report). Resolved at runtime via ResolveAddr.
static const uintptr_t kExtPtrRefSerialize = 0x14000b7b0;
typedef void (*ExtPtrRefSerialize_t)(void* field, void* serializer);

static void FloorDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kFloorDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr (null)
    *(uint8_t*) (s + 0x28) = 0;   // bool
}

void FloorDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Bool) return;
    ExtPtrRefSerialize_t extSer =
        (ExtPtrRefSerialize_t)ResolveAddr(Module::Exe, kExtPtrRefSerialize);
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    if (extSer) extSer(s + 0x20, serializer);             // ExternalPtr +0x20
    g_fn_Serialize_Bool(serializer, s + 0x28, nullptr);   // bool +0x28
    g_fn_Serializer_Dtor(serializer);
}

void* FloorDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kFloorDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FloorDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FloorDescription");
    return cached;
}
static int   FloorDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   FloorDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* FloorDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* FloorDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* FloorDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "FloorComponent"); }

static void* g_FloorDescription_Vtable[11] = {
    reinterpret_cast<void*>(&FloorDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&FloorDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&FloorDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&FloorDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&FloorDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&FloorDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&FloorDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&FloorDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&FloorDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&FloorDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&FloorDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterFloorDescriptionVtable() {
    RegisterExeAddr(kFloorDescriptionVtable, reinterpret_cast<uintptr_t>(g_FloorDescription_Vtable));
}
extern "C" void* FloorDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    FloorDescription_Init(p);
    return p;
}
static void* FloorDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    FloorDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(FloorDescription, "Description",
              &FloorDescription_Factory, &FloorDescription_CreateInPlace);
