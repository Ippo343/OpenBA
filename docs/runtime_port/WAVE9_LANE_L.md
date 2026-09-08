# Wave 9 — Lane L: the CRT libm five

**Closed 2026-09-07.** All five bit-exact against the shipped DLL, verified by a
1.49-billion-comparison sweep with an effectiveness control and a negative
control, and by the in-game diff oracle on three routes with a working negative
control on the same routes.

| retail | function | port | verdict |
|---|---|---|---|
| `0x180128110` | `sqrtf` | `MsvcLibm::Sqrtf` | **bit-exact**, 66,846,758 comparisons |
| `0x180127d90` | `sinf` | `MsvcLibm::Sinf` | **bit-exact**, 352,059,430 comparisons |
| `0x180126d10` | `cosf` | `MsvcLibm::Cosf` | **bit-exact**, 352,059,430 comparisons |
| `0x180126680` | `atan2f` | `MsvcLibm::Atan2f` | **bit-exact**, 360,695,204 comparisons |
| `0x180127560` | `powf` | `MsvcLibm::Powf` | **bit-exact**, 361,426,788 comparisons |
| `0x18012a1a4` | `__rem_pio2` | `MsvcLibm::RemPio2` (internal) | shared Payne–Hanek helper, ported with them |

Deliverables: `runtime/src/crt/msvc_libm.{h,cpp}`,
`runtime/src/crt/msvc_libm_tables.h` (generated), `tools/extract_crt_tables.py`,
`tools/libm_probe/{sweep,find_tiny,bench}.cpp`, `make libm-sweep`.
Raw evidence: [wave9_laneL_sweep.txt](wave9_laneL_sweep.txt),
[wave9_laneL_ingame.txt](wave9_laneL_ingame.txt).

## 1. The problem

Two Wave 8 sites were still reporting divergences on the gameplay route:

```
Math::Vector3::FromYawPitch                calls=3838   divergences=16
Math::Transform::FromYawPitchOffset(...)   calls=45473  divergences=88
```

Adding `BANISHED_RT_LIBM=dll` silenced both, which proved the cause was libm
rounding rather than a port defect — but that switch works by calling the DLL's
CRT at five fixed addresses, so it dies at cutover and takes the fix with it.
After cutover MinGW/UCRT's libm becomes what ships **permanently**, and every
float the game derives from a trig call is a last bit away from retail's for the
life of the project.

The rate is the point: 16 of 3,838 is 0.4%. **A low divergence rate is exactly
what a real last-bit difference looks like**, and a small sample misses it
entirely. It is not evidence of harmlessness.

## 2. What was measured before writing anything

`tools/libm_probe/` first measured MinGW's libm against the DLL's, to size the
job and rule out a cheap fix.

| | rate vs the DLL | shape |
|---|---|---|
| `sqrtf` | **0 in 16.0M** | already bit-exact — the retail body is one `sqrtss` |
| `sinf`/`cosf` | 0 below \|x\|=0.5; ~0.1–0.2% and always exactly 1 ULP up to 2^41; then rising to 100% past 2^45 | the tail is the two argument reductions disagreeing, not the polynomials |
| `atan2f` | 620 in 3.15M (0.02%), max 1 ULP | |
| `powf` | 17 in 2.37M, **all special cases** | `pow(-1,-inf)`, `pow(NaN,±0)` |

**The cheap fix was tested and does not exist.** `(float)sin((double)x)` was
compared against the DLL over 10.0M arguments and produced *character for
character the same* diff count as `sinf` (634,912 for both) — MinGW's `sinf`
already *is* `(float)sin((double)x)`. The remaining difference is a
correctly-rounded double `sin` versus MSVC's minimax polynomial, and no wrapper
closes it. The bodies had to be ported.

## 3. Method

Read the disassembly, not Ghidra's C — `objdump -d -M intel` against
`Runtime-x64.dll` at each address, per
`reference_ghidra_c_output_vs_disassembly.md`.

**The coefficient tables are the whole game and they are not retyped.**
`tools/extract_crt_tables.py` parses the PE section table and lifts every
constant as a raw `uint64_t`/`uint32_t` bit pattern into
`runtime/src/crt/msvc_libm_tables.h`. That is not fussiness — the sin and cos
series use *different* coefficients for the same mathematical term:

```
sin 1/120 : 0x3f81111111111111      cos 1/120 : 0x3f81111111110bb3
sin 1/5040: 0xbf2a01a01a01a01a      cos 1/5040: 0xbf2a01a019e83e5c
```

`1.0/120.0` typed into the source would silently produce the first in both
places, and `cosf` would have been 1 ULP wrong on a slice of its domain — the
identical failure this lane exists to remove. Tables lifted: 18 sin constants,
17 cos constants, the 14-limb 36-bit expansion of 2/π, 12 atan2 constants plus
the 241-entry `atan(k/256)` table, and for `powf` two 129-entry log tables, the
32-entry `2^(k/32)` table and the 26 integer constants its special-case dispatch
reads.

Operation order is transcribed instruction for instruction, including
temporaries that look redundant, because floating point is not associative.
`msvc_libm.cpp` carries an `#error` on `__FMA__`: the project builds for
baseline x86-64 where no FMA instruction exists, and a contracted multiply-add
would round once where retail rounds twice.

### What was deliberately not ported

Every retail body also calls the CRT's error reporting — `_invalid_parameter`,
`_matherr`, `errno`, `feraiseexcept` — on domain and range errors. Those calls
**do not change the returned value** (verified: `0x18012a940` saves its `xmm2`
argument into `xmm6` and returns it unchanged on every path), and nothing in the
game reads `errno` or the FP exception flags. Every RETURN VALUE is ported; each
omission is marked `// retail: reports <what>` at the site.

### Two retail behaviours that are not C99, and are kept

* `pow(-1, ±Inf)` returns **-1** for `-Inf` (C99 says `1`). The dispatch routes
  `|exp|` exponent-field `> 0x4f000000` to the huge-exponent path, which tests
  `base == 0xbf800000` and lands in the shared `-1` tail, where
  `cmove edx, 0xbf800000` fires on `exp == 0xff800000`.
* `pow(NaN, ±0)` returns **the NaN** (C99 says `1`). The exponent-is-zero branch
  checks the base for NaN *before* returning 1.

The DLL is what ships. A "corrected" port would be a divergence.

## 4. Verification

### 4.1 Sweep — `make libm-sweep`

`tools/libm_probe/sweep.cpp` calls ours and the DLL's body and compares **raw
32-bit patterns**: NaN payloads and signed zeros included, no `float ==`
anywhere. Six phases: dense over all 254 finite exponents both signs; the whole
subnormal band; a 2-D atan2/pow grid in all four sign quadrants; pow with
integral and quarter-integral exponents (the odd/even-integer sign rule and the
`2^(j/32)` index); 40 special values exhaustively crossed (±0, ±1, ±π, ±π/2,
±Inf, quiet and signalling NaN, ±FLT_MIN, ±denormal-min, denormal-max,
±FLT_MAX, the exact `powf` thresholds, and arguments large enough to force
Payne–Hanek); and **sin/cos exhaustive over every representable float in
[2^-4, 2^12]** — the band the game actually feeds them.

```
sqrtf    n=66846758     diff=0    BIT-EXACT
sinf     n=352059430    diff=0    BIT-EXACT
cosf     n=352059430    diff=0    BIT-EXACT
atan2f   n=360695204    diff=0    BIT-EXACT
powf     n=361426788    diff=0    BIT-EXACT
```

**1,493,087,610 comparisons, 0 differing bits.**

### 4.2 Effectiveness control

Per `reference_probe_needs_an_effectiveness_control.md`, the counters live
**inside the ported bodies** (`MSVC_LIBM_COVERAGE`, compiled out of the game
build) rather than being re-derived by the probe — a classifier that disagrees
with the body it is describing measures nothing. Full table in
[wave9_laneL_sweep.txt](wave9_laneL_sweep.txt); the branches that matter:

```
sinCodyWaite 230,373,623   sinPayneHanek 28,597,992   sinExtendedSplit 6,698,212
remPio2 57,195,984         remPio2RoundUp 28,597,942  remPio2ExtraLimb 28,860,172
atanTablePath 11,124,924   atanPolyPath 25,142,748    atanSubnormal 7,741,491
powCore 23,293,102         powNegBaseOddInt 1,089,388 powNegBaseEvenInt 2,134,436
powOverflow 14,460,110     powUnderflow 14,228,921    powBaseMinusOne 396
```

Every branch of all five bodies is entered **except one**, and that one has a
proof rather than an excuse.

#### The single uncovered branch, and why it is uncoverable

`sinReducedTiny` / `cosReducedTiny` — the path taken when Cody–Waite reduction
leaves `|y|` below about 2^-33 — shows **0**. `tools/libm_probe/find_tiny.cpp`
scans **every float** in the band where that branch can be reached
(π/4 < |x| < 500000; above that `__rem_pio2` takes over and skips it),
replicating the port's reduction exactly, extended split included:

```
floats in the Cody-Waite band : 162,206,757
extended-split refinements     : 43,457,596
reducedSmall (0x3df..0x3f1)    : 25,289
reducedTiny  (<= 0x3de)        : 0
smallest reduced exponent      : 0x3e3  at x=0x437ce5f1 (252.898209)
-> reducedTiny is UNREACHABLE from a float argument.
```

The branch is dead code in the `f`-suffixed entry point: it exists because the
same code shape serves the double-precision routine, where a 53-bit argument can
land that close to a multiple of π/2. It is ported anyway, faithfully, and
labelled.

### 4.3 Negative control

The sweep re-runs itself with MinGW's libm as "ours" and **requires** a nonzero
diff count; it exits 3 and says the port result proves nothing otherwise.

```
sinf     diff=171593 (32.85%)    cosf   diff=171631 (32.86%)
atan2f   diff=12     (0.007%)    powf   diff=364    (0.041%)
sqrtf    diff=0                  BIT-EXACT
```

4 of 5 fire, so the comparison can see a defect. **`sqrtf` reporting 0 for the
control too is a true result, not a harness failure**: the retail body is a bare
`sqrtss` and IEEE square root is correctly rounded, so every conforming
implementation agrees. That is also why `sqrtf` never appeared in any in-game
divergence.

### 4.4 In game

Gameplay route (`BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900
BANISHED_PAUSEMENU_QUIT=120`), reading the raw log, never a `tail -N`:

| run | whole-run oracle | `Vector3::FromYawPitch` | `Transform::FromYawPitchOffset` |
|---|---|---|---|
| **port (default)** | 267 sites, **29,586,428 calls, 0 divergences** | calls=3835 **div=0** | calls=43429 **div=0** |
| `BANISHED_RT_LIBM=std` (control) | 267 sites, 29,799,839 calls, **125 divergences in 2 sites** | calls=3839 **div=27** | calls=46875 **div=98** |
| `BANISHED_RT_LIBM=dll` (oracle still works) | 271 sites, 28,719,661 calls, 0 divergences | calls=3837 div=0 | calls=42949 div=0 |
| save-load route (`BANISHED_AUTOLOAD=Save9`) | 292 sites, **51,513,160 calls, 0 divergences** | calls=2892 **div=0** | calls=322172 **div=0** |

`calls=` is read, not just `divergences=`
(`reference_uncalled_port_is_not_verified.md`): both sites stay in the thousands
— 322,172 on the save route — so this is a live clean, not a dead one. The
`=std` row is the in-game negative control: it puts the defect back and the
oracle finds it again, which attributes the fix to this lane rather than to
anything else that moved today.

## 5. Cutover effect

`rt_libm.cpp` now defaults to the ported bodies, installed at **static-init
time** so a caller running before `RtLibm::Init()` already gets retail's
arithmetic. The five `ResolveRuntimeRaw` addresses stay in the file but are only
reached under `BANISHED_RT_LIBM=dll`; with the DLL absent that path resolves
null, warns, and falls back to the port rather than executing address 0.

So **the five `ResolveRuntimeRaw` sites Lane 0 flagged are off the default path**
(Lane 0 had noted they appeared in no harness cell — absence of evidence; they
are now absent by construction). Two A/B switches remain:

* `BANISHED_RT_LIBM=dll` — retail's CRT, for as long as the DLL exists.
* `BANISHED_RT_LIBM=std` — MinGW's libm, to reproduce the pre-port measurements.

## 6. Performance

These sit on hot per-frame paths (322k `FromYawPitchOffset` calls in one save
route), so "bit-exact but slower" was a real risk. `tools/libm_probe/bench.cpp`,
ns/call, best of 3 × 2M:

| | port | MinGW (what it replaces) | DLL |
|---|---|---|---|
| `sqrtf` | **1.04** | 4.89 | 13.19 |
| `sinf` | **3.01** | 23.53 | 4.15 |
| `cosf` | **3.01** | 24.07 | 4.22 |
| `atan2f` | **3.15** | 34.52 | 4.06 |
| `powf` | **6.37** | 37.27 | 6.95 |

The port is 6–11× faster than the libm it replaces and slightly faster than the
DLL's own copy (no cross-module call, no error-reporting prologue). This lane
moves the perf gate the right way; no separate stress run was needed to clear it.

## 7. Audits

Run after the change, all clean:

```
export_name_audit.py     652 ResolveExport names; 0 NOT in Runtime-x64.dll
port_bypass_audit.py     ADDRESS door 350 sites, 0 reach an already-ported name
                         NAME door    73 sites, 0 are a ported fn on a live path
resolve_fallback_audit.py 163 guarded fallbacks; 0 do NOT point at their own name
runtime_census.py        832 called / 545 ported / 65.5%   (exit 0)
```

Both `standalone-debug` and the log-stripped `standalone` were built, with
`standalone/build/` deleted in between so the release link could not silently
reuse debug objects.

## 8. Reproducing

```bash
make libm-sweep                          # full sweep (minutes)
make libm-sweep LIBM_SWEEP_ARGS=--quick  # skip the exhaustive band (seconds)
make libm-tables                         # re-extract the tables from the DLL
```

`make libm-sweep` exits non-zero on any divergence **and** on a negative control
that finds nothing. It needs `Runtime-x64.dll` present, so it is a pre-cutover
gate: run it before the DLL is removed, because afterwards there is nothing left
to compare against.
