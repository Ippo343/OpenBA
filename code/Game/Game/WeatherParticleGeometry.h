#pragma once
// ============================================================================
// code/Game/Game/WeatherParticleGeometry.h — ENDGAME B.3.8 block LOOP
// (B38_RULES sec. 5, "the 5 unreflected exe subclasses of Runtime classes")
//
// `WeatherParticleGeometry` (0xf0): the exe's unreflected `Graphics::Geometry`
// subclass that draws rain/snow (weather_particles.cpp). Its +0xc8 points at a
// `Weather` descriptor (Environment/*.rsc); it owns a particle array (+0xe0,
// stride 0x24) and an index buffer (+0xd0). Retail has no name for it; the
// name is the tree's.
//
// RETAIL (Application-x64.exe): table 0x140160240, 9 slots, MemAlloc(0xf0),
// ctor 0x1400d3f10.
//   [0] 0x140025060 scalar deleting dtor (body 0x1400d3c10)
//   [1] 0x1400d2e60 Update        [2] 0x1400d3120 Draw
//   [3] 0x1400d30b0 AddDrawItems  [4]..[8] Graphics::Geometry's (imports)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "graphics/Geometry.h"
#include "Action/PathSearch.h"   // RawArray<T>

class Weather;   // Game/Weather.h

// One rain / snow particle (0x24, stride of the +0xE0 array; grown by 0x140025090).
struct WeatherParticle {
    float position[3];           // +0x00
    float previousPosition[3];   // +0x0C (the streak's tail)
    float velocity[3];           // +0x18
};
static_assert(sizeof(WeatherParticle) == 0x24, "WeatherParticle stride 0x24");
static_assert(offsetof(WeatherParticle, previousPosition) == 0x0C, "WeatherParticle +0x0C");
static_assert(offsetof(WeatherParticle, velocity) == 0x18, "WeatherParticle +0x18");

class WeatherParticleGeometry final : public Graphics::Geometry {
public:
    WeatherParticleGeometry();                                        // 0x1400d3f10

    // body 0x1400d3c10
    ~WeatherParticleGeometry() override;                              // [0] 0x140025060
    void Update(const System::Time& time) override;                   // [1] 0x1400d2e60
    void Draw(Video::Context& context, const Graphics::Camera& camera,
              Graphics::ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;     // [2] 0x1400d3120
    void AddDrawItems(const Graphics::Scene& scene,
                      Graphics::Renderer& renderer) const override;   // [3] 0x1400d30b0

    // 0x1400d3f80: binds the Weather descriptor, seeds the particles and indices.
    void Configure(Weather* descriptor);


public:
    float    _intensity;          // +0xC0  0..1
    float    _fadeDirection;      // +0xC4  +1 fade in, -1 fade out
    Weather* _descriptor;         // +0xC8  ExternalPtr<Weather> (0x1400d3f80 stores it)
    RawArray<uint16_t> _indices;  // +0xD0  Array<uint16> (0x1400d3f80 sizes it by 0x14001c890), 6 per particle
    RawArray<WeatherParticle> _particles;   // +0xE0  Array<WeatherParticle> (0x140025090)
    // Raw fields (RawArray): ~WeatherParticleGeometry (0x1400d3c10) frees both by hand.
};

static_assert(sizeof(WeatherParticleGeometry) == 0xF0, "WeatherParticleGeometry is MemAlloc(0xf0)");
static_assert(offsetof(WeatherParticleGeometry, _intensity) == 0xC0, "+0xC0");
static_assert(offsetof(WeatherParticleGeometry, _fadeDirection) == 0xC4, "+0xC4");
static_assert(offsetof(WeatherParticleGeometry, _descriptor) == 0xC8, "+0xC8");
static_assert(offsetof(WeatherParticleGeometry, _indices) == 0xD0, "+0xD0");
static_assert(offsetof(WeatherParticleGeometry, _particles) == 0xE0, "+0xE0");
