// ============================================================================
// runtime/src/audio/audio_play_platform.cpp — batch 7 (Input/Audio leaves)
//   Audio::Device::PlayPlatform(...)                  retail 0x1800de7a0  ord 3424
// ============================================================================
//
//   ?PlayPlatform@Device@Audio@@AEAA_NHV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMM@Z
//
// `AEAA` is PRIVATE, not unexported (reference_aeaa_is_private_not_unexported.md);
// checked against the retail export directory with pefile before writing this
// file, not assumed. It was Audio's last remaining unported BODY row: two
// earlier lanes (WAVE28_LANE_SLOTS.md §the callback table, WAVE29_LANE_AVBODY.md
// §6.1) sized it and declined it as "632 bytes, six indirect XAudio2 calls,
// effects that live in the driver" -- an honest judgement call, not a
// dependency block. Its only two unported dependencies
// (docs/runtime_port/closure/unported_deps.csv) are `FUN_180127560` (a plain
// CRT `powf`, see below) and `Audio::Stream::Play`, which THIS SAME BATCH ports
// in `audio_stream_play_win.cpp`.
//
// The ONE live caller in our tree is `audio_scene_update.cpp`'s
// `Audio::Scene::Update` port, which already resolves this export BY NAME into
// a local `PlayPlatformFn` typedef -- reproduced verbatim here so the two files
// agree on the ABI:
//     bool (*)(void* device, int32_t slot, void* bufferRef,
//              const int64_t* startTime, float vol, float pitch, float third)
//
// ---------------------------------------------------------------------------
// THE BODY, READ FROM THE DISASSEMBLY (not Ghidra's decompile -- it silently
// drops the 7th argument, `third`, the way
// reference_ghidra_c_output_vs_disassembly.md warns; the disassembly settles
// every register and every stack offset used below)
// ---------------------------------------------------------------------------
//   1800de7b4  MOV RCX,[this+0xa88]         ; IXAudio2* engine (already named
//                                            ; `kOffXAudio2` in
//                                            ; audio_platform_startup_win.cpp)
//   1800de7c4  JNZ live ; else return false  ; no engine, no voice, nothing to undo
//   1800de7ec  RDX = this + slot*0x20 + 0xa98            ; &VoiceCallback[slot]
//                                                         ; (the SAME record
//                                                         ; audio_voice_callback_win.cpp
//                                                         ; names -- cross-file
//                                                         ; agreement, not a
//                                                         ; second derivation)
//   1800de809  RSI = RDX + 8                              ; &record.voice
//   1800de817  R8  = bufferRef + 0x54                      ; pSourceFormat
//                                                          ; (SoundBuffer embeds
//                                                          ; a WAVEFORMATEX at
//                                                          ; +0x54 -- see below)
//   1800de827  CALL [engineVtbl+0x40](engine, &record.voice, pSourceFormat,
//                                     Flags=0, MaxFreqRatio=2.0f@0x1801616d8,
//                                     pCallback=&record, pSendList=0,
//                                     pEffectChain=0)      ; CreateSourceVoice
//   1800de82a  TEST EAX,EAX ; JZ live ; else return false  ; no cleanup needed,
//                                                          ; nothing was created
//
// `engineVtbl+0x40` = IXAudio2::CreateSourceVoice and `+0x50` =
// CreateMasteringVoice are ALREADY the two indices
// `audio_voice_callback_win.cpp` cites as the identification for
// `??0Device@Audio@@`'s use of the same `+0xa88` pointer -- so this body opens
// no new interface-identity question, it reuses one already measured.
//
// ---------------------------------------------------------------------------
// THE BYTE-POSITION / PLAYBEGIN CALCULATION (0x1800de835..0x1800de8dc)
// ---------------------------------------------------------------------------
// SoundBuffer embeds a WAVEFORMATEX at +0x54 (independently confirmed: it is
// the exact `pSourceFormat` passed to CreateSourceVoice above), so the fields
// below are read at the WAVEFORMATEX offsets +0x02/+0x04/+0x0c/+0x0e:
//   +0x20 (byte)   loop-forever flag       -> LoopCount 0xFFFFFFFF or 0
//   +0x21 (byte)   "streaming" flag        -> skip SubmitSourceBuffer, and
//                                             afterwards hand off to
//                                             Audio::Stream::Play
//   +0x24 (float)  loop duration, SECONDS
//   +0x38 (ptr)    pAudioData
//   +0x40 (dword)  AudioBytes
//   +0x48 (ptr)    a table of ascending byte-offset segment boundaries (uint32[])
//   +0x50 (dword)  that table's BYTE length (element count = length >> 2)
//   +0x54 (word)   WAVEFORMATEX::wFormatTag        (unused here)
//   +0x56 (word)   WAVEFORMATEX::nChannels
//   +0x58 (dword)  WAVEFORMATEX::nSamplesPerSec
//   +0x60 (word)   WAVEFORMATEX::nBlockAlign
//   +0x62 (word)   WAVEFORMATEX::wBitsPerSample
//
// Retail, instruction for instruction (`spt` = System::Timer::_secondsPerTick):
//   loopDurationTicks = (int64)((double)buf.loopDurationSeconds / spt)   ; CVTTSD2SI
//   remainderTicks    = *startTime % loopDurationTicks                  ; IDIV, signed
//   secondsInLoop     = (float)((double)remainderTicks * spt)           ; CVTPD2PS
//   bytesPerSecond    = (nChannels * wBitsPerSample * nSamplesPerSec) >> 3
//                       (three 32-bit IMULs then a LOGICAL SHR -- reproduced
//                        with unsigned 32-bit wraparound, not widened)
//   bytePos           = (int32)(secondsInLoop * (float)bytesPerSecond)  ; CVTTSS2SI
//
//   playBegin = 0
//   if (segmentCount > 0) {
//       for (idx = 0; ; ) {
//           if (segmentTable[idx] > (uint32)bytePos) {   ; JA, UNSIGNED compare
//               playBegin = nBlockAlign * idx;           ; idx BEFORE any
//                                                          ; increment this pass
//               break;
//           }
//           if (++idx >= segmentCount) break;            ; not found -> stays 0
//       }
//   }
//   if (isStreaming) playBegin = 0;                       ; CMOVNZ, unconditional
//                                                          ; override -- bytePos
//                                                          ; itself is NOT
//                                                          ; overridden and is
//                                                          ; still used below
//                                                          ; for Stream::Play
//
// `idx` at the break is the count of segments strictly BELOW the match --
// retail increments AFTER the comparison, so the value used is the
// pre-increment one. Reproduced with the same two-step loop rather than a
// tidier `std::upper_bound`, because the untidiness (using the index before
// vs after the increment) is exactly the kind of thing that is invisible until
// wrong.
//
// ---------------------------------------------------------------------------
// THE SUBMIT / VOLUME / PITCH / START SEQUENCE (0x1800de8dc..0x1800de9eb)
// ---------------------------------------------------------------------------
//   XAUDIO2_BUFFER     at a local 0x30-byte scratch (Flags=0x40 END_OF_STREAM,
//                       AudioBytes, pAudioData, PlayBegin, PlayLength=0,
//                       LoopBegin=0, LoopLength=0, LoopCount) -- standard field
//                       order and standard field sizes, confirmed against the
//                       exact byte ranges retail writes (RSP+0x50..+0x70).
//   XAUDIO2_BUFFER_WMA-shaped local (pDecodedPacketCumulativeBytes = the SAME
//                       segment-table pointer, PacketCount = segmentCount) --
//                       at RSP+0x40..+0x50, passed as the call's THIRD arg.
//
//   if (!isStreaming) {
//       hr = voiceVtbl[0xa8/8](voice, &buffer, &wma);       ; SubmitSourceBuffer
//       if (hr != 0) goto cleanup;
//   }
//   hr = voiceVtbl[0x60/8](voice, vol, 0);                  ; SetVolume
//   if (hr != 0) goto cleanup;
//   ratio = FUN_180127560(1.0594631f /* 2^(1/12), @0x1801616a0 */, pitch);
//   hr = voiceVtbl[0xd0/8](voice, ratio, 0);                ; SetFrequencyRatio
//   if (hr != 0) goto destroyAndFail;                        ; SKIPS Start entirely
//   hr = voiceVtbl[0x98/8](voice, 0, 0);                    ; Start
//   if (hr == 0) {
//       if (isStreaming)
//           Stream::Play(this + 0xa00, slot, bytePos);       ; 0x1800a0d10,
//                                                            ; THIS SAME BATCH
//       return true;
//   }
// cleanup / destroyAndFail:
//   voiceVtbl[0x90/8](voice);                               ; DestroyVoice
//   *pVoiceSlot = nullptr;
//   return false;
//
// The SetFrequencyRatio-failure branch (`JNZ 0x1800de9c5`) jumps PAST the
// shared `MOV RCX,[RSI]` reload straight into the DestroyVoice call, using the
// voice/vtable it already had in RCX/RAX from the SetFrequencyRatio call --
// same destination, no behavioural difference, reproduced here as one
// `goto`-free `if` because C++ has no reason to distinguish the two entry
// points into the same cleanup.
//
// `third` (the 7th argument) IS IN THE MANGLED SIGNATURE AND NEVER READ BY
// THIS BODY -- checked with a full listing, not merely "not seen so far": no
// instruction in the function touches its stack slot. The caller
// (`audio_scene_update.cpp`) already says as much ("the third PlayPlatform
// float ... NOT named, because this body only forwards it"); this file is
// the confirmation from the callee's side.
//
// `this+0xa00` is `Audio::Device`'s single embedded `Audio::Stream` --
// consistent with `audio_device.cpp`'s own `kChannelCount * kChannelStride
// <= kLockOffset (0xA50)` bound: 64*0x28 = 0xA00 exactly, so the channel table
// ends exactly where the Stream begins.
//
// ---------------------------------------------------------------------------
// FUN_180127560 IS A PLAIN CRT `powf`, AND IT IS OUT OF THIS BATCH'S SCOPE
// ---------------------------------------------------------------------------
// Decompiled to CHECK what it is, not to port it: it is the textbook
// edge-case-laden IEEE754 `powf(base, exponent)` (NaN/Inf/zero/sign special
// cases, a log2/exp2 polynomial core, `FUN_18012a940` as the shared
// error-code path) -- not a Banished-specific algorithm. It is not one of this
// batch's five worklist rows, so it stays unregistered and this call reaches
// it through `ResolveAddr`, which tries the port registry first and falls
// back to the DLL -- the same door `audio_stream_win.cpp`'s
// `ReleaseField30Buffer` opens onto 0x18000c4a0, and `audio_stream_play_win.cpp`
// (this batch) opens onto 0x18000c300, for the identical reason.
//
// 0x1801616a0 = 0x3f873c9e = 1.0594631433486938f = 2^(1/12), the semitone
// ratio -- so `pitch` is consumed as a SIGNED SEMITONE OFFSET and this call is
// `pow(2^(1/12), pitch)`, the standard equal-temperament pitch-to-frequency-
// -ratio conversion. Read as the bit pattern's VALUE, matching
// `reference_crt_coefficients_must_be_lifted_not_retyped.md`'s rule for
// literals pulled out of `.rdata` (this one is the CONSTANT ARGUMENT, not a
// tempering coefficient, so it is spelled as the float value rather than the
// hex bit pattern; both name the same IEEE754 bits).
//
// ---------------------------------------------------------------------------
// ORACLE -- NONE, and the reason is the same class the whole Audio cluster
// gives for anything that touches a live XAudio2 voice
// ---------------------------------------------------------------------------
// This function creates a REAL XAudio2 source voice, submits a REAL buffer,
// and calls REAL Start/SetVolume/SetFrequencyRatio on a REAL COM object -- i.e.
// it starts audible sound. A replay would either double-submit a buffer to a
// second voice (if allowed to create one) or corrupt the slot's single voice
// pointer. `audio_scene_update.cpp`'s own `Scene::Update` faces the identical
// class of side effect and settles for attendance counters over an oracle;
// this file does the same, for the same reason.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

// ---- Audio::Device offsets (no Device.h -- Audio.h's own header notes it
// carries neither Device nor SoundBuffer; these are single-reader byte offsets
// per PLAN §6 step 8, cross-confirmed where noted) --------------------------
constexpr int kOffXAudio2         = 0xa88;  // IXAudio2* (audio_platform_startup_win.cpp)
constexpr int kVoiceCallbackBase  = 0xa98;  // &VoiceCallback[slot], stride 0x20
constexpr int kVoiceCallbackVoice = 0x08;   // VoiceCallback::voice, +0x08 (audio_voice_callback_win.cpp)
constexpr int kVoiceCallbackStride = 0x20;
constexpr int kDeviceStreamOffset = 0xa00;  // the embedded Audio::Stream

// ---- SoundBuffer offsets, all pinned by an instruction cited in the banner -
constexpr int kSbLoopForever   = 0x20;  // byte
constexpr int kSbStreaming     = 0x21;  // byte
constexpr int kSbLoopDuration  = 0x24;  // float, seconds
constexpr int kSbAudioData     = 0x38;  // const void*
constexpr int kSbAudioBytes    = 0x40;  // uint32
constexpr int kSbSegmentTable  = 0x48;  // const uint32_t*
constexpr int kSbSegmentBytes  = 0x50;  // int32 -- byte length of the table
constexpr int kSbFormat        = 0x54;  // embedded WAVEFORMATEX
constexpr int kWfChannels       = kSbFormat + 0x02;
constexpr int kWfSamplesPerSec  = kSbFormat + 0x04;
constexpr int kWfBlockAlign     = kSbFormat + 0x0c;
constexpr int kWfBitsPerSample  = kSbFormat + 0x0e;

// ---- XAudio2 (2.7) vtable byte offsets used by THIS body, each pinned by the
// argument shapes in the banner (not by a header this project does not carry)
constexpr int kEngineCreateSourceVoice = 0x40;
constexpr int kVoiceSubmitSourceBuffer = 0xa8;
constexpr int kVoiceSetVolume          = 0x60;
constexpr int kVoiceSetFrequencyRatio  = 0xd0;
constexpr int kVoiceStart              = 0x98;
constexpr int kVoiceDestroy            = 0x90;

constexpr uint32_t kXAudio2EndOfStream = 0x40;
constexpr uint32_t kXAudio2LoopInfinite = 0xffffffffu;
constexpr float    kMaxFrequencyRatio  = 2.0f;      // 0x1801616d8
constexpr float    kSemitoneRatio      = 1.0594631433486938f;  // 0x1801616a0, 2^(1/12)

uint64_t g_calls = 0;
uint64_t g_voicesCreated = 0;
uint64_t g_placed = 0;

struct PlayPlatformSummary {
    ~PlayPlatformSummary() {
        if (!g_calls) return;
        printf("[RtPort] Audio::Device::PlayPlatform ATTENDANCE: calls=%llu "
                    "voicesCreated=%llu placed=%llu (NO ORACLE: starts real "
                    "XAudio2 playback; see the banner)\n",
                    (unsigned long long)g_calls, (unsigned long long)g_voicesCreated,
                    (unsigned long long)g_placed);
        fflush(stdout);
    }
};
PlayPlatformSummary g_summary;

template <typename T> inline T Load(const void* p, int off) {
    T v; std::memcpy(&v, static_cast<const uint8_t*>(p) + off, sizeof(T)); return v;
}
template <typename T> inline void Store(void* p, int off, T v) {
    std::memcpy(static_cast<uint8_t*>(p) + off, &v, sizeof(T));
}

double SecondsPerTick() {
    double* slot = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return slot ? *slot : 0.0;
}

// FUN_180127560 -- a plain CRT powf(base, exponent). Out of this batch's
// scope; reached by ADDRESS, port registry first, DLL fallback. See the banner.
float PowF(float base, float exponent) {
    using Fn = float (*)(float, float);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x180127560));
    return fn ? fn(base, exponent) : 1.0f;
}

// The XAUDIO2_BUFFER shape this body builds, standard field order/sizes.
struct Xa2Buffer {
    uint32_t    flags;        // +0x00
    uint32_t    audioBytes;   // +0x04
    const void* audioData;    // +0x08
    uint32_t    playBegin;    // +0x10
    uint32_t    playLength;   // +0x14
    uint32_t    loopBegin;    // +0x18
    uint32_t    loopLength;   // +0x1c
    uint32_t    loopCount;    // +0x20
    void*       context;      // +0x28 -- retail leaves this stack slot
                              //          uninitialised; nothing reads it (the
                              //          voice-callback OnBufferEnd ignores its
                              //          own pBufferContext, per
                              //          audio_voice_callback_win.cpp), so it is
                              //          zeroed here rather than left as C++ UB.
};
static_assert(sizeof(Xa2Buffer) == 0x30, "XAUDIO2_BUFFER-shaped local must be 0x30");

// The XAUDIO2_BUFFER_WMA shape, passed as the SubmitSourceBuffer call's 3rd arg.
struct Xa2BufferWma {
    const uint32_t* cumulativeBytes;  // +0x00
    uint32_t         packetCount;       // +0x08
    uint32_t         _pad;              // +0x0c
};
static_assert(sizeof(Xa2BufferWma) == 0x10, "XAUDIO2_BUFFER_WMA-shaped local must be 0x10");

// Computes bytePos (used unconditionally, including for Stream::Play) and
// playBegin (used only when NOT streaming). See the banner for the exact
// instruction sequence this reproduces.
void ComputePlayPosition(const void* buf, int64_t nowTicks,
                         int32_t* outBytePos, uint32_t* outPlayBegin) {
    const double spt = SecondsPerTick();
    const float loopDurationSeconds = Load<float>(buf, kSbLoopDuration);
    const double loopDurationTicksD = static_cast<double>(loopDurationSeconds) / spt;
    const int64_t loopDurationTicks = static_cast<int64_t>(loopDurationTicksD);  // CVTTSD2SI

    const int64_t remainderTicks = (loopDurationTicks != 0) ? (nowTicks % loopDurationTicks) : 0;
    const double secondsInLoopD = static_cast<double>(remainderTicks) * spt;
    const float secondsInLoop = static_cast<float>(secondsInLoopD);  // CVTPD2PS

    const uint32_t nChannels      = Load<uint16_t>(buf, kWfChannels);
    const uint32_t wBitsPerSample = Load<uint16_t>(buf, kWfBitsPerSample);
    const uint32_t nSamplesPerSec = Load<uint32_t>(buf, kWfSamplesPerSec);
    uint32_t bitsPerSecondAllCh = nChannels * wBitsPerSample;    // 32-bit wraparound,
    bitsPerSecondAllCh = bitsPerSecondAllCh * nSamplesPerSec;    // reproduced as-is
    const uint32_t bytesPerSecond = bitsPerSecondAllCh >> 3;     // logical shift

    const float bytePosF = secondsInLoop * static_cast<float>(static_cast<int32_t>(bytesPerSecond));
    const int32_t bytePos = static_cast<int32_t>(bytePosF);  // CVTTSS2SI, truncate

    uint32_t playBegin = 0;
    const int64_t segByteLen = static_cast<int32_t>(Load<int32_t>(buf, kSbSegmentBytes));
    const int64_t segCount = static_cast<uint64_t>(segByteLen) >> 2;  // matches the
                                                                       // sign-extend-then-
                                                                       // logical-shift retail does
    if (segCount > 0) {
        const uint32_t* segTable = Load<const uint32_t*>(buf, kSbSegmentTable);
        const uint16_t nBlockAlign = Load<uint16_t>(buf, kWfBlockAlign);
        for (int64_t idx = 0; ; ) {
            if (segTable[idx] > static_cast<uint32_t>(bytePos)) {  // JA, unsigned
                playBegin = static_cast<uint32_t>(nBlockAlign) * static_cast<uint32_t>(idx);
                break;
            }
            if (++idx >= segCount) break;  // not found: playBegin stays 0
        }
    }

    *outBytePos = bytePos;
    *outPlayBegin = playBegin;
}

}  // namespace

// ============================================================================
// Audio::Device::PlayPlatform — retail 0x1800de7a0, ord 3424
// ============================================================================
extern "C" bool RtAudio_Device_PlayPlatform(void* device, int32_t slot, void* bufferRef,
                                            const int64_t* startTime, float vol,
                                            float pitch, float third) {
    (void)third;  // read by NOTHING in retail -- see the banner
    ++g_calls;

    auto* self = static_cast<uint8_t*>(device);

    void* engine = Load<void*>(self, kOffXAudio2);                        // 1800de7b4
    if (!engine) {                                                        // 1800de7c4
        return false;
    }

    uint8_t* record = self + kVoiceCallbackBase + static_cast<int64_t>(slot) * kVoiceCallbackStride;
    void** voiceSlot = reinterpret_cast<void**>(record + kVoiceCallbackVoice);

    // ---- CreateSourceVoice ---------------------------------------------
    using CreateSourceVoiceFn = int32_t (*)(void* engine, void** ppVoice, const void* pFormat,
                                            uint32_t flags, float maxFreqRatio, void* pCallback,
                                            void* pSendList, void* pEffectChain);
    void** engineVtbl = *reinterpret_cast<void***>(engine);
    auto createSourceVoice = reinterpret_cast<CreateSourceVoiceFn>(engineVtbl[kEngineCreateSourceVoice / 8]);
    const void* pFormat = static_cast<const uint8_t*>(bufferRef) + kSbFormat;   // 1800de817
    int32_t hr = createSourceVoice(engine, voiceSlot, pFormat, 0, kMaxFrequencyRatio,
                                   record, nullptr, nullptr);                    // 1800de827
    if (hr != 0) {                                                              // 1800de82a
        return false;
    }
    ++g_voicesCreated;
    void* voice = *voiceSlot;

    // ---- byte position / PlayBegin -------------------------------------
    int32_t bytePos = 0;
    uint32_t playBegin = 0;
    ComputePlayPosition(bufferRef, *startTime, &bytePos, &playBegin);

    const bool loopForever = Load<uint8_t>(bufferRef, kSbLoopForever) != 0;
    const bool isStreaming = Load<uint8_t>(bufferRef, kSbStreaming) != 0;
    if (isStreaming) playBegin = 0;  // CMOVNZ, unconditional override

    Xa2Buffer buf{};
    buf.flags      = kXAudio2EndOfStream;
    buf.audioBytes = Load<uint32_t>(bufferRef, kSbAudioBytes);
    buf.audioData  = Load<const void*>(bufferRef, kSbAudioData);
    buf.playBegin  = playBegin;
    buf.playLength = 0;
    buf.loopBegin  = 0;
    buf.loopLength = 0;
    buf.loopCount  = loopForever ? kXAudio2LoopInfinite : 0u;
    buf.context    = nullptr;

    Xa2BufferWma wma{};
    wma.cumulativeBytes = Load<const uint32_t*>(bufferRef, kSbSegmentTable);
    wma.packetCount     = static_cast<uint32_t>(
        static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(
            Load<int32_t>(bufferRef, kSbSegmentBytes)))) >> 2);

    void** voiceVtbl = *reinterpret_cast<void***>(voice);

    using SubmitSourceBufferFn = int32_t (*)(void*, const void*, const void*);
    using SetFloatOpFn         = int32_t (*)(void*, float, uint32_t);
    using StartFn              = int32_t (*)(void*, uint32_t, uint32_t);
    using DestroyVoiceFn       = void    (*)(void*);

    auto cleanup = [&]() {
        auto destroy = reinterpret_cast<DestroyVoiceFn>(voiceVtbl[kVoiceDestroy / 8]);
        destroy(voice);
        *voiceSlot = nullptr;
    };

    if (!isStreaming) {                                                      // 1800de933
        auto submit = reinterpret_cast<SubmitSourceBufferFn>(
            voiceVtbl[kVoiceSubmitSourceBuffer / 8]);
        hr = submit(voice, &buf, &wma);                                      // 1800de948
        if (hr != 0) {                                                       // 1800de950
            cleanup();
            return false;
        }
    }

    auto setVolume = reinterpret_cast<SetFloatOpFn>(voiceVtbl[kVoiceSetVolume / 8]);
    hr = setVolume(voice, vol, 0);                                           // 1800de964
    if (hr != 0) {                                                           // 1800de967
        cleanup();
        return false;
    }

    const float ratio = PowF(kSemitoneRatio, pitch);                         // 1800de98a
    auto setFreqRatio = reinterpret_cast<SetFloatOpFn>(voiceVtbl[kVoiceSetFrequencyRatio / 8]);
    hr = setFreqRatio(voice, ratio, 0);                                      // 1800de998
    if (hr != 0) {                                                           // 1800de9ae
        cleanup();  // SetFrequencyRatio failure skips Start entirely
        return false;
    }

    auto start = reinterpret_cast<StartFn>(voiceVtbl[kVoiceStart / 8]);
    hr = start(voice, 0, 0);                                                 // 1800de9b5
    if (hr != 0) {                                                           // 1800de9bd
        cleanup();
        return false;
    }

    ++g_placed;
    if (isStreaming) {                                                       // 1800de9d5
        using StreamPlayFn = void (*)(void*, int32_t, int32_t);
        static StreamPlayFn streamPlay = nullptr;
        if (!streamPlay)
            streamPlay = reinterpret_cast<StreamPlayFn>(
                ResolveExport("?Play@Stream@Audio@@QEAAXHH@Z"));
        if (streamPlay) streamPlay(self + kDeviceStreamOffset, slot, bytePos);  // 1800de9e4
    }

    return true;                                                             // 1800de9e9
}

// ============================================================================
// Registration — unconditional (DIRECTION.md: a port registers unconditionally,
// no new kill switches).
// ============================================================================
int RtAudioPlayPlatformRegister() {
    int n = 0;
    RegisterRuntimeExport(
        "?PlayPlatform@Device@Audio@@AEAA_NHV?$ExternalPtr@VSoundBuffer@Audio@@@Object@@AEBVTime@System@@MMM@Z",
        (void*)&RtAudio_Device_PlayPlatform);
    RegisterRuntimeAddr(0x1800de7a0, (void*)&RtAudio_Device_PlayPlatform); ++n;
    return n;
}
