// code/Game/Dialog/dialog_exe_helpers.cpp
// Clean reimplementations of two exe-internal helpers referenced by the Dialog
// module, ported to close the standalone Dialog code-gap.
//
//   0x14000d6c0  WorkplaceUI FindComponent (type 0x1401c7008) — guard check.
//   0x14005dec0  forwarder to AutoSaveDialog::Autosave (autosavedialog.cpp).
//   GameContainer_EnsureLists, the standalone stand-in for 0x140099db0.
//
// Decompiled from Application-x64.exe via Ghidra.

#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "achievement/AchievementManager.h"   // ENDGAME B.3.7 callers (K1)
#include "save/SaveLoad.h"   // ENDGAME B.3.7 callers (K1)
#include "dialog_exe_helpers.h"
#include "system/Value.h"
#include "object/TypeInfo.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/String.h"
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/BuildComponent.h"   // X::TypeId() (B.3.8 callers)
#include "Dialog/exe_bodies.h"
#include "Dialog/AutoSaveDialog.h"
#include "Entity/ComponentEntity.h"   // ComponentEntity::FindComponent<T>


// ============================================================
// WorkplaceUI FindComponent (FUN_14000d6c0, type 0x1401c7008)
// ============================================================
// Member of the byte-for-byte-identical FindComponent family (cf.
// code/Game/Work/work_base_reimpl.cpp). Scans the entity's component list
// (ptr array at entity+0x30, count at entity+0x38), calls each component's
// GetTypeId (vtable +0x10), and on the first type match runs an IsDerivedFrom
// check before returning the component through an out InternalBasePtr.
// Caller signature (workplaceui.cpp): void* fn(void* entity, void* retIBP).
void* Dialog_FindComponent_d6c0(void* entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<BuildComponent>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<BuildComponent>());   // B4 FORWARD: receiver typed by the callers' blocks
}

// ============================================================
// AutoSaveDialog autosave handler (FUN_14005dec0)
// ============================================================
// The body is AutoSaveDialog::Autosave (autosavedialog.cpp).
// B4 FORWARD: 1 outside caller (standalone/saveload_harness.cpp)
void Dialog_AutoSaveHandler(void* self, void* /*p2*/, int /*p3*/, int /*p4*/) { reinterpret_cast<AutoSaveDialog*>(self)->Autosave(); }

// ============================================================
// GameContainer_EnsureLists — standalone stand-in for GameContainer_Init (0x140099db0)
// ============================================================
// Retail populates the Game container (the object *g_pKeyMapSingleton points at) in
// GameContainer_Init, called from FUN_140001120 with the menu loop's master
// ExternalList. Standalone has no master list and never runs that init, so every slot
// stays as the ctor left it: NULL.
//
// Container index -> byte offset -> resource (from the retail decompile):
//   [0] +0x00  Game/Achievement/AchievementList.rsc
//   [1] +0x08  Game/Scenario/ScenarioList.rsc      <- ScenarioDialog::Slot4 reads this
//   [2] +0x10  Game/Climate/ClimateList.rsc
//   [3] +0x18  Game/StartCondition/StartConditions.rsc
//   [4] +0x20  Game/Terrain/TerrainList.rsc
// (retail also fills [0x19]/[0x1a]/[0x1b] StringTables and [0xb]..[0x11] UI audio
// effects; those are filled elsewhere in standalone and are not needed here.)
//
// This was previously an ad-hoc lambda inside newgamedialog.cpp that filled ONLY the
// three slots the New Game combos needed (+0x10/+0x18/+0x20). ScenarioDialog::Slot4
// reads +0x08 and calls ExternalList::GetResourceCount on it with no null check -
// faithfully, since retail always has it - so opening the Tutorial/Scenario dialog was
// an ACCESS_VIOLATION reading null+0x28. The fix is to fill the slot upstream, not to
// guard the deref. Shared here so every dialog gets the same container.
//
// Uses ExternalFactory::FindOrLoad and trusts the result rather than IsDerivedFrom-
// checking it (the type-check trips over the incomplete standalone factory tree - see
// newgamedialog.cpp's original note). Idempotent: each slot is loaded at most once.
void GameContainer_EnsureLists() {
    if (g_gameContainer == nullptr) return;
    if (!::Object::ExternalFactory::_singleton) return;

    GameContainer* container = g_gameContainer;

    struct Entry { Object::ExternalPtr<Object::ExternalList> GameContainer::* list; size_t off; const char* path; };
    static const Entry kEntries[] = {
        { &GameContainer::achievementList, 0x00, "Game/Achievement/AchievementList.rsc" },
        { &GameContainer::scenarioList,    0x08, "Game/Scenario/ScenarioList.rsc" },
        { &GameContainer::climateList,     0x10, "Game/Climate/ClimateList.rsc" },
        { &GameContainer::startConditions, 0x18, "Game/StartCondition/StartConditions.rsc" },
        { &GameContainer::terrainList,     0x20, "Game/Terrain/TerrainList.rsc" },
    };

    for (const Entry& e : kEntries) {
        Object::ExternalPtr<Object::ExternalList>& slot = container->*e.list;
        if (slot) continue;                       // already loaded
        System::Filename fnameObj(e.path);   // ctor already normalises delimiters
        // Trusted, not IsDerivedFrom-checked (see above).
        Object::ExternalList* ext = static_cast<Object::ExternalList*>(const_cast<::Object::External*>(
            ::Object::ExternalFactory::_singleton->FindOrLoad(fnameObj)));
        slot = ext;
        fprintf(stderr, "[GameContainer] +0x%02zx '%s' -> %p\n", e.off, e.path, (void*)ext);
    }

    // Retail's GameContainer_Init hands slot [0] straight to the AchievementManager
    // (0x140099fe3), immediately after filling the five list slots:
    //     AchievementManager::SetAchievements(_singleton, container[0]);
    // That call is what gives the manager its achievement list; it also sizes the
    // awarded/instance arrays to the resource count and reads the "Ach" registry keys.
    // Without it the manager is the zeroed object Engine::Startup left behind, and
    // GetAchievementCount -- `*(int*)(*(void**)this + 0x28)` -- derefs a null list.
    // That is exactly how the Achievements dialog crashed: the manager was there, its
    // list was not. Once per process, like retail.
    static bool s_achievementsSet = false;
    if (!s_achievementsSet) {
        Object::ExternalList* achievementList = container->achievementList.Get();
        Achievement::AchievementManager* mgr = ::Achievement::AchievementManager::_singleton;
        if (mgr && achievementList) {
            mgr->SetAchievements(container->achievementList);
            s_achievementsSet = true;
            // Called outside the printf argument list: the perf build strips printf arguments.
            const int count = mgr->GetAchievementCount();
            fprintf(stderr, "[GameContainer] AchievementManager::SetAchievements(mgr=%p, list=%p) "
                            "-> count=%d\n", (void*)mgr, (void*)achievementList, count);
        } else {
            fprintf(stderr, "[GameContainer] SetAchievements SKIPPED (mgr=%p list=%p)\n",
                    (void*)mgr, (void*)achievementList);
        }
    }
    fflush(stderr);
}
