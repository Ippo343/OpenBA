// ============================================================================
// tests/unit/audio_abi_test.cpp — ENDGAME B.3 step 7, block B4
//
//   Audio::SoundBuffer   : Object::CompiledExternal  0x80  9 slots, Compile INHERITED
//   Audio::SoundEffect   : Object::External          0x40  9 slots, Compile OVERRIDDEN
//   Audio::Stream        : System::Thread            0x88  2 slots (Run, OnCompleted)
//   Audio::Device        : non-polymorphic           0x1298, holds the Stream POD
//   Audio::Scene         : non-polymorphic           0x38, held by value (exe)
//   Audio::Configuration : non-polymorphic           0x10
//
// Built to the contract of pathfinding_picking_abi_test.cpp (block B6).
//
// PART A — LAYOUT: sizeof / alignof / every offset the headers claim, the
//   parent edge `_ParentTypeName` proves, abstract / not default-constructible.
// PART B — SLOT ORDER: a probe subclass of the REAL class; each slot is
//   raw-called by byte offset and the marker that runs names the member.
//   SoundBuffer's slot 4 IS Object::External's Compile (B7-1.7); SoundEffect's
//   is its own. Stream's slot 0 is Run and slot 1 is Thread's OnCompleted
//   (B7-1.4: no deleting-dtor slot).
// PART C — THE QUALIFIED DOOR CALL: a minted-table slot pointing at a door that
//   delegates `->Audio::SoundEffect::Compile()` enters the door once.
// PART D — ARGUMENT PASSING (B7-5): `ExternalPtr<T>` by value and
//   `System::Time` by value are both ONE REGISTER, pinned by a raw retail-order
//   call through an untyped pointer.
// PART E — the one-line accessors read retail's offsets (bytes read from the
//   retail DLL, cited in each header).
//
// The classes' out-of-line members are defined HERE as markers
// (runtime/src/audio does not link into a host test).
// ============================================================================
#include "unit.h"

#include "runtime/types/audio/Audio.h"
#include "runtime/types/audio/Device.h"
#include "runtime/types/audio/SoundBuffer.h"
#include "runtime/types/audio/SoundEffect.h"
#include "runtime/types/audio/Stream.h"
#include "runtime/types/system/Value.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

const void* g_lastSelf = nullptr;
int         g_lastMark = -1;
void Note(const void* self, int mark) { g_lastSelf = self; g_lastMark = mark; }
void Reset() { g_lastSelf = nullptr; g_lastMark = -1; }

template <typename Fn>
Fn SlotAt(void* obj, int byteOffset) {
    const uint8_t* vt = *reinterpret_cast<const uint8_t* const*>(obj);
    Fn f = nullptr;
    const void* raw = *reinterpret_cast<const void* const*>(vt + byteOffset);
    std::memcpy(&f, &raw, sizeof f);
    return f;
}
const void* const* VTableOf(void* obj) {
    return *reinterpret_cast<const void* const* const*>(obj);
}

constexpr int kDeletingDtor   = 0x00;
constexpr int kTypeName       = 0x08;
constexpr int kType           = 0x10;
constexpr int kGetFileVersion = 0x18;
constexpr int kCompile        = 0x20;
constexpr int kSerialize      = 0x28;
constexpr int kReconstruct    = 0x30;
constexpr int kDestruct       = 0x38;
constexpr int kConstruct      = 0x40;

// A bare Object::External leaf: whatever it carries at +0x20 IS
// `?Compile@External@Object@@` (0x1800066e0).
struct BareExternalProbe final : Object::External {
    Object::Base* DeletingDtor(unsigned) override { return this; }
    int32_t GetFileVersion() const override { return 0; }
    bool Serialize(void*) override { return true; }
    Object::External* Reconstruct() override { return this; }
    void Destruct() override {}
};

}  // namespace

// ===========================================================================
// The out-of-line members, AS MARKERS.
// ===========================================================================
namespace Audio {
const char* SoundBuffer::TypeName() const { Note(this, 1); return "SoundBuffer"; }
uint32_t SoundBuffer::Type() const { Note(this, 2); return 0x5b0fu; }
int32_t SoundBuffer::GetFileVersion() const { Note(this, 3); return 0xa; }
bool SoundBuffer::Serialize(void*) { Note(this, 5); return true; }
Object::External* SoundBuffer::Reconstruct() { Note(this, 6); return this; }
void SoundBuffer::Destruct() { Note(this, 7); }
Object::External* SoundBuffer::Construct() { Note(this, 8); return this; }
SoundBuffer::~SoundBuffer() {}

const char* SoundEffect::TypeName() const { Note(this, 1); return "SoundEffect"; }
uint32_t SoundEffect::Type() const { Note(this, 2); return 0x5e0fu; }
int32_t SoundEffect::GetFileVersion() const { Note(this, 3); return 3; }
Object::External::CompileResult SoundEffect::Compile(const System::Filename&) { Note(this, 4); return 1; }
bool SoundEffect::Serialize(void*) { Note(this, 5); return true; }
Object::External* SoundEffect::Reconstruct() { Note(this, 6); return this; }
void SoundEffect::Destruct() { Note(this, 7); }
Object::External* SoundEffect::Construct() { Note(this, 8); return this; }
SoundEffect::~SoundEffect() {}

void Stream::Run() { Note(this, 50); }

Configuration::~Configuration() { Note(this, 90); }
}  // namespace Audio

namespace {

struct SoundBufferProbe final : Audio::SoundBuffer {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct SoundEffectProbe final : Audio::SoundEffect {
    Object::Base* DeletingDtor(unsigned) override { Note(this, 0); return this; }
};
struct StreamProbe final : Audio::Stream {};

using DtorFn    = void* (*)(void*, unsigned);
using NameFn    = const char* (*)(const void*);
using TypeFn    = uint32_t (*)(const void*);
using VerFn     = int32_t (*)(const void*);
using CompileFn = uint32_t (*)(void*, const void*);
using SerFn     = bool (*)(void*, void*);
using PtrFn     = void* (*)(void*);
using VoidFn    = void (*)(void*);

// Raw-call the eight slots every Audio resource owns (4 checked separately).
void CheckOwnSlotsExceptCompile(void* raw) {
    Reset(); SlotAt<DtorFn>(raw, kDeletingDtor)(raw, 0u);
    CHECK_EQ(g_lastMark, kDeletingDtor / 8); CHECK(g_lastSelf == raw);
    Reset(); SlotAt<NameFn>(raw, kTypeName)(raw);          CHECK_EQ(g_lastMark, kTypeName / 8);
    Reset(); SlotAt<TypeFn>(raw, kType)(raw);              CHECK_EQ(g_lastMark, kType / 8);
    Reset(); SlotAt<VerFn>(raw, kGetFileVersion)(raw);     CHECK_EQ(g_lastMark, kGetFileVersion / 8);
    Reset(); SlotAt<SerFn>(raw, kSerialize)(raw, nullptr); CHECK_EQ(g_lastMark, kSerialize / 8);
    Reset(); SlotAt<PtrFn>(raw, kReconstruct)(raw);        CHECK_EQ(g_lastMark, kReconstruct / 8);
    Reset(); SlotAt<VoidFn>(raw, kDestruct)(raw);          CHECK_EQ(g_lastMark, kDestruct / 8);
    Reset(); SlotAt<PtrFn>(raw, kConstruct)(raw);          CHECK_EQ(g_lastMark, kConstruct / 8);
    CHECK(g_lastSelf == raw);
}

// The qualified door (vtbatch7_soundbuffer_soundeffect.cpp's
// RtAudio_SoundEffect_Compile shape).
int g_doorEntries = 0;
int32_t MintedSoundEffectCompileDoor(void* self, const void* path) {
    ++g_doorEntries;
    return static_cast<int32_t>(reinterpret_cast<Audio::SoundEffect*>(self)      // B.3.7 bridge
        ->Audio::SoundEffect::Compile(*static_cast<const System::Filename*>(path)));
}

// B7-5 receivers: the typed shapes of Scene::PlayEffect's first argument and
// Scene::Update's two Times, as the doors hand them over.
void* g_seenEffect = nullptr;
uint32_t TakesEffectByValue(void* /*self*/, Object::ExternalPtr<Audio::SoundEffect> fx) {
    g_seenEffect = fx.Get();
    return 11u;
}
int64_t g_seenA = 0, g_seenB = 0;
bool TakesTwoTimesByValue(void* /*self*/, System::Time a, System::Time b) {
    g_seenA = a._ticks;
    g_seenB = b._ticks;
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// PART A — layouts
// ---------------------------------------------------------------------------
UNIT_TEST(audio_soundbuffer_layout) {
    CHECK_EQ((int)sizeof(Audio::SoundBuffer), 0x80);
    CHECK_EQ((int)alignof(Audio::SoundBuffer), 8);
    CHECK((std::is_base_of<Object::CompiledExternal, Audio::SoundBuffer>::value));  // B7-2
    CHECK(std::is_abstract<Audio::SoundBuffer>::value);
    CHECK(!std::is_default_constructible<Audio::SoundBuffer>::value);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _loopForever),     0x20);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _streamed),        0x21);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _durationSeconds), 0x24);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _string28),        0x28);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _streamPath),      0x30);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _data),            0x38);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _length),          0x40);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _wmaData),         0x48);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _wmaLength),       0x50);
    CHECK_EQ((int)offsetof(Audio::SoundBuffer, _format),          0x54);
}

UNIT_TEST(audio_soundeffect_layout) {
    CHECK_EQ((int)sizeof(Audio::SoundEffect), 0x40);
    CHECK_EQ((int)alignof(Audio::SoundEffect), 8);
    CHECK((std::is_base_of<Object::External, Audio::SoundEffect>::value));
    CHECK(!(std::is_base_of<Object::CompiledExternal, Audio::SoundEffect>::value));
    CHECK(std::is_abstract<Audio::SoundEffect>::value);
    CHECK(!std::is_default_constructible<Audio::SoundEffect>::value);
    CHECK_EQ((int)offsetof(Audio::SoundEffect, _radius),      0x20);
    CHECK_EQ((int)offsetof(Audio::SoundEffect, _innerRadius), 0x24);
    CHECK_EQ((int)offsetof(Audio::SoundEffect, _maxLength),   0x28);
    CHECK_EQ((int)offsetof(Audio::SoundEffect, _samples),     0x30);
    CHECK_EQ((int)sizeof(Audio::SoundEffect::Sample), 0x18);
    CHECK_EQ((int)sizeof(Audio::SoundEffect::Variant), 0x18);
}

UNIT_TEST(audio_stream_layout_and_shape) {
    CHECK_EQ((int)sizeof(Audio::Stream), 0x88);
    CHECK_EQ((int)alignof(Audio::Stream), 8);
    CHECK((std::is_base_of<System::Thread, Audio::Stream>::value));
    CHECK(std::is_polymorphic<Audio::Stream>::value);
    CHECK(!std::is_abstract<Audio::Stream>::value);
    CHECK(!std::is_default_constructible<Audio::Stream>::value);   // protected (B4b-2)
    CHECK_EQ((int)offsetof(Audio::Stream, _buffers),      0x30);
    CHECK_EQ((int)offsetof(Audio::Stream, _wakeEvent),    0x40);
    CHECK_EQ((int)offsetof(Audio::Stream, _bufferEvent),  0x48);
    CHECK_EQ((int)offsetof(Audio::Stream, _lock),         0x50);
    CHECK_EQ((int)offsetof(Audio::Stream, _bytePosition), 0x78);
    CHECK_EQ((int)offsetof(Audio::Stream, _channel),      0x7c);
    CHECK_EQ((int)offsetof(Audio::Stream, _stopping),     0x80);
    CHECK_EQ((int)sizeof(RtAudio::Stream), 0x88);
    CHECK(std::is_trivially_destructible<RtAudio::Stream>::value);
}

UNIT_TEST(audio_device_layout_holds_the_stream_pod) {
    CHECK_EQ((int)sizeof(Audio::Device), 0x1298);
    CHECK(!std::is_polymorphic<Audio::Device>::value);
    CHECK_EQ((int)sizeof(Audio::Device::AudioChannel), 0x28);
    CHECK_EQ((int)sizeof(Audio::Device::PlatformChannel), 0x20);
    CHECK_EQ((int)offsetof(Audio::Device, _stream),           0xa00);
    CHECK_EQ((int)offsetof(Audio::Device, _xaudio2),          0xa88);
    CHECK_EQ((int)offsetof(Audio::Device, _masteringVoice),   0xa90);
    CHECK_EQ((int)offsetof(Audio::Device, _platformChannels), 0xa98);
    // B7-12: the Device holds the Stream POD, not the polymorphic class.
    CHECK((std::is_same<decltype(Audio::Device::_stream), RtAudio::Stream>::value));
    // The three Stream fields the Device bodies reach by Device offset.
    CHECK_EQ((int)(offsetof(Audio::Device, _stream) + offsetof(RtAudio::Stream, _lock)),      0xa50);
    CHECK_EQ((int)(offsetof(Audio::Device, _stream) + offsetof(RtAudio::Stream, _wakeEvent)), 0xa40);
    CHECK_EQ((int)(offsetof(Audio::Device, _stream) + offsetof(RtAudio::Stream, _stopping)),  0xa80);
}

UNIT_TEST(audio_scene_and_configuration_layout) {
    CHECK_EQ((int)sizeof(Audio::Scene), 0x38);
    CHECK(!std::is_polymorphic<Audio::Scene>::value);
    CHECK(!std::is_abstract<Audio::Scene>::value);
    CHECK(!std::is_default_constructible<Audio::Scene>::value);   // the ctor DOOR builds it
    CHECK(std::is_destructible<Audio::Scene>::value);             // implicit = retail's
    CHECK(!std::is_trivially_copyable<Audio::Scene>::value);      // B7-8
    CHECK_EQ((int)offsetof(Audio::Scene, channels), 0x10);
    CHECK_EQ((int)offsetof(Audio::Scene, effects),  0x20);
    CHECK_EQ((int)offsetof(Audio::Scene, paused),   0x30);
    CHECK_EQ((int)sizeof(Audio::Scene::Channel), 0x70);
    CHECK_EQ((int)sizeof(Audio::Scene::AttenutedEffect), 0x40);
    CHECK_EQ((int)sizeof(Audio::Scene::EmitterInstance), 0x10);
    CHECK_EQ((int)sizeof(Audio::Scene::SoundType), 4);
    CHECK((std::is_same<RtAudio::EffectSlot, Audio::Scene::AttenutedEffect>::value));
    CHECK((std::is_same<RtAudio::Scene, Audio::Scene>::value));

    CHECK_EQ((int)sizeof(Audio::Configuration), 0x10);
    CHECK(!std::is_polymorphic<Audio::Configuration>::value);
    CHECK_EQ((int)offsetof(Audio::Configuration, effectVolume), 0x0c);
}

// ---------------------------------------------------------------------------
// PART B — slot order
// ---------------------------------------------------------------------------
UNIT_TEST(audio_soundbuffer_slot_order_and_inherited_compile) {
    SoundBufferProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckOwnSlotsExceptCompile(raw);
    // Slot 4 is NOT SoundBuffer's: it is Object::External's Compile, the same
    // pointer a bare External leaf carries, and it answers retail's 1.
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kCompile / 8] == VTableOf(static_cast<void*>(&bare))[kCompile / 8]);
    CHECK_EQ(SlotAt<CompileFn>(raw, kCompile)(raw, nullptr), 1u);
}

UNIT_TEST(audio_soundeffect_slot_order_and_own_compile) {
    SoundEffectProbe probe;
    void* raw = static_cast<void*>(&probe);
    CheckOwnSlotsExceptCompile(raw);
    Reset();
    CHECK_EQ(SlotAt<CompileFn>(raw, kCompile)(raw, nullptr), 1u);
    CHECK_EQ(g_lastMark, kCompile / 8);
    BareExternalProbe bare;
    CHECK(VTableOf(raw)[kCompile / 8] != VTableOf(static_cast<void*>(&bare))[kCompile / 8]);
    const void* const* vt = VTableOf(raw);
    for (int i = 0; i < 9; ++i)
        for (int j = i + 1; j < 9; ++j)
            CHECK(vt[i] != vt[j]);
}

UNIT_TEST(audio_stream_slot0_is_run_and_slot1_is_threads_oncompleted) {
    StreamProbe probe;
    void* raw = static_cast<void*>(&probe);
    Reset();
    SlotAt<VoidFn>(raw, 0x00)(raw);
    CHECK_EQ(g_lastMark, 50);
    CHECK(g_lastSelf == raw);
    // Slot 1 is System::Thread's own OnCompleted: the same pointer the
    // Thread-derived Engine::Loader-shaped probe carries at +0x08.
    struct OtherThread final : System::Thread { void Run() override {} };
    OtherThread other;
    CHECK(VTableOf(raw)[1] == VTableOf(static_cast<void*>(&other))[1]);
    CHECK(VTableOf(raw)[0] != VTableOf(static_cast<void*>(&other))[0]);
}

// ---------------------------------------------------------------------------
// PART C — the qualified door call does not re-enter the door
// ---------------------------------------------------------------------------
UNIT_TEST(audio_soundeffect_qualified_call_does_not_reenter_the_door) {
    SoundEffectProbe probe;
    void* raw = static_cast<void*>(&probe);
    const void* minted[9] = {};
    const void* const* real = VTableOf(raw);
    for (int i = 0; i < 9; ++i) minted[i] = real[i];
    minted[kCompile / 8] = reinterpret_cast<const void*>(&MintedSoundEffectCompileDoor);
    *reinterpret_cast<const void* const**>(raw) = minted;

    g_doorEntries = 0;
    Reset();
    const uint32_t r = SlotAt<CompileFn>(raw, kCompile)(raw, nullptr);
    CHECK_EQ(g_doorEntries, 1);
    CHECK_EQ(g_lastMark, kCompile / 8);
    CHECK_EQ(r, 1u);

    *reinterpret_cast<const void* const**>(raw) = real;
}

// ---------------------------------------------------------------------------
// PART D — B7-5: ExternalPtr and System::Time by value are single registers
// ---------------------------------------------------------------------------
UNIT_TEST(audio_externalptr_by_value_is_a_bare_register) {
    CHECK((std::is_trivially_copyable<Object::ExternalPtr<Audio::SoundEffect> >::value));
    CHECK_EQ((int)sizeof(Object::ExternalPtr<Audio::SoundEffect>), 8);
    using RawFn = uint32_t (*)(void*, void*);
    uint32_t (*typed)(void*, Object::ExternalPtr<Audio::SoundEffect>) = &TakesEffectByValue;
    RawFn rawFn = nullptr;
    std::memcpy(&rawFn, &typed, sizeof rawFn);
    int self = 0, fx = 0;
    g_seenEffect = nullptr;
    CHECK_EQ(rawFn(&self, &fx), 11u);
    CHECK(g_seenEffect == static_cast<void*>(&fx));
}

UNIT_TEST(audio_time_by_value_is_a_register) {
    CHECK((std::is_trivially_copyable<System::Time>::value));
    CHECK_EQ((int)sizeof(System::Time), 8);
    // Scene::Update's door hands the two tick counts over as raw int64s in
    // RDX / R8 — retail's `MOV [R15],R8` stores the second one raw.
    using RawFn = bool (*)(void*, int64_t, int64_t);
    bool (*typed)(void*, System::Time, System::Time) = &TakesTwoTimesByValue;
    RawFn rawFn = nullptr;
    std::memcpy(&rawFn, &typed, sizeof rawFn);
    int self = 0;
    CHECK(rawFn(&self, 0x1234, -77));
    CHECK_EQ((long long)g_seenA, 0x1234LL);
    CHECK_EQ((long long)g_seenB, -77LL);
}

// ---------------------------------------------------------------------------
// PART E — the inline accessors read retail's offsets
// ---------------------------------------------------------------------------
UNIT_TEST(audio_accessors_read_retails_offsets) {
    SoundBufferProbe sb;
    const Audio::SoundBuffer& b = sb;
    const uint8_t* base = reinterpret_cast<const uint8_t*>(&b);
    CHECK(reinterpret_cast<const uint8_t*>(&b.GetStreamPath()) == base + 0x30);   // 0x18000bca0
    CHECK(static_cast<const uint8_t*>(b.Format()) == base + 0x54);                // 0x18000bcb0
    sb._loopForever = 1; sb._streamed = 0;
    CHECK(b.IsLooping()); CHECK(!b.IsStreamed());

    SoundEffectProbe se;
    se._radius = 3.5f; se._innerRadius = 1.25f; se._maxLength = 0;
    CHECK(se.GetRadius() == 3.5f);
    CHECK(se.GetInnerRadius() == 1.25f);
    CHECK(se.IsLooping());                      // 0x1800506b0: `[RCX+0x28] == 0`
    se._maxLength = 9;
    CHECK(!se.IsLooping());

    alignas(8) static uint8_t devStorage[sizeof(Audio::Device)] = {};
    Audio::Device& dev = *reinterpret_cast<Audio::Device*>(devStorage);
    CHECK(reinterpret_cast<uint8_t*>(dev.GetChannel(3)) == devStorage + 3 * 0x28);            // 0x18000c710
    CHECK(reinterpret_cast<uint8_t*>(dev.GetPlatformChannel(2)) == devStorage + 0xa98 + 0x40); // 0x18000c720
    CHECK(reinterpret_cast<uint8_t*>(dev.StreamObject()) == devStorage + 0xa00);

    alignas(8) uint8_t sceneStorage[sizeof(Audio::Scene)] = {};
    sceneStorage[0x30 + 4] = 1;
    const Audio::Scene& scene = *reinterpret_cast<const Audio::Scene*>(sceneStorage);
    CHECK(scene.IsPaused(4)); CHECK(!scene.IsPaused(3));                          // 0x180050990

    alignas(4) uint8_t cfgStorage[sizeof(Audio::Configuration)] = {};
    Audio::Configuration& cfg = *reinterpret_cast<Audio::Configuration*>(cfgStorage);
    cfg.SetMusicVolume(50);
    CHECK_EQ(*reinterpret_cast<int32_t*>(cfgStorage + 4), 50);                    // 0x18000d4f0
    CHECK_EQ(cfg.GetMusicVolume(), 50);
}
