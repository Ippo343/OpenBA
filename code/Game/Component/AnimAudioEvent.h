#pragma once
// ============================================================================
// code/Game/Component/AnimAudioEvent.h — ENDGAME B.3.8 block VIS
//
// `AnimAudioEvent` as a real C++ class with a compiler-generated vtable
// (B38_RULES.md D1-D5, §2.2), a subclass of the Runtime's real
// `Graphics::AnimationEvent` (block RB).
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AnimAudioEvent", parent "Graphics::AnimationEvent", ti 0x1401c58d8
//   create 0x140016f00: MemAlloc(0x30, 8), `??0AnimationEvent@Graphics@@`,
//                       stamp 0x14015e0c0, +0x20 = 0, +0x28 = 1.0f
//   createInPlace 0x140016f60 (= slot 9, Construct)
//   table 0x14015e0c0, 10 slots:
//     [0] 0x140017000 scalar deleting dtor (restamp, ~AnimationEvent, MemFree on &1)
//     [1] 0x140016ef0 TypeName   [2] 0x140016ee0 Type
//     [3] 0x140087f30 GetFileVersion = 2
//     [4] External::Compile (import, inherited)
//     [5] 0x140087ed0 Serialize (ExternalPtr +0x20, float +0x28; returns 1)
//     [6] 0x140016fb0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140087e40 the event's action (plays the sound when a random roll
//         is under +0x28) — AnimationEvent's pure slot 8, which the Runtime
//         header names `Construct` (a misnomer, see below) — PORT GAP
//     [9] 0x140016f60 Construct (folded with createInPlace, B38_RULES §2.2)
//   sizeof 0x30.
//
// NAME CLASH: runtime/types/graphics/Animation.h calls AnimationEvent's pure
// slot 8 `Construct()`. Retail's slot 8 of this leaf is the audio trigger
// (0x140087e40, RDX = the event's transform) and the leaf's OWN Construct is
// slot 9 (§2.2's triad). The override below keeps the Runtime's spelling for
// slot 8 and names slot 9 `ConstructInPlace`; renaming the Runtime slot is the
// Runtime owner's call.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "graphics/Animation.h"
#include "Object/exe_type.h"

class AnimAudioEvent final : public Graphics::AnimationEvent {
    DECLARE_EXE_TYPE(AnimAudioEvent)

public:
    AnimAudioEvent();                                                    // inlined in 0x140016f00

    ~AnimAudioEvent() override = default;                                // [0] 0x140017000
    int32_t           GetFileVersion() const override { return 2; }      // [3] 0x140087f30
    bool              Serialize(void* stream) override;                  // [5] 0x140087ed0
    Object::External* Reconstruct() override;                            // [6] 0x140016fb0
    void              Destruct() override;                               // [7] 0x140001640
    void              Construct(const Math::Transform& transform) override; // [8] 0x140087e40 PORT GAP (the trigger)
    virtual Object::External* ConstructInPlace();                        // [9] 0x140016f60

    // PUBLIC while the ported bodies read them (B38 decision 1).
    void*    _sound;       // +0x20 ExternalPtr: the sound resource (ctor 0)
    float    _chance;      // +0x28 play probability (ctor 1.0f)
    uint8_t  _pad2c[4];    // +0x2c
};

static_assert(sizeof(AnimAudioEvent) == 0x30, "AnimAudioEvent is 0x30 (create 0x140016f00)");
static_assert(alignof(AnimAudioEvent) == 8, "AnimAudioEvent alignment");
static_assert(offsetof(AnimAudioEvent, _sound) == 0x20, "AnimAudioEvent +0x20");
static_assert(offsetof(AnimAudioEvent, _chance) == 0x28, "AnimAudioEvent +0x28");
