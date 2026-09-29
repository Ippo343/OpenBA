#pragma once
// ============================================================================
// code/Game/Component/ParticleComponent.h — ENDGAME B.3.8 block CIT
//
// `ParticleComponent` (a building's particle emitters: chimney smoke, fire) as a
// real C++ class with a compiler-generated vtable (B38_RULES.md D1-D4). The
// engine/types POD is a forwarder to this file. (The `Tornado` Graphics::Particle
// class is block LOOP's, not this.)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ParticleComponent", parent "Component", ti 0x1401c4720
//   create 0x14000a360: MemAlloc(0x48, 8) + ctor 0x1400533d0
//   ctor   0x1400533d0: Component ctor, stamp 0x14015a290, +0x28..+0x40 = 0,
//                       flags |= 0xc3
//   dtor   0x140053310: restamp; for each of the +0x40 geometry entries
//                       (stride 0x18 at +0x38) with a non-null object,
//                       `[vptr+0](obj, 1)`; MemFree(+0x38) + zero +0x38/+0x40;
//                       MemFree(+0x28) + zero +0x28/+0x30; stamp 0x140162920,
//                       ~Internal. Scalar 0x14000a560 = dtor + MemFree on &1
//   table  0x14015a290, 16 slots:
//     [0] 0x14000a560 scalar deleting dtor   [1]/[2] TypeName/Type
//     [3] 0x140053270 Persist                [4] 0x140053230 Startup (setup)
//     [6] 0x140052f70 OnAdded (fade-in)      [7] 0x140052e80 OnRemove
//     [8] 0x140052b30 Tick (per frame)       [11] 0x1400531b0 Slot11 (restart)
//     [12] 0x140053090 OnEvent (2/3 fade)    every other slot = Component's
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Component.h"
#include "Object/exe_type.h"

namespace Graphics { class ParticleInstance; }

// One emitter's live state: an element of ParticleComponent's +0x38 array
// (stride 0x18; resize 0x14000a640 builds {0, -1, 0, 0}; the retail dtor
// 0x140053310 destroys `instance` through its slot 0). Fields from Place
// 0x140052560, StartEmitter 0x140052a50, StartEmitterFade 0x140053660 and Tick
// 0x140052b30.
struct ParticleEmitterSlot {
    Graphics::ParticleInstance* instance;  // +0x00 the ParticleSystemInstance (Particle::CreateSystem)
    int32_t sceneId;                       // +0x08 Graphics::Scene object id, -1 = not in the scene
    int32_t nodeIndex;                     // +0x0C mesh node / attach point followed, -1 = static
    float   fadeRate;                      // +0x10 opacity units per second (0 = steady)
    int32_t _pad14;                        // +0x14
};
static_assert(sizeof(ParticleEmitterSlot) == 0x18, "ParticleComponent emitter slot stride 0x18");
static_assert(offsetof(ParticleEmitterSlot, sceneId) == 0x08, "");
static_assert(offsetof(ParticleEmitterSlot, nodeIndex) == 0x0C, "");
static_assert(offsetof(ParticleEmitterSlot, fadeRate) == 0x10, "");

class ParticleComponent final : public Component {
    DECLARE_EXE_TYPE(ParticleComponent)

public:
    ParticleComponent();                                          // 0x1400533d0

    // PORT GAP (retail 0x140053310) — B38 D2: retail destroys the geometry
    // objects and frees both arrays; the tree never did (Inert_ScalarDtor).
    ~ParticleComponent() override = default;                      // [0]  0x14000a560
    void Persist(void* stream) override;                          // [3]  0x140053270
    void Startup() override;                                      // [4]  0x140053230
    void OnAdded() override;                                      // [6]  0x140052f70
    void OnRemove() override;                                     // [7]  0x140052e80
    void Tick(void* time) override;                               // [8]  0x140052b30
    void Slot11() override;                                       // [11] 0x1400531b0
    void OnEvent(int eventId) override;                           // [12] 0x140053090
    // Fade every emitter OUT (defined in diseasecomponent.cpp, B4 COMP-c).
    void StopEmitterFade();                                       // 0x1400537f0

    // ---- non-virtual members (retail addresses) ----
    void Setup();                                                 // 0x140052480
    void Place(int index);                                        // 0x140052560
    void StopEmitter(int index);                                  // 0x140052990
    void StartEmitter(int index);                                 // 0x140052a50
    void StartEmitterFade(int index);                             // 0x140053660
    void FadeOutEmitter(int index);                               // 0x140053720 (defined in tornadocomponent.cpp, COMP-e)

    // PUBLIC while the ported bodies read them (B38 decision 1). Types from
    // Persist 0x140053270 (int count, then that many bools: Array<bool>) and the
    // dtor 0x140053310 (Array<ParticleEmitterSlot>, both freed by the retail dtor).
    bool*                active;        // +0x28 per-emitter "running" flags (Array<bool> data)
    int32_t              activeCount;   // +0x30
    int32_t              activeCapacity;// +0x34
    ParticleEmitterSlot* emitters;      // +0x38 Array<ParticleEmitterSlot> data
    int32_t              emitterCount;  // +0x40
    int32_t              emitterCapacity;// +0x44
};

static_assert(sizeof(ParticleComponent) == 0x48, "ParticleComponent is 0x48 (create 0x14000a360)");
static_assert(alignof(ParticleComponent) == 8, "ParticleComponent alignment");
static_assert(offsetof(ParticleComponent, active) == 0x28, "");
static_assert(offsetof(ParticleComponent, activeCount) == 0x30, "");
static_assert(offsetof(ParticleComponent, activeCapacity) == 0x34, "");
static_assert(offsetof(ParticleComponent, emitters) == 0x38, "");
static_assert(offsetof(ParticleComponent, emitterCount) == 0x40, "");
static_assert(offsetof(ParticleComponent, emitterCapacity) == 0x44, "");
