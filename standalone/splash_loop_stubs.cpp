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

#ifdef STANDALONE_EXE

#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "UpdateLoop.h"
#include "DialogLoaderLoopT.h"
#include "MainLoop.h"
#include "common.h"
#include "async_loader.h"
#include "stall_debug.h"
#include "Game/dialogloaderloop.h"
#include "Game/uifade.h"
#include <cstdio>
#include <cstdint>
#include <windows.h>

namespace {

void Stub_Destroy(UpdateLoop* self, unsigned flags) {
    printf("[SplashDLT::Destroy] self=%p flags=%u\n", (void*)self, flags);
    // Skip the real cleanup chain (DialogLoaderLoopT::~DialogLoaderLoopT unwinds
    // UI::UserInterface, String, Audio::Scene, Random — all leak on this run but
    // the process exits immediately after MainLoop::Run returns).
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Stub_Startup(UpdateLoop* self) {
    StallDebug::Phase startupPhase("SplashDLT::Stub_Startup");
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

    // Step 3: kick our standalone async loader on next->vtable->Load. This
    // replaces the Runtime Engine::Loader thread (which hangs in standalone
    // mode). The worker thread runs MenuDLT::Load (or LoadingSplash::Load
    // etc.) in parallel with this splash's Update/Draw, so the splash
    // animation no longer freezes for ~125ms while MenuDLT::Load runs.
    // Stub_Update gates the transition on AsyncLoader::IsAsyncLoadBusy().
    if (next) {
        AsyncLoader::StartAsyncLoad(next);
    }

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

    if (clamped) {
        dt->ticks = origTicks;
    }

    // Loading-screen frame cap (matches real LoaderLoop ~14ms sleep).
    Sleep(14);

    ll->m_elapsedTicks += dt->ticks;

    // Once the splash hold elapses, kick off the fade-OUT (to opaque black)
    // and wait one extra tick before transitioning. Mirrors the original
    // FUN_140098DA0 (LoaderLoop::Update) sequence.
    if (ll->m_elapsedTicks >= ll->m_delayTicks) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;

        static UpdateLoop* fadeOutInitiatedFor = nullptr;
        if (fadeOutInitiatedFor != self) {
            fadeOutInitiatedFor = self;
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

#endif // STANDALONE_EXE
