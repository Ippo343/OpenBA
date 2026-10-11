# B7 (the named-prerequisite pass) — close numbers (2026-10-09)

Start `a4944195c` (B6 CLOSED; kit `b4aeac7c0`) -> close `455db4163` (Merge b7-RT, the last gated code merge), then
CLOSE-A `40773f626` / `47cc3b7dd` (audit F2 / F3, `behaviour:`) / CLOSE-B `048e82e29` and the close documents. 77 commits (42 `behaviour:`, 25 `B7:`, 9 merges, the kit / gate-table / rule commits),
5 gated merges (GATE_TABLE.md rows 1-5), 0 problems left standing; 197 files, +1,906 / -2,662 lines.

## 1. What the step did

- **Every "named prerequisite" cast but one is gone (23 -> 1)** because the prerequisite was supplied:
  - **the inline release** (RT): `System::String`'s ctor / copy / dtor / `Release` / `Adopt` / `operator=`,
    `Filename`'s ctors, `rtcoll::ArrayDtorImpl` and `MemFree` are header-inline (`[[gnu::always_inline]]`,
    the same reads and stores; GCC otherwise kept an out-of-line `Release` on GetNextItem's normal path).
    Typed: GetNextItem's String / Filename locals (0x1800cdf40: MemFree the only call per release, as
    retail), the by-value `Collection::Array<uint32_t> / <uint8_t>` getters (0x180025530 / 0x180025580),
    `TypeRecord::children` as a real `Collection::Array<TypeRecord*>` (`behaviour:` -- MemFree is no longer
    called with a null buffer at Factory teardown, retail's null test 0x1800093ff), `MaterialTexture::name`
    as a `System::String` (AddTexture 0x180103350 / Draw 0x1800c2eb0 build the pair as a typed local), a
    typed `EventLogEntry` in a block ending after the grow; the ruled `widestring.cpp:196` cast falls too.
  - **the ctor doors are real ctors** (CTOR): `System::Random()` 0x1800c7f20 / `Random(uint32_t)`
    0x1800c7ed0, `Engine::Engine(void*)` 0x1800dbf00 / `~Engine` 0x1800dbec0, `Graphics::Renderer()` /
    `Scene()` and their dtors, `Volume::Frustum`'s two ctors 0x1800fa1e0 / 0x1800fa3d0 (which ZERO FIRST,
    C1); every `RtSystem_Random_Ctor` / `RtEngine_Engine_*` / `RtGraphics_Renderer_*` / `RtGraphics_Scene_*`
    / `RtVolume_Frustum_Ctor_*` door deleted with its last caller. `behaviour:` where a store moved: the
    0x700 Engine memset and the 0xC0 Frustum byte-zeroing retail lacks are gone; MenuDialogLoopT seeds its
    Random before the Audio::Scene (0x14009bb2e); `Particle::_rng` is seeded by its static ctor as retail's
    CRT init 0x180149650. `Context::PixelConstantStaging::pixel` is `Math::Vector4[5]` over the ctor's
    memset (C2; the row-4 store stays one MOVUPS).
  - **the two leftovers** (LX): `IO::Persistence` +0x00 and `System::Configuration` +0x10 hold a real
    `Collection::Set<Pair<String,String>>` (Map_Find 0x18002ace0 IS the template's `Find`; Configuration
    gets its real ctor 0x1800c51b0 and `Array<String>` at +0x00, so its dtor runs Set then Array as
    0x1800c5170); `WorkCellBST_Remove_ca60` 0x14001ca60 is a template over the node type in `Work/CellBst.h`
    (the five `CAST-OK: elem_stride` wrappers and the erased core gone).
  - **the two harness puns** (LX, ruled D8): the force-work store into `CitizenComponent::disease` (+0x68)
    is gone (no harness reads it); `ClearAreaComponent_HarnessSweep` builds a real `ClearAreaComponent`
    local (its dtor unregisters from the InternalFactory).
- **The B6 residue rows (no cast)**, all read at their retail addresses:
  - RES: 24 items -- rows 82, 83, 84, 89, 98, 99, 250, 251, 252, 291, 330, 310 G1 + 994's 0x140057b20,
    350, 352, 451, 692, 960, 920 / 921 / 922 (ruled D10 and dropped), 443 (one port of 0x140119470), 962
    (the priming queues through SetSearchFlags / SetStepCost with `runInline = 0`, one command 4, as
    0x14009e8c0), the pair-cache tick; 78 and 84's generation re-read NOT A DIFFERENCE; the
    GatherRawMaterials site ctor already retail's.
  - LX: 992 (ProfessionList on the Profession instantiation 0x140018020; every ExternalPtr caller on its
    retail instantiation, eleven cores added), 942 (a loaded reference is kept only `if IsDerivedFrom(cell)`,
    else null, as 0x140002340 at 0x1400024d8), 993 (`IsDerivedFromNullSafe` -> the member, row 7 first;
    0x14001fd10 keeps retail's own null test), 994 G33 (ToolUI's deselect block dropped, EntityUI slot 9's
    `SelectTool(0)` restored) and G30 (textSeason guard dropped).
- **Rulings**: 920 / 921 / 922 dropped (no route builds the missing piece; D10); the two harness puns (D8).

## 2. The metric

| | start `a4944195c` | close |
|---|---|---|
| castmetric PROD total | 26 (23 named prerequisite + 3 ruled) | **3** (= 1 named prerequisite + 2 ruled + 0 work) |
| code / runtime / standalone / engine / dx11 | 13 / 13 / 0 / 0 / 0 | 1 / 2 / 0 / 0 / 0 |
| cast_ok / tier_i | 720 / 448 | 712 / 447 |
| acceptance (`b4_session/acceptance.sh`) | 1 line off (10 = 19) | **1 line off: 10 = 19** (the accepted template-field miscount); 01 = 3 of target 3; 08 back to 0 after the 13 serializer cores were listed in LEFT_FREE §A |
| unit tests | 920 / 0 | **920 / 0** |
| `exe_bodies.h` / `*_funcs.h` declarations | 401 | 412 (+13 ExternalPtr serializer cores, -2 doors) |
| known_port_issues cells touched | — | 35; new rows 1000, 1020, 1040, 1041, 1060-1065 |
| resave golden | `3efc80a84907fbb8e25d64b4f2c99cff` | **`994eb184a8e88db9372b5b9cd13a9ed9`** (RE-CUT once, row 84: a resource-load-order change; C3) |

The one cast left: `src/engine/collection/introsort.cpp:355` -- re-measured with the inline dtor: a typed
empty `Array<EmitterInstance>` at the 0x1800dfd10 caller keeps its free because its address escapes into the
out-of-line `Array16Assign`, so `Scene::Update` gains a MemFree call retail lacks; the prerequisite is now an
escape GCC can see through, not the inline dtor (the tag says so).

## 3. Gate rows

Five merges (CTOR, RES, LX, LX2, RT): every one builds debug / release / perf / unit exit 0, 0 new warnings,
new game `5683/0`, tutorial `5773/12`, typetable `b983f144`, unit 920 / 0. Two merges landed with a resave
problem, each root-caused and closed by the next commit:
- **RES (row 2)**: resave 0/5, 105 bytes at identical size. Bisected commit by commit in a scratch worktree:
  row 84 (`184a69e9f`, `NamedExternalRef::Resolve` find-only as retail 0x1400265b0). A probe showed 169
  Resolve calls at factory generation 0 missing `Profession.rsc:builder / :laborer / :trader` and others,
  which the old body FindOrLoad-ed early; the three Profession objects now get other heap addresses and
  `CitizenBehavior`'s profession bucket tree (+0x100, keyed on the raw `Profession*`, 0x140018020) is saved
  in another order. Golden RE-CUT (`tools/resave_gate/README.md`), 5/5 after; C3.
- **LX (row 3)**: resave 0/5, a REAL data change (payload -107: a saved id -> -1, the profession tree 3 -> 1
  buckets). Probe: callers on the WRONG ExternalPtr instantiation nulled by row 942's validation -- the two
  bucket-tree keys on the StringTable core (`citizenlist_serialize.cpp:492 / 569`), 163 `:edge` meshes
  (HighlightDescription) and TiledDecals materials (DecalDescription) on the GraphicsMesh core, the two
  nomad materials on ComponentDescription. Fixed on LX2 (row 4: the Profession / ComponentDescription cores
  for the keys, two new array cores EdgeMesh 0x140007580 / MaterialInstance 0x140020150, NomadDescription's
  materials on MaterialInstance); all 73 ComponentDescription sites re-verified against their retail bodies'
  call order; resave back to 5/5 with NO re-cut. C4.
- ui routes on RES (by hand), LX2, RT; tools routes on CTOR, LX2; devlost on CTOR (INJECT=1 COMPLETE=5 crash 0).

## 4. Close checks

- `worldcmp.sh` vs `oldbase` (`b2a5434c`, seed 12345): 118 lines, DIFFERS on ONE line -- the `[PfSeed]` log
  text (`ASYNC worker-thread mode` -> `ASYNC worker-thread`), reworded by row 962's commit `84b58b58f`; no
  world line differs (`runs/close_worldcmp`).
- `stepclose.sh` at `a2136019b` (`runs/stepclose/summary.txt`): mods / menuquit / options / achievements / goaldialog
  / tool_placement / tool_road / tool_removal clean; in-game save (file mode) SAVE OPERATION COMPLETE, crash 0;
  `sa-stress` exit 0 at 139 fps (+0.5%); true release compile exit 0 (939 files, 6,493,754 bytes, 189 warnings as
  before); clean rebuild debug / perf / unit exit 0; gate `closeclean` 0 problems (resave 3/3); NOT IMPLEMENTED
  stubs 0, stub set 0 / 0 vs baseline; tutorial census citizen=16.
- audit `b7_session/AUDIT.md` (62 commits: 54 OK / 5 FINDING / 3 QUESTION): F1 / F5 / F6 documentary, applied in
  CLOSE-A (`40773f626`); F2 (retail DOES test the pathfinder's MemAlloc result: the placement-new shape restored) and
  F3 (the EventLogEntry aggregate zeroed two pads retail leaves alone: a seven-field ctor) in `47cc3b7dd`
  (`behaviour:`); F7 (the callerless `RtString_ReleaseInline`) in CLOSE-B (`048e82e29`). F4 (0e3708da8's `B7:` title
  over a MemFree(null) that is gone) and Q4 (`B7:` titles over inlining-only DIFFs) stand as written: recorded here,
  no amend (B6 C10's rule). Q1 (the file-scoped pragma) is the ruling in NEXT_SESSION §5. Q2 / Q3 (citizen picking,
  the entity-panel close: no gated route) handed on.
- the final gate on the close tree `048e82e29` (`runs/CLOSE`, full mode): builds exit 0, 0 new warnings, unit 920 / 0,
  new game 5683 / 0, tutorial 5773 / 12, loadgame clean, -O2 new game clean, typetable == baseline, resave 5/5 against
  `994eb184`; GATE RESULT 0 problems.

## 5. Residue handed on (NEXT_SESSION §2)

Rows 1000 (AddEventCore's four tests and its release point), 1020 (NewGameDialog's `_padADC` store), 1040
(`Desc_LoadExternalArray`: one untyped body for two retail array instantiations, a type per caller), 1041
(the five remaining `IsDerivedFromOrTrust` callers), 1060-1065 (RES's six: EvictRoster's progress break,
the `liveSeatShuffle` flag over a retail call, ProfessionUI's set-up tests, Expire's ring test,
ResolveExternalByName_Impl's factory test and generation order, RegisterStorageLocation's return); row 310's
four unread files (PART FIXED); the introsort escape; the user decisions not taken (row 5 slot 8, SpawnSubResource
entry 3, row 34's launcher dialog port). The probe technique (C4) is the way to check the unprobed routes'
references once a route exercises them.

## 6. Rules added this step

B7_RULES §7: C1 (retail's Frustum ctors zero first), C2 (the Vector4 pixel rows over Context's memset), C3 (a
resource-load-order change moves a pointer-keyed save tree; bisect with the golden copied into the worktree;
two bisect scripts never share one worktree), C4 (row 942's validation is only as right as each caller's
instantiation; the probe technique; a shrinking payload after a type-validation change is a nulled reference).
ENDGAME B.1 decision 7 DECIDED (the user, 2026-10-09): C++ exceptions where retail unwinds, error returns where
retail returns a status.
