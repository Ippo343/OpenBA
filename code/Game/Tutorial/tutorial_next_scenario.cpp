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
//   0x140098060  GameUpdateLoop ctor      -> the real class (Game/GameUpdateLoop.h)
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

#include "Game/exe_bodies.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/Filename.h"
#include "system/String.h"
#include "Tutorial/Tutorial.h"
#include "Game/GameUpdateLoop.h"       // ENDGAME B.3.9 EXA: the real loop classes
#include "Game/GameLoadDialogLoopT.h"
#include "Game/GameLoaderLoop.h"
#include "Game/Scenario.h"
#include "io/Persistence.h"   // ENDGAME B.3.4 caller batch P
#include "io/FileSystem.h"    // ENDGAME B.3.4 caller batch P
#include "save/SaveLoad.h"    // ENDGAME B.3.7 callers (K2)
#include <new>
#include <cstdint>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "system/Value.h"      // RtSystem_Time_Ctor

namespace {

// Steps 2 and 3 of the chain, shared by both branches:
//   GameLoadDialogLoopT (0xb60) wrapping the GameUpdateLoop, then the outer
//   GameLoaderLoop (0x48). Identical to retail's tail and to the builders in
//   worldsetup_reload.cpp / newgamedialog.cpp / saveloaddialog.cpp.
// ENDGAME B.3.9 EXA: the real loop classes (retail inlines the splash's
// subclass ctor here; the class's ctor is that body).
void* BuildLoaderChain(Engine::UpdateLoop* pGameLoop) {
    Engine::UpdateLoop* pDlLoop = nullptr;
    void* dlLoop = System::MemoryAllocator::MemAlloc(0xb60, 4);
    if (dlLoop) {
        uint64_t delayTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&delayTime), 1.0f);

        System::String loadingName;
        GetLoadingSoundString(&loadingName);   // 0x140099130

        pDlLoop = ::new (dlLoop) GameLoadDialogLoopT(
            loadingName, pGameLoop, *reinterpret_cast<const System::Time*>(&delayTime));
    }

    void* pOuterLoop = nullptr;
    void* outerLoop = System::MemoryAllocator::MemAlloc(0x48, 4);
    if (outerLoop) {
        uint64_t zeroTime = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&zeroTime), 0.0f);
        pOuterLoop = ::new (outerLoop) GameLoaderLoop(
            pDlLoop, *reinterpret_cast<const System::Time*>(&zeroTime));
    }
    return pOuterLoop;
}

// One-line report of anything unresolved, instead of an EXEC-of-0.
const char* MissingDependency(bool needSaveLoad) {
    if (g_gameContainer == nullptr) return "KeyMap/scenario-list singleton";
    if (GameUpdateLoop::_singleton == nullptr) return "GameWorld singleton";
    if (!needSaveLoad) return nullptr;
    if (!::Save::SaveLoad::_singleton) return "SaveLoad singleton";
    if (!::IO::Persistence::_singleton) return "Persistence singleton";
    if (!::IO::FileSystem::_singleton) return "FileSystem singleton";
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
        reinterpret_cast<uintptr_t>(g_gameContainer) + 8);
    uintptr_t resArray = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(scenarioList) + 0x20);
    void* resource = *reinterpret_cast<void**>(resArray + static_cast<uintptr_t>(scenIdx) * 8);

    void* scenario = Object::IsDerivedFromNullSafe(resource, Scenario::TypeId()) ? resource : nullptr;
    uintptr_t sc = reinterpret_cast<uintptr_t>(scenario);

    // 0x140013AA0 -- Scenario::GetSaveName, String(scenario + 0x88). Retail runs
    // this on a possibly-null `scenario` (the IsDerivedFrom filter can yield 0);
    // String(0x88) off null would fault, so a null scenario is an upstream bug we
    // want to see named rather than silently skipped.
    if (!scenario) {
        fprintf(stderr, "[TutNext] scenario[%d]=%p is not a Scenario (typeId filter %u) "
                        "— cannot start the next tutorial\n",
                scenIdx, resource, Scenario::TypeId());
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

    Engine::UpdateLoop* pGameLoop = nullptr;

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

        void* gameLoop = System::MemoryAllocator::MemAlloc(0x160, 8);
        if (gameLoop) {
            // ENDGAME B.3.9 EXA: the real class; ctor 0x140098060 copy-constructs
            // the name (retail's by-value String arrives by hidden pointer), and
            // our local releases its own reference as retail's ~String does.
            pGameLoop = ::new (gameLoop) GameUpdateLoop(
                nullptr, ibpCopy, scenIdx, displayNameStr,
                static_cast<int32_t>(seedVal), static_cast<int32_t>(terrainSizeVal),
                /*p8=*/disasterOff != 0, reinterpret_cast<void*>(mapGen),
                reinterpret_cast<void*>(terrainGen), reinterpret_cast<void*>(climateGen));
        }
    } else {
        // ---- SAVED-SCENARIO branch -----------------------------------------
        if (const char* missing = MissingDependency(/*needSaveLoad=*/true)) {
            fprintf(stderr, "[TutNext] SAVED branch ABORTED — %s is NULL\n", missing);
            fflush(stderr);
            return;
        }

        void* saveLoad = ::Save::SaveLoad::_singleton;
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(3), 0);   // B.3.7 bridge
        reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveName(saveNameStr, 2);   // B.3.7 bridge

        // Tutorial saves live inside WinData/*.pkg, so Persistence+0x90 must
        // point at the DATA directory for the load, then be restored. Identical
        // to ScenarioDialog::LoadSelected -- including that GetSavePath returns a
        // System::String (destroyed with String::~String at the very end) while
        // the GetDataDirectory temp is a Filename.
        void* persistence = ::IO::Persistence::_singleton;
        System::String savePathBuf;
        // B.3.4 bridge, twice. Each member returns BY VALUE through MSVC's
        // hidden return slot, and the buffer below is the slot the door was
        // handed, so the prvalue is constructed straight into it (guaranteed
        // elision): one AddRef each, exactly as the door did, and each local's
        // own destructor is the one release it always was.
        ::new (&savePathBuf) System::String(
            reinterpret_cast<const IO::Persistence*>(persistence)->GetSavePath());
        void* fileSystem = ::IO::FileSystem::_singleton;
        System::Filename dataDirBuf;
        ::new (&dataDirBuf) System::Filename(
            reinterpret_cast<const IO::FileSystem*>(fileSystem)->GetDataDirectory());
        *reinterpret_cast<System::String*>(
            reinterpret_cast<uintptr_t>(::IO::Persistence::_singleton) + 0x90) =
            dataDirBuf.AsString();

        int operation = static_cast<int>(reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetOperation());   // B.3.7 bridge
        int loopCount = 0;
        while (operation != 0) {
            uint8_t zeroTime[8];
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(zeroTime), 0.0f);
            reinterpret_cast<Save::SaveLoad*>(saveLoad)->Update(*reinterpret_cast<const System::Time*>(zeroTime));   // B.3.7 bridge
            operation = static_cast<int>(reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetOperation());   // B.3.7 bridge
            ++loopCount;
        }
        fprintf(stderr, "[TutNext] SaveLoad pump finished after %d iterations\n", loopCount);
        fflush(stderr);

        *reinterpret_cast<System::String*>(
            reinterpret_cast<uintptr_t>(::IO::Persistence::_singleton) + 0x90) = savePathBuf;

        void* savedState = reinterpret_cast<Save::SaveLoad*>(saveLoad)->GetSaveState();   // B.3.7 bridge

        void* gameLoop = System::MemoryAllocator::MemAlloc(0x160, 8);
        if (gameLoop) {
            // Retail passes an EMPTY String here (local_res8 = 0). A null COW
            // pointer IS the empty string -- no allocation, nothing to destruct.
            const System::String emptyName;
            pGameLoop = ::new (gameLoop) GameUpdateLoop(
                savedState, ibpCopy, scenIdx, emptyName,
                0, 0, /*p8=*/false, nullptr, nullptr, nullptr);
        }

    }

    void* pOuterLoop = BuildLoaderChain(pGameLoop);

    // Publish on the GAME WORLD singleton: cursor + fullscreen fade on its UI
    // (+0x98), next loop at +0xC8. Retail has no controller test here.
    SetEngineNextLoop(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton), pOuterLoop);

    fprintf(stderr, "[TutNext] next-scenario loop chain queued (gameLoop=%p loader=%p)\n",
            (void*)pGameLoop, pOuterLoop);
    fflush(stderr);
}
