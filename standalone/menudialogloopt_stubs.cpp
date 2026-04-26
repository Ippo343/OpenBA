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
#include <cstdio>
#include <cstdint>

namespace {

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
    (void)alpha;
    // Mirror original MenuDLT::Draw (exe 0x14009B440): when m_scene (self+0xb90)
    // is null we skip the 3D scene block and just draw the UI. Standalone
    // currently has no menu 3D scene set up, so this path is correct here.
    //
    //   ctx = engine_singleton->m_context   (engine+0x10)
    //   UI::UserInterface::Draw(self+0x40, ctx)
    //
    // self+0x40 is the embedded UI::UserInterface (DialogLoopT layout).
    static int draws = 0;
    bool trace = (draws++ < 3);

    // Use MainLoop singleton's m_context — populated by MainLoop ctor and
    // unchanged for the run. (g_pEngineSingleton was captured at boot when
    // the slot was still null, so it can't be read directly.)
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    void* context = mainLoop
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mainLoop) + 0x10)
        : nullptr;
    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x40);

    if (trace) {
        printf("[MenuDLT::Draw] self=%p alpha=%f mainLoop=%p ctx=%p ui=%p\n",
               (void*)self, alpha, mainLoop, context, ui);
    }

    if (fn_UI_Draw && context) {
        fn_UI_Draw(ui, context);
    } else if (trace) {
        printf("[MenuDLT::Draw] WARN: skipping UI::Draw (fn_UI_Draw=%p ctx=%p)\n",
               (void*)fn_UI_Draw, context);
    }
}

void Stub_DrawFailed(UpdateLoop* self) {
    printf("[MenuDLT::DrawFailed] self=%p\n", (void*)self);
}

void Stub_Load(UpdateLoop* self) {
    // Original MenuDLT::Load (exe 0x14009A930) tail-calls Engine::DialogLoopT::Load
    // first to populate self+0x30 (m_dialogExternal) from m_dialogFilename ("Dialog\MainMenu.rsc"),
    // then loads companion resources (Audio/Music/Menu.rsc, Game/MenuScene/SceneN.rsc) for the
    // 3D menu background. We delegate only the base Load — companion 3D scene work is
    // deferred to M3 (rendering). This is enough to populate m_dialogExternal so that
    // DialogLoopT::Startup → CreateDialog → MainMenuDialog can succeed.
    void* dialogExtBefore = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] self=%p m_dialogExternal(before)=%p\n", (void*)self, dialogExtBefore);
    if (!fn_DialogLoopT_Load) {
        printf("[MenuDLT::Load] fn_DialogLoopT_Load is NULL — cannot load dialog rsc\n");
        return;
    }
    fn_DialogLoopT_Load(self);
    void* dialogExtAfter = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] m_dialogExternal(after)=%p\n", dialogExtAfter);
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
