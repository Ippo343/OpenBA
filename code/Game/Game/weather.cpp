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
