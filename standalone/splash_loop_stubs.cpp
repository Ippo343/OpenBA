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
#include "common.h"
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
    // pushes the IBP into the UI's pending-open list at self+0x88, then
    // calls Loader::Load on m_nextLoop.
    if (ll->m_dialogExternal && fn_DialogLoaderLoopT_Startup) {
        printf("[SplashDLT::Startup] invoking DialogLoaderLoopT::Startup\n");
        fn_DialogLoaderLoopT_Startup(self);
        printf("[SplashDLT::Startup] DialogLoaderLoopT::Startup returned, "
               "m_dialogInstance=%p\n", ll->m_dialogInstance);
    } else {
        printf("[SplashDLT::Startup] WARN: skipping DialogLoaderLoopT::Startup "
               "(rsc=%p fn=%p)\n", ll->m_dialogExternal,
               (void*)fn_DialogLoaderLoopT_Startup);
    }

    // Step 3: synchronously load the next loop's .rsc. DialogLoaderLoopT::Startup
    // already called Loader::Load(loader, next), but that just signals an event
    // — without a Loader thread we drive the next->Load() directly here so that
    // by the time we transition, next has a valid m_dialogExternal too.
    if (next && next->vtable && next->vtable->Load) {
        typedef void (*LoadFn)(UpdateLoop*);
        LoadFn loadFn = reinterpret_cast<LoadFn>(next->vtable->Load);
        printf("[SplashDLT::Startup] invoking next->vtable->Load(%p) at %p\n",
               (void*)next, (void*)loadFn);
        loadFn(next);
        printf("[SplashDLT::Startup] next->Load returned\n");
    }
}

void Stub_Shutdown(UpdateLoop* self) {
    printf("[SplashDLT::Shutdown] self=%p\n", (void*)self);
    // Original is RET 0 (literal no-op).
}

bool Stub_Update(UpdateLoop* self, void* time) {
    DialogLoaderLoopT* ll = reinterpret_cast<DialogLoaderLoopT*>(self);
    Time* dt = reinterpret_cast<Time*>(time);

    // Pump the embedded UI — flushes the pending-open list (where Startup
    // pushed our LoaderDialog), runs each open dialog's OnUpdate, processes
    // pending-close, and advances animation. Without this the splash dialog
    // is created but never moved into the active list, so Draw walks an
    // empty list and the screen stays blank. Mirrors Runtime
    // DialogLoaderLoopT::Update @ 0x1800daf20.
    if (fn_UI_Update) {
        void* ui = reinterpret_cast<uint8_t*>(self) + 0x60;
        fn_UI_Update(ui, time);
    }

    // Loading-screen frame cap (matches real LoaderLoop ~14ms sleep).
    Sleep(14);

    ll->m_elapsedTicks += dt->ticks;

    // Once the splash hold elapses, transition. Skip the m_finished check —
    // standalone has no real Loader thread, so m_finished stays 0 forever.
    if (ll->m_elapsedTicks >= ll->m_delayTicks) {
        UpdateLoop* next = ll->m_nextLoop;
        void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
        printf("[SplashDLT::Update] self=%p elapsed=%lld delay=%lld -> transition next=%p\n",
               (void*)self, (long long)ll->m_elapsedTicks,
               (long long)ll->m_delayTicks, (void*)next);
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
    (void)alpha;
    // Mirror Runtime DialogLoaderLoopT::Draw — call UI::UserInterface::Draw on
    // the embedded UI at +0x60 (DialogLoaderLoopT layout, NOT +0x40 like
    // DialogLoopT). With no dialog instance yet (Stub_CreateDialog returns
    // null), the UI's dialog list is empty and Draw effectively just sets the
    // camera and clears the SpriteContext — enough to avoid the uncleared-
    // back-buffer flicker we see during the 2.5s splash chain.
    static int draws = 0;
    bool trace = (draws++ < 3);

    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    void* context = mainLoop
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mainLoop) + 0x10)
        : nullptr;
    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x60);

    if (trace) {
        printf("[SplashDLT::Draw] self=%p alpha=%f mainLoop=%p ctx=%p ui=%p\n",
               (void*)self, alpha, mainLoop, context, ui);
    }

    if (fn_UI_Draw && context) {
        fn_UI_Draw(ui, context);
    } else if (trace) {
        printf("[SplashDLT::Draw] WARN: skipping UI::Draw (fn_UI_Draw=%p ctx=%p)\n",
               (void*)fn_UI_Draw, context);
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

#endif // STANDALONE_EXE
