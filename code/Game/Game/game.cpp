// code/Game/Game/game.cpp
// Clean reimpl of the exe-internal Game/KeyMap container ctor.
//
// Original: FUN_140001050 (Application-x64.exe).
// Decompiled body (paraphrased):
//   - zero param_1[0..0x14] (21 qwords)
//   - UI::Sprite::Sprite(param_1 + 0x15)   // 16 bytes at slot 0x15
//   - UI::Sprite::Sprite(param_1 + 0x17)   // 16 bytes at slot 0x17
//   - param_1[0x19] = param_1[0x1a] = param_1[0x1b] = 0
//   - g_pKeyMapSingleton = param_1
//
// Total struct size: 0x1c qwords = 0xe0 bytes. The slot index 0x15..0x18 holds
// two 16-byte UI::Sprite values.

#include "game.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

extern "C" void* Game_Ctor(void* self) {
    // Zero the entire 0xe0-byte struct, including the regions where the
    // sprites will be initialized — Sprite default ctor is itself a 16-byte
    // zero-init in the engine, so this is safe.
    std::memset(self, 0, 0xe0);

    // Two embedded UI::Sprite default ctors at slot 0x15 and slot 0x17.
    auto base = reinterpret_cast<uint64_t*>(self);
    if (fn_Sprite_DefaultCtor2) {
        fn_Sprite_DefaultCtor2(&base[0x15]);
        fn_Sprite_DefaultCtor2(&base[0x17]);
    }

    // Publish to global KeyMap singleton storage (DAT_1401c3de8).
    if (g_pKeyMapSingleton) {
        *g_pKeyMapSingleton = self;
    }
    return self;
}
