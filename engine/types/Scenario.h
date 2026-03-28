#pragma once
// Scenario — Game scenario (extends Achievement::Achievement), binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x90.
//
// Decompiled from: Scenario_Factory (0x14001e9f0), Scenario_Slot5 (0x1400a9370)
// Inherits: Achievement::Achievement (0x58 bytes) -> Scenario
//
// Serialize order from Slot5:
//   Achievement base serialize, then:
//   int +0x60, int +0x64, String +0x58, ExtPtr<MapGen> +0x70,
//   ExtPtr<TerrainGen> +0x78, ExtPtr<ClimateGen> +0x80, String +0x88,
//   bool +0x68, bool +0x69

#include <cstdint>
#include <cstddef>

namespace ScenarioAddr {
    constexpr uintptr_t Factory      = 0x14001e9f0;
    constexpr uintptr_t Vtable       = 0x14015f020;
    constexpr uintptr_t RegisterType = 0x14014cd10;
    constexpr uintptr_t Cleanup      = 0x14001ea70;  // Slot8/Init
    constexpr uintptr_t Dtor         = 0x14001eb40;
    constexpr uintptr_t Slot3        = 0x1400a9460;  // GetFileVersion
    constexpr uintptr_t Slot5        = 0x1400a9370;  // Serialize
    constexpr uintptr_t Slot6        = 0x14001ead0;  // Cleanup (dtor+reinit)
}

struct Scenario {
    // --- Achievement base (0x58 bytes, constructed by Achievement::Achievement) ---
    void*    vtable;             // +0x00: -> 0x14015f020
    uint8_t  _achievBase[0x50];  // +0x08: Achievement base fields (External + goal lists etc.)

    // --- Scenario fields ---
    void*    scenarioName;       // +0x58: System::String (COW refcounted)
    int32_t  _field60;           // +0x60: int, serialized
    int32_t  _field64;           // +0x64: int, serialized
    bool     _field68;           // +0x68: bool, serialized (default true)
    bool     _field69;           // +0x69: bool, serialized (default true)
    uint8_t  _pad6A[6];          // +0x6A: padding
    void*    mapGenerator;       // +0x70: ExternalPtr<MapGenerator>, serialized
    void*    terrainGenerator;   // +0x78: ExternalPtr<TerrainGenerator>, serialized
    void*    climateGenerator;   // +0x80: ExternalPtr<ClimateGenerator>, serialized
    void*    _field88;           // +0x88: System::String (COW refcounted)
};

static_assert(sizeof(Scenario) == 0x90, "Scenario size mismatch");
static_assert(offsetof(Scenario, vtable) == 0x00, "");
static_assert(offsetof(Scenario, scenarioName) == 0x58, "");
static_assert(offsetof(Scenario, _field60) == 0x60, "");
static_assert(offsetof(Scenario, _field64) == 0x64, "");
static_assert(offsetof(Scenario, _field68) == 0x68, "");
static_assert(offsetof(Scenario, _field69) == 0x69, "");
static_assert(offsetof(Scenario, mapGenerator) == 0x70, "");
static_assert(offsetof(Scenario, terrainGenerator) == 0x78, "");
static_assert(offsetof(Scenario, climateGenerator) == 0x80, "");
static_assert(offsetof(Scenario, _field88) == 0x88, "");
