# CUTOVER BOOT GATE — attempt 24, 2026-09-09

Run by the integrator immediately after Wave 15 closed, against
[WAVE9_CUTOVER_BOOT_RUNBOOK.md](WAVE9_CUTOVER_BOOT_RUNBOOK.md). Exe
`reimpl-x64.exe` 31182702 bytes, built 21:40 from `wave10/base` @ `7fd8a43`
(Wave 15 phase 1 closed). Game lock held as `wave16-bootgate`; DLL renamed in
place and **restored** — verified present afterwards.

Evidence: `wave16_bootgate/attempt24_stderr.txt`, `attempt24_crash.txt`.

## Result: the wall has NOT moved since Wave 10 opened

Attempt 23 (2026-09-08, Wave 10 opening) died at
`DeviceData::EnumerateInterfaces`. **Attempt 24 dies in the same place**, exit 3:

    [Dx11Seam] FORWARD TO ORIGINAL: DeviceData slot 1
      (DeviceData::EnumerateInterfaces) — but the shipped DLL is not loaded,
      so there is no original. Returning null; the caller will fault.
    Fault: EXEC of 0000000000000000   (VCALL-of-0)

That is expected, not disappointing: Waves 12–15 worked on Application, Save and
the `_singleton` data rows. **None of them was on the boot path**, so none of
them could have moved this wall. The gate is confirming that, which is the
question it exists to answer.

## What blocks it — six functions, entirely unported

The immediate blockers are the `Video::Device` interface-enumeration cluster.
`grep -rn` across `runtime/src/` returns **zero** references to either class, so
these are not ported-but-unregistered — they are absent:

    ??0GraphicsInterface@Device@Video@@QEAA@XZ            default ctor
    ??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z    copy ctor
    ??1GraphicsInterface@Device@Video@@QEAA@XZ            dtor
    ??0Adapter@Device@Video@@QEAA@XZ                      default ctor
    ??0Adapter@Device@Video@@QEAA@AEBU012@@Z              copy ctor
    ??1Adapter@Device@Video@@QEAA@XZ                      dtor

Each failure is a DX11 `devicedata` IAT import that could not resolve, followed
by nine more `Imports field #N is null` entries — the DX11 seam wants a Runtime
import table it cannot fill.

## Full residue — 17 distinct symbols, and it is a LOWER BOUND

A cutover boot is a GATE, not a PLANNER: the residue can only name what the
process reached before faulting, and it faulted after 82 stderr lines. Do not
read this as the remaining work.

    Video::Device      ??0/??1 Adapter, ??0/??1 GraphicsInterface,
                       AdjustDisplay, FindOrAddMode, GetDisplayMode
    Graphics           Draw@Renderer, BuildParameters@MaterialInstance,
                       Serialize@MeshInstance, Update@AnimationManager
    Object             FindOrLoad@ExternalFactory, SaveText@External
    Audio              Update@Scene
    Engine             LoadResources@UpdateLoop

**Do not compare this 17 with Wave 10's 17.** Wave 10 recorded residue moving
17 → 25 and called the growth PROGRESS, because the 11 entrants were the closed
body's own callees. A count returning to 17 is not a return to that state; only
a set diff would say what changed, and this run did not do one.

## Consequence for planning — this changes the next wave

**Wave 11 (the object creation path) is NOT on the cutover critical path.** The
wall is a six-function `Video::Device` cluster. Wave 11 remains the largest
untouched subsystem and the worst-covered namespace (`Object` 57.1%), so it is
real census work — but it will not move this boot one line.

Two genuinely different objectives, now clearly separated:

| objective | work | payoff |
|---|---|---|
| **make the DLL-absent boot advance** | the 6-function `Adapter`/`GraphicsInterface` cluster, then whatever the next wall is | cutover readiness |
| **raise the census** | Wave 11 object creation (24-function gap), or Video/Graphics (74) | breadth |

The cluster is small, precisely named, and provably unported — the cheapest
next wall by a wide margin. Note that `Video` also carries the largest absolute
census gap (150 called / 109 ported = 41), so this cluster is the rare piece of
work that serves both objectives at once.

One caution for whoever takes it: `?AdjustDisplay@Device@Video@@` appears in this
residue AND in Wave 15's lane INSTR output as a `PORTED-UNREGISTERED` review
flag — it owns 6 `Configuration@Video` reads and nothing registers it. Check that
before treating it as unported work.
