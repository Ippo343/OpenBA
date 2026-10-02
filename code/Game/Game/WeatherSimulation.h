#pragma once
// ============================================================================
// code/Game/Game/WeatherSimulation.h — ENDGAME B.4 GAME-a
//
// The 0x78-byte weather simulation at GameUpdateLoop+0x130 (m_weather): the
// temperature the citizens and crops read, the rain / snow particle effects,
// the lighting blend and the disaster roll. NOT the `Weather` resource
// (Weather.h: the reflected 0x80-byte rain / snow description).
//
// CLASS NAME UNCONFIRMED: retail gives it no reflected name or vtable; its
// source is Game/Game/Weather.cpp (profile build), whose reflected class is
// the resource, so the simulation's own name is not known. Descriptive name.
// No vtable: a plain struct.
//
// RETAIL (Application-x64.exe):
//   ctor 0x140093f60: +0x00/+0x08 = 0, +0x10 = 0, +0x14 = 4, +0x18 = 0,
//     +0x1C..+0x33 = 0, PerlinNoise(+0x34, 0.25, 4, rand, rand), +0x44/+0x48
//     = 0, +0x4C = -1, +0x50..+0x77 = 0
//   Bind 0x140093e30(this, env, climate): +0x00 = env, +0x08 = climate; sizes
//     +0x50 (0x1400191f0) and +0x60 (0x1400032e0) to env+0x30; +0x2C = +0x30
//     = climate+0x34
//   Serialize 0x140093c90: ExternalPtr +0x00 (0x1400192a0), +0x08
//     (0x1400194a0); int +0x10, int +0x14, float +0x18, Vector4 +0x1C, float
//     +0x2C, +0x30, +0x44, +0x48, int +0x4C; per effect: a bool, then floats
//     effect+0xC4 / +0xC0
//   Update 0x1400944e0: ticks the effects (+0x50) and their emitters (+0x60);
//     +0x2C / +0x44 ease toward +0x30 / +0x48; +0x4C = the 48-step season
//     index from Calendar::yearTime; +0x70 += dt feeds the disaster roll
//     (0x1400a9290 against climate+0x108) while +0x18 is above
//     DAT_1401721d8 and +0x10 is 0
//   dtor 0x140093eb0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "system/Value.h"            // System::Time (8 bytes, trivial)
#include "utility/PerlinNoise.h"     // RtUtility::PerlinNoise (0x10, trivial)

class EnvironmentDescription;   // Component/EnvironmentDescription.h
class ClimateGenerator;         // Game/ClimateGenerator.h
class ComponentEntity;
namespace Graphics { class Geometry; }
namespace IO { class Stream; }

class WeatherSimulation {
public:
    EnvironmentDescription* environment;     // +0x00 ExternalPtr ("Environment/Environment.rsc")
    ClimateGenerator*       climate;         // +0x08 ExternalPtr (GameUpdateLoop::m_climateConfig)
    int32_t                 field_0x10;      // +0x10 (Serialize int); non-zero stops snow and the disaster roll
    int32_t                 field_0x14;      // +0x14 ctor 4 (Serialize int)
    float                   temperature;     // +0x18 (Serialize float); CitizenComponent::Update's cold test
    float                   field_0x1C[4];   // +0x1C Math::Vector4 (Vector4::Serialize)
    float                   field_0x2C;      // +0x2C eases toward field_0x30 (Update)
    float                   field_0x30;      // +0x30 target (Bind: climate+0x34)
    RtUtility::PerlinNoise  noise;           // +0x34 ctor PerlinNoise(0.25, 4, rand, rand)
    float                   field_0x44;      // +0x44 eases toward field_0x48 (Update)
    float                   field_0x48;      // +0x48
    int32_t                 seasonStep;      // +0x4C 0..47 from Calendar::yearTime (ctor -1)
    Graphics::Geometry**    effects;         // +0x50 Array data: the weather particle effects (env+0x30 of them)
    int32_t                 effectCount;     // +0x58
    int32_t                 effectCapacity;  // +0x5C
    ComponentEntity**       emitters;        // +0x60 Array<InternalPtr<ComponentEntity>> data: each effect's sound entity
    int32_t                 emitterCount;    // +0x68
    int32_t                 emitterCapacity; // +0x6C
    System::Time            disasterTime;    // +0x70 += dt (Update's disaster roll)

    // ---- members (B4 GAME-a; bodies in gameupdateloop_serialize.cpp) ----
    void Serialize(IO::Stream& stream);   // 0x140093c90
    // 0x140093e30: binds the environment (its weather descriptors size the two
    // arrays) and the climate (+0x34 seeds +0x2C / +0x30). Body in gameupdateloop_load.cpp.
    void Bind(EnvironmentDescription* env, ClimateGenerator* climate);
    // B.4 GAME-c (bodies in weather_sim.cpp / weather_particles.cpp)
    void Update(const System::Time* time);              // 0x1400944e0 (the per-tick sim)
    void SetEffectEnabled(int slot, bool enable);       // 0x1400942c0
};

static_assert(sizeof(WeatherSimulation) == 0x78, "WeatherSimulation is 0x78 (Load MemAlloc(0x78, 8))");
static_assert(offsetof(WeatherSimulation, environment) == 0x00, "WeatherSimulation::environment @ +0x00");
static_assert(offsetof(WeatherSimulation, climate) == 0x08, "WeatherSimulation::climate @ +0x08");
static_assert(offsetof(WeatherSimulation, field_0x10) == 0x10, "WeatherSimulation +0x10");
static_assert(offsetof(WeatherSimulation, field_0x14) == 0x14, "WeatherSimulation +0x14");
static_assert(offsetof(WeatherSimulation, temperature) == 0x18, "WeatherSimulation::temperature @ +0x18");
static_assert(offsetof(WeatherSimulation, field_0x1C) == 0x1C, "WeatherSimulation +0x1C");
static_assert(offsetof(WeatherSimulation, field_0x2C) == 0x2C, "WeatherSimulation +0x2C");
static_assert(offsetof(WeatherSimulation, field_0x30) == 0x30, "WeatherSimulation +0x30");
static_assert(offsetof(WeatherSimulation, noise) == 0x34, "WeatherSimulation::noise @ +0x34");
static_assert(offsetof(WeatherSimulation, field_0x44) == 0x44, "WeatherSimulation +0x44");
static_assert(offsetof(WeatherSimulation, field_0x48) == 0x48, "WeatherSimulation +0x48");
static_assert(offsetof(WeatherSimulation, seasonStep) == 0x4C, "WeatherSimulation::seasonStep @ +0x4C");
static_assert(offsetof(WeatherSimulation, effects) == 0x50, "WeatherSimulation::effects @ +0x50");
static_assert(offsetof(WeatherSimulation, effectCount) == 0x58, "WeatherSimulation::effectCount @ +0x58");
static_assert(offsetof(WeatherSimulation, effectCapacity) == 0x5C, "WeatherSimulation::effectCapacity @ +0x5C");
static_assert(offsetof(WeatherSimulation, emitters) == 0x60, "WeatherSimulation::emitters @ +0x60");
static_assert(offsetof(WeatherSimulation, emitterCount) == 0x68, "WeatherSimulation::emitterCount @ +0x68");
static_assert(offsetof(WeatherSimulation, emitterCapacity) == 0x6C, "WeatherSimulation::emitterCapacity @ +0x6C");
static_assert(offsetof(WeatherSimulation, disasterTime) == 0x70, "WeatherSimulation::disasterTime @ +0x70");
