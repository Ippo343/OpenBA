// standalone/reg_player_render.cpp
// Player registration wave, round 2d — batch "render" registrar.
//
// Entity SELECTION-OUTLINE (the retail yellow outline around a selected/followed
// entity). The full HighlightComponent EdgeMesh tessellation chain is ported in
// code/Game/Player/tool_base_render.cpp (b930 tessellator + c4a0 render mutation
// + the c9e0/cb40/7080/b440/b10e0/74a0 closure). This registrar wires the entry
// points our other reimplemented code reaches by exe address:
//
//   0x1400fc9e0  Entity_impl_c9e0  -> HighlightSelect_Impl   (select finalize; the
//                one edittool_select.cpp resolves as s_fn_c9e0 to paint the outline)
//   0x14004c4a0  Entity_impl_c4a0  -> HighlightRepaint_Impl  (rebuild+resubmit mesh)
//   0x1400fcb40  Entity_impl_cb40  -> HighlightClear_Impl    (clear highlight flag)
//
// *** DEFAULT ON — BANISHED_SELECTION_OUTLINE is in the default-on table ***
// (component_factory_base.cpp). The env var is now only a KILL-SWITCH: set it to 0
// to un-register the three entry points, which makes s_fn_c9e0/s_fn_cb40 in
// edittool_select.cpp null and reduces the select path to "info dialog opens, no
// overlay".
//
// This comment used to say DEFAULT OFF and was stale — it cost real debugging time
// on 2026-07-26, when the bridge info-dialog crash was initially ruled out as
// "the outline can't be running, it's opt-in". It was running. Whenever a gate is
// graduated into the default-on table, fix its registrar's header too.
//
// The caution the old note recorded still applies and is why the flag matters:
// b930 mutates the live Graphics::Scene, so a wrong tessellation is silent
// corruption. A/B with BANISHED_SELECTION_OUTLINE=0.
//
// The internal closure (b930, b440, b10e0, 74a0, 7080, cb40) is NOT registered
// separately — those are direct C++ calls within the ported TU. (b10e0 is also
// registered by RegisterPlayerTileRenderBatch for the placement footprint grid;
// registering it here too would double-register.)
#include "resolve.h"
#include <cstdint>
#include <cstdlib>

extern "C" void HighlightSelect_Impl(void*);    // 0x1400fc9e0
extern "C" void HighlightClear_Impl(void*);     // 0x1400fcb40
extern "C" void HighlightRepaint_Impl(void*);   // 0x14004c4a0
extern "C" void HighlightDeselect_Impl(void*);  // 0x1400fcc90

void RegisterPlayerRenderBatch() {
    RegisterExeAddr(0x1400fc9e0, reinterpret_cast<uintptr_t>(&HighlightSelect_Impl));
    RegisterExeAddr(0x1400fcb40, reinterpret_cast<uintptr_t>(&HighlightClear_Impl));
    RegisterExeAddr(0x14004c4a0, reinterpret_cast<uintptr_t>(&HighlightRepaint_Impl));
    // NOTE: cc90 (0x1400fcc90, deselect) is deliberately NOT registered. The clear-
    // on-reselect is done in edittool_select.cpp's d3b0 step 6 (via cb40) instead,
    // which fires only when a NEW selection actually completes. cc90 is ALSO the
    // EditTool deactivate vtable slot (0x1401ee710), which the tool invokes EVERY
    // FRAME in standalone — if cc90 were registered, that per-frame call would run
    // its cb40 and wipe the just-set highlight one frame after c9e0 submits it
    // (observed: c9e0=1, cc90=8973, citizen outline removed the frame after
    // SUBMITTED). Leaving 0x1400fcc90 unresolved makes that per-frame slot a no-op.
    // (void)&HighlightDeselect_Impl;  // still exported for DLL-mode / documentation
}
