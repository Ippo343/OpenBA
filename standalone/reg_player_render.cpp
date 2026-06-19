// standalone/reg_player_render.cpp
// Player registration wave, round 2d — batch "render" registrar.
//
// VERDICT: target DEFERRED — this registrar is intentionally EMPTY.
// See code/Game/Player/tool_base_render.cpp for the full closure analysis.
//
// 0x1400fcc90 Entity_impl_cc90 — DEFER. Closure transits Entity_impl_c4a0
//   (0x14004c4a0, render mutation against the live Graphics::Scene) ->
//   TradeWork_impl_b930 (0x14004b930, ~250-line EdgeMesh tessellation/vertex-buffer
//   builder). b930 cannot be ported 1:1 with confidence: CONCAT44 count/capacity
//   packing aliased with ushort* views, multiple inlined array-growth loops with
//   divergent strides, and a ~28-argument Graphics::EdgeMesh::Set whose arg recovery
//   is unreliable. Per the wave SAFETY RULE, deferring b930 defers its whole consumer
//   chain (c4a0 -> cb40 -> cc90).
//
//   Also unregistered in the closure (to land in the dedicated sub-wave, bottom-up):
//     exe code : 0x140007080 0x1400074a0 0x14004b440 0x1400b10e0 (portable leaves)
//                0x14004b930 0x14004c4a0 0x1400fcb40 0x1400fcc90
//     slots    : 0x140151378 0x140151390 0x140152b60 0x140152b68 (all -> Runtime exports)
//
// Registering cc90 with b930 half-ported is silent corruption on a DORMANT (edit-tool
// overlay) path the A/B test cannot catch — strictly worse than the safe no-op.
#ifdef STANDALONE_EXE

void RegisterPlayerRenderBatch() {
    // intentionally empty — 0x1400fcc90 DEFERRED (see header / tool_base_render.cpp)
}
#endif
