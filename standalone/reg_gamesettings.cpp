// standalone/reg_gamesettings.cpp
// GameSettings <-> IO::Persistence bridge.
//
// addr          symbol               one-liner
// -----------   ------------------   ---------------------------------------------
// 0x140098970   GameSettings_Save    Persistence::Set x16 (GameMain shutdown, Options Apply)
// 0x140098b60   GameSettings_Load    Persistence::Get x16 (GameMain startup)
//
// Body: code/Game/Game/game_settings_persist.cpp
#include "resolve.h"

extern "C" void GameSettings_Save(int*);   // 0x140098970
extern "C" void GameSettings_Load(int*);   // 0x140098b60

void RegisterGameSettingsPersist() {
    RegisterExeAddr(0x140098970, reinterpret_cast<uintptr_t>(&GameSettings_Save));
    RegisterExeAddr(0x140098b60, reinterpret_cast<uintptr_t>(&GameSettings_Load));
}
