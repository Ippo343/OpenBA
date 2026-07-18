// code/Game/Component/boatdescription.cpp
// BoatDescription (entity description block) — trader-boat / vessel parameters.
// Extends Object::External (0x20 base). Vtable 0x140158fd0. Parent "Description".
// Size 0x20 (no own fields). GetVersion=2.
//
// Factory 0x14014a8c0-area (MemAlloc 0x20 + base init + vtable; no field inits).
// Serialize (vtable +0x28, 0x1400469c0): Serializer ctor/dtor only — no fields.
// GetVersion (vtable +0x18, 0x140046a00): mov eax,2; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kBoatDescriptionVtable = 0x140158fd0;

static void BoatDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kBoatDescriptionVtable);
    // no own fields (size 0x20)
}

void BoatDescription_Serialize(void* self, void* stream) {
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    // no fields serialized
    g_fn_Serializer_Dtor(serializer);
}

void* BoatDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kBoatDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t BoatDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("BoatDescription");
    return cached;
}
static int   BoatDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion=2
static int   BoatDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* BoatDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* BoatDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* BoatDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "BoatComponent"); }

static void* g_BoatDescription_Vtable[11] = {
    reinterpret_cast<void*>(&BoatDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&BoatDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&BoatDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&BoatDescription_Stub_Return2),     // +0x18 GetVersion=2
    reinterpret_cast<void*>(&BoatDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&BoatDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&BoatDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&BoatDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&BoatDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&BoatDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&BoatDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterBoatDescriptionVtable() {
    RegisterExeAddr(kBoatDescriptionVtable, reinterpret_cast<uintptr_t>(g_BoatDescription_Vtable));
}
extern "C" void* BoatDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    BoatDescription_Init(p);
    return p;
}
static void* BoatDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    BoatDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(BoatDescription, "Description",
              &BoatDescription_Factory, &BoatDescription_CreateInPlace);
