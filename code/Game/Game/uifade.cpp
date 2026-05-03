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
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>

namespace UIFade {

// UI::UserInterface field offsets — confirmed via decompilation of
// Runtime 0x18011F500 (SetFullscreenFade) and 0x180121890 (Draw).
namespace UIOff {
    constexpr uintptr_t fadeSpriteSheet = 0xE0;  // void* — sprite resource ptr
    constexpr uintptr_t fadeSpriteIndex = 0xE8;  // int    — index into sheet, -1 = invalid
    constexpr uintptr_t fadeRate        = 0xF0;  // float  — ±1.0 or 0.0
    constexpr uintptr_t fadeAlpha       = 0xF4;  // float  — current alpha [0..1]
    constexpr uintptr_t fadeColor       = 0xF8;  // uint32 — packed 0xRRGGBBAA, alpha overridden
}

// KeyMap field offset for the UI fade sprite (16 bytes — Sprite resource +
// index pair, format matches what SetFullscreenFadeSprite expects).
// Offset confirmed via OptionsDialog reimpl (code/Game/Dialog/optionsdialog.cpp):
//   *(u64*)&sprite[0] = *(u64*)(km + 0xA8);   // sprite sheet ptr
//   *(u64*)&sprite[8] = *(u64*)(km + 0xB0);   // sprite index (low half)
constexpr uintptr_t kKeyMapFadeSpriteOffset = 0xA8;
constexpr size_t    kFadeSpriteBlobBytes    = 16;

float GetCurrentAlpha(const void* userInterface) {
    if (!userInterface) return 0.0f;
    return *reinterpret_cast<const float*>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeAlpha);
}

float GetCurrentRate(const void* userInterface) {
    if (!userInterface) return 0.0f;
    return *reinterpret_cast<const float*>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeRate);
}

bool EnsureFadeSprite(void* userInterface) {
    if (!userInterface) return false;

    // Skip if already non-zero (idempotent — Startup may run multiple times
    // across loops).
    void* existing = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeSpriteSheet);
    if (existing) return true;

    if (!fn_UI_SetFullscreenFadeSprite) {
        // Resolved at engine_funcs init — if missing, the Runtime export
        // changed names. Defer silently; fades just won't render.
        return false;
    }

    void* keyMap = (g_pKeyMapSingleton ? *g_pKeyMapSingleton : nullptr);
    if (!keyMap) return false;

    // Copy the 16-byte Sprite blob from KeyMap+0xA8 into a local buffer; the
    // setter then writes UI+0xE0 (qword[0]) and UI+0xE8 (qword[1]) from it.
    uint8_t spriteBlob[kFadeSpriteBlobBytes];
    *reinterpret_cast<uint64_t*>(&spriteBlob[0]) =
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(keyMap) + kKeyMapFadeSpriteOffset);
    *reinterpret_cast<uint64_t*>(&spriteBlob[8]) =
        *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(keyMap) + kKeyMapFadeSpriteOffset + 8);

    fn_UI_SetFullscreenFadeSprite(userInterface, spriteBlob);

    void* installedSheet = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeSpriteSheet);
    int32_t installedIdx = *reinterpret_cast<int32_t*>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeSpriteIndex);
    printf("[UIFade::EnsureFadeSprite] ui=%p sheet=%p idx=%d "
           "(KeyMap+0x%02x src=%p,%lld)\n",
           userInterface, installedSheet, installedIdx,
           (unsigned)kKeyMapFadeSpriteOffset,
           *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(keyMap) + kKeyMapFadeSpriteOffset),
           (long long)*reinterpret_cast<int64_t*>(
               reinterpret_cast<uintptr_t>(keyMap) + kKeyMapFadeSpriteOffset + 8));

    // Sheet may be null in standalone if KeyMap hasn't yet had ui/fadeSprite
    // loaded — caller will see fades as "alpha set but nothing drawn".
    return installedSheet != nullptr;
}

void TriggerReveal(void* userInterface, uint32_t color) {
    if (!userInterface || !fn_UI_SetFullscreenFade) return;

    // 1. Instant set — alpha jumps to 1.0 (opaque) so the very next Draw
    //    fully covers whatever the loop renders behind it.
    fn_UI_SetFullscreenFade(userInterface, /*fadeIn=*/true,  color, /*instant=*/true);
    // 2. Animated reverse — rate becomes -1.0; UI::Update advances alpha
    //    from 1.0 toward 0.0 over ~0.25s, revealing the loop.
    fn_UI_SetFullscreenFade(userInterface, /*fadeIn=*/false, color, /*instant=*/false);

    printf("[UIFade::TriggerReveal] ui=%p color=0x%08x alpha=%.3f rate=%.3f\n",
           userInterface, color, GetCurrentAlpha(userInterface), GetCurrentRate(userInterface));
}

void TriggerFadeIn(void* userInterface, uint32_t color) {
    if (!userInterface || !fn_UI_SetFullscreenFade) return;
    // Animated forward — rate becomes +1.0; UI::Update advances alpha from
    // current value toward 1.0 over ~0.25s. Caller typically sequences this
    // before transitioning to the next loop, so the screen fades to black
    // first.
    fn_UI_SetFullscreenFade(userInterface, /*fadeIn=*/true, color, /*instant=*/false);
    printf("[UIFade::TriggerFadeIn] ui=%p color=0x%08x alpha=%.3f rate=%.3f\n",
           userInterface, color, GetCurrentAlpha(userInterface), GetCurrentRate(userInterface));
}

void LogState(const char* tag, const void* userInterface) {
    if (!userInterface) return;
    float alpha = GetCurrentAlpha(userInterface);
    float rate  = GetCurrentRate(userInterface);
    void* sheet = *reinterpret_cast<void* const*>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeSpriteSheet);
    int32_t idx = *reinterpret_cast<const int32_t*>(
        reinterpret_cast<uintptr_t>(userInterface) + UIOff::fadeSpriteIndex);
    printf("[UIFade::%s] ui=%p alpha=%.3f rate=%.3f sheet=%p idx=%d\n",
           (tag ? tag : ""), userInterface, alpha, rate, sheet, idx);
}

} // namespace UIFade
