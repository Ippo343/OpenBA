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
// ENDGAME B.3.8 block LOOP: the hand-built table is gone. This file now holds
// the MEMBER bodies of the real class `MenuDialogLoopT`
// (code/Game/Game/MenuDialogLoopT.h), whose compiler vtable 0x14015ed00 serves
// (B38_RULES D5), plus the standalone resource-loading helpers they call.


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
#include "Game/MenuDialogLoopT.h"   // ENDGAME B.3.8 LOOP
#include "Object/exe_type.h"      // DEFINE_EXE_VTABLE
#include "autostart.h"
#include <cstdio>
#include <cstdint>
#include "system/Filename.h"   // -Iruntime/types
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Time
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "ui/Sprite.h"          // ENDGAME B.3.6 UI callers (K4)
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "engine/UpdateLoop.h"   // -Iruntime/types: ENDGAME B.3.7 Engine loop classes
#include "engine/MainLoop.h"     // -Iruntime/types: ENDGAME B.3.7 Engine::MainLoop
#include "graphics/Scene.h"     // ENDGAME B.3.7: MenuDLT::Shutdown (carry-over 11)
#include "save/SaveLoad.h"      // -Iruntime/types: ENDGAME B.3.7 callers (K5)
#include "audio/Audio.h"        // ENDGAME B.3.8 LOOP: ~MenuDialogLoopT runs ~Audio::Scene
#include <new>
#include <cstring>
#include <initializer_list>   // ENDGAME B.3.7: MenuDLT::Shutdown field list

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

// ============================================================================
// MenuDialogLoopT (vtable @ 0x14015ed00) — code/Game/Game/MenuDialogLoopT.h
// ============================================================================
// ENDGAME B.3.8 block LOOP: the old hand-built `g_menuDialogLoopVtable[13]`'s
// stub bodies, moved into the real class's members unchanged.

// 0x14009bb00 — was the inline `MenuDialogLoopT_Ctor` (engine_funcs.h), which
// the two unconverted construction sites (mainmenudialog.cpp, tutorialui.cpp)
// still use; this is the same body with the vtable store the compiler's.
MenuDialogLoopT::MenuDialogLoopT(const System::String& filename)
    : Engine::DialogLoopT(filename) {
    fn_Random_Default_Ctor(&_random);     // System::Random at +0x168
    fn_AudioScene_Ctor(_audioScene);      // Audio::Scene at +0xb30
    // Zero-init the MenuDialogLoopT-specific fields (+0xb68..+0xbf8).
    uint8_t* const s = reinterpret_cast<uint8_t*>(this);
    for (uintptr_t off = 0xb68; off <= 0xbf8; off += 8)
        *reinterpret_cast<uint64_t*>(s + off) = 0;
}

// 0x14001a310 (read from the disassembly): MemFree the data of the three
// arrays +0xbf0, +0xbe0, +0xbd0 (in that order) and zero each {data, size|cap},
// then ??1Scene@Audio@@ on +0xb30 (import 0x140151fb0), then the tail JMP to
// ??1DialogLoopT@Engine@@ (import 0x140152378) — the compiler's base chain
// (~UserInterface, ~String, ~UpdateLoop). The elements the arrays point at were
// deleted by Shutdown (0x14009b280), as in retail.
MenuDialogLoopT::~MenuDialogLoopT() {
    for (uint8_t* arr : {_sceneIdArray, _instanceArray, _particleArray}) {
        fn_MemFreeStatic(*reinterpret_cast<void**>(arr));   // MemFree(null) returns
        *reinterpret_cast<uint64_t*>(arr)     = 0;          // _data
        *reinterpret_cast<uint64_t*>(arr + 8) = 0;          // _size, _capacity
    }
    AudioScene()->~Scene();

    // STANDALONE DIVERGENCE (the one thing retail's 0x14001a310 does not do):
    // hand ~UpdateLoop a -1 bit so it does NOT run ExternalFactory::Clean for
    // the menu's bit. The menu is the first loop GameMain builds, so it owns
    // bit 0, the bit every standalone boot-time FindOrLoad is tagged with
    // (Dialog/SpriteSheet.rsc and its White sprite, the two splash .rsc files)
    // and MenuDialogLoopT::Startup's lazy loads carry the menu's bit too (the
    // six cursors, the nine UI SoundEffects: KeyMap+0x28..+0xc0). Retail loads
    // all of those in PlacementTool_Ctor from the INNER LoaderLoop's Load
    // (0x140001120), which then drops that loop's bit (_loadBit = -1) so they
    // stay resident for the life of the process. Our InnerLoaderLoop::Load does
    // not build the PlacementTool (PORT GAP), so a faithful Clean here would
    // evict resources whose raw pointers KeyMap still holds, and the game's
    // Hand cursor / fade sprite would dangle. Dropping the bit keeps them
    // resident exactly as the old skipped dtor did; the rest of the teardown
    // (UI, filename, arrays, Audio::Scene) is retail's. Remove when Inner Load
    // loads the PlacementTool resources under its own dropped bit.
    _loadBit = -1;
}

Engine::UpdateLoop* MenuDialogLoopT::DeletingDtor(unsigned flags) {
    printf("[MenuDLT::Destroy] self=%p flags=%d\n", (void*)this, (int)flags);
    // Retail 0x14001a2e0: ~MenuDialogLoopT (0x14001a310, below), MemFree on &1.
    this->~MenuDialogLoopT();
    if (flags & 1) {
        fn_MemFreeStatic(this);
    }
    return this;
}

void MenuDialogLoopT::Startup() {
    StallDebug::Phase startupPhase("MenuDLT::Stub_Startup");

    // Menu-side Save::SaveLoad (+0xb70) — retail MenuDLT keeps its own SaveLoad
    // and ticks it every frame (MenuDLT::Update 0x14009b71f calls the
    // ?Update@SaveLoad dispatch slot on *(self+0xb70)); the menu Load button's
    // list-scan/load operations can only progress through that tick. The ctor
    // also publishes Save::SaveLoad::_singleton, which SaveLoadDialog and the
    // autosave path read. (Save/load wave 2026-08-27.)
    {
        if (_saveLoad == nullptr) {
            typedef void* (*fn_slctor_t)(void* self);
            static fn_slctor_t s_slCtor = reinterpret_cast<fn_slctor_t>(
                ResolveExport("??0SaveLoad@Save@@QEAA@XZ"));
            void* mem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, 0x90, 4) : nullptr;
            if (mem && s_slCtor) {
                s_slCtor(mem);
                _saveLoad = mem;
                printf("[MenuDLT::Startup] menu SaveLoad created at +0xb70 = %p\n", mem);
            }
        }
    }

    void* dialogExt = _dialogExternal;
    printf("[MenuDLT::Startup] self=%p m_dialogExternal=%p\n",
           (void*)this, dialogExt);
    // Original MenuDLT::Startup (exe 0x14009B930) chains Engine::DialogLoopT::Startup
    // (Runtime 0x1800db2f0) which:
    //   1. Calls slot 12 CreateLoopDialog → MainMenuDialog_FromMenuDLT.
    //   2. Stores the InternalPtr at self+0x38 (_dialogInstance).
    //   3. Invokes the dialog's Slot4 (OnOpened/Setup).
    //   4. Pushes the IBP into the UI's pending-open dialog list.
    // M2 wiring (SplashDLT → Loader::Load → MenuDLT::Load → DialogLoopT::Load)
    // populates _dialogExternal before this fires, so DialogInstance::DialogInstance
    // and the UI list iteration both have valid data to read.
    if (!dialogExt) {
        printf("[MenuDLT::Startup] WARN: m_dialogExternal is NULL, skipping Startup "
               "(would crash in DialogInstance ctor)\n");
        return;
    }

    // Phase A: by the time MenuDLT activates, the Loading splash's
    // ChunkedSceneLoader has already run LoadSceneA1 + chunked PhaseB +
    // LoadSceneA3 (driven from GameLoadDialogLoopT::Startup / ::Update
    // across the splash's display period). MenuDLT::Startup
    // just confirms the work was done and falls back to synchronous
    // LoadScene_PhaseA recovery if some boot path skipped the splash chain
    // (e.g. direct MenuDLT entry from a debug harness).
    void* menuSceneExt = _menuScene;
    if (!menuSceneExt) {
        printf("[MenuDLT::Startup] WARN: scene fields empty (Loading splash "
               "didn't run ChunkedSceneLoader) — falling back to synchronous "
               "LoadScene_PhaseA\n");
        StallDebug::Phase phaseA("MenuDLT LoadScene_PhaseA fallback");
        bool sceneOk = MenuDialogLoopT_LoadScene_PhaseA(this);
        printf("[MenuDLT::Startup] fallback LoadScene_PhaseA returned %s\n",
               sceneOk ? "true" : "false");
    } else {
        printf("[MenuDLT::Startup] scene already populated by Loading "
               "splash ChunkedSceneLoader (menuScene=%p) — skipping "
               "LoadScene_PhaseA\n", menuSceneExt);
    }

    {
        StallDebug::Phase dialogPhase("MenuDLT DialogLoopT::Startup");
        // QUALIFIED (slot 1 is VIRTUAL on the base; this is the class's own Startup).
        Engine::DialogLoopT::Startup();
    }
    printf("[MenuDLT::Startup] DialogLoopT::Startup returned, m_dialogInstance=%p\n",
           _dialogInstance);

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

    void* ui = &_ui;
    void* keyMap = (g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    void* arrowCursor = keyMap
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + 0x28)
        : nullptr;
    if (arrowCursor) {
        _ui.SetCursor(Object::ExternalPtr<UI::Cursor>(reinterpret_cast<UI::Cursor*>(arrowCursor)));  // B.3.6 bridge
        printf("[MenuDLT::Startup] UI::SetCursor(ui=%p, arrow=%p)\n", ui, arrowCursor);
    } else {
        printf("[MenuDLT::Startup] WARN: skipping SetCursor (arrow=%p)\n", arrowCursor);
    }
    _ui.EnableCursor(true);
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
    // plays Audio/Music/Menu.rsc (loaded into self+0xb88 by Load) on the
    // music channel. Per-frame Audio::Scene::Update (Update) advances it.
    MenuDialogLoopT_StartMusic(this);

    // Bind the UI hover/press/click sounds onto the embedded UserInterface
    // (self+0x40), mirroring OptionsDialog_helper_9260's sound block. Must run
    // after StartMusic wired the audio scene at self+0xd0 (== UI+0x90) and
    // after EnsureUISoundsLoaded populated the KeyMap container. The element
    // framework (ButtonElement/Element::Update, driven by DialogLoopT::Update
    // in Update) then plays these automatically on hover/click.
    MenuDialogLoopT_BindUISounds(this);
}

void MenuDialogLoopT::Shutdown() {
    printf("[MenuDLT::Shutdown] self=%p\n", (void*)this);
    // ENDGAME B.3.7 CARRY-OVER 11: retail MenuDialogLoopT::Shutdown
    // (Application-x64.exe 0x14009b280), transcribed from its DISASSEMBLY, whole.
    // This stub used to run only step 11 (the StopAll) and skip the rest -- in
    // particular the call to `DialogLoopT::Shutdown`, which retail makes
    // DIRECTLY through import 0x1401523a0 (`?Shutdown@DialogLoopT@Engine@@`),
    // and which closes the menu dialog. Now that the base member is callable
    // it is called QUALIFIED (slot 2 is VIRTUAL on the base; this IS the
    // class's slot 2). Import names read from the exe's IAT with pefile:
    //   0x140151eb8 ?RemoveGlobalObject@Scene@Graphics@@   0x140151360 ?RemoveObject@Scene@Graphics@@
    //   0x140152b68 ?MemFree@MemoryAllocator@System@@       0x140151fa8 ??1Scene@Graphics@@
    //   0x1401521f0 ??1Renderer@Graphics@@                 0x140151f88 ??1SaveLoad@Save@@
    //   0x1401523a0 ?Shutdown@DialogLoopT@Engine@@          0x140151f98 ?StopAll@Scene@Audio@@
    //   0x140151bf8 ?EnableCursor@UserInterface@UI@@ (tail JMP)
    // Retail re-reads every field from `this` at each use and NEVER nulls one
    // after freeing it; both are reproduced. Unguarded where retail is.
    uint8_t* const base = reinterpret_cast<uint8_t*>(this);
    auto field = [base](uint32_t off) -> void* {
        return *reinterpret_cast<void**>(base + off);
    };
    auto scene = [&field]() {                        // MOV RCX,[RBX+0xba0], re-read
        return static_cast<Graphics::Scene*>(field(0xba0));
    };
    auto deleteObject = [](void* o) {                // MOV RAX,[RCX]; MOV EDX,1; CALL [RAX]
        reinterpret_cast<Object::Base*>(o)->DeletingDtor(1);
    };
    typedef void (*DtorFn)(void*);
    static DtorFn s_sceneDtor = reinterpret_cast<DtorFn>(
        ResolveExport("??1Scene@Graphics@@QEAA@XZ"));
    static DtorFn s_rendererDtor = reinterpret_cast<DtorFn>(
        ResolveExport("??1Renderer@Graphics@@QEAA@XZ"));
    static DtorFn s_saveLoadDtor = reinterpret_cast<DtorFn>(
        ResolveExport("??1SaveLoad@Save@@QEAA@XZ"));

    // 1. +0xbc0: the menu scene's first Geometry -- REMOVED, not owned (no delete).
    if (field(0xbc0))
        scene()->RemoveGlobalObject(static_cast<Graphics::Geometry*>(field(0xbc0)));
    // 2-4. +0xbb0 (SunLight), +0xba8 (main mesh), +0xbc8 (DLC ship): removed
    //      from the scene's global list, then deleted through slot 0.
    for (uint32_t off : {0xbb0u, 0xba8u, 0xbc8u}) {
        if (field(off)) {
            scene()->RemoveGlobalObject(static_cast<Graphics::Geometry*>(field(off)));
            if (void* o = field(off)) deleteObject(o);
        }
    }
    // 5. every scene-grid instance: RemoveObject(id[i]) then delete object[i].
    //    Count from +0xbe8, ids from +0xbf0 (uint32), objects from +0xbe0.
    for (int32_t i = 0; i < *reinterpret_cast<int32_t*>(base + 0xbe8); ++i) {
        const uint32_t id = static_cast<const uint32_t*>(field(0xbf0))[i];
        scene()->RemoveObject(id);
        if (void* o = static_cast<void**>(field(0xbe0))[i]) deleteObject(o);
    }
    // 6. +0xbb8 (the Transform block): freed.
    if (void* t = field(0xbb8)) fn_MemFreeStatic(t);
    // 7. +0xba0 Graphics::Scene: ~Scene then freed.
    if (void* g = field(0xba0)) { s_sceneDtor(g); fn_MemFreeStatic(g); }
    // 8. +0xb98 Graphics::Renderer: ~Renderer then freed.
    if (void* r = field(0xb98)) { s_rendererDtor(r); fn_MemFreeStatic(r); }
    // 9. +0xb70 the menu's Save::SaveLoad: ~SaveLoad then freed.
    if (void* sl = field(0xb70)) { s_saveLoadDtor(sl); fn_MemFreeStatic(sl); }
    // 10. `CALL [0x1401523a0]` -- Engine::DialogLoopT::Shutdown, QUALIFIED.
    Engine::DialogLoopT::Shutdown();
    // 11. Audio::Scene::StopAll(this+0xb30). NOT skippable: it parks the
    //     streamed menu music's thread before the next LoaderLoop's
    //     Loader::Load remounts the packages under it (open defect (c)).
    MenuDialogLoopT_StopMusic(this);
    // 12. The tail `JMP UI::UserInterface::EnableCursor(this+0x40, false)`.
    _ui.EnableCursor(false);
}

bool MenuDialogLoopT::Update(const System::Time& timeRef) {
    StallDebug::Tick("MenuDLT::Update");
    // The first-tick clamp below writes the caller's Time and restores it
    // before returning (MainLoop owns it); retail's slot takes it by const&.
    void* time = const_cast<System::Time*>(&timeRef);   // B.3.8 bridge
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
               (void*)this, time, tick);
    }

    // Retail step 1 also ticks the menu-side Save::SaveLoad (+0xb70, created in
    // Startup): `if (*(self+0xb70)) SaveLoad::Update(*(self+0xb70), time)`
    // at 0x14009b71f. Without this the menu Load button's list-scan and load
    // operations never progress. (Save/load wave 2026-08-27.)
    {
        void* menuSaveLoad = _saveLoad;
        if (menuSaveLoad)
            reinterpret_cast<Save::SaveLoad*>(menuSaveLoad)->Update(  // B.3.7 bridge
                *reinterpret_cast<const System::Time*>(time));
    }

    // Auto-start driver (env-var gated): once the menu has settled, inject the
    // New Game + Start button clicks so the standalone walks straight into a
    // new game without manual clicking. No-op unless BANISHED_AUTOSTART is set.
    // Runs before the DialogLoopT::Update tail-call below so the injected click
    // is processed in the same frame, and its queued next-loop is picked up by
    // the Stage C poll on a subsequent tick.
    AutoStart::TickMenu(this);

    // Wave 10B lane SHOW: arm the MainLoop+0x90 nested-pump callback (no-op
    // unless BANISHED_MODAL_DRIVER is set). Armed here because this is the
    // earliest per-frame tick at which Engine::MainLoop::_singleton is live;
    // once appended the callback stays in MainLoop's array for the life of the
    // process, so a single arming covers in-game modals too. See
    // standalone/modal_driver.cpp.
    if (ModalDriver::Enabled())
        ModalDriver::Poll();

    // Clamp the first tick's dt to ~16ms. Startup blocks for ~540ms while
    // LoadScene_PhaseA runs synchronously on the main thread; the first
    // Update tick post-Startup gets all that accumulated time as `dt`. If we
    // forward it as-is to DialogLoopT::Update, UI::Update advances the
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
        MenuDialogLoopT_UpdateCamera_PhaseA(this, dt);
    }

    // Per-frame particle simulation tick (chimney smoke, etc). Without this
    // particles are added to the scene at Load but never simulate — emitters
    // don't spawn sprites, so smokestacks render empty.
    if (time) {
        MenuDialogLoopT_TickParticles(this, time);
    }

    // Per-frame Audio::Scene::Update — advances the menu music stream (and any
    // ambient/3D channels) on the loop's embedded Audio::Scene (self+0xb30).
    // Mirrors MenuDLT::Update (exe 0x14009b6e0). Must run BEFORE the
    // Audio::Device::Update channel reaper below, matching retail frame order.
    // Without this, StartMusic's PlayEffect starts the music but it never
    // progresses (no new sample buffers get queued).
    if (time) {
        MenuDialogLoopT_TickAudioScene(this, time);
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
        void* pendingLoop = _pendingLoop;
        float fadeRate    = *(float*)(reinterpret_cast<uintptr_t>(this) + 0x130);
        if (pendingLoop && fadeRate == 0.0f) {
            void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
            fprintf(stderr, "[MenuDLT::Update] STAGE C entry: pendingLoop=%p "
                    "fade settled (rate=0) -> MainLoop::SetNextLoop\n", pendingLoop);
            fflush(stderr);
            if (mainLoop) {
                reinterpret_cast<Engine::MainLoop*>(mainLoop)->SetNextLoop(  // B.3.7 bridge
                    reinterpret_cast<Engine::UpdateLoop*>(pendingLoop));
            } else {
                fprintf(stderr, "[MenuDLT::Update] WARN: cannot advance "
                        "(mainLoop=%p)\n", mainLoop);
                fflush(stderr);
            }
        }
    }

    // Wave 10D lane MOUSE: the menu half of the cursor route. The menu UI is
    // MenuDLT+0x38 (DialogInstance) -> +0x10 (UserInterface), the same walk
    // ModalDriver::RunSelfTest uses above. No-op unless BANISHED_MOUSE_INJECT.
    void* menuUI = nullptr;
    if (MouseInject::Enabled()) {
        void* menuDlg = _dialogInstance;
        if (menuDlg)
            menuUI = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(menuDlg) + 0x10);
        MouseInject::PreUpdate(menuUI);
    }
    // QUALIFIED (slot 3 is VIRTUAL on the base; this is the class's own Update).
    bool result = Engine::DialogLoopT::Update(*reinterpret_cast<const System::Time*>(time));
    if (menuUI) MouseInject::PostUpdate(menuUI);

    // Restore the original dt so MainLoop's frame accounting stays correct.
    if (clamped) {
        reinterpret_cast<Time*>(time)->ticks = origTicks;
    }
    return result;
}

void MenuDialogLoopT::Draw(float alpha) {
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
    // unchanged for the run.
    void* mainLoop = (g_pMainLoopSingleton ? *g_pMainLoopSingleton : nullptr);
    void* context = mainLoop
        ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mainLoop) + 0x10)
        : nullptr;
    void* ui = &_ui;

    if (trace) {
        void* menuScene = _menuScene;
        printf("[MenuDLT::Draw] self=%p frameAlpha=%f mainLoop=%p ctx=%p ui=%p menuScene=%p\n",
               (void*)this, alpha, mainLoop, context, ui, menuScene);
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
        MenuDialogLoopT_DrawScene_PhaseA(this, alpha, context);
    }

    if (context) {
        _ui.Draw(*reinterpret_cast<Video::Context*>(context));  // B.3.6 bridge
    } else if (trace) {
        printf("[MenuDLT::Draw] WARN: skipping UI::Draw (ctx=%p)\n", context);
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

void MenuDialogLoopT::Load() {
    // Original MenuDLT::Load (exe 0x14009A930) tail-calls Engine::DialogLoopT::Load
    // first to populate self+0x30 (_dialogExternal) from _dialogFilename
    // ("Dialog\MainMenu.rsc"), then loads companion resources (Audio/Music/Menu.rsc,
    // Game/MenuScene/SceneN.rsc) for the 3D menu background.
    //
    // This runs on the single Runtime Engine::Loader thread (kicked by
    // the Loading splash's Startup, as in retail). Only thread-safe work
    // goes here:
    // DialogLoopT::Load is safe (it's a single-call resource load with internal
    // refcounting). MenuDialogLoopT_LoadScene_PhaseA is NOT — it iterates 15K+
    // calls to Graphics_Scene_AddGlobalObject which mutates global scene state in
    // a way that produced corrupted vtable reads on first Update tick when run on
    // a worker thread. The PhaseA scene allocation has been moved to Startup
    // (main thread, runs while screen is fully faded to black so the freeze is
    // invisible to the user — see Startup above).
    void* dialogExtBefore = _dialogExternal;
    printf("[MenuDLT::Load] self=%p m_dialogExternal(before)=%p\n", (void*)this, dialogExtBefore);
    // QUALIFIED (slot 6 is VIRTUAL on the base; this is the class's own Load,
    // dispatched by Loader::Run on the loader thread).
    Engine::DialogLoopT::Load();
    void* dialogExtAfter = _dialogExternal;
    printf("[MenuDLT::Load] m_dialogExternal(after)=%p (PhaseA deferred to Startup)\n",
           dialogExtAfter);

    // Load the menu music (Audio/Music/Menu.rsc -> self+0xb88). Mirrors the
    // music-load block at the top of the original MenuDLT::Load (exe 0x14009a930).
    // Like DialogLoopT::Load this is a plain refcounted ExternalFactory load,
    // safe to run on the Engine::Loader thread (no scene-graph mutation).
    // Playback is started later from Startup (main thread).
    MenuDialogLoopT_LoadMusic(this);
}

void MenuDialogLoopT::OnVideoError() {
    printf("[MenuDLT::OnVideoError] self=%p\n", (void*)this);
}

void MenuDialogLoopT::OnVideoRestart() {
    printf("[MenuDLT::OnVideoRestart] self=%p\n", (void*)this);
}

const char* MenuDialogLoopT::GetResourceFilename() {
    // Original exe slot 10 returns the literal "Game/Menu" via LEA RAX,[rip+...].
    return "Game/Menu";
}

bool MenuDialogLoopT::AllowsModChanges() {
    // Original exe slot 11 = MOV AL,1; RET — i.e. always true.
    return true;
}

extern "C" void* MainMenuDialog_FromMenuDLT(void* loop, void* retInternalBasePtr);

Object::InternalPtr<UI::DialogInstance>* MenuDialogLoopT::CreateLoopDialog(
        Object::InternalPtr<UI::DialogInstance>* retPtr) {
    printf("[MenuDLT::CreateDialog] self=%p retPtr=%p rsc=%p -> MainMenuDialog_FromMenuDLT\n",
           (void*)this, (void*)retPtr, _dialogExternal);
    // M2 wired Loader::Load → MenuDLT::Load → DialogLoopT::Load, so self+0x30
    // (_dialogExternal) is populated with Dialog/MainMenu.rsc by the time
    // DialogLoopT::Startup invokes this slot. Delegate to the standalone
    // MainMenuDialog factory which mirrors the exe ctor at 0x14009BA30:
    // builds the DialogInstance + patches our 20-slot vtable + writes the
    // resulting IBP into retPtr.
    return static_cast<Object::InternalPtr<UI::DialogInstance>*>(
        MainMenuDialog_FromMenuDLT(this, retPtr));
}

// B38_RULES D5: `Vtable()` is the COMPILER's table; 0x14015ed00 keeps serving
// it while the inline `MenuDialogLoopT_Ctor` (engine_funcs.h, used by
// mainmenudialog.cpp and tutorialui.cpp) stamps g_MenuDialogLoopT_Vtable.
DEFINE_EXE_VTABLE(MenuDialogLoopT, "_ZTV15MenuDialogLoopT")

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterMenuDialogLoopVtable() {
    RegisterExeAddr(0x14015ed00, reinterpret_cast<uintptr_t>(MenuDialogLoopT::Vtable()));
}
