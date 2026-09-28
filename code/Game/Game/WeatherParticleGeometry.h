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

class WeatherParticleGeometry final : public Graphics::Geometry {
public:
    WeatherParticleGeometry();                                        // 0x1400d3f10

    Graphics::Geometry* DeletingDtor(unsigned flags) override;        // [0] 0x140025060
    void Update(const System::Time& time) override;                   // [1] 0x1400d2e60
    void Draw(Video::Context& context, const Graphics::Camera& camera,
              Graphics::ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;     // [2] 0x1400d3120
    void AddDrawItems(const Graphics::Scene& scene,
                      Graphics::Renderer& renderer) const override;   // [3] 0x1400d30b0


protected:
    ~WeatherParticleGeometry();                                       // body 0x1400d3c10

public:
    float    _intensity;          // +0xC0  0..1
    float    _fadeDirection;      // +0xC4  +1 fade in, -1 fade out
    void*    _descriptor;         // +0xC8  Weather*
    void*    _indices;            // +0xD0  uint16 index block (6 per particle)
    uint64_t _indexCount;         // +0xD8
    void*    _particles;          // +0xE0  {data,
    uint64_t _particleCountCap;   // +0xE8   count|cap}, stride 0x24
};

static_assert(sizeof(WeatherParticleGeometry) == 0xF0, "WeatherParticleGeometry is MemAlloc(0xf0)");
static_assert(offsetof(WeatherParticleGeometry, _intensity) == 0xC0, "+0xC0");
static_assert(offsetof(WeatherParticleGeometry, _fadeDirection) == 0xC4, "+0xC4");
static_assert(offsetof(WeatherParticleGeometry, _descriptor) == 0xC8, "+0xC8");
static_assert(offsetof(WeatherParticleGeometry, _indices) == 0xD0, "+0xD0");
static_assert(offsetof(WeatherParticleGeometry, _particles) == 0xE0, "+0xE0");
