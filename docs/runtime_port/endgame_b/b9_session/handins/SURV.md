# B9 SURV hand-in

Branch `b9-SURV`, base `cf62fb35a`, main merged at `296c68594` (main `fb7cfc339`: RT and RES in).
Static gate on the merged tree: debug / perf / unit builds exit 0, 0 new warnings (both baselines),
unit 920 / 0, castmetric **2** (RT's D4; SURV changed no castmetric line). The game was never run.

Routes the coordinator must run on this merge: the new-game route (gatherers with an empty home,
the eat / fuel paths of GetFoodCount / GetFuelCount, the quarry dig), `disaster_route.sh`
(tornado through DebugUI_Slot9, fire), the orchard route (PlantOrchard / HarvestOrchard ctors,
BANISHED_ORCHARD_DIAG counters), the trade route (AutoTrade's typed tally and ~TradeStockTree, the
stock tree retrace), the resave gate (the WorkBST serializer templates and the set teardowns).

## Per survivor

| survivor | ruling | commit | B11 | left / measurement |
|---|---|---|---|---|
| `TerraformComponent_RegionRecalc` (row 12) | D2: the second port deleted; QuarryComponent::ApplyDepth calls `WorldGeometry::MarkChunksDirty`; the zero-chunk guard dropped (retail 0x140054b2e, no test; the chunk size is 0x20 from world generation on every route) | `718fc8bb4` behaviour | quarrycomponent_deform.o IDENTICAL 14 + RENAMED-IDENTICAL 1; terraformcomponent.o ONLY-BASE the copy | row 12 FIXED; `terraformcomponent_funcs.h` now empty (coordinator deletes) |
| `HideRenderWorkplaceMesh`'s pointer parameter | D5(b): the helper deleted; the two Gather states spell retail's inline block (home test, count, `StatusIconComponent::SetFlag(6 / 7, 1)` -- the block is SetFlag inlined) | `3c75c136a` behaviour | citizenai_gather.o DIFF 2 (the indirect call and the inline icon copy replaced by the direct count call + `call SetFlag`), ONLY-BASE the helper | SetFlag's `settings &&` filed as row 1263 |
| `CitizenComp_Storage_GetResourceCount` / `Storage_GetResourceCount2` | moved: `ResidenceComponent::GetFoodCount` (0x14012a9d0) / `GetFuelCount` (0x14012ab00), RCX the residence at every retail caller (HeatTick 0x140129aab, Gather 0x14010bf2f ...) | `17b073745` B9 | vs previous commit: RENAMED-IDENTICAL both bodies, every caller TU IDENTICAL | -- |
| GetFoodCount's port tests | dropped (storage null, IsDerivedFromNullSafe, material null; retail 0x14012aa0a / 0x14012aae1 / 0x14012aa75) | `98b9b64c5` behaviour | SAME by b11cmp's multisets; disassembly: not-found now reaches the `[storage+0x30]` read (ud2), no-material the `[0+0x5c]` read | -- |
| `EventLog_YearString_Reg` (+ `_SeasonString_Reg`) | LEFT_FREE's premise was wrong: both are retail BODIES (0x1400951d0 / 0x140095230, ten callers each), EventLog members (RDX the return slot, RCX the log: 0x140094e2c / 0x140094e41); now `EventLog::YearString() / SeasonString()`, exports + statics gone | `538df6bd3` B9 | eventlog.o RENAMED-SAME 2, SAME 3, ONLY-BASE the season export; diseasecomponent.o IDENTICAL 28 + SAME 1 (Infect: RCX now the log, as retail) | -- |
| their port tests | dropped (clock null x2, seasons table null / "" fallback; retail 0x1400951f5 / 0x140095260 / 0x140095291) | `5fd710408` behaviour | eventlog.o: the two clock tests and the table test gone (SeasonString inlined into AddEventCore) | -- |
| `HarvestOrchard_CtorFromEntity` / `PlantOrchard_CtorFromEntity` | D6: `standalone/orchard_diag.h`; the counter at the top of each from-entity ctor; OrchardComponent's three states call `new (mem) T(...)` by name; the doors and their declarations gone | `7df3d5079`, `0c416a1f3` B9 | orchardcomponent_states.o DIFF 3 (door call -> ctor call), SAME 7 (new includes); *_ctor.o DIFF the ctor (+1 counter call), ONLY-BASE the door; the rest IDENTICAL | not B11-IDENTICAL by construction (an instrument call moved); gameupdateloop.cpp still declares OrchardDiag_Tick by a local extern (not this block's file) |
| `TradeResourceTree_Retrace` | moved: `TradeStockTree::Retrace` (0x14000e360); RCX is always a TradeStockTree -- the dock's tally IS one on retail's stack (0x140059b2b / 0x140059b2f ctor, dtor inlined 0x14005a440..0x14005a470); the dock local typed, its release is the dtor | `5f399498e` B9 | serialize.o RENAMED-IDENTICAL; dock.o DIFF 1: AutoTrade calls `~TradeStockTree` where the base had the same release inline (the dtor lives in tradecomponent.cpp) | the out-of-line dtor call is the one accepted DIFF |
| `TradeResourceTree_NewNode` | stays (c): retail inlines `new Node(k, v, parent)` with the placement-new null test (0x140059ee7 / 0x140059f5c); the spelling is a TradeResourceNode ctor the tree lacks | `8f852e4a7` tag | -- | a node ctor + the five inline sites (dock.o would turn a call into inline construction) |
| `TradeMerchantOrders_Destroy` | stays (c), MEASURED: 0x14000ef10 is Array<TradeMerchantOrder>'s element destroy (RCX the array, unread; RDX first element, R8D count), the inlined ~TradeMerchantOrder with NO null test before MemFree | `8f852e4a7` tag | as `rtcoll::TDestroy<TradeMerchantOrder>` over the implicit dtor: tradecomponent.o DIFF (the ~Array J1 `if (_data)` test and the goods spec's element-destroy dispatch added) | the loop stays |
| `WorkBase_TreeNodeFree_14c30` / `_e320` | templated: one `template <Root, Node> [[gnu::noipa]] WorkBase_TreeNodeFree` (Work/exe_bodies.h), the cores deleted from work_base_reimpl.cpp (RES told) | `12bdf9122` B9 | WHOLE tree: IDENTICAL 16106, ONLY-BASE 2 cores, ONLY-TIP 20 instantiations (each retail's 3-call body; link offsets checked per TU), SAME 2, DIFF 1 (PlayerToolManager::Teardown: Avl38FreeSubtree unroll order, same callees / counts) | plain template unrolls, `noinline` alone makes an `.isra` clone that drops RCX; `noipa` is what matches retail. The commit title says "28 call sites": it is 42 call lines (the title is wrong, the code is not) |
| `WorkBST_RegisterWork_1a30` / `WorkBST_BST_Serialize_296b0` | typed out-node local measured on two callers (SAME / SAME), then the family: templates over the node type in Work/KeyBst.h, `[[gnu::noipa]]` entries per header kind; the 14 insert callers' out slots typed locals; serializer load key = retail's one InternalPtr slot | `5738a31c7` B9 | WHOLE tree: IDENTICAL 16108, SAME 9 (the typed locals' stack slots), ONLY-BASE 2, ONLY-TIP 22, DIFF 10 (Work leaves' dtors / Set::Clear: unroll order only, call multisets equal); serializer instantiations = the core (one cmp operand order); insert instantiation CALLs the retrace (retail 0x140021b18) where the in-TU core inlined it | port tests in the serializers filed as row 1262 |
| `WorkCellBST_Insert_9a40` / `_Serialize_9de0` | stays (c): the one body serves element types with different key / value names (WorkCellAssignment {cell, citizen}, Claim {slot, citizen}, the scheduler's raw WorkSchedulerIdNode {key, work} + anonymous records); a template needs an accessor or one element type | `8f852e4a7` tag | -- | per-element accessor or one element type |
| `DebugUI_Slot9` | harness, re-read: case 329 never loads RCX (0x140060ea5); no DebugUI exists on the route (only the type factory 0x140010ec0 / RegisterType_DebugUI 0x14014be9e makes one, from a dialog resource no route opens) | `8f852e4a7` tag | -- | the literal null stays (UB in name only: the case reads no field). Alternative not taken: call 0x14005fd20's body by name |
| `PickingComponent_RegisterMeshForEntity` | harness / port-only: RegisterEntity's OnAdded broadcast already runs RefreshMesh | `8f852e4a7` tag | -- | row 1261 (a second RefreshMesh per placement, SUSPECTED) |
| `TownRegistry_RegisterResidence` | stays: it IS 0x140129de0's whole body + three port tests + a log; EnterBuilt (residencecomponent.cpp, RES) its caller | `8f852e4a7` tag | -- | row 1260 (fold into EnterBuilt, `behaviour:`) |
| `TownRegistry_RegisterWorkplace`, `CitizenEmploy_SeatWorkers`, `Pf_RegionPairCache_Build` | harness / port-only, retail path named on the line (0x14012f520 -> 0x1400895a0; 0x14007dd10's loop; 0x14009e8c0's in-place build) | `8f852e4a7` tag | -- | -- |
| `FindUIElementByPath_Impl` (0x1400e1870) | free in retail confirmed: 0x1400e18a2 MOV R14,RCX (the InternalPtr return slot), RDX the path String, R8 the dialog out | -- | -- | -- |
| `NameDB_GenerateName` (0x1400a4b30) | free in retail confirmed: RCX R12 (the String slot), RDX the NameDatabase (0x1400a4b63 MOV RBX,RDX) | -- | -- | -- |
| `NameDB_PickAndGenerate` (0x1400a4c60) | free in retail confirmed: RCX the String slot (0x1400a4c7d MOV RDI,RCX), RDX the Random, R8B the flag (0x1400a4c94) | -- | -- | -- |
| `ResourceCategory` (0x140088f90) | free in retail confirmed: 0x140088faa MOV RBX,RDX, RCX never read | -- | -- | -- |

Docs: `83a6c5cea` (row 12 status; rows 1260, 1261 (§14a), 1262, 1263 (§14d)).

## Notes for the coordinator
- `regen.py` (b8_session/leftfree) picks a line's ruling by `'B8:' in c`; the SURV lines now carry
  `// B9:` tags, which it reads only through its whole-comment fallback -- extend it to `B9:`.
- The merge commit `296c68594` carries git's default message (no attribution lines).
- Stale comments still name the deleted `WorkBase_TreeNodeFree_14c30 / _e320` in several files'
  banners (other blocks' files included); comments only.
- B11 against my own previous commit was taken either from an object snapshot before the edit
  or by compiling the previous source with the same flags; the whole-tree runs canonicalised the
  renamed template instantiations (the scripts are in my scratchpad, not committed).
