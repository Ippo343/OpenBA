// code/Game/Player/tool_base_render.cpp
// Player registration wave, round 2d — batch "render" (edittool.cpp render tier).
//
// Target addr (resolved from code/Game/Player/edittool.cpp):
//   0x1400fcc90  Entity_impl_cc90  — close/teardown of the edit-tool entity overlay
//
// VERDICT (2026-06-19, agent R4-render): DEFER. No body is provided and nothing is
// registered (see standalone/reg_player_render.cpp, an empty registrar). The chain
// is gated by TradeWork_impl_b930 (0x14004b930), a ~250-line EdgeMesh tessellation /
// vertex-buffer builder that I cannot port 1:1 with confidence (see "Faithfulness
// assessment" below). Per the wave SAFETY RULE, a function that DEFERs drags its
// whole consumer chain (c4a0 -> cb40 -> cc90) with it. These addrs are DORMANT on
// the autostart path (edit-tool entity overlay), so a wrong port is undetectable
// silent corruption by the A/B test — strictly worse than the safe no-op of leaving
// the call site skipped.
//
// The closure below is fully decompile-verified and is the starting point for the
// dedicated render sub-wave that will land cc90.
//
// ============================================================================
// FULL CLOSURE (every exe sub-callee, recursive; reg status verified 2026-06-19)
// ============================================================================
//
// 0x1400fcc90  Entity_impl_cc90            [TARGET]   simple/portable, NOT the blocker
//   -> Entity_impl_cb40                    0x1400fcb40 [UNREG]  simple, NOT the blocker
//   -> UI::DialogInstance::GetController                       (Runtime export, OK)
//   -> UI::UserInterface::CloseDialog                         (Runtime export, OK)
//   -> fn_InternalBasePtr_Ctor / _CheckPtr_slot / fn_IsDerivedFrom  (engine_funcs, OK)
//   -> DWORD_1401c5788 (type-id, ResolveAddr 0x1401c5xxx, OK)
//   -> g_pGameWorldSingleton + 0x98 (UserInterface) (OK)
//
//   Entity_impl_cb40 (0x1400fcb40):
//     -> FUN_140004740                     0x140004740 [REGISTERED]
//     -> thunk_helper_d250_sub_7080        0x140007080 [UNREG, portable leaf]
//     -> Entity_impl_c4a0                  0x14004c4a0 [UNREG, RENDER MUTATION]
//
//   thunk_helper_d250_sub_7080 (0x140007080):  FindComponent loop, type DAT_1401c4410.
//     Only fn_InternalBasePtr_CheckPtr_slot / fn_IsDerivedFrom / fn_InternalBasePtr_Ctor.
//     -> NO exe sub-callees. CONFIDENT 1:1 portable.
//
//   Entity_impl_c4a0 (0x14004c4a0):  rebuilds the EdgeMeshInstance and re-adds it to
//   the live Graphics::Scene. This is render mutation against global Scene state.
//     -> Graphics::Scene::RemoveObject / AddObject               (Runtime export, OK)
//     -> Graphics::EdgeMeshInstance::SetMesh                     (Runtime export, OK)
//     -> Graphics::Geometry::SetTransform                       (Runtime export, OK)
//     -> System::Time::Time                                     (Runtime export, OK)
//     -> vtable call (*(this+0x28))+0x8  (EdgeMeshInstance update slot) (OK if mesh valid)
//     -> FUN_1400c3470                     0x1400c3470 [REGISTERED]
//     -> FUN_140004680                     0x140004680 [REGISTERED]
//     -> FUN_140004740                     0x140004740 [REGISTERED]
//     -> Entity_ClearPathQueue             0x140003f60 [REGISTERED]
//     -> thunk_helper_d250_sub_7080        0x140007080 [UNREG]
//     -> TradeWork_impl_b930               0x14004b930 [UNREG, *** THE BLOCKER ***]
//     -> fn_IsDerivedFrom, DAT_1401c4448 (type-id)              (OK)
//
//   TradeWork_impl_b930 (0x14004b930):  the ~250-line EdgeMesh tessellation builder.
//     -> CharacterComponent_impl_f3e0      0x14009f3e0 [REGISTERED]
//     -> FUN_140004680                     0x140004680 [REGISTERED]
//     -> FUN_1400b10e0                     0x1400b10e0 [UNREG, portable leaf]
//     -> TradeWork_impl_b440               0x14004b440 [UNREG, portable, uses 4 slots]
//     -> Math::Vector3::Vector3 (slot 0x140152a30 [REGISTERED])
//     -> Math::Quaternion::Quaternion                          (Runtime export, OK)
//     -> Graphics::EdgeMesh::EdgeMesh / ::Set (~28 args) / ::SetMaterials
//        / ::TriangleVertex::TriangleVertex                    (Runtime exports)
//     -> Volume::Box::Box / ::Extend / ::operator=             (Runtime exports)
//     -> System::MemoryAllocator::MemAlloc / MemFree           (Runtime exports)
//     -> g_pGameWorldSingleton (+0xf8 map grid, +0x110 heightfield),
//        DAT_140172114, g_fUVScale, DAT_1401c4448, vtable PTR_FUN_140159a60
//
//   TradeWork_impl_b440 (0x14004b440):  appends 4 EdgeVertex + 6 inlined index-array
//     growth loops. Mechanical but uses UNREGISTERED dispatch slots:
//       0x140151378 -> Graphics::EdgeMesh::TriangleVertex::TriangleVertex
//       0x140151390 -> Graphics::EdgeMesh::EdgeVertex::EdgeVertex
//       0x140152b60 -> System::MemoryAllocator::MemAlloc (static)
//       0x140152b68 -> System::MemoryAllocator::MemFree  (static)
//     -> TradeWork_impl_sub_74a0           0x1400074a0 [UNREG, portable leaf]
//        (16-byte-element array growth; the EdgeVertex buffer grow helper)
//     (NOTE: the brief's "FUN_1400b10e0 is a b440 dep" is slightly off — b10e0 is a
//      b930 dep, not a b440 dep. b440's only exe sub-callee is sub_74a0.)
//
//   FUN_1400b10e0 (0x1400b10e0):  samples a 6-corner terrain tile (18 floats) from
//     the heightfield, two parity branches. Uses slot 0x140152a30 (Vector3::Vector3,
//     REGISTERED), DAT_1401720bc const, heightfield fields +0xc0/+0xc4/+0xd0/+0xd8.
//     -> NO exe sub-callees. Portable.
//
// ----------------------------------------------------------------------------
// UNREGISTERED EXE CODE in closure: 7080, 74a0, b440, b10e0, b930, c4a0, cb40, cc90.
// UNREGISTERED DISPATCH SLOTS:      0x140151378, 0x140151390, 0x140152b60, 0x140152b68
//                                   (all four -> Runtime exports; registerable).
// Everything else in the closure (other exe addrs + Runtime exports + globals) is OK.
//
// ============================================================================
// Faithfulness assessment (why DEFER)
// ============================================================================
// Confident 1:1 ports: 7080, 74a0, b440, b10e0 (mechanical leaves), cb40, cc90.
//
// NOT confidently portable: TradeWork_impl_b930 (0x14004b930). It is the exact case
// the brief flagged for DEFER:
//   * Heavy pointer/int aliasing through CONCAT44-packed locals: local_260, local_1e8,
//     local_1f8 each carry TWO 32-bit values (count + capacity) in one 64-bit slot,
//     and puVar22/24/26/27 alternate between `ushort*` and `(uint)` views of the same
//     storage. A clean-C++ restatement risks transposing count/capacity or a pointer
//     for its low-32 int.
//   * TWO independently inlined array-growth loops with DIFFERENT element strides
//     (index buffer << 1 = ×2 bytes; the other << 3 = ×8 bytes) plus b440/74a0's own
//     inlined growth (×2 and ×0x10). Easy to mismatch a stride.
//   * Graphics::EdgeMesh::Set is called with ~28 arguments; Ghidra's argument recovery
//     for a call that wide is unreliable, and the exact (count,ptr) pairing order
//     would have to be confirmed instruction-by-instruction from the disassembly.
//   * Builds local_c8[Vector3] + local_c4[33] as one contiguous Vector3[6] scratch
//     buffer that feeds b10e0 — a contiguous-locals hazard.
// Because cc90's render path is DORMANT on autostart, a plausible-but-wrong b930 would
// build a half-correct EdgeMesh against the live Scene with NO A/B signal. That is
// silent corruption, strictly worse than the skipped call. => DEFER b930, and with it
// c4a0 / cb40 / cc90.
//
// ============================================================================
// To land in the dedicated render sub-wave (bottom-up order)
// ============================================================================
//   1. RegisterDispatchSlot the four slots -> their Runtime exports
//        0x140151378 -> "?TriangleVertex@..@EdgeMesh@Graphics@@..."   (ctor)
//        0x140151390 -> "?EdgeVertex@..@EdgeMesh@Graphics@@..."       (ctor)
//        0x140152b60 -> System::MemoryAllocator::MemAlloc (static)
//        0x140152b68 -> System::MemoryAllocator::MemFree  (static)
//      (verify each value-return ABI from the disasm before wiring; or add fn_*
//       engine_funcs entries and call those from the bodies instead of the slots.)
//   2. Port the leaves 7080, 74a0, b10e0, b440 (mechanical).
//   3. Port b930 1:1 from the DISASSEMBLY (not the decompile) — confirm the EdgeMesh::Set
//      argument list and each inlined growth stride against the asm.
//   4. Port c4a0 (render mutation), then cb40, then cc90.
//   5. RegisterExeAddr bottom-up: 7080,74a0,b10e0,b440 -> b930 -> c4a0 -> cb40 -> cc90.
//
// No code is emitted in this file — it is documentation only for the deferred set.
