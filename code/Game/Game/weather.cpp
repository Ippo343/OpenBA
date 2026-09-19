// code/Game/Game/weather.cpp
// Clean reimplementation of Weather game logic.
//
// Weather (0x80 bytes) — weather particle system + environment settings.
// Extends Object::External (0x20 base).
// Vtable at 0x140160290.
//
// Weather defines particle effects (rain/snow) and environment lighting
// (ambient color, sun direction, fog). Loaded from .rsc resources like
// Environment/Rain.rsc and Environment/Snow.rsc.
//
// Decompiled from Application-x64.exe.

#include "Game/weather_funcs.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include <cstdint>
#include <new>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Weather::Dtor (vtable +0x00) — Scalar deleting destructor
// ============================================================
// Calls Object::External::~External to clean up base, then
// conditionally frees memory if flags & 1.
//
// Original: 0x140025020
void* Weather_Dtor(Weather* self, uint32_t flags) {
    fn_External_Dtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// Weather::Cleanup (vtable +0x30, Slot6) — Reset to defaults
// ============================================================
// Destroys the object (without freeing) then re-constructs in place.
// This resets all weather fields to their default values.
//
// Original: 0x140025000
void Weather_Cleanup(Weather* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    Weather_Init(self);
}

// ============================================================
// Weather::Init — Initialize all fields to defaults
// ============================================================
// Mirrors Weather_Ctor at 0x1400d3e50.
// Calls Object::External::External for base init, then sets
// Weather-specific fields to their default values.
void Weather_Init(Weather* self) {
    // Initialize External base class
    fn_External_External(self);

    // Set Weather vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, WeatherAddr::Vtable);
    self->vtable = s_vtable;

    // Particle system defaults
    self->material = nullptr;
    self->particleCount = 100;
    self->velocityDirX = 0.0f;
    self->velocityDirY = 0.0f;
    self->velocityDirZ = -1.0f;
    self->velocity = 0.0f;
    self->velocityTolerence = 0.0f;
    self->velocityStretch = false;
    self->particleSize = 0.25f;
    self->size = 30.0f;
    self->nearFade = 25.0f;
    self->nearFadeStart = 30.0f;
    self->farFadeStart = 1.0f;
    self->farFade = 0.0f;
    self->transitionTime = 0.0f;

    // Environment/lighting defaults
    self->ambientColor = 0xFF595965;
    self->lightColor = 0xFFCBCBBC;
    self->fogColor = 0xFFBFBFF2;
    self->fogNear = 90.0f;
    self->fogFar = 225.0f;
    self->fogMax = 0.95f;
    self->sunHeading = 120.0f;
    self->sunPitch = 65.0f;
}

// ============================================================
// Weather::Serialize (vtable +0x28, Slot5) — Save/Load all fields
// ============================================================
// Creates an IO::Serializer, then serializes every Weather field
// in declaration order. ExternalPtr (material) uses a dedicated
// serialize helper; Vector3 and Color have their own member
// serialize methods; int/float/bool use the generic overloads.
//
// Original: 0x1400d3c90

void Weather_Serialize(Weather* self, void* stream) {
    // Create IO::Serializer on the stack (24 bytes), version 0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // ExternalPtr material (+0x20)
    fn_ExternalPtr_Serialize3(&self->material, serializer);

    // int particleCount (+0x28) — declared int32_t == int door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->particleCount, nullptr);   // B.3.4 bridge

    // Vector3 velocityDir (+0x2C..+0x34)  — member function: this=vec3, arg=serializer
    // B.3.1 bridge: WeatherParticle keeps the three floats as separate members.
    reinterpret_cast<Math::Vector3*>(&self->velocityDirX)->Serialize(serializer);

    // float velocity (+0x38) — declared float == float door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->velocity, nullptr);   // B.3.4 bridge

    // float velocityTolerence (+0x3C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->velocityTolerence, nullptr);   // B.3.4 bridge

    // bool velocityStretch (+0x40) — declared bool == bool door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->velocityStretch, nullptr);   // B.3.4 bridge

    // float particleSize (+0x44) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->particleSize, nullptr);   // B.3.4 bridge

    // float size (+0x48) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->size, nullptr);   // B.3.4 bridge

    // float nearFade (+0x4C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->nearFade, nullptr);   // B.3.4 bridge

    // float nearFadeStart (+0x50) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->nearFadeStart, nullptr);   // B.3.4 bridge

    // float farFadeStart (+0x54) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->farFadeStart, nullptr);   // B.3.4 bridge

    // float farFade (+0x58) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->farFade, nullptr);   // B.3.4 bridge

    // float transitionTime (+0x5C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->transitionTime, nullptr);   // B.3.4 bridge

    // Color ambientColor (+0x60)  — member function: this=color, arg=serializer
    reinterpret_cast<System::Color*>(&self->ambientColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // Color lightColor (+0x64)
    reinterpret_cast<System::Color*>(&self->lightColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // Color fogColor (+0x68)
    reinterpret_cast<System::Color*>(&self->fogColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // float fogNear (+0x6C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->fogNear, nullptr);   // B.3.4 bridge

    // float fogFar (+0x70) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->fogFar, nullptr);   // B.3.4 bridge

    // float fogMax (+0x74) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->fogMax, nullptr);   // B.3.4 bridge

    // float sunHeading (+0x78) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->sunHeading, nullptr);   // B.3.4 bridge

    // float sunPitch (+0x7C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(self->sunPitch, nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
}

// ============================================================
// STANDALONE_EXE scaffolding — Weather factory + vtable + REGISTER_TYPE
// ============================================================
// Mirrors the StatGoal pattern in code/Game/Goal/statgoal.cpp.
//
// Original retail vtable @ 0x140160290 (9 slots):
//   +0x00 Dtor                  (Weather_ScalarDtor   @ 0x140025020)
//   +0x08 GetTypeName           (Weather_Slot1        @ 0x140024fa0) — returns "Weather"
//   +0x10 GetTypeId             (Weather_Slot2        @ 0x140024f90) — returns CRC32 typeId
//   +0x18 ?Slot3                (Weather_Shutdown     @ 0x1400d3e40) — returns 5
//   +0x20 Compile (External)    (External::Compile JT @ 0x14003a912)
//   +0x28 Slot5/Serialize       (Weather_Slot5        @ 0x1400d3c90)
//   +0x30 Slot6/Cleanup         (Weather_Cleanup      @ 0x140025000)
//   +0x38 Slot7/Draw shim       (vtable[0](this,0)    @ 0x140001640)
//   +0x40 Slot8/CreateInPlace   (Weather_OnRemove     @ 0x140024ff0)
//
// For the MenuScene deserialize-only call path, only Slot5 (Serialize) and
// Slot8 (CreateInPlace) really need to fire. Other inherited slots are wired
// to retail addresses via ResolveAddr fallback when something virtual-calls
// them.


#include "Object/factory_registration.h"

// Slot +0x08: GetTypeName — returns "Weather".
static const char* Weather_GetTypeName(void* /*self*/) {
    return "Weather";
}

// Slot +0x10: GetTypeId — returns CRC32 typeId set by Factory::Startup.
static uint32_t Weather_GetTypeId(void* /*self*/) {
    static uint32_t cached = 0;
    if (cached == 0) {
        cached = ::Object::GetTypeIdByName("Weather");
    }
    return cached;
}

// Slot +0x18: matches retail Weather_Shutdown — returns 5 (external category enum).
static uint64_t Weather_Slot3_StaticInt(void* /*self*/) {
    return 5;
}

// Slot +0x40 trampoline (BaseTypeInfo inPlaceCreateFn). The retail Weather_OnRemove
// is the in-place ctor (same as Weather_Init on already-allocated memory).
static void* Weather_Slot8_InPlace(void* memory) {
    if (!memory) return nullptr;
    Weather_Init(reinterpret_cast<Weather*>(memory));
    return memory;
}

static void* g_Weather_Vtable[9] = {
    reinterpret_cast<void*>(&Weather_Dtor),                // +0x00 dtor
    reinterpret_cast<void*>(&Weather_GetTypeName),         // +0x08 GetTypeName
    reinterpret_cast<void*>(&Weather_GetTypeId),           // +0x10 GetTypeId
    reinterpret_cast<void*>(&Weather_Slot3_StaticInt),     // +0x18 returns 5
    nullptr,                                               // +0x20 External::Compile (jumptable; unused on menu path)
    reinterpret_cast<void*>(&Weather_Serialize),           // +0x28 Slot5
    reinterpret_cast<void*>(&Weather_Cleanup),             // +0x30 Slot6
    nullptr,                                               // +0x38 Slot7 (Draw-shim; unused on menu path)
    reinterpret_cast<void*>(&Weather_Slot8_InPlace),       // +0x40 in-place ctor
};

#include "address_registry.h"

extern "C" void RegisterWeatherVtable() {
    RegisterExeAddr(WeatherAddr::Vtable, reinterpret_cast<uintptr_t>(g_Weather_Vtable));
}

extern "C" void* Weather_Factory(void) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, sizeof(Weather), 8);
    if (!p) return nullptr;
    Weather_Init(reinterpret_cast<Weather*>(p));
    return p;
}

extern "C" void* Weather_CreateInPlace(void* memory) {
    if (!memory) return nullptr;
    Weather_Init(reinterpret_cast<Weather*>(memory));
    return memory;
}

REGISTER_TYPE(Weather, "Object::External",
              &Weather_Factory, &Weather_CreateInPlace);

