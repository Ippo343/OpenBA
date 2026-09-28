// code/Game/Game/climategenerator.cpp
// Clean reimplementation of ClimateGenerator game logic.
//
// ClimateGenerator (0x110 bytes) -- climate/weather generation parameters.
// Extends Object::External (0x20 base). Vtable at 0x14015e140.
//
// Contains temperature ranges, precipitation params, wind speed,
// and a 0xC0-byte block of seasonal/biome data (48 floats).
//
// ENDGAME B.3.8 block LOOP: a REAL class (code/Game/Game/ClimateGenerator.h,
// field layout and retail slot map there) with a compiler-generated vtable.
// Gone: the 11-slot `g_ClimateGenerator_Vtable_Standalone` (two over-read
// guards past retail's 9), its trampolines and stubs, the lazy vtable resolve
// in the old Init and the C entry points (reached only through the table).
//
// In standalone the exe's *_RegisterType never runs, so without this class
// ExternalFactory::FindOrLoad("Game/Climate/ClimateList.rsc")'s per-element
// load hits a NullFactory and crashes inside Runtime IsDerivedFrom.
//
// Decompiled from Application-x64.exe.

#include "Game/ClimateGenerator.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "system/String.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch B
#include "object/Internal.h" // Object::detail::InternalMemAlloc / InternalMemFree
#include <cstdint>
#include <cstring>
#include <new>

// ============================================================
// ClimateGenerator::ClimateGenerator (0x1400930c0)
// ============================================================
// External() and the 0x14015e140 stamp are the compiler's; then the defaults.
ClimateGenerator::ClimateGenerator() {
    // ExternalPtr at +0x20 = null
    _field20 = nullptr;
    // String at +0x28 = null (8-byte COW string)
    _field28 = nullptr;

    // Climate parameters
    _field30 = 1;              // enabled
    temperatureBase = 50.0f;   // base temperature
    tempVariation = 30.0f;     // temperature variation
    precipitation = 5.0f;      // precipitation amount
    windSpeed = 10.0f;         // wind speed

    // Trailing float fields
    _field104 = 0;             // zeroed (float 0.0 or uint 0)
    _field108 = 1.0f;
    _field10C = 1.0f;

    // Zero the 0xC0-byte climate data block at +0x44
    memset(_climateData, 0, 0xC0);
}

// ============================================================
// ClimateGenerator::~ClimateGenerator / DeletingDtor (vtable +0x00)
// ============================================================
// Original: 0x140018f60. Destroys the String at +0x28; External::~External is
// the compiler's chain; MemFree if flags & 1.
ClimateGenerator::~ClimateGenerator() {
    reinterpret_cast<System::String*>(&_field28)->~String();   // B.3.1 bridge: the COW pointer
}

// ============================================================
// ClimateGenerator::Reconstruct (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140018f40. Destroys the object (slot 0, flags 0: no free) then
// re-constructs in place.
Object::External* ClimateGenerator::Reconstruct() {
    this->~ClimateGenerator();
    return ::new (static_cast<void*>(this)) ClimateGenerator();
}

// [7] 0x140001640 — the shared body `MOV RAX,[RCX]; XOR EDX,EDX; JMP [RAX]`:
// slot 0 with flags 0 (destroy, no free).
void ClimateGenerator::Destruct() { this->~ClimateGenerator(); }

// ============================================================
// ClimateGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x140092fa0. Serializes all fields using IO::Serializer with
// version 0. Returns 1 (success) -- original returns AL=1.
bool ClimateGenerator::Serialize(void* stream) {
    // Create Serializer on stack (24 bytes)
    alignas(8) uint8_t serializer[24];
    ::new (static_cast<void*>(serializer)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    IO::Serializer& s = *reinterpret_cast<IO::Serializer*>(serializer);   // B.3.4 bridge

    // Serialize ExternalPtr at +0x20
    Reimpl_ExternalPtr_Serialize_StringTable(&_field20, serializer);

    // Serialize String at +0x28
    reinterpret_cast<System::String*>(&_field28)->Serialize(serializer);

    // Serialize bool at +0x30 — declared uint8_t, not exactly bool: cast kept
    s.Serialize(*reinterpret_cast<bool*>(&_field30), nullptr);

    // Serialize float fields: temperatureBase(+0x34), tempVariation(+0x38),
    // precipitation(+0x3C), windSpeed(+0x40)
    s.Serialize(temperatureBase, nullptr);
    s.Serialize(tempVariation, nullptr);
    s.Serialize(precipitation, nullptr);
    s.Serialize(windSpeed, nullptr);

    // Serialize climate data block: 0x30 (48) floats = 0xC0 bytes at +0x44 — declared
    // uint8_t[0xC0]: reinterpret to float* per the float-array door
    s.Serialize(0x30, reinterpret_cast<float*>(_climateData), nullptr);

    // Serialize float fields at end: field104(+0x104), field108(+0x108), field10C(+0x10C)
    s.Serialize(_field104, nullptr);
    s.Serialize(_field108, nullptr);
    s.Serialize(_field10C, nullptr);

    // Destroy serializer
    s.~Serializer();
    return true;
}

// ============================================================
// ClimateGenerator::Construct (vtable +0x40)
// ============================================================
// Original: 0x140018f30 — placement-new of the class on `this` (MSVC's
// placement new null-checks `this`, as the old slot did).
Object::External* ClimateGenerator::Construct() {
    return ::new (static_cast<void*>(this)) ClimateGenerator();
}

// ============================================================
// Reflection: create / createInPlace, the registration
// ============================================================
void* ClimateGenerator::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ClimateGenerator), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) ClimateGenerator());
}
void* ClimateGenerator::CreateInPlaceObject(void* memory) {
    if (!memory) return nullptr;
    return static_cast<Object::Base*>(::new (memory) ClimateGenerator());
}


DEFINE_EXE_TYPE(ClimateGenerator, "Object::External")
