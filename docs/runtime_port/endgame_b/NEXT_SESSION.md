# Next session plan (written 2026-09-19, at the B.3.5 close)

State and counts live in `../STATUS.md`; this file is only the plan. Delete it (or replace
it) when the session it describes has started.

## 0. Sync (5 minutes)

- `git log --oneline -8`, `git worktree list`, `git branch --no-merged main` (expect none).
- Step 5 ran about 25 worktree agents. Prune the merged worktrees under `.claude/worktrees/`
  (check each is clean; `git worktree remove` without `--force`, `git branch -d`): they slow
  every unscoped grep and a stale one is where an agent's "zero hits" comes from. Step 5's
  branches are all `b35-*` and all merged. Leftover branches that are NOT this plan's:
  `endgame-a3`, `wave29/base`, `defects-open-5`.
- Greps over `standalone/` or `tests/` exclude `standalone/build*/` and `tests/unit/build/`.
- Open Ghidra (all four programs) BEFORE launching an agent that needs it. `switch_program`
  is GLOBAL state: every agent asserts `get_current_program_info` before each read batch
  (retail `Runtime-x64.dll` 15290 functions; the profile DLL, 16002, shares the image base and
  is never a source of offsets; `Application-x64.exe` 11473; `VideoDX11-x64.dll` 576) and
  switches BACK. Ghidra scripts must be JAVA (no Python provider in this install); step 5's
  dump scripts are in `b35_session/ghidra/` and are the model for a UI dump.

## 1. Carry-overs (each its own gated change; none blocks step 6)

Unchanged from before step 5 (details in the B.3.4 block of `STATUS.md` and `B34B_RULES.md`):
1. The IAT-cached `fn_InternalBasePtr_Ctor` / `_DeletePtr` callers in ten files.
2. One `?_typeInfo@` record per name (`Base`, `External`, `ExternalList`, `CompiledExternal`,
   `Internal`): owner task, needs `rt_data_mint.cpp` + `rt_mint_gen.py` and a live type-table
   comparison.
3. `ExternalList::Find`'s second parameter is `Object::Type` by value, not `uint32_t`.
4. A menu-driver route that opens Options. NOW MORE USEFUL: step 5 converted
   `Dialog/optionsdialog.cpp`'s twenty `Video::Device` / `Configuration` sites with build
   coverage only (display-mode validation, `Configuration::Save`).
5. `System::MemoryAllocator` — deferred on purpose (~1,146 sites). New related hazard found in
   step 5, left byte-identical: `code/Game/Work/gatherrawmaterials.cpp:~232` and
   `foresterwork.cpp` keep a fallback `ResolveAddr(Module::Exe, 0x1401527e8)` labelled
   `iatMemAlloc`; that exe slot is `?_rng@Particle@Graphics@@` (a DATA import). It only runs if
   the `MemAlloc` export fails to resolve. Fix it with the allocator work.
6. The eleven `CUTOVER_REQUIRED` System statics + step 5's ten Video/Graphics statics
   (`B35_RULES.md` section 9): `MintStorageCell` redirect owed at the class-static conversion,
   the END of B.3.
7. `System::Configuration::CurrentBuildId` (0x1800c4c60) stays a free door.
8. The 125 call sites through the twelve `ExternalPtr` exe-side doors. Step 5 adds:
   `Geometry.h`'s `Graphics::ExternalPtrHandle` alias serves FIVE element types, so retyping it
   to `Object::ExternalPtr<T>` means about 30 uses across six headers — do it with this work.
   Settled in step 5 from retail prologues (ten getters read): an `ExternalPtr<T>` RETURN is a
   hidden-retslot return in retail; an `ExternalPtr<T>` ARGUMENT is a bare pointer in a register
   and its pointer ctor does no AddRef.
9. Small: eight direction reads with an unproved receiver (`d6ee7bfc`);
   `standalone/res_load_lock.{h,cpp}` live-or-dead; `Time::AsSeconds` / the `Time` ctor reach
   `System::Timer` by name although the members are static now.

New from step 5 (details in `STATUS.md`'s B.3.5 block, `B35_RULES.md` and its dated corrections):
10. **THE DEVICE-LOST / LOADER RACE NEEDS A DECISION, NOT A PATCH.**
    `endgame_b/DEVICE_LOST_LOADER_RACE.md`: a device-lost that lands while the loader thread is
    building the world kills the loader thread (four stack families, incl. the
    `Factory::IsDerivedFrom` near-null one first seen in step 5 and the `GpuBuffer_Create` null
    `d3dDevice`). PRE-EXISTING (pre-step exe 5/11, step-5 exe 1/11 with
    `BANISHED_FORCE_DEVICE_LOST=400`; clean at `=2000`) and RETAIL HAS THE SAME WINDOW: all seven
    `Device::Lock` sites are reproduced, retail releases the lock right after `Present` and runs
    the whole recovery unlocked, `Create` and the load arm of `Serialize` take no lock, and
    `LoaderLoop::OnVideoError` is a bare RET. Any fix (hold the lock across recovery, park the
    loader, refuse to inject mid-build) DIVERGES from retail: the user decides. Cheap next move
    that is not a divergence: print `Device::_singleton`, `activeDeviceData`, `d3dDevice`, lock
    owner tid (+0x88) and depth (+0x8C) in the crash dump (a `standalone/` change). One step-5
    run died with that stack at frame 357, BEFORE its frame-400 injection: still unexplained,
    and a plain-route recurrence is a FINDING (the crash dump shows a live allocator and intact
    free lists, so `B34B_RULES` 6.3's W2 / W2' are excluded for these stacks).
11. The concrete dx11 `*Data` classes and the 84-slot seam (`Dx11SeamInstall`) are a declared
    leave (RULE B5-8): converting them means re-deriving the seam's completeness guarantee that
    device-lost recovery dispatches through. dx11's import table is down to 38 live rows:
    allocator doors, ctor rows, the two singletons, and four `Application` rows that wait for
    `Application` to become a class (step 7).
12. Sixteen Video/Graphics ctor doors in `engine_funcs` and every `??0/??1/??2/??3` door stay
    by design (B4b-2); mint rows, `RT_DELDTOR` rows and `_typeInfo` rows retire at the end of
    B.3. 425 (a ceiling) Video/Graphics members are declared and defined nowhere: they are
    UNPORTED retail members nobody calls — not debt of step 5; a caller that appears is a link
    error, which is the point.
13. Left with a reason in step 5 and worth a small pass each: `Camera.h`'s eight wrong POD
    field names (PODs have users in eight files; the classes carry the right names);
    `standalone/scene_watch.{h,cpp}`'s ~60 raw-offset reads of Scene/Geometry/Renderer/Camera;
    `standalone/boot_window.cpp`'s `Configuration+0x42` write (deliberately header-free);
    `InterpolatedStateT` vs `InterpolatedState<T>` (same layout, inheritance NOT proved);
    the 11 hand-inlined slot calls left in `runtime/src/{video,graphics}` and 18 in `code/Game`
    (unproved receivers); the nine int64 `ReaderWriter` slot calls (five left after step 5).
14. From before, still open: the quit-during-splash exposure, `MenuDLT` `Stub_Shutdown` skipping
    the rest of retail's Shutdown (a leak; belongs with step 7's Audio/loop work), `e353fb15`'s
    Mods-route timing — watch the Mods routes.

## 2. B.3 step 6 — `UI`

Same shape that has now worked seven times: read-only inventory -> committed rule sheet
`B36_RULES.md` (+ `B36_SLOTS.md`, `B36_FILEMAP.md`) -> ONE owner for the question pass ->
class blocks by DISJOINT FILE SET on worktrees -> coordinator gates each merge -> fragment
passes -> caller batches by directory (two rounds) -> one closing owner. The session kit is
`b35_session/` (README there): `BLOCK_BRIEF.md`, `CALLER_BATCH_BRIEF.md`, the Ghidra dump
scripts, `CLOSE_NUMBERS.md` as the measurement template; `b34_session/` still holds `gate.sh`,
`reftable_diff.py`, `mods_route.sh`, `warn_ab2.sh` and the sweep scripts.

### 2a. What is already measured (`UI_INVENTORY.md` — a MAP, NOT EVIDENCE; nothing in it was verified by the coordinator)
- 68 polymorphic `UI::*` classes, ALL with a `_typeInfo` row: 28 `*Description` (incl. the base
  `ElementDescription`), 28 `*Element` (incl. `Element`), 6 Dialog-family (`Dialog`,
  `DialogController`, `DialogControllerConfig`, `DialogInstance`, `ComboDialog`,
  `ToolTipDialog`), 6 other (`CharacterSet`, `Cursor`, `Font`, `SpriteCapture`, `SpriteSheet`,
  `StringTable`); 67 `RT_DELDTOR` rows (one short, unreconciled); 4 non-polymorphic roots
  (`UserInterface`, `SpriteContext`, `Sprite`, `UIString`). `ENDGAME.md`'s "53" reconciles
  with neither 56 nor 68: settle the class list FIRST.
- 1,663 registrations in `runtime/src/ui` (105 files, 46k lines): 1.6x step 5. The census's
  largest namespace (1,625 DLL rows).
- Callers: 118 `engine_funcs` globals bound to a `@UI@@` export (names are NOT reliably
  `fn_UI_*`) with 1,434 call sites in `code/Game` (step 5 had 61); 155 `ResolveExport`; 184
  `RT_EXPORT_ONCE` (all inside `runtime/src/ui`); 205 exe import slots resolve to a UI export;
  417 hand-inlined `(*(void***)p)[N]` calls across 39 `code/Game/Dialog` files, `tradeui.cpp`
  (150) and `townhallui.cpp` (95) alone 59% of it.
- Type headers use FOUR namespace spellings (`RtUI`, `namespace UI`, bare, and a
  `StringTableOffsets`-only shape); `UIString` has NO namespace wrapper at all.

### 2b. Block 0 — the question pass (ONE opus owner, no ported code touched, into `B36_RULES.md`)
1. The class list and hierarchy from the RETAIL DLL byte-wise (the `_ParentTypeName` literals
   settle it: all 68 classes have one — step 5 had 18 of 39), slot lists (copy from the mint
   table, which is generated from retail and was right every time in step 5), `sizeof` from each
   slot-0 deleting dtor's `MOV EDX,imm`, who constructs and stamps what.
2. **The second POD tree**: `engine/types/ui/{DialogController,DialogInstance,Dialogs,Elements,
   Panels}.h` + `engine/types/{Descriptions,DialogLoaderLoopT,DialogLoopT}.h` (~1,887 lines)
   mirror `runtime/types/ui/`. Decide ONE definition per class and which tree owns it (the
   duplicate-POD hazard; step 5 collapsed every dx11 view onto the runtime header).
3. **The exe-side subclasses**: `DialogInstance` is the base of 22 exe dialog controllers;
   `code/Game/Dialog/{dialog_controller_vtable,mod_dialog_vtables}.cpp` build vtables by an idiom
   a hex-literal grep does not see; `tool_base_toolui.cpp:136-142` canonicalises `DialogInstance`'s
   vtable 0x180156d70. Enumerate them all: they decide which bases can be abstract and where the
   QUALIFIED-CALL rule bites (see 2d).
4. Multiple inheritance / secondary vptrs (the `ControllerComponent` pattern starts in UI
   controllers), overloaded virtuals, by-value holdings (`String`, `Color`, `Rectangle`, `Array`,
   `ExternalPtr` members; element structs embedded in dialog structs), the ~12 hidden-retslot
   exports (inventory: none through a vtable slot — CONFIRM), threading (the loader constructs
   UI resources while the menu draws).
5. File ownership: which of the 105 files serve more than one class (the wave/lane/batch files).

### 2c. Proposed blocks after block 0 (inventory section 6 — confirm disjointness first)
1. `Element` + `ElementDescription` bases (serial, one owner; dominates caller traffic).
2. `Dialog` / `DialogInstance` / `DialogController` / `DialogControllerConfig` / `ComboDialog` /
   `ToolTipDialog` (serial; the 22-subclass constraint).
3. The 27 leaf Description/Element pairs — FULLY PARALLEL, the cleanest fan-out B has offered;
   batch them 4-5 pairs per agent by file set.
4. `Sprite` / `SpriteSheet` / `SpriteCapture` / `SpriteContext` / `Font` / `CharacterSet` /
   `StringTable` / `UIString` / `Cursor`.
5. `UserInterface` + the owner files (`rt_factories_ui*`, `rt_ui_port.cpp`, typeinfo accessors).
6. Fragment pass, then caller batches by directory: `code/Game/Dialog` split in three or four
   (it holds most of the 1,434 sites), `tradeui.cpp` + `townhallui.cpp` as their own batch, then
   the rest of `code/`, `engine` + `standalone`, `runtime/src` outside `ui`; a second round; one
   closing owner (`engine_funcs` zero-caller sweep by BINDING, dead caches and guards, numbers).

### 2d. Standing rules every prompt carries (B.3.3/B.3.4's lists in `B34B_RULES.md` and the old plan in git history `bd483075` still hold; these are step 5's)
Class shape
- **A by-value return through a VIRTUAL is declared `T* M(T* ret, args…)` in retail order.**
  GCC's ms_abi passes the hidden return pointer BEFORE `this`; MSVC passes `this` first. A C++
  by-value virtual writes the result THROUGH `this` when a retail-shaped caller (a minted row, a
  hand-built exe vtable) calls it. Non-virtual by-value members are fine behind a door with the
  explicit `(this, ret)` signature that placement-news the prvalue.
- **A polymorphic class's slot 0 (`DeletingDtor`) is declared `= 0`** while its body is a
  `deleting_dtor.cpp` row: a non-pure declaration puts an undefined symbol in GCC's vtable and
  the exe does not link (seen in four blocks). So nearly every class is ABSTRACT, which is fine:
  nothing constructs them as C++ objects. A class is concrete only when something holds it BY
  VALUE (then protected `= default` ctor + `friend` the holder + trivially-copyable assert).
- Ctors and dtors that stamp a MINTED vtable stay doors; a member dtor chain that would re-run
  retail's out-of-line teardown (array frees, `PlatformDestruct`) is defined EMPTY or not at all.
  A nested value type whose `Array<T>` helper destroys elements through an open-coded copy of
  its dtor must NOT get real special members (`ShaderProgram::Parameter`: double release).
- `Construct` is introduced PER BRANCH (slot 8 in Video/Graphics; `ParticleSystem` had it at 9
  behind an inherited pure slot): read the slot list, never assume the index. `virtual`, not
  `override`, on the branch's first class.
- Type-name literals are copied from the binary per class, never derived from the C++ name:
  retail qualifies a parent with its namespace only when it lives in a DIFFERENT namespace
  (`"Particle"` but `"Video::ImageBuffer"`), and the CRC of the literal is what saves key on.
- Declare EVERY exported member up front (unported ones `// not ported`, defined nowhere): other
  blocks and the caller batches never edit your header, and an unported member that gains a
  caller is a link error.
- A converted call INSIDE a slot body of a hand-built exe-phase subclass must be QUALIFIED
  (`->Graphics::Geometry::UpdateAttached()`): the base member is virtual and an unqualified call
  dispatches back into the same function (seven sites in step 5; UI's 22 dialog controllers are
  exactly this shape).
- One definition per layout: where class and POD are field-identical the POD becomes an ALIAS of
  the class (step 5 did this for `Context`, `Device`, the buffers) so files in other blocks
  compile untouched. Two same-size structs with different members under one name is an ODR
  defect that stays latent until one TU includes both (`InterpolatedTransform`).
- Header comments are not evidence: step 5 found 23 wrong retail addresses in one header, a
  22-parameter member that has 25, a parameter pair named backwards, a field map with two names
  swapped, and "NOT PORTED" notes on members ported for months. Tokenise the mangled name; read
  the export directory (`tools/pe_exports.py`).
Finding callers — by BINDING; now TEN spellings
- The eight in `B34B_RULES`, plus 9: a mangled name held as a `const char*` in an initialised
  table resolved by a loop (dx11's import struct); plus 10: **an exe dispatch slot**
  (`ResolveAddr(Module::Exe, 0x14015…)` + deref, often through a file-local `RSlot` / `Slot` /
  `ReadSlot` / `ResolveSlot` / `resolveIAT` helper). Spelling 10 DOMINATES `code/Game` and is
  invisible to name-anchored sweeps: map every `0x140…` literal through the exe's PE import table
  (the source `tools/dispatch_slot_map.py` uses). It found 361 hits / 29 symbols that the
  `ResolveExport` sweep had not, and a file with 18 bindings and no literal or mangled string.
- `engine_funcs` globals are found by their INITIALISER, never their name: of 76 that bound a
  Video/Graphics symbol ~20 were spelled `fn_Video_*`/`fn_Graphics_*`, and 3 were bound only BY
  ADDRESS — one of those kept four live callers through three sweeps.
- The inventory's per-directory `ResolveExport` counts for `engine/` and `standalone/` are
  mostly registry rows and global initialisers, not callers (74 -> 0 and 36 -> 4 in step 5).
- A per-door "converted" tally can read complete when it is not (a door called three times per
  file, first site converted): finish with a line-by-line re-sweep of the final tree.
Process
- Blocks own FILES, not classes. A body of someone else's class in your file stays a door and
  goes on the coordinator's fragment list; a fragment pass per namespace side follows the class
  blocks, then callers. Two caller rounds: round 1 leaves every site whose member is "declared
  but not defined", the fragment passes define them, round 2 converts.
- Conflicts are always "two blocks appended at the same spot" (`tests/unit/sources.mk`, a shared
  include list): keep both sides. Better: tell blocks NOT to touch `sources.mk` (`UNIT_SRCS` is a
  wildcard; the file only carries comments for test-only blocks).
- More than three agents that run the game starve the game dir: caller batches are static-gate
  only; class blocks run one route at the end. The coordinator gates every merge.
- The gate's warning diff flags a standing warning whose TEXT changed (a type renamed
  `RtVideo::Context` -> `Video::Context` in an unused-function signature): compare, then add the
  respelling to the baseline.
- A converted file that reaches `Volume.h` after `<windows.h>` no longer breaks (`Volume.h` uses
  elaborated specifiers since step 5); ~40 files still carry a "runtime headers first" ordering
  comment — harmless.
- `rt_mint_gen.py --check` rewrites `standalone/rt_export_addrs.inc` line endings: `git checkout
  --` it before merging or committing. A bash heredoc eats one level of backslashes even with a
  quoted delimiter: write scripts and commit messages as FILES.
- An agent that ends a verification run by killing the process loses the summary lines
  (`[RtResolve]`, `[ExtLoad]`): let the pause-menu quit finish.

### 2e. The gate for every merge (coordinator only)
Unchanged: debug + release + perf build exit 0 · new-warning set diff vs a clean-rebuild
baseline · `make unit` · new game + pause-menu quit · tutorial · Load Game dialog · resave gate
PASS x3 (x5 for anything on a serialize or load path) · pointer-free type table vs baseline ·
`[RtResolve] 0 misses`, no `[RtMint] TRAP`, 0 GATE WARN, new-game `calls=5686`, tutorial
`calls=5776 openFileFail=15` (the Load Game route's count floats 4760-4789: not pinned) ·
`dispatch_slot_map.py --check` 0 · cutover manifest gate · `rt_mint_gen --check` · `git diff
--stat` against the agent's file set. At a step close also: the Mods route with `.pkm` mounted,
one functional `make sa-stress` (record, never chase), one true release compile from a clean obj
dir FOLLOWED by `make clean`, a clean-rebuild warning diff. For UI add: the front-end menu route,
the Load Game dialog, the Mods dialog, the tutorial (help/tutorial UI) and — owed since B.3.3 —
an Options route; a UI step with no dialog route driven is not gated.
