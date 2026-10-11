# B9 (exceptions and the B8 residue) — close numbers (2026-10-10)

Start `22b1d9613` (B8 CLOSED) -> Block 0 `cf62fb35a` (the exception inventory, `-fexceptions`, rows 1210 / 1211, the
rule sheet and kit) -> four gated merges (GATE_TABLE.md rows RT `df2c61a96`, RES `b2f495fe4`, SURV `41427ae97`, RT2
`cbb701d58`) -> LEFT_FREE regenerated `abc172787` -> the close documents. 54 block commits (19 `behaviour:`, 33 `B9:`,
2 hand-ins), 4 merges, 0 problems left standing; 146 files, +2,320 / -1,333 lines (code / runtime / standalone / Makefile:
118 files, +847 / -1,209).

## 1. What the step did

- **Decision 7 (exceptions), DONE by inventory (Block 0, `b9_session/EH_INVENTORY.md`):** a Ghidra `.pdata` scan of the
  three retail binaries classified every frame with a handler (SEH scope tables, C++ `FuncInfo.nTryBlocks`, GS cookies)
  and listed the `_CxxThrowException` xrefs. Exe: 33 SEH frames (all MSVC CRT), 2321 C++ cleanup-only frames, **0
  catch, 0 game throws**. Runtime DLL: 57 SEH frames = 56 CRT + **1 game** (`Video::Device::EnumerateVideoModes`'
  access-violation swallow, filter `code == 0xC0000005`), 2227 cleanup-only, **0 catch**, **1 game throw**
  (`Debug::ForceDump`: `throw 0` inside an SEH scope whose filter writes a minidump; no caller in the exe). VideoDX11:
  30 CRT SEH, 0 catch, 1 CRT throw. The tree: **0** `setjmp` / `longjmp` / `__try` / `RaiseException` sites; the crash /
  hang / fail-fast handlers live in `standalone/` only. So the set of sites to convert was EMPTY; the change is the
  explicit `-fexceptions` and rows 1210 (RULED: a hardware fault with no portable spelling, kept as an error-path
  difference) / 1211 (NOT A DIFFERENCE: diagnostics). `ENDGAME.md` B.1 item 7 marked DONE.
- **RT (the introsort escape + Collection survivors):** `rtsort::Array16Assign` deleted, its four callers on
  `Array<EmitterInstance>::operator=` (castmetric 3 -> 2); row 1163 (`RangeResize` -> `RawArraySetSize` / SetSizeCore)
  and 1164 (`s_tidCompEnt &&`) FIXED; `QwordArraySetSize` (19 callers) on `Array<T>::SetSize` / `SetSizeFill`,
  `BuildComponent_FindInQueue` / `CemeteryListErase_Reimpl` -> `Array<T>::Find` / `Remove` (3 + 29 callers),
  `Reimpl_Array_Resize_Stride08`'s null test dropped; seven survivors re-tagged with new measurements
  (`PathPairs_Clear_25b30` is a constant-folded one-argument `SetSize(0)`: the "Ghidra dropped EDX" note was wrong).
  Found: the free after the assign was still there (the core out of line) -> RT2; the three element-loop serializers are
  ONE template but on `IO::Serializer`, not `Array` -> RT2. Row 1220 filed.
- **RES (the LEFT rows):** 1120 / 1121 / 1122 (Map's cell guards; ValidateCell sign-extends both cells as retail's MOVSXD),
  1123 (menu loop slot pieces: the `!time` tests and an empty-array return that also skipped `_frameIndex++` and
  `Scene::Clean`), 1126 (ApplyKeyMapToUI's tests + the two OptionsDialog `if (keyMap)`), 1128 (SpawnStartingHerd's five
  tests; the group-count return moved after `Rand(2)`, which retail always draws), 1200 (the harness stopped re-driving
  ProfessionUI slot 5 — AddDialog's open already runs it — then the `IsSetUp` skip and OnElementEvent's test dropped),
  1201 (ProductionUI's tracker / Ready / empty-lists returns), 1080 (the second `PlaySlot` port deleted, caller on
  `em->PlaySlot(1)`), 993's last site (`desc->IsDerivedFrom`), 1208 / 1209 (SerializeAll's citizen-list tests; GameMain's
  Save before its settings test): all FIXED; 1207 NOT A DIFFERENCE. Rows 1240-1247 filed (1243 a real missing call).
- **SURV (the measured survivors):** row 12 FIXED (`TerraformComponent_RegionRecalc` -> `WorldGeometry::MarkChunksDirty`,
  the zero-chunk guard dropped); `HideRenderWorkplaceMesh`'s function-pointer parameter gone (the Gather states spell
  retail's `StatusIconComponent::SetFlag` block); `ResidenceComponent::GetFoodCount` / `GetFuelCount` members (every TU
  IDENTICAL) with GetFoodCount's three tests dropped; `EventLog::YearString` / `SeasonString` members (LEFT_FREE's
  "retail inlines it" was wrong: both are retail bodies with ten callers each) with their tests dropped; the orchard ctor
  doors real ctors (`src/diag/orchard_diag.h`); `TradeStockTree::Retrace` member (the dock's tally typed as the
  TradeStockTree retail builds on its stack); `WorkBase_TreeNodeFree` one `[[gnu::noipa]]` template (callers IDENTICAL
  tree-wide); the WorkBST insert / serializer templates over the node type (typed out locals measured SAME first); the
  rest re-tagged with a measurement. Rows 1260-1263 filed.
- **RT2 (the two owed items):** `rtcoll::ArrayAssignCore` header-inline (B7's shape) — the empty-source free in
  `Scene::Update` GONE (row 880 closed; the one difference left: retail CALLs the assign where we inline it);
  `IO::Serializer::Serialize<T>(int, T*, const char*)` `[[gnu::noinline]]` with `CropSlot::Serialize`,
  `MenuSceneMesh::Serialize`, the one-argument `InternalPtr<T>::Serialize` (T's own cell = retail's at every caller:
  0x1401c5d70 / 0x1401c7b30) and `Collection::Array<T>::Serialize` (hazard 6 corrected: the DLL has no Array row, the exe
  instantiates one); the three free serializers deleted; row 1230 = what stays.

## 2. The metric

| | start `22b1d9613` | close |
|---|---|---|
| castmetric PROD total | 3 (1 named prerequisite + 2 ruled) | **2** (2 ruled; the escape is gone) |
| cast_ok | 702 | 691 (ceiling 754) |
| `exe_bodies.h` / `*_funcs.h` declarations (acceptance line 08) | 121 names / 122 | **102 names / 103** (17 headers; 58 free in retail, 28 stays free, 23 harness) |
| `void*`-first functions in code/Game (line 09) | 39 | **34** |
| acceptance (`b4_session/acceptance.sh`) | 1 line off (10 = 19) | **1 line off: 10 = 19** (accepted); 01 = 2 of 2; 08 / 09 = 0 against the REGENERATED LEFT_FREE.md (`b8_session/leftfree/regen.py`, now reading `B9:` tags first) |
| unit tests | 920 / 0 | **920 / 0** |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `965b527a` (since B8 HDR), resave == `994eb184…` | **unchanged on all four merges**; loadgame calls 3416 / 3425 / 3432 (B8's own spread) |
| known_port_issues | — | rows 12, 880, 993, 1080, 1120-1123, 1126, 1128, 1163, 1164, 1200, 1201, 1208, 1209 FIXED; 1207 NOT A DIFFERENCE; 1210 RULED, 1211 NOT A DIFFERENCE; 16 new SUSPECTED / owed rows 1220, 1230, 1240-1247, 1260-1263 |
| `setjmp` / `longjmp` / SEH sites in the tree | 0 (measured) | **0** (`-fexceptions` explicit) |
| sa-stress | 139 fps (B8 close) | see §6 |
| worldcmp (seed 12345) | identical to B8's DEC run | see §6 |

## 3. Blocks and merges

| block | items | moved / deleted / fixed | stays (re-tagged) | behaviour: | B9: | merge |
|---|---:|---|---|---:|---:|---|
| RT | 14 | Array16Assign, RangeResize, QwordArraySetSize, FindInQueue, CemeteryListErase, 1164 | 7 | 3 | 5 | `df2c61a96` (tools by hand; the 3 phantom problems = C1) |
| RES | 13 rows | 12 FIXED, 1 NOT A DIFFERENCE | — | 12 | 2 | `b2f495fe4` (ui, tools, panelclose, pick, utility / professions, fire) |
| SURV | 27 | row 12, GetFoodCount / GetFuelCount, YearString / SeasonString, orchard ctors, Retrace, TreeNodeFree, the WorkBST family, SetFlag spelled | 12 + 4 free-in-retail confirmed | 4 | 8 | `41427ae97` (tools, fire, tornado, tradepanel, orchardpanel) |
| RT2 | 2 | ArrayAssignCore inline (row 880), the Serializer template (3 serializers) | row 1230 | 0 | 3 | `cbb701d58` (tools) |

## 4. Routes at the merges (all exit 0, crash log 0)

newgame / tutorial / loadgame / newgame_perf and resave 5/5 on every merge; ui (mods, menuquit, options, achievements,
goaldialog) on RES; tools (placement, road, removal — the removal route's `SKIP CalcPath: removalDesc null` is B8's
shape) on all four; `panelclose` (CLOSED), `pick` (PICKED -> CitizenUI OPENED), `utility` with
`Template\UtilityProfessions.rsc` (ProfessionUI Slot5 ran ONCE, Slot8 n=20), fire (2/2 houses ignited) on RES; fire,
tornado (spawned through `DebugUI_Slot9`, the swirl live=239/750 at tick 720), `tradepanel` with the trading-post demo
(TradeUI LIVE, 9 controllers), `orchardpanel` with the orchard demo + SELECT (TYPED, `PlantOrchard constructed=1`) on
SURV.

## 5. Rule-sheet corrections (B9_RULES §7)

C1 the `RESOLVE_CACHE_GONE` marker; C2 the HANDIN.md merge collision; C3 attributes as measured spellings
(`always_inline` / `noinline` / `noipa`); C4 the heredoc backslash trap (again). Details in `B9_RULES.md` §7 and
`NEXT_SESSION.md` §5.

## 6. stepclose and the close extras

- **stepclose** (`runs/stepclose/summary.txt`, on `abc172787`): mods / menuquit / options / achievements clean,
  goaldialog 1 OpenGoal (killed=YES expected), tool placement / road / removal exit 0 (removal's `SKIP CalcPath` as
  B8), in-game save (resave file mode) `SAVE OPERATION COMPLETE`, crash log 0 (exit 1 = the expected whole-file DIFF
  in that mode, as B8); **sa-stress 139 fps (+0.5%)**, 800 citizens / 6000 frames, median cpu 40%; true release
  compile exit 0, 934 files, exe 6,507,877 bytes, 188 warnings (B8: 189); clean rebuild debug / perf / unit exit 0;
  gate closeclean (skipbuild) **0 problems** (0 new warnings, unit 920 / 0, newgame 5683 / 0, tutorial 5773 / 12,
  loadgame, newgame_perf, resave 3/3 PASS); NOT IMPLEMENTED stubs 0, stub set 0 new / 0 gone; tutorial census
  citizen=16 herd=16 as B8.
- **worldcmp** (seed 12345, `runs/CLOSE_worldcmp`): the close exe vs `oldbase` (`b2a5434ca`) DIFFERS by the one
  `[PfSeed]` wording line (as every close since B6); **identical to B8's CLOSE run** (118 lines, md5 97fcc629).
- **Decisions taken at the close (the user, 2026-10-10):** ENDGAME B is CLOSED here; no B10; next is Phase D — the
  restructure, then the portable platform layer (settings file instead of the registry and of the launcher dialog,
  SDL for input / audio, DXVK for the renderer): `ENDGAME.md` §E, `NEXT_SESSION.md` §2.
