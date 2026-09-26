// standalone/reg_tutorial_wave.cpp
//
// Lane 2 (Tutorial) wave registrations.
//
// The 12 exe-internal helper FUNCTIONS the Tutorial module resolves by
// address and that nothing registered, so every one of them was a null
// function pointer. Bodies in code/Game/Tutorial/tutorial_wave.cpp.
//
// (The second kind this file used to hold, the six hand-built tutorial dialog
// vtables and two hand-rolled ctors registered at their .rdata / code
// addresses, is gone: ENDGAME B.3.8 block GOAL made TutorialUI and the three
// tutorial dialogs real classes and no by-address reader of those addresses
// remains.)
//
// ENTRY VERIFICATION: every function address below was checked in Ghidra with
// get_function_by_address and Entry == the address, so none of them is a
// mid-function resolve ([[reference-midfunction-raw-address-resolves]]).
//
// CLOSURE: get_function_callees was run on all 12. Exe sub-callees resolve to
//   0x140014890  fn_BST_Insert_0x14                    — registered (reg_toolbar_utility)
//   0x140100270  fn_FindDialogByControllerType         — registered (reg_toolbar_utility)
//   0x14001B1D0  ResolveExternalByName                 — NOT called by address; the
//                inline ResolveExternalByName_Impl in engine_funcs.h is used instead
//   0x1400E1870  FindUIElementByPath                   — LANE 1's list. Both call
//                sites guard on fn_FindUIElementByPath and log "[Tutorial] NOT PORTED"
//   0x1400E0D70  tutorial-complete transition          — NOT PORTED (its own closure
//                needs 0x140013A60 / 0x140013AA0 / 0x140098060 / 0x140099130)
// Everything else is a Runtime-x64.dll export reached through engine_funcs.
//
// ⚠ DUPLICATE-REGISTRATION SWEEP: this worktree is based on a commit ~284
// commits behind HEAD, so the local grep for pre-existing RegisterExeAddr of
// these addresses is NOT authoritative
// ([[reference-integration-duplicate-registeraddr-sweep]]). The integrator must
// re-run the sweep for every address in this file against real HEAD. The full
// list is repeated in notes/wave_remaining/LANE2_TUTORIAL_REPORT.md.

#include "resolve.h"
#include <cstdint>

// --- (a) helper functions (code/Game/Tutorial/tutorial_wave.cpp) ---
extern "C" void  TutWave_SerializeTutorialRef(void*, void*);                    // 0x1400262B0
extern "C" void  TutWave_SerializeDialogRef(void*, void*);                      // 0x140028650
extern "C" void  TutWave_TutorialTreeInsert(void*, uint64_t, void*, uint32_t);  // 0x1401001F0
extern "C" void  TutWave_SetElementFlagsRecursive(void*, void*, char);          // 0x1400E2250
extern "C" void  TutWave_SetAllDialogFlags(void*, char);                        // 0x1400E2330
extern "C" void* TutWave_FindHighlightDialog(void*, void*);                     // 0x1400E24F0
extern "C" void  TutWave_OpenHighlightDialog(void*, char);                      // 0x1400E2700
extern "C" void  TutWave_ShowListedElements(void*);                             // 0x1400E29F0
extern "C" void* TutWave_ExternalRefCopy(void*, void*);                         // 0x14001AF90
extern "C" void* TutWave_ExternalRefResolve(void*, void*);                      // 0x14001AFE0
extern "C" void  TutWave_RefreshStepUI(void*);                                  // 0x1400E1260
extern "C" void  TutWave_StepExecute(void*, void*, void*, uint64_t);            // 0x1400E1DB0

void RegisterTutorialWaveBatch() {
    // (a) helpers
    RegisterExeAddr(0x1400262b0, reinterpret_cast<uintptr_t>(&TutWave_SerializeTutorialRef));
    RegisterExeAddr(0x140028650, reinterpret_cast<uintptr_t>(&TutWave_SerializeDialogRef));
    RegisterExeAddr(0x1401001f0, reinterpret_cast<uintptr_t>(&TutWave_TutorialTreeInsert));
    RegisterExeAddr(0x1400e2250, reinterpret_cast<uintptr_t>(&TutWave_SetElementFlagsRecursive));
    RegisterExeAddr(0x1400e2330, reinterpret_cast<uintptr_t>(&TutWave_SetAllDialogFlags));
    RegisterExeAddr(0x1400e24f0, reinterpret_cast<uintptr_t>(&TutWave_FindHighlightDialog));
    RegisterExeAddr(0x1400e2700, reinterpret_cast<uintptr_t>(&TutWave_OpenHighlightDialog));
    RegisterExeAddr(0x1400e29f0, reinterpret_cast<uintptr_t>(&TutWave_ShowListedElements));
    RegisterExeAddr(0x14001af90, reinterpret_cast<uintptr_t>(&TutWave_ExternalRefCopy));
    RegisterExeAddr(0x14001afe0, reinterpret_cast<uintptr_t>(&TutWave_ExternalRefResolve));
    RegisterExeAddr(0x1400e1260, reinterpret_cast<uintptr_t>(&TutWave_RefreshStepUI));
    RegisterExeAddr(0x1400e1db0, reinterpret_cast<uintptr_t>(&TutWave_StepExecute));
}
