# Wave 9, lane E3 — closing the arena-growth coverage hole

2026-09-08, `wave/upgrade-path-complete`. **Source only. Nothing was built, deployed or
run** — lane W holds the game directory. The three touched files pass
`g++ -std=c++17 -fsyntax-only -Wall -Wextra` (a syntax check, not a build; it produces no
object, no exe and no deploy).

Answering [WAVE9_LANE_E2.md](WAVE9_LANE_E2.md) §5.1, the one open item under the GO gate.

**Headline: the growth path is UNREACHABLE by any workload we can drive, and TRIVIALLY
reachable by configuration — because the 512 MB arena is not a property of the engine, it
is a literal argument in our own source at `code/Game/Game/game_main.cpp:439`.** The cell
that closes this needs no new probe: the existing shadow oracle covers the growth path
unchanged, on the real allocator, with the real DLL running beside it.

---

## 1. What makes a second `MemoryRange`

### The condition, in one sentence

A second range is created **iff a single `Allocate` asks for `want` bytes and no linked free
block anywhere in the arena is `>= want`.** Nothing else in the process can create one.

### Retail — read from the shipped bytes, not from our transcription

A full rel32 scan of `Runtime-x64.dll`'s `.text` finds **exactly three** call sites for
`AddNewMemoryRange` (`0x1800c7690`) and **exactly one** for `ExpandMemoryRangeTable`
(`0x1800c77e0`):

| site | inside | reachable? |
|---|---|---|
| `0x1800c6ecf` | `MemoryAllocator::Allocate` | **yes — the only growth trigger in the process** |
| `0x1800c7654` | `MemoryAllocator::Startup` (`0x1800c7620`) | **no** — that function has zero callers in the shipped game (E2 §2, proved two ways) |
| `0x1800db905` | `Engine::Startup` (`0x1800db8a0`), where MSVC inlined `Startup` | yes, exactly once, creating range 0 |
| `0x1800c76d6` (`ExpandMemoryRangeTable`) | `AddNewMemoryRange` | its only caller |

Disassembled at `0x1800c6eb2`:

```
1800c6eb2  49 8b cf           MOV  RCX, R15          ; this
1800c6eb5  48 8d 68 08        LEA  RBP, [RAX+8]      ; want = total + 8
1800c6eb9  48 8b d5           MOV  RDX, RBP
1800c6ebc  e8 6f 03 00 00     CALL 1800c7230         ; GetFreeTagForSize(this, want)
1800c6ec1  4c 8b f0           MOV  R14, RAX
1800c6ec4  48 85 c0           TEST RAX, RAX
1800c6ec6  75 31              JNZ  1800c6ef9         ; got a block -> no growth
1800c6ec8  48 8b d5           MOV  RDX, RBP
1800c6ecb  49 8b cf           MOV  RCX, R15
1800c6ecf  e8 bc 07 00 00     CALL 1800c7690         ; AddNewMemoryRange(this, want)
1800c6ed4  48 8b d5           MOV  RDX, RBP
1800c6ed7  49 8b cf           MOV  RCX, R15
1800c6eda  e8 51 03 00 00     CALL 1800c7230         ; retry
1800c6edf  4c 8b f0           MOV  R14, RAX
1800c6ee2  48 85 c0           TEST RAX, RAX
1800c6ee5  75 13              JNZ  ...               ; else FatalOutOfMemory(msg, true)
```

That is byte-for-byte the shape of the ported body (`runtime/src/system/allocator.cpp`,
`Allocate`), including passing `want` — not a size class — to `AddNewMemoryRange`.

### Why `GetFreeTagForSize` returning null really does mean "no block fits"

`GetFreeTagForSize` starts at `max(GetBinForSize(want), self->minBin)` and walks bins upward,
first-fit within each. Two things could have made a null a *search artefact* rather than
genuine exhaustion, and neither does:

* the clamp is **up** to `minBin`, and `minBin` is a lower bound on the lowest occupied bin
  (`LinkFreeTag` drives it down, `UnlinkFreeTag` only raises it when the bin it emptied *was*
  `minBin`). Bins below it are empty by construction, so clamping can never skip a usable
  block;
* blocks under 16 bytes are never linked (`LinkFreeTag` early-returns), so they are invisible
  — but `want = total + 8` with `total >= 8`, so `want >= 16` always, and a 16-byte block is
  linked. No usable block is ever hidden.

### The two routes to it, and the sizing rule

`AddNewMemoryRange(self, want)` reserves `size = max(self->minRangeSize, want)`, then
`if (maxTotalSize != 0 && maxTotalSize < size + totalReserved) return false`.

| route | what it takes |
|---|---|
| **A — exhaustion / fragmentation** | the arena's largest free block drops below `want`. With coalescing and a virgin tail block, this means the high-water mark approaches `minRangeSize`. |
| **B — one oversized request** | `want` exceeds the largest free block outright. On a fresh arena that is `want > minRangeSize`. This range is sized to the *request*, not to `minRangeSize`. |

Banished passes `maxTotalMB = 0`, so the `maxTotalSize` refusal branch is **dead in this
game** — growth is never denied, it just keeps reserving.

### Correction to the ask: N > 1 is not enough — the target is N >= 3

`ExpandMemoryRangeTable` only runs when `rangeCount == rangeCapacity`, and capacity grows by
**two** at a time:

| range created | capacity before | expands? | branch taken |
|---|---|---|---|
| 1 (arming) | 0 | yes, 0→2 | `oldCap == 0`: no `memcpy` of live data, no `release` |
| 2 | 2 | **no** | — |
| 3 | 2 | yes, 2→4 | **`oldCap != 0`: `memcpy(dst, src, oldCap*0x18)` AND `g_plat.release(src)`** |
| 5 | 4 | yes, 4→6 | same |

So a run that reaches `ranges=2` still leaves the **range-table realloc-and-copy — and the
release of the old table — unexecuted**. The first `AddNewMemoryRange` during arming already
covers the range-*construction* body. What `ranges=2` newly buys is the call *from inside
`Allocate`*, the multi-range scans, and `rangeIndex != 0`. The copy path needs a **third**
range. The gate should require `N >= 3`, not `N > 1`.

---

## 2. Can any existing harness cell reach it? No — and no workload ever will

`minRangeSize` is 512 MB (observed, logged: `Engine::Startup ENTRY (min=512MB max=0MB)`).
E2's three cells ran 12,750,873 shadowed operations and finished at `ranges=1` on every one,
including `sa-stress`, which is the largest workload this project has. The stress town's peak
is ~700 MB *private process* but comfortably under 512 MB of *arena*.

**Plainly: route A is unreachable by any town we can build, and route B would need a single
half-gigabyte allocation, which this game never makes.** If the arena size were fixed, the
honest disposition would be "argue it, don't test it".

**It is not fixed.** `code/Game/Game/game_main.cpp:439` — our own code — reads:

```cpp
unsigned ok = s_fn_Engine_Startup(&engine, cmdLine, 0x200, 0, false);
```

`0x200` = 512 is `minRangeMB`; `0` is `maxTotalMB`. Confirmed at
`0x1800db8d9: SHL RAX,0x14 ; MOV [RCX+0x10],RAX` (WAVE8_LANE_B.md:225) and by the driver's
own arming line. The exe chooses the arena size, and we are the exe.

So the disposition is neither "test it with a bigger town" nor "argue it": **shrink the arena
and re-run the cells we already have.** The growth path then executes in the real game, under
the real allocator, with the real DLL side by side and the existing oracle comparing every
call. That is strictly better than a synthetic probe — a probe would exercise the growth code
against inputs we invented; this exercises it against the inputs the game actually produces.

### Bonus coverage this cell gets for free

With `minRangeSize` at 16 MB, ordinary large buffers (terrain, meshes, textures) exceed it,
so `size = max(minRangeSize, want)` takes the **`want` branch** and creates a range sized to
the request. That is route B, which 512 MB can never reach.

---

## 3. The cell — schedulable as-is

One new knob, `BANISHED_ARENA_MB`, landed at `code/Game/Game/game_main.cpp`. **Unset (the
default and every normal run) the call is byte-identical to what it has always been.**
Clamped to `[4, 4096]`; out-of-range values are ignored with a log line.

```bash
mingw32-make standalone-debug && mingw32-make sa-deploy

# G1 — autostart, aggressive arena
tools/rt_run.sh g1 -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 \
                      BANISHED_ARENA_MB=16 BANISHED_AUTOSTART=1 \
                      BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60

# G2 — save load (E2's deepest cell: 3.28M allocations)
tools/rt_run.sh g2 -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 \
                      BANISHED_ARENA_MB=16 BANISHED_AUTOLOAD=Save9 \
                      BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60

# G3 — stress town
BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_ARENA_MB=16 \
  tools/stress_run.sh --debug --frames 1500

# G4 — NEGATIVE CONTROL, REQUIRED to go red (see §4)
tools/rt_run.sh g4 -- BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 \
                      BANISHED_RT_ALLOC_SHADOW_NEGCTL=grow \
                      BANISHED_ARENA_MB=16 BANISHED_AUTOSTART=1 \
                      BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
```

If 16 MB destabilises anything, walk it back: **64 → 32 → 16**. 64 MB is enough for route A
several times over on the save-load cell; 16 MB is what makes route B and a deep range table
certain. Do not go below 4 (the clamp refuses).

### Pass criteria for G1–G3

```
[RtAllocShadow] COVERAGE ranges ours=N theirs=N  reserved ours=RMB theirs=RMB
[RtAllocShadow] GROWTH   grow=G tableCopy=T crossRange=C
[RtAllocShadow] SUMMARY  armed=1 observed=... divergences=0
                skipped_pre_startup=0 other_allocator=0 map_full=0 free_unmapped=0
```

| term | required | why |
|---|---|---|
| `ranges ours == theirs`, `N >= 3` | yes | N≥3 is what reaches `ExpandMemoryRangeTable`'s copy-and-release branch (§1) |
| `reserved ours == theirs` | yes | catches a right-count/wrong-size growth |
| `grow >= 1` | yes | **the effectiveness control.** `grow` counts Allocate-driven range creations that actually happened, not requests made at the path. `grow=0` ⇒ the cell measured nothing about growth, whatever `divergences` says |
| `tableCopy >= 1` | yes | the `oldCap != 0` `memcpy` + `release` actually ran |
| `crossRange` large | yes | comparisons that landed outside range 0 — the `rangeIndex != 0` term was exercised, not just present |
| `divergences = 0` | yes | |
| the four skip counters `= 0` | yes | unchanged from E2 |

The driver prints the two failure modes explicitly rather than leaving them to be noticed:
`grow=0 — the arena-growth path did NOT run in this cell` and `tableCopy=0 — … that needs a
THIRD range; shrink BANISHED_ARENA_MB further`.

---

## 4. The negative control — and why the two existing ones do not cover growth

`=minbin` is a **proved no-op** (E2 §4: `LinkFreeTag` overwrites the perturbation while
arming). `=size` goes red, but it fires on the *first perturbed request*, which happens in
range 0 long before any growth — it proves the geometry terms work, it says nothing about
whether the oracle can see a growth error. **Neither is a control for this path**, which is
`reference_shadow_predicate_must_name_its_failing_term.md` applied to a third term.

### `BANISHED_RT_ALLOC_SHADOW_NEGCTL=grow` (new)

`RtAllocShadowStartup` adds `0x1000` to the replica's `minRangeSize` **after** range 0 has
been created at retail's exact size. Consequences, in order:

1. Range 0 is byte-identical to theirs, so the replica is **silent** for the entire
   single-range life of the run — a `=grow` run on a 512 MB arena is green by design, and
   correctly so.
2. At the **first** Allocate-driven `AddNewMemoryRange`, the replica reserves 4096 bytes
   more, so `totalReserved` differs **on that very call**. `AddNewMemoryRange` only ever adds
   to `totalReserved`, so it can never converge back.
3. The longer range 1 then fills one allocation later than theirs, so `rangeIndex` and
   `offsetWithinRange` — the terms that actually prove placement — diverge shortly after and
   never recover.

It cannot be silently overwritten the way `minBin` was: `minRangeSize` is written once, at
arming, and read only by `AddNewMemoryRange`.

### Power — computed, not asserted

This is the failure mode the lead flagged (lane N: green at 0 of 525 samples against a
1-in-23,654 effect, expected count 0.02 — a control that could not have fired). The
arithmetic here:

* `=size` perturbs **every 100,000th** request. On E2 run 1 that is
  `floor(1,771,964 / 100,000) = 17` expected perturbations; it fired on the first. Powered,
  but only ~17-fold.
* `=grow` perturbs the **input to every Allocate-driven growth**, so its per-event detection
  probability is **1**, not a sampled fraction. Expected divergences =
  *every comparison from the first growth to the end of the run* — on a cell where growth
  happens during boot that is `>= 10^5`, of the same order as `=size`'s 392,243.
* The one way it can be underpowered is `grow = 0`, i.e. the cell never reached the path —
  and `grow` is **measured and printed**, so that case is observable rather than inferred.
  **A green `=grow` run with `grow >= 1` is not "weak evidence"; it means the oracle is BLIND
  to growth and every green growth run must be thrown away.**

That conditional structure is the thing lane N's control lacked: its power depended on an
event rate nobody measured. Here the enabling event is a reported counter, and the pass
criteria for G1–G3 already require it to be non-zero.

**G4 pass criterion: `divergences > 0` AND `grow >= 1`.** Either alone is not a result.

---

## 5. Source changes

| File | Change | Default behaviour |
|---|---|---|
| `code/Game/Game/game_main.cpp` | `BANISHED_ARENA_MB` overrides `Engine::Startup`'s `minRangeMB` argument (was the literal `0x200`), clamped `[4,4096]` | **unchanged** — unset ⇒ 512, the same call as before |
| `runtime/src/system/allocator.cpp` | `grow` / `tableCopy` / `crossRange` counters, derived from replica state **deltas** in the shadow layer so the ported bodies stay byte-faithful and untouched; `rangeCount` and `totalReserved` added to the compared tuple and to both descriptors; `NEGCTL=grow` | inert without `BANISHED_RT_ALLOC_SHADOW` |
| `runtime/types/system/MemoryAllocator.h` | declares `RtAllocShadowGrowth` | — |
| `standalone/rt_alloc_swap.cpp` | `[RtAllocShadow] GROWTH` line plus the two explicit failure messages | inert without the shadow |

Two terms joined the per-call equality tuple: `rangeCount` and `totalReserved`. They are the
two fields `AddNewMemoryRange` writes that nothing else in the tuple observed, so without
them a growth at the *wrong allocation*, or one reserving the *wrong number of bytes*, was
only caught indirectly — and only once a later allocation happened to land in the wrong
range. They are also what makes `=grow` fire on the growth call itself rather than several
thousand allocations later.

`BANISHED_RT_ALLOC` was not set, not armed, and not touched.

---

## 6. Disposition

§5.1 moves from **open** to **scheduled**: the growth condition is pinned to one instruction
in retail, the path is unreachable by workload but reachable by one argument we own, the cell
is specified with an effectiveness control that fails loudly when the path does not run, and
the negative control is new, growth-specific, and powered conditionally on a counter the run
reports.

It is **not closed** — nothing here has executed. Closing it needs G1–G4 run and read.
