// ============================================================================
// runtime/src/audio/audio_voice_callback_win.cpp — WAVE 28 lane SLOTS
//
// The seven slots of the UNEXPORTED vtable at DLL 0x180157c60.
// Before this file the backlog row read `0x180157c60  0 / 7` — the single
// largest well-defined body gap left in the promotion backlog, and the only
// one whose table had no name at all.
// ============================================================================
//
// ---------------------------------------------------------------------------
// 1. WHAT THE TABLE IS.  IDENTIFIED — and the identification is a chain of
//    MEASUREMENTS, not a resemblance.
// ---------------------------------------------------------------------------
// `0x180157c60` is the vtable of `IXAudio2VoiceCallback` as implemented by an
// internal, unexported helper class embedded 64 times inside `Audio::Device`,
// one per audio channel, at `Device+0xa98`, stride 0x20.
//
// **The class has NO NAME in the shipped binary**: it is not exported, no
// `??_7…@Audio@@6B@` points here, and nothing in the retail `.rdata` names it.
// The name used in this file — `VoiceCallback` — is DESCRIPTIVE, chosen by this
// lane, exactly as `encoding.cpp` chose `IsBinary`/`IsText`/`GetKind` for the
// unexported 0x1801554a8 table. Do not read it as attested.
//
// What IS attested, each item read out of `C:\GOG Games\Banished\Runtime-x64.dll`
// on disk with `objdump` (true-VA disassembly) and `pefile` (export directory,
// import directory, raw `.rdata`) — never through Ghidra:
//
//   a) `??0Device@Audio@@QEAA@XZ` (0x1800de3a0) stamps this exact pointer into
//      64 records at `Device+0xa98` stride 0x20:
//          1800de3f6: lea  0xa98(%rbx),%rax
//          1800de3fd: lea  0x7985c(%rip),%rcx     # 0x180157c60
//          1800de410: mov  %rcx,(%rax)  … lea 0x20(%rax),%rax ; dec %edi ; jns
//      There are exactly two RIP-relative references to 0x180157c60 in the
//      whole `.text` (scanned exhaustively): this one and the single-object
//      constructor at 0x1800501a0. No other reference of any kind exists.
//
//   b) `?PlayPlatform@Device@Audio@@AEAA…` (0x1800de7a0) hands that record to
//      XAudio2 as the `pCallback` argument of `CreateSourceVoice`:
//          1800de7e8: shl  $0x5,%rax                 ; i * 0x20
//          1800de7ec: lea  0xa98(%r15),%rdx          ; base
//          1800de7fb: add  %rax,%rdx                 ; &record[i]
//          1800de809: lea  0x8(%rdx),%rsi            ; &record[i].voice
//          1800de812: mov  %rdx,0x28(%rsp)           ; <-- ARG 5 = pCallback
//          1800de827: call *0x40(%rax)               ; IXAudio2::CreateSourceVoice
//
//   c) The interface is pinned by TWO further vtable indices on the same COM
//      objects, both read from the same disassembly:
//          `call *0x50(%rax)` on the IXAudio2  = index 10 = CreateMasteringVoice
//          `call *0x90(%rax)` on a voice       = index 18 = IXAudio2Voice::DestroyVoice
//      Index 8 (0x40) = CreateSourceVoice and index 10 (0x50) =
//      CreateMasteringVoice agree simultaneously; that pair is unique to the
//      XAudio2 2.7 `IXAudio2` layout. `IXAudio2VoiceCallback` has exactly SEVEN
//      methods and this table has exactly seven slots.
//
//   d) The one import the table touches was read from the DLL's OWN import
//      directory with pefile, not guessed: `0x18014b368` is
//      `KERNEL32.dll!SetEvent`. (`audio_stream_win.cpp` names the same slot.)
//
//   e) The HANDLE it signals is written by `?Run@Stream@Audio@@` at 0x1800a114d:
//          mov 0x48(%rsi),%rax        ; Audio::Stream's CreateEventW handle
//          mov %rax,0x18(%r14)        ; r14 = Device+0xa98 + channel*0x20
//      so `+0x18` is a real event HANDLE with a real writer. That is a
//      MEASUREMENT of the writer, not N readers agreeing on a shape
//      (reference_a_writer_beats_readers_agreeing_on_a_sign.md).
//
// ---------------------------------------------------------------------------
// 2. THE RECORD LAYOUT — every field has a writer that was read
// ---------------------------------------------------------------------------
//   +0x00  vptr                -> 0x180157c60            (Device ctor loop 2)
//   +0x08  IXAudio2SourceVoice*                          (CreateSourceVoice out
//                                                         param; DestroyVoice'd
//                                                         by FreeChannelPlatform
//                                                         at Device+0xaa0+i*0x20)
//   +0x10  AudioChannel*       -> &Device->channels[i]   (Device ctor loop 3:
//                                                         `mov %rcx,(%rax)` with
//                                                         rax = Device+0xaa8+i*0x20)
//   +0x18  HANDLE              -> Audio::Stream's +0x48 event (Stream::Run)
//
// `audio_device.cpp` already models `AudioChannel` (0x28 bytes, `flags` at
// +0x00). THIS FILE DOES NOT REDEFINE IT — the bit-1 set below is the *writer*
// of the very bit `Audio::Device::Update` reads as "finished, reap me", and the
// two readings were derived independently and agree.
//
// ---------------------------------------------------------------------------
// 3. THE SEVEN BODIES, BYTE FOR BYTE
// ---------------------------------------------------------------------------
// Transcribed from `objdump -d --start-address=0x1800501c0
// --stop-address=0x180050240 "C:/GOG Games/Banished/Runtime-x64.dll"`. The
// mint's slot order (standalone/rt_data_mint_tables.inc kVSlots2) was checked
// against the raw `.rdata` bytes with pefile and agrees slot for slot.
//
//   slot 0  0x180050200  c2 00 00           ret $0x0     OnVoiceProcessingPassStart
//   slot 1  0x1800501f0  c2 00 00           ret $0x0     OnVoiceProcessingPassEnd
//   slot 2  0x1800501c0  48 8b 41 10        mov 0x10(%rcx),%rax      OnStreamEnd
//                        83 08 02           orl $0x2,(%rax)
//                        c3                 ret
//   slot 3  0x180050210  c2 00 00           ret $0x0     OnBufferStart
//   slot 4  0x1800501d0  48 8b 49 18        mov 0x18(%rcx),%rcx      OnBufferEnd
//                        48 85 c9           test %rcx,%rcx
//                        74 07              je  0x1800501e0
//                        48 ff 25 ..        jmp *0xfb188(%rip)  # SetEvent
//                        f3 c3              repz ret
//   slot 5  0x180050220  c2 00 00           ret $0x0     OnLoopEnd
//   slot 6  0x180050230  c2 00 00           ret $0x0     OnVoiceError
//
// `ret $0x0` rather than `ret` is MSVC's way of giving five otherwise-identical
// empty bodies five distinct addresses so COMDAT folding cannot merge them —
// and the table DOES hold five distinct addresses, so the anti-folding worked.
// Reproducing that is neither possible nor necessary from C++: what a caller
// observes is "returns, touching nothing", and five separate C++ functions give
// five separate addresses for the same reason.
//
// SLOT NAMES ARE INFERRED FROM THE INTERFACE, NOT FROM THE DLL. The mapping
// slot->method is the documented `IXAudio2VoiceCallback` order. The two
// NON-EMPTY slots corroborate it and are the only ones where a mis-mapping
// could do harm:
//   * slot 2 (OnStreamEnd) sets the channel's "finished" bit — which is exactly
//     what a stream-end notification must do, and exactly the bit
//     `Device::Update` reaps on.
//   * slot 4 (OnBufferEnd) signals the streaming thread's buffer event — which
//     is the textbook use of OnBufferEnd and matches the writer in Stream::Run.
// The five empty ones cannot be distinguished by behaviour at all, so for them
// the names are LABELS and nothing depends on them.
//
// ---------------------------------------------------------------------------
// 4. NO ORACLE, and the reason is structural, not convenience
// ---------------------------------------------------------------------------
// These are callbacks XAUDIO2 invokes, on XAudio2's own audio thread. There is
// no call site in our tree and never will be one: the caller is inside
// `XAudio2_*.dll`. A differential replay is impossible in both directions —
// running retail's copy after ours would set the bit twice (harmless but
// unobserved) and SIGNAL THE EVENT TWICE, which hands the streaming thread a
// spurious wake-up. `reference_uncalled_port_is_not_verified.md` applies in
// full, and this file claims nothing about attendance.
//
// UNVERIFIED: none of these seven bodies has been observed executing.
//
// ---------------------------------------------------------------------------
// 5. WHY THIS ROW IS DIFFERENT FROM EVERY OTHER BACKLOG ROW
// ---------------------------------------------------------------------------
// For a `??_7`-exported table, closing it to COMPLETE buys nothing until
// somebody establishes ownership of the stamping function. Here WE ALREADY OWN
// THE STAMPER: `Ctor_AudioDevice` (runtime/src/engine/singleton_ctors.cpp, the
// port of `??0Device@Audio@@QEAA@XZ`) reads
// `ResolveAddr(Module::Runtime, 0x180157c60)` and stamps whatever that returns
// into all 64 records. So every Device our constructor builds carries the
// minted table, and with these seven registrations the minted table holds our
// bodies instead of seven `RtMintTrapStub_2_*` aborts.
//
// That is NOT a promotion and must not be reported as one. The table is
// address-keyed and UNEXPORTED, so it can never enter the census's `promoted`
// bucket, which only a `??_7` symbol can (see `encoding.cpp`'s registration
// banner, which makes the identical point about 0x1801554a8). What changes is
// that the row goes 0/7 -> 7/7 COMPLETE and that a dispatch reaching it at
// cutover runs our code rather than aborting.
// ============================================================================

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"

namespace RtAudio {

// The per-channel XAudio2 voice-callback record. Modelled here and nowhere else;
// `audio_device.cpp` models the SIBLING array (AudioChannel, stride 0x28) and
// the two are cross-linked by the Device constructor's third loop.
struct VoiceCallback {
    const void* vptr;      // +0x00  0x180157c60
    void*       voice;     // +0x08  IXAudio2SourceVoice*
    void*       channel;   // +0x10  Audio::Device::AudioChannel* (stride 0x28)
    HANDLE      bufferEnd; // +0x18  Audio::Stream's +0x48 event, or null
};
static_assert(sizeof(VoiceCallback) == 0x20,
              "Audio VoiceCallback record must be 0x20 (Device ctor loop 2 stride)");
static_assert(offsetof(VoiceCallback, voice)     == 0x08, "voice @ +0x08");
static_assert(offsetof(VoiceCallback, channel)   == 0x10, "channel @ +0x10");
static_assert(offsetof(VoiceCallback, bufferEnd) == 0x18, "bufferEnd @ +0x18");

// The bit slot 2 raises. `audio_device.cpp`'s `Update` tests `flags & 2` as
// "finished, reap me" and its reap clears bits 0 and 1 (`and $0xfffffffc`).
// This is the WRITER of that bit; the two were read independently.
static const uint32_t kChannelFinished = 0x2u;

// ---- slot 0  0x180050200 ---------------------------------------------------
// IXAudio2VoiceCallback::OnVoiceProcessingPassStart(UINT32 BytesRequired)
extern "C" void RtAudio_VoiceCallback_OnVoiceProcessingPassStart(void* self,
                                                                 uint32_t bytesRequired) {
    (void)self; (void)bytesRequired;   // retail: `ret` — RCX and EDX never read
}

// ---- slot 1  0x1800501f0 ---------------------------------------------------
// IXAudio2VoiceCallback::OnVoiceProcessingPassEnd()
extern "C" void RtAudio_VoiceCallback_OnVoiceProcessingPassEnd(void* self) {
    (void)self;
}

// ---- slot 2  0x1800501c0 ---------------------------------------------------
// IXAudio2VoiceCallback::OnStreamEnd()
//
// UNGUARDED on purpose. Retail dereferences `channel` with no null test, and
// it is right to: the Device constructor's third loop points every one of the
// 64 records at its own AudioChannel before any voice can exist. A guard here
// would not restore the behaviour, it would skip it (CLAUDE.md, "Don't 'fix' a
// faithful crash with a null check").
extern "C" void RtAudio_VoiceCallback_OnStreamEnd(void* self) {
    VoiceCallback* cb = static_cast<VoiceCallback*>(self);
    *static_cast<uint32_t*>(cb->channel) |= kChannelFinished;
}

// ---- slot 3  0x180050210 ---------------------------------------------------
// IXAudio2VoiceCallback::OnBufferStart(void* pBufferContext)
extern "C" void RtAudio_VoiceCallback_OnBufferStart(void* self, void* bufferContext) {
    (void)self; (void)bufferContext;
}

// ---- slot 4  0x1800501d0 ---------------------------------------------------
// IXAudio2VoiceCallback::OnBufferEnd(void* pBufferContext)
//
// The null test IS retail's own (`test %rcx,%rcx ; je`), not a port's caution:
// the Device constructor zeroes `+0x18` and only `Stream::Run` ever fills it,
// so a non-streamed voice reaches here with a null handle every time.
// `pBufferContext` is ignored by retail — RDX is never read.
extern "C" void RtAudio_VoiceCallback_OnBufferEnd(void* self, void* bufferContext) {
    (void)bufferContext;
    VoiceCallback* cb = static_cast<VoiceCallback*>(self);
    if (cb->bufferEnd != nullptr)
        SetEvent(cb->bufferEnd);            // tail-jmp *[0x18014b368] in retail
}

// ---- slot 5  0x180050220 ---------------------------------------------------
// IXAudio2VoiceCallback::OnLoopEnd(void* pBufferContext)
extern "C" void RtAudio_VoiceCallback_OnLoopEnd(void* self, void* bufferContext) {
    (void)self; (void)bufferContext;
}

// ---- slot 6  0x180050230 ---------------------------------------------------
// IXAudio2VoiceCallback::OnVoiceError(void* pBufferContext, HRESULT Error)
extern "C" void RtAudio_VoiceCallback_OnVoiceError(void* self, void* bufferContext,
                                                   int32_t error) {
    (void)self; (void)bufferContext; (void)error;
}

}  // namespace RtAudio

// ============================================================================
// Registration — ADDRESS DOORS ONLY, because there is no name to register.
// ============================================================================
// Not one of the seven is exported: checked against the retail export directory
// with pefile (4,926 named exports scanned; none resolves to any of these VAs,
// and the nearest preceding export to 0x1800501a0 is
// `??4VideoProgram@Video@@QEAAAEAV01@AEBV01@@Z` +0x12a0, i.e. far away).
// `AEAA` means PRIVATE, not unexported — so "unexported" was MEASURED here
// rather than inferred from a mangling
// (reference_aeaa_is_private_not_unexported.md).
//
// An `RT_MINT_UNNAMED` slot resolves through `RtPortLookupAddr` on its `dllVa`,
// so these seven `RegisterRuntimeAddr` calls are exactly what turns
// `kVSlots2[0..6]` from `RtMintTrapStub_2_*` aborts into our bodies. Same door
// `encoding.cpp` uses for the five slots of 0x1801554a8.
// ============================================================================
int RtAudioVoiceCallbackRegister() {
    using namespace RtAudio;
    if (!RtWaveEnabled("BANISHED_RT_AUDIOVOICECB")) {
        printf("[RtPort] BANISHED_RT_AUDIOVOICECB=0 -- the 7 slots of the "
               "unexported XAudio2 voice-callback vtable 0x180157c60 left on "
               "the DLL\n");
        return 0;
    }
    int n = 0;
    RegisterRuntimeAddr(0x180050200,
        (void*)&RtAudio_VoiceCallback_OnVoiceProcessingPassStart); ++n;
    RegisterRuntimeAddr(0x1800501f0,
        (void*)&RtAudio_VoiceCallback_OnVoiceProcessingPassEnd);   ++n;
    RegisterRuntimeAddr(0x1800501c0,
        (void*)&RtAudio_VoiceCallback_OnStreamEnd);                ++n;
    RegisterRuntimeAddr(0x180050210,
        (void*)&RtAudio_VoiceCallback_OnBufferStart);              ++n;
    RegisterRuntimeAddr(0x1800501d0,
        (void*)&RtAudio_VoiceCallback_OnBufferEnd);                ++n;
    RegisterRuntimeAddr(0x180050220,
        (void*)&RtAudio_VoiceCallback_OnLoopEnd);                  ++n;
    RegisterRuntimeAddr(0x180050230,
        (void*)&RtAudio_VoiceCallback_OnVoiceError);               ++n;
    return n;
}
