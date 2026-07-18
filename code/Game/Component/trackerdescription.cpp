// code/Game/Component/trackerdescription.cpp
// Clean reimplementation of TrackerDescription (entity description block).
//
// TrackerDescription (0x20 bytes) — trade/route tracker. No extra fields.
// Extends Object::External (0x20 base). Vtable at 0x14015ac00. Parent "Description".
//
// Factory (TrackerDescription_Factory): MemAlloc 0x20 + External ctor + vtable.
// Serialize (vtable +0x28, 0x1400593c0): Serializer ctor/dtor only — returns 1.
// GetVersion (vtable +0x18, 0x140059400): mov eax,1; ret -> version 1.
// Dtor (vtable +0x00, 0x14000d940): External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kTrackerDescriptionVtable = 0x14015ac00;

static void TrackerDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kTrackerDescriptionVtable);
    *(void**)self = s_vtable;
}

void TrackerDescription_Serialize(void* self, void* stream) {
    (void)self;
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor) return;
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    g_fn_Serializer_Dtor(serializer);
}

void* TrackerDescription_Dtor(void* self, uint32_t flags) {
    uintptr_t vt = ResolveAddr(Module::Exe, kTrackerDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t TrackerDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("TrackerDescription");
    return cached;
}

static int   TrackerDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   TrackerDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* TrackerDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* TrackerDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* TrackerDescription_GetComponentTypeId(void*, void* out) { return ::Object::WriteComponentTypeId(out, "TrackerComponent"); }

static void* g_TrackerDescription_Vtable[11] = {
    reinterpret_cast<void*>(&TrackerDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&TrackerDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&TrackerDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&TrackerDescription_GetComponentTypeId),// +0x40 Slot8
    reinterpret_cast<void*>(&TrackerDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&TrackerDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterTrackerDescriptionVtable() {
    RegisterExeAddr(kTrackerDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_TrackerDescription_Vtable));
}

extern "C" void* TrackerDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x20, 8);
    if (!p) return nullptr;
    TrackerDescription_Init(p);
    return p;
}

static void* TrackerDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    TrackerDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(TrackerDescription, "Description",
              &TrackerDescription_Factory, &TrackerDescription_CreateInPlace);

