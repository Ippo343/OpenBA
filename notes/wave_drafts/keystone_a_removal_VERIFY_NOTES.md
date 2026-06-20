# RemovalTool CalcPath (0x1401035e0) — deferred verify-then-land roadmap

Status (2026-06-20): opus-ported by PortKAremove, closure confirmed BOUNDED + closure-safe,
syntax-clean. DEFERRED from Phase 2 Keystone A wave 2b — needs a lead offset-by-offset
verify pass before wiring (the draft compiles clean but, like every agent port this phase,
must be lead-verified vs disasm — firework/B-tail each had real drift bugs caught only by
lead verification).

Files (move back into place before building):
  notes/wave_drafts/keystone_a_removalcalcpath.cpp.draft -> code/Game/Player/keystone_a_removalcalcpath.cpp
  notes/wave_drafts/reg_keystone_a_removal.cpp.draft     -> standalone/reg_keystone_a_removal.cpp
Wire: extern void RegisterKeystoneARemovalBatch(); + call in RegisterCodeFunctions() (address_registry.cpp).
Registers bottom-up: 0x14002d960, 0x1400f8d50, 0x14011e4d0, 0x14011dfa0, 0x14009c700, 0x1400f9ad0, then 0x1401035e0.
Effect when landed: Player gap 8 -> 7.

## Cross-cutting safety (lead-checked 2026-06-20)
- 0x14011dfa0 (StateMachine_dfa0): xrefs = UpgradeUI_Update + RemovalTool_Slot13 + 3 .rdata vtable DATA refs (original exe).
- 0x1400f9ad0 (CitizenAI_9ad0): xrefs = FireComponent_Ignite + RemovalTool_Slot13 + .rdata DATA refs.
- NOT autostart-reached: components are inert in autostart (Phase 1; their vtable +0x48 = Inert_Stub, NOT dfa0),
  no HUD (UpgradeUI) and no fires (FireComponent_Ignite) at t=0. The DATA refs are the ORIGINAL exe vtables, not
  our standalone MakeInertTyped vtables. So registering these exe addrs only affects code that ResolveAddr()s them
  (RemovalTool, + UpgradeUI/FireComponent reimpls if any) — all non-autostart. A/B will confirm no regression.

## PRIORITY verify target (agent's own flagged soft-spot)
SM_RunPhase inside RemovalCalc_StateMachine_dfa0 — the nested vtable-walk action-tick loops at
disasm 0x14011e08a-0x14011e181 (slot calls +0x18/+0x20/+0x28/+0x30, deepest-child descent via node[2]).
Densest part; structurally faithful per agent but verify var-for-var vs disasm FIRST.

## Closure table (agent-reported, spot-check during verify)
Already registered: c3720 ComponentEntity_CopyCtor, c48a0 RegisterEntity, c2ea0 MarkEntityRemoval,
  003110 ArrayPush, 003ea0 FindComp(MapComp), 005780 FindComp(Destroy), 11e710 is-removable-child,
  12ead0 (component_economy), af370 TradeWork_impl_f370, 104670/770/8a0 sub_46/47/48,
  f8e30 (reg_action_complete), a6e0 (reg_work_bst), a8c0 (reg_pf_avl), 9f2f0 (reg_pf_roots_road),
  a6fe0 (reg_pf_astar), a8c90 (reg_pf_road), a9660 StateMachine_Tick (reg_component_economy);
  + engine ptrs (InternalBasePtr_Ctor/CheckPtr, IsA, IsDerivedFrom) + slots (static MemAlloc 0x140152b60, Time::Time 0x140152b00).
Ported this wave (were unregistered): d960 (FindComp-by-type, container +0x28/+0x30, type 0x1401c7430),
  f8d50 (in-order AVL successor), 11e4d0 (StateMachine slot+0x10 dispatch), 11dfa0 (StateMachine action-tick),
  9c700 (road-removal cell handler), f9ad0 (CitizenAI per-entity AVL update), 1035e0 (CalcPath).

## Fidelity notes to confirm vs disasm (agent-reported; the error-prone bits)
- 1035e0: corner-normalization + nested loop; entity writes (short)col@+0x28 / (short)row@+0x2a;
  cellId = *map_width*row + col, cell stride 0x30, cell array at map+0x80 (int* idx 0x20).
- c700: road BST root at byte +0xe8 (int* +0x3a); child convention param_2 < key -> +0x10(left) / > key -> +0x18(right).
  TypeInfo_BuildRoad global = **0x1401c6860** (NOT 0x1401c79e0) — resolve via ResolveAddr+fn_IsA. cell+0x2b = (byte)*(uint*)(comp+0x64); removeKind=+0x60; res10=+0x68.
- 9ad0: OPPOSITE child convention vs c700 — first BST param_2 < key -> +0x18 / > key -> +0x20; id-AVL erase entId < key -> +0x10 / > key -> +0x18;
  node [0]=key(iVar6 | id<<32), +8=parent, +0x10=left, +0x18=right, +0x20=dword; MemAlloc(0x28,8); balance via a8c0(&node,&prev).
- dfa0: two-phase action-stack tick over state+0x48; phase2 transition slot(+0x30) takes &outState, phase1 doesn't.
  Top-state byte +0x52 (fallback +0x50 when negative); early-returns on 6/6/7; else DestroyUI e4d0 only when none match.

VERIFY METHOD: decompile each of the 7 fns, compare the on-disk .draft var-for-var vs disasm (NOT the agent's
report — agents 2nd-pass-edit after reporting), fix drift, then move files into place, wire, build, A/B (confirms
no autostart regression even though the path isn't exercised), commit.
