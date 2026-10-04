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

// The harnesses' EditTool entry points (defined in edittool.cpp; no retail address:
// standalone drivers). B4 P2 SA: declared here once, in place of each harness's local
// `extern "C"` copy.
extern "C" bool      EditTool_IsEditTool(void* tool);
extern "C" EditTool* EditTool_HarnessAcquire(PlayerToolManager* entityUI);
extern "C" int       EditTool_HarnessSelectEntity(EditTool* editTool, ComponentEntity* entity);
extern "C" int       EditTool_HarnessSelectCitizen(EditTool* editTool);
extern "C" int       EditTool_HarnessSelectNearby(EditTool* editTool);

// The harness factories of the two build tools (placementtool.cpp / removaltool.cpp,
// `CreateNewObject` with C linkage; standalone/tool_harness.cpp is the caller).
extern "C" void* PlacementTool_Factory_SA(void);
extern "C" void* RemovalTool_Factory_SA(void);
