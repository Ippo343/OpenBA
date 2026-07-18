// code/Game/Component/autopickupdescription.cpp
// AutoPickupDescription (entity description block) — auto-pickup parameters.
// Extends Object::External (0x20 base). Vtable 0x140158e70. Parent "Description".
// Size 0x28. GetVersion=1.
//
// Factory 0x14014a840-area (MemAlloc 0x28 + vtable + defaults). Defaults:
//   bool @ +0x20 = 1
// Serialize (vtable +0x28, 0x140044b60), stream order:
//   bool @ +0x20
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kAutoPickupDescriptionVtable = 0x140158e70;

static void AutoPickupDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kAutoPickupDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint8_t*)(s + 0x20) = 1;   // bool
}

void AutoPickupDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !g_fn_Serialize_Bool) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serialize_Bool(serializer, s + 0x20, nullptr);   // bool +0x20
    g_fn_Serializer_Dtor(serializer);
}

void* AutoPickupDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kAutoPickupDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t AutoPickupDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("AutoPickupDescription");
    return cached;
}
static int   AutoPickupDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   AutoPickupDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* AutoPickupDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* AutoPickupDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* AutoPickupDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "AutoPickupComponent"); }

static void* g_AutoPickupDescription_Vtable[11] = {
    reinterpret_cast<void*>(&AutoPickupDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&AutoPickupDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&AutoPickupDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&AutoPickupDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&AutoPickupDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&AutoPickupDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterAutoPickupDescriptionVtable() {
    RegisterExeAddr(kAutoPickupDescriptionVtable, reinterpret_cast<uintptr_t>(g_AutoPickupDescription_Vtable));
}
extern "C" void* AutoPickupDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    AutoPickupDescription_Init(p);
    return p;
}
static void* AutoPickupDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    AutoPickupDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(AutoPickupDescription, "Description",
              &AutoPickupDescription_Factory, &AutoPickupDescription_CreateInPlace);
