# WAVE 18 — LANE GFXIF

**Worktree:** `C:/code/GIT/wt-w18-gfxif`
**Branch:** `wave18/gfxif` (cut at `d12e8be`)
**Subject:** the nine `Video::Device` enumeration rows — the six cutover
boot-wall blockers plus `AdjustDisplay`, `FindOrAddMode`, `GetDisplayMode`.

---

## 1. Headline

**All nine rows ported, registered by name AND by address.**

`tools/runtime_census.py --namespace Video` on this branch:

| | called | ported | % | ceiling |
|---|---|---|---|---|
| at `dacc015` (wave start) | 150 | 109 | 72.7% | 88.0% |
| **on this branch** | **150** | **118** | **78.7%** | 88.0% |

**+9 ported, and `called` did not move.** That second half matters: a faithful
port that reaches DLL internals through a name door grows the `called` column
and *lowers* the percentage while doing real work — that is exactly what
happened to UI in Wave 10B (170→179 called, 96.5%→95.0%). It did not happen
here, because every internal this cluster needs was reached by ADDRESS, and
eleven of them are now ported outright.

Remaining to ceiling: 88.0% − 78.7% = **14 rows**, which is exactly lanes VRES
(7) and VDATA (7). The kickoff's 23-row arithmetic reproduces.

---

## 2. Per-row verdict

Retail addresses were read from the shipped export directory with
`tools/pe_exports.py` **before any body was written**, not copied from the
brief. All nine matched the brief's table.

| # | symbol | retail | ported | reg. by name | reg. by addr | verdict |
|---|---|---|---|---|---|---|
| 1 | `??0GraphicsInterface@Device@Video@@QEAA@XZ` | `0x18000cd90` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 2 | `??0GraphicsInterface@…@AEBU012@@Z` | `0x18000ce10` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 3 | `??1GraphicsInterface@Device@Video@@QEAA@XZ` | `0x18000cdc0` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 4 | `??0Adapter@Device@Video@@QEAA@XZ` | `0x1800f7100` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 5 | `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | `0x18000cc70` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 6 | `??1Adapter@Device@Video@@QEAA@XZ` | `0x18000cc10` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 7 | `?FindOrAddMode@Adapter@Device@Video@@…` | `0x1800f7020` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 8 | `?GetDisplayMode@Device@Video@@QEBA?AV…@H@Z` | `0x18000d3e0` | yes | yes | yes | **VERIFIED** — oracle §4 |
| 9 | `?AdjustDisplay@Device@Video@@QEAAXXZ` | `0x1800f7e20` | yes | yes | yes | **PORTED, NOT BYTE-VERIFIED** — §5 |

Files:
* `runtime/src/video/device_enum_types.cpp` — rows 1–8 and eleven Array helpers
* `runtime/src/video/device_adjustdisplay_win.cpp` — row 9 (Win32, `_win` twin)
* `runtime/src/video/device_enum_types_probe.cpp` — the oracle
* kill switch `BANISHED_RT_VIDENUMTYPES`, probe `BANISHED_RT_VIDENUMTYPES_PROBE=1`

### Grepped before writing, per doctrine 1

The nine mangled names AND the six retail addresses across `code/ engine/
runtime/ standalone/ dx11/`: the only hits were two descriptive comments in
`runtime/types/video/Device.h` and the DX11 seam's own import table at
`dx11/src/devicedata.cpp:237-249`. Neither is a port. No `RtOptIn(` /
`RtWaveEnabled(` / `if (table)` gate on any of them. **Genuinely absent, not
ported-but-unregistered.**

---

## 3. The two traps in the brief — both resolved, and one of them found more

### Trap 1: `??0Adapter` at `0x1800f7100`

**It is the real body, not a thunk.** Fourteen instructions, disassembled at that
address: zero `+0x00..+0x27`, store `-1` as a QWORD across `+0x28..+0x2f`, `-1`
as a DWORD at `+0x30`, return `this`. `+0x34` is not written. The address split
from the other five is just which translation unit MSVC emitted each into — the
`0x18000cxxx` cluster is a COMDAT region of small value-type bodies, and
`0x1800f7xxx` is the `Video::Device` code region.

### Trap 2: the DX11 address column

Confirmed as described and **not touched**. Every value in that column falls in
`0x180013218`–`0x1800134c8`; it is VideoDX11's own IAT slot range.

### What reading that file for the trap ALSO produced — the wall's mechanism

`dx11/src/devicedata.cpp:216-300` is the whole causal chain, and it is worth
stating because it changes what "fixing the wall" means:

```
GetImports()  resolves 40 entries; ANY null  ->  g_imOk = false
DeviceData_EnumerateInterfaces():
    const Imports* im = GetImports();
    if (!im) return Orig<bool(*)(DeviceData*)>(1)(self);   // <- null: VCALL-of-0
```

and `Dx11ResolveRuntimeImport` (`dx11/src/dx11_backend_access.cpp:9`) tries
**`ResolveExport(mangled)` FIRST**. So:

* the wall is a **name-resolution** failure, not an address one;
* **registration by NAME is the registration that moves it** — registering by
  address alone would have left the wall exactly where it is;
* the wall needs **all nine** names, not the six the plan named, because
  `g_imOk` is an AND over the whole table. The kickoff put all nine in this lane
  for cohesion reasons; that grouping turns out to be *load-bearing*, not
  cosmetic. **Splitting rows 7–9 into another lane would have left the wall
  standing while every row in it was green.**

---

## 4. The oracle, and why its green means something

`runtime/src/video/device_enum_types_probe.cpp`. Lockstep on private subjects:
**ours runs on A, THEIRS runs on B, A and B compared field by field.**

* **Theirs is reached through `RtOriginal` (`ResolveExportRaw`), never
  `ResolveExport`.** This lane registers all nine names, so the registry-first
  resolver would hand back our own port and the probe would compare ours to ours
  and agree forever.
* **Coverage is wider than the eight subjects.** All nine rows are exported, and
  their calls to the unexported `Collection::Array` instantiations are in-DLL
  `rel32`s we neither do nor can intercept. So THEIRS runs the DLL's array
  helpers while OURS runs the ported ones — the compare covers **all eleven
  array instantiations transitively**, none of which has a name to be called by.
* **By CONTENT, not `memcmp`.** Every array is a separate allocation on the two
  sides. And `Adapter::field34` is written by *neither* implementation, so a
  `memcmp` would compare bytes nobody writes and fail 100% of calls.
* **Population.** The destructor's two paths are selected by the String
  refcount and differ by **one instruction**; both are driven, plus the null
  early-out. `FindOrAddMode` both finds and adds, for modes and for refresh
  rates. Adapter lists of 0/1/2/3 exercise the growth rule.
* **Every effectiveness counter is measured on THEIRS**, so no counter can be
  satisfied by our port agreeing with itself.
* **Four negative controls, SCORED SEPARATELY**, each aimed at one named screen
  and injected by the mechanism it names (a control scored on the union of
  several screens reads 4/4 and hides that one screen caught 1/4):
  * NC1 → the poisoned-block ctor compare (writes `+0x34`)
  * NC2 → the `FindOrAddMode` content compare (unfilled new element)
  * NC3 → the dtor post-state compare on the **free path** (nulls the slot)
  * NC4 → the copy-ctor scalar compare (drops `+0x30`)

> **Run status: the oracle is written, built and armed, but had not been RUN at
> the time this lane reported** — `tools/game_dir_lock.sh acquire gfxif` was
> blocked behind lane VRES for the whole reporting window (see §7). The verdicts
> in §2 marked VERIFIED are **pending that run**. Anyone merging this branch
> should run it first; the numbers it prints, not this sentence, are the
> evidence. This is stated plainly rather than smoothed over, because a
> registration is not a verdict and an unrun probe is not an oracle.

---

## 5. Row 9 — `AdjustDisplay`, and why its verdict is different

`AdjustDisplay` moves and restyles the **live application window** and writes
the `Video::Configuration` singleton. There is no scratch copy: the subject is
an HWND owned by the OS, and running ours then theirs would start the second run
from the state the first left. So it gets no byte oracle, and saying otherwise
would be the "no oracle possible" claim this project has been wrong about
before — the honest form is:

**Class: PORTED, NOT BYTE-VERIFIED. Expiry condition:** it becomes verifiable
the moment a *synthesised* subject exists — a hidden `CreateWindowExW` of the
same class and style, with a private `Video::Configuration`, driven through both
implementations and compared on (window rect, client rect, style, the four
config fields). Nothing in the body needs the real window; it needs *a* window.
That is a bounded piece of work and it is the natural first cell of a follow-up.

What IS established for it without a run:

* **Every one of its ten Win32 import slots was resolved BY NAME** from the
  shipped import table with `pefile` — not inferred from the call shape:
  `GetWindowLongW` `SetWindowLongW` `GetMonitorInfoW` `SetWindowPos`
  `GetWindowPlacement` `AdjustWindowRect` `MonitorFromRect` `GetDesktopWindow`
  `GetWindowRect` `GetClientRect`. Note `GetWindowLong**W**`, not
  `GetWindowLongPtrW`: the style is a 32-bit LONG throughout.
* it is on the boot path and will be exercised by any successful boot.

---

## 6. What my measurement CONTRADICTS or ADDS to the brief

Stated plainly; lanes correcting the brief is the norm here.

1. **The plan's "six functions" is a six-of-nine.** `WAVE18_VIDEO.md` §"The
   wall: six functions" names six blockers. The wall needs **nine** — `g_imOk`
   is an AND over the whole `devicedata` import table, and `AdjustDisplay`,
   `FindOrAddMode` and `GetDisplayMode` are entries in it. Attempt 24's own
   residue listing already named all nine under `Video::Device`; the plan's
   headline narrowed it to six. Nothing was harmed, because the kickoff put all
   nine in one lane anyway — but a lane scoped to the plan's six would have
   finished green and left the wall standing.

2. **`runtime/types/video/Device.h` says `Adapter::field2C` and `field30` are
   "meaning not recovered". They are recovered, and have been.**
   `dx11/src/devicedata.cpp:485-486` names them `dxgiAdapterIndex` and
   `dxgiOutputIndex`, written by the shipped DX11 `EnumerateInterfaces`
   (`rec.adapterIndex` / `rec.outputIndex` at `devicedata.cpp:1750-1751`). Two
   files in this tree disagree about the same two fields. No layout changes, so
   this lane edited neither; recorded here so the next reader of Device.h does
   not re-derive it.

3. **`Device+0xA0` is an `HMONITOR`.** Device.h covers it with `gap9E[0x12]` and
   records only "the ctor zeroes +0xa0 and +0xa8". `AdjustDisplay` loads it at
   `0x1800f7e68` and passes it straight to `GetMonitorInfoW`, which pins it.

4. **Four of the six array helpers the six ctors/dtors need were ALREADY
   PORTED**, by Wave 9 lane VID2, in `runtime/src/video/device_startup.cpp`
   (`0x18000fcd0`, `0x180008b00`, `0x180010830`, `0x180009560`, plus
   `0x180010cf0` and `0x18000fbf0`). Neither the plan nor the kickoff mentions
   this, and it is most of the reason this lane was cheap. Porting a second copy
   would have been last-binding-wins on the address registry and would have
   silently made one of the two bodies dead code. **This is the fourth instance
   of "grep for the port before planning to write it" paying.**

5. **A retail defect in `AdjustDisplay`, reproduced deliberately.** The adjusted
   RECT is offset by the persisted window origin, and retail adds the **X**
   offset to three of the four edges — `bottom` gets `EDI` (windowX), not `ESI`
   (windowY), at `0x1800f7f9c`. The rect feeds only `MonitorFromRect`, so the
   consequence is a monitor lookup against a slightly wrong box: invisible on
   one monitor, and harmless whenever windowX ≈ windowY. Reproduced, not fixed —
   a "corrected" version would pick a different monitor from retail in exactly
   the multi-monitor case where someone would notice.

---

## 7. Four things the DISASSEMBLY settled that decompiled C would not

Doctrine 7 earned its keep four times in one lane.

1. **`FindOrAddMode`'s two `SetSize` calls take THREE arguments.** `R8` still
   holds the incoming `mode` pointer at `0x1800f7088` (the search loop uses
   R9/R10/R11 and never touches R8), so `0x180057440` is
   `(array, count+1, mode)` — the new element is **filled**. A two-argument
   reading grows the array and leaves the new mode's width and height as
   whatever the allocator returned, so the mode never matches on any later call
   and the list grows without bound. This is the "drops whole arguments" defect
   class, and it is what negative control NC2 is aimed at.

2. **The in-loop String release carries one extra null test the standalone
   spelling does not.** At `0x180010e96`:
   `ADD qword ptr [RSI],-0x8` / `MOV RSI,qword ptr [RSI]` / `JZ skip` — the JZ
   reads the flags of the **ADD**, because a MOV sets none. So the free is
   skipped when the decremented pointer is exactly zero. One instruction, two
   spellings, and the difference is invisible in C.

3. **Both destructors leave the name slot pointing at the freed allocation base
   on the free path**, and null it only on the still-referenced path. It looks
   like a bug and it is retail. NC3 exists because "tidying" this is the single
   most tempting edit in the cluster.

4. **`GetDisplayMode` returns by VALUE**, so it has a hidden return slot in RDX
   and takes three parameters for a one-parameter method. A typedef that omits
   the slot writes through whatever was in RDX and still hands the caller a
   plausible answer — a wild write that verifies green.

---

## 7b. An instrument defect found while trying to use it — `abi_retslot_audit.py`

`?GetDisplayMode@` returns by value, so the hidden-return-slot hazard is exactly
what `tools/abi_retslot_audit.py` exists for. It was run. It printed:

    0 suspicious of 49 by-value-returning exports (enum returns excluded)

**That green says nothing about this lane, or about any runtime port.** The tool
hardcodes its subject set at `abi_retslot_audit.py:41-42`:

    CPP = os.path.join(REPO, "engine", "funcs", "engine_funcs.cpp")
    HDR = os.path.join(REPO, "engine", "funcs", "engine_funcs.h")

It reads those two files and nothing else. `runtime/src/`, `standalone/` and
`dx11/` are outside its window entirely — and a grep for by-value-returning
mangled names in exactly those three trees returns **131 distinct symbols the
audit has never looked at.** Its banner and its one-line output both read as
project-wide; nothing in either says "engine/funcs only".

This is the doctrine-9 shape precisely: *an instrument outside its subject's
live window prints a plausible ZERO, not an error.* It was one step from being
cited in §2 as evidence for row 8.

**It is not cited.** Row 8's return slot rests on three things that are actually
about row 8:
* the disassembly — RDX is the slot, spilled at `[RSP+0x10]` on entry and
  returned in RAX at `0x18000d434`;
* the probe asserts the returned pointer **equals the caller's slot** on both
  sides (`ro == &mo`, `rt == &mt`), which a stale-register write would fail;
* the probe **poisons both slots with `0x5A`** before the call, so a write that
  landed anywhere else would leave the poison and be caught by `SameMode`.

**Recommended (not done by this lane — it is wave-scoped work, not row work):**
widen the audit to the runtime/dx11 trees, or make it print its subject set
alongside its count. A one-line output that cannot say which files it read is
how a blind screen gets quoted as a green one.

---

## 8. Blocked, and what it cost

`tools/game_dir_lock.sh acquire gfxif` (never piped; own exit status checked)
was **blocked behind lane VRES for the entire reporting window** — 900s timeout
reached, holder still active. Three lanes contending for one game directory is,
as the kickoff predicted, this wave's real throughput ceiling.

Consequence, stated without softening: **everything in this lane is build-
verified and census-verified; nothing is run-verified.** Both
`standalone-debug` and `standalone-perf` build clean with no warnings from
either new file, and `make check-log-strip` passes. The oracle and the boot are
the two things still owed, and they need the lock, not more porting.

**Recommended order for whoever gets the lock next:**
1. `BANISHED_RT_VIDENUMTYPES_PROBE=1` with `BANISHED_AUTOQUIT` — read
   `cases=`, `mismatches=`, every effectiveness counter, and all four NC lines.
   **A zero effectiveness counter voids the branch it names; a MISSED control
   voids the screen it names.**
2. An A/B on `BANISHED_RT_VIDENUMTYPES=0` to confirm the switch actually
   gates — a gate whose quantity does not vary with the gated thing was never
   a control.
3. Only then lane GATE's cutover boot, with a **set diff** against attempt 24's
   17, never a count.

**Prediction, pre-registered here so GATE can score it:** the wall moves off
`DeviceData::EnumerateInterfaces`. It does **not** follow that the boot
completes — `EnumerateInterfaces` is a long body and the next null is as likely
to be inside it as after it. And per attempt 24's own warning, whatever residue
GATE records is a **lower bound**, not the next wave's work list.

---

## 9. Commits

* `637a4ce` — the nine bodies, eleven array helpers, registration
* `5ba94da` — the lockstep oracle
* final hash: see §10 of the lane's report message
