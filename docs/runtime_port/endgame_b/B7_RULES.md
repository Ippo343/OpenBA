# B7 — the named-prerequisite pass: rule sheet (written 2026-10-09, start of the step)

Part B's step after B6. The 23 casts tagged "named prerequisite" (`b4_session/LEFT_FREE.md` §C) each
wait on ONE thing the tree lacks: an inlinable String / Array release, a real constructor where a door
builds raw storage, a non-zeroing construction where retail hands an untouched block, a class member
where a header is viewed as the class. This step supplies the prerequisite and types the site. The B6
RESIDUE (`known_port_issues.md` rows the D block did not reach, rows 991-994, 920-922, 942 / 992, 960-962)
is the same task without a cast and is done here too. Session kit: `b7_session/` (gate scripts copied
from `b6_session/`; `runs/baseline`, the warning baselines and the B11 base objects
`runs/b11base/{debug,perf}` cut from main `a4944195c`, whose exes were built after the last code commit
`a8e786963`).

Starting numbers (main `a4944195c`, B6 CLOSED): PROD 26 = 23 named prerequisite + 3 ruled + 0 work;
code 13, runtime 13, standalone 0, engine 0, dx11 0. Acceptance 1 line off (10 = 19, accepted). Unit
920 / 0. Invariants: new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 == golden
`3efc80a84907fbb8e25d64b4f2c99cff`.

## 0. Scope (the user's, NEXT_SESSION §0 and §2)

The DANGEROUS casts go; only safe casts stay. A raw-storage member or local becomes a real one only in
a commit that reproduces retail's constructor AND destructor (and their POINT in the function). A guard
retail lacks is dropped (B6 D4), never added. A faithful crash is root-caused upstream, and the root may
be a fixture (B6 C5). Device-lost stays DEFERRED (NEXT_SESSION §3). The 3 RULED casts stay listed.
Two user decisions are NOT taken here: `NaturalResourceComponent` slot 8 (row 5) and `SpawnSubResource`
(growthcomponent entry 3) stay as they are; row 34 (the launcher dialog) needs a port of 0x1800a1ae0 and
its dialog resource and is recorded as owed, not done.

## 1. Decisions

D1. **The unit of work is one prerequisite.** Supplying it (an inline release, a real ctor, a Set
    member, a templated core) is one commit; typing each site that waited on it is one commit per
    retail BODY (B6 C1: sites that are one retail body share one commit). Title `behaviour:` when any
    store, call, order or destruction point that RUNS changes (a memset dropped, a zeroing dropped, a
    door's placement-new replaced by the ctor at a different point); title `B7:` when the B11 verdict
    at -O2 is IDENTICAL or SAME for every function in the touched translation units (a typing only).
D2. **What a commit message carries** (B6 D2, unchanged): the cast(s) removed (`file:line`) and the
    metric delta; the retail address and the instruction range read, what it does in one line;
    BEFORE / AFTER and the observable (or "none observable: allocation size only" / "codegen only");
    the B11 verdict per touched function (IDENTICAL / SAME / DIFF with the DIFF read and explained);
    what was RUN ("static gate only: debug / perf / unit builds exit 0, 0 new warnings, unit N / 0").
D3. **B11 is the gate for a typing.** Before editing, a block builds its worktree's `-O2` objects once
    (`mingw32-make -j2 standalone-perf`) — that is its base, OR it uses main's snapshot
    `b7_session/runs/b11base/perf` (same flags, same commit). After the change it runs
    `python docs/runtime_port/endgame_b/b4_session/b11cmp.py <base> standalone/build-perf/obj <tu-substring> --detail`
    on every touched translation unit. A typed local MUST NOT ADD A CALL retail lacks at the measured
    site: for unit 1 the retail instruction ranges on the cast lines are the oracle (every String
    release in `GetNextItem` 0x1800cdf40 is open-coded: DEC [data-4]; on zero store data-8 and CALL
    0x1800c7ad0, else store 0 — the typed local's -O2 code must show MemFree as the only call at each
    release). A DIFF that adds a call is a finding; the site stays raw and the message says why.
D4. **Unit 1 — the inline release.** `System::String::String()`, `String(const String&)`, `~String()`,
    `Release()`, `Adopt()` and `rtcoll::ArrayDtorImpl` get header-defined (`inline`) bodies with the SAME
    reads and stores (`StringRefCount` at data-4; the asymmetric slot: data-8 on the freeing branch,
    null on the sharing branch — reproduce it; `MemFree` / `RtMemFree` stay calls, retail calls
    0x1800c7ad0 too). The retail addresses stay in the comments. Nothing in the tree takes the address of
    these bodies (checked at the kit: `saveload.cpp` / `saveload_meta.cpp` call `~String()` qualified,
    fine). `WideString` (ruled, shares `String::Release`) and `Filename` (derives from String) follow
    for free. Then the sites: `documenttext.cpp:155 / 157 / 159` (GetNextItem's String / Filename
    locals; `ReleaseString` / `AssignString` become the member ops where the instruction range is the
    same), `modification.cpp:272 / 273 / 286 / 287` (the by-value `Collection::Array<uint32_t> / <uint8_t>`
    shape at 0x180025530 / 0x180025580, measured S4 RT2: the EH landing pad and the out-of-line
    ArrayDtorImpl call were the blockers — with the inline body the caller's free + three zero stores
    must land inline as at 0x1800afb71..0x1800afbb6), `avl_trees.cpp:268` (`PtrArrayDtor` over
    `TypeRecord::children`: the raw triple in TypeInfo.h becomes `Collection::Array<TypeRecord*>` ONLY if
    retail Startup's two bare zero stores at 0x1800c0728 / 0x1800c072d come out as such; else the
    function stays and the cast is re-tagged with the new measurement), `introsort.cpp:355`
    (`Array16Assign`'s source: the 0x1800dfd10 caller's stack EffectSlot, 0x1800dfcad..0x1800dfccc),
    `wave24_lane_gfxmat.cpp:363` (`MaterialTexture::name` becomes `System::String` once AddTexture
    0x180103350 and Draw 0x1800c2eb0 B11 as retail's open-coded zero / INC / release), and
    `eventlog.cpp:286` (a typed `EventLogEntry` built with retail's stores in order, destroyed at
    retail's point — right after the grow 0x140019b70, before the autoSlow tail: a scoped block, or a
    ctor that takes the text and an explicit scope).
D5. **Unit 2 — the ctor doors.** A door that placement-constructs (`RtSystem_Random_Ctor` 0x1800c7f20 /
    `_CtorSeed` 0x1800c7ed0, `RtEngine_Engine_Ctor` 0x1800dbf00 / `_Dtor` 0x1800dbec0,
    `RtGraphics_Renderer_Ctor` 0x180105180 / `_Dtor` 0x1800180a0, `RtGraphics_Scene_Ctor` 0x180102730 /
    `_Dtor` 0x1801026c0) becomes the class's real ctor / dtor (its body moves into the member; the
    class keeps NO door beside the member — every caller calls the ctor, the door is deleted with its
    last caller; no `extern "C"` forwarder remains). `System::Random` stays trivially destructible
    (retail has no `~Random`; Value.h's pins hold) — a real ctor does not break B4b-2's reason, which
    was the dtor. Sites: `exe_bodies.h:274` (`GetLoadingSoundString`'s 2520-byte block: read
    0x140099130's frame — the local is a 0x9c4 Random at a fixed RSP offset; a typed local is
    "allocation size only"), `game_main.cpp:334` (the `memset(0, 0x700)` is a store retail lacks:
    0x1800dbf00 runs the allocator ctor first, then fifteen qword zero stores, +0x698 = handle, one
    16-bit store at +0x6f0 — the memset goes in a `behaviour:` commit; the local's dtor runs where
    `RtEngine_Engine_Dtor(eng)` is called today, line 747: a scoped block if anything follows it), the
    seven other `RtSystem_Random_Ctor` callers (newgamedialog 841, noiseui 174, gameupdateloop_load 493
    / 495 / 783, menudialogloopt_stubs 248, splash_loop_stubs 150: each becomes the ctor; a member built
    by a door inside a ctor body becomes a mem-initialiser at its retail position — B11 the ctor),
    `minimap_paint.cpp:173 / 175` (a scoped block `{ Graphics::Renderer renderer; Graphics::Scene scene; …
    context.Reset(); }` — C++ destroys `scene` then `renderer`, which IS retail's order 0x1801026c0 then
    0x1800180a0; the block ends exactly after `context.Reset()`), and the loader's Scene at
    `gameupdateloop_load.cpp:1090` if it is a door too.
D6. **Unit 3 — the zeroing ctors.** `Math::Vector4()` 0x18001e0a0 IS retail's zeroing ctor and stays.
    `Volume::Frustum` gets its REAL perspective ctor `Frustum(const Scalar& near, const Scalar& far,
    const Scalar& fovX, const Scalar& fovY)` (0x1800fa1e0's body, planes initialised in the
    mem-initialiser so no plane is zeroed first; store order planes[0]..[5] then `isOrthographic`
    — B11 against 0x1800fa1e0) and the general ctor 0x1800fa3d0 likewise; `RtVolume_Frustum_Ctor_*`
    are deleted with their last caller. Sites `gameupdateloop_load.cpp:574 / 577` (a typed local; the
    `fr ? fr : frustum` arm goes with the door) and `:800` (the `= {}` byte-zeroing is a store retail
    lacks: `behaviour:`, none observable — every byte is written by the ctor). `gameupdateloop.cpp:1416`
    and `minimap_paint.cpp`'s row: `Context::PixelConstantStaging::pixel[5][4]` — the row-4 store must
    stay ONE 16-byte store (measured: memcpy splits it) and `Context`'s ctor must not gain five zeroing
    stores retail lacks: read `Video::Context`'s ctor (does it memset the staging block? then a
    `Math::Vector4 pixel[5]` member's zeroing is value-identical but a store retail lacks — B6 C8's
    policy: a member retail leaves to the memset stays as the memset writes it). Allowed shapes, in
    order of preference: a `Math::Vector4 pixel[5]` member if the ctor B11s SAME (the memset subsumes the
    zeroing in the same function and no extra store remains at -O2); else a typed accessor on the
    staging struct that returns `Math::Vector4&` over the row WITHOUT a cast expression counted by
    `castmetric.py` (a reference member is not allowed; a union is a pun — not allowed); else the cast
    stays, re-tagged with the measurement, and is listed as residue. No `memcpy`.
D7. **Unit 4 — the two leftovers.** (a) `set_clear.cpp:210`: `IO::Persistence` +0x00 and
    `System::Configuration` +0x10 hold `Collection::Set<Collection::Pair<System::String, System::String>>`
    (the instantiation whose `Clear` IS 0x18000f7d0 — `set_clear.cpp`'s own reading; B6 fixed the Set
    ctor to write root + count only, which is exactly ??0Persistence's two stores 0x1800b8047 /
    0x1800b804a). `OwnedSettingsMap` / `SettingsMap` / `SettingsMapNode` go: the node view is the
    template's `Node` (static_assert the layout it pinned: key +0x00, value +0x08, parent +0x10, left
    +0x18, right +0x20, balance +0x28, height +0x2a, 0x30 bytes); `rtsetclear::Clear` and its two
    helpers are deleted when every caller (engine_platform.cpp:186, persistence_registry_win.cpp:235,
    persistence_saves.cpp, configuration.cpp:545) calls the member; `rtavl::StringMapInsert` / Map_Find
    (0x18002a090 / 0x18002ace0) are typed on the Set (or ARE `Set::Insert` / `Find` if the template's
    bodies are those — read both). Configuration's by-hand zero + Clear becomes the member's ctor / the
    member's Clear at the same instructions (0x1800c518f). One `behaviour:` H5 commit per owner class
    (B6 D8: ctor stores and dtor releases in retail's ORDER, the member at its offset, the pinning unit
    tests rewritten in the same commit). (b) `goals_serialize.cpp:86`: `WorkCellBST_Remove_ca60`
    (0x14001ca60, one body over five node / header types by ICF) becomes a TEMPLATE over the node type
    (as `AvlRebalance<>` in B4): header-defined in `Work/exe_bodies.h` (or a `Work/CellBst.h` it
    includes), every caller instantiates it on its own tree / node type, the five `CAST-OK: elem_stride`
    wrappers and the erased core go. Same for `WorkCellBST_Insert_9a40` / `_Serialize_9de0` /
    `WorkBST_BSTRemove_a6e0` ONLY if their bodies read the same fields over every layout (the ICF
    argument); else they stay with their wrappers and the hand-in says which. `B7:` commits (B11: the
    instantiations are the one body's instructions; five copies instead of one is code size, not
    behaviour).
D8. **The two harness puns** (coordinator's ruling, the user's call deferred to the report).
    (a) `citizenaicomponent.cpp:590`: the force-work harness stores the building into
    `CitizenComponent::disease` (+0x68), the slot `reference_citizen_0x68_is_disease_not_housing`
    says is `Disease*`: a building pointer in a Disease slot is a latent fault under
    `BANISHED_FORCE_WORK`, planted under the old "+0x68 is housing" belief. The store GOES; the
    harness's homing is the `citizen->home` placement above it. Read every force-work reader of +0x68
    first (grep `g_pForceWorkBuilding`, `disease` in the harness files); if one reads the building
    there, retarget it to `home`. Observable only under the harness; `behaviour:` naming the route.
    (b) `clearareacomponent.cpp:191`: `ClearAreaComponent_HarnessSweep` builds a raw zeroed component.
    It becomes a real `ClearAreaComponent` local (its ctor registers with the InternalFactory, its dtor
    must unregister — read both; if the dtor does not unregister, the harness entry keeps a heap
    `new ClearAreaComponent` destroyed by the owner path the ctor expects) with `_entity` set and
    `FootprintSweep()` called. If neither shape is safe, the cast is RE-TAGGED `RULED: harness
    (test code; the user decides retire / keep)` and the report says so. Its callers are
    `tool_harness.cpp`'s routes, which the gate's `tools` run exercises.
D9. **The residue rows (no cast).** Every SUSPECTED 14d row is READ at its address (B6 D6): retail
    has the test → `NOT A DIFFERENCE` with the instruction quoted (`B7:` doc commit, the status cell
    appended); retail lacks it → the test GOES (`behaviour:` with the range), unless D4(a) / (b) of
    B6 applies and the site's comment names the harness route / shared template / PORT GAP. Rows and
    owners: §3. Row 942 / 992 ORDER (row 992's own text): 992 first (ProfessionList::Serialize on the
    Profession instantiation 0x140018020), then every `ExternalPtr<T>::Serialize` caller on its
    retail instantiation (the type cell its retail body tests), THEN 942 (`IsDerivedFromOrTrust` →
    `IsDerivedFrom(obj, cell)` storing null otherwise, one commit). A route that then loses a
    resource (a null where a default was kept) is a FINDING: the resource is of the wrong declared
    type somewhere upstream — root-cause it, never re-trust. Row 993: `IsDerivedFromNullSafe` →
    the member `IsDerivedFrom` (0x1800c0390 dereferences `this`), one commit per retail body, row 7
    first. Row 994: read 0x140057b20 (RES: it is `citizen_lifeevent_handler.cpp`, with row 310's G1 /
    G28 bullets) and the two dialog bodies G30 / G33 (LX), rule (a) / (b) with the address or drop.
D10. **Rulings owed** (921, 922, 920): 921 / 922's "partial-world" / "inert controller" reasons are
    PRE-B and STALE unless the block finds a route that builds a WorkPlaceComponent without a
    StatusIconComponent or a controller without a machine (name it, D4(a)) — else the tests go. Row
    920: the ctor's `extraField = 0` / `_pad34 = 0` stores go if every rotation reader is preceded by
    a writer on every path (list them in the message); else it is a finding at the reader.
D11. **Rows that change the generated world or the loader counts:** none expected. If `worldcmp.sh`
    differs at the close, every differing line is attributed to a commit in CLOSE_NUMBERS or the
    commit is reverted.

## 2. Per-site recipe

1. Read the site and its tag in full; read the INVENTORY row; read retail at the quoted address
   (decompile + disassembly, `program="Application-x64.exe"` or `"Runtime-x64.dll"` passed EXPLICITLY;
   never `switch_program`).
2. Supply the prerequisite (one commit), then type the site: the smallest change that makes the
   tree's -O2 object show retail's shape (D3). No new helper unless retail has the function. No null
   guard added. Delete the `LEFT: DEFERRED (named prerequisite …)` / `audit DEFERRED (…)` tag with the
   cast; if a cast legitimately remains for ANOTHER reason, re-tag it with that reason and the
   measurement (the coordinator rules it at the close).
3. Build `-j2` debug + perf + unit (600000 ms timeout, foreground, re-invoke). 0 new warnings against
   `b7_session/runs/warn_baseline_*.txt` (`tr -d '\r'` the baseline first in a worktree, B6 C3). Unit
   0 failures. `castmetric.py` on your worktree: the site's casts gone, nothing new. `b11cmp.py` on
   every touched TU (D3).
4. Commit (D1 / D2). One prerequisite or one retail body per commit.
5. `known_port_issues.md`: a row that exists gets ` | FIXED (B7 <block>, <hash>): <one line>` appended
   to its status cell; a cast with no row needs none (the tag was the row). New findings: new rows at
   the end of the relevant §14 table, numbered RT 1000-1019, CTOR 1020-1039, LX 1040-1059, RES
   1060-1079.
6. Hand in (BLOCK_BRIEF.md).

## 3. Shape and order

| block | unit | sites / rows | files owned |
|---|---|---|---|
| RT | 1 (the inline release) | documenttext 155 / 157 / 159, modification 272 / 273 / 286 / 287, avl_trees 268, introsort 355, wave24_lane_gfxmat 363, eventlog 286 | `runtime/types/system/{String,Filename,WideString}.h`, `runtime/src/system/{string,filename,widestring}.cpp`, `runtime/types/collection/Collection.h` (the ArrayDtorImpl lines only), `runtime/src/collection/{array,avl_trees,introsort,array_internal}.*`, `runtime/src/io/modification.cpp` (+ `runtime/types/io/Modification.h`), `runtime/src/ui/documenttext.cpp`, `runtime/src/graphics/wave24_lane_gfxmat.cpp` + `runtime/types/graphics/Material.h`, `runtime/types/object/TypeInfo.h` + `runtime/src/object/factory_dtors.cpp`, `code/Game/Game/eventlog.cpp` + `EventLog.h`, `runtime/types/audio/Audio.h` (EffectSlot) |
| CTOR | 2 + 3 (ctor doors, zeroing ctors) | exe_bodies.h 274, game_main 334, minimap_paint 173 / 175, gameupdateloop_load 574 / 577 / 800, gameupdateloop 1416 | `runtime/types/system/Value.h`, `runtime/src/system/value_types.cpp`, `runtime/types/engine/Engine.h`, `runtime/src/engine/engine_lifecycle.cpp`, `runtime/types/graphics/{Camera,Scene,Renderer}.h`, `runtime/src/graphics/{renderer,scene}.cpp`, `runtime/types/volume/Volume.h`, `runtime/src/volume/frustum.cpp`, `runtime/types/video/Context.h`, `code/Game/Game/{game_main,minimap_paint,gameupdateloop,gameupdateloop_load,menudialogloopt_stubs,splash_loop_stubs}.cpp`, `code/Game/Game/exe_bodies.h`, `code/Game/Game/PlayerController.h`, `code/Game/Dialog/{newgamedialog,noiseui}.cpp`, `code/Game/Game/weather_particles.cpp` |
| LX | 4 + the harness puns + rows 992 / 942 / 993 / 994 (G30, G33) | set_clear 210, goals_serialize 86, citizenaicomponent 590, clearareacomponent 191; rows 992, 942, 993, 994 (dialog parts) | `runtime/src/collection/set_clear.cpp`, `runtime/types/io/Persistence.h`, `runtime/src/io/persistence_*.cpp`, `runtime/types/system/Configuration.h`, `runtime/src/system/configuration.cpp`, `runtime/src/engine/engine_platform.cpp`, `runtime/src/collection/set_avl*.h`, `code/Game/Work/exe_bodies.h`, `code/Game/Work/work_base_cellbst.cpp`, `code/Game/Game/goals_serialize.cpp`, the five `WorkCellBST_Remove_ca60` caller files (Action/action_base_complete, action_pf_region, region_paircache; Component/citizenai_work_scheduler; Work/cleararea), `code/Game/Component/{citizenaicomponent,clearareacomponent}.cpp` + the harness files they name, `code/Game/Game/{serialize_externals,professionlist}.cpp`, `code/Game/Object/factory_registration.cpp`, the files rows 7 / 8 / 10 / 40 / 92 / 993 name in Dialog, `code/Game/Dialog/{toolui,statusbarui}.cpp` |
| RES | the SUSPECTED 14d residue + rulings | rows 82, 83, 84, 89 (Tutorial), 78, 692 (Player), 98, 99, 960, 962 (Game), 250, 251, 252, 291, 330, 310's G1 / G28 + 994's 0x140057b20, 350, 352, 920, 921, 922 (Component), 451, 443 (Dialog; 443 also Action), the GatherRawMaterials site-ctor guards (Work), `Pf_RegionPairCacheTick_5d10`'s null tests (Action) | the files those rows name; NOT a file in another block's list (443's `Dialog/exe_bodies.h` and `Action/action_child_ctors2.cpp` are RES's; `region_paircache.cpp`'s Tick is RES's, its Remove call is LX's — RES edits the Tick lines only and says so) |

Order: the four blocks run in parallel in worktrees (`-j2`, static gate only). Merge order: RT first
(the header change every other block's build also compiles), then CTOR, LX, RES; a block that merged
main after RT's merge re-B11s nothing (its own TUs are unchanged by RT's headers except by inlining,
which is the point). The coordinator alone gates (`b7_session/mergegate.sh`), with `ui` on LX and RES,
`tools` on LX (the harness) and CTOR (minimap / frustum draw paths).

## 4. The gate (every merge, coordinator only)

`b7_session/mergegate.sh <branch> <label> "<title>" [ui] [tools]`: builds the MERGED tree before the
merge commit exists, then `gate.sh` (debug / release / perf / unit builds, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame, the -O2 new-game route, typetable == baseline, resave 5/5 ==
golden). `devlost.sh` on CTOR (it touches Renderer / Scene / Context). A crash on a route is read with
`standalone_crash.log` first (skill `triage-run`), bisected by the block's commits, root-caused upstream.

## 5. Working rules (B6's, unchanged)

- Agents never run the game; builds `-j2` in the foreground; at most four building at once; the
  coordinator alone gates. No `git stash`, no amend / rebase / push, no sub-agents, no `taskkill /IM`.
  A fresh worktree is reset to the NAMED base commit (`git log -1` before editing). Merge main before
  the final build.
- Commit the gate table (and anything else) BEFORE `mergegate.sh`: it refuses a dirty tree.
- A long close runs with `nohup bash … &` from a short foreground call.
- Never `git add` the session directory wholesale (`runs/`, `gate_*.out`, merge logs are untracked).
- Write files that contain backslashes with the Write / Edit tool, never a heredoc.
- A marker proposal is not a marker; no ceiling rises in this step.
- `keepboth.py` doubles a table row both sides edited; the next merge de-duplicates it or it is done
  by hand. A block's hand-in that names a row done by another block is resolved by the LATER merger
  keeping main's body.

## 6. Acceptance at the close

`b4_session/acceptance.sh` line 01 = the new PROD total with the named-prerequisite group at its residue
(target 0; a residue is listed site by site in `LEFT_FREE.md` §C with the MEASUREMENT that keeps it and
the owing row); lines 02-09, 11, 12 at 0; 10 = 19 (accepted). Unit 0 failures. Every residue row's status
cell ends in FIXED / NOT A DIFFERENCE / LEFT (with the owing row or the user decision named).
`stepclose.sh` clean. `worldcmp.sh` identical, or DIFFERS with every line attributed (D11).

## 7. Corrections (dated, appended as blocks find them)

2026-10-09, from block CTOR's hand-in:
- C1. **Retail's Frustum ctors ZERO FIRST** (D6 / INVENTORY A.3 were wrong; Volume.h's banner was right):
  0x1800fa1e0 writes twelve zero qwords over +0x00..+0x5f and the +0x60 byte (0x1800fa212..0x1800fa241)
  before the planes; 0x1800fa3d0 likewise (0x1800fa3e5..0x1800fa41c). The real ctors mirror that: the
  `Vector4()` members and the flag in the mem-initialiser, then the plane stores. The "untouched stack
  block" on the three cast lines described the CALLER, which is still true (no caller-side zeroing).
- C2. **`Context::PixelConstantStaging::pixel` is `Math::Vector4[5]`** with no ctor cost because `Context`
  is built over raw storage by `RtVideo_Context_Ctor`'s memset (retail's nineteen zero qwords), so the
  zeroing ctor never runs (D6 shape 1 held: context_ctor / setlight / Draw B11 IDENTICAL). The memsets
  over a block holding `Vector4` take `static_cast<void*>` (-Wclass-memaccess).

2026-10-09, from the RES merge gate (resave 0/5):
- C3. **A resource-LOAD-ORDER change moves a pointer-keyed save tree.** Row 84 (`184a69e9f`) made
  `NamedExternalRef::Resolve` find-only as retail; the old load-on-miss had loaded three professions early
  (factory generation 0, during the goal description loads). `CitizenBehavior`'s profession bucket tree
  (+0x100) is keyed on the raw `Profession*` and saved in that order, so the golden moved by one 105-byte
  block with identical keys, counts and members. RULE: a resave DIFF with `reftable_diff.py` saying
  "rows=+0 payload=+0" is read as BYTES (python: the differing ranges), bisected commit by commit in a
  scratch worktree WITH THE GOLDEN COPIED IN (the `.bin` is git-ignored: an empty worktree makes its own
  golden and reports NEW-GOLDEN, not DIFF), and re-cut only once the container and the ordering rule are
  named (`tools/resave_gate/README.md`). Two bisect scripts must never share one worktree.
