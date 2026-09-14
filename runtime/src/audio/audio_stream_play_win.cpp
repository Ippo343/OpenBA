// ============================================================================
// runtime/src/audio/audio_stream_play_win.cpp — batch 7 (Input/Audio leaves)
//   Audio::Stream::Play(int,int)                     retail 0x1800a0d10  ord 3417
// ============================================================================
//
// `?Play@Stream@Audio@@QEAAXHH@Z`. The last body `Audio::Device::PlayPlatform`
// (audio_play_platform.cpp, this same batch) reaches by ADDRESS, and it is
// the caller that names this file's two int parameters: `Stream::Play(slot,
// bytePosition)`.
//
// Confirmed EXPORTED by pefile against the retail export directory before
// writing this file (docs/runtime_port/closure/WORKLIST.md's Audio table
// already said so; re-checked rather than trusted):
//     0x1800a0d10  ord 3417  ?Play@Stream@Audio@@QEAAXHH@Z
//
// Its own `_win.cpp`, separate from `audio_stream_win.cpp` (the ctor/dtor),
// for the same reason that file gives: it needs SetEvent / ResetEvent /
// WaitForSingleObject, and PLAN.md §4.1 decision 1 keeps <windows.h> out of
// audio.cpp.
//
// ---------------------------------------------------------------------------
// THE BODY, FROM THE DISASSEMBLY (Ghidra's decompile folds two of these
// KERNEL32 imports into names that read fine but were re-derived here anyway
// -- reference_ghidra_c_output_vs_disassembly.md)
// ---------------------------------------------------------------------------
//   1800a0d1f  TEST byte [RCX+0x8],1           ; System::Thread::runFlags bit0
//   1800a0d2b  JZ -> 1800a0d4b
//   1800a0d2d  MOV byte [RCX+0x80],1           ; Stream's own "stopping" flag
//   1800a0d38  CALL [SetEvent]      (self->0x40)
//   1800a0d45  CALL [WaitForSingleObject] (self->0x28, -1)
//   1800a0d4b  LEA RCX,[RBX+0x30]
//   1800a0d4f  CALL 0x18000c300                 ; grow the 4-slot stream buffer
//   1800a0d58  MOV byte [RBX+0x80],0
//   1800a0d5f  MOV dword [RBX+0x7c],ESI          ; ESI = EDX = 1st explicit arg
//   1800a0d62  MOV dword [RBX+0x78],EDI          ; EDI = R8D = 2nd explicit arg
//   1800a0d65  CALL [ResetEvent]    (self->0x28)
//   1800a0d7e  JMP  [SetEvent]      (self->0x18)  ; TAIL CALL
//
// Every import resolved from the DLL's OWN import directory with pefile, not
// guessed: 0x18014b368 SetEvent, 0x18014b3c0 WaitForSingleObject, 0x18014b198
// ResetEvent (all KERNEL32.dll).
//
// FIELD IDENTITIES, cross-checked against `runtime/types/system/Thread.h`
// (Audio::Stream is a `System::Thread` subclass, and audio_stream_win.cpp's
// ctor/dtor already establish +0x40/+0x28/+0x18 as Stream/Thread handles):
//   +0x08 (bit0)  System::Thread::runFlags -- "the worker is mid-pass"
//   +0x18         System::Thread::hStart    -- woken at the tail to restart
//   +0x28         System::Thread::hFinished -- reset, and (if mid-pass) waited on
//   +0x40         Stream's OWN event, created SECOND by the ctor -- the "please
//                 stop your current pass and pick up the new track" wake
//   +0x78 / +0x7c Stream's own track-selection ints, set from this call's args
//   +0x30         the 3-word Array<T> header FUN_18000c300 grows to 4 slots
//
// SO: if the worker is currently mid-pass (runFlags bit0), this sets the
// "stop" flag, wakes it with its OWN event (NOT hStop -- hStop is the
// dtor's shutdown wake, a different signal), and BLOCKS until the current
// pass finishes (hFinished, manual-reset). Only then does it grow the buffer
// table, clear the stop flag, stamp the new (slot, bytePosition) pair, reset
// hFinished, and wake the worker again via hStart to begin the new track.
//
// ---------------------------------------------------------------------------
// FUN_18000c300 IS OUT OF THIS BATCH'S SCOPE, AND REACHED BY ADDRESS
// ---------------------------------------------------------------------------
// Decompiled to check what it does (not to port it): it is a
// `Collection::Array<T>::SetSize(4)`-shaped resize of the 3-word header at
// self+0x30 to hold exactly 4 elements of 0x8ba0 bytes each, allocated through
// `System::MemoryAllocator::MemAlloc`/`Free` under `MemoryAllocator::_singleton
// +0x640`'s critical section, with an SSE-unrolled element copy when both old
// and new buffers are 16-byte aligned. It is NOT one of this batch's five
// worklist rows (docs/runtime_port/closure/WORKLIST.md), so it stays
// unregistered here and this call reaches it through `ResolveAddr`, which
// consults the port registry first and falls back to the DLL -- exactly the
// door `audio_stream_win.cpp`'s `ReleaseField30Buffer` already opens onto
// 0x18000c4a0 for the same reason.
//
// ---------------------------------------------------------------------------
// ORACLE -- NONE, and the reason is the same class the ctor/dtor gave
// ---------------------------------------------------------------------------
// This drives a REAL streaming worker thread: it can block on a real
// synchronisation object (hFinished) and it wakes a real OS thread twice
// (hStart, and conditionally its own +0x40 event). A replay would wake the
// worker and grow/replace the buffer table a second time, which is a second
// side effect, not a comparison -- audio_stream_win.cpp's ctor/dtor face
// exactly this and settle for a self-check; this body cannot even offer that
// much, because unlike a ctor's field layout there is no static
// post-condition here that a wrong port would reliably break (the two int
// fields legitimately vary by caller, and the buffer-growth call is opaque
// from here). Attendance only.
// ============================================================================

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

constexpr int kThreadRunFlagsOffset = 0x08;  // System::Thread::runFlags, bit0
constexpr int kStreamStoppingOffset = 0x80;  // byte
constexpr int kStreamWakeEvent      = 0x40;  // HANDLE, Stream's own wake event
constexpr int kThreadFinishedEvent  = 0x28;  // System::Thread::hFinished
constexpr int kThreadStartEvent     = 0x18;  // System::Thread::hStart
constexpr int kStreamTrackB         = 0x7c;  // stores the 1st explicit arg
constexpr int kStreamTrackA         = 0x78;  // stores the 2nd explicit arg
constexpr int kStreamBufferHeader   = 0x30;  // &self[0x30], handed to FUN_18000c300

uint64_t g_calls = 0;
uint64_t g_drained = 0;  // "did work" = the mid-pass drain-and-wait branch ran

struct PlaySummary {
    ~PlaySummary() {
        if (!g_calls) return;
        printf("[RtPort] Audio::Stream::Play ATTENDANCE: calls=%llu drained=%llu "
                    "(NO ORACLE: wakes a real streaming thread; see the banner)\n",
                    (unsigned long long)g_calls, (unsigned long long)g_drained);
        fflush(stdout);
    }
};
PlaySummary g_summary;

template <typename T> inline T& Field(void* p, int off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

// 0x18000c300 -- Collection::Array<T>::SetSize(4) for the Stream's 4-slot
// streaming buffer table (element stride 0x8ba0). Out of THIS batch's scope;
// reached by ADDRESS so the port registry gets first refusal. See the banner.
// NOT CONVERTED (ENDGAME B.3.2 step 2): `kSpec_Blob8ba0` (array.cpp) itself
// says "the sole caller is Engine::Play and nothing there names [the element
// type]" — there is no `runtime/types` struct for Audio::Stream at all, so
// `headerPtr` (`&self[0x30]`) is a raw byte offset with no typed
// `Collection::Array<T>` field to call `.SetSize()` on. Converting this site
// would mean inventing a Stream type header, which is member/struct work, not
// a caller conversion.
void GrowStreamBuffers(void* headerPtr) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x18000c300));
    if (fn) fn(headerPtr);
}

}  // namespace

// ============================================================================
// Audio::Stream::Play(int,int) -- retail 0x1800a0d10, ord 3417
// ============================================================================
extern "C" void RtAudio_Stream_Play(void* selfPtr, int32_t a, int32_t b) {
    ++g_calls;
    auto* self = static_cast<uint8_t*>(selfPtr);

    const bool midPass = (Field<uint32_t>(self, kThreadRunFlagsOffset) & 1u) != 0;  // 1800a0d1f
    if (midPass) {
        ++g_drained;
        Field<uint8_t>(self, kStreamStoppingOffset) = 1;                     // 1800a0d2d
        SetEvent(Field<HANDLE>(self, kStreamWakeEvent));                     // 1800a0d38
        WaitForSingleObject(Field<HANDLE>(self, kThreadFinishedEvent),       // 1800a0d45
                           INFINITE);
    }

    GrowStreamBuffers(self + kStreamBufferHeader);                          // 1800a0d4f

    Field<uint8_t>(self, kStreamStoppingOffset) = 0;                        // 1800a0d58
    Field<int32_t>(self, kStreamTrackB) = a;                                 // 1800a0d5f
    Field<int32_t>(self, kStreamTrackA) = b;                                 // 1800a0d62
    ResetEvent(Field<HANDLE>(self, kThreadFinishedEvent));                  // 1800a0d65
    SetEvent(Field<HANDLE>(self, kThreadStartEvent));                       // tail, 1800a0d7e

}

// ============================================================================
// Registration -- unconditional (DIRECTION.md: a port registers unconditionally,
// no new kill switches).
// ============================================================================
int RtAudioStreamPlayRegister() {
    int n = 0;
    RegisterRuntimeExport("?Play@Stream@Audio@@QEAAXHH@Z", (void*)&RtAudio_Stream_Play);
    RegisterRuntimeAddr(0x1800a0d10, (void*)&RtAudio_Stream_Play); ++n;
    return n;
}
