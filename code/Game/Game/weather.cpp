// code/Game/Game/weather.cpp
// Clean reimplementation of Weather game logic.
//
// Weather (0x80 bytes) — weather particle system + environment settings.
// Extends Object::External (0x20 base).
// Vtable at 0x140160290.
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/Weather.h, field layout
// and retail slot map there) with a compiler-generated vtable. Gone: the
// hand-built 9-slot `g_Weather_Vtable` (two null slots: Compile, Destruct), its
// stubs, the lazy vtable resolve in the old Init and the C entry points.
//
// Weather defines particle effects (rain/snow) and environment lighting
// (ambient color, sun direction, fog). Loaded from .rsc resources like
// Environment/Rain.rsc and Environment/Snow.rsc.
//
// Decompiled from Application-x64.exe.

#include "Game/Weather.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <new>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Weather::Weather — Initialize all fields to defaults
// ============================================================
// Mirrors Weather_Ctor at 0x1400d3e50. External() and the 0x140160290 stamp
// are the compiler's; then the Weather-specific defaults.
Weather::Weather() {
    // Particle system defaults
    material = nullptr;
    particleCount = 100;
    velocityDirX = 0.0f;
    velocityDirY = 0.0f;
    velocityDirZ = -1.0f;
    velocity = 0.0f;
    velocityTolerence = 0.0f;
    velocityStretch = false;
    particleSize = 0.25f;
    size = 30.0f;
    nearFade = 25.0f;
    nearFadeStart = 30.0f;
    farFadeStart = 1.0f;
    farFade = 0.0f;
    transitionTime = 0.0f;

    // Environment/lighting defaults
    ambientColor = 0xFF595965;
    lightColor = 0xFFCBCBBC;
    fogColor = 0xFFBFBFF2;
    fogNear = 90.0f;
    fogFar = 225.0f;
    fogMax = 0.95f;
    sunHeading = 120.0f;
    sunPitch = 65.0f;
}

// ============================================================
// Weather::Reconstruct (vtable +0x30, Slot6) — Reset to defaults
// ============================================================
// Original: 0x140025000. Destroys (slot 0, flags 0) then re-constructs in place.
Object::External* Weather::Reconstruct() {
    this->~Weather();
    return ::new (static_cast<void*>(this)) Weather();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void Weather::Destruct() { this->~Weather(); }

// [8] 0x140024ff0 — the in-place ctor (placement-new of the class on `this`).
Object::External* Weather::Construct() {
    return ::new (static_cast<void*>(this)) Weather();
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

bool Weather::Serialize(void* stream) {
    // Create IO::Serializer on the stack (24 bytes), version 0
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // ExternalPtr material (+0x20)
    Reimpl_ExternalPtr_Serialize_Typed_7140(&material, serializer);

    // int particleCount (+0x28) — declared int32_t == int door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(particleCount, nullptr);   // B.3.4 bridge

    // Vector3 velocityDir (+0x2C..+0x34)  — member function: this=vec3, arg=serializer
    // B.3.1 bridge: WeatherParticle keeps the three floats as separate members.
    reinterpret_cast<Math::Vector3*>(&velocityDirX)->Serialize(serializer);

    // float velocity (+0x38) — declared float == float door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(velocity, nullptr);   // B.3.4 bridge

    // float velocityTolerence (+0x3C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(velocityTolerence, nullptr);   // B.3.4 bridge

    // bool velocityStretch (+0x40) — declared bool == bool door type: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(velocityStretch, nullptr);   // B.3.4 bridge

    // float particleSize (+0x44) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(particleSize, nullptr);   // B.3.4 bridge

    // float size (+0x48) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(size, nullptr);   // B.3.4 bridge

    // float nearFade (+0x4C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(nearFade, nullptr);   // B.3.4 bridge

    // float nearFadeStart (+0x50) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(nearFadeStart, nullptr);   // B.3.4 bridge

    // float farFadeStart (+0x54) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(farFadeStart, nullptr);   // B.3.4 bridge

    // float farFade (+0x58) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(farFade, nullptr);   // B.3.4 bridge

    // float transitionTime (+0x5C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(transitionTime, nullptr);   // B.3.4 bridge

    // Color ambientColor (+0x60)  — member function: this=color, arg=serializer
    reinterpret_cast<System::Color*>(&ambientColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // Color lightColor (+0x64)
    reinterpret_cast<System::Color*>(&lightColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // Color fogColor (+0x68)
    reinterpret_cast<System::Color*>(&fogColor)->Serialize(*reinterpret_cast<IO::Serializer*>(serializer));   // B.3.4b bridge

    // float fogNear (+0x6C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(fogNear, nullptr);   // B.3.4 bridge

    // float fogFar (+0x70) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(fogFar, nullptr);   // B.3.4 bridge

    // float fogMax (+0x74) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(fogMax, nullptr);   // B.3.4 bridge

    // float sunHeading (+0x78) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(sunHeading, nullptr);   // B.3.4 bridge

    // float sunPitch (+0x7C) — declared float: cast dropped
    reinterpret_cast<IO::Serializer*>(serializer)->Serialize(sunPitch, nullptr);   // B.3.4 bridge

    // Destroy serializer
    reinterpret_cast<IO::Serializer*>(serializer)->~Serializer();   // B.3.4 bridge
    return true;
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* Weather::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(Weather), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) Weather());
}
void* Weather::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) Weather());
}


DEFINE_EXE_TYPE(Weather, "Object::External")
