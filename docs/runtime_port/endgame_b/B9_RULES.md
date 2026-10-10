# B9 — exceptions (ENDGAME B.1 decision 7) and the B8 residue: rule sheet (written 2026-10-10, start of the step)

Part B's step after B8 (`NEXT_SESSION.md` §2 of 2026-10-10). Two halves:

1. **Decision 7, exceptions** -- Block 0 is the inventory (`b9_session/EH_INVENTORY.md`): one row per
   `setjmp` / `longjmp` / SEH site in the tree and per throw / catch / `__try` frame in retail, each with
   what it is. Result, measured: the tree has ZERO non-C++ unwind sites (the ~25 files of 2026-09-13 were
   the oracle and the wave harnesses, deleted in ENDGAME A.4 / the registry teardown); retail's game code has
   ZERO `catch` frames and ZERO throws in the exe; the Runtime DLL's one throw (`Debug::ForceDump`, a dump
   trigger) and one `__try` (`EnumerateVideoModes`, an access-violation swallow) are diagnostics / a
   hardware-fault path, ruled in rows 1210 / 1211. Decision 7 therefore changes the tree by ONE flag
   (`-fexceptions` explicit) and closes on the inventory. No block converts anything: there is nothing to
   convert. §5 of the inventory is the rule for any future body that does unwind.
2. **The B8 residue** -- the LEFT rows, the introsort escape, the 49 measured survivors in
   `LEFT_FREE.md` §A. Three opus blocks (RT, RES, SURV), the coordinator gating as B8.

The user decisions owed (rows **1162**, **1202 / 1203 / 1204**) are NOT taken in this step: the user was not
available to ask at the start; they stay OWED and are the first item of the next NEXT_SESSION. No block
touches those four rows' bodies (`wg_f390_smooth`'s buffer fill, the three uninstalled slot bodies
`DecalComponent_Serialize` / `MapComponent_Draw` / `RawMaterialComponent_Update`).

Session kit: `b9_session/` (gate scripts copied from `b8_session/`; `runs/baseline` and the warning
baselines copied from B8's close; B11 base objects `runs/b11base/{perf,debug}` = main `22b1d9613`'s
`standalone/build-perf/obj` / `standalone/build/obj`, which `make -n` showed fully up to date at the kit
commit -- the `-fexceptions` spelling recompiles nothing, since make does not track flags and the flag is
GCC's default).

Starting numbers (main `22b1d9613` + Block 0): castmetric PROD **3** (1 named prerequisite + 2 ruled);
LEFT_FREE §A 121 names (54 free in retail, 49 `stays free` with a measurement, 18 harness); acceptance 1
line off (10 = 19, accepted); unit 920 / 0; invariants new game 5683 / 0, tutorial 5773 / 12, typetable
`b983f144`, resave 5/5 == golden `994eb184a8e88db9372b5b9cd13a9ed9`.

## 0. Scope

Every block commit is either `behaviour:` (B7 D2's message shape: the retail instruction range quoted, every
caller's retail call site quoted, what was dropped / added and why, what was RUN) or `B9:` (a typing / move
whose B11 verdict is IDENTICAL / SAME / RENAMED-IDENTICAL for every touched TU, verdicts in the message).
Device-lost stays DEFERRED (NEXT_SESSION §3). The four user-decision rows are untouched (above). A retail
difference a block FINDS outside its rows is a new `known_port_issues.md` row in its range (D8), not a fix.

## 1. Decisions

D1. **A LEFT argument test is read at every retail caller, then dropped or named.** Rows 1120-1123 / 1126 /
    1128 keep a test of an ARGUMENT (a null cell array, a null template, a null centre, a null UI, a range)
    that retail's body never makes. For each: read every retail caller's argument construction; if the
    argument is impossible-null / in-range at every caller, the test is dropped (`behaviour:`, callers
    quoted); if a STANDALONE route can hand the value (a harness, a shared template, a PORT GAP arm), the
    test stays AT THAT CALLER with a comment naming the route, and the row's status says which (B6 D4(a)/(b)).
    Never a test left where it is with a vaguer comment.
D2. **A second port of a retail body goes; its caller goes on the one member** (row 1080's
    `FireComp_StartAudioEmitter_42c60` -> `AmbientEmitterComponent::PlaySlot(1)`; `TerraformComponent_RegionRecalc`
    -> `WorldGeometry::MarkChunksDirty`): `behaviour:`, with the guards the copy had and the member lacks listed
    and each one ruled by D1 (dropped, or kept at the caller with its route). The copy's declaration line is
    deleted; an emptied `*_funcs.h` is deleted by the COORDINATOR at the close (B8 D4).
D3. **The dialog set-up tests (rows 1200 / 1201) are root-caused at the harness, not kept.** `ProfessionUI::
    OnOpened`'s skip serves the harness re-drive of slot 5 (`ToolbarUtility_HarnessDriveController("ProfessionUI", 5)`
    in `standalone/tool_harness.cpp`): the harness stops re-driving a slot `AddDialog` already ran, then the
    skip goes. `ProductionUI::UpdateList`'s `if (!tracker)`: the entity panel is built from the entity's OWN
    UIDescription, so every template that carries ProductionUI carries a TrackerComponent -- read the templates
    (`grep -l ProductionUI` over the extracted resources, or the dialog registry) and drop; the coordinator runs
    `harness_route.sh panelclose` on the merge. `OnElementEvent`'s test: no route, drop.
D4. **The introsort escape** (`rtsort::Array16Assign` 0x180050d30, `introsort.cpp:353`) becomes the existing
    `Collection::Array<Audio::Scene::EmitterInstance>::operator=` (`Assign`, header-defined over
    `rtcoll::ArrayAssignCore`) at its four call sites (audio.cpp:940, audio_scene_update.cpp:277 / 593,
    introsort.cpp:428); the 0x1800dfd10 caller's bare `int64_t empty[2]` becomes a typed empty
    `Collection::Array<EmitterInstance>` local scoped to the call. Then the B7 measurement is RE-TAKEN: B11 on
    `Scene::Update`'s TU must show no `System::MemoryAllocator::MemFree` call added after the assign (the
    escape was the out-of-line body; `Assign` inline lets GCC see `_data == nullptr`). If it still escapes,
    the spelling is changed until it does not or the row is re-tagged with the NEW measurement -- not the
    old one. `introsort.cpp:355` goes with the body; castmetric 3 -> 2 is the expected number, measured, not
    assumed. The stride-16 / align-4 facts in the banner are kept as the instantiation's comment.
D5. **A survivor is settled by reading retail, not by re-tagging.** Each `stays free` row in LEFT_FREE §A
    names a blocker. The block reads the retail body and callers and does ONE of: (a) moves it (a member of the
    Collection template -- an index-of, a `Remove(value)`, a `Serialize` IF retail has `Array<T>::Serialize` as
    a template body: read Collection.h hazard 6 FIRST, the three element-loop serializers are one question;
    a free function becoming a header-inline static for its one caller), (b) deletes it (the function-pointer
    parameter of `HideRenderWorkplaceMesh` goes when retail's two callers each call their storage member
    directly -- read 0x1401157c2 / 0x140115892), (c) re-tags it with a NEW measurement that names what the
    tree still lacks, or (d) rules it `free in retail` / `harness` with the RCX evidence. A row's old tag is
    never copied forward unread. B11 for (a): RENAMED-IDENTICAL for the body, IDENTICAL for callers
    (`b9_session/b11pair.py` with a rename map, B8 C1); accepted DIFF shapes B8 C2 / C5 / C6.
D6. **Harness-only survivors are made real where the port's instrumentation is the only blocker.** The orchard
    ctor doors (`HarvestOrchard_CtorFromEntity` / `PlantOrchard_CtorFromEntity`) run `OrchardDiag_Note*Constructed`
    through a local `extern "C"`: the counter gets a header (`standalone/orchard_diag.h`, included by the two
    `.cpp`s), the doors become the real ctors called by name. `DebugUI_Slot9`'s harness caller passes a literal
    null `DebugUI`: the harness constructs or finds the real `DebugUI` (the dialog exists in the menu loop) or
    the row stays with the measurement. `PickingComponent_RegisterMeshForEntity`, `TownRegistry_Register*`,
    `CitizenEmploy_SeatWorkers`, `Pf_RegionPairCache_Build`: read; a port-only wiring with no retail body
    stays `harness / port-only` (ruled, with the retail path named).
D7. **B11 is the gate for a `B9:` commit; the resave golden and the route set for a `behaviour:` one.** Base
    `b9_session/runs/b11base/perf` (`/debug`). A `behaviour:` commit quotes the expected DIFF (the dropped
    compare, the removed call). The coordinator's gate (`b9_session/mergegate.sh`) runs the full route set on
    every merge; `harness_route.sh pick | panelclose` and `disaster_route.sh` on RES and SURV.
D8. **New rows:** coordinator 1210-1219, RT 1220-1239, RES 1240-1259, SURV 1260-1279, at the end of the
    relevant §14 table (never a rewrite of another row; `keepboth.py` doubles a row both sides edited).
D9. **No new instrument, no new flag, no new helper / overload / default argument retail lacks, no guard
    added** (CLAUDE.md, DIRECTION.md §2, B6 D4). A faithful crash a route reveals is root-caused upstream.

## 2. Per-row recipe

1. Read the row (`known_port_issues.md`) or the LEFT_FREE line; read the body; read the retail body at the
   address AND its callers (decompile and the first instructions: what RCX is; Ghidra drops arguments on
   indirect calls and forwarders -- the disassembly decides).
2. Apply D1-D6. One commit per row or per survivor (a family of survivors with one ruling may share a commit
   if the title names every row).
3. Build `-j2` debug + perf + unit in the foreground (600000 ms timeout, re-invoke if cut; check the EXIT
   CODE). 0 new warnings against `b9_session/runs/warn_baseline_*.txt`. Unit 0 failures. `castmetric.py`:
   3 (RT: 2 after D4), nothing new. B11 per touched TU (D7).
4. Commit: `behaviour: <row> -- <what>` or `B9: <what>`; body: retail instruction range, callers quoted, B11
   verdicts per TU, what was RUN ("static gate only: ..."), rows left and why. The two attribution lines.
5. Hand in: the block's `HANDIN.md` in its worktree (`b9_session/handins/<block>.md` on merge).

## 3. Shape

| block | rows / items | files owned (callers anywhere, line-local) |
|---|---|---|
| **RT** | the introsort escape (D4); row 1163 (`RangeResize` -> `Array<GeometryDrawRange>::SetSize`, `Game/water_contour_mesh.cpp`); row 1164 (one token, `Game/gameupdateloop_serialize.cpp`); the Collection survivors: `QwordArraySetSize`, `ActRm_ArrayResize16_da70`, `ActRm_VectorClone_d9d0`, `Reimpl_Array_Resize_Stride08`, `PathPairs_Clear_25b30`, `BuildComponent_FindInQueue`, `CemeteryListErase_Reimpl`, `CropFieldComponent_SerializeEntityList`, `BucketTree_Serialize_21ba0`, `Reimpl_Array_Serialize_Stride20` (D5) | `src/engine/collection/*`, `src/engine/collection/*`, `src/engine/audio/Audio.h`, `src/engine/audio/{audio,audio_scene_update}.cpp`, `src/game/Game/{water_contour_mesh,gameupdateloop_serialize}.cpp`, the declaration lines of its rows in `src/game/*/exe_bodies.h`, the erased cores' wrappers and callers (line-local) |
| **RES** | rows 1120, 1121, 1122, 1123, 1126, 1128 (D1); 1200, 1201 (D3); 1080 (D2); row 993's 0x140046a90 (`citizen_lifeevent_handler.cpp`: `IsDerivedFromNullSafe` -> the member `IsDerivedFrom`); the B8 audit rows 1207 (HandleLifeEvent's 0x400 ring case, `citizen_onremove.cpp`), 1208 (`SerializeAll`'s `if (m_citizenList)`), 1209 (`GameMain`'s `if (settings)`) | `src/game/Work/work_base_reimpl.cpp`, `Component/{pasturecomponent_states,pasturecomponent_spawn,firecomponent_ignite,residencecomponent,citizen_lifeevent_handler,citizen_onremove}.cpp`, `Component/firecomponent_funcs.h`, `Player/roadtool_calcpath.cpp`, `Game/{menudialogloopt,game_main}.cpp` and the SerializeAll file, `Dialog/{optionsdialog,professionui,productionui}.cpp`, `standalone/tool_harness.cpp` (the slot-5 re-drive only) |
| **SURV** | the code/Game survivors not RT's: `TerraformComponent_RegionRecalc` (row 12, D2), `Storage_GetResourceCount2` / `CitizenComp_Storage_GetResourceCount` (D5(b), `citizenai_gather.cpp`), `EventLog_YearString_Reg`, `TradeResourceTree_NewNode` / `_Retrace`, `TradeMerchantOrders_Destroy`, the WorkBST rows `WorkBST_RegisterWork_1a30` / `WorkBST_BST_Serialize_296b0` / `WorkCellBST_Insert_9a40` / `WorkCellBST_Serialize_9de0` (the out-node slot: a typed `Node**` with a local per caller, or re-tagged with the measurement), `WorkBase_TreeNodeFree_14c30` / `_e320` (a non-recursive spelling measured, or stays), the harness rows (D6: the orchard ctor doors, `DebugUI_Slot9`, `PickingComponent_RegisterMeshForEntity`, `TownRegistry_Register*`, `CitizenEmploy_SeatWorkers`, `Pf_RegionPairCache_Build`), the `free in retail` confirmations it passes (`FindUIElementByPath_Impl`, `NameDB_*`, `ResourceCategory`) | `src/game/Work/*`, `src/game/Action/*` (bodies, not the Collection declaration lines RT owns), `src/game/Component/{citizenai_gather,terraformcomponent*,tradecomponent*,eventlog*}.cpp` and their headers, `src/game/Game/eventlog.cpp` / `EventLog.h`, `src/game/Dialog/debugui*.cpp`, `standalone/{orchard_diag,disaster_harness}.cpp` (+ the new `orchard_diag.h`), the `exe_bodies.h` / `*_funcs.h` declaration lines of ITS rows |

One wave, three worktrees (`b9-RT`, `b9-RES`, `b9-SURV`), `-j2`, static gate only. Merge order: RT first (its
Collection.h is what the others compile against; a block that merges main after RT re-B11s nothing -- its TUs
differ by inlining only), then RES, then SURV. The coordinator alone gates; `tools` on all three, `ui` on RES
(dialogs); `harness_route.sh panelclose` + `pick` and `disaster_route.sh` on RES and SURV.

## 4. The gate (every merge, coordinator only)

`b9_session/mergegate.sh <branch> <label> "<title>" [ui] [tools]`: builds the MERGED tree before the merge
commit exists, then `gate.sh` (debug / release / perf / unit builds, 0 new warnings, new game `5683/0`,
tutorial `5773/12`, loadgame, the -O2 new-game route, typetable == baseline, resave 5/5 == golden
`994eb184…`). A resave DIFF is read with B7 C3 / C4 FIRST (a payload that shrinks is a nulled reference).

## 5. Working rules (B8's, unchanged; §5 of NEXT_SESSION)

- Agents never run the game; builds `-j2` in the foreground; at most four building at once; the coordinator
  alone gates. No `git stash`, no amend / rebase / push, no sub-agents, no `taskkill /IM` (kill BY PID).
  A fresh worktree is reset to the NAMED base commit (`git log -1` before editing). Merge main before the
  final build.
- Commit the gate table (and anything else) BEFORE `mergegate.sh`: it refuses a dirty tree.
- Never `git add` the session directory wholesale (`runs/`, `gate_*.out`, merge logs are untracked).
- Files with backslashes go through the Write / Edit tool, never a heredoc.
- Ghidra MCP: pass `program=` explicitly; never `switch_program` (the instance is shared).
- A kept guard with a "route" story is root-caused, not accepted (B8).

## 6. Acceptance at the close

Decision 7: `EH_INVENTORY.md` §1 at zero (re-grep at the close), `-fexceptions` in both flag sets, rows 1210 /
1211 ruled, `ENDGAME.md` B.1 item 7 marked DONE. Residue: every LEFT / OPEN / SUSPECTED row of NEXT_SESSION §2
(except the four user-decision rows) ends FIXED / NOT A DIFFERENCE / LEFT-with-the-owing-row; castmetric
measured (2 expected); `LEFT_FREE.md` regenerated by `b8_session/leftfree/regen.py` (every `stays free`
line re-read: a NEW measurement or a move); `acceptance.sh` 08 / 09 = 0 against it; unit 0 failures;
invariants unchanged (or a resave re-cut attributed to one `behaviour:` commit); `stepclose.sh` clean;
`worldcmp.sh` identical to the B8 close run or every line attributed. Empty `*_funcs.h` deleted.

## 7. Corrections (dated, appended as blocks find them)

2026-10-10, from the RT gate:
- C1. **The kit needs the `RESOLVE_CACHE_GONE` marker beside `gate.sh`**: without it the gate counts a missing
  `[RtResolve] 0 misses` line (a printer deleted with the resolve cache) as a problem per route. RT's gate printed
  3 phantom problems; the marker was copied, the row is 0. Copy it with the scripts next time.

2026-10-10, from the RT2 merge:
- C2. **`HANDIN.md` at a worktree root collides**: two blocks adding it conflict (RT2 kept its own over SURV's), and
  the LATER merge re-resolved main's renamed `handins/SURV.md` onto RT2's text (git's rename + modify). Restored by
  hand (`abc172787`). Blocks write `b9_session/handins/<block>.md` directly next time; the coordinator checks the
  hand-in files after every merge.

2026-10-10, from SURV's and RT2's hand-ins:
- C3. **An attribute is a measured spelling, not a helper** (D9 does not forbid it): `[[gnu::always_inline]]` where
  retail inlines (B7's shape; `ArrayAssignCore`), `[[gnu::noinline]]` where retail CALLs each instantiation and GCC
  would open-code it (`IO::Serializer::Serialize<T>`), `[[gnu::noipa]]` where `noinline` alone yields an `.isra`
  clone dropping an unread argument (`WorkBase_TreeNodeFree`, the WorkBST templates). Each quoted with the B11 that
  chose it. A `noinline` on a template is per TEMPLATE: one instantiation may CALL where retail inlines (row 1230).

2026-10-10, from the coordinator:
- C4. **The Bash tool's heredoc eats backslashes AGAIN**: a python heredoc with `Template\UtilityProfessions` died on
  a `\U` escape (the known B8 trap, different symptom). Every text with a backslash goes through the Write / Edit tool,
  gate-table rows included.
