// standalone/reg_player_edit2.cpp
// Player registration wave, round 2c — batch "edit2" registrar.
//
// Both original targets are settled elsewhere — this registrar stays intentionally
// EMPTY. (Historical closure analysis: code/Game/Player/tool_base_edit2.cpp.)
//
// 0x1400fcc90 Entity_impl_cc90 — DELIBERATELY UNREGISTERED, permanently: it doubles
//   as the EditTool per-frame deactivate vtable slot, so a registered body would wipe
//   the selection outline one frame after c9e0 submits it. Canonical rationale in
//   reg_player_render.cpp. (The EdgeMesh closure originally cited here — c4a0/b930/
//   b440/b10e0/7080 — has since been ported in tool_base_render.cpp, so closure is
//   no longer the blocker.)
//
// 0x140100380 Entity_impl_0380 — REGISTERED by reg_player_screenrect.cpp (r2d). The
//   Math-slot sub-wave this file called for was solved differently: the reimpl bodies
//   resolve every value-return Math export DIRECTLY via ResolveExport, and the .rdata
//   dispatch slots stay deliberately unregistered (slot 0x140152780 is mislabeled
//   "iatMemAlloc" in two Work files — see reg_player_screenrect.cpp).

void RegisterPlayerEdit2Batch() {
    // intentionally empty — cc90 deliberately-null (reg_player_render.cpp),
    // 0380 landed in reg_player_screenrect.cpp
}
