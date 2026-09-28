// code/Game/Game/game_settings_persist.cpp
//
// The GameSettings <-> IO::Persistence bridge — retail's two halves of
// "remember what the player chose last time":
//
//   0x140098b60  GameSettings_Load  — Persistence::Get  x16, called by GameMain
//                                     right after the struct is default-filled
//   0x140098970  GameSettings_Save  — Persistence::Set  x16, called by GameMain
//                                     at shutdown and by OptionsDialog's Apply
//
// Neither was ported, so the standalone always came up on the hardcoded defaults
// and never wrote anything back. That is what made the five New Game combo picks
// (LastTerrain / LastMapSize / LastClimate / LastStart / LastDisaster) survive a
// quit-to-menu but not a restart: NewGameDialog's OK writes them into the
// settings struct, and ONLY GameSettings_Save moves the struct into Persistence.
// Persistence::WriteSettings — which game_main.cpp already calls at shutdown —
// flushes Persistence to disk, but it has nothing to flush until Set runs.
//
// Settings struct is 0x34 bytes. Field map, as int32 indices (retail passes an
// int* and indexes it, mixing in three bools at raw byte offsets 0x15/0x16/0x17):
//   [0] UIScale            [1] AutosaveInterval   [2] StatusIconOpacity
//   [4] StatusIconScale    [7] LastOption         [8] LastTerrain
//   [9] LastMapSize        [10] LastClimate       [11] LastStart
//   [12] LastDisaster
// and SIX bools at raw byte offsets, NOT ints (retail casts the int* slot to bool*):
//   byte 0x0C StatusIconEnable   byte 0x14 EdgeScroll   byte 0x18 AutoPause
//   byte 0x15 ShowCelcius        byte 0x16 ShowWeather  byte 0x17 AutoSlow

#include "options.h"
#include "io/Persistence.h"   // ENDGAME B.3.4a closing: Persistence::Get/Set members
#include <cstdint>
#include <cstdio>
#include "Game/exe_bodies.h"

namespace {

void* PersistenceSingleton() {
    return ::IO::Persistence::_singleton;
}

// The 16 keys in retail's order. Order matters only for log readability, but
// keeping it identical makes a diff against the decompile trivial.
struct IntKey  { const char* key; int index; };
struct BoolKey { const char* key; int byteOffset; };

const IntKey  kIntKeys[] = {
    { "UIScale", 0 }, { "AutosaveInterval", 1 }, { "StatusIconOpacity", 2 },
    { "StatusIconScale", 4 }, { "LastOption", 7 }, { "LastTerrain", 8 },
    { "LastMapSize", 9 }, { "LastClimate", 10 }, { "LastStart", 11 },
    { "LastDisaster", 12 },
};
// THREE of these were in kIntKeys until 2026-09-04 and are NOT ints. Retail
// (0x140098b60 / 0x140098970) passes `(bool*)(param_1 + 3)`, `(bool*)(param_1 + 5)`
// and `(bool*)(param_1 + 6)` — the bool& overload of Persistence::Get/Set, one byte
// at settings+0xC / +0x14 / +0x18. Reading them through the int& overload is a
// different Persistence key type, and the Set side wrote the key back as an INT,
// changing its stored type out from under retail and the Options dialog. The one
// that matters most is StatusIconEnable: settings byte +0xC is exactly what
// StatusIcon_Toggle checks before it will Graphics::Scene::AddObject an icon, so a
// wrong read there silently hides EVERY status icon in the game, citizen and
// building alike, with nothing logged anywhere.
const BoolKey kBoolKeys[] = {
    { "StatusIconEnable", 0x0C }, { "EdgeScroll", 0x14 }, { "AutoPause", 0x18 },
    { "ShowCelcius", 0x15 }, { "ShowWeather", 0x16 }, { "AutoSlow", 0x17 },
};

}  // namespace

// GameSettings_Load — faithful port of 0x140098b60.
extern "C" void GameSettings_Load(int32_t* settings) {
    if (!settings) return;
    void* p = PersistenceSingleton();
    if (!p) {
        fprintf(stderr, "[GameSettings] Load SKIPPED — persistence=%p\n", p);
        return;
    }
    // ENDGAME B.3.4a closing: onto IO::Persistence::Get. The DOOR names the
    // overload (int& / bool&), as §7.3 requires — the bool keys are one byte at
    // settings+0xC/+0x14/+0x18 and must not pick the int& overload.
    for (const auto& k : kIntKeys)
        reinterpret_cast<const IO::Persistence*>(p)->Get("Game", k.key, settings[k.index]);   // B.3.4 bridge
    for (const auto& k : kBoolKeys)
        reinterpret_cast<const IO::Persistence*>(p)->Get(
            "Game", k.key,
            *reinterpret_cast<bool*>(reinterpret_cast<uint8_t*>(settings) + k.byteOffset));   // B.3.4 bridge

    fprintf(stderr, "[GameSettings] Load: LastTerrain=%d LastMapSize=%d LastClimate=%d "
            "LastStart=%d LastDisaster=%d (UIScale=%d AutosaveInterval=%d)\n",
            settings[8], settings[9], settings[10], settings[11], settings[12],
            settings[0], settings[1]);
    fprintf(stderr, "[GameSettings] Load: StatusIconEnable=%d (settings+0xC) -- 0 hides "
            "EVERY status icon (citizen health, building warnings); it is the Options "
            "checkbox, persisted as GameStatusIconEnable under "
            "HKCU/Software/Shining Rock Software LLC/Banished\n",
            (int)*(reinterpret_cast<uint8_t*>(settings) + 0x0C));
    fflush(stderr);
}

// GameSettings_Save — faithful port of 0x140098970.
extern "C" void GameSettings_Save(int32_t* settings) {
    if (!settings) return;
    void* p = PersistenceSingleton();
    if (!p) {
        fprintf(stderr, "[GameSettings] Save SKIPPED — persistence=%p\n", p);
        return;
    }
    // ENDGAME B.3.4a closing: onto IO::Persistence::Set, DOOR-typed as above.
    for (const auto& k : kIntKeys)
        reinterpret_cast<IO::Persistence*>(p)->Set("Game", k.key, settings[k.index]);   // B.3.4 bridge
    for (const auto& k : kBoolKeys)
        reinterpret_cast<IO::Persistence*>(p)->Set(
            "Game", k.key,
            *(reinterpret_cast<uint8_t*>(settings) + k.byteOffset) != 0);   // B.3.4 bridge

    fprintf(stderr, "[GameSettings] Save: LastTerrain=%d LastMapSize=%d LastClimate=%d "
            "LastStart=%d LastDisaster=%d\n",
            settings[8], settings[9], settings[10], settings[11], settings[12]);
    fflush(stderr);
}
