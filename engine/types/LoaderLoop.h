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

// ENDGAME B.3.7 (block B1): the REAL class is `Engine::LoaderLoop`
// (runtime/types/engine/UpdateLoop.h). This exe-side mirror STAYS a POD with its `m_`
// spellings until the caller round renames the users outside block B1's file
// set (code/Game/{Dialog,Game,Goal,Tutorial}, standalone/autostart.cpp,
// engine/funcs/engine_funcs.h); it then becomes `using LoaderLoop = RtEngine::LoaderLoop;` (B7-6). Every offset here is
// pinned against the class by tests/unit/engine_loop_abi_test.cpp.

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
    // +0x40: auto-advance gate. CORRECTED BY WAVE 8 LANE A: this was called
    // `m_finished` and described as "set to 1 by loading thread on completion".
    // It is not. The CONSTRUCTOR (0x1800dc1e0) sets it to 1 and nothing in the
    // family writes it again; ?Update@LoaderLoop@ (0x1800dc2a0) only READS it,
    // as the first term of the three-term test that hands m_nextLoop to
    // MainLoop. The completion signal Update actually tests is bit 0 of
    // Engine::Loader::_loader's System::Thread::runFlags (0x1801fe7c0), which
    // is also all that ?IsFinished@LoaderLoop@ (0x1800dc150) looks at — and
    // IsFinished does NOT read this byte at all. So zeroing it (which three
    // sites in code/Game/ do) suppresses the inner loop's own transition and
    // leaves the outer LoaderLoop to drive it; that is correct, and only the
    // comments at those sites were wrong.
    uint8_t m_autoAdvance;      // +0x40: ctor sets 1
    uint8_t _pad41[7];          // +0x41: NEVER written by the ctor
};
static_assert(sizeof(LoaderLoop) == 0x48, "LoaderLoop must be 0x48 bytes");
static_assert(offsetof(LoaderLoop, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(LoaderLoop, m_nextLoop) == 0x28, "m_nextLoop at +0x28");
static_assert(offsetof(LoaderLoop, m_elapsedTicks) == 0x30, "m_elapsedTicks at +0x30");
static_assert(offsetof(LoaderLoop, m_delayTicks) == 0x38, "m_delayTicks at +0x38");
static_assert(offsetof(LoaderLoop, m_autoAdvance) == 0x40, "m_autoAdvance at +0x40");
