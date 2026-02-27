#pragma once
// Engine::MainLoop — binary-compatible struct layout.
// Runtime-x64.dll, build 170910. Size = 0xA0, singleton at RVA 0x1fe750.
//
// Decompiled from: Engine_MainLoop_ctor (0x1800dd6b0)
// Verified against: all getter methods (GetDeltaTime, GetFrameTime, etc.)
//
// This is a plain struct with explicit padding — no C++ inheritance.
// All field offsets validated with static_assert.

#include <cstdint>
#include <cstddef>
#include "common.h"
#include "UpdateLoop.h"

// Ghidra addresses for MainLoop in Runtime-x64.dll
namespace MainLoopAddr {
    // Singleton pointer
    constexpr uintptr_t Singleton       = 0x1801fe750;

    // Methods (RVA in Runtime-x64.dll Ghidra space)
    constexpr uintptr_t Get             = 0x180031570;
    constexpr uintptr_t IsRunning       = 0x1800314b0;
    constexpr uintptr_t GetContext      = 0x1800314d0;
    constexpr uintptr_t GetCurrentLoop  = 0x1800314e0;
    constexpr uintptr_t GetDeltaTime    = 0x1800314f0;
    constexpr uintptr_t GetFrameTime    = 0x180031500;
    constexpr uintptr_t GetFrameStartTime = 0x180031510;
    constexpr uintptr_t GetFrameUpdateTime = 0x180031520;
    constexpr uintptr_t MarkFrameUpdateTime = 0x180031530;
    constexpr uintptr_t Run             = 0x1800dd3c0;
    constexpr uintptr_t Frame           = 0x1800dd100;
    constexpr uintptr_t Update          = 0x1800dd070;
    constexpr uintptr_t Draw            = 0x1800dcd40;
    constexpr uintptr_t SetNextLoop     = 0x1800dd7a0;
    constexpr uintptr_t Ctor            = 0x1800dd6b0;
    constexpr uintptr_t Dtor            = 0x1800dd640;
    constexpr uintptr_t CreateVideoCtx  = 0x1800dd500;
    constexpr uintptr_t ReleaseVideoCtx = 0x1800dd5e0;
    constexpr uintptr_t AddPostUpdate   = 0x1800dcf60;
    constexpr uintptr_t AddPostDraw     = 0x1800dd040;
    constexpr uintptr_t RemovePostUpdate = 0x1800dceb0;
    constexpr uintptr_t RemovePostDraw  = 0x1800dcf90;
    constexpr uintptr_t OnVideoError    = 0x1800dc5e0;
    constexpr uintptr_t OnVideoRestart  = 0x1800dc3b0;
}

struct MainLoop {
    // --- Resource tracking (used by OnVideoError/OnVideoRestart) ---
    void*          m_resources;           // +0x00: array of tracked external resources
    int32_t        m_resourceCount;       // +0x08: number of tracked resources
    uint8_t        _pad0C[4];            // +0x0C

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
    PostCallback*  m_postDrawCallbacks;   // +0x80
    int32_t        m_postDrawCount;       // +0x88
    uint8_t        _pad8C[4];            // +0x8C

    // --- Post-update callbacks: array of PostCallback ---
    PostCallback*  m_postUpdateCallbacks; // +0x90
    int32_t        m_postUpdateCount;     // +0x98
    uint8_t        _pad9C[4];            // +0x9C
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
