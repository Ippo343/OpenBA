// standalone/reg_code_lane5.cpp
// Code-fn wave LANE 5 registration batch (docs/waves/lanes/code_lane5.txt).
//
// Call RegisterCodeLane5Batch() from standalone/address_registry.cpp alongside
// the other reg_*.cpp waves (one line, see the report).
//
// Of the 8 addresses in this lane, exactly ONE was a real gap (0x1400a40e0).
// The other seven are already covered — either by a reimpl wired directly at
// its call site, or by a standalone vtable that installs our port in the slot.
// Three of those are registered here purely so the resolve stops returning 0
// (they close a spurious census row and remove a latent EXEC-of-0); the
// remaining four are argued as leave/dead in the lane report and are NOT
// registered, because their only consumers are dead `static fn_*` variables
// whose correct fix is deleting the resolve, not filling it in.

#include "resolve.h"
#include "engine_funcs.h"                 // FindDialogByName_Impl (inline)
#include "Entity/entitylist_funcs.h"      // EntityList_Update
#include <cstdint>

// code/Game/Game/unreg_lane5_musicplayer.cpp
extern "C" void MusicPlayer_Update(void* musicPlayer);
// code/Game/Game/minimap_paint.cpp
extern "C" void Minimap_PaintTerrain(void* minimap, void* context, void* worldGeom,
                                     void* sunLight);

// FindDialogByName_Impl is an inline in engine_funcs.h; give it one out-of-line
// address so RegisterExeAddr has something stable to publish.
static void* Lane5_FindDialogByName(void* retval, void* nameStr) {
    return FindDialogByName_Impl(retval, nameStr);
}

extern "C" void RegisterCodeLane5Batch() {
    // ---------------------------------------------------------------------
    // 0x1400a40e0  MusicPlayer::Update  (LOAD-WINDOW)  — THE REAL GAP
    // ---------------------------------------------------------------------
    // GameUpdateLoop::Update step 7 null-guard-and-SKIPs this every frame
    // (gameupdateloop.cpp, `post_simulation:` block), so BACKGROUND MUSIC NEVER
    // PLAYS — this is the only writer of the music-player state and the only
    // caller of Audio::Scene::PlayEffect on the music channel. Retail's sole
    // caller is GameUpdateLoop::Update (0x140096574). Port + the full
    // reasoning (incl. why the "MusicTrackList is null" blocker no longer
    // holds, and the one deliberate deviation) is in
    // code/Game/Game/unreg_lane5_musicplayer.cpp.
    // The port self-gates on BANISHED_LIVE_MUSIC=1 — UNPROVEN, this lane may
    // not build or run — so with the gate off this registration is
    // behaviourally identical to leaving the address unregistered.
    RegisterExeAddr(0x1400a40e0, reinterpret_cast<uintptr_t>(&MusicPlayer_Update));

    // ---------------------------------------------------------------------
    // 0x1400a5140  ImageCapture::DrawCheck / minimap terrain paint (LOAD-WINDOW)
    // ---------------------------------------------------------------------
    // NO behaviour change: gameupdateloop.cpp:571 already does
    //   fn_ImageCapture_DrawCheck = ResolveAddr(Exe, 0x1400a5140);
    //   if (!fn_ImageCapture_DrawCheck) fn_ImageCapture_DrawCheck = &Minimap_PaintTerrain;
    // i.e. an explicit null-guard-and-FALLBACK onto the very function we
    // publish here. Registering just makes the resolve succeed with the same
    // pointer, and stops the address showing up as an unregistered census row.
    // (The Ghidra label "ModDialog_impl_5140" is a MIS-LABEL — see the note in
    // standalone/reg_mods.cpp. Retail caller is 0x1400960f5, GameWorld::Draw.)
    RegisterExeAddr(0x1400a5140, reinterpret_cast<uintptr_t>(&Minimap_PaintTerrain));

    // ---------------------------------------------------------------------
    // 0x1400c4090  EntityList::Update  (LOAD-WINDOW)
    // ---------------------------------------------------------------------
    // NO behaviour change: gameupdateloop.cpp calls our reimpl
    // EntityList_Update() DIRECTLY (line ~1255); the `fn_EntityList_Update`
    // pointer resolved at line 480 is never invoked — its only use is the
    // CHECK_FN TRACE at line 677, which currently prints a misleading
    // "is NULL!" warning. Retail's sole caller is FUN_140096380
    // (GameUpdateLoop::Update, 0x14009642b), which we replace wholesale.
    // Registered so the census row closes and so any future retail-shaped
    // caller lands on our reimpl instead of NotImplementedStub (returns 0).
    RegisterExeAddr(0x1400c4090, reinterpret_cast<uintptr_t>(&EntityList_Update));

    // ---------------------------------------------------------------------
    // 0x1400e1600  FindDialogByName / TSD_FindByString
    // ---------------------------------------------------------------------
    // NO behaviour change: fully reimplemented as the inline
    // FindDialogByName_Impl in engine_funcs.h, and both retail callers
    // (TutorialGoalDialog_OnRemove 0x1400deef2, TutorialSetupDialog_OnRemove
    // 0x1400e2630) are ported and call the Impl directly
    // (tutorialgoaldialog.cpp:181, tutorialsetupdialog.cpp:355). The two
    // `fn_*` pointers resolved from this address — fn_FindDialogByName
    // (engine_funcs.cpp:2887) and fn_TSD_FindByString (:4528) — have ZERO call
    // sites; they are what makes this address show up TWICE in the census.
    // Signature matches both typedefs exactly: (void* retval, void* nameStr).
    RegisterExeAddr(0x1400e1600, reinterpret_cast<uintptr_t>(&Lane5_FindDialogByName));

    // ---------------------------------------------------------------------
    // NOT registered, deliberately — see the lane report for the full argument:
    //   0x1400a2980  MapGenerator::Serialize   — our port is installed in the
    //                standalone vtable slot 0x14015eda8 by
    //                RegisterMapGeneratorVtable(); the address's only consumer
    //                is the dead `s_fn_OrigSerialize` in mapgenerator.cpp:51.
    //   0x1400a3630  MenuScene::Serialize      — same shape (vtable slot
    //                0x14015ee08); dead consumer `s_fn_Serialize_Orig`
    //                (menuscene.cpp:88). Our MenuScene_Serialize is a
    //                deliberately PARTIAL port; publishing the address would
    //                not change that and would imply a completeness we do not
    //                have.
    //   0x1400d6f00  CitizenBehavior::ProcessPaths     — both are called
    //   0x1400d70c0  CitizenBehavior::BuildPathGraph     DIRECTLY by our
    //                CitizenBehavior_Update (citizenbehavior.cpp:353/359). The
    //                `fn_CitizenBehavior_{ProcessPaths,BuildPathGraph}`
    //                pointers (engine_funcs.cpp:4843/4845) have ZERO call
    //                sites, and their typedefs DISAGREE with our ports'
    //                return types (uint64_t/void vs bool/void), so publishing
    //                them would plant a latent ABI mismatch for the sake of a
    //                pointer nobody reads. The right fix is deleting those two
    //                resolves.
}
