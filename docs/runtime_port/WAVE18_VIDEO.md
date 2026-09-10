# WAVE 18 — the Video::Device interface-enumeration cluster

**Written 2026-09-10 by the integrator at `3da1503`. Not yet dispatched.**
Planning input for the next wave. Every fact below was verified for this
document, not carried over from an earlier one — the last series had six briefs
corrected by a lane, all from repeating prose instead of running a query.

## Why this wave, and what it is FOR

**This is a CUTOVER wave, not a census wave.** Those are now cleanly separate
objectives and this one serves the first.

Cutover boot-gate attempt 24 (`fa4fa86`) dies in the same place as attempt 23,
five waves earlier: **`DeviceData::EnumerateInterfaces`, VCALL-of-0, exit 3.**
That is the correct result rather than a disappointing one — Waves 12-15 worked
on Application, Save and singleton data rows, and none of those is on the boot
path, so none *could* have moved this wall.

Wave 11 and its five follow-ups then took **Object from 57.1% to its proven
ceiling of 83.6%** and, exactly as attempt 24 predicted, **did not move this wall
one line.** The prediction held. This wave is the one that targets it.

## The wall: six functions

Verified absent by the integrator on this tip. The only references in the tree
are (a) a descriptive comment in `runtime/types/video/Device.h` and (b) the DX11
seam's own import table at `dx11/src/devicedata.cpp:241-249`, which *wants* them.
Neither is a port.

Retail addresses read from the DLL's export table (`pefile`), not from any doc:

| symbol | retail |
|---|---|
| `??0GraphicsInterface@Device@Video@@QEAA@XZ` | `0x18000cd90` |
| `??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z` | `0x18000ce10` |
| `??1GraphicsInterface@Device@Video@@QEAA@XZ` | `0x18000cdc0` |
| `??0Adapter@Device@Video@@QEAA@XZ` | `0x1800f7100` |
| `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | `0x18000cc70` |
| `??1Adapter@Device@Video@@QEAA@XZ` | `0x18000cc10` |

Note `??0Adapter` (default) sits at `0x1800f7100`, far from the other five —
worth confirming it is the body and not a thunk before porting.

**A near-miss worth recording:** `dx11/src/devicedata.cpp` lists a *different*
address beside each of these (e.g. `0x1800132d0` for the `GraphicsInterface`
ctor). That column is **the DX11 IAT slot to fill, not the Runtime function
address** — the integrator nearly filed it as an address discrepancy before
reading the surrounding rows, all of which fall in the tight `0x180013218`-`0x1800134c8`
slot range. Do not "fix" it.

## The head start — the layouts are already done

**Both structs are fully pinned in `runtime/types/video/Device.h`**, with
`static_assert`s on every field:

* `Video::Device::Adapter` — **0x38 bytes**, 10 asserts. The comment already
  records that `??0Adapter` (`0x1800f7100`) zeroes `+0x00..+0x27` and writes the
  rest.
* `Video::Device::GraphicsInterface` — **0x18 bytes**, 5 asserts: `name` at
  `+0x00` (a `System::String` data pointer), then an `Array<Adapter>` at
  `+0x08`/`+0x10`/`+0x14`. `??0GraphicsInterface` "zeroes exactly three qwords";
  `GetInterface` (`0x18000d3c0`) strides by `0x18`.

So this is **six ctor/copy-ctor/dtor bodies over already-pinned layouts**, not a
layout-recovery job. That is why it is scopeable as one wave. Wave 11 follow-up 2
was blocked on exactly the opposite (layout absent, ctors exported) and cost a
full lane; this is the cheap direction.

## What this wave will NOT do — state it before the numbers arrive

**It will barely move Video's percentage, and that must not read as failure.**
Video's 41-row gap decomposes:

| | rows |
|---|---|
| DATA (`??_7` / `?_typeInfo@` / `?_singleton@`) | **25 (61%)** |
| ctor/dtor | 10 |
| CODE | 6 |

The six blockers are inside the ctor/dtor tranche. **The 25 data rows are lane
CEIL's subject (Wave 17, running now)** — and after Object, the live hypothesis
is that a large share of them are unpromotable NO-OPs, in which case Video's real
ceiling is well under 100% and its "72.7%" is an understatement of how finished
it is. **Do not scope this wave against Video's percentage; scope it against the
boot.**

### MEASURED (Wave 17 lane CEIL, merged) — and it refutes the hypothesis above

The "live hypothesis" in the paragraph above was the integrator's, and it was
**right for Video and wrong for IO and Graphics**:

| namespace | ported | **ceiling** | unpromotable | **real work left** |
|---|---|---|---|---|
| Video | 72.7% | **88.0%** | 18 (12 NO-OP + 6 HARMFUL) | **23** |
| IO | 69.2% | **97.5%** | 3 (all HARMFUL) | **34** |
| Graphics | 66.7% | **96.0%** | 4 (all HARMFUL) | **29** |

IO's and Graphics' data rows are **BLOCKED on unported vtable slots — ordinary
port work, not a ceiling.** So the raw-gap ranking that put Video first
(41 > 37 > 33) is **backwards** by real remaining work: **IO 34 > Graphics 29 >
Video 23.**

**This does not change Wave 18**, because Wave 18 is scoped against the *boot*,
not the census — and that separation is exactly why it survives the correction.
Video is now the SMALLEST census target of the three and still the ONLY cutover
target. But it does mean: **do not follow Wave 18 with "more Video" on census
grounds.** IO and Graphics are the larger real gaps.

The 12 Video NO-OPs are the `??_7*Data@Video@@6B@` companion bases — our ported
ctor stamps them through a registry-first door and `dx11/`'s factory overwrites
the stamp on the very next statement (the dtor's stamp is followed only by
`MemFree`). That is the `??_7DialogController@UI@@6B@` shape, third instance.

**The lesson the lane paid for, worth carrying into any ceiling work: BLOCKED is
not a ceiling.** It predicted 40 NO-OPs and measured 12, because it reasoned
about whether promotion would *matter* without first asking whether it was
*possible* — **22 of 26 vtable rows carry abort traps and are simply unfinished.**
BLOCKED absorbed almost exactly that error (5 predicted, 35 actual).

## The residue is a LOWER BOUND, and it has already changed

Attempt 24 recorded 17 distinct residue symbols and said so explicitly: a cutover
boot is a **GATE, not a PLANNER** — it can only name what it reached before
faulting, and it faulted after 82 stderr lines.

**Two of those 17 have since been closed by Wave 11 follow-up 2**:
`?SaveText@External@Object@@` is ported and registered; `?FindOrLoad@ExternalFactory@Object@@`
was ported by lane OBJEXT. So the residue set has moved even though the wall has
not.

**Requirement: any new attempt must do a SET DIFF, not a count.** Attempt 24
refused to compare its 17 with Wave 10's 17 for exactly this reason — Wave 10
saw residue grow 17 → 25 and correctly called the growth *progress*, because the
entrants were the closed body's own callees. A count returning to 17 is not a
return to that state.

## Suggested lane split

1. **Lane GFXIF** — the six blocker bodies over the pinned layouts, plus
   registration by **name and address** (13 live sites reach Runtime symbols
   through `ResolveAddr(Module::Runtime, …)`, which is registry-first).
2. **Lane GATE** — re-run the cutover boot gate on the tip, with a **set diff**
   against attempt 24's 17, and record the new wall. Runs *after* GFXIF merges;
   its job is to say whether the wall moved and to where, not to port.

Lane GATE should also carry attempt 24's own warning forward: **scope the wave
after it with three separate bounded screens** — predicted walls (upper bound),
residue (lower bound), census gap (lossy in both directions) — and partition by
class family, rather than treating the residue as the work list.

## Standing doctrine for both lanes

Restated because each has cost this project a wave, and several were earned in
the last series:

1. **Prose is a hypothesis; a grep is the answer.** Grep the mangled name AND the
   retail address AND for `RtOptIn(`/`RtWaveEnabled(`/`if (table)` gates before
   writing anything. The tree itself printed *"deliberately NOT ported"* for four
   ported functions.
2. **A census `unported` row has four meanings** — absent / gated / unregistered
   / storage-never-written. They imply completely different work.
3. **An instrument outside its subject's live window prints a plausible ZERO, not
   an error.** Sample from the frame tick; give every screen a positive control
   that MUST read non-zero, and verify it prints before trusting a zero. Four
   instruments in the last series would have lied.
4. **`BANISHED_AUTOQUIT` overrides `BANISHED_AUTOSTART` AND `BANISHED_AUTOLOAD`**
   — it replaces them, it does not compose.
5. **A control must produce a comparable divergence, not UB**, must be scored
   **separately** with its catch rate, and must catch **via the mechanism it
   names**.
6. **Do not "fix" a faithful crash with a null check** — find the upstream null.
7. `tools/game_dir_lock.sh acquire <lane>` — **never piped**, check its OWN exit
   status; it now has `assert-held <lane>` to gate a deploy on.
8. Build **both** `standalone-debug` and `standalone-perf`; `make check-log-strip`
   fails the stripped builds on qualified `std::printf`.
9. **Commit early and often** — a machine restart destroyed one lane's entire
   uncommitted session in the last series.
