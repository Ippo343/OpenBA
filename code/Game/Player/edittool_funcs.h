#pragma once
// Function declarations for deeply reimplemented EditTool methods.
// Called from engine/reimpl/edittool.cpp wrappers.

#include "EditTool.h"
#include <cstdint>

// Dtor (vtable +0x00, Slot0): Scalar deleting destructor.
// Resets vtable chain (EditTool -> Tool), calls Object::Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x14002db00
void* EditTool_Dtor_Impl(EditTool* self, uint32_t flags);

// Serialize (vtable +0x50, Slot10): Compare entity reference.
// Calls CheckPtr on self->lastEntityRef (+0x30), then compares
// *param_2 with self->lastEntityRef. Returns true if they match.
// Original: 0x1400fd290
bool EditTool_Serialize_Impl(EditTool* self, uint64_t* param_2);

// OnKey (vtable +0x30, Slot6): Entity selection via picking.
// Casts ray from cursor, finds entity under cursor via Picking::Scene::Intersect
// and map hit test. Calls fn_EditTool_SelectEntity to select/deselect entities.
// Original: 0x1400fd030
uint64_t EditTool_OnKey_Impl(EditTool* self, char param2);

// Startup (vtable +0x38, Slot7): Initialization and dialog positioning.
// Gets dialog controller, finds ToolUI and PageToggleUI controllers,
// positions dialog near clicked entity using entity screen rect.
// Also runs the retail close-on-entity-death branch: an unpinned info panel
// whose entity died is deselected + closed via EditTool_CloseInfoDialog.
// Original: 0x1400fcda0
void EditTool_Startup_Impl(EditTool* self);

// Deselect + close the open info dialog — port of Entity_impl_cc90 (0x1400fcc90).
// Clears the selection highlight, drops tool+0x28/+0x30, closes the dialog via
// UI::UserInterface::CloseDialog (which destroys the DialogInstance + controllers).
// NOT registered at the exe address (it doubles as the per-frame deactivate slot;
// see standalone/reg_player_render.cpp). dialogPtr = resolved DialogInstance*.
// Original: 0x1400fcc90
void EditTool_CloseInfoDialog(EditTool* self, uintptr_t dialogPtr);
