# Wave 9 — Lane R: repairs

Round 2, 2026-09-07 on `wave/upgrade-path-complete`, on top of `551d3b8`, `199e9e4`,
`357221a`. Two defect sets, both found in round 1 and both latent-but-real, plus the
question set 1 raised: **are there more?**

Everything below was measured. The exe used for every run was built at **20:53:18** and
deployed at **20:54:32**; every file another round-2 lane has in flight is stamped
20:55–20:58, i.e. **after** the deploy, so nothing in these numbers is attributable to
work I do not own (`reference_lane_edits_after_report_invalidate_the_measurement.md`).

## Headline

| Item | Before | After |
|---|---|---|
| Live port bypasses (`tools/port_bypass_audit.py`) | **5**, not 3 | **0** |
| Defective resolve fallbacks (`tools/resolve_fallback_audit.py`) | 5 of 166 | **0 of 163**, exit 0 |
| `[RtResidue]`, gameplay cell | 306 | **303** — the 3 bypasses gone, nothing else moved |
| Census (venv) | 830 called / 512 ported | **825 / 512**, 62.1% |

Two new standing checkers/fixes landed: `tools/port_bypass_audit.py`, and a blind spot in
`tools/resolve_fallback_audit.py` that this lane triggered with its own comment.

---

## SET 1 — the port bypasses. There were five, and one was a latent crash

### The sweep

`tools/port_bypass_audit.py` (new) asks, from source: *does any call site ask
`ResolveAddr(Module::Runtime, 0x…)` for an address the shipped DLL exports under a name we
have ALREADY ported and registered?* That set is exactly "ports that are bypassed". It
reuses `runtime_census.py`'s scanner (so the two tools cannot disagree about what a live
registration is) and `resolve_fallback_audit.py`'s pairing logic, and exits 2 rather than
answering if the DLL is not there to map addresses onto names.

**347 by-address Runtime resolve sites. 5 bypasses**, not the 3 lane 0 found:

| Site | Address | Name | Reached in lane 0's matrix? |
|---|---|---|---|
| `code/Game/Tutorial/tutorialgoaldialog.cpp:40` | `0x1800b9960` | `??0Serializer@IO@@QEAA@AEAVStream@1@H@Z` | yes, all 9 cells |
| `code/Game/Tutorial/tutorialgoaldialog.cpp:41` | `0x1800b99b0` | `??1Serializer@IO@@QEAA@XZ` | yes, all 9 cells |
| `code/Game/Component/herdcomponent.cpp:81` | `0x1800c7d40` | `?RandFloat@Random@System@@QEAAMXZ` | yes, all 9 cells |
| **`runtime/src/graphics/scene.cpp:265`** | `0x18001e5f0` | `?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ` | **no** |
| **`code/Game/Component/growthcomponent.cpp:1027`** | `0x1800c7cf0` | `?Serialize@Random@System@@…` | **no** |

The two lane 0 could not see are the two the nine-cell matrix never executes — which is the
point of having a source checker beside the process one. Residue answers "what did this run
ask the DLL for"; the audit answers "what would ask, if reached".

### The first correction to the brief: the sweep says 35 before it says 5

A first version of the checker reported **35**, and 30 of those are the fallback arm of

    fn = ResolveExport("?Mangled@…");   // <- this reaches the port
    if (!fn) fn = ResolveAddr(Module::Runtime, 0x…);   // dead

The primary asks by name, so the port IS reached and the address branch never runs. Those
belong to set 2 / lane F (all 163 must be deleted before cutover), not here. **A bypass is
an UNGUARDED by-address call**, and the checker now says so by construction rather than by
my reading of a list. Worth recording because 35 would have been published as an
order-of-magnitude finding.

### `growthcomponent.cpp:1027` is not a bypass. It is a wrong address that would fault

This is the one thing in this lane that is worse than the brief predicted.

The line asked for `0x1800c7cf0` through `uint32_t(*)(void* self, uint32_t n)`, with a
comment saying `System::Random::Rand(this, range)`. But:

* `0x1800c7cf0` is **`?Serialize@Random@System@@QEAAXAEAVSerializer@IO@@@Z`**. Read out of
  the DLL, its second instruction pair is `MOV RCX,[RDX+8]; MOV RAX,[RCX]; CALL [RAX+0x98]`
  — it dereferences RDX as a `Serializer`. The call passed `range`, a small integer, in RDX.
* Retail does not call it. `FUN_14011fba0` at `0x14011fe2c` — between the two exe calls this
  code already reproduces, `0x140125320` and `0x1401253d0` — is `CALL [0x1401512e8]`, and
  `tools/dispatch_slot_map.py 0x1401512e8` says
  `?Rand@Random@System@@QEAAII@Z`, which lives at **`0x1800c8040`**.

So the old line was one execution away from an access violation, and it survived because
**this statement has never executed** in any of lane 0's nine cells (its `static` initialiser
would have recorded `addr:0x1800c7cf0` in the residue; it appears in none). Fixed to
`ResolveExport("?Rand@Random@System@@QEAAII@Z")`, which corrects the address and reaches our
Wave 2 port in one move.

That is `reference_uncalled_port_is_not_verified.md` from a fourth angle: not a port with no
callers, not a port whose callers go around it, but **a caller no run reaches, holding a
wrong address that no oracle can see.**

### The fix, and why it is the call sites

All five now ask by NAME. Not a fallback from `ResolveAddr` to the export map: that would
still need the shipped DLL's export directory to turn an address into a name, so it cannot
work on the day the DLL is deleted — and it would leave the call sites reading as if asking
the DLL were fine.

`runtime/src/graphics/scene.cpp` needed one more thing: **the runtime tree had no
port-aware name resolver at all.** `runtime/funcs/rt_port.h` deliberately re-declares the
seam rather than including `standalone/resolve.h`, and it declared only `ResolveExportRaw`
— which is `GetProcAddress` and skips the port registry on purpose, for the diff oracle.
So a `runtime/src` file that wanted to CALL something had nothing to use but an address.
`ResolveExport` is now declared there too, with the difference between the two spelled out.
That absence is *why* one of our ports was bypassing another inside `runtime/`.

`tools/port_bypass_audit.py` now reports **0**, exit 0.

---

## SET 2 — the five defective fallbacks

Each verified against the shipped export directory (`pefile`) and, where the verdict turned
on it, against the disassembly — not taken from lane D's table on faith. All five of lane
D's verdicts held.

| # | Site | Action | Evidence I re-took |
|---|---|---|---|
| 1 | `engine_funcs.cpp:2855` `FindCheckBoxes` | **corrected to `0x180047c00`**, with the reasoning in a comment above it | The export directory contains exactly two `FindCheckBoxes` manglings: `…QEAAXXZ` (public, 0 args) at `0x180047c00` and `…AEAAXV?$InternalPtr@…` (private, 1 arg) at `0x1800d5750`. The old fallback was the second. It is a real entry point, so it would have RUN with garbage in RDX. |
| 2 | `engine_funcs.cpp:3144` `GetPath@External` | **corrected to `0x180006250`** | `0x180006250` is the export; disassembled, it is 15 instructions. `0x18006a3e0` is not an export address (mid-body of `FUN_18006a390`). |
| 3 | `fencedtool.cpp:373` `Abs@Scalar@Math` | **fallback deleted** | The name is a registered Wave 1 port and `ResolveExport` checks the registry first, so the branch cannot fire unless the registration is gone — at which point null is the better failure. Both call sites are already `if (ptr && …)` guarded. |
| 4 | `fencedtool.cpp:378` `Vector3(MMM)` | **fallback deleted** | Same, same two guarded call sites. |
| 5 | `tool_base_toolui.cpp:96` `~DialogInstance` | **else arm deleted** — see below | |

### Site 5: the shared assumption, checked before touching it

The brief was right that this one is not local. The decision, and the reason:

`fn_DialogInstance_Dtor` is used at **21 slot-0 wrappers** across `code/Game/Dialog/` and
`code/Game/Player/`. Twenty of them are exactly `if (fn_DialogInstance_Dtor)
fn_DialogInstance_Dtor(self);` — **guard, and on null SKIP.** `tool_base_toolui.cpp` was the
only one with an `else` arm, and that arm computed `0x180000000 + 0x1b800e`, which lands in
`.rdata`: the one case it existed for would have executed read-only data.

So the shared idiom is not "fall back", it is "guard and skip", and this site was the
outlier. Deleting the arm makes it the twenty-first identical site. Correcting it to
`0x180114d40` was the alternative and is worse: `fn_DialogInstance_Dtor` is assigned in
**exactly one place** (`engine/funcs/engine_funcs.cpp:1837`), which already carries its own —
verified correct — `0x180114d40` fallback, so a second copy here can only fire after that one
has already failed, and would be one more of the by-address branches lane F must delete.

`tools/resolve_fallback_audit.py`: **163 sites, 0 defects, exit 0.**

### The count check — and the checker defect it caught

166 → 163 is exactly the three fallbacks deleted (2 `fencedtool`, 1 `tool_base_toolui`),
with **0 gained and nothing else lost**, confirmed by diffing the audit's CSV before and
after on `(file, export)`.

It did not start that way. The first post-edit run said **162**, and the missing row was
`FindCheckBoxes` — **the site I had just fixed.** Cause: `find_sites()` looked for the
null-guard in a fixed 6-line window above the statement, and the twelve-line comment I added
explaining the fix pushed the `if (!fn_…)` out of it. The site did not become a defect; it
**vanished from the audit entirely.**

That is the worst possible failure mode for this class of tool — *documenting a fallback
deleted it from the checker*, and a dropped site is indistinguishable from a fixed one. The
window now counts only code lines, skipping blanks and `//` comments, and the reason is in
the source. Without the before/after CSV diff the brief asked for, this would have read as
"163 sites, 0 defects" and been a clean pass.

---

## Verification

### Builds

* `mingw32-make standalone-debug` — clean. (First attempt failed: `ResolveExport` was not
  declared in `runtime/src`; that is the missing declaration described above, now added.)
* **Log-stripped config — clean.** `mingw32-make standalone-perf` compiled the whole tree
  with `-DBANISHED_STRIP_LOGS -include standalone/log_strip.h` (the same
  `STRIP_LOGS_FLAGS` `make standalone` uses) at 20:54, before any other lane's files
  existed, and linked. No `std::printf` anywhere in the tree (`grep`ed; the three hits are
  the header and two comments about it).
* **`mingw32-make standalone` does NOT link at the moment, and it is not this lane's
  change.** It fails with `undefined reference to RtCollectionProbeRun()` from
  `runtime/src/collection/array.cpp` — an **untracked file stamped 20:58**, written by
  another round-2 lane, whose `array_probe.cpp` does not exist yet. Every source file
  *compiled*, including all six I edited under the strip flags; the failure is at link and
  is a missing definition, not a log-strip fault. `standalone/build/` has been removed
  rather than left holding `-O3` objects that a later `make standalone-debug` would silently
  reuse (the Makefile documents that trap).

### Checkers

| Tool | Result |
|---|---|
| `tools/dispatch_slot_map.py --check` | 62 registrations, **0 mismatches**, 0 shadowed |
| `tools/export_name_audit.py` | 652 names, **0 phantoms** |
| `tools/resolve_fallback_audit.py` | 163 sites, **0 defects**, exit 0 |
| `tools/port_bypass_audit.py` (new) | 347 by-address sites, **0 bypasses**, exit 0 |
| `tools/runtime_census.py` (venv) | **825 called / 512 ported / 62.1%** |

### The census moved, and every point of it is accounted for

**830 → 825 called**, ported unchanged at 512, coverage 61.7% → 62.1%. Five distinct
functions left the "called" column, and they are exactly the five addresses this lane stopped
asking for:

* `0x18008f530`, `0x18008f880` — the two deleted `fencedtool` fallbacks (mid-body addresses,
  bucketed `internal`).
* `0x18006a3e0` — the corrected `GetPath` fallback; its replacement `0x180006250` folds onto
  a name already counted.
* `0x1800d5750` — the private `FindCheckBoxes` overload; its replacement folds onto the
  public one, already counted.
* `0x1800c7cf0` — `Random::Serialize`, which **nothing in our tree calls any more**. It was
  only ever on the list because of the wrong address in `growthcomponent.cpp`.

The three by-address→by-name conversions (`Serializer` ctor/dtor, `RandFloat`) moved no
number, because the census already folded those addresses onto their names — which is
precisely why it called them ported while the process asked the DLL.

### The bypasses are closed — by measurement

`BANISHED_RT_STRICT=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120`
(a route that reaches gameplay; `BANISHED_AUTOQUIT=1` quits at the main menu and has already
cost this project one wrong published finding). Clean exit, no crash log,
`[startTown::…]` lines confirm the world was built.

**306 → 303 distinct requests.** Diffed against lane 0's `wave9_residue_gameplay_pause.txt`,
the change is exactly:

```
lost:  addr:0x1800b9960   ??0Serializer@IO@@…
       addr:0x1800b99b0   ??1Serializer@IO@@…
       addr:0x1800c7d40   ?RandFloat@Random@System@@…
gained: (none)
```

Raw block: `docs/runtime_port/wave9_laneR_residue_gameplay.txt`.

### The ports are reached AND correct

`BANISHED_RT_DIFF=1`, same route, raw log read (no `tail` —
`reference_truncated_tool_output_manufactures_calls_zero.md`):

```
IO::Serializer::Serializer(Stream&,int)   calls=3633       divergences=0
IO::Serializer::~Serializer               calls=3633       divergences=0   [not diffable]
System::Random::RandFloat                 calls=222851     divergences=0
System::Random::Rand(uint)                calls=16028716   divergences=0
Math::Transform::AsMatrix()               calls=228763     divergences=0
```

Whole run: **250 sites, 27,817,475 calls, 83 divergences in 2 sites** — and those 2 are the
pre-existing Wave 1 one-ULP `sin`/`cos` family (`Transform::FromYawPitchOffset` 68,
`Vector3::FromYawPitch` 15) that every wave since 1 has recorded. Nothing else diverged.

**Stated honestly:** those `calls=` counts prove the *ports* execute and agree with the DLL;
they aggregate all callers, so they do not by themselves prove the two newly-fixed sites
(`growthcomponent`, `scene.cpp`) executed. Neither appears in the residue before or after,
so this cell does not reach either — the same fact that let the `growthcomponent` wrong
address survive. What is proved for those two is static: the address is now the one retail's
own import slot names, and it goes through the port.

## Which DOOR the fixed sites use — asked after the fact, answered by measurement

`25f78a5` lit three previously dark resolve doors, and raised a fair question about this
lane's own fix: a by-name resolve that goes through `ResolveExportRaw` is still a DLL
request — it just files under `[RtRawResidue]` instead of `[RtResidue]`, so a clean
`[RtResidue]` would be measuring the fix's bookkeeping rather than its effect. Lane 0 found
**34 already-ported functions in exactly that state.**

**They use `ResolveExport`, the lit port-aware door, and they HIT it.** Three independent
lines of evidence, because the first two alone would not settle it:

1. **Source.** `ResolveExport` (`standalone/resolve_standalone.cpp:581`) checks
   `RtExportReg()` first and returns our function on a hit. On a MISS it calls
   `RecordRuntimeResidue(name, …)` and then `ResolveExportRawImpl` — the *un-instrumented*
   body. So a miss is visible, and it is visible in `[RtResidue]` under the **mangled name**,
   never under `raw:`.
2. **Measurement.** No name key for any of the five converted sites appears anywhere in the
   303-row `[RtResidue]` list. Since a miss would have printed one, every one of them is a
   port-registry hit. This is the same fact as "three `addr:` rows disappeared and nothing
   was gained", read from the other side — and it is why the disappearance is evidence of a
   HIT rather than merely of a changed spelling.
3. **The `[RtRawResidue]` block, checked by name.** 77 rows this cell. Two of them are
   `raw:??0Serializer@IO@@…` and `raw:??1Serializer@IO@@XZ`, count **1** each, both from
   return address `…7286`. Symbolised against the exe (`nm`, load base `0x7ff79a7d0000`,
   delta 0) that is **`VerifyBody+0x20`** — the registration-time structural check inside
   `runtime/src/io/serializer.cpp`, i.e. lane 0's harmless category, **not** the tutorial
   dialog. `?RandFloat@…`, `?Rand@…QEAAII@Z` and `?AsMatrix@Transform@Math@@` appear in
   neither block.

Raw block: `docs/runtime_port/wave9_laneR_rawresidue_gameplay.txt`.

## The lazy-`static` timing hazard: real in principle, absent here, and left alone

Both bypass sites cache in a function-local `static` inside an `EnsureResolved()` /
`EnsureHelpers()`, so converting them changes *when* they resolve relative to
`RuntimePortInit`, not only *which* registry answers. If either helper could run first, a
by-name resolve would cache the DLL's function **forever** — the residue line would vanish
and the port would still never run, which is a worse outcome than the bug.

It cannot happen here, structurally and then measurably:

* `RuntimePortInit()` is called at `standalone/main.cpp:1655`, **before** `EngineFuncsInit`
  and before any game, dialog or component code exists. The two helpers hang off a tutorial
  dialog's construction and a herd component's tick; neither can precede it.
* And it did not happen: a pre-`RuntimePortInit` resolve would have been a registry MISS,
  which prints the mangled name in `[RtResidue]`. No such row exists (evidence 2 above).

So the caching scheme is **unchanged** — deliberately. Re-checking the cache or routing
through a different path would be a behaviour change with no defect behind it, and a fresh
`static` that provably resolves after `RuntimePortInit` is the same shape every other port
call site in the tree uses.

## Reproduced on a pinned build

The numbers above were re-measured on a **clean worktree of `6dd6354`** (built 21:05,
deployed 21:06), so that no other lane's in-flight edits could enter them —
`runtime/src/collection/`, `rt_port.h` and `resolve_standalone.cpp` were all being modified
in the main tree at the time. The `[RtResidue]` set came back **byte-identical** to the
20:54 run: 303 rows, same members. The residue is a property of the source, which is what
lane 0 measured independently on its own two passes.

## Files

Changed: `code/Game/Component/growthcomponent.cpp`, `code/Game/Component/herdcomponent.cpp`,
`code/Game/Tutorial/tutorialgoaldialog.cpp`, `code/Game/Player/fencedtool.cpp`,
`code/Game/Player/tool_base_toolui.cpp`, `engine/funcs/engine_funcs.cpp`,
`runtime/funcs/rt_port.h`, `runtime/src/graphics/scene.cpp`,
`tools/resolve_fallback_audit.py`.
Added: `tools/port_bypass_audit.py`, `docs/runtime_port/wave9_port_bypasses.csv`,
`docs/runtime_port/wave9_laneR_residue_gameplay.txt`,
`docs/runtime_port/wave9_laneR_rawresidue_gameplay.txt`, this file.
Regenerated: `docs/runtime_port/wave9_resolve_fallbacks.csv` (163 rows, all CORRECT).

## For lane F

The 166-site item is now **163**, and three of them were retired by deleting rather than
correcting — which is what lane F must do to the other 163 anyway. Two of the three deletions
were possible only because the name is a registered port; that is the general rule for the
rest: **a fallback whose primary names a port is already dead, and can go now.**
`tools/port_bypass_audit.py --all` lists them as `GUARDED-FALLBACK`.
