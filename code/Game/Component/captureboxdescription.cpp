// code/Game/Component/captureboxdescription.cpp
// Clean reimplementation of CaptureBoxDescription (UI element description block).
//
// CaptureBoxDescription (0x78 bytes) — sprite-capture-box UI element descriptor.
// Extends ElementDescription (Object::External base, 0x20). Vtable at 0x14015d4a0.
// Parent type "ElementDescription".
//   +0x68: ExternalPtr (sprite sheet)
//   +0x70: String      (sprite name)
//
// Factory  : MemAlloc 0x78 + ElementDescription ctor + vtable + zero +0x68/+0x70.
// Serialize (vtable +0x28, 0x1400791a0):
//   ElementDescription::Serialize(self, stream)  [base]
//   then: ExternalPtr @ +0x68  +  String @ +0x70
// GetVersion (vtable +0x18, 0x140079210): ElementDescription::GetFileVersion(self) | 0x100.
// CreateElement (vtable +0x40, 0x1400790a0) — builds a CaptureBoxElement.
// CreateInPlace (vtable +0x48, 0x140013df0): ElementDescription ctor + zero fields.
// Dtor (vtable +0x00, 0x140014010): ~String(+0x70) + ElementDescription::~ + free.
//
// Field knowledge harvested from engine/reimpl/captureboxdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K4)

static const uintptr_t kCaptureBoxDescriptionVtable = 0x14015d4a0;

static void CaptureBoxDescription_Init(void* self) {
    fn_ElementDescription_Ctor(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kCaptureBoxDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x68) = 0;   // ExternalPtr (sprite sheet)
    *(uint64_t*)(s + 0x70) = 0;   // String (sprite name)
}

void CaptureBoxDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalPtr_Serialize) {
        return;
    }
    // base — QUALIFIED: this body IS slot 5 of a hand-built ElementDescription table.
    reinterpret_cast<UI::ElementDescription*>(self)->UI::ElementDescription::Serialize(stream);  // B.3.6 bridge
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    fn_ExternalPtr_Serialize(s + 0x68, serializer);          // ExternalPtr +0x68
    reinterpret_cast<System::String*>(s + 0x70)->Serialize(serializer);             // B.3.1 bridge: String      +0x70
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();                                       // B.3.4 bridge
}

void* CaptureBoxDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kCaptureBoxDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    reinterpret_cast<System::String*>(s + 0x70)->~String();            // B.3.1 bridge: String +0x70
    if (fn_ElementDescription_Dtor) fn_ElementDescription_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t CaptureBoxDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("CaptureBoxDescription");
    return cached;
}

// GetVersion (vtable +0x18): ElementDescription::GetFileVersion() | 0x100 — the 0x100
// bit flags a custom serialize format on top of the base ElementDescription version.
static uint32_t CaptureBoxDescription_GetVersion(void* self) {
    // base — QUALIFIED: this body IS slot 3 of a hand-built ElementDescription table.
    uint32_t base = static_cast<uint32_t>(
        reinterpret_cast<const UI::ElementDescription*>(self)->UI::ElementDescription::GetFileVersion());  // B.3.6 bridge
    return base | 0x100u;
}

static int   CaptureBoxDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* CaptureBoxDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* CaptureBoxDescription_CreateInPlace(void* memory);

static void* g_CaptureBoxDescription_Vtable[11] = {
    reinterpret_cast<void*>(&CaptureBoxDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&CaptureBoxDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&CaptureBoxDescription_GetVersion),         // +0x18 GetVersion
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_Return0),   // +0x20 thunk
    reinterpret_cast<void*>(&CaptureBoxDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_Return0),   // +0x38 thunk
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_ReturnNull),// +0x40 CreateElement (not needed for load)
    reinterpret_cast<void*>(&CaptureBoxDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&CaptureBoxDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterCaptureBoxDescriptionVtable() {
    RegisterExeAddr(kCaptureBoxDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_CaptureBoxDescription_Vtable));
}

extern "C" void* CaptureBoxDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x78, 8);
    if (!p) return nullptr;
    CaptureBoxDescription_Init(p);
    return p;
}

static void* CaptureBoxDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    CaptureBoxDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(CaptureBoxDescription, "ElementDescription",
              &CaptureBoxDescription_Factory, &CaptureBoxDescription_CreateInPlace);

