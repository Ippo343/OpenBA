// code/Game/Tutorial/tutorial_next_scenario.cpp
// Port of exe 0x1400E0D70 -- FinishTutorialInstance_helper_0d70, the
// "tutorial finished -> boot the next scenario" transition.
//
// This is what the tutorial panel's final button does. Until now
// TutWave_StepExecute ended the last step with a DarkOnce stub, so after
// "step 45/45 complete" the button did nothing at all -- the observable being
// a stream of `[TutorialUI] Slot9 event type=0x12 id=1400` with no response.
//
// The stub's comment claimed four unported exe functions made it out of scope.
// Three of them are one-liners and the fourth is a flat field-init:
//   0x140013A60  Scenario::GetDisplayName -> String(scenario + 0x58)
//   0x140013AA0  Scenario::GetSaveName    -> String(scenario + 0x88)
//   0x140099130  GetLoadingSoundString    -> already inline in engine_funcs.h
//   0x140098060  GameUpdateLoop ctor      -> flat 0x160-byte field init, below
// (See reference_stub_comment_outlived_the_port_that_replaced_it: grep the
// retail ADDRESS, not the stub's prose.)
//
// Retail body, verified against the decompile of 0x1400E0D70:
//   scenIdx = *(int*)(tutorialDef + 0x38); if (scenIdx < 0) return;
//   scenario = list[scenIdx] filtered by IsDerivedFrom(Scenario)
//   saveName = String(scenario + 0x88)                      // 0x140013AA0
//   if saveName is empty:
//       GameUpdateLoop(0x160) ctor'd with the scenario's worldgen params
//   else:
//       SaveLoad::SetOperation(3,0); SetSaveName(saveName, 2);
//       swap Persistence+0x90 to the DATA directory, pump SaveLoad::Update
//       until GetOperation()==0, restore Persistence+0x90;
//       GameUpdateLoop ctor'd from SaveLoad::GetSaveState()
//   then GameLoadDialogLoopT(0xb60) -> LoaderLoop(0x48) -> publish on the
//   GAME WORLD singleton (cursor + fade on its UI at +0x98, next loop at +0xC8).
//
// The tutorials ship a .sav inside WinData/*.pkg, so the SAVED branch is the one
// that runs here -- the same path ScenarioDialog::LoadSelected takes, including
// the Persistence directory swap that makes SaveLoad read out of the data dir.
//
// NOTE on the publish target: retail 0x1400E0D70 always uses
// g_pGameWorldSingleton, with no controller and no +0xC0/+0xB8 test. That is why
// this cannot simply call WorldSetup_LoadFromSave (0x1400b49f0), whose publish
// step branches on a controller's +0xC0. SetEngineNextLoop() is byte-for-byte
// 0D70's tail.

#include "engine_funcs.h"
#include "system/Filename.h"
#include "system/String.h"
#include "Tutorial/tutorial_funcs.h"
#include "io/Persistence.h"   // ENDGAME B.3.4 caller batch P
#include "io/FileSystem.h"    // ENDGAME B.3.4 caller batch P
#include "save/SaveLoad.h"    // ENDGAME B.3.7 callers (K2)
#include <new>
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

namespace {

// ---------------------------------------------------------------------------
// GameUpdateLoop ctor -- exe 0x140098060
// ---------------------------------------------------------------------------
// Flat field init over the 0x160-byte allocation. Arg -> field map taken from
// the decompile (the same map documented in worldsetup_reload.cpp):
//   p2 savedState -> +0x58 | p3 &IBP -> +0x150 (= *p3) | p4 int -> +0x15C
//   p5 String     -> +0x28 (copy)    | p6 -> +0x30     | p7   -> +0x34
//   p8 char       -> +0xD0 = (p8 ? 0 : 0xF0)
//   p9 -> +0x40   | p10 -> +0x48     | p11 -> +0x50    | everything else zeroed
//
// p5 is a System::String COPY-construct in retail. Both of our call sites hand
// it a string we own for the duration, and a String is a single COW pointer, so
// copying the pointer and NOT destructing our source would double-free. We
// instead copy the pointer and let the caller's String live on -- matching what
// the three existing inline copies of this ctor already do.
void GameUpdateLoop_Ctor_98060(void* self, void* savedState, uint64_t ibp,
                               uint32_t scenarioIdx, uint64_t nameStr,
                               uint32_t p6, uint32_t p7, char p8,
                               uint64_t p9, uint64_t p10, uint64_t p11) {
    uintptr_t g = reinterpret_cast<uintptr_t>(self);

    fn_UpdateLoop_Ctor(self);
    *reinterpret_cast<void**>(self) = (void*)g_GameUpdateLoop_Vtable;

    *reinterpret_cast<uint64_t*>(g + 0x28) = nameStr;
    *reinterpret_cast<uint32_t*>(g + 0x30) = p6;
    *reinterpret_cast<uint32_t*>(g + 0x34) = p7;
    *reinterpret_cast<uint64_t*>(g + 0x38) = 0;
    *reinterpret_cast<uint64_t*>(g + 0x40) = p9;
    *reinterpret_cast<uint64_t*>(g + 0x48) = p10;
    *reinterpret_cast<uint64_t*>(g + 0x50) = p11;
    *reinterpret_cast<uint64_t*>(g + 0x58) = reinterpret_cast<uint64_t>(savedState);
    for (uintptr_t off = 0x60; off <= 0xB8; off += 8)
        *reinterpret_cast<uint64_t*>(g + off) = 0;
    *reinterpret_cast<uint16_t*>(g + 0xC0) = 0;
    *reinterpret_cast<uint64_t*>(g + 0xC8) = 0;
    *reinterpret_cast<uint32_t*>(g + 0xD0) = p8 ? 0u : 0xF0u;
    for (uintptr_t off = 0xD8; off <= 0x148; off += 8)
        *reinterpret_cast<uint64_t*>(g + off) = 0;
    *reinterpret_cast<uint64_t*>(g + 0x150) = ibp;
    *reinterpret_cast<uint8_t*>(g + 0x158)  = 0;
    *reinterpret_cast<uint32_t*>(g + 0x15C) = scenarioIdx;
}

// Steps 2 and 3 of the chain, shared by both branches:
//   GameLoadDialogLoopT (0xb60) wrapping the GameUpdateLoop, then the outer
//   LoaderLoop (0x48). Identical to retail's tail and to the copies in
//   worldsetup_reload.cpp / newgamedialog.cpp / saveloaddialog.cpp.
void* BuildLoaderChain(void* pGameLoop) {
    void* pDlLoop = nullptr;
    void* dlLoop = fn_MemAlloc(g_pMemAllocSingleton, 0xb60, 4);
    if (dlLoop) {
        uintptr_t dl = reinterpret_cast<uintptr_t>(dlLoop);
        uint64_t delayTime[1];
        fn_Time_Ctor_Float((void*)delayTime, 1.0f);

        System::String loadingName;
        GetLoadingSoundString(&loadingName);   // 0x140099130

        fn_DialogLoaderLoopT_Ctor(dlLoop, &loadingName, pGameLoop, (void*)delayTime);

        *reinterpret_cast<void**>(dlLoop) = (void*)g_GameLoadDialogLoopT_Vtable;
        *reinterpret_cast<uint32_t*>(dl + 0x188) = 0xFF000000;
        *reinterpret_cast<uint32_t*>(dl + 0x18C) = 0xFF000000;
        *reinterpret_cast<uint8_t*>(dl + 0x190)  = 0;   // retail `this[400]`
        fn_Random_Default_Ctor((void*)(dl + 0x194));
        *reinterpret_cast<uint64_t*>(dl + 0xB58) = 0;
        *reinterpret_cast<uint8_t*>(dl + 0x40)   = 0;
        pDlLoop = dlLoop;
    }

    void* pOuterLoop = nullptr;
    void* outerLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x48, 4);
    if (outerLoop) {
        uint64_t zeroTime[1];
        fn_Time_Ctor_Float((void*)zeroTime, 0.0f);
        fn_LoaderLoop_Ctor(outerLoop, pDlLoop, (void*)zeroTime);
        *reinterpret_cast<void**>(outerLoop) = (void*)g_GameLoaderLoop_Vtable;
        pOuterLoop = outerLoop;
    }
    return pOuterLoop;
}

// One-line report of anything unresolved, instead of an EXEC-of-0.
const char* MissingDependency(bool needSaveLoad) {
    if (!g_pKeyMapSingleton || !*g_pKeyMapSingleton) return "KeyMap/scenario-list singleton";
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return "GameWorld singleton";
    if (!g_TypeId_Scenario) return "Scenario type id (0x1401c5b08)";
    if (!fn_IsDerivedFrom) return "IsDerivedFrom";
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return "MemAlloc";
    if (!fn_UpdateLoop_Ctor) return "UpdateLoop::UpdateLoop";
    if (!g_GameUpdateLoop_Vtable) return "GameUpdateLoop vtable";
    if (!fn_DialogLoaderLoopT_Ctor) return "DialogLoaderLoopT::DialogLoaderLoopT";
    if (!g_GameLoadDialogLoopT_Vtable) return "GameLoadDialogLoopT vtable";
    if (!fn_LoaderLoop_Ctor) return "LoaderLoop::LoaderLoop";
    if (!g_GameLoaderLoop_Vtable) return "GameLoaderLoop vtable";
    if (!fn_Time_Ctor_Float) return "Time::Time(float)";
    if (!fn_Random_Default_Ctor) return "Random::Random";
    if (!fn_InternalBasePtr_Ctor) return "InternalBasePtr::InternalBasePtr";
    if (!needSaveLoad) return nullptr;
    if (!g_pSaveLoadSingleton || !*g_pSaveLoadSingleton) return "SaveLoad singleton";
    if (!g_pPersistenceSingleton || !*g_pPersistenceSingleton) return "Persistence singleton";
    if (!g_pFileSystemSingleton2 || !*g_pFileSystemSingleton2) return "FileSystem singleton";
    return nullptr;
}

}  // namespace

// ---------------------------------------------------------------------------
// FinishTutorialInstance_helper_0d70 (exe 0x1400E0D70)
// ---------------------------------------------------------------------------
// `self` is the FinishTutorialInstance; self+0x30 is the Tutorial definition and
// tutorialDef+0x38 is the index of the scenario to load next.
extern "C" void TutWave_LoadNextScenario(void* self) {
    if (!self) return;
    uintptr_t s = reinterpret_cast<uintptr_t>(self);

    uintptr_t tutorialDef = *reinterpret_cast<uintptr_t*>(s + 0x30);
    if (!tutorialDef) {
        fprintf(stderr, "[TutNext] tutInst+0x30 (Tutorial def) is NULL — no next scenario\n");
        fflush(stderr);
        return;
    }

    // Retail's only bounds test. A negative index means "no scenario follows
    // this tutorial", which is how the last tutorial in the chain ends.
    int scenIdx = *reinterpret_cast<int32_t*>(tutorialDef + 0x38);
    if (scenIdx < 0) {
        fprintf(stderr, "[TutNext] scenarioIndex=%d (<0) — this tutorial ends the chain\n", scenIdx);
        fflush(stderr);
        return;
    }

    if (const char* missing = MissingDependency(/*needSaveLoad=*/false)) {
        fprintf(stderr, "[TutNext] ABORTED — %s is NULL\n", missing);
        fflush(stderr);
        return;
    }

    // The scenario list is the SAME one ScenarioDialog enumerates:
    // *(*g_pKeyMapSingleton + 8), resources at *(list + 0x20)[i].
    void* scenarioList = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pKeyMapSingleton) + 8);
    uintptr_t resArray = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(scenarioList) + 0x20);
    void* resource = *reinterpret_cast<void**>(resArray + static_cast<uintptr_t>(scenIdx) * 8);

    void* scenario = fn_IsDerivedFrom(resource, *g_TypeId_Scenario) ? resource : nullptr;
    uintptr_t sc = reinterpret_cast<uintptr_t>(scenario);

    // 0x140013AA0 -- Scenario::GetSaveName, String(scenario + 0x88). Retail runs
    // this on a possibly-null `scenario` (the IsDerivedFrom filter can yield 0);
    // String(0x88) off null would fault, so a null scenario is an upstream bug we
    // want to see named rather than silently skipped.
    if (!scenario) {
        fprintf(stderr, "[TutNext] scenario[%d]=%p is not a Scenario (typeId filter %u) "
                        "— cannot start the next tutorial\n",
                scenIdx, resource, *g_TypeId_Scenario);
        fflush(stderr);
        return;
    }

    System::String saveNameStr("");
    // Scenario::GetSaveName -- B.3.1 bridge, the Scenario is still untyped bytes.
    saveNameStr = *reinterpret_cast<const System::String*>(sc + 0x88);

    bool hasSaveState = (saveNameStr._data != nullptr && saveNameStr.GetLength() > 0);

    fprintf(stderr, "[TutNext] next scenario index=%d resource=%p hasSaveState=%d\n",
            scenIdx, scenario, (int)hasSaveState);
    fflush(stderr);

    uint8_t nullIBP[8];
    new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    uint64_t ibpCopy = *reinterpret_cast<uint64_t*>(nullIBP);

    void* pGameLoop = nullptr;

    if (!hasSaveState) {
        // ---- NEW-SCENARIO branch -------------------------------------------
        // 0x140013A60 -- Scenario::GetDisplayName, String(scenario + 0x58).
        System::String displayNameStr("");
        displayNameStr = *reinterpret_cast<const System::String*>(sc + 0x58);

        // Worldgen params. The outer two of the generator triple are stored in
        // the order mapGen(+0x70) / terrainGen(+0x78) / climateGen(+0x80) --
        // positionally p9/p10/p11 -> loop +0x40/+0x48/+0x50, which
        // GameUpdateLoop::Load reads back in that order. Same mapping
        // ScenarioDialog::LoadSelected documents.
        uint32_t seedVal        = *reinterpret_cast<uint32_t*>(sc + 0x60);
        uint32_t terrainSizeVal = *reinterpret_cast<uint32_t*>(sc + 0x64);
        char     disasterOff    = *reinterpret_cast<char*>(sc + 0x68);
        uint64_t mapGen         = *reinterpret_cast<uint64_t*>(sc + 0x70);
        uint64_t terrainGen     = *reinterpret_cast<uint64_t*>(sc + 0x78);
        uint64_t climateGen     = *reinterpret_cast<uint64_t*>(sc + 0x80);

        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        if (gameLoop) {
            GameUpdateLoop_Ctor_98060(gameLoop, nullptr, ibpCopy,
                                      static_cast<uint32_t>(scenIdx),
                                      reinterpret_cast<uint64_t>(displayNameStr._data),
                                      seedVal, terrainSizeVal, disasterOff,
                                      mapGen, terrainGen, climateGen);
            pGameLoop = gameLoop;
            // The ctor STEALS the COW pointer into the loop at +0x28 (see its
            // header comment) rather than copy-constructing, so hand ownership
            // over: null ours so the destructor does not release the loop's.
            displayNameStr._data = nullptr;
        }
    } else {
        // ---- SAVED-SCENARIO branch -----------------------------------------
        if (const char* missing = MissingDependency(/*needSaveLoad=*/true)) {
            fprintf(stderr, "[TutNext] SAVED branch ABORTED — %s is NULL\n", missing);
            fflush(stderr);
            return;
        }

        void* saveLoad = *g_pSaveLoadSingleton;
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(3), 0);   // B.3.7 bridge
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveName(saveNameStr, 2);   // B.3.7 bridge

        // Tutorial saves live inside WinData/*.pkg, so Persistence+0x90 must
        // point at the DATA directory for the load, then be restored. Identical
        // to ScenarioDialog::LoadSelected -- including that GetSavePath returns a
        // System::String (destroyed with String::~String at the very end) while
        // the GetDataDirectory temp is a Filename.
        void* persistence = *g_pPersistenceSingleton;
        System::String savePathBuf;
        // B.3.4 bridge, twice. Each member returns BY VALUE through MSVC's
        // hidden return slot, and the buffer below is the slot the door was
        // handed, so the prvalue is constructed straight into it (guaranteed
        // elision): one AddRef each, exactly as the door did, and each local's
        // own destructor is the one release it always was.
        ::new (&savePathBuf) System::String(
            reinterpret_cast<const IO::Persistence*>(persistence)->GetSavePath());
        void* fileSystem = *g_pFileSystemSingleton2;
        System::Filename dataDirBuf;
        ::new (&dataDirBuf) System::Filename(
            reinterpret_cast<const IO::FileSystem*>(fileSystem)->GetDataDirectory());
        *reinterpret_cast<System::String*>(
            *reinterpret_cast<uintptr_t*>(g_pPersistenceSingleton) + 0x90) =
            dataDirBuf.AsString();

        int operation = static_cast<int>(reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetOperation());   // B.3.7 bridge
        int loopCount = 0;
        while (operation != 0) {
            uint8_t zeroTime[8];
            fn_Time_Ctor_Float(zeroTime, 0.0f);
            reinterpret_cast<Save::SaveLoad*>(saveLoad)->Update(*reinterpret_cast<const System::Time*>(zeroTime));   // B.3.7 bridge
            operation = static_cast<int>(reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetOperation());   // B.3.7 bridge
            ++loopCount;
        }
        fprintf(stderr, "[TutNext] SaveLoad pump finished after %d iterations\n", loopCount);
        fflush(stderr);

        *reinterpret_cast<System::String*>(
            *reinterpret_cast<uintptr_t*>(g_pPersistenceSingleton) + 0x90) = savePathBuf;

        void* savedState = reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetSaveState();   // B.3.7 bridge

        void* gameLoop = fn_MemAlloc(g_pMemAllocSingleton, 0x160, 8);
        if (gameLoop) {
            // Retail passes an EMPTY String here (local_res8 = 0). A null COW
            // pointer IS the empty string -- no allocation, nothing to destruct.
            GameUpdateLoop_Ctor_98060(gameLoop, savedState, ibpCopy,
                                      static_cast<uint32_t>(scenIdx),
                                      /*nameStr=*/0,
                                      0, 0, /*p8=*/'\0', 0, 0, 0);
            pGameLoop = gameLoop;
        }

    }

    void* pOuterLoop = BuildLoaderChain(pGameLoop);

    // Publish on the GAME WORLD singleton: cursor + fullscreen fade on its UI
    // (+0x98), next loop at +0xC8. Retail has no controller test here.
    SetEngineNextLoop(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton), pOuterLoop);

    fprintf(stderr, "[TutNext] next-scenario loop chain queued (gameLoop=%p loader=%p)\n",
            pGameLoop, pOuterLoop);
    fflush(stderr);
}
