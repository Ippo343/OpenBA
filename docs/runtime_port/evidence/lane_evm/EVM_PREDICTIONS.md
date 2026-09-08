# Lane EVM — predictions recorded BEFORE any run

Branch `wave9/lane-evm`, base `e26b422`. Build identity lines:
`[LaneEVM] rev=EVM-r1 interfaces registered=2` and
`[LaneEVM] mode/refresh sort addresses registered=5`.

Baseline for every delta below is **the same binary** with the lane switch
`BANISHED_RT_EVM=0`. Tool for the residue set-diff: the `BANISHED_RT_STRICT`
census in `standalone/resolve_standalone.cpp`, **unchanged by this lane from
base `e26b422`**. Stated as DELTAS, never absolutes — the tree moves.

## P1 — cutover-residue set-diff (baseline minus mine)

REMOVED, predicted exactly one:
    ?EnumerateVideoModes@Device@Video@@QEAA_NXZ

ADDED, predicted exactly one:
    ?_disableHandler@Debug@System@@0_NA
(my `EnumerateVideoModes` reads it through `RT_DATA_SLOT`; a grep of
`standalone/` and `runtime/` found ZERO other askers, so it is a brand-new row)

**NET COUNT CHANGE PREDICTED: ZERO.** One out, one in. This is the case that
makes a count worthless and a set-diff necessary — a lane reporting "residue
unchanged" here would be reporting a true number that hides two moves.

NOT predicted to move, and each for a stated reason:
  * `?InitializeInterfaces@` and `?GetDLLForInterface@` — my bodies call each
    other with DIRECT C++ calls, not through the seam, so neither ever reaches
    the resolver in either configuration.
  * the five sort addresses — the probe reaches the shipped bodies through
    `ResolveRuntimeRaw`, which is recorded in the separate `[RtRawResidue]`
    list, not in `[RtResidue]`.

## P2 — diff counters under `BANISHED_RT_DIFF=1`

    Video::Device::GetDLLForInterface     calls=8  worked=5  divergences=0
    Video::Device::InitializeInterfaces   calls=1  worked=1  divergences=0
    Video::Device::EnumerateVideoModes    calls=1  worked=1  divergences=0

`calls=8` for GetDLLForInterface is **2 live + 6 from the probe**, and the live
number is 2 rather than 3 on purpose: slot 1 is pre-filled by
`VideoInterfaceFillDx11Slot` and `continue`s before the call, so index 1 is
unreachable from the live site. `worked=5` is 2 live + 3 probe cases that named
a plugin (the probe's other three inputs are out of range and produce only the
suffixes).

A `calls=2 worked=2` here would mean the probe never ran; a `calls=0` would mean
the registration did not take.

## P3 — probe output, positive run

    [EvmProbe] GetDLLForInterface cases=6 named=3 differing=0
    [EvmProbe] sorts rate: cases=160 differing=0 discriminating=<large>
               mode: cases=160 differing=0  |  deep: cases=3 differing=0
    [EvmProbe]   rate branches: every counter NON-ZERO, no "STRUCTURAL PATH" warning
    [EvmProbe]   mode branches: every counter NON-ZERO including deep>0
    [EVM] InitializeInterfaces skipped=1 ... dx11Prefilled=1
    [EVM] EnumerateVideoModes created>=1 enumerated>=1 interfaces>=1

`skipped=1` is the load-bearing one: it is precisely the assertion that the DX11
pre-fill landed BEFORE the loop looked at slot 1. `skipped=0` would mean the
port went and loaded the real VideoDX11-x64.dll.

## P4 — negative controls, `BANISHED_EVM_NEGCTL=1`

    [EvmProbe] GetDLLForInterface NEGATIVE-CONTROL cases=6 named=3 differing=6
    [EvmProbe] sorts NEGATIVE-CONTROL rate: differing == discriminating
               mode: differing == (its own discriminating count)

**`differing == discriminating` is the sharp form and it is falsifiable both
ways.** The control substitutes an ASCENDING insertion sort, and ascending
differs from descending on exactly those cases holding two or more distinct
keys — no more (a single-key or single-element case cannot differ) and no fewer.
If `differing < discriminating` the control is not firing everywhere it must; if
`differing > discriminating` the counting of distinct keys is wrong.

The GetDLLForInterface DIFF SITE is predicted to stay at `divergences=0` even
under the negative control, because the control perturbs the probe's copy AFTER
our body returned. That is intentional and is itself a check that the two
instruments are independent.

## P5 — what I expect the DLL-less cutover boot to do (integrator's run, not mine)

`Engine::Startup` should pass `g_startupFailAt` 6/11/12/13 as before and now
also get through the `0x1800dbccf` block that calls `EnumerateVideoModes`,
stopping at whatever the next unported export is. Two specific things to watch,
because they are the parts of this lane that cutover exercises differently:
  * `VideoInterfacePrefilled()` must be TRUE. If it is false, nothing filled the
    DX11 slot and `CreateVideoInterface` will find slot 1 incomplete.
  * a video driver that FAULTS during enumeration now takes down the process
    instead of being skipped — the `__except` is not reproduced (see the lane's
    honest limits).
