// code/Game/Component/tornadodescription.cpp
// TornadoDescription (entity description block) — tornado disaster parameters.
// Extends Object::External (0x20 base). Vtable 0x14015ab00. Parent "Description".
// Size 0x38. GetVersion=1.
//
// Factory 0x14014b900-area (MemAlloc 0x38 + ctor sub_8e80). Ctor defaults:
//   float @ +0x20 = 1.0 (0x3f800000),  int @ +0x24 = 10
//   Time  @ +0x28 = 0   (8 bytes)
//   int   @ +0x30 = 1,  int @ +0x34 = 2
// Serialize (vtable +0x28, 0x140058d90), stream order (note: Time @ +0x28 is
// written AFTER the +0x30 int):
//   float @ +0x20
//   int   @ +0x24
//   int   @ +0x30
//   Time  @ +0x28   (System::Time::Serialize, 8 bytes)
//   int   @ +0x34
// GetVersion (vtable +0x18, 0x140058e30): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch

static const uintptr_t kTornadoDescriptionVtable = 0x14015ab00;

static void TornadoDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kTornadoDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)   (s + 0x20) = 1.0f;
    *(int32_t*) (s + 0x24) = 10;
    *(uint64_t*)(s + 0x28) = 0;     // Time (8 bytes)
    *(int32_t*) (s + 0x30) = 1;
    *(int32_t*) (s + 0x34) = 2;
}

void TornadoDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<float*>(s + 0x20), nullptr);  // float +0x20   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x24), nullptr);                       // int   +0x24   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x30), nullptr);                       // int   +0x30   // B.3.4 bridge
    reinterpret_cast<System::Time*>(s + 0x28)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));                               // Time  +0x28   // B.3.4b bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x34), nullptr);                       // int   +0x34   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* TornadoDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kTornadoDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TornadoDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TornadoDescription");
    return cached;
}
static int   TornadoDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion=1
static int   TornadoDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* TornadoDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* TornadoDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* TornadoDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "TornadoComponent"); }

static void* g_TornadoDescription_Vtable[11] = {
    reinterpret_cast<void*>(&TornadoDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&TornadoDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&TornadoDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&TornadoDescription_Stub_Return1),     // +0x18 GetVersion=1
    reinterpret_cast<void*>(&TornadoDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&TornadoDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&TornadoDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&TornadoDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&TornadoDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&TornadoDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&TornadoDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterTornadoDescriptionVtable() {
    RegisterExeAddr(kTornadoDescriptionVtable, reinterpret_cast<uintptr_t>(g_TornadoDescription_Vtable));
}
extern "C" void* TornadoDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    TornadoDescription_Init(p);
    return p;
}
static void* TornadoDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    TornadoDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(TornadoDescription, "Description",
              &TornadoDescription_Factory, &TornadoDescription_CreateInPlace);
