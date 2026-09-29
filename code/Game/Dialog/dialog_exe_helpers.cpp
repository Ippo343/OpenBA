// code/Game/Dialog/dialog_exe_helpers.cpp
// Clean reimplementations of two exe-internal helpers referenced by the Dialog
// module, ported to close the standalone Dialog code-gap.
//
//   0x14000d6c0  WorkplaceUI FindComponent (type 0x1401c7008) — guard check.
//   0x14005dec0  AutoSaveDialog autosave handler (timer-event 0x13).
//   0x140099d40  autosave-name builder ("Autosave" UIString) — sub-callee of 0x14005dec0.
//
// Depends only on engine_funcs.h.
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
// Autosave-name builder (FUN_140099d40)
// ============================================================
// Builds a UI::String "Autosave" into the out buffer from the KeyMap's
// string-table external (keymap+0xd8). Sub-callee of the autosave handler.
//   String local("Autosave"); UIString::Ctor(retbuf, *(keymap+0xd8), local); ~local;
static void* Dialog_AutoSaveBuildName(void* keymap, void* retUIString) {
    System::String nameStr("Autosave");
    ::new (retUIString) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(keymap) + 0xd8))), nameStr);  // B.3.6 bridge
    return retUIString;
}

// ============================================================
// AutoSaveDialog autosave handler (FUN_14005dec0)
// ============================================================
// Triggered by AutoSaveDialog::Slot8 on timer event 0x13. If the game is not
// paused and not in the bit-10 "blocked" state, and no save/load operation is
// already in progress, kicks off an autosave: 1s min-display time, the KeyMap
// screenshot sprite, the localized "Autosave" name, then SetOperation(4,-4).
//
// param_1 = AutoSaveDialog* (self; UserInterface at +0x10). p2/p3/p4 forwarded.
//
// UI::UserInterface::PausesGame is a direct member call (ENDGAME B.3.6, K2: the
// file-local by-name cache went with its last caller); the Save::SaveLoad calls are
// direct member calls too (ENDGAME B.3.7, K1).

static void AsBail(const char* why, long v) {
    fprintf(stderr, "[AutoSave] timer FIRED but bailed: %s (%ld)\n", why, v);
    fflush(stderr);
}

void Dialog_AutoSaveHandler(void* self, void* /*p2*/, int /*p3*/, int /*p4*/) {
    // Not while the UI pauses the game.
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(self) + 0x10);
    if (reinterpret_cast<const UI::UserInterface*>(ui)->PausesGame()) { AsBail("UI PausesGame", 1); return; }  // B.3.6 bridge

    // Not while GameWorld flag bit 10 is set.
    if (GameUpdateLoop::_singleton == nullptr) { AsBail("no gameWorld", 0); return; }
    uint32_t gwFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xd0);
    if ((gwFlags >> 10) & 1) { AsBail("gameWorld+0xd0 bit10 set", (long)gwFlags); return; }

    // Only when no operation is already in progress (Operation::None == 0).
    if (!::Save::SaveLoad::_singleton) { AsBail("no SaveLoad singleton", 0); return; }
    void* saveLoad = ::Save::SaveLoad::_singleton;
    { int op = reinterpret_cast<const Save::SaveLoad*>(saveLoad)->GetOperation();  // B.3.7 bridge
      if (op != 0) { AsBail("SaveLoad busy, Operation", (long)op); return; } }

    // Min operation-display time = 1.0s.
    uint8_t timeBuf[8];
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeBuf), 1.0f);   // import slot Time::Time(float)
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetMinOperationTime(*reinterpret_cast<const System::Time*>(timeBuf));  // B.3.7 bridge

    // Screenshot sprite (16 bytes) copied from KeyMap+0xb8.
    if (g_gameContainer == nullptr) return;
    uintptr_t keymap = reinterpret_cast<uintptr_t>(g_gameContainer);
    uint8_t spriteBuf[16];
    *reinterpret_cast<uint64_t*>(spriteBuf)     = *reinterpret_cast<uint64_t*>(keymap + 0xb8);
    *reinterpret_cast<uint64_t*>(spriteBuf + 8) = *reinterpret_cast<uint64_t*>(keymap + 0xc0);
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.7 bridge

    // Localized "Autosave" name → System::String → SetSaveName(slot 2).
    uint8_t uiStrBuf[16];
    Dialog_AutoSaveBuildName(reinterpret_cast<void*>(keymap), uiStrBuf);
    System::String sysStr;
    ::new (&sysStr) System::String(reinterpret_cast<const UI::String*>(uiStrBuf)->operator System::String());  // B.3.6 bridge
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetSaveName(sysStr, 2);  // B.3.7 bridge


    // Kick off the save operation (Operation 4, param -4).
    reinterpret_cast<Save::SaveLoad*>(saveLoad)->SetOperation(static_cast<Save::SaveLoad::Operation>(4), -4);  // B.3.7 bridge
}

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

    char* container = reinterpret_cast<char*>(g_gameContainer);

    struct Entry { size_t off; const char* path; };
    static const Entry kEntries[] = {
        { 0x00, "Game/Achievement/AchievementList.rsc" },
        { 0x08, "Game/Scenario/ScenarioList.rsc" },
        { 0x10, "Game/Climate/ClimateList.rsc" },
        { 0x18, "Game/StartCondition/StartConditions.rsc" },
        { 0x20, "Game/Terrain/TerrainList.rsc" },
    };

    for (const Entry& e : kEntries) {
        void** slot = reinterpret_cast<void**>(container + e.off);
        if (*slot) continue;                       // already loaded
        System::Filename fnameObj(e.path);   // ctor already normalises delimiters
        void* ext = const_cast<::Object::External*>(
            ::Object::ExternalFactory::_singleton   // B.3.3 bridge
                ->FindOrLoad(fnameObj));
        *slot = ext;
        fprintf(stderr, "[GameContainer] +0x%02zx '%s' -> %p\n", e.off, e.path, ext);
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
        void* achievementList = *reinterpret_cast<void**>(container + 0x00);
        void* mgr = ::Achievement::AchievementManager::_singleton;
        if (mgr && achievementList) {
            reinterpret_cast<Achievement::AchievementManager*>(mgr)->SetAchievements(  // B.3.7 bridge
                Object::ExternalPtr<Object::ExternalList>(reinterpret_cast<Object::ExternalList*>(achievementList)));
            s_achievementsSet = true;
            // Called outside the printf argument list: the perf build strips printf arguments.
            const int count =
                reinterpret_cast<Achievement::AchievementManager*>(mgr)->GetAchievementCount();  // B.3.7 bridge
            fprintf(stderr, "[GameContainer] AchievementManager::SetAchievements(mgr=%p, list=%p) "
                            "-> count=%d\n", mgr, achievementList, count);
        } else {
            fprintf(stderr, "[GameContainer] SetAchievements SKIPPED (mgr=%p list=%p)\n",
                    mgr, achievementList);
        }
    }
    fflush(stderr);
}
