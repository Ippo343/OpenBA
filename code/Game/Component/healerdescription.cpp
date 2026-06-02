// code/Game/Component/healerdescription.cpp
// HealerDescription (entity description block) — healer/herbalist building parameters.
// Extends Object::External (0x20 base). Vtable 0x1401597c0. Parent "Description".
// Size 0x20 (no own fields). GetVersion=1.
//
// Factory 0x14014ae00-area (MemAlloc 0x20 + base init + vtable; no field inits).
// Serialize (vtable +0x28, 0x14004b090): Serializer ctor/dtor only — no fields.
// GetVersion (vtable +0x18, 0x14004b0d0): mov eax,1; ret.
// Decompiled READ-ONLY from Application-x64.exe (Foundation C wave 2).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kHealerDescriptionVtable = 0x1401597c0;

static void HealerDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kHealerDescriptionVtable);
    // no own fields (size 0x20)
}

void HealerDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    // no fields serialized
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* HealerDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kHealerDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HealerDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HealerDescription");
    return cached;
}
static int   HealerDescription_Stub_Return1(void* /*self*/) { return 1; } // GetVersion=1
static int   HealerDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* HealerDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* HealerDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* HealerDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "HealerComponent"); }

static void* g_HealerDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HealerDescription_Dtor),             // +0x00
    reinterpret_cast<void*>(&HealerDescription_Stub_ReturnNull),  // +0x08
    reinterpret_cast<void*>(&HealerDescription_GetTypeId),        // +0x10
    reinterpret_cast<void*>(&HealerDescription_Stub_Return1),     // +0x18 GetVersion=1
    reinterpret_cast<void*>(&HealerDescription_Stub_Return0),     // +0x20 Compile
    reinterpret_cast<void*>(&HealerDescription_Serialize),        // +0x28 Serialize
    reinterpret_cast<void*>(&HealerDescription_Stub_Return0),     // +0x30
    reinterpret_cast<void*>(&HealerDescription_Stub_Return0),     // +0x38
    reinterpret_cast<void*>(&HealerDescription_GetComponentTypeId),  // +0x40
    reinterpret_cast<void*>(&HealerDescription_CreateInPlace),    // +0x48 CreateInPlace
    reinterpret_cast<void*>(&HealerDescription_Stub_ReturnNull),  // +0x50 guard
};

extern "C" void RegisterHealerDescriptionVtable() {
    RegisterExeAddr(kHealerDescriptionVtable, reinterpret_cast<uintptr_t>(g_HealerDescription_Vtable));
}
extern "C" void* HealerDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    HealerDescription_Init(p);
    return p;
}
static void* HealerDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    HealerDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(HealerDescription, "Description",
              &HealerDescription_Factory, &HealerDescription_CreateInPlace);
#endif
