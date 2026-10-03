#pragma once
// ============================================================================
// code/Game/Component/EnvironmentDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md
// D1-D5, §2.2)
//
// `EnvironmentDescription`, the per-map weather/lighting environment, as a real
// C++ class. It derives Object::External DIRECTLY (not a `Description`: no
// GetComponentType slot).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "EnvironmentDescription", parent "Object::External", ti 0x1401c5980
//   create 0x140019120: MemAlloc(0x1b8, 8), ctor 0x1400941c0 (`??0External@Object@@`,
//   the stamp, the defaults). createInPlace 0x140019160.
//   table 0x14015e1f0, 9 slots:
//     [0] 0x140019190 scalar deleting dtor   [1] 0x140019110 TypeName
//     [2] 0x140019100 Type                   [3] 0x1400941b0 GetFileVersion = 7
//     [4] External::Compile (import, inherited)
//     [5] 0x140094040 Serialize              [6] 0x140019170 Reconstruct
//     [7] 0x140001640 Destruct (shared)      [8] 0x140019160 Construct
//         (the leaf-introduced last slot, folded with createInPlace)
//   sizeof 0x1b8.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class ComponentDescription;   // Component/ComponentDescription.h
class Weather;                // Game/Weather.h

class EnvironmentDescription final : public Object::External {
    DECLARE_EXE_TYPE(EnvironmentDescription)

public:
    // One month's lighting (0x20 bytes): three packed ARGB colours, five floats.
    struct WeatherEntry {
        uint32_t skyColor;       // +0x00 default 0xff595965
        uint32_t groundColor;    // +0x04 default 0xffcbcbbc
        uint32_t fogColor;       // +0x08 default 0xffbfbff2
        float    fogStart;       // +0x0c default 90.0f
        float    fogEnd;         // +0x10 default 225.0f
        float    sunIntensity;   // +0x14 default 0.95f (bits 0x3f733333)
        float    sunAngle;       // +0x18 default 120.0f
        float    sunSpread;      // +0x1c default 65.0f
    };

    // One weather choice (0x10 bytes; Serialize 0x140094040: ExternalPtr<Weather>
    // 0x1400196a0, then ExternalPtr<ComponentDescription> 0x140002340).
    struct WeatherChoice {
        Weather*              weather;       // +0x00
        ComponentDescription* description;   // +0x08
    };

    EnvironmentDescription();

    ~EnvironmentDescription() override;                                    // [0] 0x140019190
    int32_t           GetFileVersion() const override { return 7; }        // [3] 0x1400941b0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x140094040
    Object::External* Reconstruct() override;                              // [6] 0x140019170
    void              Destruct() override;                                 // [7] 0x140001640
    virtual Object::External* Construct();                                 // [8] 0x140019160

    // ---- fields ----
    ComponentDescription* _environmentRef;   // +0x20 ExternalPtr<ComponentDescription> (0x140002340)
    WeatherChoice* _weatherArray;   // +0x28 Array<WeatherChoice> data (resize 0x1400198a0)
    int32_t      _weatherCount;     // +0x30 Serialize(int&) through a local
    int32_t      _weatherCap;       // +0x34
    WeatherEntry _months[12];       // +0x38..+0x1b8
};

static_assert(sizeof(EnvironmentDescription::WeatherEntry) == 0x20, "EnvironmentDescription::WeatherEntry is 0x20");
static_assert(sizeof(EnvironmentDescription::WeatherChoice) == 0x10, "EnvironmentDescription::WeatherChoice is 0x10");
static_assert(sizeof(EnvironmentDescription) == 0x1b8, "EnvironmentDescription is 0x1b8 (create 0x140019120 MemAlloc(0x1b8))");
static_assert(alignof(EnvironmentDescription) == 8, "EnvironmentDescription alignment");
static_assert(offsetof(EnvironmentDescription, _environmentRef) == 0x20, "EnvironmentDescription::_environmentRef @ +0x20");
static_assert(offsetof(EnvironmentDescription, _weatherArray)   == 0x28, "EnvironmentDescription::_weatherArray @ +0x28");
static_assert(offsetof(EnvironmentDescription, _weatherCount)   == 0x30, "EnvironmentDescription::_weatherCount @ +0x30");
static_assert(offsetof(EnvironmentDescription, _weatherCap)     == 0x34, "EnvironmentDescription::_weatherCap @ +0x34");
static_assert(offsetof(EnvironmentDescription, _months)         == 0x38, "EnvironmentDescription::_months @ +0x38");
