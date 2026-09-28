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
//   ExternalPtr @ +0x20  (FUN_140002340 / Reimpl_ExternalPtr_Serialize_StringTable)
//   int count   @ +0x30
//   [if loading: Reimpl_WeatherArray_Resize(+0x28, count)]
//   for each array entry (stride 16): WeatherExternalSerialize(entry) + ExternalPtr(entry+8)
//   12 weather entries @ +0x38 (each: 3 Color + 5 float, all 4-byte Serialize)
// GetVersion (vtable +0x18, 0x1400941b0): mov eax,7; ret -> version 7.
// Dtor (vtable +0x00, 0x140019190): MemFree(array @+0x28) + External::~External + free.
//
// The three helpers (0x1400198a0 Reimpl_WeatherArray_Resize, 0x1400196a0
// Reimpl_ExternalPtr_Serialize_Weather, 0x140002340) are called directly (B.3R).
//
// Field knowledge harvested from engine/reimpl/environmentdescription.cpp.
// Decompiled from Application-x64.exe (READ-ONLY recon).
//
// ENDGAME B.3.8 K1: a REAL class (EnvironmentDescription.h) with a
// compiler-generated 9-slot vtable. The hand-built 11-slot table (slot 8 holding
// createInPlace, two guard slots past retail's end) and its
// RegisterExeAddr(0x14015e1f0) row are gone: nothing in the tree reads that
// address back (B38_RULES D5).

#include "Component/EnvironmentDescription.h"
#include "engine_funcs.h"
#include "io/Serializer.h"
#include "object/Internal.h"      // Object::detail::InternalMemAlloc / InternalMemFree
#include "Object/factory_registration.h"
#include <cstring>
#include <cstdint>
#include <new>
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"

namespace {
// The retail ctor stores raw IEEE bit patterns; this keeps them exact.
inline float FloatBits(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
}  // namespace

// ctor 0x1400941c0 (called by create 0x140019120 / createInPlace 0x140019160):
// `??0External@Object@@`, the stamp (the compiler's), then the defaults. The 12
// monthly entries all start from the same lighting.
EnvironmentDescription::EnvironmentDescription()
    : Object::External(),
      _environmentRef(nullptr),
      _weatherArray(nullptr),
      _weatherCount(0),
      _weatherCap(0) {
    for (WeatherEntry& w : _months) {
        w.skyColor     = 0xff595965;
        w.groundColor  = 0xffcbcbbc;
        w.fogColor     = 0xffbfbff2;
        w.fogStart     = 90.0f;
        w.fogEnd       = 225.0f;
        w.sunIntensity = FloatBits(0x3f733333);   // 0.95f
        w.sunAngle     = 120.0f;
        w.sunSpread    = 65.0f;
    }
}

// dtor (slot 0 0x140019190 minus the stamps): free the dynamic weather array.
// The compiler then runs ~External.
EnvironmentDescription::~EnvironmentDescription() {
    if (_weatherArray) ::System::MemoryAllocator::MemFree(_weatherArray);
    _weatherArray = nullptr;
    _weatherCount = 0;
    _weatherCap = 0;
}

// [5] 0x140094040.
bool EnvironmentDescription::Serialize(void* stream) {
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.8 bridge (External::Serialize takes void*)

    Reimpl_ExternalPtr_Serialize_StringTable(&_environmentRef, &ser);   // ExternalPtr +0x20

    // count through a local (the resize below owns +0x30)
    uint32_t count = _weatherCount;
    ser.Serialize(*reinterpret_cast<int*>(&count), nullptr);

    // On load: resize the dynamic weather-entry array.
    if (static_cast<int>(ser.GetDirection()) == 0) {   // IO::Serializer::GetDirection, retail 0x180006210
        Reimpl_WeatherArray_Resize(&_weatherArray, (int)count);
    }

    uintptr_t* arrPtr = static_cast<uintptr_t*>(_weatherArray);
    for (uint32_t i = 0; i < count; ++i) {
        Reimpl_ExternalPtr_Serialize_Weather(arrPtr, &ser);
        Reimpl_ExternalPtr_Serialize_StringTable(arrPtr + 1, &ser);
        arrPtr += 2;   // 16-byte entries
    }

    // 12 inline weather entries @ +0x38, each 0x20 bytes: 3 Colors + 5 floats,
    // all through the 4-byte int door.
    for (WeatherEntry& w : _months) {
        ser.Serialize(*reinterpret_cast<int*>(&w.skyColor), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.groundColor), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.fogColor), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.fogStart), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.fogEnd), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.sunIntensity), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.sunAngle), nullptr);
        ser.Serialize(*reinterpret_cast<int*>(&w.sunSpread), nullptr);
    }
    return true;
}

// [6] 0x140019170: retail destroys and rebuilds in place: a virtual `this->~T()` (retail: slot 0, flags 0),
// then the ctor again on `this` (D9 #10).
Object::External* EnvironmentDescription::Reconstruct() {
    this->~EnvironmentDescription();
    return new (static_cast<void*>(this)) EnvironmentDescription();
}

// [7] 0x140001640: retail's shared body, a virtual `this->~T()` (retail: slot 0, flags 0) (D9 #10).
void EnvironmentDescription::Destruct() { this->~EnvironmentDescription(); }

// [8] 0x140019160: placement-new on `this` (folded with createInPlace). The
// hand-built table already held createInPlace here.
Object::External* EnvironmentDescription::Construct() {
    return new (this) EnvironmentDescription();
}

// create 0x140019120: MemAlloc(0x1b8, 8), then the ctor.
void* EnvironmentDescription::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(EnvironmentDescription), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) EnvironmentDescription());
}
// createInPlace 0x140019160.
void* EnvironmentDescription::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) EnvironmentDescription());
}

DEFINE_EXE_TYPE(EnvironmentDescription, "Object::External")
