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

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    field28;      // +0x28 per-emitter bool array data (freed by the retail dtor)
    void*    field30;      // +0x30 its count / capacity
    void*    field38;      // +0x38 geometry entries (0x18 stride; objects destroyed by the retail dtor)
    void*    field40;      // +0x40 their count / capacity
};

static_assert(sizeof(ParticleComponent) == 0x48, "ParticleComponent is 0x48 (create 0x14000a360)");
static_assert(alignof(ParticleComponent) == 8, "ParticleComponent alignment");
static_assert(offsetof(ParticleComponent, field28) == 0x28, "");
static_assert(offsetof(ParticleComponent, field30) == 0x30, "");
static_assert(offsetof(ParticleComponent, field38) == 0x38, "");
static_assert(offsetof(ParticleComponent, field40) == 0x40, "");
