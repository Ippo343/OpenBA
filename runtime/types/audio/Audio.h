#pragma once
// ============================================================================
// runtime/types/audio/Audio.h — Audio::Scene / Audio::Configuration layouts
// ============================================================================
//
// Wave 8, lane C of the Runtime-x64.dll port (docs/runtime_port/PLAN.md §4.2,
// docs/runtime_port/WAVE8.md, docs/runtime_port/WAVE8_LANE_C.md). Every offset
// below was read out of a decompiled+disassembled Runtime-x64.dll function
// (image base 0x180000000, read 2026-09-07); the function and field access
// that pins it is named in the comment beside it.
//
// PORTABILITY: no <windows.h>, per PLAN.md §4.1.
//
// WHAT Audio::Scene IS
// ---------------------
// One live audio scene: a fixed-size table of up to 256 concurrently playing
// "Channels" (2D/general sound-effect handles allocated by PlayEffect) and a
// SEPARATE fixed-size table of up to 4096 "EffectSlots" that is a single,
// id-keyed store shared by TWO different callers:
//   - Audio::Scene::AddEmitter/MoveEmitter/RemoveEmitter/SetEmitterVolume/
//     GetEmitterCount, which use the caller-supplied `id` directly as a
//     bucket key (ambient/building emitters — one slot per emitter GROUP,
//     with a NESTED array of individual instance positions+volumes hanging
//     off the slot: `EffectSlot::positions`); and
//   - Audio::Scene::PlayEffect3D/FindEffect3D/StopEffect3D, which allocate a
//     fresh id from `Scene::_playGeneration` and use ONE EffectSlot per
//     playing 3D sound effect (`EffectSlot::soundEffectRef`, `::pos`).
// Nothing distinguishes the two USES of an EffectSlot at the type level —
// retail really does share the table — so this header carries every field
// either caller writes and leaves it to the .cpp to know which half it uses.
//
// ID / HANDLE ENCODING (two independent 32-bit handle spaces; do not confuse
// them — they index DIFFERENT tables with DIFFERENT bit layouts):
//   Channel handle   (PlayEffect,     IsEffectPlaying):
//       bits 0     = 0 (never set on this side)
//       bits 1..8  = channel index (0..255), i.e. `index << 1`
//       bits 9..31 = Scene::_playGeneration at allocation time
//     Validity check used throughout: `(storedTag ^ handle) & 0xfffffe00 == 0`
//     (top 23 bits — index bit 8 plus the whole generation — must match).
//   EffectSlot id    (AddEmitter's caller-supplied id, PlayEffect3D,
//                      FindEffect3D/StopEffect3D/GetEmitterCount):
//       bits 0      = 1 for a PlayEffect3D-allocated slot, 0 for an
//                      AddEmitter-owned emitter-group id (never checked by
//                      this bit alone; AddEmitter's caller picks its own ids)
//       bits 1..12  = slot index (0..4095), i.e. `index << 1`
//       bits 13..31 = Scene::_playGeneration at allocation time
//     Validity check: `(storedTag ^ id) & 0xffffe000 == 0` (top 19 bits).
//
// EVIDENCE (retail Runtime-x64.dll addresses):
//   ??0Scene@Audio@@QEAA@XZ                 0x1800e0160  ctor: zeroes fields,
//       grows _effects to 0x1000 (FUN_180051390), _channels to 0x100
//       (FUN_180051020). NEITHER OF THOSE HELPERS' FIRST WRITE IS A VTABLE
//       POINTER — `*(undefined8*)this = 0` is the very first store and it is
//       Scene::_time, not a vtable slot. Consistent with no `??_7Scene@Audio@@`
//       export.
//   ??1Scene@Audio@@QEAA@XZ                 0x1800e0120  dtor.
//   FUN_180052110 (nested-array teardown, called from ~Scene on _effects
//       before freeing the outer buffer) frees `EffectSlot::positions._data`
//       for every live slot — the ONE owned pointer inside EffectSlot.
//   ?AddEmitter@Scene@Audio@@…               0x1800dedf0
//   ?MoveEmitter@Scene@Audio@@…               0x1800dec40
//   ?RemoveEmitter@Scene@Audio@@…             0x1800decf0
//   ?SetEmitterVolume@Scene@Audio@@…          0x1800ded60
//   ?GetEmitterCount@Scene@Audio@@…           0x1800dec00
//   ?FindEffect3D@Scene@Audio@@…              0x1800dee70
//   ?PlayEffect@Scene@Audio@@…                0x1800def60  (the REAL, QEAA-
//       spelled export; see rt_audio_port.cpp's phantom-name note)
//   ?PlayEffect3D@Scene@Audio@@…               0x1800deeb0
//   ?IsEffectPlaying@Scene@Audio@@…            0x1800df680
//   FUN_180051aa0 = Collection::Array<Vector4>::SetSize(n, fillValue) — the
//       SAME growth policy documented in Collection.h's Array<T> comment,
//       independently confirmed a third time here (two more instantiations
//       already cross-checked there).
//   ?Save@Configuration@Audio@@…               0x1800dde10
//   ?Startup@Configuration@Audio@@…            0x1800ddee0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "../collection/Collection.h"

namespace RtAudio {

// ============================================================
// Audio::Scene::Channel — one 2D/general playback slot, 0x70 (112) bytes.
// Pinned by Scene::PlayEffect (writes +0x00/+0x08/+0x10/+0x68), Scene::Update
// (+0x18 volume, +0x1c pitch, +0x6c "started" flag), Scene::StopEffect /
// Scene::PauseEffect (+0x20..+0x5f eight platform-channel pointers, +0x60..
// +0x67 eight per-sub-channel flag bytes — the default-construct pattern in
// FUN_180051020 zeroes all of +0x00..+0x67 and +0x6c..+0x6f but sets +0x18
// and +0x1c to 1.0f, matching StopEffect's identical reset pattern).
// ============================================================
struct Channel {
    uint64_t soundEffectRef;      // +0x00  Object::ExternalPtr<SoundEffect>
    uint32_t soundType;           // +0x08  Audio::Scene::SoundType
    uint32_t _pad0c;              // +0x0c  never read/written (alignment)
    int64_t  startTime;           // +0x10  System::Time ticks
    float    volume;              // +0x18  default 1.0f
    float    pitch;               // +0x1c  default 1.0f
    void*    platformChannel[8];  // +0x20  up to 8 live Audio::Device sub-channels
    uint8_t  channelFlags[8];     // +0x60  one byte per platformChannel slot
    uint32_t handleTag;           // +0x68  see the handle-encoding note above
    uint8_t  started;             // +0x6c  "has been given a start reference tick"
    uint8_t  _pad6d[3];           // +0x6d..+0x6f
};
static_assert(sizeof(Channel) == 0x70, "Audio::Scene::Channel must be 0x70 bytes");
static_assert(offsetof(Channel, soundEffectRef) == 0x00, "Channel::soundEffectRef @ +0x00");
static_assert(offsetof(Channel, soundType)      == 0x08, "Channel::soundType @ +0x08");
static_assert(offsetof(Channel, startTime)      == 0x10, "Channel::startTime @ +0x10");
static_assert(offsetof(Channel, volume)         == 0x18, "Channel::volume @ +0x18");
static_assert(offsetof(Channel, pitch)          == 0x1c, "Channel::pitch @ +0x1c");
static_assert(offsetof(Channel, platformChannel) == 0x20, "Channel::platformChannel @ +0x20");
static_assert(offsetof(Channel, channelFlags)   == 0x60, "Channel::channelFlags @ +0x60");
static_assert(offsetof(Channel, handleTag)      == 0x68, "Channel::handleTag @ +0x68");
static_assert(offsetof(Channel, started)        == 0x6c, "Channel::started @ +0x6c");

// A {x,y,z,volume} instance record inside EffectSlot::positions. Pinned by
// AddEmitter (builds this exact 16-byte value on the stack before calling
// the array SetSize/insert) and MoveEmitter/SetEmitterVolume/RemoveEmitter,
// which all stride the nested array in 16-byte (4-float) steps.
struct EmitterInstance {
    float x, y, z;
    float volume;
};
static_assert(sizeof(EmitterInstance) == 0x10, "EmitterInstance must be 16 bytes");

// ============================================================
// Audio::Scene::EffectSlot — one id-keyed slot in the 4096-entry table,
// 0x40 (64) bytes. Shared by the ambient-emitter API (AddEmitter/…, which use
// `positions`) and the 3D-effect API (PlayEffect3D/…, which use `pos`,
// `soundEffectRef`, `channelHandle`). See the file header for which fields
// which caller uses.
// ============================================================
struct EffectSlot {
    float               posX, posY, posZ;  // +0x00  PlayEffect3D's own position
    uint32_t             soundType;         // +0x0c
    uint64_t             soundEffectRef;     // +0x10  ExternalPtr<SoundEffect>
    int64_t               startTime;          // +0x18  System::Time ticks
    uint32_t               channelHandle;      // +0x20  into the Channel table, 0 = none
    uint32_t               idTag;              // +0x24  see the handle-encoding note above
    float                   baseVolume;         // +0x28  default 1.0f
    uint32_t                 _pad2c;             // +0x2c
    Collection::Array<EmitterInstance> positions; // +0x30  (0x10 bytes: data/size/cap)
};
static_assert(sizeof(EffectSlot) == 0x40, "Audio::Scene::EffectSlot must be 0x40 bytes");
static_assert(offsetof(EffectSlot, posX)           == 0x00, "EffectSlot::posX @ +0x00");
static_assert(offsetof(EffectSlot, soundType)      == 0x0c, "EffectSlot::soundType @ +0x0c");
static_assert(offsetof(EffectSlot, soundEffectRef) == 0x10, "EffectSlot::soundEffectRef @ +0x10");
static_assert(offsetof(EffectSlot, startTime)      == 0x18, "EffectSlot::startTime @ +0x18");
static_assert(offsetof(EffectSlot, channelHandle)  == 0x20, "EffectSlot::channelHandle @ +0x20");
static_assert(offsetof(EffectSlot, idTag)          == 0x24, "EffectSlot::idTag @ +0x24");
static_assert(offsetof(EffectSlot, baseVolume)     == 0x28, "EffectSlot::baseVolume @ +0x28");
static_assert(offsetof(EffectSlot, positions)      == 0x30, "EffectSlot::positions @ +0x30");

// ============================================================
// Audio::Scene — 0x38 (56) bytes. NO VTABLE: the ctor's first store is
// `*(undefined8*)this = 0` (Scene::_time), not a vtable pointer, and there is
// no `??_7Scene@Audio@@` export. Consistent with WAVE8.md's prediction.
// ============================================================
struct Scene {
    int64_t  time;                    // +0x00  current System::Time ticks (set every Update)
    int32_t  playGeneration;          // +0x08  starts at 1; ++ on every PlayEffect/PlayEffect3D
    uint32_t _pad0c;                  // +0x0c
    Collection::Array<Channel>     channels; // +0x10  fixed at 256 entries after ctor
    Collection::Array<EffectSlot>  effects;  // +0x20  fixed at 4096 entries after ctor
    uint8_t  paused[5];               // +0x30  one flag per Audio::Scene::SoundType (0..4)
    uint8_t  _pad35[3];               // +0x35..+0x37
};
static_assert(sizeof(Scene) == 0x38, "Audio::Scene must be 0x38 bytes (see engine_funcs.h comment)");
static_assert(offsetof(Scene, time)           == 0x00, "Scene::time @ +0x00");
static_assert(offsetof(Scene, playGeneration) == 0x08, "Scene::playGeneration @ +0x08");
static_assert(offsetof(Scene, channels)       == 0x10, "Scene::channels @ +0x10");
static_assert(offsetof(Scene, effects)        == 0x20, "Scene::effects @ +0x20");
static_assert(offsetof(Scene, paused)         == 0x30, "Scene::paused @ +0x30");

// ============================================================
// Audio::Configuration — 4 persisted volume sliders, 0x10 bytes. NO VTABLE
// (same evidence class as Scene: Save/Startup only ever touch +0x00..+0x0f
// as four plain int32s, never a vtable slot). Pinned by
// Audio::Configuration::Save (0x1800dde10), which reads exactly these four
// offsets and nothing else.
// ============================================================
struct Configuration {
    int32_t uiVolume;       // +0x00
    int32_t musicVolume;    // +0x04
    int32_t ambientVolume;  // +0x08
    int32_t effectVolume;   // +0x0c
};
static_assert(sizeof(Configuration) == 0x10, "Audio::Configuration (ported fields) must be 0x10 bytes");

}  // namespace RtAudio
