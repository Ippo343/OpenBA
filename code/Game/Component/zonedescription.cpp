// code/Game/Component/zonedescription.cpp
// Clean reimplementation of ZoneDescription (entity description block).
//
// ZoneDescription (0x20 bytes) — marker description for a zone-type building.
// Extends Object::External (0x20 base). Vtable at 0x140164eb0.
// Parent type "Description".
//
// Factory  : 0x14003a700  (MemAlloc 0x20 + ctor = ZoneComponent_vslot16 init)
// CreateInPlace: 0x14003a760
// Ctor body: just External::External + vtable (no extra fields).
// Serialize (vtable +0x28, 0x140130430): EMPTY — only constructs/destroys the
//   stack Serializer. Zero stream bytes (pure type marker).
// GetVersion (vtable +0x18, 0x140130470): mov eax,1; ret  -> file version 1.
// Dtor (vtable +0x00, 0x14003a810): vtable reset + External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kZoneDescriptionVtable = 0x140164eb0;

// ============================================================
// ZoneDescription_Init — base External init + vtable (no extra fields)
// ============================================================
static void ZoneDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kZoneDescriptionVtable);
    *(void**)self = s_vtable;
}

// ============================================================
// ZoneDescription_Serialize (vtable +0x28, 0x140130430)
// ============================================================
// The original constructs a stack Serializer and immediately destroys it —
// no fields are read. We mirror that (a true no-op on the stream).
void ZoneDescription_Serialize(void* self, void* stream) {
    (void)self;
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// ZoneDescription_Dtor (vtable +0x00, 0x14003a810)
// ============================================================
void* ZoneDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kZoneDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    // No owned String/array fields — go straight to base External dtor.
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t ZoneDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("ZoneDescription");
    return cached;
}

static int   ZoneDescription_VtblStub_Return1(void* /*self*/)    { return 1; }  // GetVersion
static int   ZoneDescription_VtblStub_Return0(void* /*self*/)    { return 0; }
static void* ZoneDescription_VtblStub_ReturnNull(void*, ...)     { return nullptr; }

static void* ZoneDescription_CreateInPlace(void* memory);

// Vtable @ 0x140164eb0 (Object::External 9-slot shape):
//   +0x00 Dtor        -> ZoneDescription_Dtor
//   +0x08 GetTypeInfo -> ReturnNull stub
//   +0x10 GetTypeId   -> ZoneDescription_GetTypeId
//   +0x18 GetVersion  -> Return1 (mov eax,1)
//   +0x20 Compile     -> Return0 (shared External::Compile thunk; unused on load)
//   +0x28 Serialize   -> ZoneDescription_Serialize
//   +0x30 Cleanup     -> Return0 (unused on load)
//   +0x38 GetDesc     -> Return0 (shared base forwarder; unused on load)
//   +0x40 Slot8       -> ReturnNull (unused on load)
//   +0x48 CreateInPlace-> ZoneDescription_CreateInPlace
// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* ZoneDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "ZoneComponent"); }

static void* g_ZoneDescription_Vtable[11] = {
    reinterpret_cast<void*>(&ZoneDescription_Dtor),               // +0x00
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_ReturnNull),// +0x08
    reinterpret_cast<void*>(&ZoneDescription_GetTypeId),          // +0x10
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_Return0),   // +0x20
    reinterpret_cast<void*>(&ZoneDescription_Serialize),          // +0x28
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_Return0),   // +0x30
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_Return0),   // +0x38
    reinterpret_cast<void*>(&ZoneDescription_GetComponentTypeId),// +0x40
    reinterpret_cast<void*>(&ZoneDescription_CreateInPlace),      // +0x48
    reinterpret_cast<void*>(&ZoneDescription_VtblStub_ReturnNull),// +0x50 (over-read guard)
};

extern "C" void RegisterZoneDescriptionVtable() {
    RegisterExeAddr(kZoneDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_ZoneDescription_Vtable));
}

extern "C" void* ZoneDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    ZoneDescription_Init(p);
    return p;
}

static void* ZoneDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    ZoneDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(ZoneDescription, "Description",
              &ZoneDescription_Factory, &ZoneDescription_CreateInPlace);

