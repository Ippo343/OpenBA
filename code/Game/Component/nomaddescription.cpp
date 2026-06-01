// code/Game/Component/nomaddescription.cpp
// NomadDescription (entity description block) — nomad arrival parameters.
// Extends Object::External (0x20 base). Vtable 0x14015a0b0. Parent "Description".
// Size 0x38. GetVersion=2.
//
// Factory 0x14014b340-area (MemAlloc 0x38). Ctor defaults: ExternalPtr fields
//   @ +0x20, +0x28, +0x30 all = 0.
// Serialize (vtable +0x28, 0x14004ff20), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 = fn_ExternalPtr_BuildSerialize)
//   ExternalPtr @ +0x28  (FUN_140007140 — single-ExternalPtr variant w/ type-check;
//                          stream-identical to fn_ExternalPtr_BuildSerialize)
//   ExternalPtr @ +0x30  (FUN_140007140)
// GetVersion (vtable +0x18, 0x14004ff90): mov eax,2; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kNomadDescriptionVtable = 0x14015a0b0;

static void NomadDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kNomadDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x30) = 0;   // ExternalPtr
}

void NomadDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);     // ExternalPtr +0x20
    fn_ExternalPtr_BuildSerialize(s + 0x28, serializer);     // ExternalPtr +0x28
    fn_ExternalPtr_BuildSerialize(s + 0x30, serializer);     // ExternalPtr +0x30
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* NomadDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kNomadDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t NomadDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("NomadDescription");
    return cached;
}
static int   NomadDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion=2
static int   NomadDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* NomadDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* NomadDescription_CreateInPlace(void* memory);

static void* g_NomadDescription_Vtable[11] = {
    reinterpret_cast<void*>(&NomadDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&NomadDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&NomadDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&NomadDescription_Stub_Return2),     // +0x18 GetVersion=2
    reinterpret_cast<void*>(&NomadDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&NomadDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&NomadDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&NomadDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&NomadDescription_Stub_ReturnNull),  // +0x40
    reinterpret_cast<void*>(&NomadDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&NomadDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterNomadDescriptionVtable() {
    RegisterExeAddr(kNomadDescriptionVtable, reinterpret_cast<uintptr_t>(g_NomadDescription_Vtable));
}
extern "C" void* NomadDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    NomadDescription_Init(p);
    return p;
}
static void* NomadDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    NomadDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(NomadDescription, "Description",
              &NomadDescription_Factory, &NomadDescription_CreateInPlace);
#endif
