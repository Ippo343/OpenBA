# WAVE 24 LANE SMALLBODY — the tail of the 22, nine rows, six namespaces

Branch `wave24/smallbody`, cut from `wave24/base` at `f5d70de2`.
Private worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\agent-a4a9958ef0421fc4d`.

Every binary read in this report is against **retail**
`C:\GOG Games\Banished\Runtime-x64.dll`, image base `0x180000000`. The profile
build was not opened for anything here.

---

## 0. HEADLINE — FOUR ROWS PORTED, AND **FIVE OF NINE STATED BLOCKERS DID NOT SURVIVE RE-CHECKING**

Four of the nine are ported, registered and building. The blocker table (§2) is
the other deliverable, and it is the larger one: **five of the nine rows'
recorded blockers are false, stale, or licensing far more than they say**, and
one of them — `??1Stream@Audio@@` — I discharged *by construction* while porting
the row it was blocking.

**My worktree also started on the WRONG COMMIT.** `git checkout -b
wave24/smallbody` ran against a stale `HEAD` (`1afa558e`, a census commit
predating the wave), not `wave24/base`. It was an ancestor, so
`git reset --hard wave24/base` was safe, but nothing announced it: the worktree
had a Wave-11-era `CLAUDE.md` on disk and every measurement taken before the
reset would have been against the wrong tree. **Check `git log --oneline -1`
against the wave base before the first measurement** — this is the third
worktree-provisioning defect in three waves
(`reference_parallel_lanes_need_private_worktrees.md`,
`reference_worktree_reassignment_crosses_lanes.md`).

---

## 1. WHAT I MEASURED, AND THE COMMANDS THAT RE-DERIVE IT

### 1.1 The board, before and after, in THIS worktree

```
tools/runtime_census.py --unported          # run with the main checkout's venv python
```

| | before (`f5d70de2`) | after | delta |
|---|---|---|---|
| TOTAL called / ported | 1319 / 1159 = 87.9% | **1321 / 1163 = 88.0%** | +2 called, **+4 ported** |
| Audio | 30 / 25 | 31 / 27 | +1 called, +2 ported |
| Input | 29 / 26 | 29 / **27** | +1 ported |
| UI | 208 / 205 = 98.6% | 208 / **206 = 99.0% AT CEILING** | +1 ported |
| internal | 187 / 142 | **188** / 142 | +1 called |

**THIS IS A LANE FIGURE, NOT A BOARD FIGURE.** It was measured in an isolated
worktree; Wave 21 published a wave-level number assembled from seven such
figures and it was wrong. The integrator re-derives on the merged tip.

### 1.2 I GREW THE CUTOVER SURFACE BY TWO ROWS — DECLARED, AS THE BRIEF REQUIRES

`called` went up by 2 and **both are mine**
(`reference_porting_can_grow_the_cutover_surface.md`):

| new `called` row | why my port created it |
|---|---|
| `??_7Stream@Audio@@6B@` (`0x180154950`) | My `Audio::Stream` ctor/dtor stamp the vptr through `ResolveAddr(Module::Runtime, 0x180154950)`. Retail stamps it with a **RIP-relative LEA**, which is not a door, so the row was previously invisible. |
| `internal` +1 — `0x18000c4a0` | The unexported `+0x30` buffer release the `Stream` dtor calls. My port reaches it by address. |

Neither is a regression, and the first is the point: **while the DLL owned that
ctor, no mint table could ever change which vtable got stamped**, because we
intercept only at doors and a RIP-relative LEA is not one
(`reference_a_ported_row_can_be_permanently_unpromotable.md`). Porting the ctor
is what converts the LEA into a door. See §3.

### 1.3 An extent tool, and a finding it produced

I wrote a union-forward `.pdata` + adjacent-export extent reader
(scratchpad `extent.py` / `frags.py`) rather than quoting sizes from docs.

**WHILE IT WAS AGREEING WITH ME, I HAND-CHECKED IT** (KICKOFF §3 rule 1 — a tool
whose output supports the conclusion you expected is no evidence the tool
works). It reproduces Wave 23 lane IOPORT's two *independently* measured rows
**exactly** — `?SetupFileSystem@` 2617 / 2624 and `??1PackageFile@` 318 / 336 —
and its negative case fires: `??1PackageFile@IO@@QEAA@XZ`, the wrong non-virtual
spelling, reports `ABSENT FROM EXPORT TABLE`.

**THE FINDING: `?Frame@MainLoop@Engine@@` IS A 5.6x `.pdata` UNDERSTATEMENT, AND
IT BITES EXACTLY AS WAVE 23 WARNED.**

```
0x1800dd100
   frag 0: 0x1800dd100..0x1800dd17b =  123 bytes   chained=False
   frag 1: 0x1800dd17b..0x1800dd318 =  413 bytes   chained=True
   frag 2: 0x1800dd318..0x1800dd3ac =  148 bytes   chained=True
   ENTRY-ONLY bound :  123 bytes
   UNION-FORWARD    :  684 bytes across 3 fragments
```

The **entry** fragment reads `chained=False`, so a reader who stops there gets
123 bytes and no signal that anything is missing — the precise direction
`WAVE24_KICKOFF §2 INTERNAL8` says it bites. It is the only one of my nine that
chains; the other eight are single-fragment and their entry bound is exact.

The existing comment (`mainloop_run.cpp:333`) says "683 bytes", i.e. the right
magnitude, so whoever wrote it did *not* size from the entry fragment. No defect
there — but 683 vs 684 is an end-inclusive off-by-one.

### 1.4 Extents of all nine (union-forward / adjacent-export upper)

| row | addr | ord | `.pdata` union | adj-export upper |
|---|---|---|---|---|
| `??2SpriteSheet@UI@@SAPEAX_K@Z` | 0x180002e60 | 895 | **99** | 112 |
| `??1Stream@Audio@@QEAA@XZ` | 0x1800a0d90 | 688 | **153** | 160 |
| `??0Stream@Audio@@QEAA@XZ` | 0x1800a0e30 | 410 | **159** | 160 |
| `?PlatformShutdown@Device@Input@@AEAAXXZ` | 0x1800e1b50 | 3395 | **259** | 272 |
| `??1PackageFile@IO@@UEAA@XZ` | 0x1800b4540 | 639 | **318** | 336 |
| `?Initialize@GoalList@Achievement@@…` | 0x18009f0e0 | 3070 | **389** | 400 |
| `?Frame@MainLoop@Engine@@QEAA_N_N@Z` | 0x1800dd100 | 2377 | **684** (3 frags) | 704 |
| `?Update@Scene@Audio@@…` | 0x1800df6c0 | 4502 | **1816** | 1824 |
| `?SetupFileSystem@Modification@IO@@QEAAXXZ` | 0x1800aebb0 | 4030 | **2617** | 2624 |

### 1.5 Manglings, read out of the export directory — not inherited

Two of my nine are destructors and the brief warns the wrong one of
`QEAA`/`UEAA` resolves NULL silently. Both spellings were checked against the
shipped export table directly, and **they genuinely differ between the two rows**:

| spelling | present? |
|---|---|
| `??1Stream@Audio@@QEAA@XZ` | **present**, 0x1800a0d90 ord 688 |
| `??1Stream@Audio@@UEAA@XZ` | **ABSENT** |
| `??1PackageFile@IO@@UEAA@XZ` | **present**, 0x1800b4540 ord 639 |
| `??1PackageFile@IO@@QEAA@XZ` | **ABSENT** |

So `Audio::Stream`'s dtor is non-virtual and `IO::PackageFile`'s is virtual.
Guessing one from the other would have produced a silent NULL in both directions.

---

## 2. THE BLOCKER TABLE — THE DELIVERABLE THE BRIEF ASKED FOR

Re-checked by measurement, not by reading forward. **VERDICT** is mine;
**SOURCE** is where the blocker is written.

| # | row | stated blocker (source) | addressee / expiry | **survives re-checking?** |
|---|---|---|---|---|
| 1 | `??0Stream@Audio@@` | *"ORACLE DEFERRED… running the shipped body twice against scratch buffers would start two threads on fake objects"* — `singleton_ctors.cpp:704` (Wave 9 C2) | neither | **PARTLY.** Technically correct and I reproduced it from the disassembly — but it is a verdict about an **ORACLE**, never about portability. Two waves of gap tables read a narrow holdback as a broad one. **PORTED.** Oracle holdback upheld and restated. |
| 2 | `??1Stream@Audio@@` | *"Porting the pair requires porting that helper [0x18000C4A0] first"* — `WAVE23_LANE_TAIL.md:194` | Wave 24 kickoff writer / none | **NO — FALSE.** See §2.1. Discharged by construction. **PORTED.** |
| 3 | `?Update@Scene@Audio@@` | *"a replay that draws twice from System::Random diverges by construction… needs snapshot/restore of the Random state"* — `audio.cpp:17` | Wave 24 kickoff writer / none | **YES, and it is an ORACLE blocker only.** `WAVE23_LANE_TAIL.md:187` says so itself and names the precedent for porting without one. **NOT A WALL.** Not ported — out of budget, not blocked. |
| 4 | `??1PackageFile@IO@@` | *"it has **no oracle**… a fabricated subject is a fake rather than a synthetic one"* — `WAVE23_LANE_IOPORT.md §2.4` | Wave 24 lane OBJ / none | **YES.** The strongest-held of the nine, and correctly scoped. Not ported. |
| 5 | `?SetupFileSystem@Modification@IO@@` | *"NOT dependency-blocked… 13 of the 16 callees are already ported"*; real blockers are SIZE, **the broken mods route**, and BLAST RADIUS — `WAVE23_LANE_IOPORT.md §2.3` | *"the Wave 24 lane that owns the ModDialog_ToggleMod_Impl crash"* / implicit | **YES — and already corrected once.** The callee wall my brief warned about does not exist and IOPORT already said so. Not ported. |
| 6 | `?Frame@MainLoop@Engine@@` | *"NOT ported (683 bytes, its own lane)"* — `mainloop_run.cpp:88, 332` | **neither** | **NO — THERE IS NO BLOCKER.** Pure scoping deferral. No technical obstacle is stated anywhere in the tree. An unclaimed row that has read as a blocked one for fifteen waves. |
| 7 | `?Initialize@GoalList@Achievement@@` | *"Blocked on Object::Factory's lookup + CreateNewObject + Cast<>, i.e. the Wave 11 object-creation path. Do not scope it as an Achievement row."* — `WAVE23_LANE_ACHIEVE.md` R1 | integrator / none | **OVERSTATED — see §2.2.** The inlined mechanism has a **ported, registered precedent since Wave 3**. Not ported. |
| 8 | `?PlatformShutdown@Device@Input@@` | blocked on unexported `0x18000ABA0`, `0x180009C30`, `0x18000ADC0` — `WAVE23_LANE_TAIL.md:441` | Wave 24 kickoff writer / none | **NO — FULLY STALE.** All three are ported and registered today. **PORTED.** |
| 9 | `??2SpriteSheet@UI@@SAPEAX_K@Z` | **none recorded anywhere** | — | n/a — simply unclaimed. **PORTED.** |

Tally: of nine recorded positions, **#2 and #8 are outright false**, **#6 and #9
are not blockers at all**, **#7 is overstated**, and **#1 licenses far less than
it was read as licensing**. Only **#3, #4 and #5** survive intact — and #5 is
itself a correction Wave 23 already published that my own brief still carried
forward.

### 2.1 `??1Stream@Audio@@` — the blocker was false, and *why* it was false generalises

`WAVE23_LANE_TAIL.md:194` says, verbatim:

> The dtor is portable *except* for `CALL 0x18000C4A0` on the member at `+0x30`,
> which is **not exported** (`pefile`, checked) — an in-DLL call with no door,
> dead memory at cutover. **Porting the pair requires porting that helper first.**

The *fact* is right: `0x18000c4a0` is unexported, and my own grep confirms no
body for it anywhere in the tree. **The inference is wrong.** "Not exported" does
not mean "no door" — `ResolveAddr(Module::Runtime, addr)` **is** a door: it
consults the port registry FIRST and falls through to the DLL when nothing is
registered (`reference_resolveaddr_consults_port_registry_first.md`). That is
exactly what the `deleting_dtor.cpp` family already does with its unported
callees, and exactly what `WAVE23_LANE_IOPORT.md §2.4` records for
`??1PackageFile@`'s own unported helper — **in the same directory, in the same
wave, about the same problem.**

So porting the pair required a door, not a prior port. Cost of the error: two
rows held for a wave.

The residual that *is* real is narrower and belongs in a different column: at
cutover, with no DLL present, `0x18000c4a0` resolves to nothing. That is a
cutover-publish item, not a port blocker.

> **HANDOVER — port or mint `0x18000c4a0`** (an `if (p) { MemFree(p); } p=0;
> p2=0;` heap-buffer release, ~0x70 bytes).
> **[SCOPE: CUTOVER-ONLY]** — with the DLL present the door falls through and the
> behaviour is retail's own.
> **[ADDRESSEE: Wave 24 lane CUTOVER, or whichever lane next runs a DLL-absent
> boot that reaches `Audio::Stream`'s destruction.]**

### 2.2 `?Initialize@GoalList@Achievement@@` — overstated, but I did NOT finish checking it

`WAVE23_LANE_ACHIEVE.md` R1 scopes this into "the Wave 11 object-creation path…
202 `?Create*Object@` exports, zero ported, all 104 minted factory pointers still
abort traps".

**What I verified myself:** the mechanism this body inlines — walk the
`Object::Factory` tree by type id, then dispatch through the found record's
`BaseTypeInfo+0x20` — **is already ported, registered by BOTH doors, and has been
since Wave 3**: `runtime/src/object/type_system.cpp:334`
`RtObject_Factory_CreateObject`, registered at `type_system.cpp:1211` (name) and
`:1244` (address `0x1800c0ae0`). So "the subsystem is entirely unported" is not
true of the part this row needs.

**What I did NOT verify, and am not asserting:** a sub-agent of this lane reports
that `Achievement::Goal`/`GoalInstance` are *exe-side* types registered through
`REGISTER_TYPE` in `code/Game/Goal/*.cpp` with real factories, so the "104 minted
factories are abort traps" fact does not apply to this caller's runtime types. I
did not check that, and **a discharge does not propagate — including one handed
to me by another agent** (KICKOFF §5 applies to cross-lane notes too). It is
recorded here as REPORTED, NOT VERIFIED.

> **HANDOVER — re-scope `?Initialize@GoalList@Achievement@@` (389 bytes).**
> Verify the one open leg above, then treat it as an ordinary body port, NOT as a
> Wave 11 creation-path row.
> One real hazard survives and must not be lost: it **frees and NULLs the
> caller's by-value `Array` on exit**, so it is replay-unsafe and needs a
> single-shot harness rather than a before/after diff.
> **[SCOPE: PERMANENT — this is about how the row is scoped, and is true DLL-present and at cutover alike.]**
> **[ADDRESSEE: Wave 24 lane TINFO, whose subject is exactly whether promoted `?_typeInfo@` rows are inert or wrong, and which therefore already owns the instrument that settles the open leg.]**

---

## 3. THE FOUR PORTS

| row | file | kill switch | doors |
|---|---|---|---|
| `??2SpriteSheet@UI@@SAPEAX_K@Z` | `runtime/src/ui/spritesheet_new.cpp` | `BANISHED_RT_UISPRITESHEETNEW` | name + address |
| `??0Stream@Audio@@QEAA@XZ` | `runtime/src/audio/audio_stream_win.cpp` | `BANISHED_RT_AUDIOSTREAM` | name + address |
| `??1Stream@Audio@@QEAA@XZ` | same | same | name + address |
| `?PlatformShutdown@Device@Input@@AEAAXXZ` | `runtime/src/input/input_platform_shutdown_win.cpp` | `BANISHED_RT_INPUTSHUTDOWN` | name + address |

Absence was established by **ADDRESS and by NAME** before a line was written, in
every case, scoped to `runtime/ code/ standalone/ engine/ dx11/` — never at the
repo root, because sibling lane worktrees inflate an unscoped grep with no
symptom (KICKOFF §1a). All hits found were consumers.

Every body was read from the **disassembly**, not the decompiler
(`reference_ghidra_c_output_vs_disassembly.md`). Every KERNEL32/DINPUT8 callee was
named from the DLL's **own import table**, not guessed.

### 3.1 `??2SpriteSheet@UI@@` — verified against its sibling BY BODY, not by resemblance

It is instruction-for-instruction `??2ToolTipDialog@UI@@SAPEAX_K@Z`
(`0x18004cfd0`), which `tooltipdialog.cpp` already ports — and that file's banner
carries the exact warning that made me check: *"context_ctor.cpp records the same
check, and found a sibling using align 4."* So the two were **disassembled side by
side**: same 99 bytes, same mnemonics, same immediates, including `MOV R8D,0x8`.
The only differing bytes are rel32/RIP displacement fields, which encode a
DISTANCE and must differ between two copies at different addresses.

**It has a live route in our own tree:** `saveload_draw.cpp:601` calls it with
`0x58` on the save/load screen. UI reaches **99.0% and AT CEILING**.

### 3.2 `Audio::Stream` — the "every element or only one" check

The brief warns that a prescribed dtor port once swapped a body destroying every
element for one destroying only the first. The dtor was enumerated from the
listing: **six `CloseHandle` calls** — the two `Stream` events at +0x48/+0x40,
and **all four** of the `System::Thread` base's handles at +0x10/+0x18/+0x20/+0x28
(matching `runtime/types/system/Thread.h`, which names exactly four). Nothing
closed twice, nothing left open.

The inlined base teardown was **proved, not assumed**, to be
`??1Thread@System@@QEAA@XZ`: the shipped body at `0x1800ca590` was disassembled
beside it and is the same twelve steps in the same order. That body is already
ported, so this port **calls** it rather than adding a thirteenth hand copy.

### 3.3 `Input::Device::PlatformShutdown` — the asymmetry a decompiler would erase

The body's one real feature: `+0x11E8` and `+0x11E0` get **both** `[vtbl+0x40]`
and `[vtbl+0x10]`; `+0x11D8` gets `[vtbl+0x10]` **only**, as a tail jump. Folding
the three into a loop would destroy exactly that.

It is not an anomaly, it is the type system: `+0x10` is `IUnknown::Release`
(all three have it), `+0x40` is `IDirectInputDevice8::Unacquire` (only a *device*
has it). **`+0x11D8` is the `IDirectInput8` FACTORY**, which was pinned **by its
writer, not by resemblance** — `PlatformConstruct` at `0x1800e18a3` calls
`DINPUT8.dll!DirectInput8Create` with `LEA R9,[RCX+0x11d8]` as the out-parameter,
then uses `[vtbl+0x18]` (`CreateDevice`) twice to fill the other two. A factory
was never acquired, so there is nothing to unacquire.

`+0x11D8` is **new to `runtime/types/input/Input.h`** and corrects that header's
"There are exactly TWO COM pointers", which was one field short. It was found by
a whole-image byte-pattern scan for the displacement — a screen for which a
**positive control exists and was used** (KICKOFF §3 rule 4): the same scan for
`+0x11E0` and `+0x11E8` re-finds the two reads the header already names
(`0x1800e1d15`, `0x1800e1f81`), so the `+0x11D8` hit list is a live parser's
output and not a dead one's silence.

The three erased callbacks are WM_SETFOCUS (7), WM_KILLFOCUS (8) and
WM_MOUSEWHEEL (0x20A) — corroborated independently by `Input.h` already naming
`OnFocusMessage`/`OnLostFocusMessage`. **Zero unported callees.**

**One deliberate divergence from retail, declared:** retail dereferences
`[0x1801fc678]` unguarded; my port counts a null singleton instead
(`g_noApp`). Per CLAUDE.md a guard is permitted only where our standalone
genuinely differs — our shutdown is reachable on a path where the `Application`
singleton was never published, and retail's is not. The counter makes the
difference **visible** rather than silently skipped.

---

## 4. PROBE SHAPES (KICKOFF §1c) AND THEIR CONTROLS

I have **no game-directory lock**, by design. All sites are written to be
exercised by lane **DIFF**'s armed `BANISHED_RT_DIFF=1` sweep.

### 4.1 `??2SpriteSheet@UI@@` — a REAL two-sided oracle, `RT_DIFF_SCALAR`

**Shape: shadow/diff site exercised by someone else's run — but an ORACLE, not a
self-check.** Deliberately **not** `RT_DIFF_SHADOW`: the ~250 shadow sites take
the port's own verdict and never consult the DLL, so a shared misunderstanding of
retail passes every time (`reference_shadow_marker_is_not_an_oracle.md`).

The naive oracle fails because `operator new` returns a POINTER and two calls
necessarily return different ones. **What is compared instead is the allocator's
own bookkeeping DELTA** across the call — `_allocatedBytes` (+0x630) and
`_allocCount` (+0x638), both written by `Allocate` and by nothing else on this
path. A delta is position-independent, which is exactly why it survives the
pointers differing. The shipped body is reached with `RtOriginal`, never
`ResolveExport`, which would now hand back our own function and compare retail
with itself (`reference_registering_an_address_converts_a_selfcheck_into_an_oracle.md`).

- **Effectiveness counter:** `g_probePairs` (comparisons that actually happened)
  and `g_probeSkipped`. On `pairs=0` the summary line **says the oracle is dead**
  rather than letting a clean `divergences=0` be read as a pass.
- **Negative control** `BANISHED_RT_SPRITESHEETNEW_CONTROL=1`: our body requests
  `bytes + 0x20`. It corrupts `_allocatedBytes`, which **is** in the compared
  set; it leaves the returned pointer, which is **not** compared, alone.
- **A control knob I REJECTED after checking it** (rather than shipping a dud):
  corrupting `align` 8→4 changes **nothing**, because `Allocate` computes
  `align8 = (align + 7) & ~7` and both give 8. It would have reported NOT CAUGHT
  and indicted a sound instrument
  (`reference_control_must_pick_a_compared_subject.md`).

### 4.2 `Audio::Stream` and `Input::Device::PlatformShutdown` — SELF-CHECKS, labelled as such

Both are `RT_DIFF_SHADOW`, and **the word SELF-CHECK is in the site name itself**
so a sweep report cannot count them as oracles. The other four shapes were each
ruled out **by a named fact**, not by "no run, therefore STRUCTURAL" (the failure
ten Wave 23 lanes made independently):

- **input sweep** — needs a pure function; none of these is (OS handles, COM, a live AVL tree).
- **synthesised oracle** — for `Stream`, a fabricated subject is a **fake, not a synthetic**: the base ctor `_beginthreadex`s a real worker. This is Wave 9's holdback, re-checked and **upheld**. For `PlatformShutdown` it is *buildable* (three stub `IDirectInputDevice8` vtables) and is the right follow-up — handed forward, not claimed.
- **two-sided A/B** — a destructor and a once-only shutdown cannot be replayed: running theirs after ours double-closes six handles / double-Releases three COM objects.
- **write-path replay** — none is a `Serialize`.

Controls and effectiveness counters:

| site | effectiveness counter | negative control | corrupts a COMPARED subject? |
|---|---|---|---|
| `Stream` ctor | `g_ctorChecks` (predicate evaluations) | `BANISHED_RT_AUDIOSTREAM_CONTROL=1` skips the `[+0x38]=0` store | **yes** — `+0x38` is in the checked set; handle *values* deliberately are not, since they legitimately vary per run |
| `Stream` dtor | `g_dtorChecks` | — | — |
| `PlatformShutdown` | `g_erased` / `g_missed` (descents that HIT) | `BANISHED_RT_INPUTSHUTDOWN_CONTROL=1` skips the WM_MOUSEWHEEL erase | **yes** — the post-condition looks up all three keys; the COM pointers are **not** examined, because after `Release` reading one is a use, not a check |

### 4.3 LANE DIFF'S SWEEP ARRIVED MID-LANE; HERE IS WHAT IT CHANGED

Lane DIFF measured that **286 of 630 shadow sites (45%) cannot catch a shared
misunderstanding of retail** — 133 of them structurally incapable of printing
anything but `divergences=0` — and warned specifically that
`IO::MemoryStream::MemoryStream`'s oracle is permanently red because it compares
a **vptr at +0x00 that MUST differ** between our table and retail's.

Checked against my three sites:

- **`??2SpriteSheet@UI@@` is in the 55%**: it resolves the shipped body with
  `RtOriginal` and compares against it. A genuine oracle, not a self-check.
- **Both `RT_DIFF_SHADOW` sites already carry `[SELF-CHECK, not an oracle]` in
  the site NAME**, so the next sweep classifies them honestly without needing to
  infer it. Neither is ATTENDANCE — neither passes a literal `true`.
- **I copied no IO ctor/dtor oracle template**, so the `MemoryStream` defect was
  not inherited.
- **`RT_DIFF_REPLAY_PRE/POST` has zero adopters and I am not a first one.**

**One real strengthening followed.** My `Stream` ctor self-check does read the
vptr — but it never compares ours against retail's; it compares the field against
the value *this body wrote*, which made it close to a tautology. It now also
asserts the vptr is **not** `??_7Thread@System@@6B@` (`0x180155f20`), which turns
a near-tautology into a live test of the one failure mode that is actually
available here: our `Stream` stamp landing *before* the base ctor instead of
after, so the base's stamp wins. That term has its own message in the failing-term
chain.

Each predicate **names its failing term** rather than restating the verdict
(`reference_shadow_predicate_must_name_its_failing_term.md`). Attendance counters
are separate from diff `calls` in all three files, because a body that ran but
never compared prints `calls=0`, which reads as "never ran" and means the
opposite.

---

## 5. THE MINT ROW I DELIBERATELY DID **NOT** TAKE

`??_7Stream@Audio@@6B@` (`0x180154950`) **is not in the mint table** — re-derived
here (`grep -n '180154950\|Stream@Audio@@6B' standalone/rt_data_mint_tables.inc
standalone/reg_mint_promoted.cpp` → zero hits), not accepted from lane CUTOVER's
note that said the same.

Minting it is **out of scope**: `rt_data_mint_tables.inc` is GENERATED, and
regenerating it mid-wave would capture six other live lanes — the reason
`tooltipdialog.cpp` gives for leaving its own row to the wave owner.

> **HANDOVER — mint `??_7Stream@Audio@@6B@` (`0x180154950`).**
> **[SCOPE: CUTOVER-ONLY]** — with the DLL present the door resolves to the DLL's
> own table, which is what retail stamps anyway, so this row is harmless today.
> **[ADDRESSEE: Wave 24 lane CUTOVER, whose attempt-25 boot faults at this very
> ctor, or the wave owner at the close regeneration — whichever comes first.]**

---

## 6. A STALE COMMENT CORRECTED IN PASSING

`singleton_ctors.cpp:719` claimed `0x180157c60` "is not in lane C2's mint table
… and will be null at cutover". It **is** in the table
(`rt_data_mint_tables.inc:608` and `:1542`, 7 slots, address-keyed). Flagged by
lane CUTOVER, **re-derived by me before editing**, and corrected in place.

I explicitly did **not** extend the correction to `Persistence`'s
`0x180155860`, which the same paragraph bracketed with it: I did not check it,
and the comment now says so. A verification that was genuinely run must not be
recorded against a wider population than it examined
(`reference_a_real_check_widened_to_a_second_symbol.md`).

---

## 7. WHAT I DID **NOT** ESTABLISH

1. **I ran nothing.** No game-directory lock, by design. **Zero of my three
   probe sites has ever been executed.** Every control in §4 is reasoned from the
   disassembly and the macro contracts; none has been fired. A control that has
   not run is a design, not a result.
2. **The four ported bodies are UNVERIFIED.** The build is clean and the census
   moved, and **a build check cannot detect a misregistered port** (KICKOFF §3
   rule 7). Correctness rests on the two-sided disassembly reads in §3.
3. **`??2SpriteSheet@UI@@`'s oracle is untested as an instrument.** Its
   effectiveness counter exists precisely because I cannot yet tell a passing
   screen from a dead one.
4. **Five of nine rows are not ported**: `?Update@Scene@Audio@@` (1816 B),
   `?Frame@MainLoop@Engine@@` (684 B), `?Initialize@GoalList@Achievement@@`
   (389 B), `??1PackageFile@IO@@` (318 B), `?SetupFileSystem@` (2617 B). Budget,
   not blockers — except #3, #4 and #5 in §2, whose holdbacks survive.
5. **§2.2's open leg is unverified** — whether `Achievement::Goal`/`GoalInstance`
   really are exe-side `REGISTER_TYPE` types outside the abort-trap population.
   Reported by a sub-agent, **not checked by me**, and handed to lane TINFO.
6. **I did not re-check `??1PackageFile@IO@@`'s blocker independently.** I read
   IOPORT's §2.4, found it well-scoped, and accepted it. Given that this lane
   found five of nine positions defective, **accepting one on the strength of it
   reading well is exactly the move this wave is about** — it is listed here as
   an acceptance, not a verification.
7. **I did not measure whether `?Frame@MainLoop@Engine@@` is *safe* to port**,
   only that nothing blocks it. It is the process's main loop; "no stated
   blocker" is not "low risk".

---

## 8. HANDOVER, EACH ITEM WITH A NAME

| # | item | addressee | scope |
|---|---|---|---|
| H1 | Port/mint `0x18000c4a0` (Stream dtor's `+0x30` release) | Wave 24 lane CUTOVER | CUTOVER-ONLY |
| H2 | Mint `??_7Stream@Audio@@6B@` (`0x180154950`) | Wave 24 lane CUTOVER, or the wave owner at close regeneration | CUTOVER-ONLY |
| H3 | Re-scope `?Initialize@GoalList@Achievement@@` off the Wave 11 creation path; verify the one open leg; keep the replay-unsafe array-free hazard | Wave 24 lane TINFO | PERMANENT |
| H4 | `?Frame@MainLoop@Engine@@` has **no blocker at all** — 684 bytes, 3 chained fragments, unclaimed since Wave 9 | the Wave 25 kickoff writer | PERMANENT |
| H5 | `?Update@Scene@Audio@@` — Audio's last BODY row; the holdback is an ORACLE one and Wave 23 names the precedent for porting without one | the Wave 25 kickoff writer | PERMANENT |
| H6 | Fire my three probes' controls in an armed sweep and report `probePairs`/`erased`/`*Checks` | Wave 24 lane DIFF | this wave |
| H7 | `.pdata` entry-only sizing understates `?Frame@MainLoop@` **5.6x** with `chained=False` on the entry fragment. Any future extent claim must union forward | anyone sizing a Runtime function | PERMANENT |

---

## 9. THE LESSON THIS LANE WOULD ADD

**"Not exported" is not "no door", and the inference is made silently.**

`WAVE23_LANE_TAIL.md` held two rows on *"an in-DLL call with no door… porting the
pair requires porting that helper first."* Every **fact** in that sentence is
correct. The **inference** is wrong, and it was already refuted — in the same
directory, in the same wave, about the same problem — by
`WAVE23_LANE_IOPORT.md §2.4`, which routes `??1PackageFile@`'s unported helper
through `ResolveAddr` without comment, because that is routine.

This is the Wave 23 §6 defect class with a twist worth naming: **the discharge
that failed to propagate was not in an older wave, it was in a SIBLING LANE of
the same wave, and neither lane could see the other.** Parallel lanes do not read
each other's reports, and the close merges their *code* without reconciling their
*verdicts*. `?PlatformShutdown@`'s blocker (#8) failed the same way — three
helpers named as walls, all three ported by other lanes.

**A blocker that names a DEPENDENCY is the most perishable kind**, because
dependencies are exactly what other lanes are busy porting. Of my nine rows,
**every dependency-shaped blocker was false** (#2, #8) or overstated (#7); **every
oracle-shaped blocker survived** (#1, #3, #4). That split is sharp enough to use
as a triage rule.
