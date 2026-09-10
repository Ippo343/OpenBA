# WAVE 18 — KICKOFF: drive Video to its measured ceiling

**Written 2026-09-10 by the advisor at `dacc015`. Dispatched.**
Read `WAVE18_VIDEO.md` first (the plan) and `WAVE17_LANE_CEIL.md` §3 (the
denominator). This document does one thing the plan does not: it **names all 23
rows** and partitions them into lanes.

## 0. A scope correction, stated up front

`WAVE18_VIDEO.md` scopes Wave 18 as a **cutover** wave — six ctor/dtor bodies,
aimed at the boot wall — and says explicitly *"do not scope this wave against
Video's percentage."*

**This wave is scoped against Video's percentage anyway, deliberately.** The
objective is: **Video 72.7% → 88.0%, its measured ceiling.**

That is not a contradiction of the plan, it is a **superset of it**. The six
blocker bodies are 6 of the 23 rows, and they stay lane 1 with first priority
precisely because they are also the boot wall. The plan's warning was written
before lane CEIL had measured anything; CEIL then showed Video's real gap is 23
rows and not 41, which makes "drive it to ceiling" a bounded, scopeable target
rather than the open-ended census chase the plan was right to refuse.

**What the plan's warning still buys, and it is kept:** *do not follow this wave
with more Video.* At ceiling, Video is finished. IO (gap 34) and Graphics (gap
29) are the larger real work and are the next wave's subject, not Video's
remainder.

## 1. The 23 rows, named

`tools/runtime_census.py --namespace Video --unported` on `dacc015`:
41 unported, 18 tagged UNPROMOTABLE by CEIL (12 NO-OP + 6 HARMFUL), **23 left.**
Do not touch the 18 — four of Video's six HARMFUL rows are *deliberately* on the
DLL and promoting one introduces a defect (CEIL §3.2).

### Tranche A — ctor/dtor (10)

| # | symbol | note |
|---|---|---|
| 1 | `??0GraphicsInterface@Device@Video@@QEAA@XZ` | **boot wall** |
| 2 | `??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z` | **boot wall** |
| 3 | `??1GraphicsInterface@Device@Video@@QEAA@XZ` | **boot wall** |
| 4 | `??0Adapter@Device@Video@@QEAA@XZ` | **boot wall** |
| 5 | `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | **boot wall** |
| 6 | `??1Adapter@Device@Video@@QEAA@XZ` | **boot wall** |
| 7 | `??0RenderBuffer@Video@@QEAA@XZ` | |
| 8 | `??1RenderBuffer@Video@@QEAA@XZ` | |
| 9 | `??1Configuration@Video@@QEAA@XZ` | |
| 10 | `??1Device@Video@@QEAA@XZ` | |

### Tranche B — CODE (6)

| symbol |
|---|
| `?AdjustDisplay@Device@Video@@QEAAXXZ` |
| `?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z` |
| `?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z` |
| `?Bind@RenderBuffer@Video@@QEBAXAEAVContext@2@@Z` |
| `?Recreate@ImageBuffer@Video@@QEAAXXZ` |
| `?SaveImageToFile@ImageBuffer@Video@@SA_NAEBVFilename@System@@HHPEAI@Z` |

### Tranche C — DATA (7)

| symbol | CEIL verdict |
|---|---|
| `??_7ImageBuffer@Video@@6B@` | BLOCKED — 10 slots / 3 ported / **7 TRAP** |
| `?_typeInfo@ImageBuffer@Video@@0VBaseTypeInfo@Object@@A` | BLOCKED on attendance |
| `?_typeInfo@PixelProgram@Video@@…` | " |
| `?_typeInfo@RenderState@Video@@…` | " |
| `?_typeInfo@VertexLayout@Video@@…` | " |
| `?_typeInfo@VertexProgram@Video@@…` | " |
| `?_typeInfo@VideoProgram@Video@@…` | " |

The six typeinfos have **exactly one consumer in the tree**: the ten-element
`ids[]` at `runtime/src/engine/mainloop_video.cpp:487-499` inside
`RtEngine_MainLoop_OnVideoError` — device-lost recovery, which no harness route
drives. CEIL §5.4: one synthesised `OnVideoError` call decides all ten (six
Video, four Graphics) in a single run. Readiness and value evidence for all ten
is already green; **the factory-trap hazard does not apply** (CEIL §2.6, and it
is structural, so it exonerates every `?_typeInfo@` row).

**10 + 6 + 7 = 23.** Arithmetic checked; it reproduces CEIL's headline exactly.

## 2. Lanes

Three port lanes in **private worktrees, already created** — plus GATE, which
runs after GFXIF merges.

| lane | worktree | branch | rows |
|---|---|---|---|
| **GFXIF** | `C:/code/GIT/wt-w18-gfxif` | `wave18/gfxif` | 9 — the 6 blockers + AdjustDisplay + FindOrAddMode + GetDisplayMode |
| **VRES** | `C:/code/GIT/wt-w18-vres` | `wave18/vres` | 7 — RenderBuffer ctor/dtor/Bind, ImageBuffer Recreate/SaveImageToFile, `??1Device`, `??1Configuration` |
| **VDATA** | `C:/code/GIT/wt-w18-vdata` | `wave18/vdata` | 7 — the `??_7ImageBuffer` traps + the 6 OnVideoError typeinfos |
| **GATE** | (later) | — | 0 rows; re-run the cutover boot gate, **set diff** vs attempt 24's 17 |

GFXIF's nine are one cohesive cluster: Adapter/GraphicsInterface are the
enumeration value types, and AdjustDisplay/FindOrAddMode/GetDisplayMode are the
`Device` methods that build and read them. Splitting them would put two lanes in
the same three files.

**GATE does not run until GFXIF is merged.** Its job is to say whether the wall
moved and to where — not to port. It must carry attempt 24's own warning: a
cutover boot is a **GATE, not a PLANNER**; its residue is a lower bound on the
remaining work, never the work list.

## 3. The shared-resource constraint

Source is isolated by worktree. **The game directory is not.** One
`C:/GOG Games/Banished`, one `reimpl-x64.exe`, one set of logs.

* `tools/game_dir_lock.sh acquire <lane>` — **never piped.** A pipeline exits
  with the *last* command's status, so piping the acquire into `tail` makes a
  TIMEOUT read as a GRANT. One lane deployed over another mid-run that way.
* Check its **own** exit status, and gate every deploy on
  `assert-held <lane>`.
* **Never background a blocking acquire** — the lock is held from the moment it
  is granted, not when you notice; that cost 19 idle minutes of a wave's
  critical path.
* Hold it for a **run batch**, not a single run, and release promptly.

Three lanes contending for one lock is the real throughput ceiling of this wave.
Do the reading, the disassembly and the porting first; batch the runs.

## 4. Doctrine — restated because each item has cost this project a wave

1. **Prose is a hypothesis; a grep is the answer.** Before writing a body, grep
   the mangled name AND the retail address across `code/ engine/ runtime/
   standalone/ dx11/`. Three times in one wave a lane ported what the tree
   already had. A duplicate registration is last-binding-wins, so it silently
   makes one body dead code.
2. **A body ported but never registered is indistinguishable from an absent one**
   to the census. Grep BOTH the address and the mangled name, by symbol not by
   class string.
3. **A census `unported` row has four meanings** — absent / gated / unregistered
   / storage-never-written. They imply completely different work. Decide which
   before planning.
4. **`AEAA` means PRIVATE, not unexported.** This DLL exports its private
   members. An address door justified by the word "unexported" in a comment is
   usually a plain name door. The export table settles it in seconds.
5. **Register by name AND address.** 13 live sites reach Runtime symbols through
   `ResolveAddr(Module::Runtime, …)`, which is registry-first —
   `ResolveAddr(Module::Runtime, addr)` is NOT "the DLL".
6. **`ResolveExport` falls through to the raw impl for an unported symbol,
   silently**, and a name the DLL does not contain returns NULL silently.
7. **Read the disassembly, not Ghidra's C.** Ghidra's decompiled C drops
   dereferences, prints MOVSXD as a zero extension, and drops whole arguments at
   a mistyped vtable call — four defect classes, two crashes, in one lane.
8. **Do not "fix" a faithful crash with a null check.** Find the upstream null.
9. **An instrument outside its subject's live window prints a plausible ZERO,
   not an error.** Every screen needs a positive control that MUST read
   non-zero, verified to print before any zero is trusted.
10. **A control must be scored separately** with its own catch rate, must
    produce a comparable divergence rather than UB, and must catch **via the
    mechanism it names**. A control scored on the union of two screens hides a
    blind one.
11. **`calls=0` has four causes and they invert the conclusion.** Read the
    oracle's `calls=` column, not just `divergences=`; and `calls=0` with
    `worked>0` means the body ran every time and always took the branch the
    oracle does not cover.
12. **`BANISHED_AUTOQUIT` overrides `BANISHED_AUTOSTART` and `BANISHED_AUTOLOAD`**
    — it replaces them, it does not compose. `[AutoStart]` lines go to **stderr**,
    not the debug log.
13. **The mods-menu route is broken on this tip** (`ModDialog_ToggleMod_Impl+0x43`,
    read of null+0xd0). Do not plan around it. Evidence:
    `docs/runtime_port/wave17_modsroute_crash.txt`.
14. Build **both** `standalone-debug` and `standalone-perf`;
    `make check-log-strip` fails the stripped builds on qualified `std::printf`.
15. **Commit early and often.** A machine restart destroyed one lane's entire
    uncommitted session last series.
16. **No heredocs** for file content or commit messages — a heredoc silently
    rewrites `\b` to a 0x08 byte, splits string literals on `\n`, and runs
    backtick command substitution. Use `Write` + `git commit -F`.
17. **Report your worktree PATH and branch name** in every status update, and
    **stop editing once you report** — a lane that keeps improving after it
    reports invalidates the integrator's measurement.

## 5. What "done" looks like

* Census satisfies the **AT CEILING identity** with no row left untriaged.
  **NOT a percentage.** Corrected 2026-09-10 by lane VDATA and verified against
  `tools/runtime_census.py:1230-1232`: `ceiling = (called - unpromotable) /
  called`, so **both halves move whenever any lane opens a door**, and
  `AT CEILING` is really the absolute count identity
  **`ported >= called - unpromotable`** — the percentage is just its lossy
  rendering. VDATA's demonstration: closing the seven `??_7ImageBuffer` trap
  slots moved Video 150/109 -> 152/110, so **ported% went DOWN (72.7 -> 72.4)
  while the printed ceiling went UP (88.0 -> 88.2) with no triage change at
  all** — one new row, `?PlatformSerialize@ImageBuffer@Video@@`, the callee
  `Serialize` reaches. With three lanes opening doors at once, **quote the
  identity and the set-diff of what entered `called`, never a percentage
  delta.** '88.0%' in §0 is the target as it stood at `dacc015` and is
  already stale; it is left there rather than rewritten so this correction
  stays legible.
* Every ported row has a **verdict with evidence**, not a registration. An
  UNTRIAGED caller-less port is not evidence of anything; it is work nobody has
  done.
* A row a lane cannot promote gets a **written verdict with a class (NO-OP /
  HARMFUL / BLOCKED) and an expiry condition** — and **BLOCKED is not a
  ceiling.** CEIL predicted 40 NO-OPs and measured 12 because it asked whether
  promotion would *matter* before asking whether it was *possible*.
* GATE reports the new wall by **set diff**, and scopes the next wave with three
  separate bounded screens (predicted walls = upper bound, residue = lower
  bound, census gap = lossy both ways).

---

## 6. Corrections from the lanes (appended as they report)

### 6.1 The wall needs NINE functions, not six — lane GFXIF, `57ed42d`

`WAVE18_VIDEO.md`'s headline is *"The wall: six functions"*. **It is nine.**

Mechanism, measured at `dx11/src/devicedata.cpp:216-300`: `GetImports()` resolves
a **50-entry** table and sets `g_imOk = false` if **any** entry is null.

> **CORRECTED 2026-09-10 by lane GATE: the table is 50 entries, not 40.** GFXIF §3
> said 40 and *this section repeated it without checking* — the integrator
> propagating an unverified number is the same failure this wave documents six
> times over. Counted directly: 50 `{ (void**)&g_im.` table entries, and 50
> `Imports` pointer fields, consistent with attempt 24 naming a null at field #28.
> **No conclusion moves** — `g_imOk` is an AND over the table at any length — but
> it is the **denominator of GATE's headline zero**, so every zero in that report
> now carries one: 50/50 imports resolved and 50/50 struct fields non-null,
> against 9 unresolved and 9 null in attempt 24.
`EnumerateInterfaces` then does `if (!im) return Orig<...>(1)(self)`, and with the
DLL absent that `Orig` is null — **that is the VCALL-of-0.** `AdjustDisplay`,
`FindOrAddMode` and `GetDisplayMode` are entries in the same table, so `g_imOk`
is an **AND over all nine**.

**A lane scoped to the plan's six would have finished green and left the wall
exactly where it is.** §2 grouped all nine into GFXIF for cohesion; that grouping
turns out to be **load-bearing, not cosmetic — do not let a future brief re-split
them.**

Related, and it decides the registration question: `Dx11ResolveRuntimeImport`
tries `ResolveExport(<mangled>)` **first**, so this is a **NAME-resolution wall.**
Registration by address alone would not have moved it.

### 6.2 Four of the six `Collection::Array` helpers were already ported

By Wave 9 lane VID2. Neither the plan nor this kickoff said so, and it is most of
the reason lane GFXIF was cheap. This is the standing hazard from §4.1-4.2 landing
in this wave's favour for once: **grep for the port before planning to write it.**

### 6.3 Layout facts the header understates

`runtime/types/video/Device.h` calls `Adapter+0x2c/+0x30` *"meaning not
recovered"*, while `dx11/` already names them `dxgiAdapterIndex` /
`dxgiOutputIndex`. `Device+0xA0` is an `HMONITOR`. Two files in this tree knew
more than the header did.

### 6.4 Both brief traps resolved as described

`0x1800f7100` **is** the real `??0Adapter` body (14 instructions, disassembled),
not a thunk; and the DX11 address column is the IAT slot range, left untouched.

### 6.5 `tools/abi_retslot_audit.py` was reading two files — fixed at `fa85957`

It hardcoded `engine/funcs/engine_funcs.{cpp,h}` and printed *"0 suspicious of 49
by-value-returning exports"* with nothing saying so. It now walks five roots,
prints its **subject set**, names **UNCHECKED** assignments individually, and
**sizes its blind spot**: 56 of 133 distinct by-value-returning mangled names in
the tree reach a door it can read; **77 do not.** Checked 49 → 61, still 0
suspicious — the widening exposed no latent defect, it exposed that the number
was over a subject nobody had stated. Positive control 1/1, and the control
subject sits in a file the old version could not read.

**The general lesson, and it applies to every screen this project owns:** a tool
that cannot name its own subject invites a lane to cite its green for something
it never looked at. Sizing a blind spot is not optional garnish — an unsized
blind spot lets a partial green read as a whole one.

### 6.6 The ten OnVideoError typeinfos DO have a harness route — lane VDATA

**CEIL §3.4 and this kickoff's §1 are both wrong** to say no harness route drives
`RtEngine_MainLoop_OnVideoError`, and **no synthesis is needed.**
`BANISHED_FORCE_DEVICE_LOST=<frame>` (`dx11/src/contextdata.cpp`, Wave 9 lane
ENGIO) injects a synthetic `DXGI_ERROR_DEVICE_RESET` into a `Present` result and
lets the real device-lost teardown run; `WAVE9_LANE_VIDL.md` §4.2 records it
firing and reaching the ported body with `calls=1`, and that body is the sole
consumer of the ten-element `ids[]`.

Pre-registered by VDATA as prediction P1 before it checked anything else, which
is what makes the outcome scoreable rather than rationalisable.

### 6.7 Closing seven abort traps is worth +1, not +7 — lane VDATA

Six of the seven `??_7ImageBuffer@Video@@6B@` slot-target names **are not in
`called` and never were** — `runtime_census.py` excludes vtable slot-target names
in the `.inc`. **The value of that work is that the row can be promoted at all,
not the +1.** Recorded here so the wave summary cannot round it up.

### 6.8 The `SaveImageToFile` holdback note is false AND self-contradictory — lane VRES

`runtime/src/video/buffers.cpp:1415` closes with *"w2_takescreenshot.cpp calls a
different, already-ported capture path — so leaving this on the DLL costs
nothing."* It does not. `w2_takescreenshot.cpp:165` resolves this exact mangled
name and `:270` calls it; the screenshot path goes through this export and
nothing else.

**The note refutes itself.** Its own opening line reads *"One caller
(code/Game/Game/w2_takescreenshot.cpp, the screenshot path)"* — both halves sat
in one comment block for four waves. "Grep the call site" is the cheap lesson;
"the note already disagreed with itself" is the cheaper one.

Its second claim, *"that is Wave 4 (IO) and System::String territory"*, is **half
stale**: the Filename/String/allocator half is all ported today; only the IO half
remains.

**Ruling: row 7 IS ported in this wave** (VRES), so the wave can close on the
§5 identity. But note what that does and does not buy — **porting it does NOT
close the screenshot cutover hole.** Its four IO callees (`?CreatePath@FileSystem@IO@@`,
`??0BinaryFileStream@IO@@`, `?Create@FileStream@IO@@`, `?CloseFile@FileSystem@IO@@`)
are reached through `ResolveExport`, which **falls through to the raw impl for an
unported symbol** — with the DLL absent they are null and the path dies either
way. Porting moves the hole from *no implementation* to *implementation with four
unresolved deps*. **Those four are Wave 19's, and are named here so the IO brief
inherits them rather than rediscovering them.**

### 6.9 `??1Device` and `??1Configuration` have no live route — lane VRES

Their only caller is `RtEngine_Engine_Shutdown` phase 2, which never runs:
`game_main.cpp` calls `ShutdownEngineStaged()`, which stops at step 13 with
*"(14+ deliberately omitted)"*. Both will read **registered-but-uncalled**.
VRES verified them with a **synthesised replay oracle** against the shipped
bodies rather than recording them unverifiable — correct, and the standing rule:
**"no live route" is NOT "no oracle"**, and a registered-but-uncalled row is
exactly where a reader is most tempted to accept a green on trust.

### 6.10 THE PATTERN OF THIS WAVE: a stated blocker is a claim with a timestamp

Three instances in one wave, from three different lanes:

1. **GFXIF** — the plan's "six functions" wall was nine (§6.1).
2. **VDATA** — "blocked on attendance, needs a synthesised call" had a shipped
   harness env var all along (§6.6).
3. **VRES** — the `SaveImageToFile` holdback note is false, self-contradictory,
   and half stale (§6.8).

Every one was written by a competent lane, was true or plausible when written,
and was inherited by later readers without re-checking. **The tree's prose is the
least reliable thing in it.** Re-check a holdback's STATED BLOCKER before you
inherit it, and before you scope a lane around it.

### 6.11 `game_dir_lock.sh acquire` is defeated by ANY compound command — lane VDATA

The banner warned against **piping** `acquire`. It is not only pipes:

```
tools/game_dir_lock.sh acquire lane-FOO 540; echo "ACQUIRE_EXIT=$?"   # reports 0
```

A trailing `;` element makes the step's exit status the **last** command's, exactly
as a pipe does. **This costume is nastier than the pipe**: the echoed text is
*correct* — `$?` really is `acquire`'s status — so the human-readable output says
`ACQUIRE_EXIT=1` while the step's own status says `0`. Two fields disagree and the
authoritative-looking one is wrong, and **the wrapper added to make the status more
visible is what hid it.**

The rule is therefore **not** "do not pipe" but: **give `acquire` a statement of
its own — no pipe, no `;`, no trailing echo, no `&&` tail** — and put
`assert-held` in front of the dangerous action regardless. `assert-held` is what
caught it here, before the lane deployed over GFXIF. **Prefer the self-releasing
`game_dir_lock.sh run` form**, which has no exit code to discard because it leaves
no gap between the check and the action. Documented in the tool's own banner at
the point of use.

### 6.12 `??1Device` does NOT call `??1GraphicsInterface` — lane VRES, correcting the advisor

I told VRES that `??1Device@Video@@` would depend on GFXIF's
`??1GraphicsInterface@` and to rebase to compile against it. **That was wrong.**
The destroy path is **inlined** through `0x1800109d0` → `0x180010f00` →
`0x180010da0` → `0x180010cf0`, all four unexported; no GFXIF-owned mangled name
appears in VRES's files, which make exactly twelve `RegisterRuntime*` calls, all
for their own six rows. VRES verified rather than complied, which saved a rebase
for nothing. **A brief is a hypothesis too, including the advisor's.**

**Carry this to integration:** because the destroy path is inlined, GFXIF landing
`??1GraphicsInterface` does **not** route `??1Device`'s array teardown through it.
They are two independent bodies doing the same work, and a reader will expect
otherwise.

### 6.13 `array_destroy.cpp`'s banner undercounts its own cluster — lane VRES

**Fourteen addresses, not eleven.** `0x180010cf0` is a second address for the
`kD_InnerArr_18` spec; `0x180010da0` and `0x180010f00` are two more of the
recursive shape the banner names once.

**This is the wave's fourth instance of one class**: a tool or file that cannot
accurately name its own contents — `abi_retslot_audit` (two files, unstated),
`runtime_census`'s ceiling (a moving denominator, unstated), CEIL's Graphics
undercount (17 vs 20), and now this. **A screen or a helper file that miscounts
its own subject invites every reader to cite it for something it does not cover.**

### 6.14 An instrument that refuted itself three times — lane VRES, worth copying

VRES's replay probe produced a confident wrong answer **three** different ways
before it was trusted, each caught and each written up where the mistake was:

1. Two nested `memcmp`s that were **use-after-free** — printed artefact
   MISMATCHes, then crashed.
2. Their replacement measured **bytes** released and failed by 24 on a **correct**
   body, because `Free` decrements by an address-dependent block size. Fixed by
   switching to `_allocCount` (`+0x638`), which is **symmetric by construction**
   rather than merely correct-looking.
3. The stride control produced a **wild free** rather than a comparable
   divergence — a control must produce a comparable divergence, not UB.

**The structural point, which the numbers alone will not convey:** a memory
comparator **cannot** see this leak, because each level frees the level below's
buffer, so a body that skips the recursion leaves the outer buffer byte-identical.
Same shape as an oracle comparing bytes neither side writes. What sees it is
`_allocCount`: the LEAK control reads `ours=5 theirs=9` and `ours=3 theirs=21`
where a correct body reads equal. Four controls, scored separately, all green.

### 6.15 SCORING RULE for GFXIF's prediction — fixed BEFORE the data arrives

Lane GFXIF, closing, proposed how an ambiguous GATE outcome should be read:
*"If GATE finds the next null inside `EnumerateInterfaces` rather than after it,
that's the second half holding, not the first half failing."*

**I am not adopting that reading, and the reason is procedural rather than
technical.** It is a rule for interpreting a result, offered by the party whose
prediction it scores, after the prediction and before the measurement. It may
well be the right reading — but this project has already paid for *a comfortable
reconciliation that made both lanes right, retired the flag, and stopped the
search* (`reference_a_reconciliation_retires_the_flag.md`). The defence against
that is to **fix the rule while the outcome is still unknown**, which is what
this section does.

**The rule, by the plain words of the prediction as pre-registered:**

| GATE observes | H1 "the wall MOVES OFF `DeviceData::EnumerateInterfaces`" |
|---|---|
| dies in a **different function** | **TRUE** |
| dies **inside `EnumerateInterfaces`**, different site or fault kind | **FALSE** — the wall did not move off that function |
| dies at the **identical site** | **FALSE**, and no movement at all |

**H2 — "it does not follow that the boot completes" — is scored on one question
only: did the boot complete?** It is independent of H1 and of where the fault
landed. H2 can hold while H1 fails; that is not a contradiction, it is what
"separable halves" means.

**And this is the part that makes the strict reading cheap rather than punitive:
distance travelled is reported SEPARATELY and is not part of either half.** The
runbook already names it as the progress metric — *"the distance travelled per
attempt is the progress metric"*. So a fault that moves deeper inside
`EnumerateInterfaces` is recorded as **real progress on the distance metric with
H1 scored FALSE**, and nothing is lost by refusing to stretch H1 to cover it. A
prediction that can absorb its own near-miss stops being an instrument.

GATE: report all three — H1, H2, and distance travelled — as three separate
lines. Do not merge them into a verdict.

### 6.16 What "sealed" means — it forbids ALTERING, not ANNOTATING

Lane VRES reported and stopped, then took two further tranches when re-tasked,
and its row-7 commit **edited the sealed sections in place** — §0's headline and
row table, §1's case list, §1.2's "three times" to "five times", §3's attendance
table, §6 and §8. It **self-reported** this, named every section that moved, and
declined to hide behind the fact that the boundary message and its commit had
crossed: *"I reported, then kept improving the same sections."*

It then found a genuine tension: the advisor had also asked it to state a
structural caveat **next to** the LEAK control's numbers — which live in a sealed
section — and asked which rule wins.

**Ruling, and it generalises.** The seal protects **the measurement**: what was
known and claimed at the moment of reporting. A caveat on how to READ evidence
already present is not a new measurement. Filing it only in an addendum leaves a
reader of the sealed section with the numbers and without the caveat, which is
the misreading the caveat exists to prevent — **a caveat that lives where the
misreading does not happen is not doing its job.**

So: **one clearly-marked, dated, additive annotation, inserted in place.** Add
nothing else, delete nothing, reword nothing, renumber nothing. A reader can
reconstruct the sealed text by deleting the marked lines, so the audit property
survives — and a marked additive annotation is auditable in a way a silent
rewrite is not. **The silent rewrite is what the rule was aimed at.**

**Two things limited the real damage, worth knowing for next time**: the *code*
was unaffected, and **the integrator re-derived every census number on the merged
tree rather than reading them out of the lane document**, so the integration
measurement never depended on the sealed sections. The loss was the artefact's
audit property, which the repair restores.

### 6.17 An instrument that can say "I MEASURED NOTHING" — lane VRES, five times

Across this lane the probe refuted itself **five** times, and the pattern in every
case is the same: **the effectiveness guard reported NO SUBJECT rather than a
green board.** In one, *the two sides AGREEING* is what pointed at the instrument
rather than the body — both wrote the same filename because the subject had no
extension, so the readback found nothing.

**An instrument that can report "I measured nothing" is worth more than one that
can only report "no difference found."** The two are indistinguishable from the
outside, and this project has repeatedly shipped the second while believing the
first. Compare lane GFXIF's `dtorFree=0` on a 50-case, 0-mismatch, all-controls-
caught run, and lane VDATA's `calls=0` refusal.

### 6.18 GATE's re-score under §6.15 — H1 TRUE, H2 TRUE, and the rule was not stretched

| line | verdict |
|---|---|
| **H1** — the wall moves off `DeviceData::EnumerateInterfaces` | **TRUE** |
| **H2** — it does not follow that the boot completes | **TRUE** |
| **distance travelled** (neither half's) | **+2 named `Engine::Startup` steps** |

**H1 established by ADDRESS RANGE**, as §6.15 required — not by nearest preceding
symbol and not from a crash-log summary field. The null call's return address
`0x1402DEEF9` lies inside `RtEngine_Engine_Startup` `[0x1402DE782, 0x1402DF472)`
at `+0x777`; `DeviceData_EnumerateInterfaces` occupies
`[0x1403F632A, 0x1403F6E3C)`. No overlap, over 1 MB apart.

**Both ambiguous shapes §6.15 said it could not pre-decide were checked, and
neither applies**: `EnumerateInterfaces` has its own non-empty range in this
binary, so it was not inlined into anything; and the complete stack
(`Startup ← RunGameMain ← WinMain ← KERNEL32 ← ntdll`) has no DX11 frame at any
depth. **The rule decided this cleanly. H1 did not need stretching and did not
get any** — which is the outcome the strict reading was betting on.

**H2 scored on its one question alone**: the boot did not complete — exit 3,
`STATE=BOOTING`, `[BootState] RUNNING` never printed. The refutation of the
*reasoning* GFXIF offered for H2 is recorded beside it and **marked as explicitly
not part of the score.**

### 6.19 GATE's own correction: its upper bound was 88, not 92

Having found and sized one blind-spot class in `unsatisfied_export_scan.py`
(named-constant registrations, 19 rows), GATE reported screen 1 as 92. **The tool
had a second class it did not measure** — adjacent string literals split across
source lines — accounting for 8 more departures, including
`?PlayAnimation@MeshInstance@Graphics@@` at `runtime/src/graphics/mesh.cpp:1781`.
Corrected: **111 − 27 = 88.**

GATE's own framing, which is the sharpest thing in it: *"I nearly filed a set-diff
finding off a one-line grep that missed a multi-line registration, and then
reported a different tool's blind spot while carrying an unmeasured instance of
the same class in my own number. The concatenation class was in front of me both
times."*

**Finding a blind-spot class does not mean you have found all of its instances —
least of all the one in your own number.**

### 6.20 The new wall is softer than it looks

`?PlatformConfigure@Engine@1@` (`0x1800a1670`) is **documented at length in
CLAUDE.md**: it opens the Win32 launcher dialog whenever the registry's
`VideoSettings` bit says the previous run never finished `Engine::Startup`, and
`HarnessGuardUnattendedBoot()` in `standalone/main.cpp` already deals with it —
by clearing that bit, not by porting it. GATE's caution that it is *"one row, not
one hour"* stands for the porting work, but the next lane inherits **a full
behavioural description and an existing seam**, which `EnumerateInterfaces` never
had.
