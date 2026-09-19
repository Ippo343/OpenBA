// code/Game/Component/welldescription.cpp
// WellDescription (entity description block) — well marker.
// Extends Object::External (0x20 base). Vtable 0x14015af20. Parent "Description".
// Size 0x20 (NO fields beyond the External base). GetVersion=1.
//
// Factory 0x14014bb80-area (MemAlloc 0x20 + vtable only, no defaults).
// Serialize (vtable +0x28, 0x14005d170): Serializer ctor/dtor only, no payload.
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kWellDescriptionVtable = 0x14015af20;

static void WellDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kWellDescriptionVtable);
}

void WellDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // No payload fields.
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* WellDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kWellDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t WellDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("WellDescription");
    return cached;
}
static int   WellDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   WellDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* WellDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* WellDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* WellDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "WellComponent"); }

static void* g_WellDescription_Vtable[11] = {
    reinterpret_cast<void*>(&WellDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&WellDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&WellDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&WellDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&WellDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&WellDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&WellDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&WellDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&WellDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&WellDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&WellDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterWellDescriptionVtable() {
    RegisterExeAddr(kWellDescriptionVtable, reinterpret_cast<uintptr_t>(g_WellDescription_Vtable));
}
extern "C" void* WellDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    WellDescription_Init(p);
    return p;
}
static void* WellDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    WellDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(WellDescription, "Description",
              &WellDescription_Factory, &WellDescription_CreateInPlace);
