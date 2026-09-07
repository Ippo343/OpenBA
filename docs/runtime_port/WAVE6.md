# Wave 6 — Video + Graphics

In progress on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2 wave 6.

## Scope

`tools/runtime_census.py` (2026-09-07, before any Wave 6 registration):

| Namespace | in DLL | called by our tree | ported |
|---|---|---|---|
| Graphics | 599 | 82 | 0 |
| Video | 573 | 66 | 0 |

The wave's denominator is the **148 functions our tree actually calls**, not the 1,172 the
two namespaces contain. Per-function lists with call sites: `wave6_lanes.txt`.

### Why this wave is different from 1–5

**VideoDX11 is already ours** (`dx11/`), so `Video::Context` / `Video::Device` sit between two
implementations we own on one side (`code/Game/`, `dx11/`) and the DLL's own `Graphics::Renderer`
on the other. Registering a `Video::Context` port redirects our callers and `dx11/`'s, but the
DLL's internal `Renderer::Draw → Context::*` edges keep going to the DLL. That is safe **only
while the port is byte-faithful on a shared object layout** — both implementations must be able to
operate on the same live object. Any lane that would change a layout, allocate differently, or
stamp a vtable must hold back and say so (PLAN §4.1 decision 5, §4.3 risk 3).

This is also the first wave where the "DLL's own internal callers" problem (PLAN §4.2 closing
note) is genuinely live. No lane installs a detour without saying why in its doc.

### Lanes

| Lane | Scope | File | Kill switch |
|---|---|---|---|
| A | `Video::Context` (27) | `runtime/src/video/context.cpp` | `BANISHED_RT_VIDCONTEXT` |
| B | `Video::Device` + `Configuration` + `RenderBuffer` (17) | `runtime/src/video/device.cpp` | `BANISHED_RT_VIDDEVICE` |
| C | `Video::ImageBuffer` + `IndexBuffer` + `VertexBuffer` (22) | `runtime/src/video/buffers.cpp` | `BANISHED_RT_VIDBUFFERS` |
| D | `Graphics::Geometry` + `Scene` (20) | `runtime/src/graphics/scene.cpp` | `BANISHED_RT_GFXSCENE` |
| E | `Graphics::MeshInstance` + `GraphicsMesh` + `EdgeMesh*` (24) | `runtime/src/graphics/mesh.cpp` | `BANISHED_RT_GFXMESH` |
| F | `Graphics::Camera` + `Renderer` + `SunLight` + `Material*` (20) | `runtime/src/graphics/renderer.cpp` | `BANISHED_RT_GFXRENDER` |
| G | `Graphics::Animation*` + `Particle*` + `FrameCopyImageBuffer` (18) | `runtime/src/graphics/animation.cpp` | `BANISHED_RT_GFXANIM` |

Namespace-level switches: `BANISHED_RT_VIDEO=0`, `BANISHED_RT_GRAPHICS=0`.

## Pre-wave control (2026-09-07, scaffolding only, 0 Wave 6 ports)

`tools/rt_run.sh wave6-control -- BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900
BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6`

**103 sites, 24,858,882 calls, 102 divergences in 2 sites.** Clean exit, empty
`standalone_crash.log`. The two divergent sites are the pre-existing Wave 1 1-ULP sin/cos family
and nothing else — `Math::Transform::FromYawPitchOffset` (85) and `Math::Vector3::FromYawPitch`
(17). Full site table: `wave6_control_autostart.txt`.

A second control on the save-load path, same exe:

`tools/rt_run.sh wave6-control-save9 -- BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1
BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6`

**114 sites, 29,602,036 calls, 49 divergences in 3 sites** — the same Wave 1 family plus
`Math::Vector3::Heading` (1 of 74,002), exactly as Wave 5's control reported. World built
(`[ChunkedSceneLoader] DONE ... PhaseB 15533 elements`), clean exit, `standalone_crash.log` 0
bytes. Full site table: `wave6_control_save9.txt`.

Note per WAVE5.md: **do not pass `BANISHED_MODAL_ANSWER` on the AUTOLOAD path** — it overrides
the `modsMissing` confirm and the load silently never happens.

**Any NEW divergent site after Wave 6 lands is a Wave 6 defect.**

## Defects found by the wave (not ports)

### 1. `Graphics::Geometry::Attach` / `Detach` were dead code — phantom names, no fallback

`tools/export_name_audit.py`, run at the start of the wave, reported both names as absent from
the shipped DLL's export table:

    ?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z    code/Game/Action/action_base_complete.cpp:383
    ?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z     code/Game/Action/action_base_complete.cpp:385

The DLL only ever exported the POINTER spelling — `PEAV12@`, not `AEAV12@`:
`?Attach@...@QEAAXPEAV12@H@Z` at `0x1800a3fa0` and `?Detach@...@QEAAXPEAV12@@Z` at `0x1800a3f20`.

What makes this more than a naming nit: **`ResolveExport` has no address fallback**
(`standalone/resolve_standalone.cpp:422` — registry hit, else `GetProcAddress`, else null), and
both call sites are written `if (s_GeomAttach) s_GeomAttach(...)`. So the pointers were null, the
guards took the skip branch, and **every attach and detach this file performs has been silently
dead since the day it was written** — the exact shape CLAUDE.md warns about, a guard that does not
restore the behaviour but skips it.

Fixed at the source (both literals plus a comment saying what they were), 2026-09-07. This is NOT
a port: it is a live functional gap the wave's opening audit turned up, and it is listed separately
here so the first integration run's behaviour can be attributed to it rather than to a lane.

## Verification

### The one confound this wave has to separate

The `Geometry::Attach`/`Detach` phantom-name fix above means those two functions execute for the
FIRST TIME in the standalone's history — and lane D ported them, so their first execution is also
our reimplementation's first execution. A fault there has two candidate causes.

The A/B that separates them is free: `BANISHED_RT_GFXSCENE=0` leaves lane D unregistered, so the
now-correct names resolve to the DLL and the fix is exercised alone. Run it before blaming either.

(runs filled in below as they happen)

## Outcome

(filled in at close)
