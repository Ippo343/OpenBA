// code/Game/Game/uifade.cpp
// Implementation of the fullscreen-fade overlay helpers used by the standalone
// loop stubs. See uifade.h for the high-level rationale.
//
// All work delegates to UI::UserInterface methods that already live in
// Runtime-x64.dll:
//   - SetFullscreenFadeSprite (Runtime 0x18011F560) writes UI+0xE0 (sprite ptr)
//     and UI+0xE8 (sprite index) from a 16-byte UI::Sprite blob.
//   - SetFullscreenFade       (Runtime 0x18011F500) writes UI+0xF0 (rate),
//     UI+0xF4 (alpha), UI+0xF8 (color/duration aliased) per call mode.
// Runtime per-frame Update advances UI+0xF4 toward 0 or 1 over ~0.25s, and
// Runtime Draw renders the overlay sprite if alpha != 0 and the sprite is set.
//
// The fade sprite is loaded into the engine-wide KeyMap singleton at +0xA8/+0xB0
// during Runtime bootstrap (KeyMap_Ctor pulls "ui/fadeSprite" or similar from
// the global UI .rsc). We just copy those 16 bytes into the UI struct.

#include "Game/uifade.h"
#include "Game/game.h"   // B3R STORAGE: g_gameContainer
#include <cstdint>
#include <cstdio>
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K4)
#include "ui/Sprite.h"          // ENDGAME B.3.6 UI callers (K4)

namespace UIFade {

// The fields are UI::UserInterface's (ui/UserInterface.h): _fadeSpriteSheet
// +0xE0, _fadeSpriteIndex +0xE8, _fadeVelocity +0xF0 (the rate, +-1.0 or 0.0),
// _fadeAmount +0xF4 (the alpha), _fadeColor +0xF8 — confirmed via Runtime
// 0x18011F500 (SetFullscreenFade) and 0x180121890 (Draw). The callers
// (standalone's loop stubs) hold the UI as `void*`: each entry takes one view.
static const UI::UserInterface* UiOf(const void* userInterface) {
    return reinterpret_cast<const UI::UserInterface*>(userInterface);   // LEFT: void* parameter of the STANDALONE loop stubs
}
static UI::UserInterface* UiOf(void* userInterface) {
    return reinterpret_cast<UI::UserInterface*>(userInterface);         // LEFT: void* parameter of the STANDALONE loop stubs
}

// The fade sprite is GameContainer::spriteWhite (+0xA8, the 16-byte UI::Sprite
// SetFullscreenFadeSprite expects; optionsdialog.cpp copies the same two qwords).
constexpr uintptr_t kKeyMapFadeSpriteOffset = 0xA8;   // (for the log line)

float GetCurrentAlpha(const void* userInterface) {
    if (!userInterface) return 0.0f;
    return UiOf(userInterface)->_fadeAmount;
}

float GetCurrentRate(const void* userInterface) {
    if (!userInterface) return 0.0f;
    return UiOf(userInterface)->_fadeVelocity;
}

bool EnsureFadeSprite(void* userInterface) {
    if (!userInterface) return false;
    UI::UserInterface* ui = UiOf(userInterface);

    // Skip if already non-zero (idempotent — Startup may run multiple times
    // across loops).
    void* existing = ui->_fadeSpriteSheet;
    if (existing) return true;

    GameContainer* keyMap = (g_gameContainer);
    if (!keyMap) return false;

    // Copy the 16-byte Sprite from GameContainer+0xA8 into a local; the
    // setter then writes UI+0xE0 (qword[0]) and UI+0xE8 (qword[1]) from it.
    UI::Sprite spriteBlob = keyMap->spriteWhite;

    ui->SetFullscreenFadeSprite(&spriteBlob);

    void* installedSheet = ui->_fadeSpriteSheet;
    int32_t installedIdx = ui->_fadeSpriteIndex;
    // (the log's second source value is the sprite's second qword: index | pad << 32)
    printf("[UIFade::EnsureFadeSprite] ui=%p sheet=%p idx=%d "
           "(KeyMap+0x%02x src=%p,%lld)\n",
           userInterface, installedSheet, installedIdx,
           (unsigned)kKeyMapFadeSpriteOffset,
           keyMap->spriteWhite.spriteSheet,
           (long long)(static_cast<uint64_t>(static_cast<uint32_t>(keyMap->spriteWhite.spriteIndex)) |
                       (static_cast<uint64_t>(static_cast<uint32_t>(keyMap->spriteWhite.pad0C)) << 32)));

    // Sheet may be null in standalone if KeyMap hasn't yet had ui/fadeSprite
    // loaded — caller will see fades as "alpha set but nothing drawn".
    return installedSheet != nullptr;
}

void TriggerReveal(void* userInterface, uint32_t color) {
    if (!userInterface) return;

    // 1. Instant set — alpha jumps to 1.0 (opaque) so the very next Draw
    //    fully covers whatever the loop renders behind it.
    UiOf(userInterface)->SetFullscreenFade(/*fadeIn=*/true,  color, /*instant=*/true);
    // 2. Animated reverse — rate becomes -1.0; UI::Update advances alpha
    //    from 1.0 toward 0.0 over ~0.25s, revealing the loop.
    UiOf(userInterface)->SetFullscreenFade(/*fadeIn=*/false, color, /*instant=*/false);

    printf("[UIFade::TriggerReveal] ui=%p color=0x%08x alpha=%.3f rate=%.3f\n",
           userInterface, color, GetCurrentAlpha(userInterface), GetCurrentRate(userInterface));
}

void TriggerFadeIn(void* userInterface, uint32_t color) {
    if (!userInterface) return;
    // Animated forward — rate becomes +1.0; UI::Update advances alpha from
    // current value toward 1.0 over ~0.25s. Caller typically sequences this
    // before transitioning to the next loop, so the screen fades to black
    // first.
    UiOf(userInterface)->SetFullscreenFade(/*fadeIn=*/true, color, /*instant=*/false);
    printf("[UIFade::TriggerFadeIn] ui=%p color=0x%08x alpha=%.3f rate=%.3f\n",
           userInterface, color, GetCurrentAlpha(userInterface), GetCurrentRate(userInterface));
}

void LogState(const char* tag, const void* userInterface) {
    if (!userInterface) return;
    float alpha = GetCurrentAlpha(userInterface);
    float rate  = GetCurrentRate(userInterface);
    void* sheet = UiOf(userInterface)->_fadeSpriteSheet;
    int32_t idx = UiOf(userInterface)->_fadeSpriteIndex;
    printf("[UIFade::%s] ui=%p alpha=%.3f rate=%.3f sheet=%p idx=%d\n",
           (tag ? tag : ""), userInterface, alpha, rate, sheet, idx);
}

} // namespace UIFade
