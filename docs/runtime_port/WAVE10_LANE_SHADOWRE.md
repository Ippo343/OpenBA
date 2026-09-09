# Wave 10 — Lane SHADOWRE — re-taking the allocator shadow measurement through the fixed installer

Worktree `C:/code/GIT/untitled_rev_eng_project/.claude/worktrees/agent-a584b59640ca06975`,
branch `wave10/lane-shadowre` (off `wave10/base` @ `ad0e580`, which includes the
lane FREEPRO merge).

**Headline, in three lines:**

1. The corruption FREEPRO found **could not** have produced a false green or a
   false red on the divergence counts. The mechanism is in §1 and it is
   structural, not probabilistic — and an A/B on one binary confirms it.
2. Lanes E2/E3's numbers **do not reproduce** on `wave10/base`. All seven cells
   come back **RED**, where E2/E3 reported 0 divergences over 12.75M operations.
3. **It is not the installer and it is not the allocator port.** The same cell
   with `BANISHED_RT_PORT=0` is **observed=1,789,828 divergences=0**, all four
   skip counters zero. Something in the *Runtime port registrations merged after
   E3* releases memory by a route the shadow does not observe. **Gate step 2 is
   NOT met on `wave10/base` today**, and `BANISHED_RT_ALLOC=1` must not be armed
   on the strength of the E2/E3 numbers.

---

## 0. Provenance

Two builds, both `-O0` debug, both deployed and md5-matched immediately before
every launch through `tools/shadowre_cell.sh` (which re-`verify`s the lane lock
and refuses on a mismatch):

| build | md5 | cells |
|---|---|---|
| 1 — `wave10/base` as merged | `4f35cafcadca7224324dc714823bf468` | s1(void), s1, s3, s4, g1, g2, g4 |
| 2 — + the `LEGACY_RSPADD` control (§3) | `33a4f28f4217c49d8ac54adaf3a6f08c` | s1fix, s1leg, s1port0, s2, g3 |

Every `md5 built` line equals its `md5 deployed` line in
`docs/runtime_port/shadowre_logs/` and in the batch transcripts. Raw
`standalone_error.log` per cell is archived under
`docs/runtime_port/shadowre_logs/` — every figure below is read from those, never
from a `tail`.

---

## 1. What the corruption could and could not do — from the bytes, not from a re-run

### The three registers, and where the fourth store lands

`Engine::Startup` (0x1800db8a0) displaced prologue, from the shipped DLL
(`pefile` + `objdump`, extent taken from the PE exception directory
`begin=0x1800db8a0 end=0x1800dbeb8`):

```
1800db8a0  48 8b c4        mov rax,rsp        RAX = E (entry RSP)
1800db8a3  55              push rbp
1800db8a4  48 8b ec        mov rbp,rsp        RBP = E-8
1800db8a7  48 83 ec 60     sub rsp,0x60
1800db8ab  48 c7 45 c0 ..  mov [rbp-0x40],-2      <- 19 bytes displaced ends here
1800db8b3  48 89 58 10     mov [rax+0x10],rbx     -> E+0x10
1800db8b7  48 89 70 18     mov [rax+0x18],rsi     -> E+0x18
1800db8bb  48 89 78 20     mov [rax+0x20],rdi     -> E+0x20
1800db8c5  0f b6 45 30     movzx eax,BYTE [rbp+0x30]   <- reads E+0x28 = ARG 5
1800dbea7  49 8b 5b 18     mov rbx,[r11+0x18]     <- E+0x10   (r11 = rsp+0x60 = E-8)
1800dbeab  49 8b 73 20     mov rsi,[r11+0x20]     <- E+0x18
1800dbeaf  49 8b 7b 28     mov rdi,[r11+0x28]     <- E+0x20
```

With `ADD RAX,8` in the trampoline, RAX = E+8 while the restore base is
untouched, so the three saves move up one slot:

* `RBX` is restored from **E+0x10, which nothing wrote** — it comes back holding
  whatever the caller's RDX home slot contained;
* `RSI` receives **RBX**'s value, `RDI` receives **RSI**'s;
* the third save lands at **E+0x28**, which is past the caller's 32-byte home
  space (E+8..E+0x27).

### E+0x28 is not "a stray slot". It is `noConfigure`.

`Engine::Startup` takes five arguments; the fifth arrives on the stack at
`[E+0x28]` and is read at `0x1800db8c5` — **after** the three saves. So the
corrupting trampoline overwrote argument 5 with the incoming RDI *before the
function read it*. And that byte is load-bearing:

```
1800dbd94  cmp BYTE PTR [rdi+0x6f1],0
1800dbd9b  jne 1800dbdc0                 ; nonzero -> SKIP
1800dbda9  call 1800a1670                ; Engine::PlatformConfigure
```

`noConfigure != 0` skips `PlatformConfigure` (and, at `0x1800dbcaa`, changes two
bytes on the object at `+0x6b8`). `game_main.cpp` passes `false`; the corrupted
runs passed the low byte of whatever RDI held.

### The question that matters: false green, false red, or neither?

**Neither — for the divergence counts.** Three independent reasons, each
sufficient:

1. **The corruption is strictly downstream of arming.** `ShadowStartup`
   (`standalone/rt_alloc_swap.cpp:182`) prints the virginity line, asserts
   `rangeCount==allocCount==allocatedBytes==skipped==0`, and calls
   `RtAllocShadowStartup` **before** `return orig(...)`. The trampoline is inside
   `orig`. Nothing the corrupted registers touch can reach the arming decision,
   because the arming has already happened.
2. **The comparison is on a different call path and reads different state.**
   Divergences are counted in `ShadowOnAlloc`/`ShadowOnFree`, called from
   `AllocateLocked`/`FreeLocked` inside the allocator's own critical section, and
   the compared tuple is `(rangeIndex, offsetWithinRange, allocatedBytes,
   allocCount, freeTagCount, minBin, rangeCount, totalReserved)` read out of two
   arenas. A rotated callee-saved register in a boot function is not an input to
   any term of that tuple. Ask the field the question a diagnostic must be able
   to answer differently: *could `divergences` have come out differently because
   RSI held RBX's value inside `Engine::Startup`?* It could not — there is no
   edge from one to the other.
3. **A false RED would need a wild write into an arena.** The corruption's one
   out-of-bounds store is at E+0x28, a *stack* slot in the wrapper's frame. It
   never touches either arena, so it cannot desynchronise the replica.

**What it CAN do — and this is the honest residue — is change the WORKLOAD.**
Forcing `noConfigure` true skips `PlatformConfigure`/`ValidateConfiguration`, so
the boot that E2/E3 measured was not the boot a normal run performs, and any
register that leaked past `ShadowStartup`'s epilogue leaked into `game_main`.
That is a *coverage* objection, not a validity one: whatever allocations do
happen are replayed into the replica identically on both sides, so no divergence
can be hidden or manufactured — only a path can go unvisited.

**Therefore a green re-run would have told us nothing new, and a red one is not
attributable to the fix.** Which is why the next section is an A/B rather than a
re-run.

---

## 2. The trigger was dead before any of this — the shadow armed on NOTHING

The first attempt (build 1, E2's exact invocations) reported:

```
[RtAllocShadow] SUMMARY armed=0 observed=0 divergences=0
                skipped_pre_startup=1922887 other_allocator=0 map_full=0 free_unmapped=0
[RtAllocShadow] observed=0 — this run MEASURED NOTHING. Do not read it as a pass.
```

`Engine::Startup` **is now ported** (`runtime/src/engine/engine_lifecycle.cpp:526`,
registered at line 1090), so `game_main.cpp:166`'s `ResolveExport` returns our
body and the DLL's bytes at 0x1800db8a0 — where the shadow's arming detour
lives — are never executed. Lane ENG wrote this down at
`engine_lifecycle.cpp:189` and `rt_engine_port.cpp:69`: **any
`BANISHED_RT_ALLOC_SHADOW=1` run must also set `BANISHED_RT_ENGINELIFE=0`.**

Two things follow that the wave should record:

* **E2 §7 and E3 §3's reproduce recipes are STALE.** Run as written on
  `wave10/base` they produce `armed=0`. The driver's own effectiveness counters
  caught it loudly, which is the whole point of them; a lane reading only
  `divergences=` would have filed a green.
* **`BANISHED_RT_ENGINELIFE=0` restores exactly the configuration E2/E3 ran
  under** (their tree had no Engine lifecycle port), so it is the right way to
  make the numbers comparable, not a workaround that changes the subject.
* Incidentally, with `ENGINELIFE` at its default the FREEPRO corruption is
  **unreachable** on `wave10/base` — the trampoline it corrupts is never called.

Every cell below sets `BANISHED_RT_ENGINELIFE=0` and every one reports
`armed=1`, `skipped_pre_startup=0`, and the virginity line
`Engine::Startup ENTRY … live=0 bytes=0 ranges=0, skipped so far=0`.

---

## 3. The A/B that attributes the moved numbers — one binary, one env var

A green (or red) re-run cannot separate "the installer" from "the eight waves of
tree changes since E3". So build 2 carries a permanent, default-off diagnostic
control, `BANISHED_RT_DETOUR_LEGACY_RSPADD=1`
(`standalone/runtime_detour.cpp`), which puts the removed `ADD RAX,8` back in the
blind-copy path only. It is a control, not an option: it reinstates a defect
Test 2b proves.

**The control is proved ACTIVE, not assumed:** in `s1leg` it printed
`LEGACY CONTROL: re-inserted the WRONG ADD RAX,8 at trampoline offset 3` twice —
once for Test 2b's synthetic subject and once at `0x7ffcab56b8a0`, the live
`Engine::Startup` — and Test 2b went from pass to
`FAIL: 2b: the CALLed trampoline CORRUPTED callee-saved registers`.

| cell | installer | Test 2b | `observed` | `divergences` |
|---|---|---|---:|---:|
| `s1fix` | fixed | **pass** | 1,931,087 | **321,076** |
| `s1leg` | corrupting (legacy) | **FAIL** | 1,915,496 | **313,451** |

**Both red, and the counts differ by less than the run-to-run spread of the same
cell** (`s1` on build 1 was 1,897,188 / 314,542). The installer is exonerated
empirically as well as mechanistically, and §1's prediction — that the fix moves
the workload slightly and the verdict not at all — is what happened. The
`noConfigure` effect is real but too small to separate from ordinary run
variance at this sample size; it is not worth a bigger sample, because the
verdict does not turn on it.

---

## 4. The cells, new numbers beside the old

`BANISHED_RT_ALLOC` unset in every cell (the shadow refuses otherwise); all
report `other_allocator=0`, `map_full=0`, `free_unmapped=0`,
`skipped_pre_startup=0`.

### E2's three cells (512 MB arena)

| | E2 `observed` | E2 `div` | **new `observed`** | **new `div`** |
|---|---:|---:|---:|---:|
| 1 autostart (`s1`) | 1,771,964 | **0** | 1,897,188 | **314,542** |
| 2 stress 1500f (`s2`) | 4,565,696 | **0** | 4,481,681 | **2,673,750** |
| 3 save load Save9 (`s3`) | 6,413,213 | **0** | 6,387,168 | **1,009,626** |
| **total** | **12,750,873** | **0** | **12,766,037** | **3,997,918** |

### E3's growth cells (`BANISHED_ARENA_MB=16`)

| | E3 | | | **new** | | |
|---|---:|---:|---:|---:|---:|---:|
| | ranges | grow/tableCopy | div | ranges | grow/tableCopy | div |
| G1 autostart | 4/4 | 3 / 1 | **0** | **5/5** | **4 / 2** | **303,091** |
| G2 save load | 5/5 | 4 / 2 | **0** | **5/5** | **4 / 2** | **1,011,106** |
| G3 stress | 5/5 | 4 / 2 | **0** | **5/5** | **4 / 2** | **2,713,855** |

`crossRange` is 665,971 / 2,849,403 / 1,975,242 — the `rangeIndex != 0` term is
exercised, the effectiveness controls are all non-zero, and **`reserved
ours == theirs` to the byte (80MB/80MB, and `res=83886088` in the raw rows)**.
So the growth machinery itself still agrees exactly; the divergence is not in it.

### The two negative controls — both valid REDs

| control | requirement | result |
|---|---|---|
| `NEGCTL=size` (`s4`) | red | **1,600,420** divergences over 1,901,417 observed. Caught on the first perturbed call (`bytes` 8 apart at Allocate #60485), geometry desynchronised immediately after. |
| `NEGCTL=grow` (`g4`) | `divergences > 0` **AND** `grow >= 1` | **1,392,426** divergences, **`grow=4`**, `tableCopy=2`. Caught at the first Allocate-driven growth (`res` 33558528 vs 33554440, +4088 = the perturbation, geometry still agreeing) — E3's predicted signature exactly. |

Both requirements are met separately, and both cells report the enabling counter
rather than leaving its power to be inferred. **However**: a negative control is
only informative against a clean positive arm, and this wave has no clean
positive arm at the same settings — so what these two now prove is that the
oracle still discriminates, not that the subject is sound.

---

## 5. The red is NOT the allocator port. The discriminating cell.

Identical `s1` cell, one variable changed — `BANISHED_RT_PORT=0`, the master
switch over every Runtime port registration. `allocator_lock.cpp` and the shadow
driver are independent of it, so the cell still measures:

```
[RtAllocShadow] Engine::Startup ENTRY (min=512MB max=0MB): real arena live=0 bytes=0 ranges=0, skipped so far=0
[RtAllocShadow] COVERAGE ranges ours=1 theirs=1  reserved ours=512MB theirs=512MB
[RtAllocShadow] SUMMARY armed=1 observed=1789828 divergences=0
                skipped_pre_startup=0 other_allocator=0 map_full=0 free_unmapped=0
```

**1,789,828 operations, zero divergences** — and note how close that is to E2's
1,771,964, on a tree whose ported surface was much smaller. The ported allocator
bodies, the replica, the replay map and the comparison are all sound. Turning the
*rest* of the Runtime port on is what makes the same cell red.

### The signature says which direction, and it is the same in all seven cells

Every first divergence has one shape — here `g1`, and `s3`/`g2`/`s2`/`s1`/`s1fix`
differ only in magnitude:

```
ours  = r4+0x2428a0 live=345044 free=151 bytes=71686712  nr=5 res=83886088
theirs= r4+0x2428a0 live=345042 free=152 bytes=71686640  nr=5 res=83886088
```

* the **geometry term agrees** (`r4+0x2428a0` on both sides) on the first
  divergent call — a wrong first-fit, split or coalesce would break *that* term
  first, and it does not;
* `ours live` is **higher**, `ours freeTagCount` **lower**, `ours bytes` higher by
  a multiple of 32;
* `free_unmapped = 0`, so no Free the shadow *saw* failed to find its key.

Those four facts together admit one reading: **the real arena performed frees the
shadow never observed.** The replica keeps blocks retail released, so ours has
more live blocks, fewer free tags and more allocated bytes — and because the
Free never reached the hook at all, `free_unmapped` stays clean. It is the same
*class* as E2's Defect 2 (a drift of one against agreeing offsets) and the
opposite cause: not a lost map entry, a lost observation.

32 bytes is the LinkedList node size (`0x18` payload + header) that
`0x180032750` allocates and `0x180009c30` frees, and those two are exactly the
"allocator-lock wrapper" addresses that Wave 9/10 lanes routed through ported
bodies (`runtime/src/collection/linkedlist.cpp:169`, `scene.cpp:238`,
`input.cpp:131`). **Named as a hypothesis, not a finding:** a ported body reaching
the real `Free` by a route that skips `allocator_lock.cpp`'s patched entry — the
lock trampoline is the obvious candidate, and lane FNHOOK's commit message
("thirteen patch targets aimed at the wrong binary, six at our own code") is the
kind of change that produces exactly this. **I did not bisect it to a function**;
that belongs to the allocator lane, and the bisect is cheap now that
`BANISHED_RT_PORT=0` is known green — the per-wave `BANISHED_RT_<WAVE>` switches
halve it in a handful of runs.

### What this costs the gate, stated plainly

The shadow cannot see all allocator traffic on `wave10/base`. That is worse than
it sounds, because the failure is *silent in the direction that matters*: a
bypassing Free shows up here as a red only because the replica is a separate
arena. **On the armed path (`BANISHED_RT_ALLOC=1`) there is no replica — the same
bypassing caller would be calling retail's `Free` on OUR arena, or ours on
retail's, with no counter to notice.** So this is not only a measurement problem.

---

## 6. Test 2b, quoted in situ

From `s1_error.log` (build 1) and `s1fix_error.log` (build 2), unmodified:

```
[RtSelfTest] ---- Test 2b: RSP-capture prologue through a CALLed trampoline ----
[RtSelfTest]   undetoured: rbx=1111111111111111 rsi=2222222222222222 rdi=3333333333333333 rax=42
[RtSelfTest] pass: 2b baseline: the synthetic subject preserves rbx/rsi/rdi undetoured
[RtSelfTest]   detoured  : rbx=1111111111111111 rsi=2222222222222222 rdi=3333333333333333 rax=42
[RtSelfTest] pass: 2b: RSP-capture prologue survives a CALLed trampoline with rbx/rsi/rdi intact
[RtSelfTest] pass: 2b: removal restores the synthetic subject byte for byte
```

`grep -c "RSP correction"` = **0** in every cell except `s1leg`, where the
legacy control deliberately reinstates it and Test 2b fails. The fixed installer
is live in every measurement above.

---

## 7. Verdict

| claim | status |
|---|---|
| The corruption could have hidden divergences (false green) | **NO** — §1, mechanism; §3, A/B on one binary |
| The corruption could have manufactured divergences (false red) | **NO** — its only out-of-bounds store is a stack slot |
| The corruption changed the measured workload | **YES, slightly** — it forced `noConfigure`, skipping `PlatformConfigure`; below run-to-run noise here |
| E2/E3's reproduce recipes still work | **NO** — they now arm on nothing; `BANISHED_RT_ENGINELIFE=0` is required |
| E2/E3's 12.75M / 0 divergences reproduce | **NO** — 12.77M / 3,997,918 |
| The allocator port is at fault | **NO** — `BANISHED_RT_PORT=0`: 1,789,828 / **0** |
| **Gate step 2 (`BANISHED_RT_ALLOC` READY TO ARM)** | **NOT MET on `wave10/base`** |

**Recommendation to the wave lead: do not arm `BANISHED_RT_ALLOC=1`** on the
strength of `WAVE10_LANE_ALLOC.md`'s GO. That verdict's allocator-shadow pillar
was taken on a tree that no longer exists, and on today's tree the same cells are
red for a reason that is *not* the allocator but *is* in the allocator's blast
radius. Re-arm the gate after the bypassing free route is found and closed, then
re-run §4's grid — it is seven cells and about twelve minutes of lock time.

---

## 8. Files this lane changed

| File | Change | Default |
|---|---|---|
| `standalone/runtime_detour.cpp` | `BANISHED_RT_DETOUR_LEGACY_RSPADD` diagnostic control (§3), blind-copy path only | **off**; unset ⇒ byte-identical to FREEPRO's fixed installer |
| `tools/shadowre_cell.sh` | cell driver: re-`verify`s the lock and md5s deployed-vs-built before **every** launch, archives the raw logs | new |
| `docs/runtime_port/shadowre_logs/*.log` | raw `standalone_error.log` for all twelve cells | evidence |
| `docs/runtime_port/WAVE10_LANE_SHADOWRE.md` | this file | — |

Nothing default-on changed. `BANISHED_RT_ALLOC` was unset, unarmed and untouched
in every cell.

### Reproducing

```bash
mingw32-make standalone-debug
tools/game_dir_lock.sh run lane-X 900 -- tools/shadowre_cell.sh deploy
E="BANISHED_RT_DIFF=1 BANISHED_RT_ALLOC_SHADOW=1 BANISHED_RT_ENGINELIFE=0"   # ENGINELIFE=0 IS REQUIRED
tools/shadowre_cell.sh cell s1     900 -- $E BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
tools/shadowre_cell.sh cell s1port0 900 -- $E BANISHED_RT_PORT=0 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60
```

Read `armed=`, `observed=` and the four skip counters out of the raw
`standalone_error.log` before reading `divergences=`.
