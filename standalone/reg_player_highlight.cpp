// standalone/reg_player_highlight.cpp
// Player wave 5 (TIER-2 geometry) — PlacementTool radius-highlight registration.
//
// *** HELD (no-op) — DO NOT register until ace40 is verified. ***
//
// The HighlightAdd (0x140055430) closure is 8 functions:
//   55430 HighlightAdd, b0d80 slot-manager, and the AVL BST family 1f6b0/1fc00/4b40/
//   1f850/14b00 — all hand-ported + reviewed in code/Game/Player/tool_base_highlight.cpp
//   (worker-thread-free; shares rotations 15000/14ed0 with reg_player_trade, and
//   fa80/1f8d0/ee40/acc10/ae180 with the pf waves) — PLUS the large radius tessellation
//   ace40 (0x1400ace40), still under verification (opus draft at
//   notes/wave5_drafts/ace40.draft.cpp).
//
// ace40 is NOT yet integrated: its Transform2 operator* member-ABI (3-arg out-buffer
// form) and its 5 Transform2/Vector2 dispatch slots (0x140152640/648/650/658,
// 0x140151e58) need disasm verification + slot registration first, and it externs exe
// .rdata constants that don't link in standalone. Registering the closure with an
// unverified ace40 would be a latent silent-corruption landmine (the radius branch is
// dormant on autostart, but blind registration is exactly what the wave rules forbid).
//
// When ace40 is verified+integrated, flip this on by registering (bottom-up):
//   14b00, 1f850, 4b40, 1fc00, 1f6b0, ace40, b0d80, 55430
// to their tool_base_highlight.cpp reimpls, and register the Transform2/Vector2 slots.
// (Intentionally NO RegisterExeAddr lines here so the gap metric stays honest at 8.)
#ifdef STANDALONE_EXE

void RegisterPlayerHighlightBatch() {
    // held — see header. ace40 verification gates this batch.
}
#endif
