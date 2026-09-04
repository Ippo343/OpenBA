// code/Game/Game/worldsetup_reload.cpp
// Port of exe 0x1400b49f0 — the "reload the world from a SavedState" loop chain.
//
// WHY IT LIVES HERE (and not in a Dialog/ file): 0x1400b49f0 is ONE retail
// function with TWO call sites in two different dialogs, and engine_funcs.h gives
// it two different typedef names for the same address —
//
//   engine_funcs.h  fn_Game_ReloadWithSave  (self, savedState, param, nullRef)
//                   called unguarded at moddialog.cpp (Apply → in-game reload)
//   engine_funcs.h  fn_LoadScenarioFromSave (worldSetup, savedState,
//                                            scenarioIdx, retvalPtr)
//                   called unguarded at scenariodialog.cpp
//
// Both resolve `ResolveAddr(Module::Exe, 0x1400b49f0)` and both were NULL →
// EXEC-of-0. The disassembly settles that they are the same function with the same
// ABI: retail does `puVar8 = (ulonglong)param_3 & 0xffffffff` then uses
// `(int)param_3`, i.e. arg3 is an INT widened to a pointer — ModDialog passes
// (void*)-1 ("no scenario"), ScenarioDialog passes the scenario resource index. So
// this must be registered EXACTLY ONCE, and neither dialog owns it; it is a
// WorldSetup/loop-chain function. Registering it here fixes BOTH call sites with no
// edit to scenariodialog.cpp.
//
// Retail body (0x1400b49f0), verified against the decompile:
//   1. MemAlloc(0x160) -> GameUpdateLoop, constructed by FUN_140098060 with
//        (savedState, &(*retvalPtr), (int)scenarioIdx, emptyString, 0,0,'\0',0,0,0)
//   2. MemAlloc(0xb60) -> GameLoadDialogLoopT: Time(1.0f), FUN_140099130 name
//        ("Dialog/Loading.rsc:gameLoad%i", Rand(12)), DialogLoaderLoopT base ctor,
//        vtable -> 0x140158800, then the subclass field init.
//   3. MemAlloc(0x48)  -> outer LoaderLoop, Time(0.0f), vtable -> 0x1401588d0.
//   4. Publish: `if (self+0xC0 == 0)` use the GAME WORLD at self+0xB8 (SetCursor +
//        fade on its UI at +0x98, next loop at +0xC8); else use the dialog loader
//        at self+0xC0 (fade on +0x40, next loop at +0xb68).
//      ⚠ Note the test is on +0xC0 FIRST here, the INVERSE of 0x1400b4790
//      (MainMenuDialog_ReturnToMenu), which tests +0xB8 first. Kept faithful.
//
// FUN_140098060 arg->field map (decompiled, 0x140098060):
//   p2 savedState -> +0x58 | p3 &IBP -> +0x150 (= *p3) | p4 int -> +0x15C
//   p5 String     -> +0x28 (copy-ctor)  | p6 -> +0x30 | p7 -> +0x34
//   p8 char       -> +0xD0 = (p8 ? 0 : 0xF0)   <- p8 is '\0' here, so +0xD0 = 0xF0
//   p9 -> +0x40 | p10 -> +0x48 | p11 -> +0x50 | everything else zeroed
//   (saveloaddialog.cpp's inline copy of this ctor passes p8=1 and therefore writes
//    +0xD0 = 0; that is a DIFFERENT call site, not a discrepancy with this one.)

#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

extern "C" void WorldSetup_LoadFromSave(void* worldSetup, void* savedState,
                                        void* scenarioIdx, void* retvalIbpPtr) {
    if (!worldSetup) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(worldSetup);

    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_UpdateLoop_Ctor ||
        !fn_DialogLoaderLoopT_Ctor || !fn_LoaderLoop_Ctor || !fn_Time_Ctor_Float ||
        !g_GameUpdateLoop_Vtable || !g_GameLoadDialogLoopT_Vtable ||
        !g_GameLoaderLoop_Vtable) {
        printf("[WorldSetup::LoadFromSave] loop-chain ctors unresolved - aborting "
                    "(alloc=%p updateLoop=%p dlt=%p loader=%p vt=%p/%p/%p)\n",
                    (void*)fn_MemAlloc, (void*)fn_UpdateLoop_Ctor,
                    (void*)fn_DialogLoaderLoopT_Ctor, (void*)fn_LoaderLoop_Ctor,
                    (void*)g_GameUpdateLoop_Vtable, (void*)g_GameLoadDialogLoopT_Vtable,
                    (void*)g_GameLoaderLoop_Vtable);
        return;
    }

    // The IBP the caller handed us is copied to a local BEFORE the ctor runs, and
    // the ctor stores that copy at gameLoop+0x150 (retail: local_40 = *param_4).
    uint64_t ibpCopy = retvalIbpPtr ? *reinterpret_cast<uint64_t*>(retvalIbpPtr) : 0;

    // --- Step 1: inner GameUpdateLoop (0x160) --------------------------------
    void* pGameLoop = nullptr;
    void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
    if (gameLoop) {
        uintptr_t g = reinterpret_cast<uintptr_t>(gameLoop);
        fn_UpdateLoop_Ctor(gameLoop);
        *reinterpret_cast<void**>(gameLoop) = (void*)g_GameUpdateLoop_Vtable;

        // +0x28: copy of retail's `local_res8 = 0` empty System::String. A null COW
        // pointer IS the empty string, so the copy-ctor result is a plain 0 - no
        // allocation, and nothing to destruct.
        *reinterpret_cast<uint64_t*>(g + 0x28) = 0;
        *reinterpret_cast<uint32_t*>(g + 0x30) = 0;   // p6
        *reinterpret_cast<uint32_t*>(g + 0x34) = 0;   // p7
        *reinterpret_cast<uint64_t*>(g + 0x38) = 0;
        *reinterpret_cast<uint64_t*>(g + 0x40) = 0;   // p9
        *reinterpret_cast<uint64_t*>(g + 0x48) = 0;   // p10
        *reinterpret_cast<uint64_t*>(g + 0x50) = 0;   // p11
        *reinterpret_cast<uint64_t*>(g + 0x58) = reinterpret_cast<uint64_t>(savedState);
        for (uintptr_t off = 0x60; off <= 0xB8; off += 8)
            *reinterpret_cast<uint64_t*>(g + off) = 0;
        *reinterpret_cast<uint16_t*>(g + 0xC0) = 0;
        *reinterpret_cast<uint64_t*>(g + 0xC8) = 0;
        // p8 == '\0' at this call site, so retail writes 0xF0 here (NOT 0).
        *reinterpret_cast<uint32_t*>(g + 0xD0) = 0xF0;
        for (uintptr_t off = 0xD8; off <= 0x148; off += 8)
            *reinterpret_cast<uint64_t*>(g + off) = 0;
        *reinterpret_cast<uint64_t*>(g + 0x150) = ibpCopy;
        *reinterpret_cast<uint8_t*>(g + 0x158)  = 0;
        *reinterpret_cast<uint32_t*>(g + 0x15C) =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(scenarioIdx) & 0xFFFFFFFFu);
        pGameLoop = gameLoop;
    }

    // --- Step 2: GameLoadDialogLoopT (0xb60) ---------------------------------
    void* pDlLoop = nullptr;
    void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
    if (dlLoop) {
        uintptr_t dl = reinterpret_cast<uintptr_t>(dlLoop);
        uint64_t delayTime[1];
        fn_Time_Ctor_Float((void*)delayTime, 1.0f);

        uint8_t loadingName[8] = {};
        GetLoadingSoundString(loadingName);   // FUN_140099130, inline in engine_funcs.h

        fn_DialogLoaderLoopT_Ctor(dlLoop, loadingName, pGameLoop, (void*)delayTime);
        fn_String_Dtor(loadingName);

        *reinterpret_cast<void**>(dlLoop) = (void*)g_GameLoadDialogLoopT_Vtable;
        *reinterpret_cast<uint32_t*>(dl + 0x188) = 0xFF000000;
        *reinterpret_cast<uint32_t*>(dl + 0x18C) = 0xFF000000;
        *reinterpret_cast<uint8_t*>(dl + 0x190)  = 0;   // retail `this[400]`
        fn_Random_Default_Ctor((void*)(dl + 0x194));
        *reinterpret_cast<uint64_t*>(dl + 0xB58) = 0;
        *reinterpret_cast<uint8_t*>(dl + 0x40)   = 0;
        pDlLoop = dlLoop;
    }

    // --- Step 3: outer LoaderLoop (0x48) -------------------------------------
    void* pOuterLoop = nullptr;
    void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
    if (outerLoop) {
        uint64_t zeroTime[1];
        fn_Time_Ctor_Float((void*)zeroTime, 0.0f);
        fn_LoaderLoop_Ctor(outerLoop, pDlLoop, (void*)zeroTime);
        *reinterpret_cast<void**>(outerLoop) = (void*)g_GameLoaderLoop_Vtable;
        pOuterLoop = outerLoop;
    }

    // --- Step 4: publish the next loop ---------------------------------------
    // Retail tests +0xC0 first (inverse of 0x1400b4790 - see the header comment).
    uintptr_t dialogLoader = *reinterpret_cast<uintptr_t*>(s + 0xC0);
    if (dialogLoader == 0) {
        uintptr_t gameWorld = *reinterpret_cast<uintptr_t*>(s + 0xB8);
        SetEngineNextLoop(gameWorld, pOuterLoop);   // UI at +0x98, next loop at +0xC8
    } else {
        SetDialogLoaderNextLoop(dialogLoader, pOuterLoop);
    }
    printf("[WorldSetup::LoadFromSave] chain built: game=%p dlt=%p loader=%p "
                "scenarioIdx=%d via %s\n",
                pGameLoop, pDlLoop, pOuterLoop,
                (int)(int32_t)(uintptr_t)scenarioIdx,
                dialogLoader ? "dialogLoader" : "gameWorld");
}
