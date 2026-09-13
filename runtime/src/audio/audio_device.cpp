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
// What it does instead is `the diff oracle` on both sites, because `calls=`
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

// ---- WAVE 23 LANE TAIL: the StopPlatform-only offsets ---------------------
// Read off 0x1800DEA20 (disassembly in the body's banner below). They are new
// to this file because `Stop`/`StopPlatform` are the first bodies here to take
// the event path; lane IOA's banner already cited this function for the voice
// array, and these are the rest of what it touches.
constexpr int kVoiceStopSlot          = 0xA0;  // vtable byte offset StopPlatform
                                               // calls - deliberately NOT the
                                               // +0x90 above, see the banner
constexpr int kVoiceChannelBackOffset = 0xAA8; // the voice record's back-pointer
                                               // to its AudioChannel; +0x08 past
                                               // the voice ptr in the same 0x20 cell
constexpr int kDeviceFlagsOffset      = 0xA08; // byte; bit 0 gates the wake
constexpr int kStopRequestedOffset    = 0xA80; // byte; set to 1 before the wake
constexpr int kStopEventOffset        = 0xA40; // HANDLE passed to SetEvent

static_assert(kVoiceChannelBackOffset == kVoiceBaseOffset + 8,
              "the voice ptr and its channel back-pointer share one 0x20 cell");

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

    auto* base = static_cast<uint8_t*>(selfPtr);
    const bool platformUp = *reinterpret_cast<void**>(base + kPlatformReadyOffset) != nullptr;


    if (!platformUp) {
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
}

// ---------------------------------------------------------------------------
// Audio::Device::StopPlatform(int) - retail 0x1800DEA20.
//
// WAVE 23 LANE TAIL. This body is NOT one of the lane's 18 rows. It is here
// because of the census trap named in
// reference_porting_can_grow_the_cutover_surface.md: `Device::Stop` (the row
// that IS on the board) calls this function, and reaching it with
// `RT_EXPORT_ONCE("?StopPlatform@Device@Audio@@AEAAXH@Z")` would have ADDED a
// new called-but-unported row in the same namespace as the one it closed -
// Audio's gap would have read the same before and after. Porting the callee
// too is what makes closing `Stop` an actual closure.
//
// It IS exported, under a PRIVATE (`AEAA`) mangling - this DLL exports its
// private members, so "private" is not "unexported"
// (reference_aeaa_is_private_not_unexported.md). Verified with pefile against
// the shipped export directory, not inferred: 0x1800DEA20 maps to
// `?StopPlatform@Device@Audio@@AEAAXH@Z`.
//
// Every layout constant it needs was ALREADY derived and cross-confirmed in
// this file's banner by Wave 9 lane IOA, which read this exact function as its
// third witness for the platform-side offsets.
//
// Retail, instruction for instruction:
//   1800dea24  CMP qword ptr [RCX+0xa88],0 ; JZ -> return   (platform down)
//   1800dea3d  RDX = [this + idx*0x28 + 8]        ; channel->source
//   1800dea46  RBX = this + idx*0x20              ; the voice-array cursor
//   1800dea4e  if (source && source[0x21] && (this[0xa08] & 1)) {
//   1800dea5d      this[0xa80] = 1;
//   1800dea6b      SetEvent([this + 0xa40]);
//              }
//   1800dea71  RCX = [this + idx*0x20 + 0xaa0]    ; the platform voice
//   1800dea85  if (RCX) CALL [[RCX]+0xa0](RCX, 0, 0)
//   1800dea8b  RAX = [this + idx*0x20 + 0xaa8]    ; the voice's channel back-ptr
//   1800dea97  *(dword*)RAX |= 2                  ; set the "reap me" bit
//
// THREE DETAILS THAT ARE FAITHFUL, NOT SLIPS:
//
//  * The slot called on the voice is +0xA0, NOT the +0x90 that
//    `FreeChannelPlatform` calls on the SAME object. That difference is the
//    whole reason lane IOA used this body as its cross-check, and it is
//    preserved. It is also why `Stop` calls BOTH: +0xA0 stops the voice,
//    +0x90 releases it.
//  * The final `OR dword ptr [RAX],0x2` is UNGUARDED - retail does not test
//    RAX for null. Left unguarded (CLAUDE.md "Don't 'fix' a faithful crash
//    with a null check"): retail relies on the back-pointer always being
//    there once the platform is up, and a guard would SKIP the reap rather
//    than restore it. Note it sits OUTSIDE the `if (voice)`, so it runs on
//    every call that gets past the platform-down gate.
//  * The lock here is NOT the critical section `FreeChannelPlatform` takes.
//    This body signals an EVENT (`SetEvent` on the handle at Device+0xA40)
//    and sets the byte at Device+0xA80 first. Both are reproduced in order.
// ---------------------------------------------------------------------------
void RtAudio_Device_StopPlatform(void* selfPtr, int index) {

    auto* base = static_cast<uint8_t*>(selfPtr);
    const bool platformUp = *reinterpret_cast<void**>(base + kPlatformReadyOffset) != nullptr;


    if (!platformUp) {
        return;
    }

    AudioChannel* ch = ChannelAt(selfPtr, index);
    uint8_t* voiceCursor = base + static_cast<int64_t>(index) * kVoiceStride;

    // The wake: only when the source wants the device lock AND the device's
    // own flag byte has bit 0 set.
    {
        auto* src = static_cast<uint8_t*>(ch->source);
        if (src && src[0x21] != 0 && (base[kDeviceFlagsOffset] & 1) != 0) {
            base[kStopRequestedOffset] = 1;
            SetEvent(*reinterpret_cast<HANDLE*>(base + kStopEventOffset));
        }
    }

    if (void* voice = *reinterpret_cast<void**>(voiceCursor + kVoiceBaseOffset)) {
        void** vtable = *reinterpret_cast<void***>(voice);
        reinterpret_cast<void (*)(void*, void*, uint32_t)>(
            vtable[kVoiceStopSlot / 8])(voice, nullptr, 0);
    }

    // Unguarded on purpose - see the banner above.
    auto* backPtr = *reinterpret_cast<uint32_t**>(voiceCursor + kVoiceChannelBackOffset);
    *backPtr |= 2u;

}

// ---------------------------------------------------------------------------
// Audio::Device::Stop(AudioChannel*) - retail 0x1800DE200, ord 4105.
// WAVE 23 LANE TAIL - one of the lane's rows.
//
// Already reached BY NAME from runtime/src/audio/audio.cpp:901
// (`ReleasePlatformChannels`), which is why it sat in the census's `called`
// column unported. Registering it flips that call site onto this body.
//
// Retail:
//   1800de20a  index = ((char*)channel - (char*)this) / 0x28   ; signed, via
//              the 0x6666666666666667 / SAR 4 magic-division idiom
//   1800de230  StopPlatform(this, index);
//   1800de23a  FreeChannelPlatform(this, index);
//   1800de23f  ---- the reap epilogue, five instructions ----
//   1800de246  cb = channel->onComplete;            ; read BEFORE the clears
//   1800de24b  channel->flags &= ~3;
//   1800de253  channel->source = 0;
//   1800de268  if (cb) cb(channel, channel->userData, channel->userValue);
//
// THE EPILOGUE IS BYTE-FOR-BYTE THE ONE `Device::Update` RUNS at
// 0x1800DE560..0x1800DE57B. Lane IOA wrote that sequence out in this file's
// banner as its cross-check for `Update`; this body is the other half of that
// pair, and the two are deliberately written to read identically below. The
// orderings that matter are the same two: `onComplete` is read BEFORE the
// clears, and the callback's 2nd and 3rd arguments AFTER them. `&= ~3` clears
// bits 0 and 1 although `Stop` - unlike `Update` - never tested either.
//
// THE DIVISION. Retail does the whole thing in 64-bit signed arithmetic and
// only then truncates to 32 bits for the two calls (`MOV EDX,EBX`), and
// re-sign-extends that truncated value for the epilogue (`MOVSXD RAX,EBX`).
// So a pointer difference that did not fit in an int would be truncated ONCE
// and the same truncated index used everywhere. Reproduced by doing the
// divide in int64_t and narrowing to `int` exactly once. The magic sequence
// (`SAR 4` then add the sign bit) truncates toward zero, which is also what
// C++ integer division does, so `/ kChannelStride` is the faithful spelling
// and not merely an approximation of it.
//
// NO ORACLE, for the reason this file's banner already gives for its two
// siblings and which applies here verbatim: the body ends in a STORED
// COMPLETION CALLBACK and a COM voice stop/release. Replaying the DLL's copy
// afterwards would fire the callback a second time and release a released
// object - a second side effect, not a comparison. `the diff oracle` carries the
// distinction `calls=` cannot: there is no early-out here, so the work verdict
// is `true` and the number to read is whether the callback fired.
// ---------------------------------------------------------------------------
void RtAudio_Device_Stop(void* selfPtr, void* channelPtr) {
          // no early-out; see the banner

    const int64_t byteDelta = static_cast<uint8_t*>(channelPtr) -
                              static_cast<uint8_t*>(selfPtr);
    const int index = static_cast<int>(byteDelta / kChannelStride);

    RtAudio_Device_StopPlatform(selfPtr, index);
    RtAudio_Device_FreeChannelPlatform(selfPtr, index);

    // ---- the reap epilogue, identical to Device::Update's ------------------
    AudioChannel* ch = ChannelAt(selfPtr, index);
    void* cb = ch->onComplete;
    ch->flags &= 0xFFFFFFFCu;
    ch->source = nullptr;
    if (cb)
        reinterpret_cast<AudioCompletionFn>(cb)(ch, ch->userData, ch->userValue);

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

    // ---- WAVE 23 LANE TAIL -------------------------------------------------
    // Both doors for both, the pattern above. `Stop`'s NAME door is the live
    // one (audio.cpp:901 resolves it by name); the ADDRESS door keeps
    // tools/port_bypass_audit.py's address route off the DLL. `StopPlatform`
    // has no caller in our tree other than `Stop` immediately above it, and is
    // registered anyway for the reason its banner gives - an unregistered body
    // is indistinguishable from an absent one to the census
    // (reference_half_registered_port_reads_as_absent.md).
    RegisterRuntimeExport("?Stop@Device@Audio@@QEAAXPEAUAudioChannel@12@@Z",
                          (void*)&RtAudio_Device_Stop);
    RegisterRuntimeAddr(0x1800de200, (void*)&RtAudio_Device_Stop); ++n;

    RegisterRuntimeExport("?StopPlatform@Device@Audio@@AEAAXH@Z",
                          (void*)&RtAudio_Device_StopPlatform);
    RegisterRuntimeAddr(0x1800dea20, (void*)&RtAudio_Device_StopPlatform); ++n;
    return n;
}
