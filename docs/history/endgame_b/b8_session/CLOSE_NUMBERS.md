# B8 (the quality pass) — close numbers (2026-10-10)

Start `e55b79f12` (B7 CLOSED) -> the two user decisions `c60271c1b` / `ed149bf42` (gate row DEC) -> kit `33325fd7b`
-> nine gated merges (GATE_TABLE.md rows 1-9: COMPA, RT, COMPB, ENT, MISC, RES (+ addendum), GAME, RT2, HDR), the
last `1afc1589a` -> LEFT_FREE regenerated `5ebacdf7e` -> the close documents. 237 block commits (108 `behaviour:`,
129 `B8:`), 9 merges, 0 problems left standing; 464 files, +4,149 / -6,376 lines before HDR (HDR: -723 more).

## 1. What the step did

- **The two user decisions of 2026-10-09, first and gated (DEC):** `NaturalResourceComponent` slot 8 is the member
  `Tick` (vtable 0x140163e10 index 8 = 0x140125c40; every cut tree / quarried rock runs the removal-animation tick);
  `GrowthComponent::SpawnSubResource` calls the placement test 0x14009f5d0 (sub-resources spread as retail) — and
  reading that body found the spawn-chance test dividing the 0.98f UV scale instead of retail's 1.0f (0x14017214c):
  corrected in the same commit. Routes, resave (unchanged: a loaded save), sa-stress 139 fps, worldcmp unchanged.
- **The quality pass:** of the 412 free declarations in `exe_bodies.h` / `*_funcs.h` (411 names), **290 are gone**:
  the members of a class in retail are that class's members (declared in its header with the retail address, defined
  where they were, every caller by name), the forwarders deleted with their callers on the member, the erased cores the
  class template's member (`Collection::Array<T>::SetSize` specialisations and instantiations, `Set<T>` /
  `Work/KeyBst.h` over the node type, `ComponentDescription::FindDescription<T>`, the `fn_FindComponent_*` family on
  `FindComponent<T>`), the ctor doors real ctors (BuildRoad, BuildStructure, GatherRawMaterials, TendArea, ClearArea,
  the four Dialog ctors, PickupRawMaterial's (citizen, pile) ctor, the ComponentEntity ctor door a placement-new).
  **121 declarations survive**, each with its ruling on its line: 54 `free in retail` (RCX not an object of a class the
  body reads — a return slot, a dead RCX, a static helper), 49 `stays free` (a function-pointer site, a B11 DIFF
  beyond the move, already typed on its one instantiation, a port-only body / harness route, the three uninstalled
  slot bodies), 18 harness / port-only with their own comments. 57 emptied headers deleted (HDR).
- **B11 as the gate (C1):** b11cmp does not pair a renamed body; COMP-A's `b11pair.py` (a rename map) is the kit's
  tool. Every moved body RENAMED-IDENTICAL / RENAMED-SAME, callers IDENTICAL / SAME; the accepted DIFF shapes are
  named (C2 a devirtualised call to the slot's own target; C5 a later null test folded after a member call; C6 a `%p`
  diagnostic field dropped; the inlined finder walk for the FindComponent family).
- **108 `behaviour:` commits**, every one a guard, test or store retail lacks dropped with the retail call sites quoted
  (C3: a forwarder's null guard and a `this`-null test at the top of a body; C4 / RT2: the 36 ExternalPtr serializer
  rows on `Object::SerializeExternal<T>` / `SerializeExternalArray<T>` by their retail cell — 53 commits, resave
  unchanged — and the 10 erased resize cores on `Array<T>::SetSize` with the port-only tests, the ALLOCSTATS tag and the
  grown-tail ZEROING dropped as retail; RES: rows 1000, 1020, 1040, 1041, 1060-1065, 1104, 1205, row 310's four files,
  ProductionUI's guard root-caused to no route; the `BANISHED_LIVE_SEAT_SHUFFLE` flag deleted: the shuffle unconditional).
- **Routes added:** `harness_route.sh pick` (citizen click-picking through the real picking path: PICKED, panel OPENED —
  row 252's finding did not fire) and `panelclose` (an entity panel closed through ToolUI: CLOSED), `disaster_route.sh`
  (two houses ignited at frame 300: the FireComponent -> DestroyComponent path unguarded as retail holds).

## 2. The metric

| | start `e55b79f12` | close |
|---|---|---|
| castmetric PROD total | 3 (1 named prerequisite + 2 ruled) | **3** (unchanged: the introsort escape awaits `Array16Assign` -> `Array<T>::operator=`, which RT2's SetSize pass did not reach) |
| cast_ok / tier_i | 712 / 447 | 702 / 447 (the `elem_stride` wrappers gone) |
| `exe_bodies.h` / `*_funcs.h` declarations (acceptance line 08) | 411 names / 412 | **121 names / 122** (19 headers; 57 headers deleted) |
| `void*`-first functions in code/Game (line 09) | 106 | **39** |
| acceptance (`b4_session/acceptance.sh`) | 1 line off (10 = 19) | **1 line off: 10 = 19** (accepted); 01 = 3 of 3; 08 / 09 = 0 against the REGENERATED LEFT_FREE.md (`b8_session/leftfree/regen.py`) |
| unit tests | 920 / 0 | **920 / 0** |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave == `994eb184…` | **unchanged on all nine merges** |
| known_port_issues | — | entry 3 and rows 5 / 552 closed; rows 1000, 1020, 1040, 1041, 1060-1065 FIXED; 37 new rows 1080, 1100-1108, 1120-1129, 1140-1143, 1160-1164, 1180-1181, 1200-1205 (10 LEFT) |
| sa-stress | 139 fps (B7 close) | **139 fps** (update p95 2.40 -> 2.72 ms: recorded, not chased) |
| worldcmp (seed 12345) | identical to B7 but for the `[PfSeed]` wording | **identical to the DEC run** (md5 97fcc629) |

## 3. Blocks and merges

| block | rows | moved / deleted | stays free | behaviour: | merge |
|---|---:|---|---|---:|---|
| COMP-A | 35 | 23 members + 6 forwarders | 6 | 0 | row 1 `be83f3c5e` |
| RT | 58 | 4 SetSize specialisations + ActionStack::Clear | 53 (-> RT2) | 0 | row 2 `d852e1673` |
| COMP-B | 52 | 49 | 3 (a root-slot core, the uninstalled slots [4] [6] [9]) | 9 | row 3 `7bb1ede51` |
| ENT | 37 (+ the FindComponent family) | 32 + 4 forwarders + 24 finders | 5 | 6 | row 4 `8f41b8ad9` |
| MISC | 152 | 25 members, 13 ctor doors / forwarders, 2 templates, 7 finders, 2 signatures trimmed | 69 free in retail + harness + 6 | 3 | row 5 `ed9350ac3` |
| RES | the B7 residue | 13 rows FIXED, 310 PART, ProductionUI, routes pick / panelclose | — | 20 | row 6 + 6b `e95059677` |
| GAME | 78 | 75 | 2 | 25 | row 7 `c2472d4fe` |
| RT2 | rows 1160 / 1161 | 34 serializer families + 10 resize cores + CropSlotArray | 3 loop serializers | 53 | row 8 `b4d49379b` |
| HDR | — | 57 headers | 4 kept with content | 0 | row 9 `1afc1589a` |

## 4. Close checks

- `worldcmp.sh` (`runs/CLOSE_worldcmp`): old = `oldbase` `b2a5434ca`, new = main `5ebacdf7e`: 118 lines, DIFFERS only on the
  `[PfSeed]` wording line (row 962, as at the B7 close); vs the DEC run: IDENTICAL.
- `sa-stress` (`runs/CLOSE_stress.log`): exit 0, 6000 frames, 139 fps.
- `stepclose.sh` (`runs/stepclose/summary.txt`): see §6.
- Independent audit `b8_session/AUDIT.md`: see §6.

## 5. Found, left (the next step's residue)

- **User decisions:** row 1162 (retail's `wg_f390_smooth` 0x1400cc350 copies UNINITIALISED floats into the heightmap's
  last column / row; the port keeps 0.0f so world builds stay deterministic); rows 1202 / 1203 / 1204 (the uninstalled
  slot bodies `DecalComponent_Serialize` [4] — keep until the material-load race is fixed upstream, `MapComponent_Draw`
  [6] — install together with removing starttown's 0x100000 pre-set, `RawMaterialComponent_Update` [9] — install, the
  slot-8 shape).
- **LEFT rows:** 1120 (ValidateCell's range guard), 1121 / 1122 (the null cell-array returns), 1123 (`!time`), 1126
  (`!userInterface`), 1128 (`!tmpl || !centre`) — port-only ARGUMENT tests GAME kept beside the dropped `this` tests;
  1163 (`RangeResize`, a second port of 0x14001efd0); 1164 (BucketTree's `s_tidCompEnt &&` test, a one-token drop);
  1200 (ProfessionUI's set-up tests: OnOpened's serves the harness slot-5 re-drive, D4(a)); 1201 (ProductionUI's
  remaining tracker / Ready / empty-list gates); row 310's 0x140046a90 NullSafe narrowing (owing row 993); 1080
  (`FireComp_StartAudioEmitter_42c60`: a second port of `AmbientEmitterComponent::PlaySlot` with tests retail lacks).
- **The introsort escape** (`introsort.cpp:355`): `rtsort::Array16Assign` 0x180050d30 is still the out-of-line
  `EmitterArray` assign (audio.cpp:940, audio_scene_update.cpp:277); once it is `Collection::Array<T>::operator=` the
  typed local can be re-measured.
- **Survivors with a measurement** (LEFT_FREE §A `B8: stays free`): the two function-pointer serializer keys
  (citizenai_gather.cpp), the WorkBST Insert / cell-set Insert cores (out-slot callers), `TreeNodeFree` (GCC unrolls the
  templated recursion), the three element-loop serializers (no `Array<T>::Serialize` by design), `TradeResourceTree_Retrace`
  (a root-slot core), `ToolbarDescription_StringArrayResize`'s siblings, HarvestOrchard / PlantOrchard ctor doors (the
  orchard diagnostic counter), DebugUI_Slot9 (the harness hands a null DebugUI).

## 6. Audit and stepclose

- **Audit** (`b8_session/AUDIT.md`, da76b3969): 231 OK / 5 FINDING / 2 QUESTION on 238 commits (62 `behaviour:` commits and
  28 RT2 serializer cells re-read in Ghidra: every cell matches its T; HDR not audited). Applied in CLOSE-A `a1f7731e0`:
  F1 (BindUISounds was a partial second port of `GameContainer::ApplyKeyMapToUI` 0x140099260 behind a g_gameContainer
  test -> the member call at retail's position 0x14009b96b, BindUISounds deleted, row 1206), Q1 (SpawnSubResource's
  `!world` and zero-denominator returns were tests retail lacks -> dropped), F6 (row 1143's cell), F2 (681dd0490 dropped
  port-only tests under a `B8:` title: recorded here as a `behaviour:` commit -- the content is right), F3 / F4 / F5
  (documentary: commit titles with the wrong counts / row number; LEFT_FREE was regenerated from the lines). Q2 filed as
  row 1207 (HandleLifeEvent's 0x400 ring case); the audit's two pre-existing unrowed guards filed as rows 1208 / 1209.
- **stepclose** (`runs/stepclose/summary.txt`, run on the CLOSE-A tree): mods / menuquit / options / achievements clean, goaldialog 1 OpenGoal (killed=YES expected), tool placement / road / removal exit 0 crash 0, in-game save COMPLETE crash 0 (file mode, the expected exit 1 as at B6 / B7), sa-stress 139 fps, true release compile exit 0 (934 files, 6.5 MB exe), clean rebuild debug / perf / unit exit 0, gate closeclean 0 problems (0 new warnings, unit 920 / 0, routes clean, typetable == baseline minus the 10 lazy by-name lookup lines RT2 deleted with ResolveTypeIdByNameLazy -- now X::TypeId(), resave 3/3), NOT IMPLEMENTED 0, stub set 0 new / 0 gone, tutorial census citizen=16 herd=16 unchanged.
