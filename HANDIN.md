# B9 RES hand-in

Branch `b9-RES`, base `cf62fb35a`, main merged at `1f620acd6` (docs only). Static gate on the merged tip:
debug / perf / unit builds exit 0, 0 new warnings (debug and perf against `warn_baseline_*.txt`), unit 920 / 0,
castmetric 3. The game was never run.

| row | ruling | commit | B11 (perf, against `b11base/perf`) | unsettled |
|---|---|---|---|---|
| 1120 | FIXED: `Map::ValidateCell`'s range guard dropped; both cell indices sign-extended as retail's MOVSXD | 89627da8e | work_base_reimpl IDENTICAL 17 SAME 1 (ValidateCell: the `test/js`, `cmp/jge` x2 gone; `movslq` for both cells) | the guard's route (unfilled MapComponent type id under BANISHED_FORCE_WORK) is gone by reading, not by a run; the -O2 / new-game routes are the check |
| 1121 | FIXED: `Map::StampCellRefs`'s null cell-array return dropped | 0d5414b0f | pasturecomponent_states IDENTICAL 22 SAME 3 (StampCellRefs and the inlined copy in PastureProduce lose `test r9/r11; je`) | callers' own tests filed as row 1241 |
| 1122 | FIXED: `Map::PlaceRoadCell`'s null cell-array return dropped | 9cbb65a87 | roadtool_calcpath DIFF 1 IDENTICAL 10 (PlaceRoadCell: the dropped `test %rsi,%rsi; je`; the compare-multiset line is the same `cmpb $0xff,0x2a(..)` with base %rbp -> %rdi) | the body's `!work` / singleton / scheduler tests filed as row 1240 |
| 1123 | FIXED: the `!time` tests of TickParticles / TickAudioScene dropped, plus TickParticles' empty-array return (it skipped `_frameIndex++` and `Scene::Clean`) and null-element skip; the Update slot's two `if (time)` wrappers removed | 633e74e89 | menudialogloopt DIFF 1 IDENTICAL 23 SAME 1 (TickParticles re-reads +0xbd8 per turn as retail); menudialogloopt_stubs IDENTICAL 24 | the slot's block shape (scene gate, the +0xbc8 slot-1 call, IsPaused / Pause, the port-only TickAudio) filed as row 1242 |
| 1126 | FIXED: `ApplyKeyMapToUI`'s `!userInterface` and `g_gameSettings` tests and the two OptionsDialog callers' `if (keyMap)` dropped | 1525789d6 | optionsdialog DIFF 1 IDENTICAL 38 SAME 3 (ApplyGameSettings' DIFF is the store base %r12 -> %rbp; the real change is the dropped container test) | GameLoadDialogLoopT::Startup lacks retail's ApplyKeyMapToUI call: row 1243; RestoreVideoSettings' singleton tests: row 1244 |
| 1128 | FIXED: `SpawnStartingHerd`'s `!tmpl` / `!centre`, `!w`, `!ent`, two `if (animal)` tests dropped; the group-count return moved after Rand(2) (retail always draws it) | cec96d9ec | pasturecomponent_spawn DIFF 1 IDENTICAL 11 (the no-animal path gone; Place / RandFloat on the finder's result) | SeedAgriPool / ResolveTemplate tests filed as row 1245 |
| 1200 | FIXED: the harness no longer re-drives ProfessionUI slot 5 (both tool_harness.cpp sites; AddDialog -> DialogInstance::OnOpened runs it); OnOpened's IsSetUp skip and OnElementEvent's test dropped, IsSetUp deleted | feb590406 | professionui DIFF 2 IDENTICAL 19 ONLY-BASE 1 SAME 2 (the counts / spinners compares gone; one Trace fewer); tool_harness DIFF 2 IDENTICAL 25 SAME 5 (the ProfessionUI drive calls gone) | needs the profession-UI utility harness route run: the claim that AddDialog's open is the one slot-5 run is read from the code, not observed |
| 1201 | FIXED: UpdateList's `!tracker`, OnUpdateLayout's `!Ready()` / empty-lists, OnCreated's `!Ready()` dropped; Ready / TypeIdsReady deleted | 1bca7c561 | productionui DIFF 3 IDENTICAL 14 ONLY-BASE 3 (Ready calls gone, tracker test gone, rebuild block re-laid) | the template proof is by construction (factory-built controllers from the entity's own UIDescription + retail's per-frame unguarded read), NOT by grepping templates: the extracted resources are not in the repo and the game dir is off-limits. `harness_route.sh panelclose` is the check |
| 1080 | FIXED: the copy `FireComp_StartAudioEmitter_42c60` and its declaration deleted; the caller on `em->PlaySlot(1)` (retail 0x14012a8a0 EDX = 1) | de936c7e1 | firecomponent_ignite IDENTICAL 13 ONLY-BASE 1; residencecomponent IDENTICAL 46 (the call sits behind the PORT GAP 0x140002280 null `em`, dead in both builds) | `firecomponent_funcs.h` is NOT empty (other declarations remain). PlaySlot's own five port-only tests filed as row 1246 |
| 993 | FIXED: 0x140046a90's `IsDerivedFromNullSafe(desc, ...)` -> `desc->IsDerivedFrom(...)`; retail has no test of desc (0x140046b74-0x140046b8c) | 4ad3ad5eb | citizen_lifeevent_handler IDENTICAL 14 SAME 1 (`test %rbx,%rbx; je` gone) | none |
| 1207 | NOT A DIFFERENCE: a member with 0x400 already went through MarkForRemoval's slot-7 broadcast (citizen flags 0xcb have bit 1), whose OnRemove unlinked it; the walk progresses | d60756eff | citizen_onremove IDENTICAL 8 (comment only) | HandleLifeEvent's head return on [gw+0xd0] bit 8 would not unlink an under-age child; retail and tree identical; whether OnRemove runs in that state was not read |
| 1208 | FIXED: SerializeAll's two `if (m_citizenList)` (Serialize and PostLoad) dropped | e4ce75e36 | gameupdateloop_serialize IDENTICAL 21 SAME 1 (two `test %rcx,%rcx; je` gone) | the body's AchievementManager / intFactory / achTid tests and CitizenBehavior::Serialize's phantom third parameter filed as row 1247 |
| 1209 | FIXED: `settings->Save()` before the test, as retail; the test now guards only the free | 54cf46522 | game_main DIFF 1 IDENTICAL 8 (the pre-Save test gone, the free's test folded per B8 C5; the store DIFF is a base-register rename) | the port-only `g_gameSettings = nullptr` store kept (a store, not a test) |

Also: c72a60036 (status-cell hashes, rows 1240-1247 sorted). New rows filed (SUSPECTED, bodies untouched): 1240-1247.

Whole-tree B11: only the TUs above differ, plus four objects with no function verdict
(`tool_base_edit2`, `factory_file_extension`, `system/debug`, `ui_operator_delete`) whose bytes differ
with no function change -- not this block's files and not touched.

Routes for the coordinator: new game and -O2 (1120, 1121, 1128, 993), tools (1122 roads), ui (1123
menu particles / music, 1126 options OK / Cancel, 1200, 1201), the profession-UI utility harness route
(1200), `harness_route.sh panelclose` (1201) and `pick`, the fire / disaster route (1080: no change
expected), loadgame and the resave gate (1208), any clean quit (1209).
