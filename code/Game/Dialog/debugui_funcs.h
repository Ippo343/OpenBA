#pragma once
// DebugUI C bodies (defined in debugui.cpp). Split out of ui_panels.h (B.4 R0-EXE).

#include "Dialog/DebugUI.h"
#include <cstdint>

// Slot9 (vtable +0x48): Event dispatch — handles button clicks (resource spawn,
// disasters, debug toggles), combo changes (disease/draw), slider changes (day/colors).
void DebugUI_Slot9(DebugUI* self, uint64_t param2, int param3, int param4);

// ToggleDebugFlag: helper for checkbox cases in Slot9 — reads checkbox state,
// sets/clears a bit in game world debug flags at game+0xD0.
void DebugUI_ToggleDebugFlag(void* checkboxElement, uint32_t flag);
