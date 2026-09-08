# Wave 10 — the Video::Device interface cluster

**Branch:** `wave10/base` (from `wave/upgrade-path-complete` @ 8f2538f)
**Opened:** 2026-09-08. Supersedes Wave 9, which is consolidated and closed
(`docs/runtime_port/WAVE9_KICKOFF.md`).

Wave 9 ran three times under one number. This is a new wave with a new number and
a starting point that was **measured, not inherited**: attempt 23, the DLL-absent
cutover boot, run at this tip.

---

## 1. The result that sets the agenda

Attempt 23. `Runtime-x64.dll` renamed aside, runbook env matrix, exe md5
`e52520b0ff3ec29cf03c7e939fa92ab7`, deployed 19:22. Genuinely DLL-less — the run
printed `[RtMintShadow] ==== phase=init DLL=ABSENT (post-cutover; oracle gone) ====`,
which is the proof, since that banner can only print when there is nothing to
compare against.

**The wall moved. `EnumerateVideoModes` is gone from the residue.**

Lane EVM's port did what it was for. The new wall, named by the run in one line:

```
[Dx11Seam] FORWARD TO ORIGINAL: DeviceData slot 1 (DeviceData::EnumerateInterfaces)
  — but the shipped DLL is not loaded, so there is no original. Returning null;
  the caller will fault. A Runtime import failed to resolve.
```

The crash is the fault that line predicts: `EXEC of 0` at RIP=0, i.e. a call
through the null it returned. Corroborated independently by
`[Dx11Seam] fallbacks to shipped code: 1 (expected 0)` — one fallback, this one.

## 2. The set-diff — why 17 → 25 is progress, not regression

The count went UP. The count is not the measurement; the set is
([[reference_diff_the_set_not_the_count]]). Diffed against the archived attempt-22
list (`evidence/attempt22/`), stored beside this wave's own at
`evidence/attempt23/rtunresolved_names.txt`:

**LEFT the residue (3) — closed by Wave 9:**
- `?EnumerateVideoModes@Device@Video@@QEAA_NXZ` ← attempt 22's wall
- `?OnVideoError@MainLoop@Engine@@QEAAXXZ`
- `?OnVideoRestart@MainLoop@Engine@@QEAAXXZ`

**ENTERED the residue (11) — and they are EnumerateVideoModes' own callees:**

| symbol | |
|---|---|
| `??0Adapter@Device@Video@@QEAA@XZ` | Adapter ctor |
| `??0Adapter@Device@Video@@QEAA@AEBU012@@Z` | Adapter copy ctor |
| `??1Adapter@Device@Video@@QEAA@XZ` | Adapter dtor |
| `?FindOrAddMode@Adapter@Device@Video@@QEAAXAEBVDisplayMode@23@AEBVRefreshRate@23@@Z` | |
| `??0GraphicsInterface@Device@Video@@QEAA@XZ` | GraphicsInterface ctor |
| `??0GraphicsInterface@Device@Video@@QEAA@AEBU012@@Z` | copy ctor |
| `??1GraphicsInterface@Device@Video@@QEAA@XZ` | dtor |
| `?AdjustDisplay@Device@Video@@QEAAXXZ` | |
| `?GetDisplayMode@Device@Video@@QEBA?AVDisplayMode@12@H@Z` | |
| `?GetInstance@Application@1@QEBAPEAUHINSTANCE__@@XZ` | |
| `?_disableHandler@Debug@System@@0_NA` | data, not a function |

This is [[reference_porting_can_grow_the_cutover_surface]] behaving exactly as
documented: porting a body replaces one unresolved name with its callees. **A
growing residue here is the shape of progress.** The wave is not measured by the
count but by which door is open.

## 3. Wave 10's scope

One coherent class family, which is why this is a wave and not a scattering of
lanes: **`Video::Device::Adapter` and `Video::Device::GraphicsInterface`**, plus
the two `Device` accessors and `Application::GetInstance`. Ten functions and one
data symbol, all reached from the same place.

The gating consumer is `DeviceData::EnumerateInterfaces` (VideoDX11 DeviceData
vtable slot 1). Closing the cluster is what lets that slot stop forwarding.

## 4. One instrument caveat, confirmed live — do NOT chase the asked-by address

Ten of the eleven new names report `asked by 00007ff6e22002bc`. Symbolized against
the deployed exe that is:

```
00000001403402a0 T VideoDX11::Dx11ResolveRuntimeImport(char const*, unsigned long long)
```

which is **the resolver, not the caller** — a live confirmation of
[[reference_diagnostic_field_cannot_express_the_answer]]. The shared address means
"these were all resolved through the dx11 import shim", not "these share a call
site". Anyone reading it as a call site will localize to the wrong function. The
names are the finding; the asked-by column is not.

Also recorded from this run, unexplained and cheap to look at first:
`[Dx11] devicedata Imports fields #26, #27, #28, #44 are null (no table entry?)`.

## 5. Inherited debt — from the Wave 9 consolidation, not from this run

- **The RELEASE target does not build**, and did not before Wave 9's consolidation
  either: `std::printf` against `standalone/log_strip.h`'s `printf(...)` macro,
  `runtime/src/collection/array_destroy.cpp:942` and instances in `array.cpp`.
  Only the debug target is green. This is a cheap, self-contained first task.
- **Lanes EVM and SERIAL are merged but NOT VERIFIED**, by their own sign-off, and
  lane RRES2's rung 2 has an intentionally empty §5. Merged on explicit direction
  to get one kickoff tip; the verification is still owed. Note that lane EVM's
  work is now *partly* vindicated — its wall left the residue — but "the door
  opened" is not "the body is correct".
- Lane EVM's refresh-rate comparator divides both sides by B's denominator. That
  is transcribed retail behaviour and must stay: at `den == 0` it yields inf/NaN
  and COMISS unordered, which is load-bearing for video-mode ordering.

## 6. Reproducing attempt 23

`docs/runtime_port/WAVE9_CUTOVER_BOOT_RUNBOOK.md`, unchanged and still accurate.
Hold the game-dir lock, deploy, quote the md5, rename the DLL aside, run the env
matrix, **restore the DLL pass or fail**, archive under `evidence/attempt24/`, and
set-diff against `evidence/attempt23/rtunresolved_names.txt` rather than against a
recollection of this document.
