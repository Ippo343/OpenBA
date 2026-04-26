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
    printf("[SplashDLT::Startup] self=%p next=%p\n", (void*)self, (void*)next);

    // Step 1: Engine::Loader::Load updates Modification state and calls
    // SetupFileSystem (needed before ExternalFactory::FindOrLoad can find rsc files
    // through the mod-aware path). It also signals an event the Loader thread waits
    // on — but standalone has no Loader thread running, so the actual loading work
    // we have to perform synchronously below.
    if (fn_Loader_Load && g_pLoader && next) {
        fn_Loader_Load(g_pLoader, next);
        printf("[SplashDLT::Startup] Loader::Load returned (next=%p)\n", (void*)next);
    }

    // Step 2: synchronously invoke the next loop's vtable[+0x30] Load slot — this
    // is what the Loader thread would call after waking up. For MenuDLT this fires
    // our Stub_Load → fn_DialogLoopT_Load → m_dialogExternal populated with
    // Dialog/MainMenu.rsc.
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
    printf("[SplashDLT::Load] self=%p\n", (void*)self);
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

void* Stub_CreateDialog(UpdateLoop* self, void* retPtr) {
    printf("[SplashDLT::CreateDialog] self=%p retPtr=%p (returning null IBP)\n",
           (void*)self, retPtr);
    // Pure-virtual override. Real impl creates a UI::DialogInstance subclass
    // (LoaderDialog) wrapping the loaded .rsc. Return an empty 16-byte
    // InternalPtr (zeroed) for now.
    if (retPtr) {
        uint64_t* p = reinterpret_cast<uint64_t*>(retPtr);
        p[0] = 0;
        p[1] = 0;
    }
    return retPtr;
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
