# WAVE 25 KICKOFF — close the gap, then cut over

Base: `wave25/base`, cut from `wave24/base` at `97402cc9` (Wave 24's seven lanes all merged,
zero conflicts, build clean, checkers green). Opened 2026-09-11. **Eight lanes.**

**The wave's purpose, in one line:** take the remaining 152 gap rows to as near zero as the
evidence allows, so that **cutover attempt 26** runs against a tree with nothing left to
publish. Attempt 26 is NOT a lane — it happens after the merge.

## 0. THE BOARD, RE-DERIVED AT KICKOFF ON THIS TIP

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported
```
```
TOTAL   4632   1323 called   1171 ported   9 detour   88.5%
=> 16 of 152 gap rows (11%) are a function to reimplement.

namespace       gap |  BODY CTOR DTOR OPNEWDEL VTABLE DATA UNKNOWN
internal         42 |     0    0    0        0      0    0      42
Graphics         19 |     4    0    0        0      8    7       0
System           19 |     7    0    0        0      1   11       0   <- all 7 BODY are ALLOC, GATED
Video            18 |     0    0    0        0     12    6       0   <- every row TAGGED
IO               14 |     1    0    1        0      8    4       0
Object           10 |     0    0    0        0      4    6       0   <- every row TAGGED
Achievement       9 |     1    0    0        0      4    4       0
Engine            8 |     1    0    0        0      4    3       0
Audio             4 |     1    0    0        0      1    2       0
ordinal           4 |     0    0    0        0      0    0       4
Input             2 |     0    0    0        0      0    2       0
UI                2 |     0    0    0        0      2    0       0
Application       1 |     0    0    0        0      0    1       0
```

## 0b. THE SCOPING FACT THAT SHAPES THIS WAVE

**41 of the 152 gap rows are already adjudicated `UNPROMOTABLE` and CANNOT be ported in the
ordinary sense** — re-derived at kickoff by parsing the census tag text:

```
rows carrying an UNPROMOTABLE tag:  41     HARMFUL 20 · NO-OP 21
  by namespace: Video 18 · Object 10 · Graphics 5 · IO 5 · UI 2 · System 1
UNTAGGED gap rows == the real portable surface:  107
  internal 42 · System 18 · Graphics 14 · Achievement 9 · IO 9 · Engine 8 ·
  Audio 4 · Input 2 · Application 1
```

**HARMFUL means registering the row BREAKS a DLL-present run.** Video's entire gap and
Object's entire gap are tagged. **A lane that "closes Video" by promoting its 18 rows has not
closed anything — it has broken the build in a way no build check can detect.**

Their correct disposition is **lane MANIFEST**: a cutover publish list, registered
conditionally, because most of them INVERT to REQUIRED the moment the DLL is gone.
**`UNPROMOTABLE` is ROUTE-SCOPED, not permanent** — the same tag means opposite things
DLL-present and at cutover, and two rows both tagged `NONE EVER` have meant opposite things.

**Nobody re-adjudicates a tagged row without re-deriving the tag first.** But do re-derive:
Wave 24 found 5 of 9 stated blockers stale, and the rule below tells you which to doubt.

## 1. HARD CONSTRAINTS

### 1a. THE TRIAGE RULE WAVE 24 PAID FOR — USE IT
> **Every DEPENDENCY-shaped blocker was false or overstated (5 of 9).
> Every ORACLE-shaped blocker survived.**

A dependency claim decays with every wave, because dependencies are exactly what other lanes
are busy porting. An oracle claim does not decay. **Classify every blocker you meet before
believing it**, and re-derive every dependency-shaped one by measurement, not by reading the
sentence. Worked specimen: *"an in-DLL call with no door"* — every fact true, inference false,
because **"not exported" is not "no door"**: `ResolveAddr(Module::Runtime, …)` consults the
port registry first and falls through.

### 1b. A SHADOW SITE IS NOT AN ORACLE
Wave 24 lane DIFF measured the harness over 99.7M calls: **at least 286 of 630 sites (45%)
cannot catch a shared misunderstanding of retail** — 115 SELF-CHECK, 38 ATTENDANCE, and
**133 NO-COMPARISON sites that print `divergences=0` and are structurally incapable of
printing anything else.** 48 sites read `calls=0` with `worked>0`.

So: **make your oracle actually consult the DLL** (`RT_ORIGINAL_ONCE` / `RtOriginal` /
`GetProcAddress`), and **label every site with its class in its own name** —
`[ORACLE]` / `[SELF-CHECK]` / `[ATTENDANCE]`. Wave 24 lanes GFXMAT and INTERNAL8 both did
this; follow them. A site that cannot fail is worse than no site, because it reports green.

**`ResolveAddr(Module::Runtime, addr)` is NOT "the DLL"** — it checks the port registry FIRST,
so an address-based "retail side" compares the port with itself. One such probe was dark for
five waves. **Do not copy `IO::MemoryStream::MemoryStream` as a template — it is a DEAD
ORACLE** (compares a vptr that must differ, via whole-buffer memcmp; permanently red, checks
nothing). `RT_DIFF_REPLAY_PRE/POST` has **zero adopters** and is inert.

### 1c. AN MSVC CTOR RETURNS `this` IN RAX
A ported ctor written to return `void` leaves RAX unset where retail's caller does `RDI = RAX`.
**It works with the DLL present and breaks at cutover** — invisible to every DLL-present test,
including a green diff sweep. If you port a ctor, disassemble its call sites for a post-call
RAX consumer (the Ghidra decompiler drops exactly that register move) and return `self` **if
the bytes do**, not because a caller seemed to want it.

### 1d. THE GAME DIRECTORY — TWO LANES, IN THIS ORDER
`ALLOC` -> `SWEEP`. Acquire `tools/game_dir_lock.sh` **bare — NEVER piped** (a pipeline
reports the LAST command's status, so a timeout reads as a GRANT). Hold briefly, release.
**The other six lanes get no run and that is by design** — write labelled oracles, and the
post-merge sweep exercises them. Before writing "unverifiable, no route", grep `standalone/`
and the `BANISHED_*` list; `tool_harness.cpp` has repeatedly already held the driver.

### 1e. SCOPE EVERY GREP
Your worktree is one of eight copies of the tree. An unscoped `grep -rn ... .` reads ~8x high
with no symptom. Scope to `runtime/ code/ standalone/ engine/ dx11/`. Wave 24's kickoff
quoted **354** shadow sites from such a grep; the real figures are 305 code sites and 630 run
sites, and the 354 also counted 49 comment lines.

### 1f. DECLARE YOUR OWN SURFACE GROWTH
A faithful port that reaches DLL internals by address **grows `called`**. Wave 24 lanes
declared every increment (+2, +2, +0) and one lane caught its own first draft **hiding** its
cost by routing doors through a helper the census greps past. **The flattering number is the
wrong one.**

## 2. LANES

| lane | lock | subject | rows |
|---|---|---|---|
| **BODY** | — | the last real bodies | 9 + the introsort |
| **ALLOC** | 1st | arm `BANISHED_RT_ALLOC` | 7 System BODY, written |
| **VTGFX** | — | vtable promotion, graphics side | Graphics 8 + Achievement 4 |
| **VTIO** | — | vtable promotion, io/engine side | IO 8 + Engine 4 + Audio 1 + System 1 |
| **DATA** | — | the DATA rows that CAN move now | ~46 minus the tagged |
| **INTERNAL42** | — | address-keyed + ordinal | 42 + 4 |
| **MANIFEST** | — | the 41 tagged rows as a cutover publish list | 41 |
| **SWEEP** | 2nd | instrument debt, so attempt 26 is readable | — |

### BODY — the last nine
```
Graphics  ?BuildParameters@MaterialInstance@   1422 B, 361 instr, 23 calls  (size-refused by Wave 24)
Graphics  ?Draw@Renderer@                       921 B  — ZERO rel32 callers, needs a REGISTRATION not a detour
Graphics  ?Update@AnimationState@               1062 B, 9 args — animation evaluator
Graphics  (4th BODY row — re-derive)
IO        ?SetupFileSystem@Modification@        13 of 16 callees ported; the callee wall does NOT exist
IO        ??1PackageFile@IO@@UEAA@XZ            dtor; UEAA is the VIRTUAL spelling
Achievement ?Initialize@GoalList@               blocker OVERSTATED (Wave 24) — dispatch ported since Wave 3
Engine    ?Frame@MainLoop@                      683 B; .pdata understates it 5.6x (union-forward: 684 B, 3 fragments)
Audio     ?Update@Scene@Audio@
internal  0x180028760                           1381 B Array<Mod> introsort — blocker NEVER re-derived; §1a says doubt it
```
`?Frame@MainLoop@` has **no technical obstacle stated anywhere** and has been unclaimed since
Wave 9. `Draw` and `BuildParameters` were refused on **size**, not difficulty.

### ALLOC — arm the allocator, or record why not
Seven System BODY rows are **written and held behind `BANISHED_RT_ALLOC`**. The no-go ended in
Wave 10 (27 exports, atomic swap, 901,685 allocs through our bodies, redzone stress zero
violations). **Arming is a measurement and a risk decision, not a port.** Deliver: an armed
run, a redzone run, a perf A/B, and a verdict. **One `sa-stress` run cannot resolve a 5-point
effect** — a single-run control once produced a false attribution. Watch for the inverse trap
too: an *unarmed* run once mutated retail's arena because a wrapper called `rtalloc::MemFree`
directly. **"Off by default" has been false here before — verify it, don't assume it.**

### VTGFX / VTIO — promotion, and the anti-correlation
**Readiness and value are ANTI-CORRELATED** (Wave 23 GFXVT): the sorting axis is the purecall
count. A table of one concrete slot and nineteen `_purecall` is trivially READY and worth
nothing. **READY is not a route** — a green readiness screen says the table is safe to stamp,
NOT that anything dispatches it. Score on whether a promotion changes a dispatch.

`??_7MaterialInstance@` is **9/9 COMPLETE** as of Wave 24 but **NOT promoted** — COMPLETE is
the static half. Promoting it is VTGFX's first candidate, and it needs a frame to prove it.
**A minted vtable row can be ported in full and permanently unpromotable** — we intercept only
at DOORS, and an in-DLL rel32 call or a RIP-relative LEA stamp is not a door. (Wave 24
SMALLBODY converted such a stamp into a real door for `??_7Stream@Audio@@6B@` — that is the
technique.)

### DATA — mint/publish, and the reader rule
**Promotion safety is governed by unported READERS, not writers** (Wave 23 GFXDATA). Re-derive
each row's in-DLL read/write counts before moving it. A row with many in-DLL readers and few
writers splits a chain if promoted while the DLL is present.

Two decoder traps, both of which shipped as confident under-counts in Wave 23: **`objdump`
emits AT&T syntax here** (an Intel-syntax WRITE regex matched nothing and every site fell
through to READ — wrong by 20x), and a reader classifier keyed on `REX.W` missed the non-REX
`MOV EAX,[rip]; RET` form entirely and would have reported "0 readers".

### INTERNAL42 — 42 address-keyed + 4 ordinal
Wave 23 established the address-keyed bucket is mostly **unexported functions + statics already
named in our own source comments**; Wave 24 ported 7 and left `0x180028760`. Start from
`WAVE23_LANE_INTERNAL.md` and `WAVE24_LANE_INTERNAL8.md`.

**Ordinal rows:** ordinal 2990 is **ported and reachable but still prints as a gap row**,
because `ResolveOrdinal` falls through to the name registry (`resolve_standalone.cpp:900`).
**Ordinals 3214 / 443 / 4462 have the identical shape** — that is a four-row positive control
handed to you. These may be a reporting defect, not port work. Settle which.

**`Runtime-x64.dll` is `/GR-` — RTTI is OFF.** A vtable screen built on RTTI has no positive
case here and can only return 0, which reads as "none are vtables". Barred from any verdict.
**`.pdata` is a LOWER bound** and bites backwards: the *entry* fragment reads `chained=False`,
so a chained function looks complete. **Union forward.** 52.9% of `.text` exports have no
`.pdata` at all. A 34-byte leaf may have no `.pdata` and no Ghidra function object — bound it
from raw image bytes.

### MANIFEST — the 41 tagged rows, as the thing attempt 26 consumes
Produce a **cutover publish manifest**: for each of the 41, the row, its current tag, whether
it INVERTS at cutover, and the registration that must happen when the DLL is gone. Wave 24
CUTOVER already carried four such lists in and they registered clean — extend that mechanism,
do not invent one.

Known inversions to fold in: the **ten Object rows**; **six Video rows** `HARMFUL -> REQUIRED`;
**`??_7Thread@System@@6B@`** registers at cutover only; `?_singleton@MemoryAllocator@System@@`
(`engine_funcs.cpp:1629` faults immediately without it); the **four Video `_first*` storage
rows**, whose storage ceases to exist at cutover so the mint becomes its only owner.

**Every tag you touch needs SCOPE and ADDRESSEE tokens.** Nine grandfathered expiries still
declare neither, and a condition with no addressee never fires. Close those nine.

### SWEEP — instrument debt, so attempt 26 is readable
1. **The dead `MemoryStream` oracle** — one-line fix, described in `WAVE24_LANE_DIFF.md`.
2. **Label the 133 NO-COMPARISON sites** so `divergences=0` stops reading as a pass.
3. **458 of 630 sites never declare `RT_DIFF_WORK`** — 73% of the harness has UNKNOWN coverage.
4. **`RT_DIFF_REPLAY_PRE/POST`: zero adopters.** Adopt it or delete it; inert documented
   machinery is how a future lane loses a day.
5. **`?Serialize@AnimationManager@` must STAY UNVERIFIED** — Wave 24 GFXMAT answered Wave 23
   GFXBODY's R4 and the answer was **no**. Do not delete that note.
6. Stale comment `singleton_ctors.cpp:719-722` (claims `0x180157c60` unminted; it is minted).
7. **`[VideoInterface] *** DETOUR FAILED ***` is a FALSE ALARM at cutover** — the slot fills
   twelve lines later. Silence or annotate it; Wave 24 named it the line most likely to cost
   attempt 26 a session.
8. Then take the lock and run one armed `BANISHED_RT_DIFF=1` sweep so every lane's new oracles
   in this wave get exercised. Report ORACLE / SELF-CHECK / ATTENDANCE / NO-COMPARISON
   **separately** — never a merged number.

## 3. INSTRUMENT RULES
1. **"When a tool's output supports the conclusion you already expected, you have received no
   evidence that the tool works."** Hand-check one row **while the tool is agreeing with you.**
   Wave 24 TINFO's tool read 25/27 while agreeing with it; both dissenters were wrong.
2. **A control must corrupt a subject the probe COMPARES** — and must not corrupt one it
   should not. Both directions have produced false verdicts here.
3. **Carry an effectiveness counter.** A green control can leave most of an instrument
   untested (22/22 while validating one field). Wave 24 TRACE scored **per screen**, never
   unioned, and found three of its six screens carried no verdict at all.
4. **A screen looking for an ABSENCE must be handed a positive shape**, or "0 hits" is
   indistinguishable from a dead parser.
5. **State what your instrument EXAMINED.** A verdict without a named subject is how six
   Wave 23 instruments went wrong.
6. **A build check cannot detect a misregistered port.** The build stays clean and links.

## 4. REPORTING
`docs/runtime_port/WAVE25_LANE_<NAME>.md`: what you measured with the command that re-derives
it; your probe shape and its controls with effectiveness counters; **what you did NOT
establish**, plainly; declared surface growth; SCOPE + ADDRESSEE on every tag touched.

**Your census figure is branch-local and is NOT a board figure** — the integrator re-derives
on the merged tip. Wave 21 published a wave number assembled from lane figures and it was
wrong in both directions.

**Stop editing when you report.**

## 5. THE FAILURE MODE THIS WAVE MUST NOT REPEAT
**A discharge does not propagate**, and Wave 24 added two fresh instances: a blocker refuted
**in the same directory in the same wave** by a sibling lane, and Wave 23's "negative result
worth more than any port" (frame non-determinism) which `tools/dx11_state_diff/README` had
already documented before Wave 23 spent a lane rediscovering it.

**Re-check every stated blocker before acting on it — including your own, and including one
written yesterday.** §1a tells you which ones to doubt first.
