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
#include "object/External.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "math/Math.h"
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
Weather::Weather() : velocityDir(0.0f, 0.0f, -1.0f) {
    // Particle system defaults
    material = nullptr;
    particleCount = 100;
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
    ambientColor._value = 0xFF595965;
    lightColor._value = 0xFFCBCBBC;
    fogColor._value = 0xFFBFBFF2;
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

bool Weather::Serialize(IO::Stream& stream) {
    IO::Serializer ser(stream, 0);

    Reimpl_ExternalPtr_Serialize_Typed_7140(&material, &ser);   // ExternalPtr material (+0x20)
    ser.Serialize(particleCount, nullptr);        // +0x28
    velocityDir.Serialize(&ser);                  // +0x2C Vector3
    ser.Serialize(velocity, nullptr);             // +0x38
    ser.Serialize(velocityTolerence, nullptr);    // +0x3C
    ser.Serialize(velocityStretch, nullptr);      // +0x40
    ser.Serialize(particleSize, nullptr);         // +0x44
    ser.Serialize(size, nullptr);                 // +0x48
    ser.Serialize(nearFade, nullptr);             // +0x4C
    ser.Serialize(nearFadeStart, nullptr);        // +0x50
    ser.Serialize(farFadeStart, nullptr);         // +0x54
    ser.Serialize(farFade, nullptr);              // +0x58
    ser.Serialize(transitionTime, nullptr);       // +0x5C
    ambientColor.Serialize(ser);                  // +0x60 Color
    lightColor.Serialize(ser);                    // +0x64
    fogColor.Serialize(ser);                      // +0x68
    ser.Serialize(fogNear, nullptr);              // +0x6C
    ser.Serialize(fogFar, nullptr);               // +0x70
    ser.Serialize(fogMax, nullptr);               // +0x74
    ser.Serialize(sunHeading, nullptr);           // +0x78
    ser.Serialize(sunPitch, nullptr);             // +0x7C
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
