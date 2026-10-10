# B8 — the quality pass: rule sheet (written 2026-10-09, start of the step)

Part B's step after B7. `exe_bodies.h` / `*_funcs.h` under `src/game/` declare 412 free functions
(acceptance.sh line 08's count); LEFT_FREE.md §A says 234 of them are MEMBERS of a class in retail
("first parameter is the object, retail symbol is a member"), 13 more are the B7 `ExternalPtr<T>` /
array serializer cores, 26 are forwarders to a member that already exists, 25 are type-erased cores
of a class template, 69 are free in retail and 18 are harness / port-only. This step makes every
member the member it is in retail — declared in the class's header with its retail address, defined
where it is defined today, called by name from every caller — deletes the forwarders and the erased
cores with their last caller, and leaves the free-in-retail and harness functions listed in a
REGENERATED `LEFT_FREE.md` §A (the generating scripts are KEPT this time: `b8_session/leftfree/`).
It also settles the B7 residue rows (NEXT_SESSION §2) in a RES block, and adds the two unprobed
routes to the harness. Session kit: `b8_session/` (gate scripts copied from `b7_session/`; `runs/baseline`,
the warning baselines and the B11 base objects `runs/b11base/{debug,perf}` cut from main `ed149bf42`,
the DEC commit — the kit commit after it is docs-only, so the objects are the kit's).

Starting numbers (main `ed149bf42` + the kit): castmetric PROD **3** (1 named prerequisite + 2 ruled);
acceptance line 08 = 0 of 412 declared (every name is listed); unit 920 / 0; invariants: new game
5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 == golden `994eb184a8e88db9372b5b9cd13a9ed9`
(unchanged by the two DEC commits: the resave fixture is a loaded save; the spread of sub-resources
needs sim time).

## 0. Scope (the user's, NEXT_SESSION §2)

The two user decisions are DONE first and gated (b8_session/GATE_TABLE.md row DEC: `c60271c1b` slot 8
installed, `ed149bf42` SpawnSubResource's callee wired and its spawn-chance constant corrected). The
pass itself changes NO behaviour: every commit is a `B8:` typing commit whose B11 verdict at -O2 is
IDENTICAL / SAME / RENAMED-SAME for every function in the touched translation units. A retail
difference a block FINDS while reading is a new `known_port_issues.md` row (§1 D10), not a fix — RES
and the next step own fixes. Device-lost stays DEFERRED (NEXT_SESSION §3). The 3 casts stay as they
are unless RT's `Array<T>::operator=` re-measures the introsort escape (RES re-measures it, D9).

## 1. Decisions

D1. **The unit of work is one CLASS.** All the rows of one target class (`b8_session/INVENTORY.md`,
    column "target class") go in one commit titled `B8: <Class> -- N members moved (<names>)`
    (a forwarder deleted or an erased core templated is named in the title too). A class with
    many rows may be split by defining file (`B8: CitizenComponent (citizen_needsupdate.cpp) -- …`).
    Never `behaviour:` in COMP-A / COMP-B / GAME / ENT / RT / MISC: if a move cannot keep B11
    IDENTICAL / SAME / RENAMED-SAME, the row stays free and is RE-TAGGED on the line with the
    measurement (`// B8: stays free -- <verdict>: <what differs>`), listed in the hand-in.
D2. **What a member looks like.** Declared in the class's own header (the file that defines the
    class — `src/game/<Ns>/<Class>.h`, or the module header the class lives in) with the retail
    address as the trailing comment, like every other member; defined in the `.cpp` that defines
    the free function TODAY (bodies do not move between files: the tree mirrors retail's source
    layout, and the B11 base is per translation unit); `self->x` / `comp->x` becomes `x`; the first
    parameter goes; `const` on the member only where the free function took `const X*`. NOT virtual
    unless the retail body is a vtable slot of the class, in which case it is ALREADY declared in
    the class (then the row is a forwarder: callers call the slot). No new helper, no new overload,
    no default argument retail lacks, no `static` unless retail's body reads no object (then it is
    `free in retail`, not a member: re-tag, hand in). A row whose first parameter is `void*` /
    `const void* /*unused*/` (LEFT_FREE §B2) gets the class retail hands in RCX (read the retail
    body: RCX is `this` of the class whose fields it reads; an RCX the body never reads is still that
    class's `this` when the caller's retail passes it — the caller's `MOV RCX,…` decides).
D3. **Callers.** `F(obj, a, b)` becomes `obj->F(a, b)` (`obj.F(a, b)` through a reference / value);
    the argument expressions stay as written (no new temporaries, no reordering: a call's
    arguments are evaluated the same way after the move). A caller that passes the function as a
    FUNCTION POINTER (B7 C4: the two CitizenBehavior bucket-tree key serializers, any `fn_*` typedef
    slot) is a caller too: a member cannot fill a plain function-pointer slot. Read what retail puts
    in that slot (a thunk? a static?) and spell THAT; if the slot needs a free function, the row stays
    free and is re-tagged `B8: stays free -- passed as a function pointer at <file:line>; retail <what>`.
    Callers in `standalone/` (the harness) and `tests/` are callers; `docs/` and comments are not.
D4. **The declaration line.** Delete the declaration's line(s) only — no reflow, no re-grouping of
    the header, no comment rewrite beyond deleting the row's own comment — so that six blocks
    deleting lines from the same `exe_bodies.h` merge without conflict. The `// ---- file.cpp` group
    headings stay even when empty; an `exe_bodies.h` / `*_funcs.h` left with no declaration is
    deleted by the COORDINATOR at the close (its includes with it), not by a block.
D5. **Erased cores become the class template's member** (B7 D7(b), `Work/CellBst.h`; memory:
    typed wrappers over the one ported core, never per-type copies). `Reimpl_Array_Resize_*` /
    `QwordArraySetSize` / `ActRm_ArrayResize16_da70` / `AmbientArrayResize_Reimpl` … are
    `Collection::Array<T>::Resize` (one header-defined template; each caller instantiates on its
    own T; the stride is `sizeof(T)`); the `Avl38Set*` / `DecalVertexSet*` / `WorkBST_*` /
    `WorkCellBST_*` / `WorkLeaf_Bst*` cores are the `Set<T>` / cell-BST template's members over the
    node type; `Reimpl_Array_Serialize_*` / `Reimpl_ExternalPtrArray_Serialize` are
    `Collection::Array<T>::Serialize` instantiations. The core and every `CAST-OK: elem_stride`
    wrapper go with their last caller. B11: the instantiations are the one body's instructions
    (ONLY-BASE the core + ONLY-TIP the instantiations, paired in the hand-in; a DIFF inside an
    instantiation is read). An erased core whose body reads DIFFERENT fields per layout (not ICF)
    stays, re-tagged with the field that differs.
D6. **`ExternalPtr<T>::Serialize` (RT).** The 13 B7 cores and the 14 older `ExternalPtrSerialize_*`
    become instantiations of ONE `template<class T> void Object::ExternalPtr<T>::Serialize(IO::Serializer&)`
    whose type cell is T's (the `DECLARE_EXE_TYPE` / type-info cell the current core names in its
    comment). B7 C4 is the law here: every caller stays on the instantiation LX2 put it on (the one
    whose retail body does `MOV EDX,[cell]` before `CALL [0x140152a98]`); a template must not let a
    caller's FIELD TYPE pick T — where the field's T differs from the retail cell (the two bucket-tree
    keys on Profession / ComponentDescription, the `:edge` meshes on EdgeMesh …) the call spells the
    instantiation explicitly. The resave gate is the oracle: a payload that SHRINKS is a nulled
    reference (C4) and the merge is refused.
D7. **B11 is the gate for a typing** (B7 D3). Base: `b8_session/runs/b11base/perf` (and `/debug`), or
    a block's own `mingw32-make -j2 standalone-perf` at the kit commit before editing. After each
    class: `python docs/runtime_port/endgame_b/b4_session/b11cmp.py <base> standalone/build-perf/obj <tu-substring> --detail`
    on every touched TU (the definer AND every caller TU). Expected: RENAMED-SAME for the moved body
    (its symbol changed), IDENTICAL for its callers (a direct call to a renamed symbol is the same
    call: b11cmp pairs renamed symbols), SAME where only register allocation moved. A DIFF (a call
    added / dropped / reordered, a store or compare changed) is read; it is a finding (the move
    changed the argument evaluation, dropped a copy, …): the row stays free (D1).
D8. **MISC's rulings.** `free in retail` and `harness / port-only` rows STAY free; the block confirms
    the ruling by reading the retail caller (RCX not an object of a class the body reads → free) and
    says so in the hand-in; a row LEFT_FREE called free that retail has as a member is moved (D2).
    `unclear` rows are read and ruled. The regenerated `LEFT_FREE.md` §A at the close lists ONLY the
    survivors with their ruling (the coordinator runs `b8_session/leftfree/`; blocks do not edit
    LEFT_FREE.md).
D9. **RES.** Rows 1040 (`Desc_LoadExternalArray`: a type per caller, then the member test — the LX2
    array cores are the shape; coordinate with RT's D6: RES types the callers, RT owns the template),
    1041 (`IsDerivedFromOrTrust`'s five callers: read each retail body, spell `IsDerivedFrom` where
    retail tests, delete the helper), 1000 (AddEventCore's four tests and its release point), 1020
    (`_padADC` store), 1060-1065 (RES's six B7 findings: EvictRoster's progress break, the
    `liveSeatShuffle` flag over a retail call — DIRECTION.md says no live-feature gates: the flag goes
    and the call is unconditional, `behaviour:`; ProfessionUI's set-up tests, Expire's ring test,
    ResolveExternalByName_Impl's factory test and generation order, RegisterStorageLocation's return),
    row 310's four unread files; the introsort escape re-measured AFTER RT's merge (`Array16Assign` →
    `Array<T>::operator=`); the two unprobed routes (citizen click-picking, row 252; closing an
    entity panel, 994 G33) added to `standalone/tool_harness.cpp` as `BANISHED_TOOL_HARNESS=pick` /
    `=panelclose` (the harness's existing route shape; the coordinator runs them). RES's commits are
    `behaviour:` (B7 D2's message shape) or `B8:` doc commits (NOT A DIFFERENCE with the instruction
    quoted).
D10. **A retail difference found while moving** (a guard retail lacks, a wrong constant, a dropped
    call) is NOT fixed by the moving block: a new `known_port_issues.md` row at the end of the relevant
    §14 table (COMP-A 1080-1099, COMP-B 1100-1119, GAME 1120-1139, ENT 1140-1159, RT 1160-1179, MISC
    1180-1199, RES 1200-1219) with the retail instruction range, the body moved AS IS. RES takes the rows
    it can reach before the close; the rest is the next step's residue.
D11. **Rows that change the generated world or the loader counts: none** (no behaviour change outside
    RES). `worldcmp.sh` at the close is compared with the DEC run (`runs/DEC_worldcmp`), not with B7's:
    the two DEC commits legitimately moved it (attributed in CLOSE_NUMBERS).

## 2. Per-class recipe

1. Read every row of the class in INVENTORY.md and the declaration's comment in the header; read
   the body; read the retail body at the address (decompile AND the first instructions: what RCX is,
   whether RDX is the second parameter — Ghidra drops arguments on indirect calls and forwarders).
2. Declare the member in the class's header at a sensible place (near its slot / retail-address
   neighbours), definition rewritten in place (D2), every caller by name (D3), the declaration line
   deleted (D4).
3. Build `-j2` debug + perf + unit (600000 ms timeout, foreground, re-invoke). 0 new warnings against
   `b8_session/runs/warn_baseline_*.txt` (`tr -d '\r'` the baseline first in a worktree). Unit 0
   failures. `castmetric.py` on your worktree: unchanged (3), nothing new. `b11cmp.py` on every
   touched TU (D7).
4. Commit (D1). Title `B8: <Class> -- N members moved (...)`; body: the rows (name → member, retail
   address), the B11 verdicts per TU (one line each), what was RUN ("static gate only: debug / perf /
   unit builds exit 0, 0 new warnings, unit 920 / 0, castmetric 3"), the rows left free and why.
5. Hand in (BLOCK_BRIEF.md).

## 3. Shape and order

| block | rows | classes / rows | files owned (headers + bodies; callers anywhere, line-local) |
|---|---:|---|---|
| COMP-A | 35 | CitizenComponent (33), Component (11), CharacterComponent / CharacterGrid / CharacterDescription, ComponentDescription (7), CropFieldComponent, CropSlotArray, ConsumeProduceComponent, ClearAreaComponent, Ambient*, AutoPickupComponent | `src/game/Component/{CitizenComponent,Component,CharacterComponent,CharacterGrid,CharacterDescription,ComponentDescription,CropFieldComponent,ConsumeProduceComponent,ClearAreaComponent,AmbientEmitterComponent,AmbientEnvironmentComponent,AutoPickupComponent}.h` and their `.cpp`s (`citizen_*.cpp`, `citizencomponent*.cpp`, `component*.cpp`, `character*.cpp`, `componentdescription*.cpp`, `cropfield*.cpp`, `consumeproduce*.cpp`, `cleararea*.cpp`, `ambient*.cpp`, `autopickup*.cpp`) |
| COMP-B | 52 | the other Component classes: TradeComponent (7), StorageComponent (6), DestroyComponent, InteractComponent, ModelComponent, NaturalResourceComponent, DiseaseComponent, MapComponent, PastureComponent, WorkPlaceComponent, FireComponent, GrowthComponent, HerdComponent, HighlightComponent, HuntComponent, ParticleComponent, PickingComponent, PopulationComponent, RawMaterialComponent, ResidenceComponent, ResourceProduceComponent, SpawnComponent, TrackerComponent, TradeMerchantOrder / TradeResourceNode, WorkComponent, the *Description rows | the headers and `.cpp`s of those classes in `src/game/Component/` |
| GAME | 78 | Map (17), MenuDialogLoopT (16), WorldGeometry (11), PlayerToolManager (8), WorkScheduler (8), GameUpdateLoop, EventLog, GameSettings, MapGenerator, Minimap, PlayerController, WeatherSimulation, GameContainer, MusicPlayer, PathfinderThread, ProfessionList, WeatherParticleGeometry, NameDatabase | `src/game/Game/*.h` and the `.cpp`s of those classes |
| ENT | 37 | ComponentEntity (37 rows incl. the `fn_FindComponent_*` family), CitizenBehavior (19), EntityList, PathGraph; TradeUI, NoiseUI, TownHallInventoryEntry, DebugUI, MainMenuDialog; Tutorial | `src/game/Entity/`, `src/game/Dialog/`, `src/game/Tutorial/` headers and bodies |
| RT | 58 | `Collection::Array<T>` (Resize / Serialize / the stride cores, 15 + 6), `Object::ExternalPtr<T>::Serialize` (27, D6), `Serializer` erased cores (5), `InternalPtr<UI::Element>` (1), the `NameDB_*` pair if NameDatabase is a runtime class (else GAME's) | `src/engine/collection/*.h`, `src/engine/collection/*`, `src/engine/object/{External,Internal,ExternalPtr}*.h`, `src/engine/object/*`, `src/engine/io/Serializer.h`, `src/game/Game/serialize_externals.cpp`, `src/game/Game/exe_bodies.h`'s Reimpl_Array_* / ExternalPtrSerialize_* lines, `src/game/Component/exe_bodies.h`'s Ambient*Resize lines |
| MISC | 152 | EditTool, BridgeTool, FencedTool, Tool (Player); MoveTo, DropOffRawMaterial, ActionStack, RegionPairCache, PickupRawMaterial, Node (Action); Work, BuildRoad, BuildStructure, ClearArea, the `*_CtorFromEntity` / `*_Ctor_Impl` rows (Work); the dialog `*_Ctor_Standalone` rows; the Work BST / Avl38Set / DecalVertexSet erased cores (D5); the 69 `free in retail` and 18 `harness` rulings (D8); LEFT_FREE §B2's `void*`-first rows | `src/game/Player/`, `src/game/Action/`, `src/game/Work/` headers and bodies, `src/game/Object/`, `standalone/tool_harness*.cpp` for the harness rows' callers |
| RES | — | NEXT_SESSION §2's residue rows (D9) | the files those rows name: `Game/eventlog.cpp`, `Dialog/newgamedialog.cpp`, `Component/{workplacecomponent,componentdescription,firecomponent,firecomponent_ignite,particlecomponent}.cpp`, `Object/factory_registration.cpp`, `Entity/componententity.cpp` (row 1041's caller lines only), `Game/town_workplace_registry.cpp`, `Dialog/professionui.cpp`, `Action/region_paircache.cpp` (Expire only), `Game/town_storage_seed.cpp`, row 310's four files, `standalone/tool_harness.cpp` (the two new routes), `standalone/options.{h,cpp}` (the `liveSeatShuffle` flag removed) |

Wave 1 (four worktrees, `-j2`, static gate only): RT, COMP-A, COMP-B, GAME. Wave 2: ENT, MISC, RES.
Merge order within a wave: as they hand in, RT first when it is ready (its headers are what every other
block's TUs compile; a block that merged main after RT re-B11s nothing — its TUs differ by inlining
only). The coordinator alone gates (`b8_session/mergegate.sh`), `ui` on GAME / ENT (dialogs, the menu
loop), `tools` on COMP-* / MISC / RES (the harness routes); RES's two new routes run by the coordinator
on RES's merge and at the close.

## 4. The gate (every merge, coordinator only)

`b8_session/mergegate.sh <branch> <label> "<title>" [ui] [tools]`: builds the MERGED tree before the
merge commit exists, then `gate.sh` (debug / release / perf / unit builds, 0 new warnings, new game
`5683/0`, tutorial `5773/12`, loadgame, the -O2 new-game route, typetable == baseline, resave 5/5 ==
golden `994eb184…`). A resave DIFF after RT is read with B7 C3 / C4 FIRST (bytes, then the probe).

## 5. Working rules (B7's, unchanged)

- Agents never run the game; builds `-j2` in the foreground; at most four building at once; the
  coordinator alone gates. No `git stash`, no amend / rebase / push, no sub-agents, no `taskkill /IM`.
  A fresh worktree is reset to the NAMED base commit (`git log -1` before editing). Merge main before
  the final build.
- Commit the gate table (and anything else) BEFORE `mergegate.sh`: it refuses a dirty tree.
- A long close runs with `nohup bash … &` from a short foreground call.
- Never `git add` the session directory wholesale (`runs/`, `gate_*.out`, merge logs are untracked).
- Write files that contain backslashes with the Write / Edit tool, never a heredoc (a heredoc through
  the Bash tool turned `'\0'` into a NUL byte on 2026-10-09).
- A marker proposal is not a marker; no ceiling rises in this step.
- `keepboth.py` doubles a table row both sides edited; the next merge de-duplicates it or it is done
  by hand.

## 6. Acceptance at the close

`b4_session/acceptance.sh` line 08 = 0 against the REGENERATED `LEFT_FREE.md` §A (survivors only: `free
in retail`, `harness / port-only`, and every `B8: stays free` re-tag with its measurement); the
`member of` count in §A is 0; line 01 = the leftfree_c.py groups (3, or 2 if the introsort escape
re-measures as gone); lines 02-09, 11, 12 at 0; 10 = 19 (accepted). Unit 0 failures. Every residue
row's status cell ends in FIXED / NOT A DIFFERENCE / LEFT (with the owing row named). `stepclose.sh`
clean. `worldcmp.sh` vs the DEC run identical, or DIFFERS with every line attributed (D11). The empty
`exe_bodies.h` / `*_funcs.h` files deleted (D4).

## 7. Corrections (dated, appended as blocks find them)

2026-10-10, from block COMP-A's hand-in:
- C1. **b11cmp.py does not pair a free function with the member it became** (it pairs a rename only when
  the qualified name is unchanged), so a move shows as ONLY-BASE + ONLY-TIP and the callers as DIFF on the
  callee's symbol. `b8_session/b11pair.py BASE TIP MAPFILE <tu-substring>... [--detail]` is b11cmp's parser
  with a by-hand map (`old_bare_name -> Class::Member`, one per line): the moved body pairs as
  RENAMED-IDENTICAL / RENAMED-SAME / RENAMED-DIFF and callers are compared with the callee renamed. D7's
  expected verdicts read with it: RENAMED-IDENTICAL for the body, IDENTICAL for the callers.
- C2. **A devirtualised call inside a moved body is a RENAMED-DIFF to accept when the target is the slot's
  own** (CitizenComponent::ShowTool: `static_cast<Graphics::Geometry*>(&meshInstance)->Update(t)` became a
  direct `MeshInstance::Update` call once GCC saw the embedded member's dynamic type): same callees in the
  same order, stores and compares equal; quoted in the commit, not a reason to keep the row free.

2026-10-10, from block GAME's question (8 of its 10 forwarders and 10 of its members carry a null test):
- C3. **A guard a forwarder carries, or a `if (!self) return;` at the top of a body retail never tests, is
  DROPPED in a `behaviour:` commit** (CLAUDE.md / B6 D4: a guard retail lacks is dropped, never added) --
  D10's "not fixed by the moving block" does not apply to this one pattern, because keeping the row free
  would leave the pass unfinished for a port-only test. One `behaviour:` commit per forwarder / body, B7 D2
  shape: every caller's retail call site quoted (the object handed with no test), the tree's callers read for
  a null source (a harness route, a shared template, a PORT GAP arm -- B6 D4(a)/(b)); where one exists the
  guard stays AT THAT CALLER with the comment naming it. Never `if (obj) obj->F()` written at callers. The
  dropped compare is the expected B11 DIFF, quoted. The coordinator's gate (the full route set) is what
  catches a null only the game reveals.
- C4. **The 36 `ExternalPtr<T>` serializer rows and the 10 erased resize cores are the same pattern at scale**
  (RT's hand-in, rows 1160 / 1161): the exe-side cores are the retail template body PLUS port-only tests (a
  null-factory return, a lazy type-by-name load with no cell, slot null tests); `Object::ExternalPtr<T>::Serialize`
  / `SerializeArray` and `Array<T>::SetSize` / `Assign` already exist over the retail bodies. They are done as
  RT2: `behaviour:` commits per CALLER FAMILY, each caller put on the existing member on its LX2 instantiation
  (B7 C4), the port-only tests dropped with the callers quoted, gated by the resave golden (a payload that
  shrinks is a nulled reference).

2026-10-10, from COMP-B's and GAME's hand-ins:
- C5. **GCC folding a LATER null test of the object after a member call is an accepted DIFF**: once `F(obj)`
  is `obj->F()`, the compiler treats `obj` as non-null after the call and deletes a following `if (obj)`
  in the caller. The callee dereferences the object first, so the folded test was dead; the move stands,
  the fold is quoted as the expected DIFF (and if the test is one retail lacks, it is a B6 D4 guard gone
  for free; say which).
- C6. **A port-only diagnostic printf that prints a row's ADDRESS (`%p`) is not a function-pointer site**
  (D3): the `X=%p` field and its argument are dropped from the format string (the `[SelDiag]`, tornado
  trace, `[FinTutInst]`, harness "probe skipped" lines), the row moves or the forwarder is deleted; the
  B11 DIFF confined to the printf's string / arguments is quoted as a diagnostic-string change.
