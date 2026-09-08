# Wave 9 lane M — the differential oracle across the full harness matrix

Closes (partially) the [WAVE9.md](WAVE9.md) PRE-CUTOVER GATE CHECKLIST row
**"Differential oracle, full matrix"**, which before this lane read:

> Gameplay route clean 2026-09-07: 266 sites, 29,456,785 calls, 0 divergences in 0 sites
> … **Other cells NOT yet run at zero.**

Every measurement below is against the shipped `Runtime-x64.dll`. After lane G there is no
second implementation to diff against, so these numbers can never be produced again. Raw
per-cell evidence — the **complete, untruncated** `[RtDiff]` block for each run, the harness
trace that proves the route actually ran, and the crash log where there was one — is in
`docs/runtime_port/wave9_lane_m/<cell>.cell.txt`.

**Verdict: the row does NOT flip to met. Six cells are recorded clean; three cells carry
divergences or crashes that are findings, not paperwork.**

## Provenance of every number here

| | |
|---|---|
| first exe | `C:/GOG Games/Banished/reimpl-x64.exe`, **2026-09-07 23:29:48 -0400**, 24,319,533 bytes — the whole matrix table below |
| built by | `mingw32-make sa-deploy` at that timestamp; the build was a **no-op link+copy** — `find code engine runtime standalone dx11 -newer <exe>` was empty before it and after it |
| tree | those edits were uncommitted when the exe was built and were committed unchanged as **`c3bcdcf`** while this lane was running, so the measured binary IS reproducible from git after all |
| second exe | **2026-09-08 00:20:42**, built from the tree at `513ba03` (with `standalone/rt_data_mint.cpp` in flight from another lane). Six cells were re-run on it — see "Reproduced on a second binary" below |
| every cell | `tools/rt_matrix_cell.sh` (new, this lane) → `tools/rt_run.sh`, which prints the exe stamp it is about to measure, and every archived `.cell.txt` carries that line. **Each run's stamp was checked against the exe it was meant to measure**; the matrix table below is entirely the first exe, the re-run set entirely the second. |

`reference_lane_edits_after_report_invalidate_the_measurement.md` applied to this lane in the
other direction: the tree was **dirty with four other lanes' live edits when the first exe was
built**, so the first pass measured something git could not name. That resolved itself — those
files were committed unchanged as `c3bcdcf` mid-lane — and the matrix was then **re-run on a
second, independently built binary from a later tree**, which is a stronger result than either
run alone.

**Reproduced on a second binary (`2026-09-08 00:20:42`, tree `513ba03`):** menu 181 sites /
1,090,230 calls / 0; gameplay 314 / 29,587,258 / 0; save-load 336 / 54,409,563 / 0; tutorial 0
328 / 8,944,664 / 0; tutorials 1/2/3 **9 / 16 / 10 divergences** in the same single site over the
same 12,896 calls. Every clean cell stayed clean and the one divergence reproduced across two
binaries and two trees.

## The matrix

`sites` is the count of `[RtDiff]   <name> calls=… divergences=…` lines in the raw log — never a
`tail`. `calls` and `divergences` are the summary line. Negative control = the same cell with
`BANISHED_RT_LIBM=std`, which swaps lane L's bit-exact libm bodies back to MinGW's.

| Cell | Env | sites | calls | divergences | Negative control |
|---|---|---:|---:|---:|---|
| **main menu only** | `AUTOSTART=1 AUTOQUIT=1` | 181 | 975,139 | **0** | **fires: 58 in 2 sites** |
| **gameplay** (control) | `AUTOSTART=1 PAUSEMENU=900 PAUSEMENU_QUIT=120` | 314 | 29,257,059 | **0** | **fires: 100 in 2 sites** |
| **save / load** | `AUTOLOAD=Save9 PAUSEMENU=900 PAUSEMENU_QUIT=60 PAUSEMENU_BUTTON=6` | 335 | 54,382,522 | **0** | **fires: 53 in 2 sites** |
| **tutorial 0** | `AUTOTUTORIAL=1` + pause-menu quit | 328 | 8,803,537 | **0** | not run (see note) |
| **tutorial 1** | `AUTOTUTORIAL=01` + pause-menu quit | 335 | 10,323,700 | **9 in 1 site** — Finding 1, SETTLED | **fires: 58 in 3 sites** (48 libm + the same 10) |
| **tutorial 2** | `AUTOTUTORIAL=02` + pause-menu quit | 332 | 10,760,566 | **17 in 1 site** — Finding 1 | not run |
| **tutorial 3** | `AUTOTUTORIAL=03` + pause-menu quit | 318 | 8,562,636 | **10 in 1 site** — Finding 1 | not run |
| **stress** run 1 | `STRESS=1 STRESS_FRAMES=1200` | 352 | 108,914,263 | **90 in 1 site** | **fires: 84 in 3 sites** |
| **stress** run 2 | same | 353 | 108,708,284 | **0** | |
| **stress** run 3 | same | — | — | — CRASHED at shutdown | |
| **stress** run 4 | same | 353 | 108,867,004 | **0** | |
| **menus — Achievements** | `AUTOMENU_BUTTON=8 AUTOMENU_CLICKS=m6` | 195 | 1,660,606 | **0** | not run |
| **menus — Mods (open/Ok/quit)** | `AUTOMENU_BUTTON=7 AUTOMENU_CLICKS=101,m6` | 182 | 2,213,462 | **0** | not run |
| **menus — Goals** | `AUTOMENU_BUTTON=10` | — | — | — **CRASHED** | |
| **menus — Mods (row click)** | `AUTOMENU_BUTTON=7 AUTOMENU_CLICKS=1001,…` | — | — | — **CRASHED** | |
| **in-game save (write path)** | `PAUSEMENU_BUTTON=3` | — | — | — **NOT DRIVEABLE** | |
| **world teardown → menu** | `PAUSEMENU_BUTTON=5` | — | — | — **NOT DRIVEABLE** | |

Six cells recorded clean: **main menu, gameplay, save/load, tutorial 0, Achievements menu, Mods
menu**. Nothing else. Four of those six were re-run on the second binary and stayed clean.

### `calls=` was read, not just `divergences=`

Per `reference_uncalled_port_is_not_verified.md`. Across all 10 completed cells there are **417
distinct diff sites**, and taking the **maximum** `calls` per site across the whole matrix,
exactly **two are never called anywhere**:

* `UI::EditElement::SetText` — deliberate. `widgets_input.cpp:565` says so in as many words:
  *"registered for visibility in RtDiffSummary(); never reported — no oracle call below."*
  A NO-ORACLE port from WAVE7_LANE_D §5. Its site exists so the summary shows it, not so the
  summary verifies it. **It is not covered by this gate and the checklist should not imply it is.**
* `Audio::Scene::Serialize` — 0 calls in every cell of this matrix. It is not unverified: lane S
  verified it with a different oracle entirely (`BANISHED_RT_SAVE_TRACE=1`, 32,772 emissions
  identical). But **the differential-matrix row does not cover it**, and a reader of the
  checklist would otherwise assume the matrix covers everything with a site.

No other site is a `calls=0` pass anywhere in the matrix.

### Negative controls

`BANISHED_RT_LIBM=std` was run on **three** cells (menu, gameplay, save/load) plus tutorial 1 and
stress, and fired in every one — 58, 100, 53, 58 and 84 divergences respectively. So each of
those clean results means the ports agree with the DLL, not that the oracle stopped working.

**Where there is no negative control, this doc says so and claims nothing more.** Tutorials 0/2/3
and the two menu cells were not re-run under `RT_LIBM=std`. Their clean results are weaker than
the five that were controlled, and should be read that way. The control is cheap (20 s per
tutorial cell, 5 s per menu cell) and any lane closing this row should just run the remaining
five rather than inherit the gap.

---

## Finding 1 — `UI::NumberElement::SetValueInt` — INVESTIGATED AND SETTLED: the ORACLE is wrong, the port is correct, and the harness is corrupting the DLL's heap

**Read this section, not the summary above, for the disposition.** The paragraphs after it are
the first-pass reasoning from the byte report alone; they reached the right verdict for a partly
wrong reason and are kept because the correction is the point. What follows is measured.

**Reproduced on a second, independently built binary** (`2026-09-08 00:20:42`, current tree):
tutorials 1/2/3 give 9/16/10 divergences over exactly 12,896 calls, against the 9/17/10 measured
on the `2026-09-07 23:29:48` exe. Tutorial 0, gameplay, save/load, menu and stress remain at 0 —
and in those cells the site is **never registered**, i.e. the function is never called there.
That is the `calls=0`-reads-as-clean trap in its purest form and it is exactly why the
gameplay-only result previously in WAVE9.md was never evidence about this function.

### The effectiveness control settles "which inputs diverge"

A temporary probe (env-gated, `BANISHED_NUMELEM_PROBE`, since reverted — `widgets_input.cpp` is
byte-identical to HEAD) counted how many of the 12,896 calls actually reach `CacheText` rather
than taking the unchanged-value early-out:

```
WORK lines: 10          divergence blocks: 10
[NumElemProbe] WORK call=8    worked=8
[NumElemProbe] WORK call=13   worked=9
[NumElemProbe] WORK call=6892 worked=10
```

**Exactly 10 calls did work, and exactly those 10 diverged. 1:1, no exceptions.** So the
divergence is **not** value- or state-dependent: it is 100% of the calls that touch the
refcounted cached string, and 0% of the 12,886 that do not. The varying count across tutorials
(9/16/10) is simply how many resource counters changed value in those 900 frames — not a
mysterious input class. Without this control, "10 in 12,896" reads as a rare defect hiding in a
specific input; with it, it reads as a structural property of the oracle.

### What the two objects actually contain

The probe dumped the input, the entry state, **every** differing range (not just the first byte,
which is all `RT_DIFF_BYTES` reports), and the bytes each side's `+0xd0` pointer points at:

```
#1 self=…c08a94a8 value=4 | BEFORE mode=0 int=0 q0xd0=…c08a95a0
                          | OURS   mode=0 int=4 q0xd0=…c0acf2b0
                          | THEIRS mode=0 int=4 q0xd0=…c08a95a0
   diff +0xd0..+0xd2 (3 bytes)          <-- the ONLY difference in the whole object
   ours[-8..40]   = 01000000 01000000 "4"           <-- len 1, refcount 1, the text for value=4
   theirs[-8..40] = 00000000 00000000 …"numberYear" <-- a ZEROED header: a FREED block
```

Every one of the ten is that shape. `_mode` and `_intValue` agree on all ten. The **only**
difference anywhere in the object is the `+0xd0` pointer, and:

* **ours points at a live block holding the correctly formatted text** — `"4"` for 4, `"39"` for
  39, then `"272"`, `"157"`, `"88"`, `"126"`, `"360"`, `"4655"`, `"42"`, `"40"`. Every one
  matches its input.
* **theirs points at the entry-state pointer, unchanged, whose 8-byte header is now all zeros**
  — a freed block. Retail did not "compute a different answer"; retail was handed a buffer our
  call had already released, and left the stale pointer in place.

So the side the oracle calls `theirs` is the side holding the wrong data, and it holds it
*because of the oracle*. **This gate, as written, penalises the correct body.**

### The order-inversion control: it does not just move the divergence, it corrupts the heap

The sharpest version of the sequence trap check the lead asked for: run the DLL's body **first**,
on the replay, before ours touches `self`. Everything else identical.

**The run crashes**, inside the Runtime allocator, with the free list corrupted:

```
ACCESS_VIOLATION  WRITE of 000001f3023fb150   Runtime-x64.dll + 0xC74EA
[REDZONE-DUMP] bucket 8: free block @…021c6040 has CORRUPTED forward link
                 qword=ffffffff00235108 -> next=…023fb150 (out of range)
[REDZONE-DUMP]   clobbered free block body: … +10=6f6e53003237322b …
```

`6f6e53003237322b` is `+272\0Sno` — **`"272"`, the input of diverging call #3.** The number text
one of the two `CacheText` calls formatted is sitting *inside a block that is already on the free
list*. That is the mechanism, caught in the act, with the diverging call's own digits as the
signature.

### Verdict

`RtUi_NumberElement_SetValueInt` is **correct**. The oracle around it is **unsound**, and it is
unsound in the exact way this file's own header block forbids — `widgets_input.cpp` lines 53-58,
written by the lane that ported it:

> **NOT done this way: anything that swaps a REFCOUNTED pointer** (a shared string, a shared
> allocation). Replaying the original there would decrement/increment the SAME refcount a second
> time — wrong regardless of whether the first decrement freed it — which is exactly why
> `EditElement::SetText` is a documented NO-ORACLE port rather than a prefix-replay one.

`NumberElement::SetValueInt` calls `NumberElementCacheText()` (the DLL's private
`0x1801189f0`), which manages precisely such a refcounted string. It was prefix-replayed anyway.
The rule was written down and did not reach the function next to it — the same shape as
`BANISHED_AUTOQUIT` producing the same wrong conclusion six waves apart.

**Recommended disposition**, for the owner of `widgets_input.cpp` (this lane did not apply it —
making a diverging cell go green is not a matrix lane's call, and it needs its own written
justification in WAVE7_LANE_D.md §5 beside `EditElement::SetText`):

* `NumberElement::SetValueInt` **and** `SetValueFloat` (identical shape, same `CacheText` call,
  currently silent only because the float setter is not called on these routes) become
  **NO-ORACLE ports**, like `EditElement::SetText`, with the reason recorded.
* Excluding `+0xd0..+0xd7` from the byte compare would be **wrong**: it would make the audit
  green while leaving the double-release — and therefore the heap corruption demonstrated above —
  in place on every diff run.
* Anything else in the tree that prefix-replays a body reaching `CacheText` or any other
  refcounted-string swap is the same defect. `NumberEditElement::SetMinMax` calls
  `RealNumberEditElementSetValue()` and is worth reading first.

**Two leads checked and closed.** (a) `UI::NumberEditElement::SetValue`, landed in `c3bcdcf`
with its fixed-signature `System::String::Format` call, is a plausible neighbour but not
implicated: the diverging path runs through the DLL's own `CacheText` at `0x1801189f0`, and the
text our side produces is correct for all ten inputs, so no shared formatting helper is
misbehaving. (b) The first-pass reasoning below argued that "the only code writing `+0xd0` in
either branch is byte-identical DLL code". **That was wrong**: the probe shows retail leaves
`+0xd0` untouched and only our call replaces it. The verdict survives, but by the measurement
above, not by that argument.

---

### First-pass reasoning, kept for the correction

## Finding 1 (first pass) — `UI::NumberElement::SetValueInt` diverges on tutorials 1, 2 and 3

**Reproducible and deterministic.** Three runs of tutorial 1 gave 9, 10 and 10 divergences over
**exactly 12,896 calls each time**; tutorials 2 and 3 give 17 and 10 over the same 12,896. It
does not appear in gameplay, save/load, menu, stress or tutorial 0 — in those cells the site is
**never registered**, i.e. the function is never called there, so those cells are silent about it
rather than evidence against it.

Every reported divergence is at the same offset:

```
DIVERGENCE UI::NumberElement::SetValueInt  call #1  ours=+0xd0: 88 06 66 39 fa 01 00 00  theirs=+0xd0: 98 85 43 39 fa 01 00 00
DIVERGENCE UI::NumberElement::SetValueInt  call #2  ours=+0xd0: 98 85 43 39 fa 01 00 00  theirs=+0xd0: 78 d2 65 39 fa 01 00 00
DIVERGENCE UI::NumberElement::SetValueInt  call #3  ours=+0xd0: 78 d2 65 39 fa 01 00 00  theirs=+0xd0: 48 94 43 39 fa 01 00 00
```

**Diagnosis: this is an ORACLE defect, not a port defect, and the argument does not rest on the
values looking like pointers.**

1. Those eight bytes are a little-endian **heap address** — `0x000001fa_39660688` and friends,
   all in one heap region, and in tutorial 2 all in `0x000001f3_81ff….`
2. `RtUi_NumberElement_SetValueInt` (`runtime/src/ui/widgets_input.cpp:505`) **never writes
   +0xd0.** It writes `_mode` (+0xc8), `_intValue` (+0xd8), `_floatValue` (+0xdc), walks the
   dirty chain, and then calls `NumberElementCacheText()` — which is `ResolveAddr(Module::Runtime,
   0x1801189f0)`, i.e. **the DLL's own private `NumberElement::CacheText`.** Retail's body calls
   the same address. **The only code that writes +0xd0 in either branch is byte-identical DLL
   code**, so the difference cannot be attributed to the port's logic; it is the address an
   allocator handed back on two independent invocations.
3. The report names the **first** differing byte. It is +0xd0 on every reported call, so
   `_mode`, `_intValue` and everything below +0xd0 matched — the early-out guard agreed and both
   branches took the same path. A guard that disagreed would have surfaced at +0xc8 or +0xd8.
4. `theirs` of call *N* is `ours` of call *N+1* on consecutive lines. That is a freed block being
   handed straight back — allocator recycling, which values cannot do.

Only ~10 of 12,896 calls diverge because the rest early-out on an unchanged value and never reach
`CacheText`, so nothing is reallocated and the objects compare identical.

**But it is not cosmetic, and the fix is not "exclude +0xd0".** The oracle replays into
`NumberElement replay; memcpy(&replay, self, sizeof(NumberElement))` — a **shallow** copy, so
`replay`'s +0xd0 is the *same* pointer `self` had on entry. Ours runs `CacheText` on `self`,
which releases that buffer; theirs then runs `CacheText` on `replay`, starting from the pointer
we just released. On those ~10 calls per run the oracle is **releasing one refcounted buffer
twice inside the DLL's heap.** That is a live heap hazard created by the harness, in a run that
also produced Finding 3 below.

Recommended disposition for whoever owns `widgets_input.cpp` (it is uncommitted-modified by
another lane right now, so this lane did not touch it): the replay needs the owning field handled
— snapshot-and-restore or a deep copy — not a byte-range exclusion, because an exclusion would
leave the double release in place and make the audit greener while doing so. Compare
`reference_oracle_must_not_compare_unwritten_bytes.md`: this is its neighbour, where **both**
sides write and the written value is an address.

**A layout note found on the way, worth a separate look.** `runtime/types/ui/NumberElement.h`
calls +0xcc..+0xd7 `_pad_cc_d8`, "not reversed by this lane". `runtime/types/ui/TextElement.h`,
derived from call sites at `0x18011e9b0`, says +0xc8 is `Array<System::String>.data` and **+0xd0
is that array's `size` (int32) and +0xd4 its `capacity`**. The observed +0xd0 is an 8-byte
pointer, which fits neither header. The two headers already disagree with a third table in
`LabelElement.h`. **Nothing in this lane depends on resolving that**, but a `NumberElement` whose
+0xd0 is documented as padding and behaves as an owning pointer is exactly the kind of gap that
makes a ctor mint the wrong shape at cutover.

## Finding 2 — `Picking::Scene::Intersect` diverges on the stress route, input-dependently

**One stress run in four.** Run 1: **90 divergences over 20,868 calls.** Runs 2 and 4: **0 over
22,668 and similar.** The call counts differ between runs, so the runs are not the same runs —
`Picking::Scene::Intersect` is driven by where the ray is cast, which on this route depends on
the real mouse cursor. **A clean re-run therefore does not clear this finding; it only means
those rays were not cast.**

```
DIVERGENCE Picking::Scene::Intersect  call #10878
  ours=outPos first differing byte=9 p=(41.4928703,26.5318527,-0.018081665)  n=(0.464434296,0,0.8856075…)
  theirs=…                       -1  p=(41.4928703,26.5318527,-0.0180778503) n=(0.464434296,0,0.88560…)
```

The return value matched (it is the first check in the `PostCheck` list and it is not the one
that failed). `outPos.x` and `outPos.y` are **bit-identical**; only `z` differs, by 3.8e-6 —
about one ULP of the *x* magnitude, which is consistent with a small difference in `hitT` on a
near-vertical ray: `z = hitT*dir.z + start.z` moves by δ while `x = hitT*dir.x + start.x` moves
by δ·dir.x and stays inside one ULP of 41.49.

The port's own comment (`runtime/src/picking/scene.cpp:970`) is explicit that it demands bit
equality with **no tolerance**, on the grounds that it is a faithful transliteration in retail's
association order and *"anything less than bit-exact means a term was moved."* By that standard
this is a defect in the port, in a branch only some ray geometries reach. It is **not** the libm
family — the stress negative control moves `FromYawPitch`, `FromYawPitchOffset` and
`Vector3::Heading`, and never touches this site.

Two candidate mechanisms worth checking first, in order of cheapness:

1. **A tie-break in the nearest-hit test.** The port keeps `if (0.0f <= hitT && hitT <= bestT)`.
   If retail is `<` rather than `<=`, then two coplanar/duplicate triangles with equal `hitT`
   pick a *different* triangle, whose independently-computed `hitT` differs in the last bits —
   which is exactly the observed magnitude, and exactly why it only fires on the stress town's
   3-sets-of-every-building geometry and never on a starting map.
2. **An association order moved** in the Möller–Trumbore block or in the inverse-transform
   expressions, in a term whose contribution is negligible for most rays.

**This lane deliberately did not touch the port.** A divergence found before cutover is the point
of running the matrix; making it pass would be the failure this gate exists to prevent.

**How to reproduce it deterministically** (this lane did not, and that is the gap): the route
needs the ray inputs pinned. `BANISHED_CAMERA_PIN` pins the camera but not the cursor, and the
cursor is what varies. A stress cell that records the (start, dir, maxDist) of every diverging
call would make this a fixture instead of a 1-in-4.

## Finding 3 — intermittent shutdown crash on the stress route, in the allocator

Stress run 3 of 4 died **after the sim finished**, during teardown, and therefore **destroyed the
whole `[RtDiff]` report for that run** (the summary is printed in `standalone/main.cpp:1895`,
after `Game::RunGameMain` returns).

```
ACCESS_VIOLATION  Fault: READ of 000001d9fffffffa
RBX=000001D9FFFFFFFA  RDX=000001DA00000002
#0  Runtime-x64.dll + 0xC6C5F
#1  (anonymous namespace)::FreeLocked(void*, void*)
#2  Runtime-x64.dll + 0xC7B17
#3  (anonymous namespace)::MemFree(void*)
#4  TreeNodeFree_2ce60
#5  Tutorial_Dtor_f8ff0
#6  GameUpdateLoop_Shutdown
```

The faulting pointer is `0x000001d9_fffffffa` — a **valid heap high dword with `-6` in the low
dword**. That is the half-written-pointer signature this project has hit before
(`reference_load_wild_write_high_dword_7fc.md`, where an `fn_String_Dtor` on an
`InternalBasePtr` decremented an allocator boundary tag's high dword). Here it is the low half
that is wrong, on a node being freed by the tutorial object's tree destructor during
`GameUpdateLoop::Shutdown` — in a **stress** run, i.e. one with no tutorial content, which makes
the tutorial tree an empty-or-nearly-empty structure being torn down.

`[REDZONE-DUMP]` reported no tag corruption and intact free-list forward links, so the bad value
is in the tree node, not in the heap metadata.

This did not reproduce in runs 1, 2 or 4 and was not chased further by this lane. It matters to
the gate for a second reason beyond the bug itself: **a shutdown crash silently converts a
recorded cell into no cell at all**, and it does it after the run looks like it succeeded.

## Finding 4 — two front-end menu routes crash, and it is NOT the oracle

Both were re-run with `BANISHED_RT_DIFF` **unset** and crash identically, so neither is
oracle-induced. Both are routes a memory entry records as CLOSED and default-on.

**(a) Goals menu — `BANISHED_AUTOMENU_BUTTON=10`, no clicks needed.**

```
Fault: READ of 0x150   RAX=0
AutoStart::TickMenu → StandaloneStub_Slot8_OnCommand → MainMenuDialog_OpenGoal
  → AddDialogToUI → GD_Slot4 → GoalDialog_Slot4          (code/Game/Dialog/goaldialog.cpp:98)
```

`code/Game/Dialog/goaldialog.cpp:98` is
`*(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x150)`. From the **front-end** menu there
is no game world, `*g_pGameWorldSingleton` is null, and the read of +0x150 faults. Per
CLAUDE.md's "don't fix a faithful crash with a null check", the question for the owning lane is
whether retail's front-end Goals button reaches this dialog at all — the earlier closure
(`project_achievements_menu_two_defects.md`) may have been verified in-game only.
`BANISHED_AUTOMENU_BUTTON=8` (Achievements) on the same build is **clean**, so this is specific
to Goals.

**(b) Mods dialog row click — `AUTOMENU_BUTTON=7 AUTOMENU_CLICKS=1001,…`, and the same in-game
via `PAUSEMENU_BUTTON=7`.**

```
Fault: READ of 0xd0   RAX=0x80   RBX=0x3E9 (=1001)
ModDialog_Slot8+0x2b8:  call *fn_Modification_GetMod ; cmpb $0x0, 0x50(%rax)
```

`fn_Modification_GetMod` is the DLL's 12-byte `&m_mods[index]`. It returned **0x80** for
index 1 — i.e. `modSingleton` was **NULL** and 0x80 is the stride. The debug log confirms the
cause upstream of the crash:

```
[ModDialog::OnOpened] rows=0 entries=0000000000000000 table=…
[ModDialog] opened + AddDialog'd (dlg=… mods=0)
```

**The mod list is empty**, with `WinData/betterFields.pkm` and `WinData/DebugMenu.pkm` both
present on disk. The row click at id 1001 is then a click on a row that does not exist, which is
harness misuse layered on top — but **`mods=0` is the real regression**, against
`project_mods_menu_closed.md`, and the dialog opens and Ok's cleanly when no row is clicked
(the `menus_mods_open` cell above is clean at 2,213,462 calls). This is *not* the
`mods-off-breaks-save-loading` situation: the `.pkm` are in `WinData`, not moved aside.

Both crashes are pre-existing on this exe, which was built from `f53f887` **plus four other
lanes' uncommitted edits**. This lane did not bisect them; the suspects worth checking first are
the uncommitted `runtime/src/object/deleting_dtor.cpp` (lane K) and
`runtime/src/system/value_types.cpp` (which `port_bypass_audit`'s known blind spot already
flagged for three live-path `String::Release` bypasses).

## Cells that could not be run, and why

| Cell | Why not |
|---|---|
| **in-game save (the save WRITE path)** | `BANISHED_PAUSEMENU_BUTTON=3` opens the SaveLoad browser (`SetOperation`, `GetSaveCount`, `GetSaveInfo` all fire, confirmed in the log) but there is **no unattended driver for the save-name entry and confirm**, and `BANISHED_PAUSEMENU_CLICKS` tokens are routed to the last-opened `ModDialog`, which is null here. The process sat on the dialog until the timeout, so no summary was written. **The matrix has NO cell covering the save WRITE path** — lane S's `BANISHED_RT_SAVE_TRACE` is the only thing that does, and it is a different oracle over a different question. This is the biggest hole in the row. |
| **world teardown → main menu** | `BANISHED_PAUSEMENU_BUTTON=5` dispatches cleanly (`[PauseMenu] OnCommand returned`) and the world is torn down, but nothing then quits the process: `BANISHED_AUTOSTART_GAMES=1` did not re-arm a second game and `AUTOQUIT` is parsed before `AUTOSTART` and cannot be combined. Timed out at 600 s with no summary. |
| **Goals menu**, **Mods row click** | crash — Finding 4. |
| **`sa-stress` proper (`mingw32-make sa-stress`)** | that target builds the **perf** exe (`standalone/build-perf/`, `-O2`, logs stripped) — a different binary with no `[RtDiff]` output. The stress *route* was run instead, on the deployed debug exe, via `BANISHED_STRESS=1 BANISHED_STRESS_FRAMES=1200`. Frames were cut from 6000 to 1200 because the oracle double-runs every ported body; 1200 frames still produced 108.9M diffed calls, the largest cell in the matrix. |

## What the checklist row should now say

> **Differential oracle, full matrix** — SIX cells recorded clean on the 2026-09-07 23:29 exe
> (main menu 181/975,139; gameplay 314/29,257,059; save-load Save9 335/54,382,522; tutorial 0
> 328/8,803,537; Achievements menu 195/1,660,606; Mods menu 182/2,213,462), each with
> `divergences=0` and, for menu/gameplay/save-load, a `BANISHED_RT_LIBM=std` negative control
> that fires. **NOT MET:** tutorials 1-3 diverge in `UI::NumberElement::SetValueInt` (an oracle
> defect: the port is CORRECT and the prefix replay double-manages a refcounted buffer, proved
> by an effectiveness control (10 of 12,896 calls do work, all 10 diverge) and an order-inversion
> control that CORRUPTS THE DLL'S FREE LIST — Finding 1); the stress route diverged in
> `Picking::Scene::Intersect` in 1 run of 4, input-dependently (Finding 2); one stress run died
> at shutdown in the allocator and lost its report (Finding 3); the Goals and Mods-row menu
> routes crash independently of the oracle (Finding 4); and **the save WRITE path has no cell
> at all.**

## Tooling added

`tools/rt_matrix_cell.sh` — one harness cell, archived. It wraps `tools/rt_run.sh` and copies
`standalone_error.log` / `standalone_debug.log` / `standalone_crash.log` out of the game
directory before the next cell deletes them, then prints the summary line and **every** site
whose `divergences` is non-zero. Its `grep` has no `tail`, for the reason recorded at the bottom
of `rt_run.sh`.

---

## The prefix-replay sweep (handed to lane N)

Finding 1's defect is not "one function was ported carelessly". The rule it broke is written in
`widgets_input.cpp`'s own header, lines 53-58, by the lane that then broke it two hundred lines
lower. **A rule that exists and is not mechanically enforced gets violated**, so the useful
deliverable is the sweep and the enforcement, not the two fixes.

**164 `RT_DIFF_BYTES` prefix-replay sites across 29 files.** Filtering to bodies that both
memcpy a scratch copy *and* reach a string / refcount / allocation name leaves **nine**, and the
matrix's own `calls=` column then splits them sharply:

| Prefix-replay body | max `calls` across the WHOLE matrix | Read |
|---|---:|---|
| `RtUi_NumberElement_SetValueInt` | 12,896 (10 did work) | **THE FINDING.** |
| `RtUi_NumberElement_SetValueFloat` | **NEVER REGISTERED** | Identical body, same `CacheText`, same hazard. **Silent only because nothing calls it.** Fix with `SetValueInt`. |
| `RtUi_NumberEditElement_SetMinMax` | **NEVER REGISTERED** | **Top priority after the two above.** Its replay shares `base._text`, and `RealNumberEditElementSetValue()` swaps that refcounted pointer — the same mechanism, with *zero* coverage to have caught it. |
| `DocumentTextElement_SetText_UiString` | **NEVER REGISTERED** | Reads SAFE on inspection: `UI::String` is argued POD/non-refcounted, and the body undoes its write so retail runs on the true pre-call state. Its neighbour even hand-decrements `theirs`'s refcount and frees it — that lane understood the hazard. Verify, do not assume. |
| `RtUi_Element_Ctor` | **NEVER REGISTERED** (under this name) | Check how it is registered before concluding anything. |
| `RtUi_SpinnerElement_SetMinMax` | 4 | Reaches the same `NumberEditElement` SetValue path. 4 calls is not coverage — check how many DID WORK. |
| `RtUiElementDescription_Ctor` | 2 | Same. |
| `RtUi_Element_SetToolTip` | 10 | Uses `ReplayGuard`; the field is a POD `UIString`. Lowest concern. |
| `RtGraphics_MeshInstance_UpdateHierarchy` | 48,742 | Heavily exercised and clean; the alloc/free hits are almost certainly incidental. |

**Three of the nine are never called anywhere in the matrix, and two of those three share
Finding 1's exact mechanism.** Their zero-divergence status is worth nothing — it is
`reference_uncalled_port_is_not_verified.md` sitting directly on top of the defect the matrix
just found.

### Making the rule checkable — three layers, cheapest first

1. **The order-inversion control is already a whole-tree detector, and it needs no new tool.**
   Running the DLL's body FIRST on the scratch is a one-flag change to the replay macro. On the
   first body it was tried against it crashed in 20 seconds, inside the allocator, with the
   diverging value's own digits in a free block. A `BANISHED_RT_DIFF_INVERT=1` pass over the
   harness matrix would surface every body that double-manages an allocation, by crashing on it,
   with no reasoning and no per-function argument required. **This is the highest
   value-per-hour item in this section.**
2. **A runtime assert that detects the shape rather than the symptom.** Before running the
   original on the scratch, walk the scratch for qwords that are heap pointers; after our body
   has run, check whether any of them now addresses a block whose `{length, refcount}` header at
   `[-8]` reads zero — i.e. the replay is about to hand retail a pointer into memory we already
   released. Name the function and abort. This turns "someone must remember lines 53-58" into a
   self-reporting harness.
3. **A static candidate list**, exactly the nine above. It has `port_bypass_audit.py`'s known
   blind spot — it cannot follow a call into a callee, so `CacheText` is only visible because it
   is named at the call site — so it yields **candidates needing a written disposition each**,
   in lane V's style, not verdicts. Nine out of 164 is a tractable list; a checker that returns
   0 here would be lying.

### One change to `RT_DIFF` that would have made this visible on day one

Every diff site reports `calls=`, which counts calls **entered**. Finding 1 was invisible for a
wave because 12,886 of 12,896 calls took an early-out and compared identical, so the site looked
richly exercised. **A site should report how many calls DID WORK as well as how many were
entered.** `reference_probe_needs_an_effectiveness_control.md` already says this about probes;
the argument here is that it belongs in the oracle itself, because "10 of 12,896 calls did
anything at all" is the number that turned a mysterious rare divergence into a structural
one-line answer.
