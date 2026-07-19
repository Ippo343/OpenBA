// standalone/reg_edittool_select.cpp
// Registers the ported EditTool "select entity -> open info dialog" so its
// engine_funcs passthrough resolves to our reimpl in the standalone exe:
//   0x1400fd3b0  Entity_impl_d3b0  ->  EditTool_SelectEntity_Impl
// Body lives in code/Game/Dialog/edittool_select.cpp.
//
// fn_EditTool_SelectEntity (engine_funcs) reads ResolveAddr(Exe, 0x1400fd3b0) in
// EngineFuncsInit, which runs AFTER AddressRegistryInit — so registering here makes
// EditTool::OnKey's click-to-select path (code/Game/Player/edittool.cpp:517) route to
// our port and open the clicked entity's DialogInstance.
//
// Closure (bottom-up): the main body needs only Runtime exports already wired through
// engine_funcs (fn_UI_GetFirstDialog, fn_DialogInstance_GetController, fn_DialogInstance_Ctor,
// fn_UI_AddDialog, fn_IsDerivedFrom, fn_InternalBasePtr_Ctor/_CheckPtr, fn_MemAlloc) plus
// g_ToolUI_ChildDialog_Vtable (RegisterPlayerToolUIBatch) and the EntityUI/UIDescription/
// UIComponent type-ids (filled lazily via Object::GetTypeIdByName inside the body).
// The body resolves the two teardown/refresh helpers as exe pointers and null-guards
// them, so the dialog opens fine with either unresolved:
//   0x1400fc9e0 (Entity_impl_c9e0, select/paint) — ported in tool_base_render.cpp,
//     registered by RegisterPlayerRenderBatch only under BANISHED_SELECTION_OUTLINE;
//     with the outline off it stays null and that step is skipped.
//   0x1400fcc90 (Entity_impl_cc90, deselect/clear) — DELIBERATELY NEVER registered:
//     it doubles as the EditTool per-frame deactivate vtable slot and would wipe the
//     outline every frame (see reg_player_render.cpp); clear-on-reselect is done via
//     cb40 in this body's d3b0 step 6 instead.
#include "resolve.h"
#include <cstdint>

extern "C" void EditTool_SelectEntity_Impl(void*, void*);  // 0x1400fd3b0

void RegisterEditToolSelectBatch() {
    RegisterExeAddr(0x1400fd3b0, reinterpret_cast<uintptr_t>(&EditTool_SelectEntity_Impl));
}
