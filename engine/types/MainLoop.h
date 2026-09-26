#pragma once
// Engine::MainLoop — binary-compatible struct layout.
// Runtime-x64.dll, build 170910. Size = 0xA0, singleton at RVA 0x1fe750.
//
// Decompiled from: Engine_MainLoop_ctor (0x1800dd6b0)
// Verified against: all getter methods (GetDeltaTime, GetFrameTime, etc.)
//
// This is a plain struct with explicit padding — no C++ inheritance.
// All field offsets validated with static_assert.
//
// ---------------------------------------------------------------------------
// SUPERSEDED by runtime/types/engine/MainLoop.h (Wave 8 lane B).
// ---------------------------------------------------------------------------
// This header stays for the exe tree that already includes it, and every
// OFFSET below is confirmed correct against the disassembly. Two claims it
// made were WRONG and are corrected in place:
//
//   1. `_pad0C`, `_pad8C` and `_pad9C` were called padding. They are NOT
//      padding. +0x00, +0x80 and +0x90 are each a `Collection::Array<T>` —
//      `{T* _data; int32 _size; int32 _capacity}` — and those three dwords are
//      the CAPACITY. `~MainLoop` (0x1800dd640) calls a distinct Array
//      destructor on each, and 0x180033880 ends by zeroing `_data` and then a
//      single qword covering both `_size` and `_capacity`. Code that grows a
//      callback array through AddPostUpdate/AddPostDraw reads the capacity.
//      The fields are renamed below; the old names are noted so the previous
//      claim is recognisable if it is met elsewhere.
//   2. `MainLoopAddr::IsRunning` (constant since removed) was commented as a no-argument query. The
//      shipped export at 0x1800314b0 is
//      `?IsRunning@MainLoop@Engine@@QEBA_NPEBVUpdateLoop@2@@Z` (ordinal 3158)
//      and takes a `const UpdateLoop*`. The no-arg `IsRunning` belongs to
//      `UpdateLoop`, at a different address (0x1800dd7b0, ordinal 3160).
//
// There is also NO VTABLE: the ctor's first store into `this` is a zero (the
// Array data pointer), not a vtable stamp — so `MainLoop` is not polymorphic.

// ENDGAME B.3.7 (block B1): the REAL class is `Engine::MainLoop`
// (runtime/types/engine/MainLoop.h). This exe-side mirror STAYS a POD with its `m_`
// spellings until the caller round renames the users outside block B1's file
// set (code/Game/{Dialog,Game,Goal,Tutorial}, standalone/autostart.cpp,
// engine/funcs/engine_funcs.h); it then becomes `using MainLoop = Engine::MainLoop;` (B7-6: MainLoop is concrete and non-polymorphic). Every offset here is
// pinned against the class by tests/unit/engine_loop_abi_test.cpp.

#include <cstdint>
#include <cstddef>
#include "common.h"
#include "UpdateLoop.h"

// The `MainLoopAddr` address constants that stood here (27, zero readers) went at
// the B.3.7 close; runtime/types/engine/MainLoop.h keeps RtEngine::MainLoopAddr.

struct MainLoop {
    // --- Resource tracking (used by OnVideoError/OnVideoRestart) ---
    // Collection::Array<VideoResourceEntry>; entries are 0x18 bytes.
    void*          m_resources;           // +0x00: element buffer (_data)
    int32_t        m_resourceCount;       // +0x08: live count (_size)
    int32_t        m_resourceCapacity;    // +0x0C: allocated count (_capacity).
                                          //        Was `_pad0C` — NOT padding.

    // --- Video context ---
    VideoContext*  m_context;             // +0x10: Video::Context* (0x2168 bytes)
    uint8_t        m_videoContextDirty;   // +0x18: set on loop switch, triggers ClearResources
    uint8_t        m_quit;               // +0x19: exit flag, checked each frame
    uint8_t        _pad1A[6];            // +0x1A

    // --- Loop state ---
    UpdateLoop*    m_currentLoop;         // +0x20: currently active loop
    UpdateLoop*    m_nextLoop;            // +0x28: pending loop (SetNextLoop)

    // --- Timing (all in engine ticks, from QPC) ---
    int64_t        m_lastTimestamp;       // +0x30: last QPC tick
    int64_t        m_accumulator;         // +0x38: fixed-step time accumulator
    int64_t        m_totalTime;           // +0x40: total elapsed game time
    int64_t        m_deltaTime;           // +0x48: fixed timestep interval
    int64_t        m_maxAccumulator;      // +0x50: accumulator clamp (spiral-of-death)
    int64_t        m_frameTime;           // +0x58: wall-clock time for last full frame
    int64_t        m_frameStartTime;      // +0x60: QPC at frame start
    int64_t        m_frameUpdateTime;     // +0x68: time spent in update phase
    int64_t        m_frameEndTime;        // +0x70: QPC at frame end

    // --- Engine back-pointer ---
    EngineInstance* m_engine;             // +0x78

    // --- Post-draw callbacks: array of PostCallback ---
    // Collection::Array<{void* func; void* context}>; entries are 0x10 bytes.
    PostCallback*  m_postDrawCallbacks;   // +0x80: element buffer (_data)
    int32_t        m_postDrawCount;       // +0x88: live count (_size)
    int32_t        m_postDrawCapacity;    // +0x8C: allocated count (_capacity).
                                          //        Was `_pad8C` — NOT padding.

    // --- Post-update callbacks: array of PostCallback ---
    PostCallback*  m_postUpdateCallbacks; // +0x90: element buffer (_data)
    int32_t        m_postUpdateCount;     // +0x98: live count (_size)
    int32_t        m_postUpdateCapacity;  // +0x9C: allocated count (_capacity).
                                          //        Was `_pad9C` — NOT padding.
};

// ============================================================
// Layout verification — build fails if any offset is wrong.
// ============================================================
static_assert(sizeof(MainLoop) == 0xA0, "MainLoop must be 0xA0 bytes");

static_assert(offsetof(MainLoop, m_resources)          == 0x00, "");
static_assert(offsetof(MainLoop, m_resourceCount)      == 0x08, "");
static_assert(offsetof(MainLoop, m_context)            == 0x10, "");
static_assert(offsetof(MainLoop, m_videoContextDirty)  == 0x18, "");
static_assert(offsetof(MainLoop, m_quit)               == 0x19, "");
static_assert(offsetof(MainLoop, m_currentLoop)        == 0x20, "");
static_assert(offsetof(MainLoop, m_nextLoop)           == 0x28, "");
static_assert(offsetof(MainLoop, m_lastTimestamp)      == 0x30, "");
static_assert(offsetof(MainLoop, m_accumulator)        == 0x38, "");
static_assert(offsetof(MainLoop, m_totalTime)          == 0x40, "");
static_assert(offsetof(MainLoop, m_deltaTime)          == 0x48, "");
static_assert(offsetof(MainLoop, m_maxAccumulator)     == 0x50, "");
static_assert(offsetof(MainLoop, m_frameTime)          == 0x58, "");
static_assert(offsetof(MainLoop, m_frameStartTime)     == 0x60, "");
static_assert(offsetof(MainLoop, m_frameUpdateTime)    == 0x68, "");
static_assert(offsetof(MainLoop, m_frameEndTime)       == 0x70, "");
static_assert(offsetof(MainLoop, m_engine)             == 0x78, "");
static_assert(offsetof(MainLoop, m_postDrawCallbacks)  == 0x80, "");
static_assert(offsetof(MainLoop, m_postDrawCount)      == 0x88, "");
static_assert(offsetof(MainLoop, m_postUpdateCallbacks)== 0x90, "");
static_assert(offsetof(MainLoop, m_postUpdateCount)    == 0x98, "");
