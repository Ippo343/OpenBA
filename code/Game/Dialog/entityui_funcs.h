#pragma once
// EntityUI C bodies (defined in entityui.cpp). Split out of ui_panels.h (B.4 R0-EXE).

#include "Dialog/EntityUI.h"

// Slot8 (vtable +0x40): Input handler — reads mouse position, checks key 5 press,
// selects entity in edit tool if mouse is over this dialog.
void EntityUI_Slot8(EntityUI* self);
