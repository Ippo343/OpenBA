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

// ENDGAME B.3.7 block B4: `Audio::Stream::Play` is a member of the real class
// (runtime/types/audio/Stream.h); the `kStream*` / `kThread*` offsets that
// lived here are its fields and System::Thread's (B6-8). The block-table grow
// (0x18000c300) is called directly — the function-local first-call cache that
// resolved it by address is gone (B6-5): Play is a member now.

#include <windows.h>

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/audio/Stream.h"

// 0x18000c300 — `Collection::Array<Block>::SetSize(4)` (kSpec_Blob8ba0,
// runtime/src/collection/array.cpp), the body registered at that address.
namespace rtcoll { void RtColl_ArraySetSize_18000c300(void* a); }

namespace {

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

}  // namespace

// ============================================================================
// Audio::Stream::Play(int,int) -- retail 0x1800a0d10, ord 3417
// ============================================================================
namespace Audio {

void Stream::Play(int32_t channel, int32_t bytePosition) {
    ++g_calls;

    const bool midPass = (_runFlags & 1u) != 0;                            // 1800a0d1f
    if (midPass) {
        ++g_drained;
        _stopping = 1;                                                     // 1800a0d2d
        SetEvent(static_cast<HANDLE>(_wakeEvent));                         // 1800a0d38
        WaitForSingleObject(static_cast<HANDLE>(_hFinished), INFINITE);    // 1800a0d45
    }

    rtcoll::RtColl_ArraySetSize_18000c300(&_buffers);                     // 1800a0d4f

    _stopping = 0;                                                         // 1800a0d58
    _channel = channel;                                                    // 1800a0d5f
    _bytePosition = bytePosition;                                          // 1800a0d62
    ResetEvent(static_cast<HANDLE>(_hFinished));                           // 1800a0d65
    SetEvent(static_cast<HANDLE>(_hStart));                                // tail, 1800a0d7e
}

}  // namespace Audio

extern "C" void RtAudio_Stream_Play(void* selfPtr, int32_t a, int32_t b) {
    reinterpret_cast<Audio::Stream*>(selfPtr)->Audio::Stream::Play(a, b);   // B.3.7 bridge
}
