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
#include "resolve.h"
#include <cstdint>

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

// Local function pointers resolved on first call (not in engine_funcs.h)
// IO::Serializer::Serialize(float&, const char*) via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* serializer, void* field, const char* name) = nullptr;
// Math::Vector3::Serialize(this, serializer) via IAT at 0x140152a68
static void (*s_fn_Vector3_Serialize)(void* vec3, void* serializer) = nullptr;
// System::Color::Serialize(this, serializer) via IAT at 0x140151ee0
static void (*s_fn_Color_Serialize)(void* color, void* serializer) = nullptr;

static bool s_weather_serialize_inited = false;

static void Weather_Serialize_InitFuncs() {
    if (s_weather_serialize_inited) return;
    s_weather_serialize_inited = true;

    uintptr_t iat;

    // Serialize_Float via IAT at [0x140152a60]
    iat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iat) s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iat);

    // Vector3::Serialize via IAT at [0x140152a68]
    iat = ResolveAddr(Module::Exe, 0x140152a68);
    if (iat) s_fn_Vector3_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iat);

    // Color::Serialize via IAT at [0x140151ee0]
    iat = ResolveAddr(Module::Exe, 0x140151ee0);
    if (iat) s_fn_Color_Serialize = *reinterpret_cast<void(**)(void*, void*)>(iat);
}

void Weather_Serialize(Weather* self, void* stream) {
    Weather_Serialize_InitFuncs();

    // Create IO::Serializer on the stack (24 bytes), version 0
    uint8_t serializer[24];
    g_fn_Serializer_Ctor(serializer, stream, 0);

    // ExternalPtr material (+0x20)
    fn_ExternalPtr_Serialize3(&self->material, serializer);

    // int particleCount (+0x28)
    g_fn_Serialize_Int(serializer, &self->particleCount, nullptr);

    // Vector3 velocityDir (+0x2C..+0x34)  — member function: this=vec3, arg=serializer
    s_fn_Vector3_Serialize(&self->velocityDirX, serializer);

    // float velocity (+0x38)
    s_fn_Serialize_Float(serializer, &self->velocity, nullptr);

    // float velocityTolerence (+0x3C)
    s_fn_Serialize_Float(serializer, &self->velocityTolerence, nullptr);

    // bool velocityStretch (+0x40)
    g_fn_Serialize_Bool(serializer, &self->velocityStretch, nullptr);

    // float particleSize (+0x44)
    s_fn_Serialize_Float(serializer, &self->particleSize, nullptr);

    // float size (+0x48)
    s_fn_Serialize_Float(serializer, &self->size, nullptr);

    // float nearFade (+0x4C)
    s_fn_Serialize_Float(serializer, &self->nearFade, nullptr);

    // float nearFadeStart (+0x50)
    s_fn_Serialize_Float(serializer, &self->nearFadeStart, nullptr);

    // float farFadeStart (+0x54)
    s_fn_Serialize_Float(serializer, &self->farFadeStart, nullptr);

    // float farFade (+0x58)
    s_fn_Serialize_Float(serializer, &self->farFade, nullptr);

    // float transitionTime (+0x5C)
    s_fn_Serialize_Float(serializer, &self->transitionTime, nullptr);

    // Color ambientColor (+0x60)  — member function: this=color, arg=serializer
    s_fn_Color_Serialize(&self->ambientColor, serializer);

    // Color lightColor (+0x64)
    s_fn_Color_Serialize(&self->lightColor, serializer);

    // Color fogColor (+0x68)
    s_fn_Color_Serialize(&self->fogColor, serializer);

    // float fogNear (+0x6C)
    s_fn_Serialize_Float(serializer, &self->fogNear, nullptr);

    // float fogFar (+0x70)
    s_fn_Serialize_Float(serializer, &self->fogFar, nullptr);

    // float fogMax (+0x74)
    s_fn_Serialize_Float(serializer, &self->fogMax, nullptr);

    // float sunHeading (+0x78)
    s_fn_Serialize_Float(serializer, &self->sunHeading, nullptr);

    // float sunPitch (+0x7C)
    s_fn_Serialize_Float(serializer, &self->sunPitch, nullptr);

    // Destroy serializer
    g_fn_Serializer_Dtor(serializer);
}
