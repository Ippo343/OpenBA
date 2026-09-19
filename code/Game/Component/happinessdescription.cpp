// code/Game/Component/happinessdescription.cpp
// HappinessDescription (entity description block) — happiness-effect parameters
// (ToolKit: type / radius / idle range). Extends Object::External (0x20 base).
// Vtable 0x140159660. Parent "Description". Size 0x30. GetVersion=3.
//
// Factory 0x14014ad00-area (MemAlloc 0x30). Ctor defaults:
//   int   @ +0x20 = 5     (HappinessType enum)
//   short @ +0x24 = 0     (two bools @ +0x24/+0x25)
//   ptr   @ +0x28 = 0     (+0x28 enum, +0x2c int)
// Serialize (vtable +0x28, 0x14004acb0), stream order:
//   enum @ +0x20  (serializer encoding vtable +0x98)
//   bool @ +0x24
//   bool @ +0x25
//   enum @ +0x28  (serializer encoding vtable +0x98)
//   int  @ +0x2c
// GetVersion (vtable +0x18, 0x14004ad50): mov eax,3; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kHappinessDescriptionVtable = 0x140159660;

static void HappinessDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kHappinessDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*) (s + 0x20) = 5;   // HappinessType enum
    *(uint16_t*)(s + 0x24) = 0;   // two bools
    *(uint64_t*)(s + 0x28) = 0;   // enum + int
}

void HappinessDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Bool || !g_fn_Serialize_Int) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)
        ->Serialize(*reinterpret_cast<unsigned int*>(s + 0x20), nullptr);   // enum +0x20   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x24), nullptr);      // bool +0x24   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<bool*>(s + 0x25), nullptr);      // bool +0x25   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)
        ->Serialize(*reinterpret_cast<unsigned int*>(s + 0x28), nullptr);   // enum +0x28   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(s + 0x2c), nullptr);       // int  +0x2c   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* HappinessDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kHappinessDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HappinessDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HappinessDescription");
    return cached;
}
static int   HappinessDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion=3
static int   HappinessDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* HappinessDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* HappinessDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* HappinessDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "HappinessComponent"); }

static void* g_HappinessDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HappinessDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&HappinessDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&HappinessDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&HappinessDescription_Stub_Return3),     // +0x18 GetVersion=3
    reinterpret_cast<void*>(&HappinessDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&HappinessDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&HappinessDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&HappinessDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&HappinessDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&HappinessDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&HappinessDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterHappinessDescriptionVtable() {
    RegisterExeAddr(kHappinessDescriptionVtable, reinterpret_cast<uintptr_t>(g_HappinessDescription_Vtable));
}
extern "C" void* HappinessDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    HappinessDescription_Init(p);
    return p;
}
static void* HappinessDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    HappinessDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(HappinessDescription, "Description",
              &HappinessDescription_Factory, &HappinessDescription_CreateInPlace);
