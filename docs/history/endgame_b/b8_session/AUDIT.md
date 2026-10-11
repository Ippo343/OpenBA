# B8 independent audit (2026-10-10)

Scope: every non-merge commit of `33325fd7b..7113881ac` (236: the coordinator's 11 on main, COMP-A 11,
RT 3, COMP-B 47, GAME 42, ENT 12, MISC 35, RES 23, RES addendum 2, RT2 50) plus the two DEC commits
`c60271c1b` / `ed149bf42` the lead named (they sit just before the kit commit). 238 rows. Main moved on
while the audit ran (`1ac911425` / `1afc1589a` / `a2c5e39d6`, the HDR header deletion): NOT audited here.
Read-only: no build, no run, nothing written to Ghidra. Every commit was read with `git show`; the
`B8:` typing commits through their changed control-flow / guard / printf lines plus the body diff, every
`behaviour:` commit in full. Ghidra re-reads passed `program="Application-x64.exe"` explicitly
(`get_assembly_context` / `disassemble_function` / `get_xrefs_to` / `read_memory` for import names).

Evidence column: **G** = the quoted call sites / instruction ranges / cell were re-read in Ghidra and
match; **M** = the diff was read against the message (and the site's comments); no Ghidra re-read.
G covers 62 `behaviour:` commits (the brief asks for 25) and 28 RT2 serializer families' cells
(the brief asks for 15, plus all bucket-tree keys, HighlightGroup's EdgeMesh and the WorldGeometry
MaterialInstance families: all G).

| verdict | count | commits |
|---|---|---|
| OK | 231 | the rest |
| FINDING | 5 | 8502fb27c, 681dd0490, 3d9001955, c6dd91ced, eede54704 (plus F6, a row finding: no commit is wrong) |
| QUESTION | 2 | ed149bf42, 9723083c0 |

## Mechanical checks (at 7113881ac, before the HDR merge landed)

```
castmetric.py code runtime standalone dx11 engine | tail -2   ->   # castmetric total: 3
acceptance.sh | tail -14:
01 counted casts in PROD (target = leftfree_c.py DEFERRED + RULED groups) = 3 (target 3)
08 exe_bodies.h / *_funcs.h declarations not named in LEFT_FREE.md = 0 (target 0)   [121 declared]
09 code/Game functions with a void* first parameter not named in LEFT_FREE.md = 0 (target 0)   [39 such functions]
10 fields of layout-asserted classes with no static_assert(offsetof) = 19 (target 0)
ACCEPTANCE: 1 line(s) off target
```
The one line off target is 10 = 19, the accepted value (B8_RULES §6). 08 / 09 read 0 against the OLD
LEFT_FREE.md (not regenerated; it still names every survivor, 412 -> 121 declared): re-run after
`b8_session/leftfree/`. Not fixed here.

## FINDINGS -- what the coordinator must do

- **F1 `8502fb27c` (GAME, row 1123): the quoted retail address does not support the claim.** The message
  says BindUISounds "runs inline in the slot (part of 0x140099260) on the slot's `this`". In retail
  0x140099260 is a separate function, GameContainer::ApplyKeyMapToUI (b3debadb4's own subject). The menu
  loop's Startup calls it at 0x14009b964-0x14009b96b with `RCX = [0x1401c3de8]` (the container global)
  and `RDX = this+0x40`, untested. The port's BindUISounds is a partial second copy of it. It copies the
  five sounds but skips SetFullscreenFadeSprite and the `+0xdc` UI-scale store, and it keeps a
  `g_gameContainer == nullptr` return retail lacks. Row 1123 marks this piece FIXED. **Do:** a new
  §14 row, then a `behaviour:` commit: Startup calls `g_gameContainer->ApplyKeyMapToUI(&_ui)` and
  BindUISounds is deleted. Correct row 1123's cell to say the object test went but the body is a second
  port.
- **F2 `681dd0490` (MISC): a behaviour change under a `B8:` title (D1).** The seven finders' port-only tests
  went with the bodies. Those were the description / out / type-id / slot null tests and
  IsDerivedFromNullSafe, now replaced by the member IsDerivedFrom. The content is right: the four
  quoted call sites 0x140104f00 / 0x1400fc809 / 0x1400fe103 / 0x14004841b are G, and the bodies open
  `CMP [RCX+0x30]` untested. ENT's precedent 1a9cdecbb was titled `behaviour:`. **Do:** nothing to
  revert. Record in GATE_TABLE / CLOSE_NUMBERS that 681dd0490 is a behaviour commit (C3 shape), gated
  by MISC's row.
- **F3 `3d9001955` (MISC): title counts contradicted by the diff.** The title says "17 rows ruled: 12 free
  in retail, 5 stay free". The diff and body have 18 lines: 12 free and 6 stay free. Documentary only.
- **F4 `c6dd91ced` (MISC): title counts contradicted by the diff.** The title says "18 free in retail,
  10 harness, 8 stay free". The diff and body have 16 free, 9 harness and 11 stay free (36 total
  matches). Documentary only. Regenerate LEFT_FREE from the lines, not from these titles.
- **F5 `eede54704` (MISC): wrong cross-reference.** It says the factory test "is RES's row 1063";
  ResolveExternalByName_Impl is row 1064 (1063 is Expire). Documentary only.
- **F6 row 1143 (status cell contradicts the code).** 451c77cd8 wrote "RES row 1061 owns the
  `liveSeatShuffle` gate around its one call", true then. 915f315f4 deleted the gate and did not update
  1143. **Do:** append "(the gate is gone, 915f315f4)" to the cell.

## QUESTIONS

- **Q1 `ed149bf42` (DEC):** SpawnSubResource keeps two tests retail lacks, both pre-existing: `if (!world)
  return;` (0x14011fbd5 reads `[0x1401c3dd8]` untested) and `if (spawnChanceDenom == 0) return;`.
  Retail divides `1.0f / (float)0` at 0x14011fc15, giving +inf, so the roll never skips there. The second
  test is observable only if some GrowthDescription has `+0x48 == 0`. **Settles it:** dump `_i48` over the
  Template\*.rsc growth descriptions. If any is 0, file a row.
- **Q2 `9723083c0` (RES, row 310 OnRemove):** the deleted DEVIATION comment named a concrete spin.
  HandleLifeEvent skips the removal queue for a member whose entity already has `+0x40 & 0x400`, so that
  member is never unlinked. The commit's termination argument (each pass unlinks the head) does not
  answer that case. **Settles it:** read retail HandleLifeEvent's 0x400 test and whether such a member
  can still be in the +0x260 ring. If retail spins too, it is a `docs/retail_bugs.md` entry, not a port
  guard.

## Retail differences the step found and LEFT (open rows, user decisions, kept guards)

- **User decisions:** 1162 (wg_f390_smooth: the port keeps 0.0f in the heightmap edge where retail
  copies uninitialised floats); 1202 Decal slot [4]; 1203 Map slot [6]; 1204 RawMaterial slot [9].
- **Port-only argument tests kept by GAME (object test dropped):** 1120 ValidateCell's [STANDALONE GUARD]
  range test; 1121 / 1122 the null cell-array returns; 1123 `!time` in TickParticles / TickAudioScene;
  1126 `!userInterface`; 1128 `!tmpl || !centre`.
- **SUSPECTED / OPEN, owed a `behaviour:` commit:** 1080 (FireComp_StartAudioEmitter_42c60, a second
  port of PlaySlot), 1140 (`source &&`), 1141 (profession test), 1142 / 1143 (`_singleton` tests),
  1163 (RangeResize, second hand port), 1164 (`s_tidCompEnt &&` x3), 1180 (BridgeTool's four tests),
  1181 (PickupRawMaterial ctor slot tests), 1200 (ProfessionUI OnOpened / OnElementEvent), 1201
  (ProductionUI's tracker deviation), row 310 PART FIXED (0x140046a90 LEFT to row 993).
- **Kept guards with a D4(a) comment:** 1200's OnOpened skip (the harness re-drive of slot 5);
  DebugUI_Slot9 stays free (disaster_harness passes a null DebugUI on purpose); firecomponent_ignite's
  caller-side `if (eventLog)`; orchard TickClearing's null-ClearAreaComponent return ("retail would
  fault here", pre-existing).
- **Not rowed, noticed while verifying (pre-existing, not this step's lines):** SerializeAll's
  `if (m_citizenList)` (retail 0x140095c81 calls 0x14008f760 untested); GameMain's `if (settings)` around
  Save (retail 0x1400422a3 calls before `TEST RBX,RBX`); GetResourcePrice's `vd ?` / `rawMat ?` tests
  (0399e5f43 says so); SpawnSubResource's two tests (Q1); b3debadb4 quotes 3 of the 4 retail callers of
  0x140099260 (0x140072832 not quoted); upgradeui.cpp's comment still names the deleted
  RemovalCalc_StateMachine_dfa0.

## 1. Verdicts, one per commit

### DEC (before the kit) and the coordinator on main

| commit | block | title | verdict | ev. | reason |
|---|---|---|---|---|---|
| c60271c1b | DEC | NaturalResourceComponent slot 8 installed | OK | G | 0x140125c40 matches the member Tick line for line (find, IsAnimating, bit 4, refresh, 0x1401260f0). |
| ed149bf42 | DEC | SpawnSubResource placement test wired, 1.0f | QUESTION Q1 | G | 0x14011fc0d 1.0f and the 10-arg 0x14009f5d0 call match; two pre-existing tests retail lacks. |
| 659fb8925 | main | gate table row DEC completed | OK | M | GATE_TABLE only. |
| 07b868800 | main | known_port_issues entry 3, rows 5 / 552 closed | OK | M | Doc only; matches the two DEC commits. |
| 7dc106db1 | main | kit: b11pair.py, C1 / C2 | OK | M | Script + rules only. |
| 70b86d6f9 | main | gate row COMPA, C3 / C4 | OK | M | Docs only. |
| c0d6c8a0f | main | gate row RT | OK | M | Docs only. |
| b829c5aa8 | main | B8_RULES C5 / C6 | OK | M | Docs only. |
| da0198529 | main | gate row COMPB, disaster_route.sh | OK | M | Docs + kit script. |
| e04608ad1 | main | gate row ENT | OK | M | Docs only. |
| d239d0478 | main | gate row MISC | OK | M | Docs only. |
| 16235032e | main | gate rows RES / RESadd / GAME, harness_route.sh | OK | M | Docs + kit script. |
| 7113881ac | main | gate row RT2 | OK | M | Docs only. |

### RT

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| aaccb6a5f | Array<T>::SetSize specialisations (4) | OK | M | Bodies verbatim; the Trade callers now pass the array (retail RCX), displacements rebased; said. |
| f0e7068f7 | ActionStack::Clear | OK | M | Body verbatim, 47 call lines `x.Clear()`. |
| 8ee7deab4 | 53 rows stay free, rows 1160 / 1161 | OK | G | Comments only; NameDB_GenerateName RCX = R12 return slot confirmed at 0x1400a4b66. |

### COMP-A

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 6fe502110 | CharacterComponent 2 members | OK | M | `character->` dropped; callers as written. |
| e79cb617c | CharacterGrid 2 members | OK | M | Walker `Init(this, ...)`; callers as written. |
| e42567b34 | CitizenComponent 9 members, 2 forwarders | OK | M | Infect's null arm dead (debugui `if (!building) return;`, harness collects non-null). |
| 1d0e54a3f | Component 6 members | OK | M | Pure moves; two rows re-tagged as function-pointer sites. |
| 5b64fcedd | ClearAreaComponent 2 forwarders | OK | M | Both callers test first (buildcomponent_state1 :243, orchard TickClearing). |
| e0b66ca6a | ConsumeProduceComponent 2 members | OK | M | Pure moves. |
| fdc97f9e8 | CropFieldComponent 1 member, 1 forwarder | OK | M | `this->slots.data` for the shadowing local; cropui returns on null first. |
| 4d8a9e3d7 | AutoPickupComponent::Append | OK | M | Pure move. |
| 52984cbd7 | AmbientEnvironment forwarder | OK | M | starttown tests `envComp` first. |
| 838270ae9 | AmbientEmitter 0 moved, row 1080 | OK | M | Re-tag + row. |
| a0732cf64 | ComponentDescription 0 moved | OK | G | 0x140088faa `MOV RBX,RDX`, RCX unread: ruling holds. |

### COMP-B

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 8eceacb2b | StorageComponent 4 members, QueryCount forwarder | OK | M | 38 call sites as written; QueryAvailable left (row 1100). |
| f1ee4e86d | TradeComponent GetOrderName | OK | M | Pure move; four guarded rows left (1101-1104). |
| ae5aaa4c8 | GrowthComponent::SetProgress | OK | M | Pure move. |
| 5688fd28a | GrowthDescription::CheckFootprint | OK | M | `owner == this`; pure move. |
| f214a6f27 | HerdComponent::StartMigration | OK | M | Pure move. |
| 01a074a91 | HuntComponent::IsReady | OK | M | Pure move. |
| 0dbb19e4c | InteractComponent 3 members | OK | M | Pure moves; FindWalkablePos left (fold, later C5). |
| a6ae282e4 | ModelComponent 4 members | OK | M | Pure moves. |
| 73f4cd34f | NaturalResourceComponent 2 members | OK | M | Pure moves; PlaceAtCell left (fold). |
| ba83d1a28 | PastureComponent 1 member, 1 forwarder | OK | M | Forwarder's `!pasture` dead (caller reads storage first); IDENTICAL. |
| 8c122594d | PopulationComponent::PickTargetCell | OK | M | Pure move. |
| 1a4744dab | RadiusDescription::AddHighlight | OK | M | Pure move. |
| 027e7e16d | ResidenceComponent::CheckHeating | OK | M | Pure move. |
| 618c2ada0 | ResourceProduceComponent::RequestProduce | OK | M | Pure move. |
| 552dcafda | TrackerComponent::Upsert | OK | M | Pure move. |
| 7290e94b4 | HighlightComponent::Repaint | OK | M | Anonymous namespace closed around the body; pure move. |
| bee1c78e8 | DiseaseComponent 2 forwarders | OK | M | Unguarded forwarders; harness calls the members. |
| 217a9a030 | ParticleComponent forwarder | OK | M | Unguarded forwarder. |
| f86e6510d | PickingComponent forwarder | OK | M | Its `!pc` dead: the 6 callers test first. |
| 6336e94d8 | SpawnComponent forwarder | OK | M | Caller returns on null first. |
| 97675a36f | MapComponent forwarder | OK | M | Caller returns on a null map first. |
| 361fe76d2 | WorkPlaceComponent forwarder | OK | M | Callers test `wp` first. |
| 96d421abf | DestroyComponent 4 rows left | OK | M | Comments + rows 1105-1107. |
| f937713ed | FireComponent 1 row left | OK | M | Comment + row 1108. |
| 1be4749fa | DecalComponent 1 row left | OK | M | Comment (slot [4], user decision 1202). |
| 1111cad83 | RawMaterialComponent 1 row left | OK | M | Comment (slot [9], 1204). |
| 9599107be | WorkComponent 1 row left | OK | M | Comment (address printed). |
| 6e7cb27cc | blank line | OK | M | Whitespace. |
| b4668eee3 | DiseaseComponent comment lines | OK | M | Comments. |
| 3379ab9cb | ParticleComponent comment line | OK | M | Comment. |
| 6f6cb501f | PickingComponent comment lines | OK | M | Comments. |
| 7f4322712 | SpawnComponent comment lines | OK | M | Comments. |
| 02835092f | HighlightComponent blank line | OK | M | Whitespace. |
| ae54b39d0 | behaviour: QueryAvailable deleted (1100) | OK | G | 0x1400e42f7 find -> 0x1400e430e CALL 0x14012ce70, untested. |
| 6f4d440df | behaviour: GetBuyEntries (1101) | OK | G | 0x14005b6f5 `MOV RCX,R15`, 0x140083812 `MOV RCX,[RBP+0x6f]`, untested. |
| 02182ca39 | behaviour: ExtendMerchantStay (1102) | OK | G | 0x140059af8 and 0x14008149d, untested. |
| 1d38606b4 | behaviour: AddPurchaseOrder (1103) | OK | G | 0x140081e5d `MOV RCX,[RSP+0x50]` -> 0x140081e66, untested. |
| 350361cc7 | behaviour: GetResourcePrice object test (1104) | OK | G | 0x140059bde / 0x1400829a8 / 0x140084053 untested; `!resource` left (RES did it). |
| 2feda2ab5 | behaviour: StopBurning (1107) | OK | G | 0x14004953a find -> 0x14004954f, untested. |
| 909b8d2c1 | behaviour: IsBeingDemolished (1105) | OK | G | 0x140049647 -> 0x14004965a, untested. |
| 97cb79cc4 | behaviour: BeginBurning (1106) | OK | G | 0x1400496e8 / 0x1400496f0, untested. |
| d7533115f | behaviour: Ignite_610 deleted (1108) | OK | G | 0x14005ff0a find -> 0x14005ff1f, untested. |
| 73ba67802 | InteractComponent::FindWalkablePos (C5) | OK | G | 0x14010d8bc reads [R13+0x30] after the call untested: the folded test is retail-lacking. |
| b59d4da71 | NaturalResourceComponent::PlaceAtCell (C5) | OK | M | Folded port-only diagnostic term; calls equal. |
| 10e88a9de | WorkComponent::PostWork, %p dropped | OK | M | C6: only `PostWork=%p` and its argument removed. |
| e8460c3f6 | DestroyComponent 4 forwarders, 2 %p | OK | M | C6: tornado `(e690..ea10)` and UpgradeUI `(driver=%p)` fields + args only. |
| f0334c94a | WorkPlaceComponent forwarder, %p | OK | M | C6: `toggleFn=%p` + arg only. |

### GAME

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 9afadb447 | Map 13 members, RoadAStar | OK | M | RoadAStar callers spell the `nullptr` 5th arg; SelDiag C6 drop correct. |
| cf28f0cf7 | MenuDialogLoopT 9 members | OK | M | Left a stale InitState declaration and CR-damaged lines; repaired by 3270cce01. |
| 4520f7464 | WorldGeometry 6 members | OK | M | Pure moves; two SelDiag `MapHit=%p` fields + args dropped only. |
| 277330b7f | PlayerToolManager 6 members | OK | M | Pure moves; harness `fc60=%p` field + arg only. |
| 1a73d2c4f | WorkScheduler 7 members, Goals forwarder | OK | M | The forwarder's two extra args were dead; caller's `if (sched)` untouched. |
| 7dea23b35 | GameUpdateLoop::SetEngineNextLoop | OK | M | Pure move. |
| dd0c0b94f | MapGenerator::SeedStartingResources | OK | M | Pure move. |
| 2ce6a7573 | GameContainer 0 moved (1126) | OK | M | Comment + row. |
| 737d7c3c8 | GameSettings 0 moved (1127) | OK | M | Comment + row. |
| 247c3e413 | Minimap::ClearTerrain | OK | M | Pure move. |
| 1d29b8a4d | PlayerController::WorldToScreen | OK | M | Pure move. |
| c7650bfab | PathfinderThread::QueryPath | OK | M | Pure move. |
| 14aa9ce3a | WeatherSimulation::Teardown | OK | M | Pure move. |
| be4268ea7 | ProfessionList forwarder | OK | M | Unguarded forwarder. |
| 410e02fd7 | nine guarded forwarders ruled (1129) | OK | M | Comments + row. |
| 3270cce01 | MenuDialogLoopT stale decl, line endings | OK | M | Repairs cf28f0cf7; file stored LF again. |
| fca4de538 | behaviour: EventLog_AddEvent forwarder | OK | G | 0x14008c1c8 `MOV RCX,R15`, 0x140096937 `[RSI+0x138]`, untested. |
| b7e8a721f | behaviour: AddCitizenEvent forwarder | OK | G | 0x140113c61 call, untested. |
| 2b407679e | behaviour: TradeWork_impl_04d0 | OK | G | 0x140095c6b `[RDI+0x110]` -> 0x140095c72. |
| b0a4bf67e | behaviour: Transform_SerializeWrapper | OK | G | 0x140095c4d `[RDI+0xe0]` -> 0x140095c54. |
| 3f859dff1 | behaviour: TradeWork_helper_e270 | OK | G | 0x1400acace LEA ret slot, 0x1400acad8 CALL; by-value Vector3 equivalent. |
| b8f30eed3 | behaviour: Minimap_PaintTerrain | OK | G | 0x1400960eb `[RDI+0x148]`, RDX = RSI, 0x1400960f5. |
| 5aeb216e3 | behaviour: MusicPlayer_Update | OK | G | 0x14009656d `[RDI+0x80]` -> 0x140096574. |
| 41691eb1f | behaviour: Weather_ResetTime | OK | G | 0x140060856 and 0x140096458, untested. |
| e125fff46 | behaviour: WeatherParticle_Config | OK | G | 0x14006097a / 0x14006097e -> 0x140060982. |
| 1ec50768c | behaviour: Map::ValidateCell (1120) | OK | G | 0x1400a010c `MOV RBX,RCX`, FillZones, `[RBX+0x80]`; range guard LEFT, said. |
| 39b6a5576 | behaviour: Map::StampCellRefs (1121) | OK | G | 0x14009f770 `TEST R9D,R9D` only; RCX used untested. |
| 1c1056fa9 | behaviour: Map::PlaceRoadCell (1122) | OK | G | 0x14009c987 / 0x14009c99c untested; RENAMED-DIFF is the compare's encoding. |
| 74084898e | behaviour: TickParticles (1123) | OK | M | Caller is the slot's `this`; `!time` LEFT. |
| f930b392c | behaviour: LoadMusic (1123) | OK | M | Caller is the slot's `this`. |
| 628ee2369 | behaviour: StartMusic (1123) | OK | M | Caller is the slot's `this`. |
| b28d55d23 | behaviour: StopMusic (1123) | OK | G | 0x14009b413 `LEA RCX,[RBX+0xb30]` -> 0x14009b41a. |
| 8502fb27c | behaviour: BindUISounds (1123) | FINDING F1 | G | 0x140099260 is a CALL on `[0x1401c3de8]`, not inline on `this`; a partial second port. |
| c027f77d6 | behaviour: TickAudioScene (1123) | OK | M | Caller is the slot's `this`; `!time` LEFT. |
| fe01693e4 | behaviour: PhaseBState::TickChunk (1123) | OK | M | Port-only chunking; both callers hand a live state. |
| f25bc40a3 | behaviour: StoreBatchMaterial (1124) | OK | G | 0x140104f2a call; body's first read [RCX+0x218]. |
| f78027b71 | behaviour: TutorialTreeInsert (1125) | OK | G | 0x1400d7b7a / 0x1400e1d78 `[RCX+0x120]` untested; C6 field + arg only. |
| b3debadb4 | behaviour: ApplyKeyMapToUI (1126) | OK | G | 0x140099260 body and xrefs read; quotes 3 of 4 call sites (0x140072832 not). |
| 2b961618c | behaviour: GameSettings::Load (1127) | OK | G | 0x14004203d / 0x1400727f4 untested. |
| c7f41bb22 | behaviour: GameSettings::Save (1127) | OK | G | 0x1400422a3 calls before `TEST RBX` (GameMain's own `if (settings)` pre-existing). |
| 657e10010 | behaviour: SpawnStartingHerd (1128) | OK | G | 0x1400a35d9 `MOV RCX,RDI` -> 0x1400a35dc; arg tests LEFT. |
| 0bd575571 | known_port_issues 1120-1129 cells | OK | M | Cells match the code (1123's BindUISounds cell: see F1). |

### ENT

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 76ac2ea3d | TradeUI 4 forwarders | OK | M | Unguarded; harness returns without a TradeUI. |
| 8601368ef | NoiseUI slots -> OnCreated / OnElementEvent | OK | M | Slot 9's (msg, id) still the slot's (a, b). |
| a65a2996c | behaviour: MainMenuDialog::LoadFromSave | OK | G | 0x14006ea53 / 0x14006ecad `[+0xd0]`, 0x140077e4c `MOV RCX,RBX`, untested. |
| 9f0a2b282 | EntityList::Shutdown, 2 doors | OK | M | Placement-new folds a dead re-test (C5 kind); harness C6 fields + args only. |
| 054b4373b | DebugUI / PathGraph re-tagged | OK | M | Comments. |
| f528842bb | ComponentEntity::MarkForRemoval, forwarder | OK | M | Pure move; two harness `c2ea0=%p` fields + args only. |
| e2463db39 | behaviour: TradeUI_OnTradeButton | OK | M | Slot 9 tail-jumps with its own `this` (0x140082055); harness tests first. |
| 6a4265a67 | CitizenBehavior 14 members | OK | M | Pure moves; row 1140 filed. |
| a82cfbafd | behaviour: AddWorker | OK | M | No retail code caller (pdata only); harness tests first; 1141 filed. |
| 37f69bdc0 | behaviour: RemoveWorker | OK | G | 0x14007655d `[RCX+0xf0]`, 0x1400878d7 `MOV RCX,RBP`, untested. |
| 451c77cd8 | behaviour: ShuffleProfessionList | OK | G | 0x14008d118 `MOV RBX,RCX`; 0x140089476 reads [RCX+0x100] first. |
| 1a9cdecbb | behaviour: the FindComponent family | OK | G | 0x140006270 walks [RCX+0x38] / slots with no test; ProductionUI kept (later RES). |

### MISC

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| d53264212 | BridgeTool 2 members | OK | M | Pure moves; `Snap=%p Validate=%p` + args only; row 1180. |
| cf20f87f6 | EditTool 3 members, 2 forwarders | OK | M | Bodies in place; CloseInfoDialog param kept, said. |
| 9db7d33b0 | FencedTool::CalcAnchor | OK | M | Pure move; shadowing locals read `this->`. |
| 585195630 | MoveTo 2 members | OK | M | Pure moves; stale-register params kept, said. |
| 59c612e2f | DropOffRawMaterial 3 members | OK | M | DropOnGround reads `_field30` after the count test as retail; signature change said. |
| d1f8738d3 | ActionStack Execute / UpdateAnimation | OK | M | Pure moves. |
| d59601280 | RegionPairCache forwarder | OK | M | Unguarded forwarder. |
| ff6385b44 | PickupRawMaterial ctor | OK | M | Real ctor; the inlined base ctor is the DIFF (C2 kind); slot tests kept (1181). |
| 489909a14 | row 1181 | OK | M | Doc. |
| f0efc207d | BuildRoad door | OK | M | Caller already inside `if (mem)`. |
| 746a8d744 | BuildStructure door | OK | M | Callers inside `if (mem)`. |
| 171d365ff | GatherRawMaterials door | OK | M | One dead `cmpq $0` folded (placement-new). |
| 732caa71a | TendArea door | OK | M | Caller inside `if (mem)`. |
| 37b5f13e1 | ClearArea door | OK | M | `&entity` for the door's copy (ctor reads it once); dead test folded. |
| c5ae66ca1 | four Dialog ctor doors | OK | M | Locals passed by value; OptionsDialog printf moved to the caller unchanged. |
| 5b0a8a183 | WeatherParticle_GeoCtor door | OK | M | No caller left. |
| eede54704 | NamedExternalRef::ResolveDescription | FINDING F5 | M | Move is right; cites RES row 1063 for 1064. |
| 4cb83ec3b | WorkBST Remove / FindRemove template | OK | M | Template text equals the deleted core; DIFFs read as inlining (D5). |
| 55dffc556 | PathGraph 2 members | OK | M | Retail caller's `MOV RCX,RBX` decides (D2). |
| 8f5d0de3d | Map::SortTilesByDistance | OK | M | Pure move. |
| b1d931284 | Random forwarder | OK | M | 23 calls `rng->RandFloatTol()`; callers' `rng ?` untouched. |
| 5e6c877b2 | Action/exe_bodies.h 7 rulings | OK | M | Comments only; counts match. |
| 77970dc30 | Component/exe_bodies.h 20 rulings | OK | M | Comments only; 13 / 1 / 6 match. |
| 174690873 | diseasecomponent_funcs.h ruling | OK | M | Comment. |
| b720a46eb | spawncomponent_funcs.h ruling | OK | M | Comment. |
| 817c53d25 | Dialog/exe_bodies.h 3 rulings | OK | M | Comments; 2 / 1 match. |
| 60e557b61 | Game/exe_bodies.h 2 rulings | OK | M | Comments. |
| 1fc146af8 | menudialogloopt_funcs.h ruling | OK | M | Comment. |
| d52022198 | edittool_funcs.h 7 harness | OK | M | Comments; 7 match. |
| 3d9001955 | Player/exe_bodies.h rulings | FINDING F3 | M | Title 17 = 12 + 5; diff 18 = 12 + 6. |
| 019d3a3c6 | roadtool_funcs.h ruling | OK | M | Comment. |
| c6dd91ced | Work/exe_bodies.h rulings | FINDING F4 | M | Title 18 / 10 / 8; diff 16 / 9 / 11. |
| 681dd0490 | FindComponent / FindDescription instances | FINDING F2 | G | Tests dropped under `B8:`; the dropped tests are retail-lacking (sites G). |
| fded8fc5f | HarvestOrchard unused local | OK | M | The call stays; the unread local goes. |
| 0cd8f3b8d | PathSetup / CloseInfoDialog trimmed | OK | M | Dropped args were plain reads; HighlightClear does not write +0x28 (read). |

### RES and RES addendum

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 974be1497 | behaviour: _padADC store (1020) | OK | G | 0x1400711af stores +0xae0; nothing writes +0xadc. |
| 1d7cbce28 | behaviour: Expire null ring (1063) | OK | G | 0x1400a5d23 / 0x1400a5d2e read the ring untested. |
| 5e2192355 | behaviour: ResolveExternalByName (1064) | OK | G | 0x14001b1d0-0x14001b262 read whole: no factory test, dtor at 0x14001b223, re-read 0x14001b24d. |
| 209dbf569 | behaviour: EvictRoster (1060) | OK | G | 0x140130068 `CMP [RSI+0x60],0; JG`: no progress test. |
| 915f315f4 | behaviour: shuffle unconditional (1061) | OK | G | 0x14008d11b CALL untested; flag + reader deleted, offsets kept. |
| a9a25b15a | behaviour: ProfessionUI::OnUpdate (1062) | OK | G | 0x1400766ae `MOV RCX,R14` on a miss then [RCX+8]: no test. |
| fe93dac32 | behaviour: RegisterStorageLocation bool (1065) | OK | G | 0x14012d25a `XOR AL,AL`. |
| 4cbe4ac44 | behaviour: AddEventCore (1000) | OK | G | 0x1400955d1 GetText on [R12] untested; 0x140095712 re-read, untested store. |
| 0399e5f43 | behaviour: `!resource` (1104) | OK | G | 0x14005a544 `CMP [RDX+0x30]` untested. |
| 0651cbc88 | behaviour: FireComponent::Update (1041) | OK | M | Same three-instruction shape as the G-read siblings. |
| bf849210e | behaviour: FireDesc x5 (1041) | OK | G | 0x140049682-0x14004969d (Ignite): `[R12+0x18]`, cell 0x1401c4218, CMOVNZ, no test. |
| 34a57c71f | behaviour: StartAudioEmitter type test (1041) | OK | G | 0x140042c7b cell read fresh, 0x140042c87 call, no test. |
| 32088938e | behaviour: GetParticleDesc x7 (1041) | OK | G | 0x140053698 / 0x1400536a2 CMOVNZ, no description test. |
| 963fe2f2a | behaviour: ComponentEntity ctor (1041) | OK | G | 0x1400c3829-0x1400c3840; helper deleted with its last caller. |
| 92b604d28 | behaviour: 0x1400049c0 callers (1040) | OK | G | Cell 0x1401c7318 at 0x140004a47 / 0x140004bad; the four call sites. |
| 0afe91435 | behaviour: SerializeExternalArray<Description> (1040) | OK | G | 0x140118b65 -> 0x140032090, cell 0x1401c72e0 (Description_Register). |
| 7cc1be730 | behaviour: SetHome (310) | OK | G | 0x14011644d / 0x1401164a3 on the find's result, untested. |
| 9723083c0 | behaviour: OnRemove (310) | QUESTION Q2 | G | 0x140113cb3 HideTool unconditional, 0x140113d9c [RDI+0x288] untested; spin case unanswered. |
| 4474a7220 | behaviour: needs-update tests (310) | OK | G | 0x14010fe01 [RDI+0x280] -> SetFlag untested. |
| a5f57416f | behaviour: AssignCitizens tests (310) | OK | G | 0x1400d483c `CMP [RAX+0x60]` on the find result, untested. |
| b3ac4099a | behaviour: ProductionUI null entity | OK | G | 0x140075ab2 FindComponent on the cast result, untested; bind order argued. |
| 2e69f6a18 | known_port_issues RES cells, rows 1200-1204 | OK | M | Cells match the code (1143's later wording: F6). |
| 927418391 | tool_harness pick / panelclose routes | OK | M | Harness only, no getenv; gated on RES's row. |
| 769d22cf9 | behaviour: Infect tests (1205) | OK | G | 0x140048413-0x14004842c untested (read with 681dd0490). |
| f5256fdce | row 1205 | OK | M | Doc. |

### RT2 (rows 1160 / 1161)

| commit | title | verdict | ev. | reason |
|---|---|---|---|---|
| 76786280b | ExternalPtr<MapGenerator> | OK | G | Cell 0x1401c59b8 read in 0x140019ec0; MapGenerator_RegisterType. |
| ea8253488 | ExternalPtr<TerrainGenerator> | OK | G | 0x1401c5b40, TerrainGenerator_RegisterType, read in 0x14001a0c0. |
| e75d3400b | ExternalPtr<ClimateGenerator> | OK | G | 0x1401c5910, ClimateGenerator_RegisterType, read in 0x1400194a0. |
| 5ddb77ba9 | ExternalPtr<EnvironmentDescription> | OK | G | 0x1401c5980 correct; undocumented CRLF churn, reverted and explained in f462c34ec. |
| 8afbd13c2 | ExternalPtr<Toolbar> | OK | G | 0x1401c6128, Toolbar_RegisterType, read in 0x140024c70. |
| c7b1d0936 | ExternalPtr<Weather> | OK | G | 0x1401c6198, Weather_RegisterType, read in 0x1400196a0. |
| f874cb8fd | ExternalPtr<GraphicsMesh> | OK | G | Slot 0x140151f20 = `?_typeInfo@GraphicsMesh@Graphics@@`. |
| 3244fa0e2 | ExternalPtr<Particle> | OK | G | Slot 0x140151420 = `?_typeInfo@Particle@Graphics@@`. |
| 1df118df7 | ExternalPtr<AnimationGroup> | OK | G | Slot 0x140152980 = `?_typeInfo@AnimationGroup@Graphics@@`. |
| 72eb2f3f4 | ExternalPtr<DialogControllerConfig> | OK | G | Slot 0x1401529c0 = `?_typeInfo@DialogControllerConfig@UI@@`. |
| 950d87134 | ExternalPtr<PointList> | OK | G | Slot 0x140151448 = `?_typeInfo@PointList@Pathfinding@@`. |
| 776b772f5 | ExternalPtr<PickingMesh> | OK | G | Slot 0x140151468 = `?_typeInfo@PickingMesh@Picking@@`. |
| c0231890c | ExternalPtr<UI::Dialog> | OK | M | Callers all on UI::Dialog; TutWave helper deleted. |
| 5bc0647db | ExternalPtr<ElementDescription> | OK | M | 5 callers all on UI::ElementDescription. |
| 1d81025cf | ExternalPtr<SoundEffect> | OK | M | 5 callers on Audio::SoundEffect; Ambient's relaxed core deleted. |
| a9dd96441 | ExternalPtr<SpriteSheet> | OK | M | 10 callers on UI::SpriteSheet. |
| 7d9ba59ce | ExternalPtr<MaterialInstance> | OK | G | Slot 0x140151350 = `?_typeInfo@MaterialInstance@Graphics@@`; Highlight / WorldGeometry sites G. |
| 2ea1c9dc9 | ExternalPtr<StringTable> | OK | G | Slot 0x140151488 = `?_typeInfo@StringTable@UI@@`. |
| 6055de3b7 | bucket-tree keys | OK | G | 0x140017ab3 / 0x140017c5a -> 0x140018020, 0x140017d43 / 0x140017eea -> 0x140002340; T*& forces Cell. |
| c6858645b | ExternalPtr<Profession> | OK | G | 0x1401c5a98, Profession_RegisterType, read in 0x140018020; holder typed. |
| db5e00854 | ExternalPtr<ComponentDescription> | OK | G | 0x1400023a5 `MOV EDX,[0x1401c7318]`; four callers G; names a pre-merge symbol (merge resolved). |
| c38a7e9c2 | ExternalPtr<CharacterDescription> | OK | G | 0x1401c70b0, CharacterDescription_Register, read in 0x140022970. |
| 989bb2491 | ExternalPtr<Tutorial> | OK | G | 0x1401c6400, Tutorial_RegisterType, read in 0x1400262b0. |
| 2f0813b28 | the 60th caller, UpgradeUIConfig | OK | G | Same cell / body as db5e00854. |
| 7457c0e3a | ExternalPtr<Description> | OK | G | 0x1401c72e0, Description_Register, read in 0x140031d10. |
| 4701b0390 | ExternalPtr<Disease> | OK | G | 0x1401c5948: its registrar passes the name "Disease" (0x14015e188); Ghidra mislabels it. |
| 59b38b324 | ExternalPtr<CitizenDescription> | OK | G | 0x1401c7190, CitizenDescription_Register, read in 0x140030990. |
| 451b26d3b | ExternalPtr<PopulationDescription> | OK | G | 0x1401c47c8, PopulationDescription_Register, read in 0x14000b1b0. |
| e6e80335c | Array<GraphicsMesh> | OK | M | 3 callers on GraphicsMesh (single form's slot G). |
| 0a69ceecc | Array<Particle> | OK | M | 1 caller on Particle. |
| 037255ea8 | Array<EdgeMesh> (HighlightGroup) | OK | G | 0x140007607 slot 0x140151330 = `?_typeInfo@EdgeMesh@Graphics@@`; 0x14004ca7d; holder typed. |
| f462c34ec | Array<MaterialInstance> | OK | G | 0x1400201d7 slot 0x140151350; 0x14011b9fc / 0x1400b0b3d / 0x1400af340 G. |
| afc5b543c | Array<ComponentDescription> | OK | G | 0x1400049c0 cell 0x1401c7318 (0x140004a47 / 0x140004bad). |
| e2bdc01bc | Array<TutorialStep / Image / Setup / Goal> | OK | G | Four cells, their registrars and callers 0x1400deb35 / 0x1400e333e / 0x1400e337b / 0x1400e33b8. |
| 14954dba6 | Array<PickingMesh> | OK | M | 1 caller on Picking::PickingMesh. |
| bf518433d | generic cores deleted | OK | M | No caller left (grep agrees: no IsDerivedFromOrTrust / Desc_LoadExternalArray). |
| 77173017a | Array<int32>::SetSize | OK | M | (h) each caller writes or stream-reads the tail first (Load memset, CharacterGrid fills). |
| 2f1511125 | Array<float>::SetSize, row 1162 | OK | M | (h) heightmap loop covers edge x edge; carve masks filled 1.0 / 0.0; smooth kept on fill (1162). |
| 0921dfead | Array<int16>::SetSize | OK | M | (h) every site stream-reads or writes all N*N. |
| 45b3d3506 | Array<uint8>::SetSize | OK | M | (h) BuildStructure zero-fills; the others stream-read. |
| 87ce015d4 | TownHallUI SetSize(0) on its 8-byte array | OK | M | Count 0: every body stores size only. |
| 8644e2bf8 | Array<TerrainVertex>::SetSize | OK | M | (h) stream-read / FillTerrainVertices before upload. |
| 6aef7958c | Array<GeometryDrawRange>::SetSize, row 1163 | OK | M | (h) four ints stream-read. |
| c7785282f | Array<NameDatabaseEntry>::SetSize | OK | M | (h) fields stream-read; pad unread. |
| c4334021d | Array<Array<Vector2>>::SetSize | OK | M | Hand row's three arms match the quoted ranges. |
| fc344b355 | Array<String>::SetSize | OK | M | String trait row = copy ctor / String() / ~String. |
| ccd0ddc32 | Array<CropSlot>::SetSize | OK | M | (h) State9Enter writes cell / ptrs / state; the old body had the same init. |
| 6b785ebc1 | three element-loop serializers, row 1164 | OK | M | Comments + row. |
| 3ba93fb39 | orphan banner removed | OK | M | Comments. |
| 7a5132456 | rows 1160 / 1161 FIXED notes | OK | M | Doc; matches the code. |

## 2. known_port_issues spot-check (e)

Read: 1000, 1020, 1040, 1041, 1060-1065, 1080, 1100-1108, 1120-1129, 1140-1143, 1160-1164, 1180, 1181,
1200-1205. Every status cell matches the code with two exceptions. Row 1143 is F6. Row 1123 calls
BindUISounds FIXED, which holds for the object test but hides the second port (F1). Spot-checks of
the code: Desc_LoadExternalArray and IsDerivedFromOrTrust have no definition or caller left (1040 /
1041). ValidateCell's range guard, StampCellRefs / PlaceRoadCell's cell-array returns and
`if (!userInterface)` are present (1120-1122 / 1126 LEFT). `source &&` is present (1140). The smooth
buffer keeps `RawArraySetSize(tmpHdr, cellCount, 0.0f)` (1162). `s_tidCompEnt &&` is present at three
lines of gameupdateloop_serialize.cpp (1164).
