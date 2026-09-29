#pragma once
// code/Game/Game/game.h
// The exe-internal 0xE0-byte "Game" container (key-binding / cursor / UI-sound
// registry) and its ctor.
//
// Original: FUN_140001050 in Application-x64.exe (the container's ctor).
// 0xe0 bytes — zeroed memory, two embedded UI::Sprites, then publishes the
// instance pointer to the cell 0x1401c3de8. GameMain allocates it
// (MemAlloc(0xe0, 4)); the init 0x140099db0 fills every field from the
// resource path named below.
//
// ENDGAME B.4 W0-H2: `GameContainer` is a real struct. CLASS NAME NOT SETTLED:
// retail's name is unconfirmed (the profile build lists `Globals.h` and
// `GameConfiguration.h` as candidates; Application-x64-profile.exe was not
// open in Ghidra). Field names are the resource each field is loaded from in
// 0x140099db0; types are the type id retail's ExternalList::Find passes (the
// five lists are gated on Object::ExternalList's type id, import slot
// 0x140151ef0). The struct is never constructed or destroyed as a C++ object:
// Game_Ctor fills raw MemAlloc memory in place and GameMain MemFrees it.

#include <cstdint>
#include <cstddef>
#include "object/External.h"   // Object::ExternalPtr<T>, Object::ExternalList
#include "ui/Sprite.h"         // UI::Sprite (by value at +0xA8 / +0xB8)

namespace UI    { class Cursor; class SpriteSheet; class StringTable; }
namespace Audio { class SoundEffect; }

struct GameContainer {
    Object::ExternalPtr<Object::ExternalList> achievementList;    // +0x00 "Game/Achievement/AchievementList.rsc" (AchievementManager::SetAchievements)
    Object::ExternalPtr<Object::ExternalList> scenarioList;       // +0x08 "Game/Scenario/ScenarioList.rsc"
    Object::ExternalPtr<Object::ExternalList> climateList;        // +0x10 "Game/Climate/ClimateList.rsc"
    Object::ExternalPtr<Object::ExternalList> startConditions;    // +0x18 "Game/StartCondition/StartConditions.rsc"
    Object::ExternalPtr<Object::ExternalList> terrainList;        // +0x20 "Game/Terrain/TerrainList.rsc"
    Object::ExternalPtr<UI::Cursor>           cursorArrow;        // +0x28 "Cursor/Arrow.rsc:cursor"
    Object::ExternalPtr<UI::Cursor>           cursorHand;         // +0x30 "Cursor/Hand.rsc:cursor"
    Object::ExternalPtr<UI::Cursor>           cursorHourglass;    // +0x38 "Cursor/Hourglass.rsc:cursor"
    Object::ExternalPtr<UI::Cursor>           cursorMove;         // +0x40 "Cursor/Move.rsc:cursor"
    Object::ExternalPtr<UI::Cursor>           cursorQuery;        // +0x48 "Cursor/Query.rsc:cursor"
    Object::ExternalPtr<UI::Cursor>           cursorPlace;        // +0x50 "Cursor/Place.rsc:cursor"
    Object::ExternalPtr<Audio::SoundEffect>   alertEffect;        // +0x58 "Audio/UI/AlertEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   buttonDownEffect;   // +0x60 "Audio/UI/ButtonDownEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   buttonUpEffect;     // +0x68 "Audio/UI/ButtonUpEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   beginDragEffect;    // +0x70 "Audio/UI/BeginDragEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   endDragEffect;      // +0x78 "Audio/UI/EndDragEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   placementMoveEffect;  // +0x80 "Audio/UI/PlacementMoveEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   placementThunkEffect; // +0x88 "Audio/UI/PlacementThunkEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   placementErrorEffect; // +0x90 "Audio/UI/PlacementErrorEffect.rsc"
    Object::ExternalPtr<Audio::SoundEffect>   warningEffect;      // +0x98 "Audio/UI/WarningEffect.rsc"
    Object::ExternalPtr<UI::SpriteSheet>      spriteSheet;        // +0xA0 "Dialog/SpriteSheet.rsc"
    UI::Sprite                                spriteWhite;        // +0xA8 Sprite(sheet, "White"); ctor 0x140001050 Sprite::Sprite(+0xA8)
    UI::Sprite                                spriteSaveLoadEmpty;  // +0xB8 Sprite(sheet, "SaveLoadEmpty"); Sprite::Sprite(+0xB8)
    Object::ExternalPtr<UI::StringTable>      gameKeys;           // +0xC8 "Dialog/StringTable.rsc:gameKeys"
    Object::ExternalPtr<UI::StringTable>      keyNames;           // +0xD0 "Dialog/StringTable.rsc:keyNames"
    Object::ExternalPtr<UI::StringTable>      globalNames;        // +0xD8 "Dialog/StringTable.rsc:globalNames"
};

static_assert(sizeof(GameContainer) == 0xE0, "GameContainer is 0xE0 (GameMain MemAlloc(0xe0, 4))");
static_assert(alignof(GameContainer) == 8, "GameContainer is qword-aligned");
static_assert(offsetof(GameContainer, achievementList) == 0x00, "achievementList @ +0x00");
static_assert(offsetof(GameContainer, scenarioList) == 0x08, "scenarioList @ +0x08");
static_assert(offsetof(GameContainer, climateList) == 0x10, "climateList @ +0x10");
static_assert(offsetof(GameContainer, startConditions) == 0x18, "startConditions @ +0x18");
static_assert(offsetof(GameContainer, terrainList) == 0x20, "terrainList @ +0x20");
static_assert(offsetof(GameContainer, cursorArrow) == 0x28, "cursorArrow @ +0x28");
static_assert(offsetof(GameContainer, cursorHand) == 0x30, "cursorHand @ +0x30");
static_assert(offsetof(GameContainer, cursorHourglass) == 0x38, "cursorHourglass @ +0x38");
static_assert(offsetof(GameContainer, cursorMove) == 0x40, "cursorMove @ +0x40");
static_assert(offsetof(GameContainer, cursorQuery) == 0x48, "cursorQuery @ +0x48");
static_assert(offsetof(GameContainer, cursorPlace) == 0x50, "cursorPlace @ +0x50");
static_assert(offsetof(GameContainer, alertEffect) == 0x58, "alertEffect @ +0x58");
static_assert(offsetof(GameContainer, buttonDownEffect) == 0x60, "buttonDownEffect @ +0x60");
static_assert(offsetof(GameContainer, buttonUpEffect) == 0x68, "buttonUpEffect @ +0x68");
static_assert(offsetof(GameContainer, beginDragEffect) == 0x70, "beginDragEffect @ +0x70");
static_assert(offsetof(GameContainer, endDragEffect) == 0x78, "endDragEffect @ +0x78");
static_assert(offsetof(GameContainer, placementMoveEffect) == 0x80, "placementMoveEffect @ +0x80");
static_assert(offsetof(GameContainer, placementThunkEffect) == 0x88, "placementThunkEffect @ +0x88");
static_assert(offsetof(GameContainer, placementErrorEffect) == 0x90, "placementErrorEffect @ +0x90");
static_assert(offsetof(GameContainer, warningEffect) == 0x98, "warningEffect @ +0x98");
static_assert(offsetof(GameContainer, spriteSheet) == 0xA0, "spriteSheet @ +0xA0");
static_assert(offsetof(GameContainer, spriteWhite) == 0xA8, "spriteWhite @ +0xA8");
static_assert(offsetof(GameContainer, spriteSaveLoadEmpty) == 0xB8, "spriteSaveLoadEmpty @ +0xB8");
static_assert(offsetof(GameContainer, gameKeys) == 0xC8, "gameKeys @ +0xC8");
static_assert(offsetof(GameContainer, keyNames) == 0xD0, "keyNames @ +0xD0");
static_assert(offsetof(GameContainer, globalNames) == 0xD8, "globalNames @ +0xD8");

extern "C" {

// Game_Ctor(self) — initializes the 0xe0-byte container in place.
// Maps to FUN_140001050 in the exe.
// Caller is responsible for the 0xe0-byte allocation.
GameContainer* Game_Ctor(GameContainer* self);

} // extern "C"

// The exe cell 0x1401c3de8 (B3R STORAGE): the live container. Game_Ctor
// (0x140001050, the cell's only retail writer at 0x1400010f1) publishes it.
// A namespace-scope pointer; was the file-static g_storage_KeyMapSingleton of
// standalone/address_registry.cpp, read through g_pKeyMapSingleton ("container
// / manager object, NOT the KeyMap"). Defined in game.cpp.
extern GameContainer* g_gameContainer;
