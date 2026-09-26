#pragma once
// ============================================================================
// runtime/types/audio/Stream.h — Audio::Stream, 0x88 bytes, a System::Thread
//                                subclass with a 2-slot table
// ============================================================================
//
// ENDGAME B.3 step 7, block B4. Before this header the layout lived as
// file-local `constexpr int kStream*` offsets in three .cpp files
// (audio_stream_win.cpp, audio_stream_play_win.cpp,
// vtbatch7b_stream_run_win.cpp); they MOVE here under the retail name (B6-8).
//
//   sizeof 0x88   `??0Device@Audio@@QEAA@XZ` (0x1800de3a0) builds the Stream at
//                 Device+0xa00 (`LEA RCX,[RBX+0xa00]; CALL ??0Stream`) and the
//                 next Device member is at +0xa88; `??0Stream` (0x1800a0e30)
//                 stores up to the byte at +0x80 (B37_SLOTS.md section 1a).
//   parent        System::Thread: `??0Stream` CALLs `??0Thread@System@@QEAA@I@Z`
//                 with EDX = 0x10000 (a 64 KB stack) and then stamps
//                 `??_7Stream@Audio@@6B@`. No `_ParentTypeName` (not an
//                 Object::Base class).
//
// `??_7Stream@Audio@@6B@` @ 0x180154950, TWO slots (B37_SLOTS.md section 1):
//   [0] +0x00 0x1800a0f60  ?Run@Stream@Audio@@UEAAXXZ          — the worker body
//   [1] +0x08 0x18000c590  ?OnCompleted@Thread@System@@UEAAXXZ — inherited
// RULE B7-1.4: slot 0 is `Run`; there is NO deleting-dtor slot, and
// `??1Stream@Audio@@QEAA@XZ` (0x1800a0d90) is a NON-virtual `QEAA` export that
// re-stamps the Stream table and then runs the inlined `~Thread`.
//
// NO REAL CONSTRUCTOR (B4b-2, B7-12): the one live Stream is built inside
// `Audio::Device` by `Ctor_AudioDevice` (runtime/src/engine/singleton_ctors.cpp)
// through the registered `??0Stream@Audio@@QEAA@XZ` DOOR
// (runtime/src/audio/audio_stream_win.cpp), and torn down by
// `Engine::Shutdown` through the `??1` door. The Device is a POD in this step
// and holds the `RtAudio::Stream` POD below (B4b-3), cross-asserted field for
// field against the class. The same shape as `Engine::Loader`
// (runtime/types/engine/UpdateLoop.h).
//
// THREADS (RULE B7-7): `Run` executes on the AUDIO STREAM WORKER (a
// `_beginthreadex` thread); `Play` / `Stop` / `Lock` / `Unlock` run on the main
// thread. The only synchronisation is the raw CRITICAL_SECTION at +0x50 (the
// ctor's `InitializeCriticalSection` + spin count 5000) — B5-6: opaque storage,
// no `std::mutex`, no <windows.h> here — and the MemoryAllocator lock. No lock
// is added anywhere; the XAudio2 callback thread writes the Device's channel
// flags without one, by retail's design.
//
// PORTABILITY: no <windows.h> (PLAN.md section 4.1 decision 1). The two event
// HANDLEs are spelled `void*`.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <type_traits>

#include "../system/Thread.h"

namespace RtAudio {

// ----------------------------------------------------------------------------
// RtAudio::Stream — the POD (B4b-3): what `Audio::Device` embeds at +0xa00.
// ----------------------------------------------------------------------------
struct Stream {
    RtSystem::Thread _thread;           // +0x00 .. +0x2f (vtable, runFlags, 4 HANDLEs)
    void*    _buffers;                  // +0x30  Array<0x8ba0-byte block>::_data
    int32_t  _buffersSize;              // +0x38
    int32_t  _buffersCapacity;          // +0x3c
    void*    _wakeEvent;                // +0x40  HANDLE, created SECOND by the ctor
    void*    _bufferEvent;              // +0x48  HANDLE, created FIRST by the ctor
    alignas(8) uint8_t _lock[0x28];     // +0x50  raw Win32 CRITICAL_SECTION
    int32_t  _bytePosition;             // +0x78  Play's 2nd argument
    int32_t  _channel;                  // +0x7c  Play's 1st argument
    uint8_t  _stopping;                 // +0x80  Play's "drop the current pass" flag
    uint8_t  _pad81[7];                 // +0x81  never written
};
static_assert(sizeof(Stream) == 0x88, "Audio::Stream is 0x88 bytes (Device+0xa00 .. +0xa88)");
static_assert(offsetof(Stream, _buffers)      == 0x30, "0x1800a0e67 MOV [RDI+0x30],RSI");
static_assert(offsetof(Stream, _wakeEvent)    == 0x40, "0x1800a0eb8 MOV [RDI+0x40],RAX");
static_assert(offsetof(Stream, _bufferEvent)  == 0x48, "0x1800a0ea4 MOV [RDI+0x48],RAX");
static_assert(offsetof(Stream, _lock)         == 0x50, "0x1800a0e73 LEA RCX,[RDI+0x50]");
static_assert(offsetof(Stream, _bytePosition) == 0x78, "0x1800a0e89 MOV [RDI+0x78],RSI (qword)");
static_assert(offsetof(Stream, _channel)      == 0x7c, "Play 0x1800a0d5f MOV [RBX+0x7c],ESI");
static_assert(offsetof(Stream, _stopping)     == 0x80, "0x1800a0e8d MOV byte [RDI+0x80],SIL");
static_assert(std::is_trivially_destructible<Stream>::value,
              "B4b-1: the POD never acquires a destructor");

}  // namespace RtAudio

namespace Audio {

// ============================================================================
// Audio::Stream — 0x88, a System::Thread subclass, 2 slots, NO deleting dtor
// ============================================================================
class Stream : public System::Thread {
public:
    // [0] 0x1800a0f60 — the real-time xWMA streaming loop. AUDIO WORKER THREAD.
    void Run() override;
    // [1] 0x18000c590 `System::Thread::OnCompleted` — inherited, not redeclared.

    // ?Play@Stream@Audio@@QEAAXHH@Z  0x1800a0d10 — main thread. Drains the
    // current pass (if any), grows the block table, stamps (channel, byte
    // position) and wakes the worker.
    void Play(int32_t channel, int32_t bytePosition);

    // ---- exported, NOT PORTED, no caller: declared, deliberately undefined --
    // 0x18000b650 / 0x18000b660: `ADD RCX,0x50 ; JMP [Enter/LeaveCriticalSection]`.
    // Audio::Device's FreeChannelPlatform takes the same lock by hand
    // (Device+0xa50 == Stream+0x50), as retail's inlined copy does.
    void Lock();
    void Unlock();
    void Stop();                                   // 0x1800a0f40

private:
    void PlatformConstruct();                      // 0x1800a0ed0  (AEAA)
    void PlatformDestruct();                       // 0x1800a0f10  (AEAA)

protected:
    // `??0Stream@Audio@@QEAA@XZ` 0x1800a0e30 / `??1Stream@Audio@@QEAA@XZ`
    // 0x1800a0d90 are the DOORS (audio_stream_win.cpp); neither is a C++
    // constructor here (B4b-2). Non-virtual (ENDGAME B.2).
    Stream() = default;
    ~Stream() = default;

public:
    // One block of the streaming ring: `[0x40 bytes: 16 cumulative packet
    // offsets][0x8b60 bytes: 16 xWMA packets of 0x8b6]` (Run's arithmetic,
    // vtbatch7b_stream_run_win.cpp). Grown to FOUR by Play through 0x18000c300.
    struct Block {
        uint32_t cumulativeBytes[16];   // +0x0000  XAUDIO2_BUFFER_WMA table
        uint8_t  packets[0x8b60];       // +0x0040  the audio
    };

    // ---- the 0x58 bytes Stream adds, at retail's offsets (public: the doors
    // and Audio::Device reach them) ----
    Block*   _buffers;                  // +0x30  Array<Block>::_data (Run reads it)
    int32_t  _buffersSize;              // +0x38  Array<Block>::_size
    int32_t  _buffersCapacity;          // +0x3c  Array<Block>::_capacity
    void*    _wakeEvent;                // +0x40  HANDLE — Play's "track changed"
    void*    _bufferEvent;              // +0x48  HANDLE — PlatformChannel OnBufferEnd
    alignas(8) uint8_t _lock[0x28];     // +0x50  CRITICAL_SECTION (B5-6: opaque)
    int32_t  _bytePosition;             // +0x78
    int32_t  _channel;                  // +0x7c  the Device channel being streamed
    uint8_t  _stopping;                 // +0x80
    uint8_t  _pad81[7];                 // +0x81
};

static_assert(sizeof(Stream::Block) == 0x8ba0, "the block stride 0x18000c300 allocates");
static_assert(sizeof(Stream) == 0x88, "Audio::Stream is 0x88 bytes");
static_assert(alignof(Stream) == 8, "Audio::Stream aligns to 8 (the vptr)");
static_assert(std::is_polymorphic<Stream>::value && !std::is_abstract<Stream>::value,
              "Stream overrides Thread's pure slot 0 (Run)");
static_assert(sizeof(Stream) == sizeof(RtAudio::Stream), "Stream class == POD");
static_assert(offsetof(Stream, _runFlags)      == offsetof(RtSystem::Thread, runFlags), "alias runFlags");
static_assert(offsetof(Stream, _buffers)       == offsetof(RtAudio::Stream, _buffers),       "alias +0x30");
static_assert(offsetof(Stream, _buffersSize)   == offsetof(RtAudio::Stream, _buffersSize),   "alias +0x38");
static_assert(offsetof(Stream, _wakeEvent)     == offsetof(RtAudio::Stream, _wakeEvent),     "alias +0x40");
static_assert(offsetof(Stream, _bufferEvent)   == offsetof(RtAudio::Stream, _bufferEvent),   "alias +0x48");
static_assert(offsetof(Stream, _lock)          == offsetof(RtAudio::Stream, _lock),          "alias +0x50");
static_assert(offsetof(Stream, _bytePosition)  == offsetof(RtAudio::Stream, _bytePosition),  "alias +0x78");
static_assert(offsetof(Stream, _channel)       == offsetof(RtAudio::Stream, _channel),       "alias +0x7c");
static_assert(offsetof(Stream, _stopping)      == offsetof(RtAudio::Stream, _stopping),      "alias +0x80");

}  // namespace Audio
