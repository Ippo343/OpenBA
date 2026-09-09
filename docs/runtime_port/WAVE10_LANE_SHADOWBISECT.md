# Wave 10 — Lane SHADOWBISECT — the allocator shadow's red, found and closed

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-aa3ff5e12a9fb111d`,
branch `wave10/lane-shadowbisect`, off `wave10/base` @ `d3589ac` (which includes
the SHADOWRE merge).

**Headline, in four lines:**

1. SHADOWRE's named hypothesis is **CONFIRMED**, by a direct instrument and not
   by inference: a ported body was reaching the real arena by a route that skips
   `allocator_lock`'s patched entry, so the shadow never observed those frees.
2. The route is **one C++ call**: `FreeLockedIgnoringSelf` (the single, deliberately
   **unconditional** owner of address `0x180009c30`) called `rtalloc::MemFree`
   **directly** instead of going through the resolve door. `rtalloc::MemFree` is
   `Get(); lockEnter; rtalloc::Free(self, p); lockLeave` — **our** ported free
   logic — and with the DLL present and `BANISHED_RT_ALLOC` unarmed, `Get()`
   returns **retail's** arena.
3. Fixed by resolving `0x1800c7ad0` through `ResolveAddr(Module::Runtime, …)`,
   which is the rule this file's own neighbours already state and is also the
   **faithful** port of `0x180009c30`. **All seven of SHADOWRE's cells are now
   green: 26.5M operations, 0 divergences, 0 gaps.** Both negative controls still
   go red, now against a clean positive arm — which SHADOWRE explicitly did not
   have.
4. **The switch bisect could never have found this.** The registration is
   unconditional inside `RtSystemPortInit`, reachable only by
   `BANISHED_RT_SYSTEM=0`, which also removes 87 exports. The bisect's terminal
   cell is one of eighteen top-level groups and is confounded by construction.
   What found it was the direct instrument in §2 — built precisely because the
   task said to test the hypothesis directly rather than only by bisection.

---

## 0. Provenance

Every number below is read from an archived raw `standalone_error.log` under
`docs/runtime_port/shadowbisect_logs/`, never from a `tail`. Extract them with
`tools/shadowbisect_extract.sh <label> …`, which prints `armed=`/`observed=`/the
four skip counters/`GAPS` **before** `divergences=`.

Two `-O0` debug builds, both deployed and md5-matched by
`tools/shadowbisect_cell.sh` immediately before **every** launch (it re-`verify`s
the lane lock too, and refuses on a mismatch):

| build | md5 | cells |
|---|---|---|
| 1 — `wave10/base` + the gap detector only | `eebf9398d44688005f7196095fc0f428` | `b1`, `b1ctl`, `b1port0` |
| 2 — + the fix and its `LEGACY_DIRECTFREE` control | `a1e0b30de23269ab53f088e63eb92c53` | `f1`, `f1leg`, `s2`, `s3`, `g1`, `g2`, `g3`, `s4`, `g4`, `bis_sys0` |

Every cell's `[shadowbisect] md5 built` line equals its `md5 deployed` line in the
batch transcripts.

### Switch settings of every cell

`BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0` in
**every** cell (`ENGINELIFE=0` is REQUIRED — see SHADOWRE §2; without it the
shadow arms on nothing). `BANISHED_RT_ALLOC` unset everywhere: the shadow
refuses otherwise, and this lane never flipped it.

| cell | workload | extra switches |
|---|---|---|
| `b1` | `AUTOSTART=1 PAUSEMENU=900 PAUSEMENU_QUIT=60` | — (full default ported surface) |
| `b1ctl` | same | `BANISHED_RT_ALLOC_SHADOW_GAPCTL=1` |
| `b1port0` | same | `BANISHED_RT_PORT=0` |
| `f1` | same | — (build 2, fixed) |
| `f1leg` | same | `BANISHED_RT_ALLOC_LEGACY_DIRECTFREE=1` |
| `s2` | `BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1500` | — |
| `s3` | `BANISHED_AUTOLOAD=Save9 PAUSEMENU=900 PAUSEMENU_QUIT=60` | — |
| `g1` | as `f1` | `BANISHED_ARENA_MB=16` |
| `g2` | as `s3` | `BANISHED_ARENA_MB=16` |
| `g3` | as `s2` | `BANISHED_ARENA_MB=16` |
| `s4` | as `f1` | `BANISHED_RT_ALLOC_SHADOW_NEGCTL=size` |
| `g4` | as `f1` | `BANISHED_ARENA_MB=16` + `NEGCTL=grow` |
| `bis_sys0` | as `f1` | `LEGACY_DIRECTFREE=1` + `BANISHED_RT_SYSTEM=0` |

---

## 1. Why the bisect was not the instrument that found it

The brief offered the per-wave kill switches as the cheap halving tool. They are
cheap, and they are the wrong tool here, for a reason that is worth recording
because it will recur.

`RtSystemAllocatorRegisterSharedAddresses()` is called from `RtSystemPortInit`
(`runtime/src/system/rt_system_port.cpp:84`) **after** the `BANISHED_RT_SYSTEM`
early-return but **outside every one of the sixteen sub-switches below it**. Its
own banner says so and says why: the two registrations it replaced were not gated
either. So the finest cut any switch can make on this defect is
`BANISHED_RT_SYSTEM=0`, which also removes String, Filename, WideString, Value,
Thread, Timer, Error and the rest — 87 exports.

That terminal cell is exactly the trap the brief names: **it disables the cause
and the observation together**, and it is one of eighteen top-level groups, not a
line of code. A bisect run to exhaustion would have reported "somewhere in
System" and stopped, and the natural next move — reading the 87 exports for a
free-shaped one — would have missed it, because the culprit is not an export at
all. It is an *address* registration whose body is three lines long.

The instrument in §2 named the line.

---

## 2. The direct test — an unobserved-mutation ("gap") detector

Both shadow hooks (`ShadowOnAlloc`/`ShadowOnFree`) run **post-op, inside the
allocator critical section**. That gives a free, exact detector for the
hypothesis, needing no second detour and no patching:

* at the **end** of every observed op, snapshot the real allocator's five tuple
  counters (`RtAllocShadowPostOp`);
* at the **start** of the next wrapper entry — still inside the same critical
  section, **before** the real body runs (`RtAllocShadowPreOp`) — compare.

Everything between those two points is serialized by that same critical section.
So **any** movement of the real allocator's counters across the window is a
mutation that did not come through `AllocateLocked`/`FreeLocked` — i.e. precisely
the bypassing route the hypothesis names. The detector reads the **real**
allocator only; it never touches the replica, so it is independent of the
divergence comparison it is being used to explain.

`runtime/src/system/allocator.cpp` (`RtAllocShadowPreOp` / `PostOp` / `Gaps`),
`standalone/allocator_lock.cpp` (two new null-by-default hook pointers called
inside the CS), `standalone/rt_alloc_swap.cpp` (filter on `g_shadowSelf`, and the
`GAPS` summary line).

### It answered on the first cell

```
b1  [RtAllocShadow] GAP #1 at observed-op 1582336: the real arena moved with no hook
                    entry between: dAllocCount=-2 dFreeTagCount=1 dAllocatedBytes=-72
    [RtAllocShadow] GAPS checks=1893169 gaps=6753 sum_dAllocCount=-6760
                    sum_dFreeTagCount=-6 sum_dAllocatedBytes=-386520 ctl_injected=0
    [RtAllocShadow] SUMMARY armed=1 observed=1893170 divergences=310834
                    skipped_pre_startup=0 other_allocator=0 map_full=0 free_unmapped=0
```

Read the arithmetic, not the adjectives:

**`1,893,170 − 1,582,336 = 310,834`, which is the divergence count to the unit.**

Every comparison **after** the first gap diverged and **none before it** did. The
same identity holds in the other red cell, on a different binary and a different
run: `f1leg` is `1,924,344 − 1,608,354 = 315,990` = its divergence count exactly.
Two independent runs, two exact hits. The gap is not correlated with the
divergence; it is its first cause, and there is no second cause.

Direction and quantum match the signature SHADOWRE described: every gap is
**negative** `dAllocCount` (frees), in `−32`/`−64`/`−72`/`−96` byte steps — the
`0x18`-payload LinkedList node plus header.

### The detector's own controls, scored separately

| control | requirement | result | verdict |
|---|---|---|---|
| `b1ctl` — `GAPCTL=1` perturbs every 100,000th **snapshot** by `+1 allocCount` | the detector must catch what it injects | `ctl_injected=19`, `gaps=6773` vs `b1`'s `6753` → **+20** caught for 19 injected (one run-to-run unit); `sum_dAllocCount` `−6780` vs `−6760` → **−20** | **PASS** — it corrupts one of the two operands the pre-op comparison actually reads, and the comparison sees it |
| `b1port0` — `BANISHED_RT_PORT=0`, the arm SHADOWRE proved green | the detector must read clean where the red is known absent | `gaps=0` over **1,793,634** checks, `divergences=0` | **PASS** — no false positives over 1.79M checks |
| `s4`/`g4` — the replica negative controls | must be **invisible** to the gap detector (they perturb the **replica**, not the real arena) | `divergences=1,610,913` / `1,423,009` with **`gaps=0`** in both | **PASS** — the two instruments are orthogonal, as they must be, and neither is reading the other's signal |

`checks=` is the effectiveness counter: `gaps=0` with `checks=0` would mean the
detector never ran and the cell says nothing. Every cell above has `checks` in the
millions.

---

## 3. The mechanism, named

`runtime/src/system/allocator.cpp`, before this lane:

```cpp
void FreeLockedIgnoringSelf(void* /*ignoredThis*/, void* p) {
    rtalloc::MemFree(p);            // <-- OUR ported free, on RETAIL's arena
}
```

and thirty lines up:

```cpp
void MemFree(void* p) {
    if (p == nullptr) return;
    MemoryAllocator* self = Get();          // the _singleton slot
    g_plat.lockEnter(self->lock);
    Free(self, p);                          // rtalloc::Free — OURS
    g_plat.lockLeave(self->lock);
}
```

`FreeLockedIgnoringSelf` is the **single, unconditional** owner of address
`0x180009c30` — Wave 9 lane VID2 consolidated three shadowed registrations into
it, correctly, and the banner explains at length why it must be unconditional:
its callers span the DLL-present path (`Graphics::RemoveGlobalObject`,
`Input::KeyMap::ClearAll`, the LinkedList node free, the Set node free) and not
only cutover. That reasoning is sound. What went unnoticed is that the body it
consolidated onto reaches the heap **by binding to our own implementation**
rather than through the door.

So on a completely ordinary, default, unarmed run:

`Graphics::RemoveGlobalObject` → `ResolveAddr(0x180009c30)` → *our*
`FreeLockedIgnoringSelf` → *our* `rtalloc::MemFree` → *our* `rtalloc::Free`
→ **retail's arena**, without ever touching `0x1800c6c00`, where
`standalone/allocator_lock.cpp`'s patch — and therefore every observer riding on
it — lives.

Two consequences, and **the second is the serious one**:

* **MEASUREMENT.** The shadow never saw those frees, so its replica kept blocks
  retail had released: `ours live` higher, `freeTagCount` lower, `bytes` higher
  by a multiple of 32, and `free_unmapped` clean *because the Free never reached
  the hook at all*. That is SHADOWRE's signature, in all four of its terms.
* **SUBJECT.** "The allocator port is off" was **not true** on this path. An
  unarmed run was already mutating retail's arena with our free logic, by a route
  no kill switch could reach and no counter reported. It worked only because the
  port is byte-faithful and the wrapper does take the same lock — which is why
  nothing ever crashed and why this survived from Wave 9 lane VID2 to now.

Nothing about this is specific to the shadow. The shadow is how it became
*visible*.

### The fix

Reach the heap through the ONE door `RegisterAllocatorAddresses()` arms — the
rule `runtime/src/collection/linkedlist.cpp:153` and
`runtime/src/graphics/scene_grid.cpp:181` both already state for the identical
wrapper, and which this file's own banner cites while not following:

```cpp
void FreeLockedIgnoringSelf(void* /*ignoredThis*/, void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}
```

This is **not** a workaround, it is the more faithful port. Retail's
`0x180009c30` is `Enter; Free(_singleton, p); Leave` — byte for byte `?MemFree@`
at `0x1800c7ad0`. Resolving that address reproduces what retail's own body does:

* **unarmed, DLL present** — the door hands back retail's `MemFree`, which calls
  retail's `Free` through its patched entry. Observed, and identical to retail.
* **armed / at cutover** — `RegisterAllocatorAddresses()` has registered
  `0x1800c7ad0 → rtalloc::MemFree`, so the door hands back exactly the body this
  code used to call directly. **Behaviour there is unchanged.**

It retries a null rather than caching one, for the reason `linkedlist.cpp` gives:
this registration is unconditional and can run before the allocator's arming path
supplies the address.

---

## 4. The A/B that attributes it — one binary, one env var

`BANISHED_RT_ALLOC_LEGACY_DIRECTFREE=1` (default off, permanent, diagnostic) puts
the direct call back. It is a control, not an option. It announces itself:

```
[RtAlloc] LEGACY CONTROL: 0x180009c30 calls rtalloc::MemFree DIRECTLY again —
          the ported Free runs on RETAIL's arena and the allocator shadow cannot
          observe it.
```

Same binary `a1e0b30de23269ab53f088e63eb92c53`, same workload, one variable:

| cell | route | `observed` | `gaps` | `divergences` |
|---|---|---:|---:|---:|
| `f1` | through the door (**fixed**) | 1,918,807 | **0** | **0** |
| `f1leg` | direct call (**legacy control**) | 1,924,344 | **6,761** | **315,990** |

`f1leg` also reproduces the identity of §2 exactly
(`1,924,344 − 1,608,354 = 315,990`). One line of code, both directions, one
binary. The attribution does not rest on comparing two builds.

### The bisect's terminal cell, for completeness

`bis_sys0` — legacy control ON (defect live) + `BANISHED_RT_SYSTEM=0`, the only
switch that reaches the registration. See §7 for its numbers. It is **confounded
by construction** and is reported as the bisect's floor, not as evidence.

---

## 5. SHADOWRE's grid, re-taken — new numbers beside the old

All cells: `armed=1`, `skipped_pre_startup=0`, `other_allocator=0`, `map_full=0`,
`free_unmapped=0`, and the virginity line
`Engine::Startup ENTRY … real arena live=0 bytes=0 ranges=0, skipped so far=0`.

### E2's three cells (512 MB arena)

| | Wave 9 `observed` | W9 `div` | SHADOWRE `observed` | SR `div` | **this lane `observed`** | **`gaps`** | **`div`** |
|---|---:|---:|---:|---:|---:|---:|---:|
| 1 autostart (`f1`/`s1`) | 1,771,964 | 0 | 1,897,188 | 314,542 | **1,918,807** | **0** | **0** |
| 2 stress 1500f (`s2`) | 4,565,696 | 0 | 4,481,681 | 2,673,750 | **4,514,878** | **0** | **0** |
| 3 save load Save9 (`s3`) | 6,413,213 | 0 | 6,387,168 | 1,009,626 | **6,407,614** | **0** | **0** |
| **total** | **12,750,873** | **0** | **12,766,037** | **3,997,918** | **12,841,299** | **0** | **0** |

### E3's growth cells (`BANISHED_ARENA_MB=16`)

| | E3 ranges | E3 grow/tbl | E3 div | SR div | **new ranges** | **new grow/tbl** | **crossRange** | **`gaps`** | **new div** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| G1 autostart (`g1`) | 4/4 | 3 / 1 | 0 | 303,091 | **5/5** | **4 / 2** | 672,266 | **0** | **0** |
| G2 save load (`g2`) | 5/5 | 4 / 2 | 0 | 1,011,106 | **5/5** | **4 / 2** | 2,855,146 | **0** | **0** |
| G3 stress (`g3`) | 5/5 | 4 / 2 | 0 | 2,713,855 | **5/5** | **4 / 2** | 1,965,382 | **0** | **0** |

`reserved ours == theirs` to the byte in every cell (512MB/512MB and 80MB/80MB).
The growth effectiveness counters are all non-zero — `grow=4`, `tableCopy=2`
(which needs a **third** range) and `crossRange` in the hundreds of thousands to
millions — so these greens are over paths that were actually taken.

**Grand total: 26,516,271 observed operations, 0 divergences, 0 gaps.**

### The two negative controls — both still valid REDs, now against a clean arm

| control | requirement | result |
|---|---|---|
| `s4` — `NEGCTL=size` | red | **1,610,913** divergences over 1,927,620. First catch at Allocate `#60485`, `bytes` 8 apart, geometry desynchronising immediately after. |
| `g4` — `NEGCTL=grow` | `divergences > 0` **AND** `grow >= 1` | **1,423,009** divergences, **`grow=4`**, `tableCopy=2`. First catch at the first Allocate-driven growth (`res` 33558528 vs 33554440, `+4088` = the perturbation), geometry still agreeing — E3's predicted signature. |

SHADOWRE recorded, correctly, that its controls proved only that the oracle still
discriminates, because there was no clean positive arm at the same settings to
score them against. **There is now**: `f1`/`s2`/`s3`/`g1`/`g2`/`g3` are green at
those exact settings on the same binary. The controls are informative again.

---

## 6. What this costs — and does not cost — the gate

* **Gate step 2 (the allocator shadow) is MET again** on this branch: 26.5M
  operations, zero divergences, zero gaps, over autostart, stress, save-load and
  three arena-growth cells, with both negative controls red and every skip
  counter zero. That is a superset of the Wave 9 grid, not a re-run of part of it.
* **The qualification on lane ALLOC's GO can be lifted** as far as *this* pillar
  is concerned. Nothing in this lane speaks to ALLOC's other evidence.
* `BANISHED_RT_ALLOC` remains **unset and off by default**. This lane never
  flipped it, and does not recommend arming on its own authority — that is the
  wave lead's call, on the whole of ALLOC's case.
* **Read §3's second bullet before arming anything.** The defect was a
  measurement problem *only because a replica existed to notice it*. On the armed
  path there is no replica, and a body that binds directly to `rtalloc::*`
  instead of resolving would be running one allocator's logic on the other's
  arena with nothing to report it. The fix removes today's instance; the class is
  worth a checker.

---

## 7. Cell-by-cell record

Raw logs: `docs/runtime_port/shadowbisect_logs/<label>_error.log`.
Regenerate the extracts with `tools/shadowbisect_extract.sh <label> …`.

| cell | build | `armed` | `observed` | `checks` | `gaps` | `divergences` | skip counters |
|---|---|---:|---:|---:|---:|---:|---|
| `b1` | 1 | 1 | 1,893,170 | 1,893,169 | 6,753 | 310,834 | all 0 |
| `b1ctl` | 1 | 1 | 1,915,075 | 1,915,074 | 6,773 (19 injected) | 317,979 | all 0 |
| `b1port0` | 1 | 1 | 1,793,635 | 1,793,634 | 0 | 0 | all 0 |
| `f1` | 2 | 1 | 1,918,807 | 1,918,806 | 0 | 0 | all 0 |
| `f1leg` | 2 | 1 | 1,924,344 | 1,924,343 | 6,761 | 315,990 | all 0 |
| `s2` | 2 | 1 | 4,514,878 | 4,514,877 | 0 | 0 | all 0 |
| `s3` | 2 | 1 | 6,407,614 | 6,407,613 | 0 | 0 | all 0 |
| `g1` | 2 | 1 | 1,919,912 | 1,919,911 | 0 | 0 | all 0 |
| `g2` | 2 | 1 | 6,386,472 | 6,386,471 | 0 | 0 | all 0 |
| `g3` | 2 | 1 | 4,498,461 | 4,498,460 | 0 | 0 | all 0 |
| `s4` | 2 | 1 | 1,927,620 | 1,927,619 | 0 | 1,610,913 (**required**) | all 0 |
| `g4` | 2 | 1 | 1,905,868 | 1,905,867 | 0 | 1,423,009 (**required**) | all 0 |

Functional check, incidentally: `s2`/`g3` are 1500-frame stress runs and
`s3`/`g2` are full save-loads, all `rc=0` with the fix in. The fix is not only
quiet on the counters, it is quiet in the game.

---

## 8. Files this lane changed

| File | Change | Default |
|---|---|---|
| `runtime/src/system/allocator.cpp` | **THE FIX**: `FreeLockedIgnoringSelf` resolves `0x1800c7ad0` through the door instead of calling `rtalloc::MemFree` directly. Plus the gap detector (`RtAllocShadowPreOp`/`PostOp`/`Gaps`), its `GAPCTL` control, and the `LEGACY_DIRECTFREE` control | fix is **on**; both controls **off** |
| `standalone/allocator_lock.cpp` | two null-by-default hook pointers, called inside the allocator CS at wrapper entry and exit | inert unless the shadow arms them |
| `standalone/rt_alloc_swap.cpp` | wires + `g_shadowSelf`-filters the two hooks; `GAPS` summary line with its `checks=` effectiveness counter | — |
| `runtime/types/system/MemoryAllocator.h` | declares the three gap entry points | — |
| `tools/shadowbisect_cell.sh` | lane cell driver (lock re-`verify` + md5 built-vs-deployed before every launch) | new |
| `tools/shadowbisect_batch{1,2,3}.sh` | the three cell batches, archived beside their numbers | new |
| `tools/shadowbisect_extract.sh` | reads counters in the order that avoids a green-over-nothing | new |
| `docs/runtime_port/shadowbisect_logs/*.log` | raw `standalone_error.log` for every cell | evidence |

Nothing default-on changed except the fix itself. `BANISHED_RT_ALLOC` was unset,
unarmed and untouched in every cell.

### Reproducing

```bash
mingw32-make standalone-debug
tools/game_dir_lock.sh run lane-X 3600 -- tools/shadowbisect_batch2.sh
tools/shadowbisect_extract.sh f1 f1leg s2 s3 g1 g2 g3 s4 g4
```

`BANISHED_RT_ENGINELIFE=0` is REQUIRED in every cell. Read `armed=`, `observed=`,
the four skip counters and `GAPS checks=` **before** reading `divergences=`.
