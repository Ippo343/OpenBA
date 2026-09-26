#pragma once
// Function declarations for deeply reimplemented EditTool methods.
// Called from engine/reimpl/edittool.cpp wrappers.

#include "EditTool.h"
#include <cstdint>

// Slot10 (vtable +0x50): "is *entityRef the entity I currently have selected?"
// Calls CheckPtr on self->lastEntityRef (+0x30), then compares *entityRef with it.
// NOT a serializer — it was called EditTool_Serialize_Impl until 2026-07-31, and a
// "Serialize" surfacing in a citizen-death teardown stack cost real diagnosis time.
// Callers MUST pass entityRef; it is dereferenced. A one-arg call site in
// nomadcomponent.cpp left RDX stale and faulted inside this function.
// Original: 0x1400fd290
bool EditTool_IsSelectedEntity(EditTool* self, uint64_t* entityRef);

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
