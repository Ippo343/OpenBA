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


#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "renderdoc_integration.h"
#include "stall_debug.h"
#include "redzone_alloc.h"
#include "frame_limiter.h"
#include "modal_driver.h"
#include "mouse_inject.h"
#include "common.h"
#include "Object/factory_registration.h"

// WAVE 11 LANE OBJLIFE — the synthesised factory-dtor oracle, called from
// Stub_Draw. Declared at FILE SCOPE on purpose: a block-scope `extern` inside
// Stub_Draw is declared in the enclosing ANONYMOUS namespace, which links
// against a symbol that does not exist rather than the real one.
void DlgCtrlRoute_Arm();                  // standalone/dlgctrl_route.cpp
#include "UpdateLoop.h"
#include "Game/menudialogloopt_funcs.h"
#include "Game/uifade.h"
#include "autostart.h"
#include <cstdio>
#include <cstdint>
#include "system/Filename.h"   // -Iruntime/types
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "ui/Sprite.h"          // ENDGAME B.3.6 UI callers (K4)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include <new>
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

    void* keyMap = *g_pKeyMapSingleton;
    void* factory = *g_pExternalFactorySingleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge

    // Skip if Arrow already loaded (this fn may be called multiple times).
    if (*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + 0x28)) {
        return;
    }

    auto loadAndStore = [&](uintptr_t offset, const char* name) {
        const System::Filename fn(name);
        void* ext = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fn)));
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

// Mirror the UI-sound block of PlacementTool_Ctor (exe 0x140099db0), which
// loads nine Audio/UI/*Effect.rsc SoundEffects into the KeyMap/PlacementTool
// container at +0x58..+0x98 — the slots right after the cursors. The retail
// Loader thread runs PlacementTool_Ctor; standalone has none, so we load them
// on demand exactly like EnsureCursorsLoaded (synchronous ExternalFactory::
// FindOrLoad, no IsDerivedFrom check — the Runtime factory tree is incomplete).
//
// MenuDialogLoopT_BindUISounds then copies five of these (BeginDrag/EndDrag/
// ButtonDown/ButtonUp/Alert) into the menu UserInterface so the element
// framework plays hover/press/click sounds automatically. The other four
// (PlacementMove/Thunk/Error/Warning) are in-game placement-tool sounds —
// loaded here for completeness/M5 but not bound to the menu UI.
//
// Container slot offsets match PlacementTool_Ctor's param_1[0xb..0x13]:
//   +0x58 Alert  +0x60 ButtonDown  +0x68 ButtonUp  +0x70 BeginDrag
//   +0x78 EndDrag  +0x80 PlacementMove  +0x88 PlacementThunk
//   +0x90 PlacementError  +0x98 Warning
static void EnsureUISoundsLoaded() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) {
        printf("[UISoundLoad] WARN: g_pKeyMapSingleton not populated, skipping\n");
        return;
    }
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) {
        printf("[UISoundLoad] WARN: ExternalFactory singleton not populated, skipping\n");
        return;
    }

    void* keyMap = *g_pKeyMapSingleton;
    void* factory = *g_pExternalFactorySingleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
    uintptr_t kmBase = reinterpret_cast<uintptr_t>(keyMap);

    // Skip if AlertEffect (first sound slot) already loaded.
    if (*reinterpret_cast<void**>(kmBase + 0x58)) {
        return;
    }

    auto loadAndStore = [&](uintptr_t offset, const char* name) {
        const System::Filename fn(name);
        void* ext = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(fn)));
        *reinterpret_cast<void**>(kmBase + offset) = ext;
        printf("[UISoundLoad] %-34s -> %p (KeyMap+0x%02llx)\n",
               name, ext, (unsigned long long)offset);
    };

    loadAndStore(0x58, "Audio/UI/AlertEffect.rsc");
    loadAndStore(0x60, "Audio/UI/ButtonDownEffect.rsc");
    loadAndStore(0x68, "Audio/UI/ButtonUpEffect.rsc");
    loadAndStore(0x70, "Audio/UI/BeginDragEffect.rsc");
    loadAndStore(0x78, "Audio/UI/EndDragEffect.rsc");
    loadAndStore(0x80, "Audio/UI/PlacementMoveEffect.rsc");
    loadAndStore(0x88, "Audio/UI/PlacementThunkEffect.rsc");
    loadAndStore(0x90, "Audio/UI/PlacementErrorEffect.rsc");
    loadAndStore(0x98, "Audio/UI/WarningEffect.rsc");
}

}  // namespace

// Mirror the SpriteSheet block from PlacementTool_Ctor (exe 0x140099ff0,
// last segment). The original loads Dialog/SpriteSheet.rsc into KeyMap+0xa0,
// then constructs two named Sprites ("White" at +0xa8/+0xb0, "SaveLoadEmpty"
// at +0xb8/+0xc0). The "White" sprite is what UI::UserInterface uses for
// the fullscreen-fade overlay (read by OptionsDialog_helper_9260 from
// KeyMap+0xa8/+0xb0 and passed to SetFullscreenFadeSprite).
//
// Without this, KeyMap+0xa8 stays null, EnsureFadeSprite installs a null
// sheet into UI+0xE0, and Runtime UI::Draw skips the overlay (gates on
// sheet != 0). The fade STATE machine still runs but nothing visible
// covers the screen during transitions → hard-cut flashes.
//
// Idempotent — exits early if SpriteSheet already loaded. Exposed so
// game_main.cpp can call it BEFORE MainLoop::Run starts (so SplashDLT's
// fade triggers find a populated KeyMap+0xa8 too, not just MenuDLT).
extern "C" void StandaloneEnsureSpriteSheetLoaded() {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return;
    if (!g_pExternalFactorySingleton || !*g_pExternalFactorySingleton) return;
    void* keyMap = *g_pKeyMapSingleton;
    void* factory = *g_pExternalFactorySingleton;
    ::Object::ExternalFactory* efactory =
        reinterpret_cast<::Object::ExternalFactory*>(factory);  // B.3.3 bridge
    uintptr_t kmBase = reinterpret_cast<uintptr_t>(keyMap);

    // Skip if SpriteSheet already loaded.
    if (*reinterpret_cast<void**>(kmBase + 0xa0)) return;

    // Step 1: load Dialog/SpriteSheet.rsc into KeyMap+0xa0.
    const System::Filename sheetPath("Dialog/SpriteSheet.rsc");
    void* spriteSheet = const_cast<void*>(static_cast<const void*>(efactory->FindOrLoad(sheetPath)));
    *reinterpret_cast<void**>(kmBase + 0xa0) = spriteSheet;
    printf("[SpriteSheetLoad] Dialog/SpriteSheet.rsc -> %p (KeyMap+0xa0)\n",
           spriteSheet);
    if (!spriteSheet) return;

    // Step 2: construct named Sprites and store at KeyMap+0xa8/+0xb0 ("White")
    // and +0xb8/+0xc0 ("SaveLoadEmpty"). fn_Sprite_Ctor signature is
    // (retval, spriteSheetExt, name) — non-trivial-return ABI but the helper
    // typedef already returns void* (the retbuf). We pass a 16-byte stack
    // buffer as retval and copy it into KeyMap.
    auto buildSprite = [&](uintptr_t kmOffset, const char* name) {
        uint8_t spriteBuf[16] = {};
        const System::String nameStr(name);
        ::new (spriteBuf) UI::Sprite(
            Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(spriteSheet)),  // B.3.6 bridge
            nameStr);
        *reinterpret_cast<uint64_t*>(kmBase + kmOffset)       =
            *reinterpret_cast<uint64_t*>(&spriteBuf[0]);
        *reinterpret_cast<uint64_t*>(kmBase + kmOffset + 8)   =
            *reinterpret_cast<uint64_t*>(&spriteBuf[8]);
        printf("[SpriteSheetLoad] Sprite(\"%s\") -> sheet=%p idx=%d "
               "(KeyMap+0x%02llx)\n",
               name,
               *reinterpret_cast<void**>(&spriteBuf[0]),
               *reinterpret_cast<int32_t*>(&spriteBuf[8]),
               (unsigned long long)kmOffset);
    };

    buildSprite(0xa8, "White");           // fullscreen-fade overlay sprite
    buildSprite(0xb8, "SaveLoadEmpty");   // save/load placeholder sprite
}

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
    StallDebug::Phase startupPhase("MenuDLT::Stub_Startup");

    // Menu-side Save::SaveLoad (+0xb70) — retail MenuDLT keeps its own SaveLoad
    // and ticks it every frame (MenuDLT::Update 0x14009b71f calls the
    // ?Update@SaveLoad dispatch slot on *(self+0xb70)); the menu Load button's
    // list-scan/load operations can only progress through that tick. The ctor
    // also publishes Save::SaveLoad::_singleton, which SaveLoadDialog and the
    // autosave path read. (Save/load wave 2026-08-27.)
    {
        uintptr_t s = reinterpret_cast<uintptr_t>(self);
        if (*reinterpret_cast<void**>(s + 0xb70) == nullptr) {
            typedef void* (*fn_slctor_t)(void* self);
            static fn_slctor_t s_slCtor = reinterpret_cast<fn_slctor_t>(
                ResolveExport("??0SaveLoad@Save@@QEAA@XZ"));
            void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x90, 4) : nullptr;
            if (mem && s_slCtor) {
                s_slCtor(mem);
                *reinterpret_cast<void**>(s + 0xb70) = mem;
                printf("[MenuDLT::Startup] menu SaveLoad created at +0xb70 = %p\n", mem);
            }
        }
    }

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

    // Phase A: by the time MenuDLT activates, the Loading splash's
    // ChunkedSceneLoader has already run LoadSceneA1 + chunked PhaseB +
    // LoadSceneA3 (driven from splash_loop_stubs.cpp Stub_Startup +
    // Stub_Update across the splash's display period). MenuDLT::Stub_Startup
    // just confirms the work was done and falls back to synchronous
    // LoadScene_PhaseA recovery if some boot path skipped the splash chain
    // (e.g. direct MenuDLT entry from a debug harness).
    void* menuSceneExt = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(self) + 0xb90);
    if (!menuSceneExt) {
        printf("[MenuDLT::Startup] WARN: scene fields empty (Loading splash "
               "didn't run ChunkedSceneLoader) — falling back to synchronous "
               "LoadScene_PhaseA\n");
        StallDebug::Phase phaseA("MenuDLT LoadScene_PhaseA fallback");
        bool sceneOk = MenuDialogLoopT_LoadScene_PhaseA(self);
        printf("[MenuDLT::Startup] fallback LoadScene_PhaseA returned %s\n",
               sceneOk ? "true" : "false");
    } else {
        printf("[MenuDLT::Startup] scene already populated by Loading "
               "splash ChunkedSceneLoader (menuScene=%p) — skipping "
               "LoadScene_PhaseA\n", menuSceneExt);
    }

    {
        StallDebug::Phase dialogPhase("MenuDLT fn_DialogLoopT_Startup");
        fn_DialogLoopT_Startup(self);
    }
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
    StandaloneEnsureSpriteSheetLoaded();
    // Load the UI hover/press/click SoundEffects into the KeyMap container
    // (retail does this in PlacementTool_Ctor via the Loader thread, which
    // standalone lacks). MenuDialogLoopT_BindUISounds below copies them into
    // the UserInterface's sound table.
    EnsureUISoundsLoaded();

    void* ui = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(self) + 0x40);
    void* keyMap = (g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    void* arrowCursor = keyMap
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + 0x28)
        : nullptr;
    if (arrowCursor) {
        reinterpret_cast<UI::UserInterface*>(ui)->SetCursor(  // B.3.6 bridge
            Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(arrowCursor)));
        printf("[MenuDLT::Startup] UI::SetCursor(ui=%p, arrow=%p)\n", ui, arrowCursor);
    } else {
        printf("[MenuDLT::Startup] WARN: skipping SetCursor (fn=%p arrow=%p)\n",
               (void*)fn_UI_SetCursor, arrowCursor);
    }
    reinterpret_cast<UI::UserInterface*>(ui)->EnableCursor(true);  // B.3.6 bridge
    printf("[MenuDLT::Startup] UI::EnableCursor(true) called on ui=%p\n", ui);

    // Trigger Startup-reveal fade — instant black -> animated reveal over ~0.25s.
    // Without this, the splash->menu transition hard-cuts. The original calls
    // SetFullscreenFade twice from OptionsDialog_impl_8fa0 in the same shape.
    // Chunked PhaseB completed during the Loading splash, so the scene is
    // fully populated by now — we can fade in immediately.
    UIFade::EnsureFadeSprite(ui);
    UIFade::TriggerReveal(ui);

    // Start the menu music. Mirrors the Audio::Scene::PlayEffect block near the
    // end of the original MenuDLT::Startup (exe 0x14009b930): registers the
    // loop's Audio::Scene (self+0xb30) at the DialogInstance base (self+0xd0) and
    // plays Audio/Music/Menu.rsc (loaded into self+0xb88 by Stub_Load) on the
    // music channel. Per-frame Audio::Scene::Update (Stub_Update) advances it.
    MenuDialogLoopT_StartMusic(self);

    // Bind the UI hover/press/click sounds onto the embedded UserInterface
    // (self+0x40), mirroring OptionsDialog_helper_9260's sound block. Must run
    // after StartMusic wired the audio scene at self+0xd0 (== UI+0x90) and
    // after EnsureUISoundsLoaded populated the KeyMap container. The element
    // framework (ButtonElement/Element::Update, driven by fn_DialogLoopT_Update
    // in Stub_Update) then plays these automatically on hover/click.
    MenuDialogLoopT_BindUISounds(self);
}

void Stub_Shutdown(UpdateLoop* self) {
    printf("[MenuDLT::Shutdown] self=%p\n", (void*)self);
    // Retail MenuDLT::Shutdown (exe 0x14009b280) tears down the menu scene,
    // renderer and SaveLoad (still skipped here — a leak, not a race), then
    // DialogLoopT::Shutdown, then `Audio::Scene::StopAll(self+0xb30)` at
    // 0x14009b41a. The StopAll is NOT skippable: it is what parks the streamed
    // menu music's thread before the next LoaderLoop's Loader::Load remounts
    // the packages under it (open defect (c): the stream thread faulting in
    // FileSystem::ReadFile on `_packfiles[]`). See MenuDialogLoopT_StopMusic.
    MenuDialogLoopT_StopMusic(self);
}

bool Stub_Update(UpdateLoop* self, void* time) {
    StallDebug::Tick("MenuDLT::Update");
    // Wave 10C lane ROUTES: install the DialogController inherited-slot route
    // (BANISHED_DLGCTRL_ROUTE=1|probe, else a silent no-op). Same placement
    // rationale as the line above -- Object::Factory::Startup has run, and no
    // dialog has been opened yet.
    DlgCtrlRoute_Arm();
    // Per-frame heap-integrity check (no-op unless BANISHED_REDZONE set). Catches
    // free-list poisoning that surfaces during the steady-state menu loop, where
    // our alloc/free are rare so the alloc/free auto-scan rarely fires.
    RedzoneAlloc_HeapCheck("menu Update (per-frame)");
    // Original MenuDLT::Update (exe 0x14009B6E0) does three things before tail-
    // jumping into Engine::DialogLoopT::Update:
    //   1. UI::SetCursor + Mod-update + audio pause/resume + audio update.
    //      We skip these for the standalone menu — UI uses the cursor we set in
    //      Startup, audio is silent, no mods.
    //   2. 3D camera animation: orbit yaw + FromLookAt → SetCameraToWorld.
    //      THIS we must do, otherwise the Camera stays at identity and the
    //      terrain projects to nothing visible (black background).
    //   3. Per-frame mesh/particle Update + scene Clean.
    //      Phase B/C — skipped (no static meshes / particles in Phase A).
    static int tick = 0;
    ++tick;
    if (tick <= 4 || (tick % 300) == 0) {
        printf("[MenuDLT::Update] self=%p time=%p tick=%d -> UpdateCamera + DialogLoopT::Update\n",
               (void*)self, time, tick);
    }

    // Retail step 1 also ticks the menu-side Save::SaveLoad (+0xb70, created in
    // Stub_Startup): `if (*(self+0xb70)) SaveLoad::Update(*(self+0xb70), time)`
    // at 0x14009b71f. Without this the menu Load button's list-scan and load
    // operations never progress. (Save/load wave 2026-08-27.)
    {
        void* menuSaveLoad = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(self) + 0xb70);
        if (menuSaveLoad && fn_SaveLoad_Update)
            fn_SaveLoad_Update(menuSaveLoad, time);
    }

    // Auto-start driver (env-var gated): once the menu has settled, inject the
    // New Game + Start button clicks so the standalone walks straight into a
    // new game without manual clicking. No-op unless BANISHED_AUTOSTART is set.
    // Runs before the DialogLoopT::Update tail-call below so the injected click
    // is processed in the same frame, and its queued next-loop is picked up by
    // the Stage C poll on a subsequent tick.
    AutoStart::TickMenu(self);

    // Wave 10B lane SHOW: arm the MainLoop+0x90 nested-pump callback (no-op
    // unless BANISHED_MODAL_DRIVER is set). Armed here because this is the
    // earliest per-frame tick at which Engine::MainLoop::_singleton is live;
    // once appended the callback stays in MainLoop's array for the life of the
    // process, so a single arming covers in-game modals too. See
    // standalone/modal_driver.cpp.
    if (ModalDriver::Enabled())
        ModalDriver::Poll();

    // Clamp the first tick's dt to ~16ms. Stub_Startup blocks for ~540ms while
    // LoadScene_PhaseA runs synchronously on the main thread; the first
    // Update tick post-Startup gets all that accumulated time as `dt`. If we
    // forward it as-is to fn_DialogLoopT_Update, fn_UI_Update advances the
    // fade alpha by `dt * rate * 4 = 0.54 * -1.0 * 4 = -2.16` in a single
    // tick, which clamps to alpha=0 immediately and skips the entire fade-in
    // animation — the menu pops in instantly instead of revealing smoothly
    // (visible as a flash). One-frame clamp is enough; subsequent ticks have
    // normal small dt because the load is done.
    int64_t origTicks = 0;
    bool clamped = false;
    if (tick <= 1 && time && g_Timer_ticksPerSecond && *g_Timer_ticksPerSecond > 0) {
        Time* tm = reinterpret_cast<Time*>(time);
        origTicks = tm->ticks;
        int64_t maxTicks = *g_Timer_ticksPerSecond / 60;  // ~16.67ms
        if (origTicks > maxTicks) {
            tm->ticks = maxTicks;
            clamped = true;
            printf("[MenuDLT::Update] tick=%d dt clamped from %lld to %lld "
                   "(%.2fms) — was post-LoadScene_PhaseA stall accumulator\n",
                   tick, (long long)origTicks, (long long)maxTicks,
                   (double)maxTicks * 1000.0 / (double)*g_Timer_ticksPerSecond);
        }
    }

    // Run the per-frame camera derivation (orbit + FromLookAt + SetCameraToWorld).
    if (time) {
        float dt = reinterpret_cast<const System::Time*>(time)->AsSeconds();   // B.3.4 bridge
        MenuDialogLoopT_UpdateCamera_PhaseA(self, dt);
    }

    // Per-frame particle simulation tick (chimney smoke, etc). Without this
    // particles are added to the scene at Load but never simulate — emitters
    // don't spawn sprites, so smokestacks render empty.
    if (time) {
        MenuDialogLoopT_TickParticles(self, time);
    }

    // Per-frame Audio::Scene::Update — advances the menu music stream (and any
    // ambient/3D channels) on the loop's embedded Audio::Scene (self+0xb30).
    // Mirrors MenuDLT::Update (exe 0x14009b6e0). Must run BEFORE the
    // Audio::Device::Update channel reaper below, matching retail frame order.
    // Without this, StartMusic's PlayEffect starts the music but it never
    // progresses (no new sample buffers get queued).
    if (time) {
        MenuDialogLoopT_TickAudioScene(self, time);
    }

    // Per-frame Audio::Device::Update. Original MainLoop::Run only fires
    // this once at startup, but the function is just channel cleanup
    // (FreeChannelPlatform on done channels) — calling per-frame keeps
    // the platform channel pool fresh so any sound that gets played can
    // be reaped. Side task from project_standalone_roadmap.md ("Audio:
    // Audio::Device::Update per-frame is not called"). Null-guarded
    // inside the helper.
    if (time) {
        MenuDialogLoopT_TickAudio(time);
    }

    // Stage C entry: poll the pending-next-loop written by NewGameDialog::
    // StartGame's SetDialogLoaderNextLoop (MenuDLT+0xb68). Once the fullscreen
    // fade-to-black settles (UI fade RATE at self+0x130 == UI(+0x40)+0xF0 == 0.0),
    // advance the MainLoop to the queued loader chain (outer LoaderLoop ->
    // GameLoadDialogLoopT -> Engine::Loader thread -> GameUpdateLoop::Load).
    // Mirrors retail
    // MenuDialogLoopT::Update (exe 0x14009b6e0) and our own GameUpdateLoop::Update
    // transition (gameupdateloop.cpp:886). Without this the menu loop never
    // advances and we never reach the in-game world build. Done BEFORE the
    // DialogLoopT::Update tail-call, matching retail ordering (reads the fade
    // state the prior tick's UI::Update settled).
    {
        void* pendingLoop = *(void**)(reinterpret_cast<uintptr_t>(self) + 0xb68);
        float fadeRate    = *(float*)(reinterpret_cast<uintptr_t>(self) + 0x130);
        if (pendingLoop && fadeRate == 0.0f) {
            void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
            fprintf(stderr, "[MenuDLT::Update] STAGE C entry: pendingLoop=%p "
                    "fade settled (rate=0) -> MainLoop::SetNextLoop\n", pendingLoop);
            fflush(stderr);
            if (fn_MainLoop_SetNextLoop && mainLoop) {
                fn_MainLoop_SetNextLoop(mainLoop, pendingLoop);
            } else {
                fprintf(stderr, "[MenuDLT::Update] WARN: cannot advance "
                        "(fn_MainLoop_SetNextLoop=%p mainLoop=%p)\n",
                        (void*)fn_MainLoop_SetNextLoop, mainLoop);
                fflush(stderr);
            }
        }
    }

    if (!fn_DialogLoopT_Update) {
        printf("[MenuDLT::Update] fn_DialogLoopT_Update is NULL, returning false\n");
        return false;
    }
    // Wave 10D lane MOUSE: the menu half of the cursor route. The menu UI is
    // MenuDLT+0x38 (DialogInstance) -> +0x10 (UserInterface), the same walk
    // ModalDriver::RunSelfTest uses above. No-op unless BANISHED_MOUSE_INJECT.
    void* menuUI = nullptr;
    if (MouseInject::Enabled()) {
        void* menuDlg = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(self) + 0x38);
        if (menuDlg)
            menuUI = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(menuDlg) + 0x10);
        MouseInject::PreUpdate(menuUI);
    }
    bool result = fn_DialogLoopT_Update(self, time);
    if (menuUI) MouseInject::PostUpdate(menuUI);

    // Restore the original dt so MainLoop's frame accounting stays correct.
    if (clamped) {
        reinterpret_cast<Time*>(time)->ticks = origTicks;
    }
    return result;
}

void Stub_Draw(UpdateLoop* self, float alpha) {
    StallDebug::Tick("MenuDLT::Draw");
    // Frame-rate ceiling. The Runtime main loop (MainLoop::Frame) is uncapped —
    // it only SwitchToThread()s, so the menu otherwise spins one core to 100%.
    // Tick() at frame start paces us to (display refresh + headroom) FPS; it's a
    // no-op whenever an external limiter (vsync/G-Sync/RTSS) already holds the
    // rate at or below refresh. This is the per-frame chokepoint we own, since
    // the outer Frame loop lives in the Runtime DLL.
    FrameLimiter::Tick();
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
        printf("[MenuDLT::Draw] self=%p frameAlpha=%f mainLoop=%p ctx=%p ui=%p menuScene=%p\n",
               (void*)self, alpha, mainLoop, context, ui, menuScene);
        printf("[MenuDLT::Draw] fps=%.1f (cap=%.1f)\n",
               FrameLimiter::GetFps(), FrameLimiter::GetCapFps());
    }

    // Per-frame fade alpha trace — log first 5 + every 30th draw so the curve
    // is visible in standalone_debug.log without spamming. UI is at self+0x40
    // for MenuDLT (DialogLoopT subclass).
    {
        static int draws = 0;
        ++draws;
        if (draws <= 5 || (draws % 30) == 0) {
            UIFade::LogState("MenuDLT::Draw", ui);
        }
    }

    // Phase A: render the 3D menu scene before the UI overlay.
    if (context) {
        MenuDialogLoopT_DrawScene_PhaseA(self, alpha, context);
    }

    if (context) {
        reinterpret_cast<UI::UserInterface*>(ui)->Draw(*reinterpret_cast<Video::Context*>(context));  // B.3.6 bridge
    } else if (trace) {
        printf("[MenuDLT::Draw] WARN: skipping UI::Draw (fn_UI_Draw=%p ctx=%p)\n",
               (void*)fn_UI_Draw, context);
    }

    // Auto-trigger a RenderDoc capture on draw 10 if RenderDoc is attached.
    // By this point the camera is stable, terrain is uploaded, and the menu
    // UI is drawn. Capturing one such frame is enough to inspect the GPU
    // state (which RTV is bound, what gets drawn, depth/cull, etc.).
    if (RenderDocIntegration::IsAttached()) {
        if (draws == 10) RenderDocIntegration::TriggerCapture();
        RenderDocIntegration::OnFrame();
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
    // Game/MenuScene/SceneN.rsc) for the 3D menu background.
    //
    // This Stub_Load runs on the single Runtime Engine::Loader thread (kicked by
    // the Loading splash's Stub_Startup, as in retail). Only thread-safe work
    // goes here:
    // fn_DialogLoopT_Load is safe (it's a single-call resource load with internal
    // refcounting). MenuDialogLoopT_LoadScene_PhaseA is NOT — it iterates 15K+
    // calls to Graphics_Scene_AddGlobalObject which mutates global scene state in
    // a way that produced corrupted vtable reads on first Update tick when run on
    // a worker thread. The PhaseA scene allocation has been moved to Stub_Startup
    // (main thread, runs while screen is fully faded to black so the freeze is
    // invisible to the user — see Stub_Startup below).
    void* dialogExtBefore = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] self=%p m_dialogExternal(before)=%p\n", (void*)self, dialogExtBefore);
    if (!fn_DialogLoopT_Load) {
        printf("[MenuDLT::Load] fn_DialogLoopT_Load is NULL — cannot load dialog rsc\n");
        return;
    }
    fn_DialogLoopT_Load(self);
    void* dialogExtAfter = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x30);
    printf("[MenuDLT::Load] m_dialogExternal(after)=%p (PhaseA deferred to Startup)\n",
           dialogExtAfter);

    // Load the menu music (Audio/Music/Menu.rsc -> self+0xb88). Mirrors the
    // music-load block at the top of the original MenuDLT::Load (exe 0x14009a930).
    // Like fn_DialogLoopT_Load this is a plain refcounted ExternalFactory load,
    // safe to run on the Engine::Loader thread (no scene-graph mutation).
    // Playback is started later from Stub_Startup (main thread).
    MenuDialogLoopT_LoadMusic(self);
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

