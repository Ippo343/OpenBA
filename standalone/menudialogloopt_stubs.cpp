// standalone/menudialogloopt_stubs.cpp
// Standalone-only stub vtable for MenuDialogLoopT (exe vtable @ 0x14015ed00).
//
// MenuDialogLoopT is the exe-internal subclass of Engine::DialogLoopT that drives
// the main menu (Dialog\MainMenu.rsc). Its 13-slot vtable lives at 0x14015ed00 in
// the original exe; in standalone mode we provide our own vtable here.
//
// Slot layout (13 entries — UpdateLoop's 12 + DialogLoopT's CreateDialog at +0x60):
//   +0x00  Destroy(self, flags)           — exe 0x14001A2E0 (scalar deleting dtor)
//   +0x08  Startup(self, remainder)       — exe 0x14009B930
//   +0x10  Shutdown(self)                 — exe 0x14009B280
//   +0x18  Update(self, time)             — exe 0x14009B6E0
//   +0x20  Draw(self, alpha)              — exe 0x14009B440
//   +0x28  DrawFailed(self)               — exe 0x14003A90C (thunk -> Runtime DialogLoopT::DrawFailed)
//   +0x30  Load(self)                     — exe 0x14009A930
//   +0x38  SaveRestore(self, save, state) — exe 0x14003A906 (thunk)
//   +0x40  OnVideoError(self)             — exe 0x14009A8D0
//   +0x48  OnVideoRestart(self)           — exe 0x14009A8B0
//   +0x50  GetResourceFilename(self)      — exe 0x14001A2D0  -> "Game/Menu"
//   +0x58  AllowsModChanges(self)         — exe 0x14001A2C0  -> true
//   +0x60  CreateDialog(self, retptr)     — exe 0x14009BA30
//
// First-pass implementation: everything except GetResourceFilename, AllowsModChanges,
// and Destroy is a stub that prints and returns a safe default. Update returns false
// so MainLoop::Run exits immediately after the first tick. This lets standalone drive
// MenuDialogLoopT_Ctor end-to-end without yet implementing all 11 game-specific slots.
//
// As real reimplementations land in code/Game/Game/menudialogloopt.cpp, replace the
// matching stub entry with the real function and remove the stub here.

#ifdef STANDALONE_EXE

#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "UpdateLoop.h"
#include "Game/menudialogloopt_funcs.h"
#include <cstdio>
#include <cstdint>
#include <cstring>

namespace {

// Mirror the cursor-loading subset of PlacementTool_Ctor (exe 0x14009A4F0).
// The original game loads 6 cursors via ExternalList::Find against the Inner
// LoaderLoop's m_resourceList (populated by the Loader thread). In standalone
// we have no Loader thread, so we bypass the list and call ExternalFactory::
// FindOrLoad directly — same end result, just synchronous and on-demand.
//
// Cursor field offsets (KeyMap+offset, qword index):
//   +0x28 (idx 5)  — Cursor/Arrow.rsc:cursor    (DEFAULT, used by MenuDLT)
//   +0x30 (idx 6)  — Cursor/Hand.rsc:cursor     (used by GameUpdateLoop)
//   +0x38 (idx 7)  — Cursor/Hourglass.rsc:cursor (used by SaveLoad/Loader screens)
//   +0x40 (idx 8)  — Cursor/Move.rsc:cursor
//   +0x48 (idx 9)  — Cursor/Query.rsc:cursor
//   +0x50 (idx 10) — Cursor/Place.rsc:cursor
static void EnsureCursorsLoaded() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) {
        printf("[CursorLoad] WARN: g_pKeyMapSingleton not populated, skipping\n");
        return;
    }
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        printf("[CursorLoad] WARN: ExternalFactory singleton not populated, skipping\n");
        return;
    }
    if (!fn_Filename_CtorCStr || !fn_Filename_Dtor || !fn_ExternalFactory_FindOrLoad) {
        printf("[CursorLoad] WARN: required engine fns not resolved\n");
        return;
    }

    void* keyMap = *g_pKeyMapSingleton;
    void* factory = *g_pExternalFactorySingleton;

    // Skip if Arrow already loaded (this fn may be called multiple times).
    if (*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + 0x28)) {
        return;
    }

    auto loadAndStore = [&](uintptr_t offset, const char* name) {
        uint8_t fnBuf[16] = {};  // System::Filename is 8-16 bytes
        fn_Filename_CtorCStr(fnBuf, name);
        void* ext = fn_ExternalFactory_FindOrLoad(factory, fnBuf);
        fn_Filename_Dtor(fnBuf);
        *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + offset) = ext;
        printf("[CursorLoad] %-32s -> %p (KeyMap+0x%02llx)\n",
               name, ext, (unsigned long long)offset);
    };

    loadAndStore(0x28, "Cursor/Arrow.rsc:cursor");
    loadAndStore(0x30, "Cursor/Hand.rsc:cursor");
    loadAndStore(0x38, "Cursor/Hourglass.rsc:cursor");
    loadAndStore(0x40, "Cursor/Move.rsc:cursor");
    loadAndStore(0x48, "Cursor/Query.rsc:cursor");
    loadAndStore(0x50, "Cursor/Place.rsc:cursor");
}

void Stub_Destroy(UpdateLoop* self, int flags) {
    printf("[MenuDLT::Destroy] self=%p flags=%d\n", (void*)self, flags);
    // Skip the real cleanup chain for now (Audio::Scene dtor, Engine::DialogLoopT dtor,
    // UI::UserInterface dtor, String dtor). Those will leak on this run but are safe to
    // skip because the process exits immediately after MainLoop::Run returns.
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
}

void Stub_Startup(UpdateLoop* self, int64_t remainder) {
    void* dialogExt = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Startup] self=%p remainder=%lld m_dialogExternal=%p\n",
           (void*)self, (long long)remainder, dialogExt);
    // Original MenuDLT::Startup (exe 0x14009B930) chains Engine::DialogLoopT::Startup
    // (Runtime 0x1800db2f0) which:
    //   1. Calls vtable[+0x60] CreateDialog → MainMenuDialog_FromMenuDLT.
    //   2. Stores the InternalPtr at self+0x38 (m_dialogInstance).
    //   3. Invokes the dialog's Slot4 (OnOpened/Setup).
    //   4. Pushes the IBP into the UI's pending-open dialog list.
    // M2 wiring (SplashDLT → Loader::Load → MenuDLT::Load → DialogLoopT::Load)
    // populates m_dialogExternal before this fires, so DialogInstance::DialogInstance
    // and the UI list iteration both have valid data to read.
    if (!fn_DialogLoopT_Startup) {
        printf("[MenuDLT::Startup] fn_DialogLoopT_Startup is NULL — staying as stub\n");
        return;
    }
    if (!dialogExt) {
        printf("[MenuDLT::Startup] WARN: m_dialogExternal is NULL, skipping Startup "
               "(would crash in DialogInstance ctor)\n");
        return;
    }
    fn_DialogLoopT_Startup(self);
    void* dialogInst = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x38);
    printf("[MenuDLT::Startup] DialogLoopT::Startup returned, m_dialogInstance=%p\n",
           dialogInst);

    // Mirror the cursor block from original MenuDLT::Startup (exe 0x14009B930):
    //   UI::SetCursor(ui, *(KeyMap+0x28))    — engine cursor sprite (Cursor/Arrow)
    //   UI::EnableCursor(ui, true)           — OS cursor visibility
    // The original game's cursor sprites are populated into KeyMap+0x28..+0x50 by
    // PlacementTool_Ctor (exe 0x14009A4F0), which the Inner LoaderLoop's Load slot
    // would call. Standalone has no Loader thread so we load the cursors lazily
    // here via ExternalFactory::FindOrLoad — same end result, synchronous.
    EnsureCursorsLoaded();

    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x40);
    void* keyMap = (g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    void* arrowCursor = keyMap
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + 0x28)
        : nullptr;
    if (fn_UI_SetCursor && arrowCursor) {
        fn_UI_SetCursor(ui, reinterpret_cast<uint64_t>(arrowCursor));
        printf("[MenuDLT::Startup] UI::SetCursor(ui=%p, arrow=%p)\n", ui, arrowCursor);
    } else {
        printf("[MenuDLT::Startup] WARN: skipping SetCursor (fn=%p arrow=%p)\n",
               (void*)fn_UI_SetCursor, arrowCursor);
    }
    if (fn_UI_EnableCursor) {
        fn_UI_EnableCursor(ui, true);
        printf("[MenuDLT::Startup] UI::EnableCursor(true) called on ui=%p\n", ui);
    } else {
        printf("[MenuDLT::Startup] WARN: fn_UI_EnableCursor is NULL\n");
    }
}

void Stub_Shutdown(UpdateLoop* self) {
    printf("[MenuDLT::Shutdown] self=%p\n", (void*)self);
}

bool Stub_Update(UpdateLoop* self, void* time) {
    // Delegate to Engine::DialogLoopT::Update — the original MenuDLT::Update
    // (exe 0x14009B6E0) tail-jumps into it after handling cursor/audio/3D-scene
    // work we're skipping for now. DialogLoopT::Update calls UI::UserInterface::Update
    // on the embedded UI at self+0x40 and always returns true, keeping MainLoop::Run
    // alive on the menu loop.
    static int tick = 0;
    ++tick;
    if (tick <= 4 || (tick % 300) == 0) {
        printf("[MenuDLT::Update] self=%p time=%p tick=%d -> DialogLoopT::Update\n",
               (void*)self, time, tick);
    }
    if (!fn_DialogLoopT_Update) {
        printf("[MenuDLT::Update] fn_DialogLoopT_Update is NULL, returning false\n");
        return false;
    }
    return fn_DialogLoopT_Update(self, time);
}

void Stub_Draw(UpdateLoop* self, float alpha) {
    // Mirror original MenuDLT::Draw (exe 0x14009B440):
    //   ctx = engine_singleton->m_context   (engine+0x10)
    //   if (self+0xb90 != null) {  // MenuScene loaded
    //       <camera setup, sun light, fog, Renderer::Draw>
    //   }
    //   UI::UserInterface::Draw(self+0x40, ctx)
    //
    // self+0x40 is the embedded UI::UserInterface (DialogLoopT layout).
    static int draws = 0;
    ++draws;
    bool trace = (draws <= 3) || (draws % 200) == 0;

    // Use MainLoop singleton's m_context — populated by MainLoop ctor and
    // unchanged for the run. (g_pEngineSingleton was captured at boot when
    // the slot was still null, so it can't be read directly.)
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    void* context = mainLoop
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mainLoop) + 0x10)
        : nullptr;
    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x40);

    if (trace) {
        void* menuScene = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(self) + 0xb90);
        printf("[MenuDLT::Draw] self=%p alpha=%f mainLoop=%p ctx=%p ui=%p menuScene=%p\n",
               (void*)self, alpha, mainLoop, context, ui, menuScene);
    }

    // Phase A: render the 3D menu scene before the UI overlay.
    if (context) {
        MenuDialogLoopT_DrawScene_PhaseA(self, alpha, context);
    }

    if (fn_UI_Draw && context) {
        fn_UI_Draw(ui, context);
    } else if (trace) {
        printf("[MenuDLT::Draw] WARN: skipping UI::Draw (fn_UI_Draw=%p ctx=%p)\n",
               (void*)fn_UI_Draw, context);
    }
}

void Stub_DrawFailed(UpdateLoop* self) {
    // Fired by Engine::MainLoop::Draw when BeginFrame (vtable+0x08 of
    // Video::Context+0x2160) returns false — i.e. the window is occluded,
    // minimized, or the DX11 swap chain is in a Reset-needed state. After
    // this returns, MainLoop::Draw calls Video::Context::Reset which is
    // the recovery path. Log the first few + every 100th so we can tell
    // whether Reset is succeeding (Draw resumes) or stuck.
    static int n = 0;
    ++n;
    if (n <= 5 || (n % 100) == 0) {
        printf("[MenuDLT::DrawFailed] self=%p count=%d\n", (void*)self, n);
        fflush(stdout);
    }
}

void Stub_Load(UpdateLoop* self) {
    // Original MenuDLT::Load (exe 0x14009A930) tail-calls Engine::DialogLoopT::Load
    // first to populate self+0x30 (m_dialogExternal) from m_dialogFilename
    // ("Dialog\MainMenu.rsc"), then loads companion resources (Audio/Music/Menu.rsc,
    // Game/MenuScene/SceneN.rsc) for the 3D menu background. We delegate the base
    // Load to Engine::DialogLoopT, then run Phase A scene allocation for the menu
    // background. Audio music + TradeWork + static-mesh + particle iteration are
    // out of Phase A scope.
    void* dialogExtBefore = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] self=%p m_dialogExternal(before)=%p\n", (void*)self, dialogExtBefore);
    if (!fn_DialogLoopT_Load) {
        printf("[MenuDLT::Load] fn_DialogLoopT_Load is NULL — cannot load dialog rsc\n");
        return;
    }
    fn_DialogLoopT_Load(self);
    void* dialogExtAfter = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] m_dialogExternal(after)=%p\n", dialogExtAfter);

    // Phase A: load Game/MenuScene/SceneN.rsc + allocate Renderer/Scene/SunLight/
    // Transform/MeshInstance and add to global scene list. Failure is non-fatal —
    // the menu UI still renders without the 3D background.
    bool sceneOk = MenuDialogLoopT_LoadScene_PhaseA(self);
    printf("[MenuDLT::Load] LoadScene_PhaseA returned %s\n", sceneOk ? "true" : "false");
}

void Stub_SaveRestore(UpdateLoop* self, bool save, void* state) {
    printf("[MenuDLT::SaveRestore] self=%p save=%d state=%p\n", (void*)self, save, state);
}

void Stub_OnVideoError(UpdateLoop* self) {
    printf("[MenuDLT::OnVideoError] self=%p\n", (void*)self);
}

void Stub_OnVideoRestart(UpdateLoop* self) {
    printf("[MenuDLT::OnVideoRestart] self=%p\n", (void*)self);
}

const char* Stub_GetResourceFilename(UpdateLoop* self) {
    (void)self;
    // Original exe slot 10 returns the literal "Game/Menu" via LEA RAX,[rip+...].
    return "Game/Menu";
}

bool Stub_AllowsModChanges(UpdateLoop* self) {
    (void)self;
    // Original exe slot 11 = MOV AL,1; RET — i.e. always true.
    return true;
}

extern "C" void* MainMenuDialog_FromMenuDLT(void* loop, void* retInternalBasePtr);

void* Stub_CreateDialog(UpdateLoop* self, void* retPtr) {
    void* rsc = self ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30) : nullptr;
    printf("[MenuDLT::CreateDialog] self=%p retPtr=%p rsc=%p -> MainMenuDialog_FromMenuDLT\n",
           (void*)self, retPtr, rsc);
    // M2 wired Loader::Load → MenuDLT::Load → DialogLoopT::Load, so self+0x30
    // (m_dialogExternal) is populated with Dialog/MainMenu.rsc by the time
    // DialogLoopT::Startup invokes this slot. Delegate to the standalone
    // MainMenuDialog factory which mirrors the exe ctor at 0x14009BA30:
    // builds the DialogInstance + patches our 20-slot vtable + writes the
    // resulting IBP into retPtr.
    return MainMenuDialog_FromMenuDLT(self, retPtr);
}

// 13-slot vtable — exposed via RegisterMenuDialogLoopVtable() to the address registry.
void* g_menuDialogLoopVtable[13] = {
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
void RegisterMenuDialogLoopVtable() {
    RegisterExeAddr(0x14015ed00, reinterpret_cast<uintptr_t>(g_menuDialogLoopVtable));
}

#endif // STANDALONE_EXE
