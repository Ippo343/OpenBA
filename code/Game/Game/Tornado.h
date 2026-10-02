#pragma once
// ============================================================================
// code/Game/Game/Tornado.h — ENDGAME B.3.8 block LOOP (B38_RULES D4 / sec. 5)
//
// `Tornado` (0x90): the tornado funnel's particle resource
// (ParticleSystems/Tornado/Tornado.rsc), a real reflected `Graphics::Particle`
// subclass, and `TornadoInstance` (0xe0): the unreflected
// `Graphics::ParticleInstance` subclass its CreateSystem builds, whose Update
// is the swirl simulation. Purely visual: tornado gameplay is TornadoComponent.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   Tornado: reflected "Tornado", parent "Graphics::Particle", table
//   0x140160190, 10 slots, sizeof 0x90, ctor 0x1400d23c0.
//     [0] 0x140024f00 scalar deleting dtor (restamp, ~Particle, MemFree on & 1)
//     [1] 0x140024e80 TypeName      [2] 0x140024e70 Type
//     [3] 0x1400d2370 GetFileVersion (Particle's | 1)
//     [4] IMPORT Object::External::Compile
//     [5] 0x1400d2210 Serialize     [6] 0x140024ee0 Reconstruct
//     [7] 0x140001640 Destruct      [8] 0x1400d21b0 CreateSystem
//     [9] 0x140024ed0 Construct
//   TornadoInstance: table 0x1401601e0, 10 slots, MemAlloc(0xe0).
//     [0] 0x140024f50 scalar deleting dtor (~ParticleInstance, MemFree on & 1)
//     [1] 0x1400d2470 Update (the swirl)   [2]..[9] ParticleInstance's (imports)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "graphics/ParticleInstance.h"
#include "graphics/Particle.h"
#include "system/Value.h"            // System::Color, System::Time
#include "Object/exe_type.h"

class Tornado final : public Graphics::Particle {
    DECLARE_EXE_TYPE(Tornado)

public:
    Tornado();                                                        // 0x1400d23c0

    // retail's body is the restamp and ~Particle
    ~Tornado() override = default;                                    // [0] 0x140024f00
    int32_t GetFileVersion() const override;                          // [3] 0x1400d2370
    bool Serialize(void* stream) override;                            // [5] 0x1400d2210
    Object::External* Reconstruct() override;                         // [6] 0x140024ee0
    void Destruct() override;                                         // [7] 0x140001640
    Graphics::ParticleInstance* CreateSystem() const override;        // [8] 0x1400d21b0
    virtual Object::External* Construct();                            // [9] 0x140024ed0

public:
    // Names are the ToolKit ones (Tornado.rsc lists the 15 fields Serialize reads).
    float    _tornadoWidth;          // +0x48  4
    float    _tornadoHeight;         // +0x4c  20
    float    _funnelWidth;           // +0x50  5
    float    _funnelSpin;            // +0x54  45 (degrees/sec)
    float    _particleSpinSpeed;     // +0x58  1
    float    _emissionRate;          // +0x5c  1
    float    _particleLifeTime;      // +0x60  10
    float    _particleLifeTimeTol;   // +0x64  0
    float    _particleStartSize;     // +0x68  0.2
    float    _particleEndSize;       // +0x6c  3
    float    _particleScaleSize;     // +0x70  30
    System::Color _startColor;       // +0x74  0xFFFFFFFF (Serialize: Color)
    System::Color _endColor;         // +0x78  0xFFFFFFFF (Serialize: Color)
    uint32_t _pad7c;                 // +0x7c
    System::Time  _particleFadeIn;   // +0x80  (Serialize: Time; ctor Time(1.0f))
    System::Time  _particleFadeOut;  // +0x88  (Serialize: Time; ctor Time(1.0f))
};

static_assert(sizeof(Tornado) == 0x90, "Tornado is MemAlloc(0x90)");
static_assert(offsetof(Tornado, _tornadoWidth) == 0x48, "+0x48");
static_assert(offsetof(Tornado, _funnelSpin) == 0x54, "+0x54");
static_assert(offsetof(Tornado, _emissionRate) == 0x5c, "+0x5c");
static_assert(offsetof(Tornado, _particleScaleSize) == 0x70, "+0x70");
static_assert(offsetof(Tornado, _startColor) == 0x74, "+0x74");
static_assert(offsetof(Tornado, _endColor) == 0x78, "+0x78");
static_assert(offsetof(Tornado, _particleFadeIn) == 0x80, "+0x80");
static_assert(offsetof(Tornado, _particleFadeOut) == 0x88, "+0x88");

class TornadoInstance final : public Graphics::ParticleInstance {
public:
    TornadoInstance();                                                // inline in 0x1400d21b0

    // Keep ParticleInstance's non-virtual `bool Update()` visible beside slot 1.
    using Graphics::ParticleInstance::Update;

    // retail's body is ~ParticleInstance
    ~TornadoInstance() override = default;                            // [0] 0x140024f50
    void Update(const System::Time& time) override;                   // [1] 0x1400d2470

public:
    float    _funnelSpinDegrees;     // +0xd8  accumulated funnel rotation (int 0 in the ctor)
    uint32_t _padDC;                 // +0xdc
};

static_assert(sizeof(TornadoInstance) == 0xe0, "TornadoInstance is MemAlloc(0xe0)");
static_assert(offsetof(TornadoInstance, _funnelSpinDegrees) == 0xd8, "+0xd8");
