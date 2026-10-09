# Next session plan (written 2026-10-09, end of B7 — the named-prerequisite pass)

**B7 is CLOSED.** The 23 "named prerequisite" casts are 1: the prerequisites were SUPPLIED (the String /
Array release family header-inline, the ctor doors real ctors, Frustum's real ctors, the
`Set<Pair<String,String>>` member, the templated cell-BST core) and the sites typed, in 42 `behaviour:` and
25 `B7:` commits; 5 gated merges (`b7_session/GATE_TABLE.md`), two of them with a resave finding each
root-caused and closed (a resource-load-order change moving a pointer-keyed save tree -> golden RE-CUT
`994eb184…`; callers on the wrong `ExternalPtr` instantiation nulled by row 942's validation -> LX2).
The B6 residue rows are read and settled (RES: 24 items; LX: 992 / 942 / 993 / 994). PROD metric **3** = 1
named prerequisite (introsort.cpp:355, re-measured: an escape, not the inline dtor) + 2 ruled; acceptance
ONE line off (10 = 19, accepted); `worldcmp.sh` vs `oldbase` identical but for the `[PfSeed]` log wording
(row 962). Numbers: `b7_session/CLOSE_NUMBERS.md`; rule sheet `B7_RULES.md` (§7 C1-C4); audit
`b7_session/AUDIT.md`.

## 0. Scope, as the user set it (unchanged)

The DANGEROUS casts go completely; only safe casts stay. The number is
`python docs/runtime_port/endgame_b/b4_session/castmetric.py code runtime standalone dx11 engine` (3).
A raw-storage member becomes a real member only in a `behaviour:` commit that reproduces retail's constructor
AND destructor; a guard retail lacks is dropped (B6 D4), never added; a faithful crash is root-caused
upstream — and the root may be a FIXTURE (B6 C5) or a LOAD ORDER (B7 C3). Device-lost stays DEFERRED (§3).
**ENDGAME B.1 decision 7 is DECIDED** (the user, 2026-10-09): C++ exceptions where retail unwinds, error
returns where retail returns a status; SEH / longjmp go; the crash / hang handlers stay Windows-only diagnostics.

## 1. Where it stands

| | |
|---|---|
| main | the B7 close commit (after `455db4163`, Merge b7-RT, the last gated code merge, and CLOSE-A / CLOSE-B) |
| metric | 26 -> **3**: code 1, runtime 2, standalone 0, engine 0, dx11 0 (`LEFT_FREE.md` §C) |
| invariants | new game 5683 / 0, tutorial 5773 / 12, typetable `b983f144`, resave 5/5 on every merge against the golden `994eb184a8e88db9372b5b9cd13a9ed9` (RE-CUT once, row 84), 0 new warnings, unit **920 / 0** |
| acceptance | 1 line off (10 = 19, accepted); 01 = 3 of target 3; 08 = 0 (the 13 serializer cores listed in LEFT_FREE §A) |
| known_port_issues | 35 B7 status cells; new rows 1000, 1020, 1040, 1041, 1060-1065 |
| close checks | `worldcmp.sh` (118 lines, one log-wording line), `stepclose.sh` clean at `a2136019b` (sa-stress 139 fps), the final gate on the close tree (CLOSE_NUMBERS §4), audit 54 / 5 / 3 applied |
| worktrees | `oldbase` (`b2a5434c`) and `bisect` (the coordinator's probe worktree; remove or reuse) |

## 2. The next step: the quality pass (`LEFT_FREE.md` §A, 234 + 13 free functions that are members in retail)

**What it is.** `exe_bodies.h` / `*_funcs.h` still declare 412 free functions whose retail body is a class
member (`LEFT_FREE.md` §A by namespace: Action 68, Game 76 + the 13 B7 serializer cores, …). Each becomes
the member it is in retail (`ExternalPtr<T>::Serialize`, `WorkScheduler::…`, …), declared in the class's
header with its retail address, called by name from every caller; the free declaration goes with its last
caller. No behaviour changes: `B7:`-style typing commits with B11 SAME / IDENTICAL at -O2 (the inline release
family means every -O2 object differs from an older base by inlining only — take a fresh base at the step's
kit commit). Shape: Block 0 inventory by class (one row per free function: the class, the retail mangled
name, callers), then opus blocks by namespace in worktrees, the coordinator gating as B7.

**FIRST, the two user decisions of 2026-10-09 (one `behaviour:` commit each, gated before the pass starts):**
- **Install `NaturalResourceComponent` slot 8** (row 5 / 552): the faithful OnRemove 0x140125c40 goes into its
  vtable slot (today a PORT GAP arm); routes: new game, stress (`sa-stress`), `tool_removal` — every tree cut and
  stone quarried runs it.
- **Wire `GrowthComponent::SpawnSubResource`'s callee** (entry 3): the port of 0x14009f5d0 called from the spawn
  arm after its signature is checked against 0x14011fba0's call; watch the entity census (`[TownCensus]`) on
  new game / tutorial / stress — sub-resources spread as retail from then on, so `worldcmp.sh` may legitimately
  move (attribute every line).

**Also in the step (the B7 residue, same task):**
- **Row 1040** (`Desc_LoadExternalArray`: one untyped body for two retail array instantiations 0x1400049c0 /
  0x140032090, no type test): a type per caller (ten), then the member test — the LX2 array cores
  (`Reimpl_Array_Serialize_External_EdgeMesh` / `_MaterialInstance`) are the shape.
- **Row 1041** (`IsDerivedFromOrTrust`'s five remaining callers): read each retail body, spell the member
  `IsDerivedFrom`, delete the helper.
- **The introsort escape** (`introsort.cpp:355`): a typed empty `Array<EmitterInstance>` at the 0x1800dfd10
  caller keeps its free because its address escapes into the out-of-line `Array16Assign`; once
  `Array16Assign` is the `Array<T>::operator=` member (this pass), re-measure.
- Rows 1000 (AddEventCore's four tests and its release point), 1020 (NewGameDialog's `_padADC` store),
  1060-1065 (RES's six: EvictRoster's progress break, the `liveSeatShuffle` flag over a retail call,
  ProfessionUI's set-up tests, Expire's ring test, ResolveExternalByName_Impl's factory test and generation
  order, RegisterStorageLocation's return), row 310's four unread files (PART FIXED).
- **Unprobed routes** (audit Q2 / Q3): citizen click-picking (row 252) and closing an entity panel (994 G33)
  changed and no gated route exercises them; the user's by-hand check, or a route added to `uiroutes.sh`.
- **The probe technique** (B7_RULES C4) is how to check ExternalPtr references a new route exercises.
- Row 34 (the launcher dialog): USER DECISION 2026-10-09 — port it with the PORT GAPs (§4), not here.

## 3. Device-lost (DEFERRED by the user — read before touching it)

Unchanged from B5: `b5_session/DEVLOST_FINDING.md`, known_port_issues row 890. The leading hypothesis is
the exe PORT GAP pair `GameUpdateLoop::OnVideoError / OnVideoRestart` (0x140095970 / 0x140095930). Do it
only when the user lifts the deferral; `devlost.sh` keeps `DLMAX=5`.

## 4. After that

- **ENDGAME B.1 decision 7 (exceptions), DECIDED** (§0): the ~25 setjmp / longjmp / SEH files become C++
  exceptions or error returns per site; `-fexceptions` everywhere; `reference_longjmp_unwind_runs_retail_cleanup`
  is the trap to read first.
- The PORT GAPs (the device-lost pair among them; **the launcher dialog, row 34, decided 2026-10-09: the proc
  0x1800a1ae0 ported, the dialog template 0x66 re-created as the exe's own resource, a menu route for the gate**),
  then device-lost.
- The retail quirks copied as-is (docs/retail_bugs.md): a list to revisit only if the user wants a "fixed"
  build flavour someday — NOT part of ENDGAME B.

## 5. Working rules that held (do not relax) and what B7 added

- Agents never run the game; builds `-j2`, at most four building agents; the coordinator alone gates; the
  gate builds the MERGED tree before the merge commit exists; commit the gate table BEFORE `mergegate.sh`;
  a fresh worktree is reset to a NAMED commit; never `git add` the session directory wholesale.
- NEW (C3): **a resave DIFF with identical rows and payload size is read as BYTES** (python over the two
  images: the differing ranges), bisected commit by commit in a scratch worktree WITH THE GOLDEN COPIED IN
  (the `.bin` is git-ignored; an empty worktree makes its own golden and reports NEW-GOLDEN); two bisect
  scripts never share one worktree; re-cut only once the container and its ordering rule are named.
- NEW (C4): **a type-validation change that SHRINKS the payload is a nulled reference** until shown
  otherwise; the probe (an `fprintf` on the failed `IsDerivedFrom` in `SerializeExternalRef`'s load lambda,
  one resave run, grouped by wanted type id) names the wrong-instantiation families directly; a caller's
  instantiation is proven by the retail body's `MOV EDX,[cell]` before `CALL [0x140152a98]`, never by the
  field's C++ type or the resource's name — and callers that pass a serializer as a FUNCTION POINTER are
  callers too (the two CitizenBehavior bucket-tree keys hid from a grep of direct calls).
- NEW: **a `-Wuninitialized` GCC surfaces after an inlining change over a field retail leaves unwritten** is
  silenced by a pragma (member-scoped where GCC honours it, file-scoped where it does not), never by a store.
- NEW: **retail's placement-new shape is `p ? new (p) T : nullptr`**, the store of the result unguarded
  (audit F2: a dropped `if (!p) return` and a dropped ctor test are not the same thing).
- Kill an orphaned exe BY PID (`taskkill //F //PID`), never `/IM`; the lock's `exe_pids=` line names it.
