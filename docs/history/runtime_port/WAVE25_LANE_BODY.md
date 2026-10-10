# WAVE 25 — LANE BODY

Branch `wave25/body`, cut from `wave25/base` at `fc56353c`. Private worktree.
Subject: the last real bodies in the named-symbol gap (kickoff §2, "BODY — the last nine").

**Headline: two bodies ported (`?Frame@MainLoop@Engine@@`, `??1PackageFile@IO@@`), and
the wave's triage rule is now WRONG IN ONE DIRECTION.** Wave 24 paid for
*"every DEPENDENCY-shaped blocker was false or overstated; every ORACLE-shaped
blocker survived"*. The first half held again — **4 of 4**. The second half
**broke**: `??1PackageFile@`'s "it has no oracle", the blocker Wave 24 named
*"the strongest-held of the nine, and correctly scoped"*, is refuted by a working
differential oracle against the shipped export. §3.

**A second finding is worth more than either port: this file's own first draft
dropped `MainLoop::Draw`'s float argument, and reading the CALLEE'S MANGLED
SIGNATURE is what caught it — not reading more instructions.** §4.

*(Rule: every census figure here is BRANCH-LOCAL, measured in this isolated
worktree. The integrator must re-measure on the merged tip.)*

---

## 0. A PROCESS NOTE THE INTEGRATOR NEEDS FIRST

**This worktree's HEAD was not `wave25/base`.** It was checked out at
`1afa558e`, a wave-15-era commit, and its `CLAUDE.md` was correspondingly stale
(it still described the UI namespace as "the next wave", with no mention of
Waves 16–24). `git checkout -b wave25/body` therefore cut the lane branch from
the **wrong tip**, silently — the branch was created, the build would have
worked, and every measurement would have been against a tree ten waves old.

Caught by running `tools/runtime_census.py` before anything else and finding it
did not exist at that path, then `ls docs/runtime_port/` and seeing no
`WAVE25_KICKOFF.md`. Fixed with `git reset --hard wave25/base`.

This is `reference_worktree_reassignment_crosses_lanes.md`'s neighbour: not a
worktree pointed at ANOTHER lane, but one pointed at a stale COMMIT.
**The cheap standing check is: the kickoff document for your own wave must
exist in your worktree before you read anything else.** If it does not, your
HEAD is wrong.

---

## 1. RE-DERIVATION — THE KICKOFF BOARD IS EXACT

```
cd <worktree> && <venv>/python.exe tools/runtime_census.py --unported
```

At `fc56353c`, before a line was written, every figure in kickoff §0 reproduced
exactly: `TOTAL 4632 / 1323 called / 1171 ported / 9 detour / 88.5%`,
`=> 16 of 152 gap rows (11%) are a function to reimplement`, and all thirteen
namespace gap rows. Nothing in the kickoff's board was stale for this lane.

### 1.1 THE FOURTH GRAPHICS BODY ROW — RE-DERIVED, AS INSTRUCTED

The brief said *"Graphics (4th BODY row — re-derive; do not assume my list is
complete)"*. It is **`?AddDrawItems@MeshInstance@Graphics@@UEBAX…@Z`**, and it
is **not an unclaimed body**: the census prints it carrying
`UNPROMOTABLE(HARMFUL)` with `expires: NONE, EVER`, adjudicated in
`WAVE21_LANE_GFXBODY.md` row 11 and reaffirmed by Wave 24 GFXMAT §3.6.
Registering it makes `standalone/scene_watch.cpp:401`'s identity-token compare
fail for every object.

So Graphics' `BODY 4` is **3 portable bodies + 1 adjudicated row**, and a lane
scoping "four Graphics bodies" would be scoping one row that must not move.

---

## 2. THE BLOCKER TABLE — THE DELIVERABLE IN ITS OWN RIGHT

Every "survived?" cell is a MEASUREMENT taken in this worktree, with the command
beside it, not a reading of the sentence.

| # | Row | Stated blocker (source) | Shape | Survived re-derivation? |
|---|---|---|---|---|
| 1 | `?BuildParameters@MaterialInstance@` | "refused **on size**, not on any oracle-shaped blocker" — GFXMAT §3.5 | **SIZE** | **YES.** 1422 B re-confirmed independently from `.pdata` (`[0x1801033e0,0x18010396e)`, 1 fragment). |
| 2 | `?Draw@Renderer@` | same sentence; "zero rel32 callers, needs a registration not a detour" | **SIZE** | **YES.** 921 B — and `.pdata`'s **entry fragment is 408 B with `chained=False`**; union-forward over 3 fragments gives 921. |
| 3 | `?Update@AnimationState@` | "Sized out, not argued out" — GFXMAT §3.3 | **SIZE** | **YES.** 1062 B — entry fragment **81 B**, `chained=False`, **5 fragments**. A **13.1x** understatement, the worst in this set. |
| 4 | `?AddDrawItems@MeshInstance@` | `UNPROMOTABLE(HARMFUL)`, scene_watch identity token | **DEPENDENCY (harmful-if-ported)** | **YES.** Tag still printed by the census on this tip; `scene_watch.cpp` still reads the address. Not a body gap at all (§1.1). |
| 5 | `?SetupFileSystem@Modification@` | callee wall (already corrected by IOPORT: 13/16 ported); real blockers **size + broken mods route + blast radius** | **DEPENDENCY (dead) + ORACLE** | Dependency half **already dead**. Oracle half **YES** — the verification route is the mods menu, and `CLAUDE.md` documents it crashing at `ModDialog_ToggleMod_Impl+0x43`. **New measurement: the body is 2617 BYTES**, a figure stated nowhere before and the largest in this set. |
| 6 | `??1PackageFile@IO@@` | (a) "**(5 unported callees)**" — `package_file.cpp:925`; (b) "**no oracle**… a fabricated subject is a fake" — IOPORT §2.4 / SMALLBODY | **DEPENDENCY + ORACLE** | **BOTH FALSE. §3.** (a) 7 callees, **6 ported and registered**; and an unported callee was never a blocker, because `ResolveAddr` falls through. (b) refuted by a working oracle. **PORTED.** |
| 7 | `?Initialize@GoalList@` | "Blocked on `Object::Factory`'s lookup + `CreateNewObject` + `Cast<>`" — ACHIEVE R1, flagged OVERSTATED by SMALLBODY | **DEPENDENCY** | **OVERSTATED, CONFIRMED — and the last leg checked.** The body inlines a typeId BST find on `0x1801fc6e8` = `?_singleton@Factory@Object@@`, which `object/type_system.cpp:76` already models, plus an is-a chain walk. **The one genuinely open door is `0x180009d70`** (258 B, `Array<8>::SetSize`-shaped), which appears **nowhere** in `runtime/ code/ standalone/ engine/`. An open door is not a blocker. **NOT PORTED — budget, not blocked.** |
| 8 | `?Frame@MainLoop@Engine@@` | "NOT ported (683 bytes, its own lane)" — `mainloop_run.cpp:88,332` | **SIZE NOTE, NO BLOCKER** | **CONFIRMED A THIRD TIME: there is none.** And 683 was understated — entry fragment **123 B `chained=False`**, union-forward **684 B over 3 fragments**, a 5.6x gap. **PORTED.** |
| 9 | `?Update@Scene@Audio@` | "a replay that draws twice from `System::Random` diverges by construction" — `audio.cpp:17` | **ORACLE** | **YES.** Not attempted. New measurement: **1816 B**. |
| 10 | `0x180028760` (Array\<Mod\> introsort) | (a) "`introsort.cpp` … **does not** cover it"; (b) "opens **TWO** unregistered doors, `0x180028fc0` and `0x1800c98a0` (String::Compare)"; (c) "needs its own replayed-permutation probe on a tie-heavy corpus" — `modification_refresh.cpp:77`, carried forward UN-RE-DERIVED by INTERNAL8 | **DEPENDENCY + ORACLE** | (a) **RE-DERIVED TRUE** — `introsort.cpp` registers only `0x1800264a0`, `0x180050d30`, `0x180051650`. (b) **HALF FALSE** — `0x1800c98a0` is `?StrCompare@String@System@@SAHPEBD0@Z`, **ported and registered by name** at `string.cpp:1343`. **One open door, not two.** (c) **YES**, and it is the real blocker. |

### 2.1 THE SCORE, AND WHY IT MATTERS MORE THAN EITHER PORT

Blockers actually met and re-derived, counted by shape:

```
DEPENDENCY-shaped   4 met   4 false or overstated   0 survived
ORACLE-shaped       3 met   1 REFUTED               2 survived
SIZE-shaped         4 met   4 survived (and 3 of the 4 were UNDERSTATED)
```

**Wave 24's rule is confirmed in its first half and falsified in its second.**
The correct statement is now:

> A dependency claim decays with every wave and has not survived a re-derivation
> in two waves. An oracle claim is *usually* durable but is **not** self-proving:
> the PackageFile one was an objection to ONE construction ("a fabricated
> subject is a fake") that got recorded as an objection to the CATEGORY. **Ask
> which construction an oracle claim rules out, not whether it rules out
> oracles.**

The SIZE row is new and is the quieter finding: **3 of 4 size-shaped blockers
were understated, one by 13x**, because everything in this tree sizes from
`.pdata` and `.pdata` bites backwards — the ENTRY fragment reads
`chained=False`, so a chained function looks complete. Understating makes work
look CHEAP, so a size refusal built on it is refusing the wrong thing.

**Re-derive the whole table:**
```
objdump -d -M intel --start-address=<lo> --stop-address=<hi> "C:/GOG Games/Banished/Runtime-x64.dll"
# extents: walk .pdata, union RUNTIME_FUNCTION entries whose start == previous end
# names/ordinals: pefile DIRECTORY_ENTRY_EXPORT, looked up BY ADDRESS
# door state: grep -rn <addr> runtime/ code/ standalone/ engine/ dx11/ | grep RegisterRuntime
```

---

## 3. `??1PackageFile@IO@@` — REFUTING AN ORACLE-SHAPED BLOCKER

**PROBE SHAPE: SYNTHESISED ORACLE.** Named before starting. Why not the others:
an *input sweep* needs purity and this mutates an object and the heap; a *shadow
site* alone **cannot work here, and that is the whole difficulty of the row** —
the two arms would share the same `String` blocks and the second run would
double-free; a *write-path replay* has no stream; *two-sided disassembly* was
the fallback and was not needed.

The blocker said *"a fabricated subject is a fake rather than a synthetic one"*.
That is true of a hand-built PackageFile and the objection is a good one. It is
not true of this subject, because **every part of it is produced by shipped
code**:

1. two 0xF0 arenas prefilled `0xCD`;
2. **the DLL's own `??0PackageFile@IO@@QEAA@XZ`** on both — retail's starting
   state, identical on both sides;
3. five `System::String` slots filled with **separate real blocks from the real
   allocator**, refcount 1 on +0x60/+0x50/+0x40 and refcount **2** on
   +0x58/+0x48. *Separate* blocks are what make two destructions safe; the
   *mixed* refcounts are what make the comparison mean anything — a population
   where every slot takes the same arm verifies one arm and reports as if it
   verified the function (`reference_uniform_population_is_not_a_verification.md`);
4. **`RtOriginal<DtorFn>("??1PackageFile@IO@@UEAA@XZ")`** on arena B. This is a
   real consultation of the DLL, which is what earns the `[ORACLE]` label.
   `ResolveAddr(Module::Runtime, …)` would **not** do — it checks the port
   registry first and would compare the port with itself
   (`reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md`);
5. our body on arena A — **the shipping body**, reached through the same
   function the registered entry point calls, not a retyped copy;
6. compare all 0xF0 bytes **except the three freeing slots**, which hold
   `data - 8` for two different heap blocks and MUST differ. Comparing them
   would fail 100% of calls and read like a catastrophic port
   (`reference_oracle_must_not_compare_unwritten_bytes.md`). They are replaced
   by a **stronger predicate row** — "each freeing slot holds its own block
   minus 8, on both sides".

### 3.1 CONTROLS, EACH SCORED ON ITS OWN ROW

Never unioned (`reference_union_control_hides_a_blind_screen.md`). Each corrupts
the **shipping body** via a parameter, not a copy.

| cell | what it corrupts | required | why that cell exists |
|---|---|---|---|
| null | nothing | agree | |
| C1 | release the Strings **ascending** | **ABSTAIN (agree)** | **EFFECTIVENESS CONTROL.** The object bytes cannot see call ORDER. Its green is the honest report of a real blind spot, not a pass. |
| C2 | null the slot on the **freeing** arm too | CAUGHT | the one-character difference the two arms exist to prevent |
| C3 | skip the +0x58 release | CAUGHT | |
| C4 | skip the vtable stamp | CAUGHT | |
| C5 | skip the `~Array` at +0x20 | CAUGHT | |

C1 is the cell that makes this instrument honest, and it is modelled on Wave 24
GFXMAT's control 6: a control that *must abstain* is the only way to state a
blind spot as a measurement rather than as a caveat.

### 3.2 WHAT THIS ORACLE DOES **NOT** ESTABLISH

* It **cannot see the ORDER** of the seven callee invocations. C1 is the
  standing proof of that, not a suspicion.
* It cannot see anything the five member destructors do to memory **outside**
  the 0xF0 arena.
* **It has not been RUN.** Lane BODY gets no game-directory lock by design
  (kickoff §1d); the site is registered and labelled so lane SWEEP's armed
  `BANISHED_RT_DIFF=1` sweep exercises it. **Until SWEEP reports, the six cells
  above are a specification, not a result**, and nothing in this document should
  be read as saying they passed. The site prints its own per-cell table.
* If the DLL is absent (cutover) the oracle has **no second arm** and says so
  explicitly rather than reporting agreement.

### 3.3 WHAT THE DTOR PINNED IN THE TYPE HEADER

`FileSystem.h` modelled +0x40/+0x48/+0x50 as Strings and +0x58..+0x67 as
`_unknown`. The destructor releases **five** slots with the refcount idiom, so
**+0x58 and +0x60 are also `System::String`**. Both are added to
`PackageFileView` with `static_assert`s citing the releasing instruction, and
both are **deliberately left unnamed** (`_string58` / `_string60`): a destructor
pins a field's TYPE and says nothing about its MEANING, and no reader of either
offset has been found.

---

## 4. `?Frame@MainLoop@Engine@@` — AND THE BUG THAT ALMOST SHIPPED

684 bytes, 3 `.pdata` fragments, the fixed-timestep frame driver. It closes the
NAME door `mainloop_run.cpp` §3 opened in Wave 9 when it ported `Run` and had to
reach `Frame` by export. **It had no stated blocker of any kind** — the only
sentence in the tree is a size note, and it has read as a blocked row for
fifteen waves because that note sat where a blocker would sit.

### 4.1 THE FINDING THAT OUTLIVES THIS LANE

`MOVAPS XMM1,XMM4` at `0x1800dd35e`. **This file's first draft called it a dead
register move and dropped it**, on three facts that are all true: XMM1 had held
`secondsPerTick`; its last `MULSD` read was at `0x1800dd356`; nothing reads XMM1
again in the function.

The conclusion was wrong. `0x1800dcd40` is
**`?Draw@MainLoop@Engine@@QEAAXM@Z`, ordinal 2251** — `M` is `float`. Under the
MS x64 ABI a member function's first float parameter is **XMM1** (RCX being
`this`), so that `MOVAPS` is the argument setup for the very next `CALL`.
Dropping it compiles, links, and hands `Draw` whatever the compiler left in XMM1
— the interpolation factor every frame is rendered with — with no fault
anywhere.

This is the same defect class as
`reference_ghidra_c_output_vs_disassembly.md`'s *"drops whole arguments at a
mistyped vtable call"*, except **arrived at by hand, from correct disassembly**.
The lesson is not "read more instructions", because the instructions were read
correctly:

> **A register move with no later reader in the CALLER is an ARGUMENT until the
> CALLEE'S SIGNATURE says otherwise. Look the callee up in the export directory
> before calling any pre-CALL move dead.** One `pefile` lookup by address
> settled it.

### 4.2 THREE MORE THINGS THE DISASSEMBLY SETTLES

**(a) The post-update pass reads `_func` and `_context` from DIFFERENT ARRAYS.**
`0x1800dd2b0` takes the trip count from `[RDI+0x98]` (`_postUpdate._size`), the
FUNCTION from `[RDI+0x90]` (`_postUpdate._data`) and the CONTEXT from
`[RDI+0x80]` (`_postDraw._data`). The offsets are not inferred — `MainLoop.h`
pins +0x80 from `?AddPostDraw@` doing `ADD RCX,0x80` and +0x90 from
`?AddPostUpdate@` doing `ADD RCX,0x90`, and the two `MOV`s carry the
displacement in their bytes. Transcribed by OFFSET, with the names beside them.
**This is recorded as WHAT THE BYTES DO, not as an established bug**: both
arrays are empty on every path this tree drives, so nobody has observed it
mattering. The obvious tidy-up would have been a store — a read — that is not in
the bytes.

**(b) The accumulator clamp contains a self-loop that can hang.** `L: CMP RAX,RCX
/ JG L`, with nothing writing either operand. Unreachable with the ctor's 1/60
and 1/15. Reproduced as written, with an `asm volatile` memory barrier inside —
**not decoration**: a side-effect-free infinite loop is UB in C++ and GCC may
delete it or assume it terminates, either of which would make the port silently
unable to reproduce retail's hang.

**(c) The tail's float sum keeps a redundant round trip.** Retail computes
`alpha = a/d` then `v = T + d*alpha`, which is algebraically `T + a` and is not
that in float. Kept (`reference_moved_term_hides_where_terms_cancel.md`).

### 4.3 A HEADER QUESTION ANSWERED

`runtime/types/engine/MainLoop.h` said of `_startTicks`: *"Whether
MainLoop::Frame also rewrites it per frame was NOT checked by lane B."* **It
does** — every call, unconditionally, at `0x1800dd146`, using the previous value
as the delta base. So the field means "the tick at which the LAST FRAME was
sampled", not "the tick Run started". Corrected in the header with the four
instructions and a re-derivation command.

### 4.4 VERIFICATION — THE SITE IS `[ATTENDANCE]` AND SAYS SO

`Frame` has **no oracle**, structurally: it advances the world irreversibly
through two virtual calls and a Win32 message pump, so there is no second arm
and nothing to restore. The site is named
**`"Engine::MainLoop::Frame [ATTENDANCE]"`** so `divergences=0` cannot be read
as a pass, plus a `[SELF-CHECK]` shadow on the one post-condition the body's own
loop establishes (`_accumulator < _deltaTime` after the fixed-step loop) —
labelled as a self-check, not dressed up as a comparison against retail.

The real test is a boot, and **this lane has not run one** (no lock, by design).

---

## 5. EVERY IMPORT AND EXPORT NAME WAS READ OUT OF THE BINARY

Not one name in either port was typed from a pattern. `Frame` reaches Win32
through six of the DLL's own IAT slots; naming one wrong would compile, link and
look right:

```
0x18014b1a8  KERNEL32!QueryPerformanceCounter    0x18014b5c8  USER32!TranslateMessage
0x18014b4b8  USER32!PostQuitMessage              0x18014b5c0  USER32!DispatchMessageW
0x18014b5d0  USER32!PeekMessageW                 0x18014b418  KERNEL32!SwitchToThread
```

They are called **directly**, not through those slots — the slots are the DLL's
IAT and do not exist once the DLL is gone.

**`AEAA` CAUGHT ME ONCE, TOO.** `?FreeChannelPlatform@Device@Audio@@AEAAXH@Z` is
spelled `AEAA` — PRIVATE — and this file's first draft reached it **by address**
on exactly that reasoning, writing "it is private, so there is no name to use".
Runtime-x64.dll **exports its private members**, and this one is ordinal 2380
(`reference_aeaa_is_private_not_unexported.md`). It is now a NAME door. Had it
stayed an address door it would have been a silent null at cutover instead of an
`[RtUnresolved]` line.

---

## 6. DECLARED SURFACE GROWTH (kickoff §1f)

**I GOT THIS WRONG FIRST, BY REASONING INSTEAD OF MEASURING, AND THE ERROR WAS
IN THE FLATTERING DIRECTION.** The declaration below is the **measured** one:
census run on `wave25/base` with my work stashed, run again with it applied,
and the two `called`/`ported` sets diffed. My reasoned estimate was **+3**; the
measurement is **+5**.

```
BEFORE (wave25/base)   1323 called   1171 ported   gap 152
AFTER  (wave25/body)   1328 called   1176 ported   gap 152
                       +5            +5            unchanged
rows that LEFT the unported list:   Engine ?Frame@MainLoop@…, IO ??1PackageFile@…
rows that ENTERED it:               Engine ?Draw@MainLoop@…,  internal 0x1800269a0
```

| what | count | note |
|---|---|---|
| rows moved into `ported` | **2** | `?Frame@MainLoop@Engine@@`, `??1PackageFile@IO@@` |
| NEW open doors (called, unported) | **2** | `?Draw@MainLoop@Engine@@QEAAXM@Z` (ord 2251, **name**), `0x1800269a0` (**address** — genuinely: not in the export directory under any name, checked by address) |
| already-ported bodies newly pulled into `called` | **3** | the internal member destructors `~PackageFile` reaches by address |
| **net gap change** | **0** | 152 -> 152 |

Two corrections the measurement forced, both of which a reasoned declaration
would have published as fact:

* **`?FreeChannelPlatform@Device@Audio@@` is NOT new surface.** I declared it as
  a new door; Audio's `called` did not move, because `audio.cpp:32` already
  referenced it. `mainloop_frame.cpp` opens **one** row, not two.
* **A THIRD CATEGORY EXISTS THAT I HAD NOT ACCOUNTED FOR AT ALL:** three
  internal addresses grew `called` **and** `ported` in the same step. They are
  bodies this tree has had and registered for waves, which **no source file
  resolved by literal address**, so the census could not see them as `called`.
  Referencing them made them visible. This is the exact mirror of
  `reference_half_registered_port_reads_as_absent.md`: there, a ported body read
  as absent; here, a ported body was invisible until something called it. **It
  raises `called` and `ported` together and moves the gap by zero** — so a lane
  reading only the totals would see "+5 called" and conclude it had opened five
  doors.

`called` is not a denominator counting down to zero
(`reference_porting_can_grow_the_cutover_surface.md`), and **a surface-growth
figure that was reasoned rather than diffed is not a measurement** — the
stash/apply diff above costs two census runs and is the only thing that made any
of this visible (`reference_diff_the_set_not_the_count.md`).

---

## 7. A CORROBORATION FOR AN OPEN HOLDBACK SET

`Frame`'s 64-channel audio sweep (`0x1800dd1bd..0x1800dd202`) walks 64 fixed
0x28-byte channel records at `Audio::Device + 0`, and for each one needing
release it frees the platform voice, clears the flags, and then **calls the
function pointer stored at record +0x20** with
`(record, *(void**)(rec+0x10), *(uint32_t*)(rec+0x18))`.

That stored pointer is a completion callback, and it is the shape
`reference_audio_holdbacks_share_a_callback_root_cause.md` names as the ONE root
cause behind all six Wave 8 AUDIO holdbacks. **This body is the site that
invokes it.** The six holdbacks are still open; whoever takes them now has a
ported caller to instrument. *(Corroboration, not a discharge — the callback's
own bodies are untouched here.)*

---

## 8. WHAT THIS LANE DID **NOT** ESTABLISH

Stated plainly, because §5 of the kickoff is about discharges that do not
propagate.

1. **Neither port has been executed.** No game-directory lock, no boot, no
   frame. The build is clean and both are registered; that is all. **A build
   check cannot detect a misregistered port** (kickoff §3.6).

   **AND BOTH ARE DEFAULT-ON**, because `RtWaveEnabled` returns true for an
   unset variable and that is the convention every sibling registration in
   `rt_engine_port.cpp` and `rt_io_port.cpp` follows. The integrator should know
   what that means here: **`MainLoop::Frame` runs on every frame of every run
   from the moment this merges, and it has never executed once.** `Run` landed
   the same way in Wave 9 and its boot test came after. The two switches exist
   precisely so the A/B is one variable wide:
   `BANISHED_RT_MAINLOOPFRAME=0`, `BANISHED_RT_PKGDTOR=0`. If the integrator
   would rather the first boot of the wave not depend on an unexecuted
   every-frame body, flipping `MAINLOOPFRAME` off for the first cutover attempt
   costs nothing and returns the call to the DLL.
2. **The `??1PackageFile@` oracle's six cells are a SPECIFICATION, not a
   result.** They have never been run. Lane SWEEP's armed sweep is what turns
   them into a measurement.
3. **`Frame`'s post-update array asymmetry (§4.2a) is NOT established as a retail
   bug.** It is established as what the bytes do. Both arrays are empty on every
   observed path, so nothing has ever exercised it.
4. **The seven Graphics/IO/Audio rows in §2 are not ported.** Four were refused
   on size (three of those sizes were *understated*, so the refusals are now
   better founded, not worse), two on surviving oracle blockers, and
   `?Initialize@GoalList@` **purely on budget** — its blocker is overstated and
   its one open door (`0x180009d70`) is an open door, not a wall. **It is the
   cheapest remaining body in this set at 389 B and the next lane should take
   it.** *(And per
   `reference_cheap_wins_list_is_the_stalest_section.md`, re-derive that
   sentence before acting on it — it is the section of this document most likely
   to be stale.)*
5. **The introsort's replayed-permutation oracle was not built.** Its
   dependency half is half-false (one open door, not two) and its "not covered
   by `introsort.cpp`" negative is **re-derived TRUE**; its oracle half stands.
6. `?SetupFileSystem@Modification@` at **2617 bytes** is now the largest
   unported named body in the gap. That number is new and nobody has scoped
   against it.

---

## 9. RESIDUAL FROM LANE VTGFX — ONE STALE NOTE STRUCK, RE-DERIVED FIRST

Lane VTGFX reported `runtime/src/graphics/scene.cpp:94-99`'s bullet 3 as stale.
**Re-derived here rather than taken on its word** (the whole point of the rule),
and it is stale. The bullet ended:

> *"Stamping our own table would silently change every slot it copies, so this
> bullet is a **STANDING BLOCKER** on minting `??_7Geometry@Graphics@@6B@`, not a
> note."*

Three measurements on this tip:

1. `code/Game/Component/statusicon_geometry.cpp:341` reads **`i < 9`** today and
   installs `nullptr` from 9 up. Wave 22 lane GEOM fixed the loop. **The overrun
   the blocker is built on does not exist any more.**
2. `standalone/rt_data_mint_tables.inc:1568` carries the row at **9 slots**,
   tagged `tail-not-a-pointer`.
3. `standalone/reg_mint_promoted.cpp:1396` already says it outright — *"this
   row's expiry condition FIRED … the below is HISTORY, not a live tag."*

**So the tree has contradicted itself since Wave 22 and nobody noticed, because
the two halves live in different directories** — and there is a sharper version
of that: an earlier pass *in this very file* (Wave 23 lane GFXBODY) corrected the
12-to-9 DETAIL and then concluded *"nothing about the decision changes."* True of
the DECISION, false of the BLOCKER — and the word `STANDING BLOCKER` survived two
more waves inside a paragraph that had just been re-checked. **A re-check scoped
to the detail leaves the headline standing.**

**What I struck, and what I deliberately did not.** The overrun blocker is
discharged. The row is **still not promoted**, and `reg_mint_promoted.cpp:1396`
calls promoting it *"a separate, still-open step."* So the note now reads "the
overrun blocker is discharged; the row still needs its own re-score", **not**
"minting Geometry is safe". Widening it to the second would be a discharge
broader than the evidence — kickoff §5's failure mode with the sign flipped.
`scene.cpp`'s own ctor decision is unchanged and never depended on this consumer.

### 9.1 THE OTHER THREE ITEMS

* **`??_7MaterialInstance@` is now PROMOTED (VTGFX).** It changes no conclusion
  here: my three MaterialInstance-adjacent rows were refused on **size**, and
  none of the refusals reasoned about where its dispatch lands.
  `?BuildParameters@` is unaffected — it has 2 rel32 callers and is reached from
  `wave24_lane_gfxmat.cpp` **by name**, neither of which is a vtable slot.
* **`g_oracleRuns` as an effectiveness counter** — noted, and the same
  discipline is built into §3.1's C1 rather than bolted on. My oracle's own
  equivalent is the per-cell table it prints; **if it prints nothing, nothing
  ran**, and §8.2 says exactly that.
* **Grep `docs/` for prior attendance before writing "unverifiable"** — noted,
  and **this lane never wrote it.** What §8 records is *"not run"*, which is a
  different claim: the oracle exists, is registered and is labelled; it has had
  no run yet. I did not need prior attendance evidence, because I am not
  claiming a verification.

## 10. TAGS TOUCHED

**None.** No `UNPROMOTABLE` tag was created, altered or retired by this lane, so
there is no SCOPE/ADDRESSEE token to file. `?AddDrawItems@MeshInstance@`'s tag
was **read and re-derived** (§1.1) and left exactly as it was. The `scene.cpp`
note struck in §9 is a source comment, not a tagged census row — the tagged row
it wrongly claimed to block is untouched.

---

## 11. FILES

```
runtime/src/engine/mainloop_frame.cpp       NEW   MainLoop::Frame + the one clock definition
runtime/src/engine/mainloop_internal.h      NEW   one owner for the clock helper
runtime/src/io/package_file_dtor.cpp        NEW   ~PackageFile + the [ORACLE]
runtime/src/engine/mainloop_run.cpp         EDIT  use the shared clock; two stale comments
runtime/src/engine/rt_engine_port.cpp       EDIT  BANISHED_RT_MAINLOOPFRAME
runtime/src/io/rt_io_port.cpp               EDIT  BANISHED_RT_PKGDTOR
runtime/types/engine/MainLoop.h             EDIT  _startTicks question answered (§4.3)
runtime/types/io/FileSystem.h               EDIT  +0x58/+0x60 are Strings (§3.3)
```

Kill switches: `BANISHED_RT_MAINLOOPFRAME=0`, `BANISHED_RT_PKGDTOR=0`. Each is
its own switch and neither is folded into an existing one — `Frame` runs every
frame with no oracle, and `~PackageFile` is the first body in its class that
frees, so a defect is a double free rather than a wrong value.
