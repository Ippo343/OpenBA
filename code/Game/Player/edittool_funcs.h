#pragma once
// The EditTool helper the harnesses call by name. The EditTool slot bodies are
// members (EditTool.h; ENDGAME B.3.8 callers).

#include "EditTool.h"
#include <cstdint>

// Deselect + close the open info dialog — port of Entity_impl_cc90 (0x1400fcc90).
// Clears the selection highlight, drops tool+0x28/+0x30, closes the dialog via
// UI::UserInterface::CloseDialog (which destroys the DialogInstance + controllers).
// NOT registered at the exe address (it doubles as the per-frame deactivate slot;
// see standalone/reg_player_render.cpp). dialogPtr = resolved DialogInstance*.
// Original: 0x1400fcc90
void EditTool_CloseInfoDialog(EditTool* self, uintptr_t dialogPtr);
