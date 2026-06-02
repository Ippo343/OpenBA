// code/Game/Component/huntdescription.cpp
// HuntDescription (entity description block) — hunting lodge parameters.
// Extends Object::External (0x20 base). Vtable 0x140163820. Parent "Description".
// Size 0x28. GetVersion=3 (slot +0x18 @ 0x1401229e0: mov eax,3; ret).
//
// Ctor defaults (HuntDescription_Factory):
//   float    @ +0x20 = 0   (not written by factory; left zero from base)
//   int/enum @ +0x24 = 5
// Serialize (vtable +0x28, 0x140122960) — Serializer ctor with version=1, stream order:
//   float @ +0x20
//   if (serializer.GetVersion() > 0): enum @ +0x24 (encoding vcall +0x98)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kHuntDescriptionVtable = 0x140163820;

static void HuntDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kHuntDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(float*)  (s + 0x20) = 0.0f;
    *(int32_t*)(s + 0x24) = 5;
}

void HuntDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serializer_GetVersion || !fn_Serializer_Serialize_Float) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 1);
    fn_Serializer_Serialize_Float(serializer, (float*)(s + 0x20), nullptr);
    if (g_fn_Serializer_GetVersion(serializer) > 0)
        SerializerEncodingVCall(serializer, 0x98, s + 0x24);  // enum +0x24
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* HuntDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kHuntDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t HuntDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("HuntDescription");
    return cached;
}
static int   HuntDescription_Stub_Return3(void* /*self*/) { return 3; } // GetVersion
static int   HuntDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* HuntDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* HuntDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* HuntDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "HuntComponent"); }

static void* g_HuntDescription_Vtable[11] = {
    reinterpret_cast<void*>(&HuntDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&HuntDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&HuntDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&HuntDescription_Stub_Return3),    // +0x18 GetVersion=3
    reinterpret_cast<void*>(&HuntDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&HuntDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&HuntDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&HuntDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&HuntDescription_GetComponentTypeId), // +0x40
    reinterpret_cast<void*>(&HuntDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&HuntDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterHuntDescriptionVtable() {
    RegisterExeAddr(kHuntDescriptionVtable, reinterpret_cast<uintptr_t>(g_HuntDescription_Vtable));
}
extern "C" void* HuntDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x28, 8);
    if (!p) return nullptr;
    HuntDescription_Init(p);
    return p;
}
static void* HuntDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    HuntDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(HuntDescription, "Description",
              &HuntDescription_Factory, &HuntDescription_CreateInPlace);
#endif
