# T0 — what the tests still need at the flip (W1 worklist)

Block T0 (branch `b3x-T0`) made `tests/unit` shift-proof ahead of the flip, as a no-op refactor
(`make unit` 914 → 916 tests, 0 failures; the two new tests are `slot_check_test.cpp`). This file is
the exact list of what T0 left on purpose because it changes meaning at the flip (B3X_RULES X5).
Line numbers are those of the T0 commit. Rebuild the list with a grep before editing, not from memory.

## What needs NO edit at the flip
- **Every PMF slot check** (1,197 lines, 33 files): `CHECK_SLOT(T, M, retail n)`,
  `CHECK_PMF_SLOT(pmf, n)`, `CHECK_DTOR_SLOT(T)`, and k1's `slot_check::SlotOk` /
  `slot_check::DtorSlotOk<D>()`. `RetailSlot` keys on the member's declaring class
  (`std::has_virtual_destructor`), and `CHECK_DTOR_SLOT` detects `T::DeletingDtor` (compiler slot 0,
  no virtual dtor) or, once it is gone, requires `has_virtual_destructor<T>`.
- **Raw reads at retail slot n >= 1** through a helper (`SlotAt`, `PkgSlot`) or through
  `RetailEntry<T>` / `RetailSlotFn<Fn, T>` / `RetailSlot<T>(n)` / the UI files' `Retail(n)`: the
  helper translates. The UI files (`ui_dialog`, `ui_element`, `ui_leaf_c1..c5`, `ui_resources`) key
  their `SlotAt` on `Object::Base` by default (every object they read raw is Object::Base-rooted);
  the other files name the object's class (or its test mirror) at each call.
- Markers stay in retail numbers (`CHECK_EQ(g_lastMark, kX / 8)` is still right).

## Whole files
- `object_vtable_abi_test.cpp`: NOT touched by T0 — it is rewritten as the test of the new rule
  (INVENTORY_E §3.2). Its `SlotAs` takes COMPILER indices on purpose (the `FakeWithVirtualDtor`
  control reads 2/3). Sites: slot-0 172,216,399,456; `DeletingDtor(0)` 271; decl 77,100,335; trait 93.
- `system_abi_test.cpp`, `b39_sys_thread_test.cpp`: Thread family (exempt) — no action but the
  traits if the Thread rule changes.
- `b39_vid_data_test.cpp` and the `Mirror*Data` classes in `video_data_abi_test.cpp`: `Video::*Data`
  is exempt BY DECISION (X1) — keep `DeletingDtor`, keep the traits.

## Per-file counts (non-comment lines)

Columns: `slot0` a raw read/call of slot 0 (becomes D1; D0 is slot 1 — the six stack-probe calls are
the double-destruction trap, INVENTORY_E §3.1); `zt` a `_ZTV…[2]` slot-0 call (→ `[3]` D0 for flags 1,
`[2]` D1 for flags 0); `arm` the vector arm `DeletingDtor(2/3/9u)`; `dd01` `DeletingDtor(0)`/`(1)`;
`decl` a `DeletingDtor(unsigned)` declaration or stand-in override; `hvd`/`isd` trait asserts
(invert on flipping classes only); `loop` a loop over a whole table's LENGTH or a minted copy's length
(bound +1; the patched index is already translated); `destroy` `Object::Destroy(`.
Exempt tables are excluded (list at the end).

| file | slot0 | zt | arm | dd01 | decl | hvd | isd | loop | destroy |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| `achievement_goal_abi_test.cpp` |  |  |  |  |  | 4 |  |  |  |
| `act_abi_test.cpp` |  |  |  |  |  | 2 |  |  |  |
| `audio_abi_test.cpp` | 2 |  | 1 | 4 | 3 | 4 | 3 | 3 |  |
| `b39_eng_collision2d_test.cpp` |  |  | 1 | 4 |  | 2 | 2 |  |  |
| `b39_io_classes_test.cpp` |  |  | 9 | 16 |  | 12 | 12 |  |  |
| `b39_obj_compiledexternal_test.cpp` |  |  | 3 | 4 |  | 3 | 3 |  |  |
| `b39_obj_externallist_test.cpp` |  |  | 1 | 3 |  | 1 | 1 |  |  |
| `b39_sys_thread_test.cpp` |  |  |  |  |  | 1 | 1 |  |  |
| `b39_uib_test_util.h` |  |  | 1 | 2 |  | 1 | 1 |  |  |
| `b39_vid_data_test.cpp` (exempt, X1) |  |  | 1 | 2 |  | 1 | 1 |  |  |
| `b39_vid_program_test.cpp` |  |  | 1 | 5 |  | 6 | 6 |  |  |
| `cit_abi_test.cpp` |  |  |  |  |  | 1 |  |  |  |
| `dlg_abi_test.cpp` |  |  |  |  |  | 1 |  |  |  |
| `engine_loop_abi_test.cpp` |  |  |  |  |  | 3 |  |  |  |
| `exe_class_abi_test.cpp` |  |  |  |  |  | 4 |  |  |  |
| `goal_abi_test.cpp` |  |  |  |  |  | 1 |  |  |  |
| `graphics_animation_abi_test.cpp` | 1 |  | 2 | 4 | 2 | 2 | 2 | 2 |  |
| `graphics_geometry_abi_test.cpp` | 1 |  |  |  | 1 | 11 |  | 2 |  |
| `graphics_material_abi_test.cpp` | 1 |  | 1 | 3 | 5 | 2 | 2 | 5 |  |
| `graphics_particle_abi_test.cpp` | 2 |  |  |  | 3 | 4 |  | 2 |  |
| `io_streams_abi_test.cpp` | 1 |  |  |  | 1 | 5 |  | 1 |  |
| `io_vtable_abi_test.cpp` | 5 |  |  | 1 | 9 | 14 | 3 | 4 |  |
| `k1_abi_test.cpp` |  |  |  |  |  | 3 |  |  |  |
| `loop_abi_test.cpp` |  |  |  |  |  | 10 |  |  |  |
| `mia_abi_test.cpp` |  |  |  |  |  | 2 |  |  |  |
| `mib_abi_test.cpp` |  |  |  |  |  | 4 |  |  |  |
| `object_external_test.cpp` | 1 |  |  |  |  | 4 |  |  | 2 |
| `object_internal_test.cpp` |  |  |  |  | 1 | 3 |  |  | 13 |
| `object_typeinfo_test.cpp` | 1 |  |  |  | 3 | 1 |  |  | 2 |
| `pathfinding_picking_abi_test.cpp` | 1 |  | 2 | 6 | 3 | 3 | 3 | 3 |  |
| `pnl_abi_test.cpp` |  |  |  |  |  | 3 |  |  |  |
| `rb_achievement_abi_test.cpp` |  | 1 | 1 | 6 | 3 | 4 | 4 |  |  |
| `rb_engine_abi_test.cpp` |  |  | 1 | 5 | 4 | 4 | 1 |  |  |
| `rb_graphics_abi_test.cpp` |  | 4 | 9 | 20 | 4 | 14 | 12 |  |  |
| `rb_ui_abi_test.cpp` | 1 | 2 | 2 | 4 | 5 | 5 |  |  |  |
| `rb_video_abi_test.cpp` |  | 1 |  | 1 |  | 2 | 1 |  |  |
| `system_abi_test.cpp` (Thread) |  |  |  |  |  | 1 |  |  |  |
| `tool_abi_test.cpp` |  |  |  |  |  | 8 |  |  |  |
| `ui_dialog_abi_test.cpp` | 3 |  | 1 | 4 | 8 | 1 | 4 | 3 |  |
| `ui_element_abi_test.cpp` | 2 |  |  |  | 3 | 2 |  | 4 |  |
| `ui_leaf_c1_abi_test.cpp` |  |  | 1 | 10 | 12 | 8 | 10 | 5 |  |
| `ui_leaf_c2_abi_test.cpp` |  |  | 1 | 11 | 12 | 5 | 10 | 2 |  |
| `ui_leaf_c3_abi_test.cpp` |  |  | 1 | 10 | 2 | 2 | 10 |  |  |
| `ui_leaf_c4_abi_test.cpp` | 2 |  |  | 10 | 3 | 1 |  | 3 |  |
| `ui_leaf_c5_abi_test.cpp` |  |  |  | 14 | 16 | 1 |  |  |  |
| `ui_resources_abi_test.cpp` | 1 |  | 1 | 6 | 7 | 5 | 5 | 4 |  |
| `video_data_abi_test.cpp` | 2 |  |  |  | 4 |  |  |  |  |
| `video_imagebuffer_abi_test.cpp` | 1 |  | 1 |  | 1 | 6 |  |  |  |
| `video_program_abi_test.cpp` | 1 |  |  |  | 1 | 5 |  |  |  |
| `vis_abi_test.cpp` |  |  |  |  |  | 2 |  |  |  |
| `wrk_abi_test.cpp` |  |  |  |  |  | 3 |  |  |  |
| **total** | **29** | **8** | **42** | **155** | **116** | **197** | **97** | **43** | **17** |

Trait rows include the lines on exempt classes (Thread, Controller, Encoding, PlatformChannel,
IPathInfo, `*Data`); those stay as they are. `video_data_abi_test.cpp:256` is a `MirrorDeviceData`
(`*Data`, exempt) slot-0 call — keep; `:437` (`MirrorVertexLayout`) follows the mirror.

Callers of the distinctness helpers pass the table LENGTH and need it +1 with the loop:
`CheckDistinct(raw, n)` / `CheckDistinctSlots(raw, n)` / `CheckNineDistinctSlots` in `io_streams`,
`io_vtable` (streamorder), `ui_dialog`, `graphics_material`.

## Lines
- `achievement_goal_abi_test.cpp`: hvd 110,111,112,113
- `act_abi_test.cpp`: hvd 63,91
- `audio_abi_test.cpp`: slot0 150,319; arm 443; dd01 434,494,495,517; decl 89,129,132; hvd 197,217,233,547; isd 196,216,267; loop 310,339,341
- `b39_eng_collision2d_test.cpp`: arm 74; dd01 64,141,142,159; hvd 90,91; isd 88,89
- `b39_io_classes_test.cpp`: arm 267,297,331,405,413,455,503,558,607; dd01 234,255,287,323,379,388,396,398,445,449,494,497,549,552,598,601; hvd 67,194,195,196,306,347,348,422,466,514,569,620; isd 191,192,193,305,344,345,346,421,465,513,568,619
- `b39_obj_compiledexternal_test.cpp`: arm 96,132,168; dd01 78,88,123,157; hvd 41,117,148; isd 40,116,147
- `b39_obj_externallist_test.cpp`: arm 132; dd01 107,108,122; hvd 62; isd 61
- `b39_sys_thread_test.cpp`: hvd 64; isd 63
- `b39_uib_test_util.h`: arm 106; dd01 92,96; hvd 32; isd 31
- `b39_vid_data_test.cpp`: arm 72; dd01 53,61; hvd 41; isd 42
- `b39_vid_program_test.cpp`: arm 64; dd01 53,155,201,262,341; hvd 122,170,216,279,288,289; isd 121,169,215,278,286,287
- `cit_abi_test.cpp`: hvd 116
- `dlg_abi_test.cpp`: hvd 57
- `engine_loop_abi_test.cpp`: hvd 77,78,79
- `exe_class_abi_test.cpp`: hvd 64,65,66,67
- `goal_abi_test.cpp`: hvd 44
- `graphics_animation_abi_test.cpp`: slot0 139; arm 295,353; dd01 280,286,338,344; decl 97,118; hvd 210,312; isd 211,311; loop 184,228
- `graphics_geometry_abi_test.cpp`: slot0 420; decl 300; hvd 725,726,727,728,729,730,731,732,733,737,738; loop 680,682
- `graphics_material_abi_test.cpp`: slot0 187; arm 708; dd01 699,740,776; decl 141,149,155,161,167; hvd 718,749; isd 717,748; loop 222,278,307,309,686
- `graphics_particle_abi_test.cpp`: slot0 329,401; decl 244,279,292; hvd 487,488,489,490; loop 441,443
- `io_streams_abi_test.cpp`: slot0 100; decl 156; hvd 201,203,205,372,401; loop 146
- `io_vtable_abi_test.cpp`: slot0 273,450,1187,1358,1676; dd01 426; decl 148,723,932,1140,1328,1339,1487,1502,1648; hvd 562,563,564,565,1247,1248,1249,1250,1287,1413,1415,1543,1545,1719; isd 570,571,572; loop 290,1241,1391,1716
- `k1_abi_test.cpp`: hvd 118,251,280
- `loop_abi_test.cpp`: hvd 111,112,116,120,124,276,311,336,351,352
- `mia_abi_test.cpp`: hvd 126,130
- `mib_abi_test.cpp`: hvd 87,107,158,162
- `object_external_test.cpp`: slot0 191; hvd 321,322,323,324; destroy 196,200
- `object_internal_test.cpp`: decl 99; hvd 156,157,158; destroy 204,215,250,266,275,320,321,322,337,346,351,396,433
- `object_typeinfo_test.cpp`: slot0 465; decl 383,396,409; hvd 422; destroy 446,457
- `pathfinding_picking_abi_test.cpp`: slot0 100; arm 302,486; dd01 279,280,292,462,463,476; decl 78,156,374; hvd 192,323,414; isd 191,413,533; loop 116,221,223
- `pnl_abi_test.cpp`: hvd 87,131,142
- `rb_achievement_abi_test.cpp`: zt 225; arm 177; dd01 168,204,205,216,259,260; decl 97,100,106; hvd 233,238,241,242; isd 232,236,237,240
- `rb_engine_abi_test.cpp`: arm 256; dd01 235,241,270,279,287; decl 120,130,134,141; hvd 298,299,300,301; isd 297
- `rb_graphics_abi_test.cpp`: zt 269,281,302,333; arm 416,487,531,606,726,805,831,904,957; dd01 328,397,404,449,457,477,516,523,595,686,692,715,719,787,796,821,887,895,939,947; decl 141,148,152,158; hvd 327,341,342,349,350,363,364,495,576,661,702,762,869,931; isd 326,361,362,470,494,575,660,701,761,814,868,930
- `rb_ui_abi_test.cpp`: slot0 355; zt 422,438; arm 291,310; dd01 280,299,417,433; decl 146,149,152,159,163; hvd 393,394,395,396,397
- `rb_video_abi_test.cpp`: zt 96; dd01 129; hvd 105,136; isd 104
- `system_abi_test.cpp`: hvd 199
- `tool_abi_test.cpp`: hvd 67,68,117,118,181,182,183,184
- `ui_dialog_abi_test.cpp`: slot0 161,210,272; arm 430; dd01 421,473,497,507; decl 115,119,128,131,134,137,140,143; hvd 365; isd 363,364,368,369; loop 183,317,319
- `ui_element_abi_test.cpp`: slot0 300,336; decl 132,139,156; hvd 226,227; loop 322,357,438,440
- `ui_leaf_c1_abi_test.cpp`: arm 918; dd01 908,967,968,983,1030,1041,1082,1100,1114,1123; decl 140,144,147,150,153,156,159,162,165,168,171,176; hvd 490,491,492,493,689,690,783,784; isd 483,485,487,489,686,688,776,778,780,782; loop 580,653,655,874,876
- `ui_leaf_c2_abi_test.cpp`: arm 738; dd01 728,774,787,805,819,858,869,879,890,901,914; decl 159,163,166,169,172,175,178,182,185,188,191,195; hvd 468,469,470,640,641; isd 457,459,461,463,465,467,633,635,637,639; loop 583,585
- `ui_leaf_c3_abi_test.cpp`: arm 588; dd01 578,627,640,651,673,689,733,750,775,799; decl 128,146; hvd 340,341; isd 322,323,324,325,328,329,330,331,332,333
- `ui_leaf_c4_abi_test.cpp`: slot0 503,538; dd01 321,328,340,355,387,402,703,719,745,757; decl 121,139,143; hvd 277; loop 521,648,650
- `ui_leaf_c5_abi_test.cpp`: dd01 673,692,712,737,770,783,796,807,840,853,878,885,898,911; decl 131,135,298,301,367,370,407,410,443,446,482,485,517,520,612,615; hvd 316
- `ui_resources_abi_test.cpp`: slot0 113; arm 442; dd01 432,481,482,509,538,567; decl 92,100,194,231,278,313,347; hvd 148,207,246,368,520; isd 147,206,245,367,519; loop 126,173,175,414
- `video_data_abi_test.cpp`: slot0 256 (keep, `*Data`),437; decl 139,194,224,406
- `video_imagebuffer_abi_test.cpp`: slot0 229; arm 298; decl 155; hvd 270,414,415,416,417,418
- `video_program_abi_test.cpp`: slot0 246; decl 214; hvd 351,352,353,354,355
- `vis_abi_test.cpp`: hvd 152,199
- `wrk_abi_test.cpp`: hvd 108,109,122

## Raw reads on exempt tables (no action; listed so W1 does not "fix" them)
`achievement_goal:222`, `engine_loop:162`, `tool:213`, `graphics_geometry:561,571` (retslot probes
with no dtor slot); `audio:330` (Audio::Stream `Run`); `b39_sys_thread:101,105`,
`system_abi:235,237,241` (Thread); `rb_engine:322` (Engine::Loader `Run`); `exe_class:212,213`,
`mia:200`, `mib:251` (Controller `FindState`); `pathfinding_picking:327` (IPathInfo);
`io_vtable:523,1263,1283` (IO::Encoding); `io_vtable:602` (TestSerializer on a hand-minted
ReaderWriter table in retail order — keyed on `MintedRwStorage`, never shifts).
