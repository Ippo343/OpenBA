# Wave 9 lane EVM — `EnumerateVideoModes` and the chain under it

Branch `wave9/lane-evm`, base `e26b422`.
Worktree `C:\code\GIT\untitled_rev_eng_project\.claude\worktrees\lane-evm`.

> **STATUS: CODE COMPLETE AND BUILDING. NOT VERIFIED — NO RUN WAS EVER TAKEN.**
> Every body below is written, transcribed against the disassembly, compiles
> clean and is registered behind the lane switch `BANISHED_RT_EVM`. **Not one
> number has been measured.** The wave was parked before the lock window.
> Nothing here may be quoted as verified; §6 says exactly what to run.

## 1. The row, and why it is EIGHT bodies

Attempt 22's wall is `?EnumerateVideoModes@Device@Video@@QEAA_NXZ`. Its **first
instruction after the prologue is `CALL 0x1800f79f0`**, which is
`?InitializeInterfaces@Device@Video@@QEAAXXZ` — so the wall cannot be ported
alone. Below that sits a chain that is inseparable at cutover: four of these are
reached BY ADDRESS, and any one left unported is a null the moment the DLL is
gone.

| # | Body | Retail | Ord | Exported |
|---|---|---|---|---|
| 1 | `?EnumerateVideoModes@Device@Video@@QEAA_NXZ` | 0x1800f8610 | 2319 | yes |
| 2 | `?InitializeInterfaces@Device@Video@@QEAAXXZ` | 0x1800f79f0 | 3073 | yes |
| 3 | `?GetDLLForInterface@Device@Video@@QEBA?AVString@System@@H@Z` | 0x1800f7b20 | 2542 | yes |
| 4 | introsort&lt;DisplayMode&gt;, stride 0x18 | 0x180057910 | — | **no** |
| 5 | its small-range pass | 0x180058120 | — | **no** |
| 6 | its element swap | 0x1800584c0 | — | **no** |
| 7 | introsort&lt;RefreshRate&gt;, stride 0x08 | 0x180057630 | — | **no** |
| 8 | its small-range pass | 0x180057fa0 | — | **no** |

**It is eight, not the seven I reported mid-lane.** The eighth is the
out-of-lined DisplayMode swap at 0x1800584c0, found after that message: the
introsort CALLs it from the three median-of-3 sites and INLINES the same
operation in the partition and in the small-range pass.

Files: `runtime/src/video/video_interfaces_win.cpp` (2, 3),
`runtime/src/video/video_mode_sort.cpp` (4–8),
`runtime/src/video/enumerate_video_modes.cpp` (1),
`runtime/src/video/video_mode_sort_probe.cpp` (the oracle).

## 2. THE ORACLE THE BRIEF ASKED FOR CANNOT BE BUILT, AND WHY

The brief asked for a snapshot-before/compare-after over the 0x108 `Video::Device`.
**That instrument would be 100% mismatch on every byte either side writes and
0% coverage of everything else.** Exactly two regions change across the call:

* **+0x40/+0x48/+0x4C** — the `Array<GraphicsInterface>` header, filled
  indirectly by the plugin's virtual. `_data` is a heap pointer: different
  between any two runs by construction. (Lane GFXD's false-mismatch class: five
  "mismatches", all `_data` pointers at one offset.)
* **+0xb0..+0xf7** — the three `InterfaceSlot`s, written by
  `InitializeInterfaces`: an `HMODULE` and two `GetProcAddress` results.
  Different every run **and different in kind** between ours and theirs, because
  slot 1 deliberately holds our sentinel module and our own
  `Dx11_CreateDeviceData`.

So the verification was relocated onto the only real computation in the
function: the three-level **sort pass**. A sort is a pure permutation of a
buffer the caller owns — deterministic, machine-independent, and comparable
against the shipped body on synthetic data. That is
`video_mode_sort_probe.cpp`, and it compares the **permutation**, never the
sortedness: "it came out sorted" is true of a bubble sort, a `std::sort` and a
broken transcription alike.

## 3. FINDING — retail's refresh-rate comparator is a defect, faithfully reproduced

**This is a retail bug. It is transcribed as a bug, deliberately.**

Every comparison in 0x180057630 and 0x180057fa0 — five sites, all checked — has
this shape:

```
    MOVD     XMM1,[X+4]      ; X.denominator
    MOVD     XMM2,[X]        ; X.numerator
    MOVD     XMM0,[Y]        ; Y.numerator      <-- Y.denominator is NEVER loaded
    CVTDQ2PS XMM1,XMM1
    CVTDQ2PS XMM2,XMM2
    CVTDQ2PS XMM0,XMM0
    DIVSS    XMM2,XMM1       ; X.num / X.den
    DIVSS    XMM0,XMM1       ; Y.num / X.den    <-- X's denominator AGAIN
    COMISS   XMM2,XMM0
```

The five sites: the three median-of-3 compares at 0x1800576a6 / 0x1800576d8 /
0x18005770a, the partition scans at 0x180057756 and 0x180057791, and the
small-range pass at 0x18005800d. In every one the divisor is the FIRST
operand's denominator, applied to BOTH numerators. Whoever wrote it meant
`Y.num / Y.den`.

Because both sides are divided by the same value, the order reduces to
`X.numerator > Y.numerator` and the denominators are ignored entirely.

**The two `DIVSS` are NOT dead weight, and this is the strong form of the
argument for transcribing rather than folding:**

1. **`den == 0` makes them load-bearing.** `A.num/0` and `B.num/0` are ±∞, or
   **NaN** when the numerator is also zero. `COMISS` with a NaN sets the
   unordered flags, so `JA`/`CMOVA` are NOT taken — a branch an integer
   numerator compare WOULD take. An unordered comparator inside an introsort
   can drive the partition somewhere no total order would.
2. Above 2²⁴ the `CVTDQ2PS` rounding can tie numerators that an integer compare
   would separate.

**Consequence, which is the half that matters:** the mode list feeds the options
dialog, so anyone who "fixes" this changes user-visible video-mode ordering.

**The probe must cover** `denominator == 0`, `0/0`, and equal numerators with
different denominators — retail treats those as EQUAL and a correct comparator
would not. **This is a gap in the probe as committed** (its corpus draws only
plausible refresh rates); see §6.

## 4. FINDING — the `__except` landing pad at 0x1800f8683

Ghidra's listing skips 20 bytes inside `EnumerateVideoModes`. Decoded from the
mapped image:

```
    1800f8681  EB 14                        JMP  0x1800f8697
    1800f8683  4C 8B A4 24 80 00 00 00      MOV  R12,[RSP+0x80]    ; self
    1800f868b  8B BC 24 98 00 00 00         MOV  EDI,[RSP+0x98]    ; the loop index
    1800f8692  48 8B 5C 24 28               MOV  RBX,[RSP+0x28]    ; the DeviceData
    1800f8697  C6 05 6A 40 10 00 00         MOV  byte [0x1801fc708],0
```

It is an **`__except` landing pad**: it restores `self`, the loop index and the
DeviceData pointer from their stack homes and falls into the
`_disableHandler = 0` path, then the null-guarded deleting destructor and the
next slot. **Retail positively expects a video driver to fault during mode
enumeration and intends to survive it** — which is also what raising
`?_disableHandler@Debug@System@@0_NA` (ord 4781, 0x1801fc708) across the call is
for.

**This port raises and lowers the flag faithfully but does NOT swallow the
fault, and that is correct rather than deferred.** MinGW has no usable
`__try`/`__except` on x64, so reproducing it means teaching our VEH to ignore
access violations while the flag is set — and a crash handler that starts
swallowing AVs during a port wave would hide exactly the faults these lanes
exist to find.

The divergence is on our side and pre-existing:
`standalone/main.cpp:1607` installs `AddVectoredExceptionHandler(1, FirstChanceFailFast)`,
and vectored handlers run **before** SEH, so a faulting driver enumeration never
reaches retail's SEH region under us. Recorded in `WAVE9.md` at `76636d0`.

## 5. Design decisions worth not re-deriving

* **`InitializeInterfaces` calls `VideoInterfaceFillDx11Slot(self)` first.**
  `standalone/video_interface.cpp:158` has asked for exactly this in writing
  since lane ENGIO: `FillDx11Slot` is reachable ONLY from the detour's wrapper,
  so at cutover — where there is no body to detour — nothing fills the DX11 slot
  unless the ported body does it. This retires that hole rather than relocating
  it. The detour stays installed and becomes redundant, not wrong: it resolves
  its target RAW, so it still patches the DLL's copy, which nothing on our side
  reaches once these registrations are live.
* **`GetDLLForInterface` returns `String` BY VALUE** — hidden retslot in RDX,
  `index` in R8. **`this` is never read**: the prologue homes RDX, not RCX. That
  makes it a pure function of an int and the one body here with a perfect
  oracle. (Same defect class lane RETSLOT audited 234 rows for.) Its default
  case is not an error path: any index outside 0..2 skips the switch AND the
  temporary's release and returns `"-x64.dll"`.
* **The DisplayMode swap is a DEEP COPY**, three `Array<RefreshRate>::operator=`
  plus one `~Array` per swap. A memcpy swap would give the same permutation and
  the same field values while leaving the original `_data` pointers in place,
  and **no contents-only comparison could tell them apart**. The inlined copies
  are lane VID2's already-ported 0x18000fbf0 open-coded, so this file calls that
  port rather than re-deriving 150 instructions of inlined allocator code —
  a deliberate substitution of a verified-equal body, not a simplification.
* **The introsort skeleton was CHECKED, not assumed.** It is the same engine
  introsort lane COLL transcribed at 0x1800264a0. **I first wrote the second
  equal-run walk as unconditional; retail reaches it ONLY when the first ran out
  of room (`j <= pivot`), never when it stopped on a non-equivalent element.**
  That was a real defect in my transcription, caught by checking COLL's verified
  form rather than my own reading — which is the argument for looking for the
  existing port before writing a new one.

## 6. WHAT THE NEXT PERSON MUST DO — in order

1. **RE-RUN `tools/rt_mint_gen.py`.** This lane's `EnumerateVideoModes` is the
   first thing in the tree to reference `?_disableHandler@Debug@System@@0_NA`
   (exported DATA, ord 4781, 0x1801fc708) and it is **NOT in the mint tables**,
   because the generator mints what the tree references. Without regenerating,
   the port's write goes nowhere at cutover. Lane VIDL hit the identical shape
   with seven `_typeInfo` globals. **I did not do this deliberately**: it
   rewrites `standalone/rt_data_mint_tables.inc`, a contended file lane TYPE
   regenerated this wave, and I could not verify the result without a run.
2. **Widen the sort probe's corpus** to include `denominator == 0`, `0/0`, and
   equal numerators with differing denominators — see §3. As committed the
   corpus cannot distinguish the transcribed comparator from a "corrected" one
   in the cases where they actually differ.
3. **Take one lock window and run the five cells** in
   `WAVE9_LANE_EVM_PREDICTIONS.md`. All predictions are registered as DELTAS and
   were written before any measurement.
4. Then, and only then, may any number in this lane be reported.

## 7. Honest limits

1. **NOTHING IS VERIFIED.** No run was taken. The bodies compile and are
   registered; that is all that is known.
2. The probe's corpus does not yet cover the cases where the comparator bug is
   observable (§3, §6.2). Its green result — if it ever produces one — would
   therefore be weaker than it looks.
3. `EnumerateVideoModes` has no oracle and cannot have one (§2). Its
   `RT_DIFF_SHADOW` is a description, not a comparison.
4. `InitializeInterfaces` has no oracle: a replay would `LoadLibrary` the same
   DLLs again, and slot 1 is ours by design so it could not match. What is
   asserted is the postcondition (`skipped >= 1`, DX11 slot filled).
5. The `__except` swallow is not reproduced (§4). On a driver that faults during
   enumeration, retail skips that renderer and continues; we die with a named
   crash. Error path, our VEH, pre-existing.
6. `?_disableHandler@` is written but unminted (§6.1) — at cutover that write
   currently has no storage behind it.
7. Three `ResolveAddr` oracle doors from the PLAT lane
   (0x18014bc20, 0x18014be30, 0x180161780) still need converting to
   `ResolveRuntimeRaw`; `ResolveAddr` consults the port registry first, so if
   anyone ports that data the comparison silently becomes true-by-construction.
   Specified and agreed, deferred to next session.
