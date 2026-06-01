// code/Game/Component/foresterdescription.cpp
// ForesterDescription (entity description block) — forester lodge parameters.
// Extends Object::External (0x20 base). Vtable 0x140163510. Parent "Description".
// Size 0x30. GetVersion=2 (slot +0x18 @ 0x14011fb00: mov eax,2; ret).
//
// Ctor defaults (ForesterDescription_Factory):
//   ExternalPtr @ +0x20 = 0   (tree/sapling material ref)
//   int/enum    @ +0x28 = 0
// Serialize (vtable +0x28, 0x14011fa80) — Serializer ctor with version=1, stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 -> fn_ExternalPtr_BuildSerialize)
//   if (serializer.GetVersion() > 0): enum @ +0x28 (encoding vcall +0x98)
// Decompiled READ-ONLY from Application-x64.exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kForesterDescriptionVtable = 0x140163510;

static void ForesterDescription_Init(void* self) {
    fn_External_External(self);
    *(void**)self = (void*)ResolveAddr(Module::Exe, kForesterDescriptionVtable);
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(int32_t*) (s + 0x28) = 0;
}

void ForesterDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serializer_GetVersion || !fn_ExternalPtr_BuildSerialize) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 1);
    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20
    if (g_fn_Serializer_GetVersion(serializer) > 0)
        SerializerEncodingVCall(serializer, 0x98, s + 0x28); // enum +0x28
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* ForesterDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kForesterDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ForesterDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ForesterDescription");
    return cached;
}
static int   ForesterDescription_Stub_Return2(void* /*self*/) { return 2; } // GetVersion
static int   ForesterDescription_Stub_Return0(void* /*self*/) { return 0; }
static void* ForesterDescription_Stub_ReturnNull(void*, ...)  { return nullptr; }
static void* ForesterDescription_CreateInPlace(void* memory);

static void* g_ForesterDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ForesterDescription_Dtor),            // +0x00
    reinterpret_cast<void*>(&ForesterDescription_Stub_ReturnNull), // +0x08
    reinterpret_cast<void*>(&ForesterDescription_GetTypeId),       // +0x10
    reinterpret_cast<void*>(&ForesterDescription_Stub_Return2),    // +0x18 GetVersion=2
    reinterpret_cast<void*>(&ForesterDescription_Stub_Return0),    // +0x20 Compile
    reinterpret_cast<void*>(&ForesterDescription_Serialize),       // +0x28 Serialize
    reinterpret_cast<void*>(&ForesterDescription_Stub_Return0),    // +0x30
    reinterpret_cast<void*>(&ForesterDescription_Stub_Return0),    // +0x38
    reinterpret_cast<void*>(&ForesterDescription_Stub_ReturnNull), // +0x40
    reinterpret_cast<void*>(&ForesterDescription_CreateInPlace),   // +0x48 CreateInPlace
    reinterpret_cast<void*>(&ForesterDescription_Stub_ReturnNull), // +0x50 guard
};

extern "C" void RegisterForesterDescriptionVtable() {
    RegisterExeAddr(kForesterDescriptionVtable, reinterpret_cast<uintptr_t>(g_ForesterDescription_Vtable));
}
extern "C" void* ForesterDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (!p) return nullptr;
    ForesterDescription_Init(p);
    return p;
}
static void* ForesterDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ForesterDescription_Init(memory);
    return memory;
}
REGISTER_TYPE(ForesterDescription, "Description",
              &ForesterDescription_Factory, &ForesterDescription_CreateInPlace);
#endif
