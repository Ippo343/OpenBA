// code/Game/Component/mapdescription.cpp
// Clean reimplementation of MapDescription (entity description block).
//
// MapDescription (0x38 bytes) — minimap / map-overlay parameters for a building.
// Extends Object::External (0x20 base). Vtable at 0x140163b20.
// Parent type "Description".
//
// Factory  : 0x140035760  (MemAlloc 0x38 + ctor sub_4670 0x140124670)
// CreateInPlace: 0x1400357a0
// Ctor defaults (from MapDescription_sub_4670):
//   int    @ +0x20 = 2          (enum: overlay type)
//   String @ +0x28 = 0          (empty)
//   short  @ +0x30 = 0
//   byte   @ +0x32 = 0
//   int    @ +0x34 = 0xff000000 (ARGB Color, opaque black)
// Serialize (vtable +0x28, 0x140124560), stream order:
//   enum   @ +0x20   (serializer-encoding vtable +0x98 dispatch)
//   String @ +0x28
//   bool   @ +0x30
//   bool   @ +0x31
//   bool   @ +0x32
//   Color  @ +0x34   (System::Color::Serialize, 4-byte ARGB)
// GetVersion (vtable +0x18, 0x140124610): mov eax,1; ret -> file version 1.
// Dtor (vtable +0x00, 0x1400357e0): vtable reset + String::~String(+0x28) +
//   External::~External + free.
//
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "system/String.h"
#include "resolve.h"
#include <cstdint>

static const uintptr_t kMapDescriptionVtable = 0x140163b20;

static void MapDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kMapDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(int32_t*)(s + 0x20) = 2;            // enum (overlay type)
    *(uint64_t*)(s + 0x28) = 0;           // String (empty)
    *(uint16_t*)(s + 0x30) = 0;           // bool/bool (two bytes)
    *(uint8_t*) (s + 0x32) = 0;           // bool
    *(uint32_t*)(s + 0x34) = 0xff000000u; // Color (ARGB)
}

// ============================================================
// MapDescription_Serialize (vtable +0x28, 0x140124560)
// ============================================================
void MapDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!g_fn_Serializer_Ctor || !g_fn_Serializer_Dtor ||
        !g_fn_Serialize_Bool || !fn_System_Color_Serialize) {
        return;
    }
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);
    // enum @ +0x20 via the serializer's encoding vtable +0x98 dispatch
    SerializerEncodingVCall(serializer, 0x98, s + 0x20);
    reinterpret_cast<System::String*>(s + 0x28)->Serialize(serializer);             // B.3.1 bridge: String  +0x28
    g_fn_Serialize_Bool(serializer, s + 0x30, nullptr);      // bool    +0x30
    g_fn_Serialize_Bool(serializer, s + 0x31, nullptr);      // bool    +0x31
    g_fn_Serialize_Bool(serializer, s + 0x32, nullptr);      // bool    +0x32
    fn_System_Color_Serialize(s + 0x34, serializer);         // Color   +0x34
    g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// MapDescription_Dtor (vtable +0x00, 0x1400357e0)
// ============================================================
void* MapDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kMapDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    reinterpret_cast<System::String*>(s + 0x28)->~String();   // B.3.1 bridge: String +0x28
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t MapDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("MapDescription");
    return cached;
}

static int   MapDescription_VtblStub_Return1(void* /*self*/) { return 1; } // GetVersion
static int   MapDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* MapDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* MapDescription_CreateInPlace(void* memory);

// +0x40 GetComponentTypeId — CopyCtor reads this to know which component to make.
static void* MapDescription_GetComponentTypeId(void*, void* out) {
    return ::Object::WriteComponentTypeId(out, "MapComponent");
}

static void* g_MapDescription_Vtable[11] = {
    reinterpret_cast<void*>(&MapDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&MapDescription_VtblStub_ReturnNull),// +0x08 GetTypeInfo
    reinterpret_cast<void*>(&MapDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&MapDescription_VtblStub_Return1),   // +0x18 GetVersion
    reinterpret_cast<void*>(&MapDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&MapDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&MapDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&MapDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&MapDescription_GetComponentTypeId), // +0x40 GetComponentTypeId
    reinterpret_cast<void*>(&MapDescription_CreateInPlace),      // +0x48 CreateInPlace
    reinterpret_cast<void*>(&MapDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterMapDescriptionVtable() {
    RegisterExeAddr(kMapDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_MapDescription_Vtable));
}

extern "C" void* MapDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
    if (!p) return nullptr;
    MapDescription_Init(p);
    return p;
}

static void* MapDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    MapDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(MapDescription, "Description",
              &MapDescription_Factory, &MapDescription_CreateInPlace);

