// code/Game/Component/frustumoutlinedescription.cpp
// Clean reimplementation of FrustumOutlineDescription (UI element description block).
//
// FrustumOutlineDescription (0x80 bytes) — camera-frustum outline UI element descriptor.
// Extends ElementDescription (Object::External base, 0x20). Vtable at 0x14015c300.
// Parent type "ElementDescription".
//   +0x68: ExternalPtr (sprite sheet)
//   +0x70: String      (sprite name)
//   +0x78: int          (default -1)
//   +0x7c: int
//
// Factory  : MemAlloc 0x80 + ElementDescription ctor + vtable + defaults.
// Serialize (vtable +0x28, 0x14006c460):
//   ElementDescription::Serialize(self, stream)  [base]
//   then: ExternalPtr @ +0x68  +  String @ +0x70  +  int @ +0x78  +  int @ +0x7c
// GetVersion (vtable +0x18, 0x14006c500): ElementDescription::GetFileVersion(self) | 0x100.
// CreateInPlace (vtable +0x48, 0x140012330): ctor + zero +0x68/+0x70 + (+0x78 = -1).
// Dtor (vtable +0x00, 0x140012500): ~String(+0x70) + ElementDescription::~ + free.
//
// Field knowledge harvested from engine/reimpl/frustumoutlinedescription.cpp (DLL-mode reimpl).
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kFrustumOutlineDescriptionVtable = 0x14015c300;

static void FrustumOutlineDescription_Init(void* self) {
    fn_ElementDescription_Ctor(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kFrustumOutlineDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x68) = 0;             // ExternalPtr (sprite sheet)
    *(uint64_t*)(s + 0x70) = 0;             // String (sprite name)
    *(int64_t*) (s + 0x78) = (int64_t)-1;   // int +0x78 = -1 (covers +0x7c too)
}

void FrustumOutlineDescription_Serialize(void* self, void* stream) {
#ifdef STANDALONE_EXE
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ElementDescription_Serialize || !g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !fn_ExternalPtr_Serialize || !g_fn_String_Serialize || !g_fn_Serialize_Int) {
        return;
    }
    fn_ElementDescription_Serialize(self, stream);           // base
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    fn_ExternalPtr_Serialize(s + 0x68, serializer);          // ExternalPtr +0x68
    g_fn_String_Serialize(s + 0x70, serializer);             // String      +0x70
    g_fn_Serialize_Int(serializer, s + 0x78, nullptr);       // int         +0x78
    g_fn_Serialize_Int(serializer, s + 0x7c, nullptr);       // int         +0x7c
    g_fn_Serializer_Dtor(serializer);
#else
    (void)self; (void)stream;
#endif
}

void* FrustumOutlineDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kFrustumOutlineDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_String_Dtor) fn_String_Dtor(s + 0x70);            // String +0x70
    if (fn_ElementDescription_Dtor) fn_ElementDescription_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

#ifdef STANDALONE_EXE

#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t FrustumOutlineDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("FrustumOutlineDescription");
    return cached;
}

// GetVersion (vtable +0x18): ElementDescription::GetFileVersion() | 0x100.
static uint32_t FrustumOutlineDescription_GetVersion(void* self) {
    uint32_t base = fn_ElementDescription_GetFileVersion
                        ? fn_ElementDescription_GetFileVersion(self) : 0;
    return base | 0x100u;
}

static int   FrustumOutlineDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* FrustumOutlineDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* FrustumOutlineDescription_CreateInPlace(void* memory);

static void* g_FrustumOutlineDescription_Vtable[11] = {
    reinterpret_cast<void*>(&FrustumOutlineDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&FrustumOutlineDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&FrustumOutlineDescription_GetVersion),         // +0x18 GetVersion
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x20 thunk
    reinterpret_cast<void*>(&FrustumOutlineDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_Return0),   // +0x38 thunk
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_ReturnNull),// +0x40 CreateElement
    reinterpret_cast<void*>(&FrustumOutlineDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&FrustumOutlineDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterFrustumOutlineDescriptionVtable() {
    RegisterExeAddr(kFrustumOutlineDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_FrustumOutlineDescription_Vtable));
}

extern "C" void* FrustumOutlineDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x80, 8);
    if (!p) return nullptr;
    FrustumOutlineDescription_Init(p);
    return p;
}

static void* FrustumOutlineDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    FrustumOutlineDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(FrustumOutlineDescription, "ElementDescription",
              &FrustumOutlineDescription_Factory, &FrustumOutlineDescription_CreateInPlace);

#endif // STANDALONE_EXE
