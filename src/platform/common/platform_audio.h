#pragma once
// ============================================================================
// src/platform/common/platform_audio.h -- the audio device seam (Phase D.4, SDL
// plan Block 3 step A1)
// ============================================================================
// D4_RULES C1: namespace `Platform`, free functions, NO OS / COM / XAudio2 / SDL
// header. Implementation:
//   src/platform/sdl/audio_sdl.cpp           SDL3 audio streams (SDL plan A4 / A5,
//                                            Phase D.4 SDL-4), on every platform
// The XAudio 2.7 backend (src/platform/win/audio_xaudio2_win.cpp, one COM call per
// primitive) was deleted by SDL-4: one engine handle serves the one-shots and the
// music, so both switched at once.
//
// THE OBJECT GRAPH (plans/SDL.md A.5). Audio::Device (src/engine/audio/Device.h)
// owns one engine (+0xa88), one master (+0xa90) and 64 PlatformChannel records
// (+0xa98, stride 0x20) whose +0x08 is the channel's voice. The three handle types
// below are pointers to INCOMPLETE types, 8 bytes each, so those fields keep their
// retail offsets. The backend defines the three object types (retail's values
// were the COM interface pointers IXAudio27*, IXAudio2MasteringVoice*,
// IXAudio2SourceVoice*). Null = none.
//
// RESULTS. Every int32_t result is the backend's HRESULT-shaped status: 0 = success,
// anything else = failure. Retail's callers test `!= 0` (an EQUALITY test, so a
// success-with-info code takes the failure branch); keep that.
//
// THE CALLBACK CONTRACT. A voice is created with an AudioVoiceCallback (in the
// game: the voice's own Audio::Device::PlatformChannel record). The backend calls
// its seven slots from ITS OWN audio thread, in IXAudio2VoiceCallback order; the
// two that do anything in Banished are
//   OnBufferEnd(ctx)  -> Platform::EventSet(record.bufferEvent) when it is set
//                        (the music Stream's buffer event; null for one-shots)
//   OnStreamEnd()     -> record.channel->flags |= 2 ("finished, reap me"),
//                        WITHOUT a lock (retail's design; the main thread reaps it)
// OnStreamEnd fires after the last buffer that was submitted with endOfStream has
// played out (looping buffers never end). OnBufferEnd fires once per submitted
// buffer as it finishes. The bodies are Banished's (audio_voice_callback.cpp);
// a backend only calls them. A backend must not hold a lock the main thread can
// wait on while it calls them, and AudioVoiceDestroy must not return while a
// callback for that voice is running (XAudio2's DestroyVoice blocks until it is
// out; an SDL backend has to give the same guarantee).
//
// THE FORMAT is the 18-byte WAVEFORMATEX a SoundBuffer embeds at +0x54. Every
// shipped sound is xWMA (formatTag 0x0161, WAVE_FORMAT_WMAUDIO2: plans/SDL.md
// section 0.2), which retail handed to XAudio2 as packets plus the
// XAUDIO2_BUFFER_WMA table. Since SDL-4 NO xWMA reaches the backend (D4_RULES §3
// Q5): a one-shot SoundBuffer is decoded at load and carries a PCM format
// (SoundBuffer::DecodeAtLoad); the music voice is created with the streamed
// buffer's xWMA format and is fed the S16 PCM the Stream worker decodes, so a
// 0x0161 format given to AudioVoiceCreate means "S16 PCM at these channels and
// this rate".
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace Platform {

struct AudioEngineObject;               // never defined: the pointer is the engine
struct AudioMasterObject;               // ... the mastering voice
struct AudioVoiceObject;                // ... a source voice
using AudioEngine = AudioEngineObject*;
using AudioMaster = AudioMasterObject*;
using AudioVoice  = AudioVoiceObject*;
static_assert(sizeof(AudioEngine) == 8 && sizeof(AudioMaster) == 8 && sizeof(AudioVoice) == 8,
              "the audio handles fill retail's 8-byte interface-pointer slots");

// WAVEFORMATEX, field for field (natural alignment gives the same offsets; the
// deleted XAudio2 backend asserted each against <mmreg.h>). Read in place from the bytes a
// SoundBuffer embeds; never constructed by the engine.
struct AudioWaveFormat {
    uint16_t formatTag;                 // +0x00  wFormatTag (0x0161 = xWMA, 1 = PCM)
    uint16_t channels;                  // +0x02  nChannels
    uint32_t samplesPerSec;             // +0x04  nSamplesPerSec
    uint32_t avgBytesPerSec;            // +0x08  nAvgBytesPerSec
    uint16_t blockAlign;                // +0x0c  nBlockAlign (xWMA: the packet size)
    uint16_t bitsPerSample;             // +0x0e  wBitsPerSample
    uint16_t extraSize;                 // +0x10  cbSize
};
static_assert(offsetof(AudioWaveFormat, blockAlign) == 0x0c, "WAVEFORMATEX::nBlockAlign");
static_assert(offsetof(AudioWaveFormat, extraSize) == 0x10, "WAVEFORMATEX::cbSize");

constexpr uint16_t kAudioFormatPcm  = 0x0001;   // WAVE_FORMAT_PCM
constexpr uint16_t kAudioFormatXwma = 0x0161;   // WAVE_FORMAT_WMAUDIO2
constexpr uint32_t kAudioLoopInfinite = 255;   // XAUDIO2_LOOP_INFINITE (XAUDIO2_MAX_LOOP_COUNT is 254)

// The seven voice notifications, in IXAudio2VoiceCallback order (slot 0..6). No
// destructor slot: a GCC vtable of a class with only these virtuals is a COM
// vtable, which the XAudio2 backend handed to XAudio2 as-is; the record's layout
// and table stay retail's (Device::PlatformChannel), the SDL backend calls them.
class AudioVoiceCallback {
public:
    virtual void OnVoiceProcessingPassStart(uint32_t bytesRequired) = 0;   // [0]
    virtual void OnVoiceProcessingPassEnd() = 0;                           // [1]
    virtual void OnStreamEnd() = 0;                                        // [2]
    virtual void OnBufferStart(void* bufferContext) = 0;                   // [3]
    virtual void OnBufferEnd(void* bufferContext) = 0;                     // [4]
    virtual void OnLoopEnd(void* bufferContext) = 0;                       // [5]
    virtual void OnVoiceError(void* bufferContext, int32_t error) = 0;     // [6]
};

// ---- the engine and the master ------------------------------------------------
// Create the engine and store it in *out ONLY on success; non-zero and *out
// untouched on a failure (retail: CoCreateInstance of the XAudio 2.7 CLSID, then
// Initialize(0, XAUDIO2_DEFAULT_PROCESSOR); SDL: SDL_InitSubSystem(SDL_INIT_AUDIO)).
int32_t AudioEngineCreate(AudioEngine* out);
// Release the engine (retail: IUnknown::Release; SDL: SDL_QuitSubSystem).
void    AudioEngineRelease(AudioEngine engine);
// The output: retail CreateMasteringVoice(out, default channels, default rate, 0,
// device 0); SDL the default playback device at its own format.
int32_t AudioMasterCreate(AudioEngine engine, AudioMaster* out);
void    AudioMasterDestroy(AudioMaster master);   // DestroyVoice

// ---- a source voice --------------------------------------------------------------
// CreateSourceVoice(out, format, flags 0, maxFrequencyRatio, callback, no sends,
// no effects). *out is written by the backend.
int32_t AudioVoiceCreate(AudioEngine engine, AudioVoice* out, const AudioWaveFormat* format,
                         float maxFrequencyRatio, AudioVoiceCallback* callback);
// DestroyVoice: blocks until no callback for this voice is running.
void    AudioVoiceDestroy(AudioVoice voice);
int32_t AudioVoiceStart(AudioVoice voice);                  // Start(0, COMMIT_NOW)
int32_t AudioVoiceStop(AudioVoice voice);                   // Stop(0, COMMIT_NOW)
int32_t AudioVoiceSetVolume(AudioVoice voice, float volume);            // SetVolume(v, COMMIT_NOW)
int32_t AudioVoiceSetFrequencyRatio(AudioVoice voice, float ratio);     // SetFrequencyRatio(r, COMMIT_NOW)
// SubmitSourceBuffer: `bytes` of PCM `data` from sample frame `playBegin` to the
// end, played `loopCount` extra times (kAudioLoopInfinite = forever; a pass after
// the first starts at frame 0, as XAudio2's LoopBegin 0). The data is referenced,
// not copied: it stays valid until the buffer finishes (OnBufferEnd). `endOfStream`
// marks the voice's last buffer (XAUDIO2_END_OF_STREAM): OnStreamEnd follows when it
// has played. PlayLength, LoopBegin and LoopLength are always 0 and pContext is
// always null in Banished. (Until SDL-4 the call also took the xWMA cumulative
// decoded-byte table, XAUDIO2_BUFFER_WMA; the data is PCM now, see THE FORMAT.)
int32_t AudioVoiceSubmit(AudioVoice voice, bool endOfStream, const uint8_t* data,
                         uint32_t bytes, uint32_t playBegin, uint32_t loopCount);
// GetState().BuffersQueued: buffers submitted and not yet finished (the one
// playing included).
uint32_t AudioVoiceBuffersQueued(AudioVoice voice);

}  // namespace Platform
