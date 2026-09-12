# WAVE 24 KICKOFF — the body phase is ending; this wave turns toward cutover

Base: `wave24/base`, cut from `wave23/base` at `7591f95e` (Wave 23's ten lanes all merged).
Opened 2026-09-11. **Seven lanes, private worktrees.**

## 0. THE BOARD, RE-DERIVED AT KICKOFF ON THIS TIP

```
tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py --unported
```
```
TOTAL   4632   1319 called   1159 ported   8 detour   87.9%
=> 22 of 160 gap rows (14%) are a function to reimplement.

namespace       gap |  BODY CTOR DTOR OPNEWDEL VTABLE DATA UNKNOWN
internal         45 |     0    0    0        0      0    0      45
Graphics         21 |     6    0    0        0      8    7       0
System           19 |     7    0    0        0      1   11       0   <- all 7 BODY are ALLOC, GATED
Video            18 |     0    0    0        0     12    6       0   <- AT CEILING, zero bodies
IO               14 |     1    0    1        0      8    4       0
Object           10 |     0    0    0        0      4    6       0
Achievement       9 |     1    0    0        0      4    4       0
Engine            8 |     1    0    0        0      4    3       0
Audio             5 |     1    1    1        0      0    2       0
ordinal           4 |     0    0    0        0      0    0       4
Input             3 |     1    0    0        0      0    2       0
UI                3 |     0    0    0        1      2    0       0
Application       1 |     0    0    0        0      0    1       0
```

**READ THE 22, NOT THE 87.9%.** Seven of the 22 are the allocator — already **written**, held
behind `BANISHED_RT_ALLOC`, and arming them is a measurement and a risk decision, not a port.
The genuinely unwritten body surface is **15 rows**, plus INTERNAL's 8 = **~23 functions**.

**That is why this wave is not shaped like Wave 23.** Three lanes port bodies; four attack
cutover and verification, because that is where the remaining risk lives.

## 1. HARD CONSTRAINTS

### 1a. THE 22 STALE WORKTREES ARE GONE — KEEP IT THAT WAY

At kickoff `.claude/worktrees/` held **22 registered worktrees** from Waves 21-23, each a full
copy of the tree. Measured before removal:

```
grep -rn "RT_DIFF_SHADOW" --include=*.cpp .            ->  7990   WRONG (22x inflated)
grep -rn "RT_DIFF_SHADOW" --include=*.cpp \
      runtime/ code/ standalone/ engine/ dx11/         ->   354   CORRECT
```

They have been removed by this kickoff. **The rule outlives them:** scope every grep to the
source roots, because *your own* worktree is about to become the 8th copy in the tree. An
unscoped grep returns a confident wrong number with no symptom — the exact shape this project
has been bitten by six times. `runtime_census.py` is already safe (it derives its root from
its own path); ad-hoc greps are not.

### 1b. THE GAME DIRECTORY IS ONE RESOURCE — THREE LANES, IN THIS ORDER

`CUTOVER` -> `DIFF` -> `TRACE`. Acquire `tools/game_dir_lock.sh` **bare — NEVER piped**
(a pipeline reports the LAST command's status, so a timeout reads as a GRANT). Hold briefly,
release.

**The other four lanes are binary-and-source work and get no run.** This is by design, not
scarcity: **GFXMAT, SMALLBODY and INTERNAL8 write their oracles as shadow sites and DIFF's
armed sweep exercises them.** Writing an oracle you do not personally run is the intended
deliverable, not a shortfall.

Before writing "unverifiable, no route": grep `standalone/` and the `BANISHED_*` env list.
`tool_harness.cpp` has repeatedly already held the driver.

### 1c. NAME YOUR PROBE SHAPE BEFORE YOU START

Wave 23's §7 item 7: ten lanes each independently concluded "no run, therefore STRUCTURAL".
State in your first report which of these you are building, and why the others do not apply:

- **input sweep** against the shipped export (works for pure functions with no caller)
- **shadow/diff site** exercised by someone else's run
- **synthesised oracle** for a body unreachable in play ("no gameplay route" is NOT "no oracle")
- **write-path replay** (a `Serialize` on the WRITE path is replayable)
- **two-sided disassembly check** (STRUCTURAL — the fallback, not the default)

## 2. LANES AND THEIR ROWS

| lane | lock | subject |
|---|---|---|
| **CUTOVER** | 1st | Attempt 25 — run a DLL-absent boot and **name the next wall** |
| **DIFF** | 2nd | One armed `BANISHED_RT_DIFF=1` sweep over 354 shadow sites |
| **TRACE** | 3rd | A draw-call/state-trace oracle to replace the dead pixel oracle |
| **TINFO** | none | Are promoted `?_typeInfo@` rows **inert, or wrong**? |
| **GFXMAT** | none | The Graphics/Serializer cluster — 6 bodies + a vtable + a detour |
| **SMALLBODY** | none | 9 rows across six namespaces — the tail of the 22 |
| **INTERNAL8** | none | The 8 real bodies inside the 45 address-keyed unknowns |

### CUTOVER — attempt 25

Wave 23 lane ENGINE **moved the wall** off `?PlatformConfigure@Engine@1@` (Half A ported,
Half B now refuses by name instead of jumping to 0) **but never ran a DLL-absent boot.**
The next wall is therefore NOT named, and a boot is a GATE, not a planner — it can only
name what it reached before faulting.

Carry in the lists Wave 23 handed forward, all of which INVERT at cutover:
- the **ten Object rows** (`WAVE23_LANE_OBJ.md` §6) — publish list
- **six Video rows** flip `HARMFUL` -> `REQUIRED`
- **`??_7Thread@System@@6B@`** registers at cutover, not before
- `?_singleton@MemoryAllocator@System@@` — `engine_funcs.cpp:1629` faults immediately without it

Deliverable is **the next wall, named, with an EXEC-of-0 or a refusal message**. Promoting
zero rows is an acceptable outcome. Read `standalone_crash.log` FIRST; and note that two
crash-log fields are structurally incapable of carrying what they get read for (asked-by names
the RESOLVER, not the caller; VCALL-of-0 is a heuristic on RCX).

### DIFF — the single highest-value action available

`WAVE23_CLOSED.md` §7 item 11. One armed run covers every Wave 23 lane's oracles at once.

**Two traps, both already paid for:**
1. **Read the `calls=` column, not just `divergences=`.** A port with zero callers reports
   clean. And `calls=0 worked>0` does NOT mean unexercised — it can mean the body ran every
   time and always took the branch the oracle does not cover.
2. **Most of the 354 sites are SELF-CHECKS, not oracles.** They take the port's own verdict
   and never consult the DLL, so a shared misunderstanding of retail passes every time.
   **Report the two populations separately.** A number that merges them is worthless.

Secondary, if the run affords it: the census reports **422 of 441 caller-less ports are
UNTRIAGED** — no verdict of any kind. That is the largest unmeasured population in the
project. A caller-less port with no verdict is not evidence of anything; it is work nobody
has done. Converting even a tranche of it to verdicts is real progress.

### TRACE — replace the pixel oracle, do not repair it

Wave 23 §3 measured five runs at identical seed, pin and command line splitting into two
clusters **~98% apart, independently of the flag under test** (two *armed* runs differed by
97.98%; an armed and an unarmed run by 0.79%). `BANISHED_SEED` + `BANISHED_CAMERA_PIN` do not
give a reproducible frame. **A pixel diff measures nothing.**

Build a **draw-call / pipeline-state trace oracle via the RenderDoc MCP** — it compares the
command stream, not pixels, so it does not need frame determinism at all. Diagnosing the
non-determinism is SECONDARY; do not spend the lane on it.

### TINFO — settle handover item 1 before any further `?_typeInfo@` promotion

`standalone/rt_data_mint_tables.inc:1607` is a bare `static RtObject::BaseTypeInfo
kTypeInfoStorage[104];` — zero-initialised, and nothing seeds it. **Verified at kickoff, not
quoted.** Rows are already promoted on that path and UI is not broken, so the standing reading
is "inert — promoted but never dispatched". That is **a hypothesis with corroboration, not a
measurement.**

`runtime/src/object/factory_dispatch_probe.cpp` and `RtMintIsMintedTypeInfo` already exist to
answer this by measurement. The predicate has a positive control
(`RtMintTypeInfoStorageProbe`) — **use it, or "0 minted nodes dispatched" is
indistinguishable from a predicate stuck on `false`.**

Gates ~10 further `?_typeInfo@` gap rows across Achievement, Graphics, IO and Object.

### GFXMAT — one lane, exactly as Wave 23 scoped it

```
?Draw@Renderer@Graphics@@QEAAXAEAVContext@Video@@MAEBVScene@2@AEBVCamera@2@@Z
?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ
?Serialize@AnimationManager@Graphics@@QEAAXAEAVSerializer@IO@@@Z
?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z
?Update@AnimationState@Graphics@@QEAA_N...           <- pulled into `called` by Wave 23 GFXBODY
??_7MaterialInstance@Graphics@@6B@                   <- the mint needs ALL NINE slots
?Setup@MaterialInstance@Graphics@@                   <- needs a DETOUR, not a registration
```

**Explicitly NOT a cheap win** (§7 item 4). `MaterialInstance::Serialize` dispatches through
the Encoding vtable at `+0xa0`, calls four unexported internals, and calls `BuildParameters`
— **three of four slots unblocks nothing.** Both `Serialize` rows are a Serializer problem,
which is why they are here and not in a Graphics-only lane.

`?Setup@` registered rather than detoured stays **dead in play**: retail's traffic is rel32,
and a registration is not a door for that.

`?AddDrawItems@MeshInstance@` is tagged `UNPROMOTABLE(HARMFUL)` and is **not** your row —
`scene_watch.cpp:401` uses its address as an identity token.

Note for the `Serialize` rows: the IO `Encoding` vtable OFFSETS DIFFER between retail and the
profile DLL (`+0x78/98/a0` vs `+0x90/b0/b8`). Confirm which binary you are reading.

### SMALLBODY — the tail of the 22, nine rows, six namespaces

```
Audio        ??0Stream@Audio@@QEAA@XZ                      (ctor)
Audio        ??1Stream@Audio@@QEAA@XZ                      (dtor)
Audio        ?Update@Scene@Audio@@QEAA_N...
IO           ??1PackageFile@IO@@UEAA@XZ                    (dtor)
IO           ?SetupFileSystem@Modification@IO@@QEAAXXZ
Engine       ?Frame@MainLoop@Engine@@QEAA_N_N@Z
Achievement  ?Initialize@GoalList@Achievement@@QEAAX...
Input        ?PlatformShutdown@Device@Input@@AEAAXXZ
UI           ??2SpriteSheet@UI@@SAPEAX_K@Z                 (operator new)
```

**`?SetupFileSystem@` is NOT dependency-blocked.** Wave 23 lane IOPORT measured **13 of its
16 callees already ported**; two waves called it "unscoped" and implied a callee wall that
does not exist. **Re-check the other eight blockers the same way before believing any of
them** — six stale-blocker instances in two waves, one of them fourteen waves old.

A dtor mangles `??1`, and `QEAA` is the NON-VIRTUAL spelling — the wrong one resolves NULL
silently. Two of your nine rows are dtors.

### INTERNAL8 — the 8 real bodies

Wave 23 lane INTERNAL established the 45 address-keyed unknowns are **30 unexported functions
+ 12 statics, all already named in our own source comments** — real work is **8 bodies**.
Start from `WAVE23_LANE_INTERNAL.md`; do not re-derive the classification.

Two of that lane's findings bind you:
- **`Runtime-x64.dll` is `/GR-` — RTTI is OFF.** Any vtable screen built on RTTI has no
  positive case in this binary and can only return 0, which reads as "none are vtables".
  **Barred from driving a verdict, anywhere in this project.**
- **`.pdata` understates, up to 20.4x**, and bites the OPPOSITE way from how it was recorded:
  the *entry* fragment reads `chained=False`, so a chained function looks complete. **Union
  forward.** 52.9% of `.text` exports have no `.pdata` at all.

Remember that a by-address door dies at cutover unless something registers that address, and
that `AEAA` means PRIVATE, not unexported — this DLL exports its private members, so an
address door justified by the word "unexported" is often a plain name door.

## 3. INSTRUMENT RULES — every one of these was paid for

1. **"When a tool's output supports the conclusion you already expected, you have received no
   evidence that the tool works."** Wave 23 TAIL's classifier was wrong on seven rows and
   *every error pushed toward the right answer* — a wrong instrument agreeing with the correct
   conclusion leaves no symptom. Hand-check one row **while the tool is agreeing with you.**
2. **A control must corrupt a subject the probe actually COMPARES** — and must not corrupt one
   it should not. Both directions have produced false verdicts in this project.
3. **A green control can leave most of the instrument untested.** Wave 23's Control 1 passed
   22/22 while validating one field, because the comparison short-circuited before the entry
   loop. **Carry an effectiveness counter** — how many cases changed anything — or you cannot
   tell a passing screen from a dead one.
4. **A screen looking for an ABSENCE must be handed a positive shape.** "0 hits" is
   indistinguishable from a dead parser.
5. **State what your instrument EXAMINED.** Six Wave 23 instruments printed a verdict without
   naming their subject; an inflating blind spot *manufactures* work.
6. **`objdump` emits AT&T syntax here.** An Intel-syntax regex silently matches nothing — this
   produced a 20x undercount in one lane and an off-by-one in another, and both would have
   shipped as confident.
7. **A build check cannot detect a misregistered port.** The build stays clean and links.
   The census catches it.
8. **Two figures taken at different tips are not an instrument disagreement.** Check both were
   measured on the same tip before investigating a gap between them.

## 4. REPORTING

Write `docs/runtime_port/WAVE24_LANE_<NAME>.md`. Required sections:
- **what you measured, with the command that re-derives it** (never cite a figure from a doc)
- **your probe shape** (§1c) and its control, with the effectiveness counter
- **what you did NOT establish** — plainly. "I could not get a run" is expected and fine.
- any `RT_UNPROMOTABLE` tag you touch needs **SCOPE and ADDRESSEE** tokens. Nine grandfathered
  expiries still declare neither; a condition with no addressee never fires. `UNPROMOTABLE` is
  ROUTE-SCOPED — the same tag means opposite things DLL-present and at cutover.

**Your figures are measured in YOUR isolated worktree.** Wave 21 published a wave-level number
built from seven such figures and it was wrong. Do not present a lane figure as a board figure;
the integrator re-derives the board on the merged tip.

**Stop editing when you report.** A lane that keeps improving after it reports invalidates the
integrator's build — this has happened.

## 5. THE FAILURE MODE THIS WAVE IS MOST LIKELY TO REPEAT

**A discharge does not propagate.** Six instances in two waves: a blocker discharged nine waves
earlier, another fourteen waves earlier in identical words in the same directory, a guard
stated in a comment but never implemented, a callee wall that does not exist.

**Re-check every stated blocker before acting on it — including your own, however recent.**
Nothing in this project carries a discharge forward, and nothing detects a stale blocker.

And the corollary that catches the rest: **a handover's "cheap wins" list is the stalest
section and the first one acted on.** Three of Wave 20's named cheap wins failed on contact
while every measured section held. Everything in §2 above has a re-derivation command or a
named source — use it.
