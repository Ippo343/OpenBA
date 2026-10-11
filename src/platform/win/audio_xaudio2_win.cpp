// ============================================================================
// src/platform/win/audio_xaudio2_win.cpp -- Platform::Audio over XAudio 2.7 (COM)
// ============================================================================
// Phase D.4, SDL plan Block 3 step A1 (`refactor:`). Every XAudio2 / COM call the
// engine made MOVED here unchanged: the same interface, the same vtable slot, the
// same arguments. Where they came from:
//   AudioEngineCreate     audio_platform_startup_win.cpp `RtAudio_CreateXAudio2`
//                         (retail's unexported helper 0x180050270), whole body
//   AudioMasterCreate     Device::PlatformStartup 0x1800de65c  IXAudio27 +0x50
//   AudioEngineRelease    PlatformStartup 0x1800de669, PlatformShutdown 0x1800de6c8
//                         (inlined in Engine::Shutdown / GameMain's teardown)  +0x10
//   AudioMasterDestroy    PlatformShutdown 0x1800de6a8                        +0x90
//   AudioVoiceCreate      Device::PlayPlatform 0x1800de827                    +0x40
//   AudioVoiceSubmit      PlayPlatform 0x1800de948, Stream::Run 0x1800a1361   +0xa8
//   AudioVoiceSetVolume   PlayPlatform 0x1800de964, SetVolumePlatform 0x1800dead7
//                         (inlined in Scene::Update 0x1800dfc06)              +0x60
//   AudioVoiceSetFrequencyRatio  PlayPlatform 0x1800de998                     +0xd0
//   AudioVoiceStart       PlayPlatform 0x1800de9b5                            +0x98
//   AudioVoiceStop        Device::StopPlatform 0x1800dea85                    +0xa0
//   AudioVoiceDestroy     PlayPlatform's cleanup, FreeChannelPlatform 0x1800de74a +0x90
//   AudioVoiceBuffersQueued  Stream::Run's GetState calls (4 sites)           +0xc8
// The offsets are on the XAudio 2.7 interfaces retail creates by CLSID / IID (the
// engine IS an IXAudio27; `<xaudio2.h>`'s IXAudio2SourceVoice has the 2.7 slot
// order up to +0xd0, and its 2.8 GetState's extra Flags argument (R8 = 0) is one
// the 2.7 method never reads -- unchanged from before the move).
//
// The handles are the interface pointers (platform_audio.h): the casts below are
// the one place an opaque handle meets its COM type.
// ============================================================================

#include <windows.h>
#include <objbase.h>
#include <xaudio2.h>

#include <cstddef>
#include <cstdint>

#include "platform_audio.h"

// The format the engine reads in place IS a WAVEFORMATEX.
static_assert(offsetof(WAVEFORMATEX, wFormatTag)      == offsetof(Platform::AudioWaveFormat, formatTag), "wFormatTag");
static_assert(offsetof(WAVEFORMATEX, nChannels)       == offsetof(Platform::AudioWaveFormat, channels), "nChannels");
static_assert(offsetof(WAVEFORMATEX, nSamplesPerSec)  == offsetof(Platform::AudioWaveFormat, samplesPerSec), "nSamplesPerSec");
static_assert(offsetof(WAVEFORMATEX, nAvgBytesPerSec) == offsetof(Platform::AudioWaveFormat, avgBytesPerSec), "nAvgBytesPerSec");
static_assert(offsetof(WAVEFORMATEX, nBlockAlign)     == offsetof(Platform::AudioWaveFormat, blockAlign), "nBlockAlign");
static_assert(offsetof(WAVEFORMATEX, wBitsPerSample)  == offsetof(Platform::AudioWaveFormat, bitsPerSample), "wBitsPerSample");
static_assert(offsetof(WAVEFORMATEX, cbSize)          == offsetof(Platform::AudioWaveFormat, extraSize), "cbSize");
static_assert(Platform::kAudioLoopInfinite == XAUDIO2_LOOP_INFINITE, "XAUDIO2_LOOP_INFINITE");

namespace {

// Copied from the pefile dump of the shipped .rdata, byte for byte, so this
// file cannot pick up a DIFFERENT XAudio2 (the 2.7 identifiers, June 2010 SDK,
// not the in-box XAudio2_8 / _9 ones):
//   8586505a 54a2 ba4f 9b82 9a24b00306af
const GUID kClsidXAudio2 =
    { 0x5A508685, 0xA254, 0x4FBA, { 0x9B, 0x82, 0x9A, 0x24, 0xB0, 0x03, 0x06, 0xAF } };
//   581fcf8b e79f 8345 8ac6 e2adc465c8bb
const GUID kIidIXAudio2 =
    { 0x8BCF1F58, 0x9FE7, 0x4583, { 0x8A, 0xC6, 0xE2, 0xAD, 0xC4, 0x65, 0xC8, 0xBB } };

constexpr UINT32 kXAudio2EndOfStream = 0x40;   // XAUDIO2_END_OF_STREAM

IXAudio27* Engine(Platform::AudioEngine e) {
    return reinterpret_cast<IXAudio27*>(e);               // CAST-OK: win32
}
IXAudio2SourceVoice* Voice(Platform::AudioVoice v) {
    return reinterpret_cast<IXAudio2SourceVoice*>(v);     // CAST-OK: win32
}

}  // namespace

namespace Platform {

// ---------------------------------------------------------------------------
// 0x180050270 -- the unexported helper (was `RtAudio_CreateXAudio2`).
// `CoCreateInstance` then `Initialize`, storing the interface into `*out` ONLY
// when both succeed. Three details from the disassembly:
//   * `TEST EAX,EAX ; JS` at 0x18005029f and again at 0x1800502c0 -- the tests
//     are SIGN tests (FAILED), not equality tests. The CALLER's test is the
//     equality one.
//   * on the CoCreateInstance failure path `*out` is NOT written at all; on the
//     Initialize failure path the interface is Released and `*out` is still not
//     written. The caller does the nulling.
//   * the success path returns Initialize's HRESULT, not S_OK -- so a
//     succeeded-with-info Initialize would make the CALLER take its failure
//     branch. Reproduced rather than normalised.
// ---------------------------------------------------------------------------
int32_t AudioEngineCreate(AudioEngine* out) {
    IXAudio27* obj = nullptr;
    const HRESULT hr = CoCreateInstance(kClsidXAudio2, nullptr, CLSCTX_INPROC_SERVER,
                                        kIidIXAudio2,
                                        reinterpret_cast<void**>(&obj));  // CAST-OK: com_iid
    if (hr < 0) return hr;                                 // 0x18005029f
    if (obj == nullptr) return hr;                         // 0x1800502a1 loads it back

    const long hr2 = obj->Initialize(0, XAUDIO2_DEFAULT_PROCESSOR);  // 0x1800502b4 +0x28
    if (hr2 < 0) {                                          // 0x1800502c0
        obj->Release();                                     // 0x1800502d3 +0x10
        return hr2;
    }
    *out = reinterpret_cast<AudioEngine>(obj);              // CAST-OK: win32  0x1800502c2
    return hr2;
}

void AudioEngineRelease(AudioEngine engine) {
    Engine(engine)->Release();                                          // +0x10
}

int32_t AudioMasterCreate(AudioEngine engine, AudioMaster* out) {
    return Engine(engine)->CreateMasteringVoice(
        reinterpret_cast<IXAudio2MasteringVoice**>(out),  // CAST-OK: win32
        0, 0, 0, 0, nullptr);                                           // +0x50
}

void AudioMasterDestroy(AudioMaster master) {
    reinterpret_cast<IXAudio2MasteringVoice*>(master)->DestroyVoice();  // CAST-OK: win32  +0x90
}

int32_t AudioVoiceCreate(AudioEngine engine, AudioVoice* out, const AudioWaveFormat* format,
                         float maxFrequencyRatio, AudioVoiceCallback* callback) {
    // `callback`'s vptr IS an IXAudio2VoiceCallback table (platform_audio.h).
    return Engine(engine)->CreateSourceVoice(
        reinterpret_cast<IXAudio2SourceVoice**>(out),     // CAST-OK: win32
        reinterpret_cast<const WAVEFORMATEX*>(format),    // CAST-OK: win32
        0, maxFrequencyRatio,
        reinterpret_cast<IXAudio2VoiceCallback*>(callback),  // CAST-OK: win32
        nullptr, nullptr);                                              // +0x40
}

void AudioVoiceDestroy(AudioVoice voice) {
    Voice(voice)->DestroyVoice();                                       // +0x90
}

int32_t AudioVoiceStart(AudioVoice voice) {
    return Voice(voice)->Start(0, 0);                                   // +0x98
}

int32_t AudioVoiceStop(AudioVoice voice) {
    return Voice(voice)->Stop(0, 0);                                    // +0xa0
}

int32_t AudioVoiceSetVolume(AudioVoice voice, float volume) {
    return Voice(voice)->SetVolume(volume, 0);                          // +0x60
}

int32_t AudioVoiceSetFrequencyRatio(AudioVoice voice, float ratio) {
    return Voice(voice)->SetFrequencyRatio(ratio, 0);                   // +0xd0
}

int32_t AudioVoiceSubmit(AudioVoice voice, bool endOfStream, const uint8_t* data,
                         uint32_t bytes, uint32_t playBegin, uint32_t loopCount,
                         const uint32_t* packetTable, uint32_t packetCount) {
    // Both callers built these two (packed) records on their own stack before
    // the move: PlayPlatform with Flags END_OF_STREAM and its PlayBegin /
    // LoopCount, Stream::Run with every field zeroed and then Flags (0 or
    // END_OF_STREAM), AudioBytes and pAudioData. pContext was null in both
    // (retail leaves PlayPlatform's uninitialised; nothing reads it).
    XAUDIO2_BUFFER buf{};
    buf.Flags      = endOfStream ? kXAudio2EndOfStream : 0u;
    buf.AudioBytes = bytes;
    buf.pAudioData = data;
    buf.PlayBegin  = playBegin;
    buf.PlayLength = 0;
    buf.LoopBegin  = 0;
    buf.LoopLength = 0;
    buf.LoopCount  = loopCount;
    buf.pContext   = nullptr;

    XAUDIO2_BUFFER_WMA wma{};
    wma.pDecodedPacketCumulativeBytes = packetTable;
    wma.PacketCount = packetCount;

    // Phase D.4 SDL-4: a SoundBuffer decoded at load is PCM and comes with no
    // table; XAudio2 takes no XAUDIO2_BUFFER_WMA for a PCM voice.
    return Voice(voice)->SubmitSourceBuffer(&buf, packetTable ? &wma : nullptr);  // +0xa8
}

uint32_t AudioVoiceBuffersQueued(AudioVoice voice) {
    XAUDIO2_VOICE_STATE state{};
    Voice(voice)->GetState(&state);                                     // +0xc8
    return state.BuffersQueued;
}

}  // namespace Platform
