// standalone/splash_loop_stubs.cpp
// Standalone-only stub vtable for the exe's splash DialogLoaderLoopT subclass
// (vtable @ 0x140158800, used for both Dialog\Loading.rsc and Dialog\ShiningRock.rsc
// in the original GameMain).
//
// Layout deduced from disassembly of g_GameLoadDialogLoopT_Vtable:
//   +0x00  Destroy(self, flags)            — exe 0x140001010 (DialogLoaderLoopT::~ + free)
//   +0x08  Startup(self)                   — exe 0x140098FA0 (chains base Startup + UI setup)
//   +0x10  Shutdown(self)                  — exe 0x140098F90 (literal RET 0)
//   +0x18  Update(self, time)              — exe 0x140098DA0 (random text + base Update)
//   +0x20  Draw(self, alpha)               — thunk 0x14003A8EE -> Runtime DialogLoaderLoopT::Draw
//   +0x28  DrawFailed(self)                — thunk 0x14003A90C -> Runtime DialogLoaderLoopT::DrawFailed
//   +0x30  Load(self)                      — thunk 0x14003A8E8 -> Runtime DialogLoaderLoopT::Load
//   +0x38  SaveRestore(self, save, state)  — thunk 0x14003A906 -> Runtime DialogLoaderLoopT::SaveRestore
//   +0x40  OnVideoError(self)              — thunk 0x14003A8C4 -> Runtime DialogLoaderLoopT::OnVideoError
//   +0x48  OnVideoRestart(self)            — thunk 0x14003A900 -> Runtime DialogLoaderLoopT::OnVideoRestart
//   +0x50  GetResourceFilename(self)       — thunk 0x14003A8FA -> Runtime DialogLoaderLoopT::GetResourceFilename
//   +0x58  AllowsModChanges(self)          — thunk 0x14003A8F4 -> Runtime DialogLoaderLoopT::AllowsModChanges
//   +0x60  CreateDialog(self, retptr)      — exe 0x140099070 (LoaderDialog ctor)
//
// First-pass: Startup/Shutdown/Update/Draw/CreateDialog are print-only stubs.
// Update returns false on the first tick so MainLoop::Run exits cleanly.
// As real reimpls land in code/Game/Game/, replace the matching stub entry.


#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "UpdateLoop.h"
#include "DialogLoaderLoopT.h"
#include "MainLoop.h"
#include "common.h"
#include "async_loader.h"
#include "chunked_scene_loader.h"
#include "stall_debug.h"
#include "boot_window.h"
#include "Game/dialogloaderloop.h"
#include "Game/uifade.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

// Defined in code/Game/Game/gameupdateloop_load.cpp. Returns true once the
// new-game world build for `loop` (a GameUpdateLoop) has fully completed.
extern "C" bool GameUpdateLoop_IsWorldBuilt(void* loop);

namespace {

// Fix 2: port of the "random loading tip" block in retail FUN_140098DA0
// (GameLoadDialogLoopT::Update). Finds the "descText" TextElement in the loading
// dialog and sets its text to a random entry from the element's string table
// (element+0xD8), using the loop's embedded System::Random at self+0x194.
//
// Retail cycles this every 2.0s (DAT_140172174) off the self+0xB58 timer; the
// caller drives the timing. This helper just sets one tip. Fully defensive: it
// silently no-ops if the dialog/element/table or any engine fn is missing (e.g.
// ShiningRock.rsc has no descText — FindElement then returns the missing-element
// sentinel, which we normalise to null and skip).
// Diagnostic: name the gate that stopped us. A silent no-op here is
// indistinguishable from "the tip rendered but is blank".
static void TipBail(const char* why, const void* p) {
    static int n = 0;
    if (n++ < 12) printf("[SplashDLT] loading tip SKIPPED: %s (%p)\n", why, p);
}

void SetRandomLoadingTip(DialogLoaderLoopT* self) {
    if (!self) { TipBail("self==null", nullptr); return; }
    if (!self->m_dialogInstance) { TipBail("m_dialogInstance==null", self); return; }
    if (!fn_String_Ctor_CStr)            { TipBail("fn_String_Ctor_CStr", nullptr); return; }
    if (!fn_String_Dtor)                 { TipBail("fn_String_Dtor", nullptr); return; }
    if (!fn_DialogInstance_FindElement)  { TipBail("fn_DialogInstance_FindElement", nullptr); return; }
    if (!fn_StringTable_GetCount)        { TipBail("fn_StringTable_GetCount", nullptr); return; }
    if (!fn_Random_Rand)                 { TipBail("fn_Random_Rand", nullptr); return; }
    if (!fn_UIString_Ctor_Index)         { TipBail("fn_UIString_Ctor_Index", nullptr); return; }
    if (!fn_TextElement_SetText)         { TipBail("fn_TextElement_SetText", nullptr); return; }

    // Find "descText" in the dialog instance (member value-return ABI:
    // RCX=this, RDX=retbuf, R8=name — the typedef carries all three).
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "descText");
    uint64_t elemBuf = 0;
    fn_DialogInstance_FindElement(self->m_dialogInstance, &elemBuf, nameStr);
    fn_String_Dtor(nameStr);

    // Normalise the missing-element sentinel (0xffffffff........) to null.
    uintptr_t raw = static_cast<uintptr_t>(elemBuf);
    if (raw < 0x10000 || raw > 0x00007FFFFFFFFFFFULL) {
        TipBail("descText not found (FindElement sentinel)", (void*)raw);
        return;
    }
    void* elem = reinterpret_cast<void*>(raw);

    // Retail gates on the TEXTELEMENT type id, read from exref 0x140151508
    // (Ghidra labels that global `fn_LabelElement_SetSprite` -- a misnamed
    // dispatch slot, which is what sent the first port to LabelElement).
    // descText is a TextDescription, so IsDerivedFrom(elem, LabelElement)
    // was FALSE and the tip was skipped on every game-load splash.
    if (fn_IsDerivedFrom && g_TypeId_TextElement &&
        !fn_IsDerivedFrom(elem, *g_TypeId_TextElement)) {
        TipBail("descText is not a TextElement", elem);
        return;
    }

    // StringTable at element+0xD8; pick a random 1-based index in [1, count].
    void* table = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(elem) + 0xD8);
    if (!table) { TipBail("elem+0xD8 StringTable==null", elem); return; }
    int count = fn_StringTable_GetCount(table);
    if (count <= 0) { TipBail("StringTable count<=0", table); return; }
    void* rng = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x194);
    uint32_t idx = fn_Random_Rand(rng, static_cast<uint32_t>(count - 1));

    uint8_t uiStr[16];
    fn_UIString_Ctor_Index(uiStr, reinterpret_cast<uint64_t>(table),
                           static_cast<int>(idx + 1));
    uint8_t textVal[16];
    *reinterpret_cast<uint64_t*>(textVal)     = *reinterpret_cast<uint64_t*>(uiStr);
    *reinterpret_cast<uint64_t*>(textVal + 8) = *reinterpret_cast<uint64_t*>(uiStr + 8);
    fn_TextElement_SetText(elem, textVal);

    static int s_tipLog = 0;
    if (s_tipLog++ < 8) {
        printf("[SplashDLT] loading tip set: descText=%p table=%p count=%d idx=%u\n",
               elem, table, count, idx + 1);
    }
}

void Stub_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[SplashDLT::Destroy] self=%p flags=%u\n", (void*)self, flags);
    // Skip the real cleanup chain (DialogLoaderLoopT::~DialogLoaderLoopT unwinds
    // UI::UserInterface, String, Audio::Scene, Random — all leak on this run but
    // the process exits immediately after MainLoop::Run returns).
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

// Per-loop latches for the deferred load kick and the fade-out kick. They used to
// be function-local statics compared as `latch != self`, on the stated assumption
// that "splash loops run one at a time, each with a unique self". Quit-to-main-menu
// broke that: a SECOND new game builds a second splash loop, the allocator can hand
// back the freed first one's address, and `latch == self` is then true on frame 0 --
// so the load kick never fires and the second New Game hangs on the loading screen
// with an unbuilt world. Clearing them in Stub_Startup (which runs exactly once per
// loop) makes the latch depend on the loop's LIFETIME rather than its address.
UpdateLoop* g_loadKickedFor       = nullptr;
UpdateLoop* g_fadeOutInitiatedFor = nullptr;

void Stub_Startup(UpdateLoop* self) {
    StallDebug::Phase startupPhase("SplashDLT::Stub_Startup");
    g_loadKickedFor       = nullptr;   // fresh loop -- see the note above
    g_fadeOutInitiatedFor = nullptr;
    DialogLoaderLoopT* ll = reinterpret_cast<DialogLoaderLoopT*>(self);
    UpdateLoop* next = ll->m_nextLoop;
    printf("[SplashDLT::Startup] self=%p next=%p m_dialogExternal(before)=%p\n",
           (void*)self, (void*)next, ll->m_dialogExternal);

    // Step 1: load self's own .rsc if not yet loaded. The original game's
    // Loader thread does this asynchronously before each loop becomes active,
    // but standalone has no thread — so we call DialogLoaderLoopT::Load on
    // ourselves synchronously here. Reads m_dialogFilename @ +0x48 and writes
    // the loaded External to m_dialogExternal @ +0x50.
    if (!ll->m_dialogExternal && fn_DialogLoaderLoopT_Load) {
        StallDebug::Phase ownLoadPhase("SplashDLT own-rsc DialogLoaderLoopT::Load");
        // Bracket the load so resolve_standalone logs every exe-side resolution
        // and the bulk NullFactory marks any hits with [TRACE!]. m_dialogFilename
        // at self+0x48 is a wchar_t COW wrapper — too risky to deref blind, so
        // we just tag with self pointer (each splash loop has a unique address).
        char label[96];
        std::snprintf(label, sizeof(label),
                      "fn_DialogLoaderLoopT_Load (Stub_Startup) self=%p",
                      (void*)self);
        TraceExeBegin(label);
        fn_DialogLoaderLoopT_Load(self);
        TraceExeEnd(label);
        printf("[SplashDLT::Startup] DialogLoaderLoopT::Load returned, "
               "m_dialogExternal=%p\n", ll->m_dialogExternal);
    }

    // Step 2: chain DialogLoaderLoopT::Startup. Mirrors original exe
    // 0x140098FA0 which tail-calls Runtime DialogLoaderLoopT::Startup
    // (0x1800db000). That function calls our vtable[+0x60] CreateDialog to
    // produce a LoaderDialog wrapping m_dialogExternal, stores it at
    // self+0x58 (m_dialogInstance), invokes the dialog's Slot4 (Setup),
    // pushes the IBP into the UI's pending-open list at self+0x88, then calls
    // Loader::Load(&_loader, m_nextLoop). The Runtime Loader thread hangs in
    // standalone (Engine+0x6c0 audio sub-object deadlock), so we ignore its
    // completion event and run our own async loader for next->vtable->Load
    // below — see Step 3.
    if (ll->m_dialogExternal && fn_DialogLoaderLoopT_Startup) {
        StallDebug::Phase chainPhase("SplashDLT fn_DialogLoaderLoopT_Startup");
        printf("[SplashDLT::Startup] invoking DialogLoaderLoopT::Startup "
               "(creates dialog + pushes to UI; Runtime Loader::Load is "
               "kicked but ignored)\n");
        fn_DialogLoaderLoopT_Startup(self);
        printf("[SplashDLT::Startup] DialogLoaderLoopT::Startup returned, "
               "m_dialogInstance=%p\n", ll->m_dialogInstance);
    } else {
        printf("[SplashDLT::Startup] WARN: skipping DialogLoaderLoopT::Startup "
               "(rsc=%p fn=%p)\n", ll->m_dialogExternal,
               (void*)fn_DialogLoaderLoopT_Startup);
    }

    // Step 3: the next loop's load (next->vtable->Load) used to be kicked HERE,
    // synchronously, before the reveal was ever triggered. For the boot splashes
    // that was a ~60-125ms hitch, but for the new-game path next->Load is
    // GameUpdateLoop::Load — the entire world build (worldgen + start-town +
    // resources), ~5.8s — so the screen sat BLACK for ~5.8s and only faded the
    // loading dialog in AFTER the load had finished. The loading screen provided
    // no feedback during the actual work (see notes: fade->load->fade stall).
    //
    // Fix 1: DEFER the load kick into Stub_Update. Startup now only builds the
    // dialog + triggers the reveal (below); Stub_Update lets the reveal animate
    // for a few frames so the loading dialog is actually on-screen, then kicks
    // the (still-synchronous, main-thread) load. The load is not moved to a
    // worker thread because the engine allocator + Graphics::Scene::AddGlobalObject
    // are not thread-safe (see async_loader.cpp) — but the one frozen frame now
    // shows a proper loading screen + tip instead of a black hang.
    //
    // Note: ChunkedSceneLoader is only ARMED at boot (RunGameMain in
    // code/Game/Game/game_main.cpp). Stub_Update runs its Init on the first
    // revealed frame and then Tick()s PhaseB, so MenuDLT's struct fields
    // (+0xb90 menuScene, +0xb98 renderer, etc.) are populated behind a drawn
    // splash rather than during the un-presented boot window.

    // Trigger reveal. With async loading, only Step 1's own-rsc load is
    // synchronous (~10ms for ShiningRock/Loading splashes; usually skipped
    // because m_dialogExternal was populated by an upstream load). The reveal
    // gets a normal first-tick dt and animates frame-by-frame.
    void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;
    UIFade::EnsureFadeSprite(ui);
    UIFade::TriggerReveal(ui);
}

void Stub_Shutdown(UpdateLoop* self) {
    printf("[SplashDLT::Shutdown] self=%p\n", (void*)self);
    // Original is RET 0 (literal no-op).
}

bool Stub_Update(UpdateLoop* self, void* time) {
    StallDebug::Tick("SplashDLT::Update");
    DialogLoaderLoopT* ll = reinterpret_cast<DialogLoaderLoopT*>(self);
    Time* dt = reinterpret_cast<Time*>(time);

    // Pump the embedded UI — flushes the pending-open list (where Startup
    // pushed our LoaderDialog), runs each open dialog's OnUpdate, processes
    // pending-close, and advances animation. Without this the splash dialog
    // is created but never moved into the active list, so Draw walks an
    // empty list and the screen stays blank. Mirrors Runtime
    // DialogLoaderLoopT::Update @ 0x1800daf20.
    //
    // Clamp first-tick dt: Stub_Startup ran synchronously and may have spent
    // 60-100ms+ doing the own-rsc load (first splash) or ~3ms (subsequent
    // splashes). Either way, MainLoop accumulates that time and feeds it back
    // as the first Update's dt. UI::Update advances the fade alpha by
    // dt * rate * 4, so a 1029ms boot-to-first-frame stall (common on the
    // very first splash because Engine::Startup + MainLoop init dominates)
    // would clamp the reveal alpha to 0 in one tick and skip the entire
    // fade-in animation. Limit dt to ~16ms (one 60Hz frame) on the first tick
    // so the fade animates frame-by-frame.
    int64_t origTicks = 0;
    bool clamped = false;
    if (ll->m_elapsedTicks == 0 && dt && g_Timer_ticksPerSecond &&
        *g_Timer_ticksPerSecond > 0) {
        origTicks = dt->ticks;
        double dtMs = (double)origTicks * 1000.0 / (double)*g_Timer_ticksPerSecond;
        printf("[SplashDLT::Update] self=%p first-tick dt=%lld (%.2fms)\n",
               (void*)self, (long long)origTicks, dtMs);
        int64_t maxTicks = *g_Timer_ticksPerSecond / 60;
        if (origTicks > maxTicks) {
            dt->ticks = maxTicks;
            clamped = true;
            printf("[SplashDLT::Update] self=%p first-tick dt clamped from %lld "
                   "to %lld (~16ms) — Startup stall accumulator\n",
                   (void*)self, (long long)origTicks, (long long)maxTicks);
        }
    }

    if (fn_UI_Update) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;
        fn_UI_Update(ui, time);
    }

    // Safety net for the deferred window reveal — if the Draw path never runs,
    // this reveals the window anyway rather than leaving the process invisible.
    BootWindow::PumpWatchdog();

    if (clamped) {
        dt->ticks = origTicks;
    }

    // Run the deferred MenuScene load, once, on the first frame where this
    // splash has actually revealed. RunGameMain only Arm()s it — running
    // LoadSceneA1 (~590ms) during boot meant paying it while the engine window
    // was already on screen and had never been presented to, which is the
    // boot-time blank-window flash. Here it lands behind a drawn splash.
    //
    // Same reveal test the deferred load-kick below uses: alpha < 0.5 (1=black,
    // 0=fully revealed) or a 0.25s safety cap so a stuck fade can't strand it.
    if (ChunkedSceneLoader::IsArmed()) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;
        int64_t revealCap = (g_Timer_ticksPerSecond && *g_Timer_ticksPerSecond > 0)
            ? *g_Timer_ticksPerSecond / 4 : 0;
        float revealAlpha = UIFade::GetCurrentAlpha(ui);
        if (revealAlpha < 0.5f || (revealCap && ll->m_elapsedTicks >= revealCap)) {
            printf("[SplashDLT::Update] self=%p reveal-alpha=%.2f -> running "
                   "deferred ChunkedSceneLoader init\n", (void*)self, revealAlpha);
            ChunkedSceneLoader::RunArmedInit();
        }
    }

    // Drive the chunked scene loader. Each tick processes ~500 PhaseB
    // elements (~17ms of work, well within a 60Hz frame budget) so the
    // splash's loading animation keeps moving while the heavy 12K-15K
    // static-mesh iteration runs in the background. When the chunk
    // function reports done, ChunkedSceneLoader::Tick auto-runs LoadSceneA3
    // (particles) and clears IsBusy() — the transition gate below waits
    // on this in addition to AsyncLoader::IsAsyncLoadBusy().
    if (ChunkedSceneLoader::IsBusy()) {
        StallDebug::Phase phase("SplashDLT ChunkedSceneLoader::Tick");
        int processed = ChunkedSceneLoader::Tick(500);
        (void)processed;
    }

    // Loading-screen frame cap (matches real LoaderLoop ~14ms sleep).
    Sleep(14);

    ll->m_elapsedTicks += dt->ticks;

    // ---- Fix 1: deferred load kick ----
    // The next loop's Load (for the new-game path, GameUpdateLoop::Load — the
    // ~5.8s world build) used to run synchronously inside Stub_Startup BEFORE
    // the reveal, so the screen sat black for the whole build. Now Startup only
    // triggers the reveal; here we let it animate for a few frames so the
    // loading dialog is actually on-screen, then kick the (still main-thread,
    // still blocking) load. The one frozen frame now shows a real loading screen
    // + tip instead of a black hang. Single-slot static like fadeOutInitiatedFor
    // below — splash loops run one at a time, each with a unique `self`.
    if (g_loadKickedFor != self && ll->m_nextLoop) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;
        float revealAlpha = UIFade::GetCurrentAlpha(ui);   // 1=black -> 0=revealed
        int64_t revealCap = (g_Timer_ticksPerSecond && *g_Timer_ticksPerSecond > 0)
            ? *g_Timer_ticksPerSecond / 4 : 0;             // 0.25s safety cap
        bool revealed = (revealAlpha < 0.5f);
        bool timedOut = (revealCap && ll->m_elapsedTicks >= revealCap);
        if (revealed || timedOut) {
            g_loadKickedFor = self;
            // The new-game world build (GameUpdateLoop::Load) is multi-second.
            // Run it on a WORKER thread so the main thread keeps animating the
            // loading screen + cycling tips (the worldReady gate below holds the
            // transition until it finishes). Without forceThreaded the default is
            // a synchronous main-thread load — fine for the small boot splashes,
            // but with BANISHED_GAME_PRELOAD on the main thread wins the build
            // race and freezes for ~6s. For the boot splashes (not the game loop)
            // stay synchronous.
            bool isGameLoop =
                (g_GameUpdateLoop_Vtable &&
                 *reinterpret_cast<void**>(ll->m_nextLoop) ==
                     reinterpret_cast<void*>(g_GameUpdateLoop_Vtable));
            // Retail does NOT seed a tip here: FUN_140098DA0 only ever sets
            // descText from inside the 2.0s timer branch, so the loading screen
            // opens showing the .rsc default and the first tip lands 2.0s later.
            // Seeding one here was a deviation -- it made the text appear at once
            // and added an extra change, which reads as "cycling faster".
            // Only the timer is reset, which retail does implicitly (ctor zeroes it).
            *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(self) + 0xB58) = 0;
            printf("[SplashDLT::Update] self=%p reveal-alpha=%.2f -> kicking "
                   "deferred load of next=%p (gameLoop=%d, threaded=%d)\n",
                   (void*)self, revealAlpha, (void*)ll->m_nextLoop,
                   (int)isGameLoop, (int)isGameLoop);
            AsyncLoader::StartAsyncLoad(ll->m_nextLoop, /*forceThreaded=*/isGameLoop);
        } else {
            // Still revealing the loading screen — keep animating, do not advance
            // to the load or the fade-out yet.
            return true;
        }
    }

    // ---- Fix 2: cycle the loading tip every 2.0s, as retail does ----
    // Retail FUN_140098DA0 drives this off the self+0xB58 timer against
    // DAT_140172174, which is exactly 2.0f. Measured here at 2015ms -- match.
    //
    // The block is gated on nextLoop->m_loadProgress != 0 (UpdateLoop+0x24),
    // exactly as retail. An earlier note here claimed the gate could not be
    // honoured because "the Runtime Loader::Load path calls SetLoadProgress and
    // our AsyncLoader replaced it" -- that was wrong on both counts. The
    // exported SetLoadProgress has ZERO callers in the whole 2017 build (no
    // '?SetLoadProgress' resolve string exists in the exe); the real publisher
    // is the exe worldgen pipeline (retail 0x1400cf390), which writes stages
    // 1..5 straight into g_pGameWorldSingleton+0x24. Our port of that pipeline
    // omitted the writes as "progress hints with no consumer" -- THIS is the
    // consumer. Restored in code/Game/Game/worldgen.cpp (wg_PublishLoadProgress).
    // Net faithful behaviour: tips cycle only while a NEW-GAME worldgen is
    // running; the boot splash chain (nextLoop=MenuDLT) and save-loads never
    // write +0x24, so those screens keep the .rsc default (a single space).
    if (g_loadKickedFor == self && g_Timer_ticksPerSecond && *g_Timer_ticksPerSecond > 0 &&
        ll->m_nextLoop && *(int*)((uintptr_t)ll->m_nextLoop + 0x24) != 0) {
        int64_t* tipTimer =
            reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(self) + 0xB58);
        *tipTimer += dt->ticks;
        if (*tipTimer >= *g_Timer_ticksPerSecond * 2) {   // 2.0s
            *tipTimer = 0;
            // [diag] measure the ACTUAL interval and the retail gate value.
            // Retail cycles only while nextLoop->m_loadProgress (UpdateLoop+0x24)
            // is non-zero, and its period is DAT_140172174 = 2.0s exactly.
            static long long s_prevTip = 0;
            LARGE_INTEGER qf, qn; QueryPerformanceFrequency(&qf); QueryPerformanceCounter(&qn);
            double sinceMs = s_prevTip ? 1000.0 * (double)(qn.QuadPart - s_prevTip) / (double)qf.QuadPart : -1.0;
            s_prevTip = qn.QuadPart;
            printf("[SplashDLT] tip cycle: wall=%.0fms tps=%lld loadProgress=%d\n",
                   sinceMs, (long long)*g_Timer_ticksPerSecond,
                   ll->m_nextLoop ? *(int*)((uintptr_t)ll->m_nextLoop + 0x24) : -1);
            SetRandomLoadingTip(ll);
        }
    }

    // Hold the loading screen until (a) the minimum display time elapsed AND
    // (b) for the new-game path, the world build has finished. Retail keeps the
    // loading screen up for the ENTIRE GameUpdateLoop::Load (the build runs on a
    // background thread) and only fades the game in — with citizens already
    // mid-path — once it's done. Without (b) we'd fade to black and transition
    // into GameUpdateLoop while it is still building, drawing a half-built world.
    // For every other loop (boot splashes -> MenuDLT) worldReady stays true and
    // the transition is purely time-based, exactly as before.
    bool worldReady = true;
    if (ll->m_nextLoop && g_GameUpdateLoop_Vtable &&
        *reinterpret_cast<void**>(ll->m_nextLoop) ==
            reinterpret_cast<void*>(g_GameUpdateLoop_Vtable)) {
        worldReady = GameUpdateLoop_IsWorldBuilt(ll->m_nextLoop);
    }

    // Once the splash hold elapses, kick off the fade-OUT (to opaque black)
    // and wait one extra tick before transitioning. Mirrors the original
    // FUN_140098DA0 (LoaderLoop::Update) sequence.
    if (ll->m_elapsedTicks >= ll->m_delayTicks && worldReady) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;

        if (g_fadeOutInitiatedFor != self) {
            g_fadeOutInitiatedFor = self;
            UIFade::EnsureFadeSprite(ui);
            UIFade::TriggerFadeIn(ui);
            printf("[SplashDLT::Update] self=%p kicked fade-out-to-black, "
                   "waiting for alpha=1\n", (void*)self);
            return true;
        }

        float alpha = UIFade::GetCurrentAlpha(ui);
        if (alpha < 0.99f) {
            return true;
        }

        // Wait for our async loader worker to finish loading m_nextLoop's
        // resources before transitioning. AsyncLoader::StartAsyncLoad was
        // called from Stub_Startup with `next` as its argument; the worker
        // thread runs `next->vtable->Load(next)` and flips a done flag.
        // (We bypass the Runtime g_pLoaderFlags because the Runtime Loader
        // thread hangs internally on Engine+0x6c0 audio access in standalone.)
        if (AsyncLoader::IsAsyncLoadBusy()) {
            static int waitCount = 0;
            if ((waitCount++ % 30) == 0) {
                printf("[SplashDLT::Update] self=%p fade=%.3f waiting for "
                       "AsyncLoader\n", (void*)self, alpha);
            }
            return true;
        }

        // Also wait for the chunked scene loader to finish PhaseB+A3. Without
        // this gate, the splash transitions before chunking completes and
        // MenuDLT would have to finish synchronously — re-introducing the
        // freeze.
        //
        // IsArmed() matters as much as IsBusy() here: between Arm() at boot and
        // RunArmedInit() above, IsBusy() is still false even though the entire
        // scene load is outstanding. Gating on IsBusy() alone would let a short
        // splash transition straight past it.
        if (ChunkedSceneLoader::IsArmed() || ChunkedSceneLoader::IsBusy()) {
            static int waitCount = 0;
            if ((waitCount++ % 30) == 0) {
                printf("[SplashDLT::Update] self=%p fade=%.3f waiting for "
                       "ChunkedSceneLoader\n", (void*)self, alpha);
            }
            return true;
        }

        UpdateLoop* next = ll->m_nextLoop;
        void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
        printf("[SplashDLT::Update] self=%p elapsed=%lld delay=%lld "
               "fade-alpha=%.3f -> transition next=%p\n",
               (void*)self, (long long)ll->m_elapsedTicks,
               (long long)ll->m_delayTicks, alpha, (void*)next);
        if (next && fn_MainLoop_SetNextLoop && mainLoop) {
            fn_MainLoop_SetNextLoop(mainLoop, next);
        }
        return false;
    }

    // Throttled trace so 2.0s @ 14ms (~143 ticks) doesn't spam.
    static int updateCount = 0;
    ++updateCount;
    if (updateCount <= 3 || updateCount % 50 == 0) {
        printf("[SplashDLT::Update] self=%p tick=%d elapsed=%lld delay=%lld\n",
               (void*)self, updateCount,
               (long long)ll->m_elapsedTicks, (long long)ll->m_delayTicks);
    }
    return true;
}

void Stub_Draw(UpdateLoop* self, float alpha) {
    StallDebug::Tick("SplashDLT::Draw");
    (void)alpha;  // frame-blend interpolation, not the fade alpha
    // Mirror Runtime DialogLoaderLoopT::Draw (0x1800DAE20). Shared body lives in
    // code/Game/Game/dialogloaderloop.cpp; DLL-mode reaches it via Reimpl_Draw
    // in engine/reimpl/dialogloaderloop.cpp.
    static int draws = 0;
    ++draws;
    bool trace = (draws <= 3);

    if (trace) {
        printf("[SplashDLT::Draw] self=%p frameAlpha=%f -> DialogLoaderLoopT_Draw_Impl\n",
               (void*)self, alpha);
    }

    // Per-frame fade alpha trace — log first 5 frames + every 10th so the curve
    // is visible in standalone_debug.log without spamming. UI is at self+0x60
    // for DialogLoaderLoopT.
    if (draws <= 5 || (draws % 10) == 0) {
        UIFade::LogState("SplashDLT::Draw", reinterpret_cast<uint8_t*>(self) + 0x60);
    }

    DialogLoaderLoopT_Draw_Impl(reinterpret_cast<DialogLoaderLoopT*>(self));

    // Reveal the engine window once a real frame is actually in the swap chain.
    // MainLoop presents AFTER Draw returns, so draw #1 has not been presented
    // yet when we get here — waiting for draw #2 guarantees the window's first
    // visible content is the splash rather than an unpresented backbuffer.
    // No-op unless boot_window armed the deferral. See boot_window.h.
    if (draws >= 2 && BootWindow::IsPending()) {
        BootWindow::ShowNow("first splash frame presented");
    }
}

void Stub_DrawFailed(UpdateLoop* self) {
    printf("[SplashDLT::DrawFailed] self=%p\n", (void*)self);
}

void Stub_Load(UpdateLoop* self) {
    DialogLoaderLoopT* ll = reinterpret_cast<DialogLoaderLoopT*>(self);
    printf("[SplashDLT::Load] self=%p m_dialogExternal(before)=%p\n",
           (void*)self, ll->m_dialogExternal);
    if (fn_DialogLoaderLoopT_Load) {
        // Same bracket pattern as Stub_Startup. Stub_Load gets called when our
        // Stub_Startup synchronously drives `next->vtable->Load` on the next
        // splash loop, so this captures the *second* splash's load too.
        char label[96];
        std::snprintf(label, sizeof(label),
                      "fn_DialogLoaderLoopT_Load (Stub_Load) self=%p",
                      (void*)self);
        TraceExeBegin(label);
        fn_DialogLoaderLoopT_Load(self);
        TraceExeEnd(label);
        printf("[SplashDLT::Load] m_dialogExternal(after)=%p\n", ll->m_dialogExternal);
    } else {
        printf("[SplashDLT::Load] WARN: fn_DialogLoaderLoopT_Load is NULL\n");
    }
}

void Stub_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[SplashDLT::SaveRestore] self=%p save=%d state=%p\n",
           (void*)self, save, state);
}

void Stub_OnVideoError(UpdateLoop* self) {
    printf("[SplashDLT::OnVideoError] self=%p\n", (void*)self);
}

void Stub_OnVideoRestart(UpdateLoop* self) {
    printf("[SplashDLT::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Stub_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original thunks to Runtime DialogLoaderLoopT::GetResourceFilename which
    // returns "Game/Loader" (see runtime ?GetResourceFilename@DialogLoaderLoopT).
    return "Game/Loader";
}

bool Stub_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    // Original thunks to Runtime DialogLoaderLoopT::AllowsModChanges which
    // returns true (loader screens always permit mod swaps).
    return true;
}

extern "C" void* LoaderDialog_FromSplashDLT(void* loop, void* retInternalBasePtr);

void* Stub_CreateDialog(UpdateLoop* self, void* retPtr) {
    void* rsc = self ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x50) : nullptr;
    printf("[SplashDLT::CreateDialog] self=%p retPtr=%p rsc=%p -> LoaderDialog_FromSplashDLT\n",
           (void*)self, retPtr, rsc);
    // Mirrors exe LoaderDialog_Ctor3 @ 0x140099070: builds a real DialogInstance
    // subclass wrapping the loaded .rsc (Dialog/ShiningRock.rsc or
    // Dialog/Loading.rsc), patches our standalone vtable, returns IBP.
    return LoaderDialog_FromSplashDLT(self, retPtr);
}

// 13-slot vtable — 12 UpdateLoop slots + DialogLoaderLoopT's CreateDialog at +0x60.
// The original layout reserves a NULL at +0x68 (8 bytes of padding before the next
// vtable in .rdata), but it is never indexed at runtime so we omit it here.
void* g_splashDialogLoopVtable[13] = {
    reinterpret_cast<void*>(&Stub_Destroy),               // +0x00
    reinterpret_cast<void*>(&Stub_Startup),               // +0x08
    reinterpret_cast<void*>(&Stub_Shutdown),              // +0x10
    reinterpret_cast<void*>(&Stub_Update),                // +0x18
    reinterpret_cast<void*>(&Stub_Draw),                  // +0x20
    reinterpret_cast<void*>(&Stub_DrawFailed),            // +0x28
    reinterpret_cast<void*>(&Stub_Load),                  // +0x30
    reinterpret_cast<void*>(&Stub_SaveRestore),           // +0x38
    reinterpret_cast<void*>(&Stub_OnVideoError),          // +0x40
    reinterpret_cast<void*>(&Stub_OnVideoRestart),        // +0x48
    reinterpret_cast<void*>(&Stub_GetResourceFilename),   // +0x50
    reinterpret_cast<void*>(&Stub_AllowsModChanges),      // +0x58
    reinterpret_cast<void*>(&Stub_CreateDialog),          // +0x60
};

} // namespace

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterSplashDialogLoopVtable() {
    RegisterExeAddr(0x140158800, reinterpret_cast<uintptr_t>(g_splashDialogLoopVtable));
}

