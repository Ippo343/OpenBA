#pragma once
// NoiseUI C bodies (defined in noiseui.cpp). Split out of ui_panels.h (B.4 R0-EXE).

#include "Dialog/NoiseUI.h"

// Slot4 (vtable +0x20): Allocates 256x256 ImageBuffer, fills black,
// creates MaterialInstance/SpriteSheet, finds 5 slider elements
// ("noisePersistance", "noiseOctaves", "noiseScale", "noiseCutOff",
// "noiseGradient") and "buttonGenerate" button (ID 1000).
void NoiseUI_Slot4(NoiseUI* self);

// Slot9 (vtable +0x48): On button click (msg=0x12, id=1000),
// regenerates Perlin noise texture using current slider values.
void NoiseUI_Slot9(NoiseUI* self, void* param2, int msg, int id,
                   int param5, int param6);
