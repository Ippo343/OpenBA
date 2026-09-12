# WAVE 23 — lane VIDVT: all nineteen Video rows

Branch `wave23/vidvt`, cut from `wave23/base` at `a60351e3`. Private worktree.
Every figure below is **BRANCH-LOCAL** (kickoff rule 13); the integrator re-measures.

---

## 0. Headline

**Video's gap was nineteen rows. It is now eighteen, and every one of the eighteen
is a DATA row with a written verdict. There is no body work left in Video.**

* The one BODY row, `?SetupShaders@VideoProgram@Video@@`, is **ported, registered
  and VERIFIED** — 64/64 agreeing cases in an input sweep against the shipped
  export, all five of its dirty bits exercised, negative control caught 24/64.
* The twelve VTABLE rows are **not what the lane brief supposed** (§1) and their
  Wave 17 NO-OP verdict **survives re-measurement from the shipped bytes** (§2).
* The six DATA rows' Wave 17 HARMFUL verdict also survives, with reference tables
  re-derived by a second independent decoder (§3).
* **The one defect found is in the TAGS, not the verdicts**: all eighteen carried
  the expiry `NONE, EVER`, and for six of them our own source comments already
  said the opposite about the cutover route. Eighteen tags now declare a route
  scope and an addressee (§4).

Branch-local census, before → after: **Video 159 called / 140 ported → 159 / 141**;
TOTAL **1314 / 1137 (86.5%) → 1314 / 1138 (86.6%)**.

Video now prints `AT CEILING`. **Do not read that as "Video is finished."** The
ceiling has `called` in both halves and moves whenever anything opens a door
(`reference_the_ceiling_percentage_is_a_moving_target.md`); what it means here,
scored on the IDENTITY, is: *0 bodies, 12 stamp-only vtables deliberately left on
the DLL, 6 mutable statics deliberately left on the DLL, and 6 of those 18 flip to
REQUIRED at cutover.*

---

## 1. TASK 0 — the structural fact, and the brief was half right

The brief asked whether the twelve `*Data` rows are really VideoDX11 plugin
companion vtables that `dx11/` already owns, in which case the shape of the lane
changes. **They are the companion classes, and `dx11/` does own the
implementation — but these twelve symbols are not `dx11/`'s tables.**

Measured, `tools/pe_exports.py` over all three shipped binaries:

| binary | exports | carries any `??_7*Data@Video@@6B@`? |
|---|---|---|
| `Runtime-x64.dll` | 4926 | **yes, all twelve**, `0x180158a28`..`0x180158c30`, `.rdata` |
| `VideoDX11-x64.dll` | **2** (`CreateDeviceData`, `GetDeviceName`) | no |
| `Runtime-x64-profile.dll` | — | not on disk at `C:\GOG Games\Banished` |

So the class hierarchy is: **Runtime declares the twelve abstract bases and
exports their vtables; VideoDX11 derives from them.** Dumping each table out of
`.rdata` settles what they contain — slot 0 is a real `.text` function
(`0x180058590`..`0x180058b10`, exactly the scalar-deleting-dtor addresses
`tools/gen_deleting_dtor.py` already lists) and **every remaining slot in all
twelve tables is the same address, `0x18007ec28`** — one shared `_purecall`.

That is why the census prints `ours 1 / slots 1 COMPLETE` for all twelve while the
mint rows say 3, 4, 7 or 20 slots: the two are counting different things and both
are right. Slot counts, adjacent-export bound narrowed by the purecall run:

| row | table slots | non-purecall |
|---|---|---|
| `ContextData`, `DeviceData` | 20 | 1 |
| `ImageBufferData`, `IndexBufferData`, `VertexBufferData` | 7 | 1 |
| `ConstantBufferData`, `RenderBufferData` | 4 | 1 |
| the other five | 3 | 1 |

All twelve agree with the counts written into the Wave 17 tags (`1/1 (19
purecall)`, `1/1 (6 purecall)`, …), independently re-derived here.

**So this is NOT twelve vtable ports.** It is twelve one-slot tables whose only
real entry is a deleting destructor **we already own** (`deleting_dtor.cpp`
registers `0x180058590`/`0x180058a90`/`0x180058b10` and their siblings by
address), and whose only *writers* are the twelve base ctors and ten base dtors
**we already ported** in Wave 9 (`runtime/src/video/companion_data.cpp`). The
lane's real question was never "port these" — it was "promote these, or not".

### 1.1 Extent — which bound, and the one that would have lied

`.pdata` is a **LOWER** bound on a function extent (MSVC chains fragments through
`UNW_FLAG_CHAININFO`) and adjacent-export spacing is an **UPPER** bound often read
as exact. For the twelve tables I used **adjacent-export, narrowed by content**:
the next export bounds the array, and every slot inside that bound decodes to
either the class's own dtor or the shared `_purecall`, so the bound is tight.

**The residual risk, stated rather than hidden:** a trailing run of `_purecall`
could in principle belong to an unexported table that this row swallowed
(`reference_adjacent_export_is_an_upper_bound.md`). I did not rule that out
independently. It cannot make a row *longer* than the bound, and the promotion
verdict below does not depend on the count.

For the one BODY row the `.pdata` route would have been **actively wrong**:
`0x1800f4520` has **no `.pdata` entry at all** (frameless leaf), so the nearest
preceding `RUNTIME_FUNCTION` belongs to `?PlatformDestruct@RenderBuffer@Video@@`
and a "size it from the covering entry" tool hands back a *different function's*
222 bytes. Details in the banner of `runtime/src/video/video_program.cpp`.

---

## 2. The twelve VTABLE rows — measured three ways, verdict unchanged

`tools/video_data_refs.py` (new, this lane) enumerates every in-DLL reference to
each subject: candidate generation by disp32 scan at every byte offset × trailing
immediate ∈ {0,1,2,4} (complete by construction), confirmed by backward capstone
decode, ambiguity resolved against a linear-sweep boundary map.

**Result, identical in shape for all twelve: exactly 5 references, ALL of them
`LEA`. Zero READs, zero WRITEs, in the whole `.text` section.** Nothing in the DLL
loads these addresses; they are only ever *stamped*.

The five stampers per class (`DeviceData` shown; the other eleven are the same
five roles):

| VA | owner | ours? |
|---|---|---|
| `0x1800f8cd0` | `??0DeviceData@Video@@QEAA@PEAVDevice@1@@Z` (ctor) | **yes** (companion_data.cpp) |
| `0x1800f8cf0` | `??1DeviceData@Video@@UEAA@XZ` (dtor) | **yes** (companion_data.cpp) |
| `0x180058b61` | inside the scalar deleting dtor `0x180058b10` | **yes** (deleting_dtor.cpp, by address) |
| `0x18007df60` | `??0DeviceData@Video@@QEAA@AEBV01@@Z` (**copy** ctor) | no |
| `0x180058566` | an unexported helper in the `0x18005xxxx` thunk region | no |

**Promotability** turns on owning the stamper (kickoff rule 4), and 3 of 5 are
ours. But that is not what decides it, and this is the point worth carrying
forward: **it does not matter who stamps a table that nothing ever loads.** The
live object's vptr is `dx11/`'s derived table (an exe-owned array —
`[Dx11Seam] 84/84 slots ours, in 12 exe-owned vtables`); the base stamp is
overwritten by the next statement in the derived ctor, and in the dtor it is the
object's last value before `MemFree`. Wave 17 §3.1 reached that by reading the
`dx11/` slots; this lane reaches it from the other side, out of the DLL's own
reference kinds, and the two agree.

**Verdict, route-scoped (rule 5):**

| route | verdict |
|---|---|
| DLL present | **NO-OP.** Promote or do not; no dispatch can observe it. |
| cutover | **NO-OP.** With the DLL gone `ResolveExport` returns null and our ctor stamps a null vptr — still into the same unread window, still overwritten by `dx11/`'s stamp before any dispatch. |

`NONE, EVER` is therefore **correct** for these twelve — the one place in this
lane where that phrase survives scrutiny. It is now *declared* as
`[SCOPE: PERMANENT]` rather than defaulted (§4).

---

## 3. The six DATA rows — split READ from WRITE, decided one at a time

### 3.1 The two singletons

Two independent implementations agree exactly (mine, and Wave 15's
`tools/rt_singleton_refs.py` re-run on this tip):

| row | refs | READ | WRITE | reads in bodies this tree has not ported |
|---|---|---|---|---|
| `?_singleton@Device@Video@@` | 165 | 163 | 2 | 113 (mine) / 77 (Wave 15's two-screen R1∪R2) |
| `?_singleton@Configuration@Video@@` | 63 | 56 | 7 | 42 (mine) / 32 (Wave 15's) |

My column is **screen R1 only** and therefore over-counts; Wave 15's is the better
number and I do not restate mine as the answer. Either way the conclusion is the
same and it is rule 6's: 77 and 32 in-DLL RIP-relative reads cannot be moved, so
minting our own storage strands them. **HARMFUL with the DLL present.**

> One instrument disagreement, recorded not resolved: Wave 17 §3.2 lists
> `Configuration@Video` as **55**/7 from Ghidra's `getReferencesTo`. Two decoder
> routes here read **56**/7. One reference, in Ghidra's direction of under-count.
> It moves no verdict and I did not chase it. **Addressee: none — this is
> recorded for whoever next re-derives that table, not handed to anyone.**

### 3.2 The four intrusive list heads

These were the rows the brief flagged as dangerous, and they are **not** the
singletons' shape. Their reference sets are tiny and *closed*: every one of the 11
references to each head sits in exactly four functions, all of that head's own
class.

| row | refs | READ | WRITE | the entire reader/writer set |
|---|---|---|---|---|
| `?_firstSurface@ImageBuffer@Video@@` | 11 | 8 | 3 | `ImageBuffer::{RecreateDynamic, ReleaseDynamic, PlatformConstruct, PlatformDestruct}` |
| `?_firstBuffer@IndexBuffer@Video@@` | 11 | 8 | 3 | the `IndexBuffer` four |
| `?_firstRenderBuffer@RenderBuffer@Video@@` | 11 | 8 | 3 | the `RenderBuffer` four |
| `?_firstBuffer@VertexBuffer@Video@@` | 11 | 8 | 3 | the `VertexBuffer` four |

Agrees exactly with Wave 17 §3.2's Ghidra counts (8/3 on all four).

**And all sixteen of those functions are ported AND registered in this tree**
(`runtime/src/video/buffers.cpp` and `dynamic_resources.cpp`, `RegisterRuntime
Export` with literal names). My R1-only column called 8 of 11 "unported" for the
VertexBuffer row; **screen R1 is wrong here and screen R2 corrects it** — a body
can be absent from the census population simply because our tree never calls it
(`reference_half_registered_port_reads_as_absent.md`, in its mirror form). Anyone
reading `wave23_vidvt_refs.txt`'s `-> N of M` line without R2 will reach the wrong
conclusion; that is why the tool prints "screen R1 only" on every one of those
lines.

**So why is promotion still HARMFUL?** Because the readers being ours is not the
same as retail *reaching* ours. `runtime/src/video/buffers.cpp` already states
the mechanism, and it is the rel32 problem again: *"no detour is installed on
these six, so the DLL's own constructors never reach our ports, and by the time
ours runs the shipped `Video::Device` startup has already put objects on those
lists."* Promote the head and retail's startup links onto the DLL's copy while our
constructs link onto the mint — **two lists over one pool of live GPU resources**,
which is the leak/double-free the brief warned about.

I re-checked the stated blocker rather than inheriting it (rule 3); it holds, and
it now has a measured reference table under it instead of only a comment.

### 3.3 The tag defect these six share

`runtime/src/video/buffers.cpp` and `dynamic_resources.cpp:144` both say, in our
own words, that **at cutover the mint owns these heads and they start NULL** — i.e.
the row is not merely promotable at cutover, it is **REQUIRED**. The same is true
of the two singletons: the storage they name ceases to exist. Yet all six carried
the expiry **`NONE, EVER`**.

That is exactly the `??_7Thread@System@@6B@` shape Wave 22 found and the
`?_singleton@FileSystem@IO@@` shape Wave 21 lane SHOT found
(`reference_unpromotable_is_route_scoped_not_permanent.md`). **Three lanes have now
independently hit it, which makes it a property of the tag population, not of any
one lane's rows.**

**Verdict, route-scoped:**

| route | verdict |
|---|---|
| DLL present | **HARMFUL.** Unchanged from Wave 17. |
| cutover | **REQUIRED.** Nothing else owns the storage. |

---

## 4. What changed in the tree

1. **`runtime/src/video/video_program.cpp`** (new) — `?SetupShaders@VideoProgram@
   Video@@QEBAXAEAVContext@2@@Z`, 122 bytes, five compare-guarded field writes,
   plus the live RT_DIFF oracle and the input sweep. Registered under
   `BANISHED_RT_VIDPROGRAM` (kill-switch `=0`) in `rt_video_port.cpp`.
2. **`standalone/reg_mint_promoted.cpp`** — the eighteen Video `RT_UNPROMOTABLE`
   tags now carry `[SCOPE: …]` and `[ADDRESSEE: …]`. **No verdict text was
   changed** (WAVE20_CLOSED §6.1 forbids rewriting another lane's verdict); the
   twelve NO-OPs became `[SCOPE: PERMANENT]`, the six HARMFULs became
   `[SCOPE: DLL-PRESENT]` with the cutover consequence spelled out and the cutover
   lane named. `unpromotable_slot_check.py` CHECK 3 moves **1 scoped / 1 addressed
   / 40 grandfathered → 19 / 19 / 22**.
3. **`tools/video_data_refs.py`** (new) — the reference enumerator, with its three
   controls.

---

## 5. `?SetupShaders@VideoProgram@` — ported, and verified WITHOUT a caller

The body is the five `?Set*@Context@Video@@` setters inlined into one call. Each
destination/dirty-bit pair matches `runtime/types/video/Context.h`'s existing
dirty-mask table one-for-one, which is what identifies the four `VideoProgram`
fields it reads — by which context slot each is copied into, not by a guess:

```
ctx+0x200 <- this        bit 0x100  _videoProgram
ctx+0x0b0 <- this+0x20   bit 0x004  _vertexLayout
ctx+0x1f0 <- this+0x28   bit 0x040  _vertexProgram
ctx+0x1f8 <- this+0x30   bit 0x080  _pixelProgram
ctx+0x1c8 <- this+0x38   bit 0x020  _renderState
```

### 5.1 It has no caller, and that was MEASURED

In-world run of this branch (`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600
BANISHED_PAUSEMENU_QUIT=120 BANISHED_RT_DIFF=1`, deployed md5 verified against the
worktree build, rc=0, **0-byte crash log**): `Video::VideoProgram::SetupShaders`
**registered no RtDiff site at all** — the body was never entered. One step up,
Wave 21's `[GFXBODY] MaterialInstance::Setup calls=1` line, which prints
unconditionally on the first call, is **also absent from the same log**. Retail's
renderer reaches `Setup` by an in-DLL rel32 call, and a rel32 is not a door.

`calls=0` is not "unexercised, therefore fine" — it is the condition under which a
wrong body ships unnoticed (`reference_uncalled_port_is_not_verified.md`).

### 5.2 The sweep, and its two controls

"No gameplay route" is not "no oracle". The body is pure — two inputs, six written
words, no calls, no allocation — so it is verified by an **input sweep against the
shipped export**: all 2⁵ equal/different patterns of the five guards × two starting
dirty masks = 64 cases, each run on two identical copies of a whole `Context` and
compared **byte for byte over all 0x2168 bytes** (which additionally proves neither
side wrote anywhere the other did not).

```
[VidProgSweep] clean: 64 cases, 64 agreed, 0 diverged | EFFECTIVENESS: 59 of 64
               cases changed the dirty mask, bits seen 0x1e4 of 0x1e4
[VidProgSweep] NEGATIVE CONTROL (render-state bit swapped for the pixel-program
               bit): 64 cases, 40 agreed, 24 diverged
[VidProgSweep] verdict: clean diverged=0, control caught=24 -> GREEN
```

* **Effectiveness control**: 59 of 64 cases actually changed the dirty mask and
  **all five** bits (`0x1e4`) were exercised — so this is not 64 green comparisons
  of nothing (`reference_probe_needs_an_effectiveness_control.md`).
* **Negative control**: corrupts a bit the comparison **actually reads**, in a
  field both sides write, and is caught in 24 of 64 cases — the 40 it does not
  catch are the cases where the render-state guard takes the equal path, which is
  the correct behaviour, not a miss
  (`reference_control_must_pick_a_compared_subject.md`, and its mirror).

Re-run: `BANISHED_AUTOQUIT=1 BANISHED_RT_VIDPROGRAM_SWEEP=1 ./reimpl-x64.exe`.
Evidence: `docs/runtime_port/wave23_vidvt_sweep.txt`.

### 5.3 The instrument bug my own control caught

`tools/video_data_refs.py`'s backward decoder first rejected any instruction
ending exactly at `p+4` — which is the **commonest** form (7-byte `mov`, disp at
+3, no trailing immediate). The positive control reported **1 of 5**; the row
counts it produced were small, plausible numbers. Fixed, then a second defect
surfaced: three or four different starts in `[p-15, p-1]` all decode and all
compute the same target, and taking the first was wrong for 1 of the 5. The sweep
boundary map resolves it. **Without the borrowed positive control both bugs would
have shipped as confident under-counts.**

---

## 6. Checkers, denominator, and what I did NOT check

```
tools/unpromotable_slot_check.py   exit 0, all six controls GREEN
    CHECK 1: 20 of 20 scoreable tags fit, 0 STALE
    CHECK 2: 20 numeric tags -- 1 declared, 19 grandfathered, 0 UNDECLARED
    CHECK 3: 41 tags with an expiry -- 19 scoped, 19 addressed, 22 grandfathered
             (was 1 / 1 / 40)
tools/dispatch_slot_map.py --check  110 registrations, 0 mismatches, 0 shadowed
mingw32-make standalone-debug       clean, 0 warnings from the new file
```

**Denominator.** 19 rows, named in the kickoff, all 19 examined. The reference
scan's population is **`.text` only** — `0x180001000..0x18014a600`, one section,
1,349,120 bytes. `runtime_census.py` figures are branch-local at
`wave23/vidvt`.

**What I did NOT check:**

* **`.rdata`/`.data` for references to the twelve vtables.** A table named only
  from another table, or held as a qword in a relocation or RTTI structure, is
  invisible to this scan. Wave 17 §2.4 did run a qword scan over all three shipped
  images and found none; I did not re-run it.
* **Whether a trailing `_purecall` run belongs to a swallowed unexported table**
  (§1.1). The verdict does not depend on the slot count.
* **The 5th stamper of each vtable** (`0x18005[3-7]xxx`) is attributed only by
  nearest-preceding-export, which for an unexported internal is an
  over-attribution. I know it is a stamp; I did not name its function.
* **Screen R2 for the two singletons.** I used Wave 15's numbers rather than
  re-deriving that screen.
* **The 1-reference Ghidra disagreement on `Configuration@Video`** (§3.1).
* **`?SetupShaders@` on a live render path.** It cannot be reached on one, by
  measurement (§5.1). The sweep is the verification; there is no in-world
  exercise of this body to report and I am not claiming one.
* **Anything about the other lanes' rows.** I touched only the 18 Video tags in
  `reg_mint_promoted.cpp`, deliberately, to keep the merge narrow.

---

## 7. Residuals, each with a NAMED addressee (rule 11)

1. **`?SetupShaders@VideoProgram@` is ported and cannot be reached in play, and
   neither can its caller.** Registering the export does not put us on retail's
   path; `?Setup@MaterialInstance@Graphics@@` needs a **detour**, not a
   registration, to take retail's rel32 traffic. That is a decision about Graphics,
   not Video. **→ the Wave 23 integrator, to hand to a Graphics lane (the same
   question Wave 21 lane GFXBODY's §7 opened and did not close).**
2. **Six Video rows flip from HARMFUL to REQUIRED at cutover**, and the mint
   already has storage for all six. **→ the cutover lane that stops loading
   `Runtime-x64.dll`**; the tags now name it, and
   `tools/video_data_refs.py` re-derives the evidence in one command.
3. **22 `RT_UNPROMOTABLE` tags still declare neither route scope nor addressee**,
   across IO, Graphics, Object, UI and Achievement. Three separate waves have now
   found a "NONE, EVER" that was really DLL-present. **→ the Wave 23 integrator**:
   this is one short pass per owning lane, not a lane of its own, and every Wave 23
   lane that touched a DATA row is in a position to do its own.
4. **`tools/video_data_refs.py` prints an R1-only port state** and says so on every
   line, but §3.2 shows R1 alone gives the wrong answer for a whole row family.
   Folding in `rt_singleton_refs.py`'s R2 screen would remove the foot-gun.
   **→ lane INTERNAL of Wave 23** (the instruments lane), or the integrator.
