#pragma once
// SpriteCaptureUI C bodies (defined in spritecaptureui.cpp). Split out of ui_panels.h
// (B.4 R0-EXE).

#include "Dialog/SpriteCaptureUI.h"

// Serialize (vtable +0x20): Finds editCapture/buttonCapture/captureBox elements,
// sets button ID 1000, sets default capture path, initializes capture state.
void SpriteCaptureUI_Serialize(SpriteCaptureUI* self);

// OnCreate (vtable +0x40): Per-frame update — drag capture box with left mouse + key 0x3E.
// When pendingExternal set and saveFrameCount reaches 1, EndSpriteCapture + SaveText.
void SpriteCaptureUI_OnCreate(SpriteCaptureUI* self);
