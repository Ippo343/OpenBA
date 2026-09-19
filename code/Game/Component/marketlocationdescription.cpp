// code/Game/Component/marketlocationdescription.cpp
// MarketLocationDescription (entity description block) — market location marker.
// Extends Object::External (0x20 base). Vtable 0x140159e60. Parent "Description".
// Size 0x20 (no fields beyond the External base).
// GetVersion=1 (slot +0x18 @ 0x14004f1b0: mov eax,1; ret).
//
// Factory @ 0x14014b180-area: MemAlloc 0x20 + External base + vtable, no extra fields.
// Serialize (vtable +0x28, 0x14004f170): Serializer ctor + dtor only (no stream fields).
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kMarketLocationDescriptionVtable = 0x140159e60;

static void MarketLocationDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kMarketLocationDescriptionVtable);
}

void MarketLocationDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // No stream fields.
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* MarketLocationDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kMarketLocationDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t MarketLocationDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MarketLocationDescription");
    return cached;
}
static int   MarketLocationDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   MarketLocationDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* MarketLocationDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* MarketLocationDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* MarketLocationDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "MarketLocationComponent"); }

static void* g_MarketLocationDescription_Vtable[11] = {
    reinterpret_cast<void*>(&MarketLocationDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&MarketLocationDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_Return1),    // +0x18 GetVersion=1
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&MarketLocationDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&MarketLocationDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&MarketLocationDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&MarketLocationDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterMarketLocationDescriptionVtable() {
    RegisterExeAddr(kMarketLocationDescriptionVtable, reinterpret_cast<uintptr_t>(g_MarketLocationDescription_Vtable));
}
extern "C" void* MarketLocationDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    MarketLocationDescription_Init(p);
    return p;
}
static void* MarketLocationDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MarketLocationDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(MarketLocationDescription, "Description",
              &MarketLocationDescription_Factory, &MarketLocationDescription_CreateInPlace);
