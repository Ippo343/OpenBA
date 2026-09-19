// code/Game/Component/pasturedescription.cpp
// PastureDescription (entity description block) — pasture parameters.
// Extends Object::External (0x20 base). Vtable 0x140163ff0. Parent "Description".
// Size 0x28. GetVersion=2.
//
// Factory 0x14014f610-area (MemAlloc 0x28 + vtable + defaults). Defaults:
//   enum @ +0x20 = 5  (4-byte store)
// Serialize (vtable +0x28, 0x140128220), stream order:
//   enum @ +0x20   (serializer-encoding vtable +0x98 dispatch, version-gated)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kPastureDescriptionVtable = 0x140163ff0;

static void PastureDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kPastureDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 5;   // enum
}

void PastureDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    // enum +0x20 is VERSION-GATED in the original (0x140128220): only read when
    // Serializer::GetVersion() > 0. Reading it unconditionally over-reads past a
    // version-0 resource -> CompressedStream::Read zero-progress spin.
    if (reinterpret_cast<IO::Serializer*>(serializer)->GetVersion() > 0) {   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<unsigned int*>(s + 0x20), nullptr);   // B.3.4 bridge (enum +0x20)
    }
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* PastureDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kPastureDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t PastureDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("PastureDescription");
    return cached;
}
static int   PastureDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   PastureDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* PastureDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* PastureDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* PastureDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "PastureComponent"); }

static void* g_PastureDescription_Vtable[11] = {
    reinterpret_cast<void*>(&PastureDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&PastureDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&PastureDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&PastureDescription_Stub_Return2),    // +0x18 GetVersion=2
    reinterpret_cast<void*>(&PastureDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&PastureDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&PastureDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&PastureDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&PastureDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&PastureDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&PastureDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterPastureDescriptionVtable() {
    RegisterExeAddr(kPastureDescriptionVtable, reinterpret_cast<uintptr_t>(g_PastureDescription_Vtable));
}
extern "C" void* PastureDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    PastureDescription_Init(p);
    return p;
}
static void* PastureDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    PastureDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(PastureDescription, "Description",
              &PastureDescription_Factory, &PastureDescription_CreateInPlace);
