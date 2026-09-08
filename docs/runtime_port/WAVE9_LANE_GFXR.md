# WAVE 9 LANE GFXR — `Matrix::Inverse` closes; the four `Graphics` declines re-measured

**Base:** `fd30180` (merge of lanes FNHOOK and COLL).
**Kill switch:** the port rides `BANISHED_RT_MATH` with the rest of the Math namespace.
**Probe:** `BANISHED_RT_INVERSE_PROBE=1` (positive) / `=neg` (negative control). Off by default.
Date: 2026-09-08.

---

## 1. Verdict table

| # | Symbol | Retail | Verdict |
|---|---|---|---|
| 1 | `?Inverse@Matrix@Math@@QEBA?AV12@XZ` | `0x1800bb310` | **PORTED — closes the row.** 2,240,021 corpus cases + ~1,544 live `SetCamera` calls, 0 divergences; negative control fired at 89.01% (§3) |
| 2 | `?Draw@Renderer@Graphics@@…` | `0x180104bb0` | **DECLINED** — blocker re-measured, still live, and one recorded fact CORRECTED (§4.1) |
| 3 | `?BuildParameters@MaterialInstance@Graphics@@QEAAXXZ` | `0x1801033e0` | **DECLINED** — blocker re-measured, still live (§4.2) |
| 4 | `?Update@AnimationManager@Graphics@@QEAAXAEBVTime@System@@_N@Z` | `0x1800fc240` | **DECLINED** — blocker re-measured and it is **WIDER than recorded** (§4.3) |
| 5 | `?Serialize@MeshInstance@Graphics@@QEAAXAEAVSerializer@IO@@@Z` | `0x1800ff680` | **DECLINED** — blocker re-measured, still live, one recorded fact CORRECTED (§4.4) |

One supporting change, in the call site of the port (§2.4): `runtime/src/video/context.cpp`'s
`MatrixInverse()` moved off the FROZEN `static Fn fn = ResolveExport(...)` form onto
`RT_EXPORT_ONCE`.

**So: 1 of 5 closed, 4 declined with the blocker MEASURED rather than inherited.** Two of the
four declines correct a fact in the record even though the verdict is unchanged.

---

## 2. `Math::Matrix::Inverse` — 0x1800bb310

### 2.1 What it is

334 instructions, all scalar SSE, **no calls**. Member with a struct return, so RCX = `this`
and RDX = the hidden return slot — confirmed at the call site as well as the callee
(`0x18001a165`: `LEA RDX,[RSP+0x20] / MOV RCX,RDI / CALL 0x1800bb310`).

A cofactor/adjugate inverse in four stages:

1. six 2x2 minors of the **bottom** two rows (`0x1800bb3e2`-`0x1800bb48f`),
2. the four top-row cofactors and the determinant (`0x1800bb47d`-`0x1800bb53a`),
3. `UCOMISS` against zero — **`det == 0.0f` exactly returns the IDENTITY matrix**, written
   straight to the return slot (`0x1800bb545`-`0x1800bb59c`),
4. otherwise ONE reciprocal `1.0f / det` (`DIVSS`, `0x1800bb5b3`) and sixteen multiplies by
   it, six more 2x2 minors of the **top** two rows, and a copy-out of four 16-byte rows from
   a stack local.

Two details that a "clean" reimplementation would get wrong:

* **The branch is `JP` then `JNZ`.** `JP` sends an *unordered* compare — a NaN determinant —
  to the DIVIDE path, not to the identity path. The port spells it `det == 0.0f`, which is
  false for NaN, i.e. the same branch.
* **One reciprocal, sixteen multiplies.** `x * (1/det)` and `x / det` differ in the last bit.

Constants lifted as raw bits, never retyped: `0x18016169c` = `0x3f800000` (1.0f) and
`0x1801617c0` = four lanes of `0x80000000` (the sign mask used by `XORPS`). Negation in the
port is an explicit sign-bit flip (`NegBits`) rather than unary minus, so the transcription is
checkable against the bytes without an argument about what the compiler does to `-x`.

### 2.2 Why this file is a transliteration

A determinant is a **near-cancelling sum**, and the adjugate has sixteen more of them. That is
exactly the shape of lane N's `Picking::Scene::Intersect` defect: a moved term — the
translation folded in FIRST instead of added LAST — algebraically identical, bit-different at
**one call in 23,654**, and therefore *never* exonerated by a clean re-run
(`reference_moved_term_hides_where_terms_cancel.md`).

So every sum in the port is parenthesised to the `ADDSS`/`SUBSS` order in the schedule, and
**each of the sixteen output cells cites the address of the instruction that fixes its
grouping**. The file carries a `DO NOT SIMPLIFY THE ARITHMETIC` banner for the next reader.

Multiplication and addition are commutative in IEEE-754, so operand order within one operation
is free; only the GROUPING is load-bearing. The operand order still mirrors the disassembly, to
keep the cross-check mechanical.

### 2.3 The oracle, and why it is not enough on its own

The `RT_DIFF_BYTES` site compares all 64 bytes of the return slot against the shipped body run
into a private scratch. `ret` is a hidden slot the caller chose and MAY alias `self`, so the
input is snapshotted before our body writes anything
(`reference_oracle_must_snapshot_aliased_inputs.md`).

**But the live call sites cannot falsify this port.** The only in-tree caller is
`Video::Context::SetCamera` — a handful of calls per frame on well-conditioned view matrices,
precisely the input class on which a near-cancellation defect is invisible. A clean live diff
here would be `reference_uncalled_port_is_not_verified.md` wearing a large `calls=` number.
Hence §3.

### 2.4 The call site was a FROZEN null cache

`context.cpp`'s `MatrixInverse()` was `static MatrixInverseFn fn = ResolveExport(...)` — lane
E3's FROZEN form, one of the ~22: the first caller fixes the answer for the process, so a
symbol registered or minted later can never be picked up and nothing names what was missing.
Moved to `RT_EXPORT_ONCE`, which caches a hit and retries a miss. It is behaviour-identical
while the DLL answers, which is why this is a cutover repair that could be **verified today**
rather than on the day no instrument survives.

---

## 3. The adversarial corpus probe

`BANISHED_RT_INVERSE_PROBE`. Runs once at `RtMathPortInit` time, with the DLL present; it can
never run at cutover, which is the whole reason it exists now.

Five families, 2,240,021 cases:

| # | Family | What it is for |
|---|---|---|
| 1 | FIXED | identity, zero, all-ones, a real perspective projection, a real view matrix, and matrices whose determinant is exactly 0 |
| 2 | RANDOM | deterministic LCG across seven magnitude decades (1e-6 … 1e9), so the cofactor products span the exponent range |
| 3 | SWEEP | a fine **CONTIGUOUS** sweep of one cell (61,001 steps x 16 cells) carrying the matrix through singularity, so every conditioning is visited *in order* |
| 4 | NEARSING | `row3 := alpha*row0 + beta*row1` (exactly singular), then an epsilon swept down through the binades to ULP scale |
| 5 | INTEGERS | small integer matrices — every product is exact, so the ONLY possible source of a difference is the grouping of the sums |

Family 3 exists because of lane N's actual diagnostic: the tell for "arithmetic specific to a
VALUE RANGE" rather than random rounding is **contiguous patches** of diverging inputs, and
patches are only visible if a parameter is swept rather than sampled. The probe reports the
longest run of adjacent diverging sweep steps for exactly that reason.

### Results — matched pair, same binary `md5 d5a6418084e343b5904cf839a286c70c`

| Run | Config | cases | divergences | negFired | longest adjacent sweep run |
|---|---|---:|---:|---:|---:|
| `gfxrPOS` | `AUTOSTART` + pause-menu gameplay, `RT_DIFF=1`, probe `=1` | 2,240,021 | **0** | 0 | **0** |
| `gfxrNEG` | probe `=neg` | 2,240,021 | 0 | **1,993,731 (89.01%)** | **28,011** |

Live diff site on the POS run: `Math::Matrix::Inverse calls=2,241,565 divergences=0` — the
corpus plus ~1,544 real `SetCamera` calls. Whole run: **505 sites, 19,299,551 calls, 1
divergence in 1 site**, and that one is the pre-existing non-diffable
`Object::Factory::Startup [probe, not diffable]` site, not this lane's. `standalone_crash.log`
zero bytes. `[RtResolve] 0 misses`, `[RtResolveEarly] 0`.

**EFFECTIVENESS control**, counted from each case's own inputs rather than assumed from the
family: `dividePath=2,239,862 identityPath=159 nearSingular=2,417 allFinite=2,240,021`. The
`det == 0` identity branch **did execute** (159 times) — a run where it had not would have been
reported as leaving that branch untested whatever the divergence column said.

**NEGATIVE control.** `=neg` runs a *copy* of the body — deliberately a copy, because a probe
that edits its subject cannot testify about its subject — with one term moved in every
three-term sum: `(x + z) + y` where the port has `(x + y) + z`. That is lane N's exact defect
class and is algebraically identical, so a control that fires is sensitive to **association
alone**. It fired on 89.01% of the corpus.

That number is the point of the exercise. Lane N's real effect rate was **1 in 23,654**, and its
first negative control had an expected occurrence count of **0.02** — it came back green and
said nothing, while looking exactly like a control that had passed
(`reference_probe_needs_an_effectiveness_control.md`). At 89% this corpus has roughly five
orders of magnitude of headroom over that rate. And the contiguity read-out reproduced lane N's
signature: **28,011 adjacent diverging sweep steps under NEG, 0 under POS.**

### Identity gating

Both runs were read out of their own log, gated on a line only this build emits
(`[RtMathPortInit] Wave 9 lane GFXR: 1 exports registered (Matrix::Inverse)` and the
`[InverseProbe] mode=…` line), not on a timestamp and not on "I rebuilt". The deployed exe's
md5 was checked identical before and after both runs, under `tools/game_dir_lock.sh` held as
`lane-GFXR`. Logs: `docs/runtime_port/wave9_lane_gfxr/`.

**Honest limit on the NEG run:** it used `BANISHED_AUTOQUIT=1`, which quits at the MAIN MENU.
That is sound *for this probe* — the probe runs at init and its 2.24M cases are identical on
every path — but it means the NEG run carries no gameplay `SetCamera` calls. The gameplay
evidence is the POS run only.

---

## 4. The four declines — blockers MEASURED

The standing rule is that a holdback's stated blocker is checked before it is believed
(`reference_holdback_blockers_are_stale_check_them_first.md`). All four were checked against
`fd30180`, not against lane GFX2's base. All four stand. **Two of the four descriptions in the
record were wrong about a fact, and are corrected here.**

Method: every callee address grepped across `code/`, `engine/`, `runtime/`, `standalone/`,
`dx11/` (every hit is a COMMENT — none is a port or a `RegisterRuntimeAddr`), **and** each
address looked up in the shipped DLL's export directory with `tools/pe_exports.py`. The export
lookup is the part that had not been done, and it is where both corrections come from.

### 4.1 `Renderer::Draw` (`0x180104bb0`) — DECLINED; one recorded fact CORRECTED

| Callee | Exported? | Ported? |
|---|---|---|
| `0x180105110` | **yes** — `?Reset@Renderer@Graphics@@QEAAXXZ` | no |
| `0x180105700` | **yes** — `?AddScene@Renderer@Graphics@@…` | no |
| `0x180104fd0` | **yes** — `?BeginDrawGroup@Renderer@Graphics@@…` | no |
| `0x1801017a0` | **yes** — `?AddCulledDrawItems@Scene@Graphics@@…` | no |
| `0x180062d60` | **NOT EXPORTED** | no |

**Correction to WAVE9_LANE_GFX2.md §6.1.** It reads as though all five are of a kind, with
`FUN_180062d60` merely "the worst". In fact **four of the five are ordinary named exports** and
only one is an unexported internal. That changes the shape of the row: `Renderer::Draw` is not
blocked by five anonymous bodies, it is blocked by **one** — `0x180062d60`, 0x2306 bytes of
per-pass sort with no name and no PDB — sitting behind four portable exports.

The verdict is unchanged and GFX2's reasoning still holds: porting `Draw` alone would convert a
named residue row into an unexported one, which is a *worse* row because no name-based
instrument can report it. But the correct next step is now specifiable: this is a
**`Graphics::Renderer` class lane** that ports the four exports and then has exactly one
unnamed body to solve, not a lane facing five.

### 4.2 `MaterialInstance::BuildParameters` (`0x1801033e0`) — DECLINED

`runtime/types/graphics/Material.h` still pins only `MaterialInstance` (0x60) and its four
entry structs. **There is no `struct Material` anywhere under `runtime/types/` or
`engine/types/`** (grepped `struct Material|class Material` across every `.h`: four
`Material*` entry structs, one `MaterialInstance`, one forward declaration, no `Material`).

Its callees, measured rather than assumed: `0x180060d30`, `0x180102900`, `0x180103020`,
`0x180102830`, `0x180102f40` — none ported — plus `0x1800c6e60`/`0x1800c6c00`
(`Allocate`/`Free`, already ours) and two IAT slots at `0x18014b388`/`0x18014b390`. The blocker
stands and it still belongs to a lane paired with `Graphics::Material`.

### 4.3 `AnimationManager::Update` (`0x1800fc240`) — DECLINED, and the blocker is WIDER than recorded

`AnimationManager::Update` itself is small — 90 instructions, a doubly-nested loop over a
0x30-byte entry array and an inner 0x48-byte state array, with exactly **one** call:
`0x1800fdb20`, `AnimationState::Update`.

The record says the blocker is that one unported callee. **It is that one plus six of its own:**

| `AnimationState::Update`'s callees | Exported? |
|---|---|
| `0x1800fd4c0` `?CopyInstancedToUnique@AnimationState@Graphics@@…` | yes |
| `0x1800fc870` `?UpdateRootOnly@AnimationState@Graphics@@…` | yes |
| `0x1800fcc20` `?UpdateNodes@AnimationState@Graphics@@…` | yes |
| `0x1800fa9a0` `?ApplyEvents@Animation@Graphics@@…` | yes |
| `0x18005aa30` | **NOT EXPORTED** |
| `0x18005b1e0` | **NOT EXPORTED** |

None of the seven appears anywhere in the tree. So this is not a two-function pickup, it is
**the `Graphics::AnimationState` class plus two unnamed internals**, and PLAN §4.1 rule 5
(class atomicity) says it should be taken as a class. Porting `AnimationManager::Update` alone
would trade one named residue row for seven, two of them unnamed — the same anti-pattern §4.1
identifies for `Draw`, and the `KeyMap::RegisterInput` effect WAVE9.md already measured
(a port that reaches into DLL internals GROWS the cutover surface).

This is the hottest skeleton path in the game and it is a genuine lane, not a leftover.

### 4.4 `MeshInstance::Serialize` (`0x1800ff680`) — DECLINED; one recorded fact CORRECTED

The body is 28 instructions:

```
this+0xd8  ->  CALL 0x1800fe250            ; serialize the embedded AnimationState
this+0xd0  ->  a virtual through Encoding vtable +0xa0
if (serializer->…+0x8 == 0)  CALL 0x18005cc60   ; Array::SetCount on this+0xc8
                             CALL 0x18005d4a0   ; serialize N array elements
```

**Correction to WAVE9_LANE_GFX2.md §6.4.** It records `0x1800fe250` as an unnamed internal
described as "serialize the mesh handle". It is neither unnamed nor that:
`0x1800fe250` is **`?Serialize@AnimationState@Graphics@@QEAAXAEAVSerializer@IO@@@Z`**, an
ordinary export. The other two, `0x18005cc60` and `0x18005d4a0`, are confirmed **not exported**
and confirmed absent from the tree — lane Q's `Collection::Array` work did not pick up the two
array-**serialisation** bodies, exactly as GFX2 said.

So the true blocker is: one exported callee that drags in the §4.3 `AnimationState` cluster,
plus two unexported array-serialisation internals. Verdict unchanged.

Worth carrying to whoever takes it: the `+0xa0` above is the **retail** `IO::Encoding` vtable
offset. The profile build's names win, its offsets must not
(`reference_encoding_vtable_offsets_differ_between_builds.md`). And a serializer that consumes
the wrong byte count does not fail where it is wrong — it desyncs and surfaces on an unrelated
record — so this belongs to a lane that runs a save/load round trip. On the WRITE path a
`Serialize` is a pure read and replays into two independent sinks, so
`runtime/src/io/save_trace.{h,cpp}`'s emission-sequence oracle is the right instrument
(`reference_write_path_serialize_is_replayable.md`).

---

## 5. Audit numbers, with matched denominators

The "before" is **the same tree with this lane's change reverse-applied**, not a different
worktree. That matters: the first attempt at a baseline used a neighbouring worktree at the
same commit and read `339`/`482` against this tree's `334`/`480` — a difference that is
another lane's untracked file, not this lane's delta. Quoting that pair would have invented a
change in both directions.

| Audit | Before | After | Delta |
|---|---|---|---|
| `port_bypass_audit.py` ADDRESS door | 334 sites, **0** bypasses (ADDR-REGISTERED 62, DETOURED 3, NO-EXPORT 163, SYMBOLIC-UNRESOLVED 13, UNPORTED 93) | 334 sites, **0** (identical buckets) | unchanged |
| `port_bypass_audit.py` NAME door | 477 sites, **0** bypasses, OK-ORACLE 439, OK-DATA 4 | 480 sites, **0**, OK-ORACLE 442 | **+3 sites, all OK-ORACLE** — the two `RtOriginal` oracle calls in the port and one in the probe. Denominator GREW |
| `rawdoor_audit.py` | 607 sites, **0 UNGUARDED-CALL**, 66 DEFERRED-CALL | 610 sites, **0**, 67 | +3 / +1, the same three oracle doors |
| `detour_target_audit.py` | clean | **clean** | unchanged — this lane installs no detour |
| `unsatisfied_export_scan.py` | 848 names through a door, **129** unsatisfied | 848 names, **128** | **-1** |
| … set-diff on the unsatisfied NAME SET | 126 distinct | 125 distinct | **CLOSED 1: `?Inverse@Matrix@Math@@QEBA?AV12@XZ`. NEW 0.** |
| `runtime_census.py` | 4632 total / 1003 called / **771** ported / 3 detour / 76.9% | 4632 / 1003 / **772** / 3 / **77.0%** | +1 ported, `called` unchanged |
| `addr_door_audit.py` | — | UNREGISTERED 16 | see limit 4 |

(The tool's own "unsatisfied" count and the distinct-name set it prints differ by three — 129
vs 126 — because the name extraction used for the set-diff normalises a few spellings. Both
move by exactly -1, which is the delta that matters.)

**The `called` denominator did not move.** That is the good case WAVE9.md describes: this
port's callees are already ours or are nothing at all (`Inverse` makes no calls), so it did not
create a lane-D row the way `KeyMap::RegisterInput` did. `RegisterRuntimeAddr(0x1800bb310, …)`
adds a registration, not a door, which is why the ADDRESS-door count is unchanged at 334.

The set-diff is the number that matters, and it is the accounting lane FIND asked for: quoting
"129 -> 128" alone would not have shown whether the one closed row came with new ones. It did
not.

---

## 6. HONEST LIMITS

1. **The corpus probe is not a proof, it is a very well-powered test.** 0 divergences in
   2,240,021 cases with a control that fires at 89% is strong evidence about the grouping of
   every sum the corpus exercises. It is not a proof of instruction-order equivalence, and
   there is no input class I can point to and say "this one is definitely covered to the last
   bit". What I *can* say is that the failure mode that took lane N a session — an effect at 1
   in 23,654 — would have been seen roughly a hundred times over.

2. **The overflow-to-infinity path was NOT reached.** Family 4 walks epsilon down toward the
   denormal range and family 2 reaches 1e9, but `allFinite` came back at 2,240,021 of
   2,240,021 — every reference result was finite. So a matrix whose cofactors overflow float is
   **untested**. I did not add a family for it because I could not argue it is reachable from
   `SetCamera`; that is an argument, not a measurement.

3. **`RegisterRuntimeAddr(0x1800bb310, …)` has no consumer today.** There is no by-address door
   on `Inverse` in the tree. The registration is cheap insurance against one being written; it
   is not evidence of anything and it must not be read as closing an address-door row.

4. **`addr_door_audit.py` has an "after" number only** (UNREGISTERED 16). I did not take a
   matched "before" for it, because the reverse-apply/re-apply cycle costs a full audit pass
   and the address door was already shown unchanged at 334 sites by `port_bypass_audit`. If a
   set-diff on that tool matters to the integrator, it needs re-running as a pair.

5. **The `identityPath` count is 159 of 2.24M.** The det==0 branch is exercised, but thinly,
   and every one of those 159 comes from families 1 and 4 (constructed singular matrices)
   rather than from anything resembling a camera. The branch is verified; its *reachability
   from real inputs* is not something this lane measured.

6. **`nearSingular` is 2,417 of 2.24M (0.11%).** That is the count of cases where the four
   determinant terms cancel to under a thousandth of the largest — the regime the whole design
   is aimed at. It is a small fraction of the corpus, and I would rather record that plainly
   than let "2.24 million cases" imply 2.24 million *hard* cases. The negative control's 89%
   hit rate is what says the corpus as a whole is discriminating; the 2,417 is what says the
   hardest regime specifically was entered a few thousand times.

7. **Neither the four declines nor this port were run with the DLL absent.** That is the
   integrator's cutover boot, not this lane's.

8. **This lane could not commit.** The harness re-pointed this session's isolated worktree four
   times mid-session (`lane-addr` -> `lane-mod9` -> `w9-addr-089e6eb4` -> `lane-uid-089e6eb4`),
   and git operations against the worktree holding this work are refused. Two consequences
   worth the integrator's attention, both recorded rather than smoothed over:
   * A first build "failure" at `fd30180` (an ambiguous `Retrace` in
     `runtime/src/object/external_set.cpp`) was **not a broken tip** — it was a neighbouring
     lane's *uncommitted* file in a worktree this session had briefly been handed. `fd30180`
     builds clean, and every measurement above was taken on a build that does.
   * This lane's three files were briefly present as uncommitted changes in lane UIF's
     worktree. They have been removed and that worktree verified back to its own contents. But
     the same churn could have gone the other way for another lane, and **a lane sweeping up a
     neighbour's files into its own commit would not announce itself** — it is worth one check
     across the wave's commits.

---

## 7. Files

| Path | What |
|---|---|
| `runtime/src/math/wave9_lane_gfxr.cpp` | the port, the probe, and `RtMathWave9LaneGfxrRegister()` |
| `runtime/src/math/rt_math_port.cpp` | +6 lines: the `extern` and the registration call |
| `runtime/src/video/context.cpp` | `MatrixInverse()` off the frozen static onto `RT_EXPORT_ONCE` |
| `docs/runtime_port/wave9_lane_gfxr/` | `gfxrPOS_probe.txt`, `gfxrNEG_probe.txt`, `gfxrPOS_error.log`, `gfxrNEG_error.log` |
