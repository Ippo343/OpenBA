// ============================================================================
// runtime/src/audio/audio_device.cpp — Wave 9 lane IOA
//   Audio::Device::Update  +  Audio::Device::FreeChannelPlatform
// ============================================================================
//
//   ?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z   retail 0x1800DE520  ord 4481
//   ?FreeChannelPlatform@Device@Audio@@AEAAXH@Z      retail 0x1800DE6F0  ord 2380
//                                                    (AEAA = PRIVATE member)
//
// Both names were checked against the shipped DLL's export directory with
// pefile BEFORE this file was written; both are present at the addresses above.
// Separate file from audio.cpp because these two need <windows.h> for the
// critical section (PLAN.md §4.1 decision 1's platform seam), and audio.cpp
// must stay free of it.
//
// ---------------------------------------------------------------------------
// WHAT THE STATED BLOCKER SAID, AND WHAT SURVIVES OF IT
// ---------------------------------------------------------------------------
// audio.cpp's own header and WAVE8_5.md §9 hold these two back like this:
//
//     "?Update@Device@Audio@@… 0x1800de520 — Device internals (platform
//      channel completion callbacks). Here the device IS the subject, not an
//      unreachable callee, so no synthetic Scene helps."
//     "?FreeChannelPlatform@Device@Audio@@…ord2380 — PRIVATE member, COM
//      vtable call at (**channel)+0x90, critical section at Device+0xa50.
//      Same class as Device::Update, and for the same reason."
//
// Every fact in those two paragraphs is TRUE and this lane confirms all of
// them. What they establish, though, is that these two functions have NO
// ORACLE — not that they cannot be ported. Wave 8.5 was answering "can I
// verify this by replay?"; a cutover row asks "is this symbol a null pointer
// when the DLL is gone?", and lane Z's triage rule for that question is
// whether every callee is satisfied on our side. Here they are:
//
//   Update            -> FreeChannelPlatform (this file), plus an indirect
//                        `CALL RAX` through a callback pointer stored in the
//                        channel record. No Runtime callee at all.
//   FreeChannelPlatform -> EnterCriticalSection / LeaveCriticalSection
//                        (KERNEL32 imports of the DLL, linked directly here),
//                        plus one virtual call through slot +0x90 of the
//                        platform voice object. No Runtime callee at all.
//
// Neither body calls a single unexported Runtime function by address. The
// hold-back was an ORACLE verdict being read as a PORT verdict; the oracle
// verdict is preserved below, in full, and this file adds no oracle it cannot
// justify.
//
// ---------------------------------------------------------------------------
// ORACLE — NONE, and the reason is the same stored callback
// ---------------------------------------------------------------------------
// `Update` reaps every finished channel and FIRES ITS STORED COMPLETION
// CALLBACK. Running the DLL's copy afterwards would find nothing left to reap
// (our body already cleared the flags) and, on the first pass, would have
// fired every callback a second time. `FreeChannelPlatform` releases a COM
// voice through a vtable slot; calling it twice releases a released object.
// A replay here is a SECOND SIDE EFFECT, not a comparison, and this file does
// not manufacture one.
//
// What it does instead is `RT_DIFF_WORK` on both sites, because `calls=`
// counts calls ENTERED and both of these are entered constantly while doing
// nothing:
//   * `Update` runs every frame and reaps zero channels in almost all of them.
//     Its work verdict is "at least one channel had the reap bit set".
//   * `FreeChannelPlatform` returns at its first instruction whenever the
//     platform is not up (`Device+0xA88 == 0`), which is the whole of a
//     no-audio-device run. Its work verdict is "the platform was up".
// A run whose work column is zero has NOT verified these bodies, and the
// counter is what makes that visible instead of leaving `calls=` to imply it.
//
// ---------------------------------------------------------------------------
// LAYOUT — every offset below is CROSS-CONFIRMED by a THIRD function
// ---------------------------------------------------------------------------
// The channel stride and base are not inferred from `Update`'s loop alone.
// `Audio::Device::Stop` (0x1800DE200, ord 4105 — already reached by name from
// audio.cpp's ReleasePlatformChannels) takes an `AudioChannel*` and converts
// it to an index with `SUB RDX,RCX` followed by the signed-division idiom
// `MOV RAX,0x6666666666666667 ; IMUL RDX ; SAR RDX,4` — division by 0x28.
// That independently pins BOTH that the array starts at Device+0x00 AND that
// its stride is 0x28. `Stop` then runs the identical five-instruction reap
// epilogue at 0x1800DE23F..0x1800DE268 that `Update` runs at
// 0x1800DE560..0x1800DE57B, which is what the callback's three arguments are
// read from.
//
// The platform-side offsets are cross-confirmed by `Device::StopPlatform`
// (0x1800DEA20), the sibling `Stop` calls just before `FreeChannelPlatform`:
// it opens with the same `CMP qword ptr [RCX+0xA88],0` gate, indexes the same
// `[this + idx*0x20 + 0xAA0]` voice pointer, and calls slot +0xA0 on it where
// `FreeChannelPlatform` calls slot +0x90.
// ============================================================================

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include "../../funcs/rt_port.h"

namespace {

// ---------------------------------------------------------------------------
// Audio::Device::AudioChannel — 0x28 bytes, 64 of them at Device+0x00.
// Only the five fields these two bodies (and `Stop`) touch are named; the rest
// is left as an unnamed gap rather than guessed, per PLAN §6 step 8.
// ---------------------------------------------------------------------------
struct AudioChannel {
    uint32_t flags;       // +0x00  dword. bit1 tested by Update ("finished,
                          //        reap me"); bits 0+1 cleared by the reap.
    uint32_t _pad04;      // +0x04  never read or written by any body here
    void*    source;      // +0x08  the sound object. Byte [+0x21] of it is the
                          //        "this playback needs the device lock" flag
                          //        FreeChannelPlatform and StopPlatform test.
                          //        Cleared by the reap.
    void*    userData;    // +0x10  2nd argument to the completion callback
    uint32_t userValue;   // +0x18  3rd argument (a DWORD, read as R8D)
    uint32_t _pad1c;      // +0x1c
    void*    onComplete;  // +0x20  THE STORED COMPLETION CALLBACK — the
                          //        `CALL RAX` that WAVE8_5.md §9 identified as
                          //        the single root cause of all six Audio
                          //        hold-backs. It is a faithful indirect call
                          //        with no Runtime dependency of its own.
};
static_assert(sizeof(AudioChannel) == 0x28, "Audio::Device::AudioChannel must be 0x28");
static_assert(offsetof(AudioChannel, source)     == 0x08, "AudioChannel::source @ +0x08");
static_assert(offsetof(AudioChannel, userData)   == 0x10, "AudioChannel::userData @ +0x10");
static_assert(offsetof(AudioChannel, userValue)  == 0x18, "AudioChannel::userValue @ +0x18");
static_assert(offsetof(AudioChannel, onComplete) == 0x20, "AudioChannel::onComplete @ +0x20");

constexpr int kChannelCount        = 0x40;   // Update's `CMP EDI,0x40 ; JL`
constexpr int kChannelStride       = 0x28;   // cross-confirmed by Device::Stop
constexpr int kLockOffset          = 0xA50;  // CRITICAL_SECTION
constexpr int kPlatformReadyOffset = 0xA88;  // qword; 0 => the platform is down
constexpr int kVoiceBaseOffset     = 0xAA0;  // voice pointer array...
constexpr int kVoiceStride         = 0x20;   // ...stride 0x20, 64 entries
constexpr int kVoiceReleaseSlot    = 0x90;   // vtable byte offset called by
                                             // FreeChannelPlatform (StopPlatform
                                             // uses +0xA0 on the same object)

static_assert(kChannelCount * kChannelStride <= kLockOffset,
              "the 64 channels (ending at 0xA00) must not overlap the lock at 0xA50");

// The completion callback's signature, read off the three registers loaded at
// the call: RCX = the channel record itself, RDX = its +0x10, R8D = its +0x18.
using AudioCompletionFn = void (*)(void* channel, void* userData, uint32_t userValue);

inline AudioChannel* ChannelAt(void* self, int index) {
    // Retail sign-extends the int index (`MOVSXD RDX,EDX`) before scaling, so
    // the arithmetic is done in 64 bits on a SIGNED value. Reproduced rather
    // than done in `int`, which would differ for a negative index.
    return reinterpret_cast<AudioChannel*>(
        static_cast<uint8_t*>(self) + static_cast<int64_t>(index) * kChannelStride);
}

}  // namespace

// ---------------------------------------------------------------------------
// Audio::Device::FreeChannelPlatform(int) — retail 0x1800DE6F0, ord 2380.
// PRIVATE (`AEAA`), which is why nothing on our side calls it by name today
// except engine_funcs.cpp's own resolve (engine_funcs.cpp:1562) — and that
// resolve is precisely the cutover row this closes.
//
// Retail:
//   1800de6f6  CMP qword ptr [RCX+0xa88],0 ; JZ -> return
//   1800de70a  RSI = this + idx*0x28       ; the channel
//   1800de71f  RDI = idx*0x20              ; the voice-array scale
//   1800de723  RAX = [RSI+8]               ; channel->source
//   1800de727  if (RAX && RAX[0x21]) EnterCriticalSection(this+0xa50)
//   1800de73f  RCX = [this + idx*0x20 + 0xaa0]      ; the platform voice
//   1800de74a  if (RCX) { CALL [[RCX]+0x90](RCX); [slot] = 0; }
//   1800de761  RAX = [RSI+8]               ; RE-READ, not the cached value
//   1800de772  if (RAX && RAX[0x21]) tail LeaveCriticalSection(this+0xa50)
//
// The RE-READ at 0x1800DE761 is the one detail worth spelling out: retail does
// NOT reuse the `locked` verdict it computed on entry. It loads
// `channel->source` a second time and re-tests byte +0x21 of it, so a virtual
// call that changed either of those changes whether the lock is released.
// That is an unbalanced-lock hazard in the shipped DLL, not a transcription
// error here, and it is reproduced exactly. (Ghidra's decompiled C for this
// function folds the two reads into one — the standing warning in
// reference_ghidra_c_output_vs_disassembly.md; the disassembly above is what
// this body was written from.)
// ---------------------------------------------------------------------------
void RtAudio_Device_FreeChannelPlatform(void* selfPtr, int index) {
    RT_DIFF_BEGIN("Audio::Device::FreeChannelPlatform");

    auto* base = static_cast<uint8_t*>(selfPtr);
    const bool platformUp = *reinterpret_cast<void**>(base + kPlatformReadyOffset) != nullptr;

    RT_DIFF_WORK(platformUp);

    if (!platformUp) {
        RT_DIFF_SHADOW(true, "platform down, nothing to free",
                       "not diffable (releases a COM voice)");
        return;
    }

    AudioChannel* ch = ChannelAt(selfPtr, index);
    auto* lock = reinterpret_cast<CRITICAL_SECTION*>(base + kLockOffset);
    void** voiceSlot = reinterpret_cast<void**>(
        base + static_cast<int64_t>(index) * kVoiceStride + kVoiceBaseOffset);

    // Entry test, on the value read NOW.
    {
        auto* src = static_cast<uint8_t*>(ch->source);
        if (src && src[0x21] != 0) EnterCriticalSection(lock);
    }

    if (void* voice = *voiceSlot) {
        void** vtable = *reinterpret_cast<void***>(voice);
        reinterpret_cast<void (*)(void*)>(vtable[kVoiceReleaseSlot / 8])(voice);
        *voiceSlot = nullptr;
    }

    // Exit test, on a FRESH read — see the note above. Deliberately not the
    // cached verdict from the entry test.
    {
        auto* src = static_cast<uint8_t*>(ch->source);
        if (src && src[0x21] != 0) LeaveCriticalSection(lock);
    }

    RT_DIFF_SHADOW(true, "released the platform voice",
                   "not diffable (releases a COM voice)");
}

// ---------------------------------------------------------------------------
// Audio::Device::Update(Time const&) — retail 0x1800DE520, ord 4481.
//
// The `Time const&` argument is NEVER READ. RDX is loaded on entry by the
// calling convention and the body never touches it again — verified against
// the disassembly, not inferred from the decompiler. The parameter is kept in
// the signature because the mangled name has it and callers pass it.
//
// Retail:
//   for (i = 0; i < 0x40; ++i) {
//       ch = this + i*0x28;
//       if ((ch->flags & 2) == 0) continue;
//       FreeChannelPlatform(this, i);           ; 1800de55b
//       cb = ch->onComplete;                    ; 1800de560, BEFORE the clears
//       ch->flags &= ~3;                        ; 1800de563
//       ch->source = 0;                         ; 1800de567
//       if (cb) cb(ch, ch->userData, ch->userValue);   ; 1800de57b, CALL RAX
//   }
//
// Two orderings matter and are reproduced: `onComplete` is read BEFORE the
// flag/source clears, and the callback's 2nd and 3rd arguments are read AFTER
// them. The `&= ~3` clears bits 0 AND 1 even though only bit 1 was tested.
// The same five instructions appear verbatim in `Device::Stop`
// (0x1800DE23F..0x1800DE268), which is the cross-check for all of it.
// ---------------------------------------------------------------------------
void RtAudio_Device_Update(void* selfPtr, const void* /*time — never read*/) {
    RT_DIFF_BEGIN("Audio::Device::Update");

    int reaped = 0;
    for (int i = 0; i < kChannelCount; ++i) {
        AudioChannel* ch = ChannelAt(selfPtr, i);
        // Retail tests the LOW BYTE of the dword (`TEST byte ptr [..],0x2`)
        // and clears the whole dword; both are reproduced.
        if ((ch->flags & 2u) == 0) continue;
        ++reaped;

        RtAudio_Device_FreeChannelPlatform(selfPtr, i);

        void* cb = ch->onComplete;
        ch->flags &= 0xFFFFFFFCu;
        ch->source = nullptr;
        if (cb)
            reinterpret_cast<AudioCompletionFn>(cb)(ch, ch->userData, ch->userValue);
    }

    // The effectiveness control this site needs most: Update is called every
    // frame and reaps nothing in almost all of them, so `calls=` alone would
    // report a body that has never actually run its interesting path as
    // heavily exercised.
    RT_DIFF_WORK(reaped != 0);
    RT_DIFF_SHADOW(true, "reaped every finished channel",
                   "not diffable (fires stored completion callbacks)");
}

int RtAudioDeviceRegister() {
    int n = 0;
    // Name AND address for both, the pattern audio.cpp already uses: the name
    // is how engine_funcs.cpp reaches them (engine_funcs.cpp:1558 and :1562),
    // and the address keeps port_bypass_audit.py's ADDRESS door off the DLL.
    RegisterRuntimeExport("?Update@Device@Audio@@QEAAXAEBVTime@System@@@Z",
                          (void*)&RtAudio_Device_Update);
    RegisterRuntimeAddr(0x1800de520, (void*)&RtAudio_Device_Update); ++n;

    RegisterRuntimeExport("?FreeChannelPlatform@Device@Audio@@AEAAXH@Z",
                          (void*)&RtAudio_Device_FreeChannelPlatform);
    RegisterRuntimeAddr(0x1800de6f0, (void*)&RtAudio_Device_FreeChannelPlatform); ++n;
    return n;
}
