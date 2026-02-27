#pragma once
// Engine::LoaderLoop — loading screen loop with background resource loading.
// Runtime-x64.dll, constructor at 0x1800DC1E0, vtable at 0x180157940.
// Inherits UpdateLoop (0x28 bytes base). Struct size: 0x48 bytes.
//
// LoaderLoop wraps another UpdateLoop (m_nextLoop). On Startup, it kicks off
// Loader::Load on a background thread. Update sleeps 14ms per tick (loading
// screen frame rate cap) and transitions to m_nextLoop when loading completes
// and the minimum display delay has elapsed.
//
// Boot chain: GameMain creates nested LoaderLoops wrapping splash screens,
// loading dialogs, and finally the main menu.

#include "UpdateLoop.h"

struct LoaderLoop {
    // --- UpdateLoop base (0x00 - 0x27) ---
    UpdateLoopVtable* vtable;   // +0x00: vptr (points to LoaderLoop vtable)
    void* m_resourceList;       // +0x08: ExternalList*
    void* m_internalListA;      // +0x10
    void* m_internalListB;      // +0x18
    int32_t m_loadBit;          // +0x20
    int32_t m_loadProgress;     // +0x24

    // --- LoaderLoop fields (0x28 - 0x47) ---
    UpdateLoop* m_nextLoop;     // +0x28: loop to transition to after loading completes
    int64_t m_elapsedTicks;     // +0x30: accumulated time since Startup
    int64_t m_delayTicks;       // +0x38: minimum display duration before transition
    uint8_t m_finished;         // +0x40: set to 1 by loading thread on completion
    uint8_t _pad41[7];          // +0x41: padding to 0x48
};
static_assert(sizeof(LoaderLoop) == 0x48, "LoaderLoop must be 0x48 bytes");
static_assert(offsetof(LoaderLoop, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(LoaderLoop, m_nextLoop) == 0x28, "m_nextLoop at +0x28");
static_assert(offsetof(LoaderLoop, m_elapsedTicks) == 0x30, "m_elapsedTicks at +0x30");
static_assert(offsetof(LoaderLoop, m_delayTicks) == 0x38, "m_delayTicks at +0x38");
static_assert(offsetof(LoaderLoop, m_finished) == 0x40, "m_finished at +0x40");
