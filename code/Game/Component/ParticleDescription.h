#pragma once
// ============================================================================
// code/Game/Component/ParticleDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ParticleDescription", parent "Description", ti 0x1401c4758
//   create 0x14000a3c0: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000a430.
//   table 0x14015a310, 10 slots:
//     [0] 0x14000a500 scalar deleting dtor   [1] 0x14000a3b0 TypeName   [2] 0x14000a3a0 Type
//     [3] 0x1400535b0 GetFileVersion = 7   [4] External::Compile (import, inherited)
//     [5] 0x140053460 Serialize (`MOV AL,1`)
//     [6] 0x14000a490 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000a4f0 GetComponentType: ParticleComponent's TypeId
//     [9] 0x14000a430 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/String.h"   // System::String (ParticleEmitterDescription::name)
#include "system/Value.h"    // System::Time

namespace Graphics { class Particle; }
namespace Pathfinding { class PointList; }

// One emitter of a ParticleDescription (+0x28 array, stride 0x28), from Serialize
// 0x140053460 (ExternalPtr, String, four bools, Time +0x20 then Time +0x18) and
// what ParticleComponent does with each field. The array is raw MemAlloc storage
// whose elements Serialize builds field by field (and the dtor releases `name`),
// so nothing constructs or destroys this struct as a whole.
struct ParticleEmitterDescription {
    Graphics::Particle* particle;       // +0x00 ExternalPtr<Particle> (CreateSystem, SetParticle)
    System::String      name;           // +0x08 attach point / mesh node name
    bool                followNode;     // +0x10 track the node every tick (default 0)
    bool                autoStart;      // +0x11 started by OnAdded (default 1)
    bool                eventToggled;   // +0x12 started / faded by OnEvent 2 / 3 (default 1)
    bool                noModelTransform;// +0x13 place at the map footprint, not the model (default 0)
    uint8_t             _pad14[4];      // +0x14
    System::Time        prewarm;        // +0x18 ticked forward on Place (default 0)
    System::Time        fadeTime;       // +0x20 fade in / out duration (default 1 s)
};
static_assert(sizeof(ParticleEmitterDescription) == 0x28, "ParticleDescription emitter stride 0x28");
static_assert(offsetof(ParticleEmitterDescription, name) == 0x08, "");
static_assert(offsetof(ParticleEmitterDescription, followNode) == 0x10, "");
static_assert(offsetof(ParticleEmitterDescription, autoStart) == 0x11, "");
static_assert(offsetof(ParticleEmitterDescription, eventToggled) == 0x12, "");
static_assert(offsetof(ParticleEmitterDescription, noModelTransform) == 0x13, "");
static_assert(offsetof(ParticleEmitterDescription, prewarm) == 0x18, "");
static_assert(offsetof(ParticleEmitterDescription, fadeTime) == 0x20, "");

class ParticleDescription final : public Description {
    DECLARE_EXE_TYPE(ParticleDescription)

public:
    ParticleDescription();

    ~ParticleDescription() override;                                       // [0] 0x14000a500
    int32_t           GetFileVersion() const override { return 7; }    // [3] 0x1400535b0
    bool              Serialize(void* stream) override;                    // [5] 0x140053460
    Object::External* Reconstruct() override;                              // [6] 0x14000a490
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000a4f0
    virtual Object::External* Construct();                                 // [9] 0x14000a430

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    Pathfinding::PointList* _attachPoints;      // +0x20 ExternalPtr<Pathfinding::PointList> (FUN_14000ab20 type-checks it)
    ParticleEmitterDescription* _emitters;      // +0x28 Array<ParticleEmitterDescription> data
    int32_t _emitterCount;                      // +0x30
    int32_t _emitterCap;                        // +0x34
};

static_assert(sizeof(ParticleDescription) == 0x38, "ParticleDescription is 0x38 (create 0x14000a3c0 MemAlloc(0x38))");
static_assert(alignof(ParticleDescription) == 8, "ParticleDescription alignment");
static_assert(offsetof(ParticleDescription, _attachPoints) == 0x20, "ParticleDescription::_attachPoints @ +0x20");
static_assert(offsetof(ParticleDescription, _emitters) == 0x28, "ParticleDescription::_emitters @ +0x28");
static_assert(offsetof(ParticleDescription, _emitterCount) == 0x30, "ParticleDescription::_emitterCount @ +0x30");
static_assert(offsetof(ParticleDescription, _emitterCap) == 0x34, "ParticleDescription::_emitterCap @ +0x34");
