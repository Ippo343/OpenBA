// code/Game/Game/climategenerator.cpp
// Clean reimplementation of ClimateGenerator game logic.
//
// ClimateGenerator (0x110 bytes) -- climate/weather generation parameters.
// Extends Object::External (0x20 base).
// Vtable at 0x14015e140.
//
// Contains temperature ranges, precipitation params, wind speed,
// and a 0xC0-byte block of seasonal/biome data (48 floats).
//
// Field layout (from constructor + serialize):
//   +0x00: vtable
//   +0x08: External base (0x18 bytes)
//   +0x20: ExternalPtr (8 bytes, serialized via FUN_14000bf40)
//   +0x28: System::String (8 bytes, name/reference)
//   +0x30: bool enabled (default 1)
//   +0x34: float temperatureBase (default 50.0f)
//   +0x38: float tempVariation (default 30.0f)
//   +0x3C: float precipitation (default 5.0f)
//   +0x40: float windSpeed (default 10.0f)
//   +0x44: float[48] climateData (0xC0 bytes, default 0)
//   +0x104: float field104 (default 0.0f)
//   +0x108: float field108 (default 1.0f)
//   +0x10C: float field10C (default 1.0f)
//
// Decompiled from Application-x64.exe.

#include "Game/climategenerator_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstring>

// ============================================================
// Local function pointers (resolved once)
// ============================================================

// IO::Serializer::Serialize(float&, const char*) -- via IAT at 0x140152a60
static void (*s_fn_Serialize_Float)(void* serializer, void* field, const char* name) = nullptr;

// IO::Serializer::Serialize(int count, void* buffer, const char* name) -- bulk serialize
// Via IAT at 0x140151ea8 (used for climate data block)
static void (*s_fn_SerializeBulk)(void* serializer, int count, void* buffer, const char* name) = nullptr;

// ============================================================
// ClimateGenerator_InitFuncs -- resolve local function pointers
// ============================================================
void ClimateGenerator_InitFuncs() {
    // Float serialize via IAT dispatch slot
    uintptr_t iatSerFloat = ResolveAddr(Module::Exe, 0x140152a60);
    if (iatSerFloat)
        s_fn_Serialize_Float = *reinterpret_cast<void(**)(void*, void*, const char*)>(iatSerFloat);

    // Bulk serialize via IAT dispatch slot (for climate data array)
    uintptr_t iatSerBulk = ResolveAddr(Module::Exe, 0x140151ea8);
    if (iatSerBulk)
        s_fn_SerializeBulk = *reinterpret_cast<void(**)(void*, int, void*, const char*)>(iatSerBulk);
}

// ============================================================
// Lazy init guard
// ============================================================
static bool s_inited = false;
static void EnsureInit() {
    if (!s_inited) {
        ClimateGenerator_InitFuncs();
        s_inited = true;
    }
}

// ============================================================
// ClimateGenerator::Dtor (vtable +0x00) -- Scalar deleting destructor
// ============================================================
// Original: 0x140018f60
// Destroys the String at +0x28, calls External::~External, then
// conditionally frees memory if flags & 1.
void* ClimateGenerator_Dtor(ClimateGenerator* self, uint32_t flags) {
    EnsureInit();
    // Destroy System::String at +0x28
    if (fn_String_Dtor) {
        fn_String_Dtor(reinterpret_cast<uint8_t*>(self) + 0x28);
    }

    // Call External base destructor
    fn_External_Dtor(self);

    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// ClimateGenerator::Cleanup (vtable +0x30, Slot6) -- Reset to defaults
// ============================================================
// Original: 0x140018f40
// Destroys the object (without freeing) then re-constructs in place.
void ClimateGenerator_Cleanup(ClimateGenerator* self) {
    // Call Dtor with flags=0 (don't free memory)
    typedef void* (*fn_VtableDtor_t)(void* self, uint32_t flags);
    fn_VtableDtor_t dtor = *(fn_VtableDtor_t*)(*(uintptr_t*)self);
    dtor(self, 0);

    // Re-construct in place
    ClimateGenerator_Init(self);
}

// ============================================================
// ClimateGenerator::Serialize (vtable +0x28, Slot5)
// ============================================================
// Original: 0x140092fa0
// Serializes all fields using IO::Serializer with version 0.
// Returns 1 (success) -- original returns AL=1.
void ClimateGenerator_Serialize(ClimateGenerator* self, void* stream) {
    EnsureInit();
    // Create Serializer on stack (24 bytes)
    uint8_t serializer[24];
    if (g_fn_Serializer_Ctor) g_fn_Serializer_Ctor(serializer, stream, 0);

    // Serialize ExternalPtr at +0x20
    if (fn_ExternalPtr_Serialize2)
        fn_ExternalPtr_Serialize2(reinterpret_cast<uint8_t*>(self) + 0x20, serializer);

    // Serialize String at +0x28
    if (g_fn_String_Serialize)
        g_fn_String_Serialize(reinterpret_cast<uint8_t*>(self) + 0x28, serializer);

    // Serialize bool at +0x30
    if (g_fn_Serialize_Bool)
        g_fn_Serialize_Bool(serializer, reinterpret_cast<uint8_t*>(self) + 0x30, nullptr);

    // Serialize float fields: temperatureBase(+0x34), tempVariation(+0x38),
    // precipitation(+0x3C), windSpeed(+0x40)
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, &self->temperatureBase, nullptr);
        s_fn_Serialize_Float(serializer, &self->tempVariation, nullptr);
        s_fn_Serialize_Float(serializer, &self->precipitation, nullptr);
        s_fn_Serialize_Float(serializer, &self->windSpeed, nullptr);
    }

    // Serialize climate data block: 0x30 (48) floats = 0xC0 bytes at +0x44
    if (s_fn_SerializeBulk)
        s_fn_SerializeBulk(serializer, 0x30, self->_climateData, nullptr);

    // Serialize float fields at end: field104(+0x104), field108(+0x108), field10C(+0x10C)
    if (s_fn_Serialize_Float) {
        s_fn_Serialize_Float(serializer, &self->_field104, nullptr);
        s_fn_Serialize_Float(serializer, &self->_field108, nullptr);
        s_fn_Serialize_Float(serializer, &self->_field10C, nullptr);
    }

    // Destroy serializer
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(serializer);
}

// ============================================================
// ClimateGenerator::Slot8 (vtable +0x40)
// ============================================================
// Original: 0x140018f30
// Null-check: if self is null, return null.
// Otherwise, call External ctor, set vtable, set defaults, return self.
void* ClimateGenerator_Slot8(ClimateGenerator* self) {
    if (!self) return nullptr;

    ClimateGenerator_Init(self);
    return self;
}

// ============================================================
// ClimateGenerator::Init -- Initialize all fields to defaults
// ============================================================
// Mirrors ClimateGenerator_Ctor at 0x1400930c0.
// Calls Object::External::External for base init, then sets
// ClimateGenerator-specific fields to their default values.
void ClimateGenerator_Init(ClimateGenerator* self) {
    // Initialize External base class
    fn_External_External(self);

    // Set ClimateGenerator vtable
    static void* s_vtable = nullptr;
    if (!s_vtable) s_vtable = (void*)ResolveAddr(Module::Exe, ClimateGeneratorAddr::Vtable);
    self->vtable = s_vtable;

    // ExternalPtr at +0x20 = null
    self->_field20 = nullptr;
    // String at +0x28 = null (8-byte COW string)
    self->_field28 = nullptr;

    // Climate parameters
    self->_field30 = 1;              // enabled
    self->temperatureBase = 50.0f;   // base temperature
    self->tempVariation = 30.0f;     // temperature variation
    self->precipitation = 5.0f;      // precipitation amount
    self->windSpeed = 10.0f;         // wind speed

    // Trailing float fields
    self->_field104 = 0;             // zeroed (float 0.0 or uint 0)
    self->_field108 = 1.0f;
    self->_field10C = 1.0f;

    // Zero the 0xC0-byte climate data block at +0x44
    memset(self->_climateData, 0, 0xC0);
}
