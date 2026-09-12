// ============================================================================
// runtime/src/save/rt_save_port.cpp — RtSavePortInit()   Wave 5 lane C
// ============================================================================
//   BANISHED_RT_SAVE=0       leave the whole Save namespace on the DLL
//   BANISHED_RT_SAVELOAD=0   ... just Save::SaveLoad + Save::SavedState
//   BANISHED_RT_SETUPLOAD=0  ... just Wave 14 Save::SavedState::SetupLoad
//   BANISHED_RT_SAVE_METADATA=0     ... just Wave 14's SaveMetaData.
//   BANISHED_RT_LOAD_METADATA=0     ... just Wave 14's LoadMetaData
//   BANISHED_RT_SAVELOAD_UPDATE=0  ... just Wave 13's six state machines,
//                            leaving Wave 5's 16 exports ported. This split
//                            is not decoration: it is the A/B that localised
//                            Wave 13's first defect to this file in one run,
//                            by showing the save completing in 61 frames with
//                            Wave 5 on and Wave 13 off -- the same 61 frames
//                            the all-DLL control took.
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include "../../funcs/rt_port.h"
#include "saveload_internal.h"

// ============================================================================
// BANISHED_RT_EARLY_TOUCH=1 — the SUBJECT the pre-init slot audit needs
// (Wave 15 lane ORDER)
// ============================================================================
//
// rt_resolve_cache.cpp §6 stops a pre-init resolve from freezing. Proving that
// mattered needs a run in which some name's answer ACTUALLY CHANGES across
// RtDataMintInit(), and on an ordinary run nothing in this family resolves that
// early: every accessor is lazy and every eager consumer (AddressRegistryInit,
// EngineFuncsInit, the lane-X probe) is sequenced AFTER the mint on purpose.
// So the audit's effectiveness number is 0 on a default run — which says the
// rule had nothing to bite on, not that it works
// (reference_probe_needs_an_effectiveness_control.md).
//
// This switch supplies the subject rather than a fake one
// (reference_synthetic_subject_beats_a_fake.md): it calls the REAL accessors,
// through the REAL door, from a point that genuinely precedes the mint
// (RtSavePortInit runs inside RuntimePortInit, well before RtDataMintInit —
// resolve_standalone.cpp:324 vs :406). The subject is chosen because it is one
// the compared set can compare: `?_singleton@SaveLoad@Save@@` is PROMOTED on
// every default run, so its answer moves from the DLL's `.data` to our minted
// storage across init (reference_control_must_pick_a_compared_subject.md).
//
// The A/B is then two runs with this on:
//   BANISHED_RT_FREEZE_EARLY unset -> [RtSlotAudit] ... REPAIRED, 0 SPLIT
//   BANISHED_RT_FREEZE_EARLY=1     -> [RtSlotAudit] ... SPLIT,   1 SPLIT   (RED)
//
// It is off by default and does nothing but read four slots and discard the
// values, so an armed run differs from a normal one only in the ORDER a resolve
// happens — which is the whole subject.
// ============================================================================
static void RtSaveEarlyTouchProbe() {
    const char* e = std::getenv("BANISHED_RT_EARLY_TOUCH");
    if (!(e && *e && std::strcmp(e, "0") != 0 && std::strcmp(e, "false") != 0)) return;
    printf("[RtEarlyTouch] resolving the four Save-lane singleton slots BEFORE "
           "RtDataMintInit(), on purpose. portInitDone=%d\n",
           RtResolvePortInitDone() ? 1 : 0);
    void* v[4] = { (void*)RtSaveDetail::SaveLoadSingletonSlot(),
                   (void*)RtSaveDetail::FileSystemSingleton(),
                   (void*)RtSaveDetail::ModificationSingleton(),
                   (void*)RtSaveDetail::PersistenceSingleton() };
    printf("[RtEarlyTouch] SaveLoad slot=%p  fs=%p mod=%p pers=%p  "
           "(preInitHits=%llu preInitFrozen=%llu)\n",
           v[0], v[1], v[2], v[3],
           (unsigned long long)RtResolvePreInitHitCount(),
           (unsigned long long)RtResolvePreInitFrozenCount());
    fflush(stdout);
}

extern int RtSaveLoadRegister();       // saveload.cpp (Wave 5 lane C)
extern int RtSaveLoadUpdateRegister(); // saveload_update.cpp (Wave 13)
extern int RtSaveSetupLoadRegister();  // savedstate_setupload.cpp (Wave 14)
extern int RtSaveSaveMetaDataRegister();// saveload_meta.cpp   (Wave 14, lane SAVEMETA)
extern int RtSaveLoadMetaDataRegister();// saveload_loadmeta.cpp (Wave 14, lane LOADMETA)
extern int RtSaveLoadDrawRegister();   // saveload_draw.cpp (Wave 14 lane DRAW)
// Wave 24 lane INTERNAL8. The two UNEXPORTED bodies UpdateList reaches by
// address: the qsort comparator (0x1800c3db0) and SaveInfo::SetThumbnail
// (0x1800c3de0). Own switch, nested under SAVELOAD for the same reason DRAW
// is: SetThumbnail is the only one of the two that touches the GPU, and the
// comparator decides the ORDER the player sees in the save list, so "the list
// is in the wrong order" and "the thumbnail is wrong" must be separable from
// Wave 13's UpdateList itself.
extern int RtSaveInfoRegister();       // saveload_saveinfo.cpp (Wave 24 INTERNAL8)

void RtSavePortInit() {
    RtSaveEarlyTouchProbe();   // off unless BANISHED_RT_EARLY_TOUCH=1
    if (!RtWaveEnabled("BANISHED_RT_SAVE")) {
        printf("[RtPort] BANISHED_RT_SAVE=0 -- Save namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_SAVELOAD")) {
        n += RtSaveLoadRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVELOAD_UPDATE")) n += RtSaveLoadUpdateRegister();
        // Wave 14: ?SetupLoad@SavedState@Save@@. Gated with Wave 5's block
        // rather than with Wave 13's, because its three callers are Wave 13
        // bodies and a SavedState method is Wave 5's subject. Its OWN kill
        // switch exists so a crash anywhere on the save path can be A/B'd
        // against this one registration without taking Wave 13 out with it.
        if (RtWaveEnabled("BANISHED_RT_SETUPLOAD")) n += RtSaveSetupLoadRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVE_METADATA"))  n += RtSaveSaveMetaDataRegister();
        // Wave 14 lane LOADMETA. Own kill switch: LoadMetaData is the callee
        // UpdateList state 4 suppresses under the predictive oracle, so leaving
        // it on the DLL while Wave 13 stays ported is a real A/B, not a
        // hypothetical one.
        if (RtWaveEnabled("BANISHED_RT_LOAD_METADATA"))  n += RtSaveLoadMetaDataRegister();
        // Wave 14 lane DRAW gets its own switch for the same A/B reason the
        // UPDATE switch exists: Draw is the only Save body that touches the
        // GPU, so "the save hangs" and "the thumbnail is wrong" have to be
        // separable from Wave 5's and Wave 13's bodies in one run.
        if (RtWaveEnabled("BANISHED_RT_SAVELOAD_DRAW")) n += RtSaveLoadDrawRegister();
        if (RtWaveEnabled("BANISHED_RT_SAVEINFO")) n += RtSaveInfoRegister();
    }
    printf("[RtPort] Save: %d exports registered\n", n);
}
