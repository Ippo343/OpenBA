// standalone/reg_player_edit2.cpp
// Player registration wave, round 2c — batch "edit2" registrar.
//
// VERDICT: both targets DEFERRED — this registrar is intentionally EMPTY.
// See code/Game/Player/tool_base_edit2.cpp for the full closure analysis.
//
// 0x1400fcc90 Entity_impl_cc90 — DEFER: closure transits Entity_impl_c4a0
//   (0x14004c4a0, render mutation) -> TradeWork_impl_b930 (0x14004b930, ~250-line
//   EdgeMesh tessellation builder) -> TradeWork_impl_b440 + FUN_1400b10e0 (both UNREG).
//   Also needs thunk_helper_d250_sub_7080 (0x140007080, UNREG leaf).
//
// 0x140100380 Entity_impl_0380 — DEFER: exe-code closure is portable (sub-callees
//   0x1400fec70 + 0x1400fe960 + 0x140098210 portable; 0x140004680/0x140003f60 reg),
//   BUT all Math ops route through 15 UNREGISTERED dispatch slots (value-return
//   Vector2/Vector3/Vector4/Matrix/Transform3/Box/Rectangle ops). Needs a Math-slot
//   sub-wave (RegisterDispatchSlot each -> Runtime export) before it can be landed.
//
// Registering either with its closure half-null is silent corruption on a dormant
// (edit-tool) path the A/B test cannot catch — strictly worse than the safe no-op.
#ifdef STANDALONE_EXE

void RegisterPlayerEdit2Batch() {
    // intentionally empty — both addrs DEFERRED (see header / tool_base_edit2.cpp)
}
#endif
