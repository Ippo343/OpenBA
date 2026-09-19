// code/Game/Component/wildanimaldescription.cpp
// WildAnimalDescription (entity description block) — wild-animal marker.
// Extends Object::External (0x20 base). Vtable 0x140164b70. Parent "Description".
// Size 0x20 (NO fields beyond the External base). GetVersion=1.
//
// Factory 0x14014fdd0-area (MemAlloc 0x20 + vtable only, no defaults).
// Serialize (vtable +0x28, 0x14012e440): constructs/destructs a Serializer but
//   serializes nothing (no payload). Returns 1.
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kWildAnimalDescriptionVtable = 0x140164b70;

static void WildAnimalDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kWildAnimalDescriptionVtable);
}

void WildAnimalDescription_Serialize(void* self, void* stream) {
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // No payload fields.
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

void* WildAnimalDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kWildAnimalDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t WildAnimalDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WildAnimalDescription");
    return cached;
}
static int   WildAnimalDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   WildAnimalDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* WildAnimalDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* WildAnimalDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* WildAnimalDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "WildAnimalComponent"); }

static void* g_WildAnimalDescription_Vtable[11] = {
    reinterpret_cast<void*>(&WildAnimalDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&WildAnimalDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&WildAnimalDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&WildAnimalDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&WildAnimalDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&WildAnimalDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterWildAnimalDescriptionVtable() {
    RegisterExeAddr(kWildAnimalDescriptionVtable, reinterpret_cast<uintptr_t>(g_WildAnimalDescription_Vtable));
}
extern "C" void* WildAnimalDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    WildAnimalDescription_Init(p);
    return p;
}
static void* WildAnimalDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    WildAnimalDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(WildAnimalDescription, "Description",
              &WildAnimalDescription_Factory, &WildAnimalDescription_CreateInPlace);
