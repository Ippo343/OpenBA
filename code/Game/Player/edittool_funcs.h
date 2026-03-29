#pragma once
// Function declarations for deeply reimplemented EditTool methods.
// Called from engine/reimpl/edittool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

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
// Casts ray from cursor, finds entity under cursor via Picking::Scene::Intersect,
// then calls Entity_impl_d3b0 to select/deselect entities.
// Currently delegates to trampoline — requires many unresolved engine APIs:
//   FUN_1401005a0 (camera ray), QuarryComponent_impl_a530 (terrain pick),
//   FUN_1400a0280 (world grid entity lookup), Picking::Scene::Intersect,
//   Picking::Scene::GetUserData, Entity_impl_d3b0 (entity select handler).
// Original: 0x1400fd030

// Startup (vtable +0x38, Slot7): Initialization and dialog positioning.
// Gets dialog controller, finds ToolUI and PageToggleUI controllers,
// positions dialog near clicked entity. Complex UI positioning logic.
// Currently delegates to trampoline — requires many unresolved engine APIs:
//   DialogInstance::GetController, Entity_impl_0380 (entity bounding rect),
//   Element::GetPaddedSize, DialogInstance::SetUpperLeft,
//   EditTool_helper_6800 (toggle page setup).
// Original: 0x1400fcda0
