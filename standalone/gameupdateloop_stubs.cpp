// standalone/gameupdateloop_stubs.cpp
// Standalone-only vtable for the exe's GameUpdateLoop (vtable @ 0x14015e2f0,
// the in-game simulation loop). Built when the NewGame "OK" button starts a
// game: NewGameDialog::StartGame allocates a GameUpdateLoop (0x160 bytes) and
// writes g_GameUpdateLoop_Vtable onto it. In standalone, 0x14015e2f0 was
// previously UNREGISTERED, so g_GameUpdateLoop_Vtable resolved to NULL and the
// loop got a NULL vptr — hardfaulting the moment MainLoop dispatched a virtual.
//
// GameUpdateLoop has the standard 12 UpdateLoop virtuals (no CreateDialog):
//   +0x00  Destroy(self, flags)            — exe 0x140019dd0  (REIMPL'd)
//   +0x08  Startup(self)                   — exe 0x140097c00  (REIMPL'd)
//   +0x10  Shutdown(self)                  — exe 0x1400977c0  (REIMPL'd)
//   +0x18  Update(self, time)              — exe 0x140096380  (REIMPL'd)
//   +0x20  Draw(self, alpha)               — exe 0x140096010  (REIMPL'd)
//   +0x28  DrawFailed(self)                — exe 0x140096000  (bare RET 0 — no-op)
//   +0x30  Load(self)                      — exe 0x140096b00  (STAGE C cliff — stub)
//   +0x38  SaveRestore(self, save, state, out) — exe 0x1400959c0 (save/load — stub)
//   +0x40  OnVideoError(self)              — exe 0x140095970  (device-lost — stub)
//   +0x48  OnVideoRestart(self)            — exe 0x140095930  (device-restored — stub)
//   +0x50  GetResourceFilename(self)       — exe 0x140019d40  (returns "Game/Game")
//   +0x58  AllowsModChanges(self)          — exe 0x14003a8f4  (Runtime thunk -> true)
//
// Slots 0-4 route to the existing clean reimpls in
// code/Game/Game/gameupdateloop.cpp. The six remaining slots are filled here:
//   - GetResourceFilename is LOAD-BEARING (the loader reads its return value to
//     know which resource group to load) and must return exactly "Game/Game".
//   - AllowsModChanges returns true (matches the Runtime thunk).
//   - DrawFailed/SaveRestore/OnVideoError/OnVideoRestart are verified-safe
//     no-ops on the new-game path (Ghidra recon 2026-05-30): DrawFailed is a
//     bare RET; SaveRestore only runs when restoring a .sav; the two video
//     handlers only fire on a D3D device-lost/restored event. They will need
//     real bodies before save/load (M5+) and live device-reset support.
//   - Load (+0x30) is the Stage-C cliff (builds ~24 world subsystems). For
//     Stage B it is a logging stub: this confirms the New Game transition
//     reaches the in-game world bring-up without faulting in the loop-
//     construction / SetNextLoop machinery. The full Load reimpl is Stage C.


#include "address_registry.h"
#include "resolve.h"
#include "engine_funcs.h"
#include <cstdio>
#include <cstdint>
#include <cstdlib>

// code/Game/Component/component_factory_base.cpp — central live-feature gate
// (default-on table + env kill-switch). Fwd-declared to avoid the include path.

// Clean reimpls in code/Game/Game/gameupdateloop.cpp. Declared extern "C" there;
// void* self here is ABI-identical to the real GameUpdateLoop* and avoids
// pulling in the type header.
extern "C" {
    void* GameUpdateLoop_Destroy(void* self, uint32_t flags);
    void  GameUpdateLoop_Startup(void* self);
    void  GameUpdateLoop_Shutdown(void* self);
    bool  GameUpdateLoop_Update(void* self, void* time);
    void  GameUpdateLoop_Draw(void* self, float alpha);
    // +0x30 Load — the M5 world build (code/Game/Game/gameupdateloop_load.cpp).
    void  GameUpdateLoop_Load(void* self);
    // +0x38 SaveRestore — code/Game/Game/gameupdateloop_serialize.cpp.
    void  GameUpdateLoop_SaveRestore(void* self, char save, void* savedState, int* outParam);
}

namespace {

// +0x28 DrawFailed — original is a literal `RET 0` (no body). Only invoked when
// Draw fails (video lost). 1:1 no-op.
void GUL_DrawFailed(void* self) {
    (void)self;
}

// +0x30 Load — the M5 world build. Now routes to the real reimpl in
// code/Game/Game/gameupdateloop_load.cpp (GameUpdateLoop_Load), which builds
// the in-game subsystems (Random, camera, Renderer, EntityList, Game, Weather,
// WorldGeometry, SunLight, the Graphics/Audio/Picking scenes, AnimationManager,
// SaveLoad). First slice defers the 8 resource Finds, the SkyMesh MeshInstance,
// and the deep worldgen — see project_gameupdateloop_load_map. The
// GameLoadDialogLoopT AsyncLoader worker dispatches this slot on a background thread.

// +0x38 SaveRestore(self, saveFlag, savedState, outParam) — serializes loop
// state to/from a Save::SavedState. REAL since the save/load wave 2026-08-27:
// routes to the ported 0x1400959c0 (code/Game/Game/gameupdateloop_serialize.cpp),
// which wraps the master serializer 0x140095a90 in a CompressedMemoryStream and
// Store/Retrieves the SavedState. Invoked by Save::SaveLoad::UpdateSave (save=1)
// and by GameUpdateLoop::Load's restore branch (save=0).
void GUL_SaveRestore(void* self, char saveFlag, void* savedState, int* outParam) {
    GameUpdateLoop_SaveRestore(self, saveFlag, savedState, outParam);
}

// +0x40 OnVideoError — releases GPU buffers on a D3D device-lost event.
// Never fires on the load path or a normal frame. Safe no-op.
void GUL_OnVideoError(void* self) {
    (void)self;
}

// +0x48 OnVideoRestart — recreates GPU buffers after device-restored. Pairs
// with OnVideoError; never fires on the load path. Safe no-op.
void GUL_OnVideoRestart(void* self) {
    (void)self;
}

// +0x50 GetResourceFilename — LOAD-BEARING getter. The loader/resource machinery
// reads this to know which resource group/package to load for the in-game loop.
// Original (exe 0x140019d40) returns the static string "Game/Game". Must return
// exactly this — returning NULL/empty makes the loader fail to find game data.
const char* GUL_GetResourceFilename(void* self) {
    (void)self;
    // The engine's UpdateLoop::LoadResources (Runtime 0x1800dd7d0) calls this slot
    // and, if it returns non-null, FindOrLoads "<ret>Resources.rsc" + its ENTIRE
    // recursive dependency tree (the master ExternalList of all game
    // ComponentDescriptions). In standalone that recursive load is NON-TERMINATING
    // (no FindOrLoad dedup, tied to the disabled Modification::SetupFileSystem) and
    // holds the FOL serialize lock forever on the Engine::Loader thread, permanently
    // starving the main-thread world build — the in-game load never completes.
    //
    // DEFAULT: return null so LoadResources skips that block and the loader
    // terminates immediately. Our GameUpdateLoop_Load loads what the world build
    // needs on-demand, so the eager preload is redundant for first-light — with it
    // skipped the world builds (~1480 entities) and GameUpdateLoop::Update ticks
    // per-frame (verified 2026-06-17). The preload graduated to always-on
    // 2026-09-13 (it had been default-on via the default-on table since 2026-07-19).
    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        printf("[GUL::GetResourceFilename] preload ON -> returning \"Game/Game\" "
               "(engine will recursively preload the HUD/dialog/toolbar resource tree)\n");
        fflush(stdout);
    }
    return "Game/Game";
}

// +0x58 AllowsModChanges — original thunks to the Runtime impl which returns
// true (the loop permits mod configuration). Side-effect-free getter.
bool GUL_AllowsModChanges(void* self) {
    (void)self;
    return true;
}

} // namespace

// 12-slot vtable matching the exe layout at 0x14015e2f0 (byte-verified).
void* g_GameUpdateLoop_Vtable_Standalone[12] = {
    reinterpret_cast<void*>(&GameUpdateLoop_Destroy),       // +0x00
    reinterpret_cast<void*>(&GameUpdateLoop_Startup),       // +0x08
    reinterpret_cast<void*>(&GameUpdateLoop_Shutdown),      // +0x10
    reinterpret_cast<void*>(&GameUpdateLoop_Update),        // +0x18
    reinterpret_cast<void*>(&GameUpdateLoop_Draw),          // +0x20
    reinterpret_cast<void*>(&GUL_DrawFailed),               // +0x28
    reinterpret_cast<void*>(&GameUpdateLoop_Load),          // +0x30 (real M5 world build)
    reinterpret_cast<void*>(&GUL_SaveRestore),              // +0x38
    reinterpret_cast<void*>(&GUL_OnVideoError),             // +0x40
    reinterpret_cast<void*>(&GUL_OnVideoRestart),           // +0x48
    reinterpret_cast<void*>(&GUL_GetResourceFilename),      // +0x50
    reinterpret_cast<void*>(&GUL_AllowsModChanges),         // +0x58
};

// Called from address_registry.cpp's RegisterVtablesAndTypeIds().
void RegisterGameUpdateLoopVtable() {
    RegisterExeAddr(0x14015e2f0,
                    reinterpret_cast<uintptr_t>(g_GameUpdateLoop_Vtable_Standalone));
}

