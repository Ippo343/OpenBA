// code/Game/Component/environmentdescription.cpp
// Clean reimplementation of EnvironmentDescription (entity description block).
//
// EnvironmentDescription (0x1b8 bytes) — per-map weather/lighting environment.
// Extends Object::External (0x20 base). Vtable at 0x14015e1f0. Parent "Object::External".
//   +0x20: ExternalPtr  (environment name/path ref)
//   +0x28: void* dynamic weather-entry array base (16-byte elements: {WeatherExt, ExtPtr})
//   +0x30: uint32 array count
//   +0x38..+0x1b8: 12 weather entries, each 0x20 bytes:
//     +0x00 Color(sky) +0x04 Color(ground) +0x08 Color(fog)
//     +0x0c float(fogStart) +0x10 float(fogEnd) +0x14 float(sunIntensity)
//     +0x18 float(sunAngle) +0x1c float(sunSpread)
//
// Factory  : MemAlloc 0x1b8 + External ctor + vtable + 12 weather-entry defaults.
// Serialize (vtable +0x28, 0x140094040), stream order:
//   ExternalPtr @ +0x20  (FUN_140002340 / fn_ExternalPtr_BuildSerialize)
//   int count   @ +0x30
//   [if loading: fn_WeatherResize(+0x28, count)]
//   for each array entry (stride 16): WeatherExternalSerialize(entry) + ExternalPtr(entry+8)
//   12 weather entries @ +0x38 (each: 3 Color + 5 float, all 4-byte Serialize)
// GetVersion (vtable +0x18, 0x1400941b0): mov eax,7; ret -> version 7.
// Dtor (vtable +0x00, 0x140019190): MemFree(array @+0x28) + External::~External + free.
//
// NOTE: fn_WeatherResize (0x1400198a0) and fn_WeatherExternalSerialize (0x1400196a0)
// resolve via ResolveAddr(Module::Exe,..) — null in standalone until those addrs are
// registered (REPORTED). fn_ExternalPtr_BuildSerialize (0x140002340) is already registered.
//
// Field knowledge harvested from engine/reimpl/environmentdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch A
#include <cstdint>
#include <new>

static const uintptr_t kEnvironmentDescriptionVtable = 0x14015e1f0;

static void EnvironmentDescription_InitWeatherDefaults(uint8_t* s) {
    uint32_t* w = (uint32_t*)(s + 0x38);
    for (int i = 0; i < 12; ++i) {
        w[0] = 0xff595965;  // Color ambient sky
        w[1] = 0xffcbcbbc;  // Color ambient ground
        w[2] = 0xffbfbff2;  // Color ambient fog
        w[3] = 0x42b40000;  // float fog start = 90.0f
        w[4] = 0x43610000;  // float fog end = 225.0f
        w[5] = 0x3f733333;  // float sun intensity = 0.95f
        w[6] = 0x42f00000;  // float sun angle = 120.0f
        w[7] = 0x42820000;  // float sun spread = 65.0f
        w += 8;             // advance 0x20 bytes
    }
}

static void EnvironmentDescription_Init(void* self) {
    fn_External_External(self);
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, kEnvironmentDescriptionVtable);
    *(void**)self = s_vtable;
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(uint64_t*)(s + 0x20) = 0;   // ExternalPtr
    *(uint64_t*)(s + 0x28) = 0;   // array base
    *(uint64_t*)(s + 0x30) = 0;   // count (+cap)
    EnvironmentDescription_InitWeatherDefaults(s);
}

void EnvironmentDescription_Serialize(void* self, void* stream) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (!fn_ExternalPtr_BuildSerialize || !fn_WeatherResize || !fn_WeatherExternalSerialize) {
        return;
    }
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    fn_ExternalPtr_BuildSerialize(s + 0x20, serializer);   // ExternalPtr +0x20

    uint32_t count = *(uint32_t*)(s + 0x30);
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(&count), nullptr);       // B.3.4 bridge  (int count +0x30)

    // On load: resize the dynamic weather-entry array.
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serializer)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        fn_WeatherResize(s + 0x28, (int)count);
    }

    uintptr_t* arrPtr = *(uintptr_t**)(s + 0x28);
    for (uint32_t i = 0; i < count; ++i) {
        fn_WeatherExternalSerialize(arrPtr, serializer);
        fn_ExternalPtr_BuildSerialize(arrPtr + 1, serializer);
        arrPtr += 2;   // 16-byte entries
    }

    // 12 inline weather entries @ +0x38, each 0x20 bytes: 3 Colors + 5 floats (all 4-byte).
    uintptr_t entry = (uintptr_t)s + 0x38;
    for (int i = 0; i < 12; ++i) {
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x00), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x04), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x08), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x0c), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x10), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x14), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x18), nullptr);   // B.3.4 bridge
        reinterpret_cast<IO::Serializer*>(serializer)->Serialize(*reinterpret_cast<int*>(entry + 0x1c), nullptr);   // B.3.4 bridge
        entry += 0x20;
    }

    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

void* EnvironmentDescription_Dtor(void* self, uint32_t flags) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    uintptr_t vt = ResolveAddr(Module::Exe, kEnvironmentDescriptionVtable);
    if (vt) *(void**)self = (void*)vt;
    if (fn_MemFreeStatic && *(void**)(s + 0x28)) {
        fn_MemFreeStatic(*(void**)(s + 0x28));   // dynamic weather array
    }
    *(uint64_t*)(s + 0x28) = 0;
    *(uint64_t*)(s + 0x30) = 0;
    if (fn_External_Dtor) fn_External_Dtor(self);
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}


#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t EnvironmentDescription_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) cached = ::Object::GetTypeIdByName("EnvironmentDescription");
    return cached;
}

static int   EnvironmentDescription_VtblStub_Return7(void* /*self*/) { return 7; } // GetVersion
static int   EnvironmentDescription_VtblStub_Return0(void* /*self*/) { return 0; }
static void* EnvironmentDescription_VtblStub_ReturnNull(void*, ...)  { return nullptr; }

static void* EnvironmentDescription_CreateInPlace(void* memory);

static void* g_EnvironmentDescription_Vtable[11] = {
    reinterpret_cast<void*>(&EnvironmentDescription_Dtor),               // +0x00 Dtor
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_ReturnNull),// +0x08 Slot1
    reinterpret_cast<void*>(&EnvironmentDescription_GetTypeId),          // +0x10 GetTypeId
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_Return7),   // +0x18 GetVersion
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_Return0),   // +0x20 Compile
    reinterpret_cast<void*>(&EnvironmentDescription_Serialize),          // +0x28 Serialize
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_Return0),   // +0x30 Cleanup
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_Return0),   // +0x38 GetDesc
    reinterpret_cast<void*>(&EnvironmentDescription_CreateInPlace),      // +0x40 CreateInPlace (this type's CreateInPlace is at +0x40)
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_ReturnNull),// +0x48
    reinterpret_cast<void*>(&EnvironmentDescription_VtblStub_ReturnNull),// +0x50 (guard)
};

extern "C" void RegisterEnvironmentDescriptionVtable() {
    RegisterExeAddr(kEnvironmentDescriptionVtable,
                    reinterpret_cast<uintptr_t>(g_EnvironmentDescription_Vtable));
}

extern "C" void* EnvironmentDescription_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, 0x1b8, 8);
    if (!p) return nullptr;
    EnvironmentDescription_Init(p);
    return p;
}

static void* EnvironmentDescription_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    EnvironmentDescription_Init(memory);
    return memory;
}

REGISTER_TYPE(EnvironmentDescription, "Object::External",
              &EnvironmentDescription_Factory, &EnvironmentDescription_CreateInPlace);

