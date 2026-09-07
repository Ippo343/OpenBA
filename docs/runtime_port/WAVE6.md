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

### Runs

All on the `-O0` `standalone-debug` exe with `BANISHED_RT_DIFF=1` and a pause-menu self-quit
(`RtDiffSummary` prints from `WinMain`, so a killed process reports nothing).

| # | Run | Sites | Calls | Divergences |
|---|---|---|---|---|
| 0a | control, autostart (pre-wave) | 103 | 24,858,882 | 102 / 2 sites |
| 0b | control, `AUTOLOAD=Save9` (pre-wave) | 114 | 29,602,036 | 49 / 3 sites |
| 1 | autostart, first integration | — | — | **CRASHED** in `MeshInstance::SetMesh` |
| 2 | autostart, after the SetMesh fix | — | — | **CRASHED** in `RootAnimationOffset` |
| 3 | autostart | 180 | 25,934,324 | 32,648 / **7 sites** |
| 4 | autostart + `BANISHED_RT_LIBM=dll` | 183 | 25,964,276 | 31,268 / 5 sites |
| 5 | autostart, after the lane E/G fixes | 183 | 25,487,765 | 88 / 3 sites |
| 6 | `AUTOLOAD=Save9` | 200 | 49,943,156 | 47 / 3 sites |
| 7 | autostart, FINAL | **180** | **25,858,610** | **101 / 2 sites** |
| 8 | `AUTOLOAD=Save9`, FINAL | **200** | **49,643,611** | **59 / 2 sites** |
| 9 | autostart + `TOOL_HARNESS=select` | 194 | 26,516,987 | 104 / 2 sites |

**Runs 7 and 8 are the wave's result: every remaining divergence is the pre-existing Wave 1
1-ULP sin/cos family, and there is not one Wave 6 divergence in either.** Both exit cleanly with
a zero-byte `standalone_crash.log`; run 8 builds the world (`PhaseB 15533 elements`).

**Run 4 is the run that matters methodologically.** `MeshInstance::UpdateHierarchy` was diverging
by 1 ULP, which is the exact signature of the known Wave 1 libm family, and the tempting move was
to file it there and move on. Under `BANISHED_RT_LIBM=dll` the two Wave 1 sites go to ZERO while
`UpdateHierarchy` goes UP, 2110 -> 2593. That one free A/B is what turned "known residual" into a
real defect — a reassociated float sum, the same shape as Wave 1's `Transform::FromDirection`.

### Coverage

Run 7 has **zero `calls=0` sites**: every diff site that registered also executed. That is not the
same as every port executing, because an unexercised port never reaches `RT_DIFF_BEGIN` and so
never registers a site at all. Measured against the site list:

* **86 of the 113 ports executed** in the autostart run, including `Geometry::Attach` and
  `Detach`, whose first execution in this project's history is the phantom-name fix above.
* **2 of the 7 EdgeMesh ports** (`EdgeMeshInstance::EdgeMeshInstance`, `EdgeMeshInstance::SetMesh`)
  executed under `BANISHED_TOOL_HARNESS=select`, 0 divergences. The other five — `EdgeMesh` ctor,
  `EdgeMesh::Set`, `SetMaterials`, and the `EdgeVertex` / `TriangleVertex` ctors — build the
  outline MESH rather than an instance of it, and neither `select` nor `selectcitizen` reaches
  them. **Registered, reachable, NOT verified.**
* `??0EdgeMeshInstance@Graphics@@QEAA@AEBV01@@Z` (the COPY ctor) is now in the census orphan list
  with no caller at all. That is a direct consequence of this wave's own `engine_funcs` fix: its
  only "caller" was the fallback that resolved 0x1800174f0 under the DEFAULT ctor's name, and that
  fallback was wrong and is deleted. The port is registered and cannot be verified by any run.

## Outcome

**113 of the 148 functions our tree calls, ported and registered.** Census after the wave:
**756 called, 360 ported (47.6%)** — Video 53/69 (76.8%), Graphics 59/87 (67.8%).
The denominators moved from 66/82 because the lanes' own `RtOriginal` oracle calls are themselves
call sites the census counts, exactly as Wave 5 saw.

| Lane | Scope | Ported | Held back |
|---|---|---|---|
| A | `Video::Context` | 23 | ctor / dtor / `operator delete` / `ClearResources` |
| B | `Video::Device` + `Configuration` + `RenderBuffer` | 9 | `Startup`, `Shutdown`, `RecreateDynamic`, `ReleaseDynamic`, `RenderBuffer` ctor/dtor |
| C | `ImageBuffer` + `Index`/`VertexBuffer` | 21 | `SaveImageToFile` |
| D | `Geometry` + `Scene` | 19 | none |
| E | `MeshInstance` + `GraphicsMesh` + `EdgeMesh*` | 20 | `AddDrawItems`, `MeshInstance::Serialize` |
| F | `Camera` + `Renderer` + `SunLight` + `Material*` | 12 | `Renderer::Draw`, `AddDrawItem`, `MaterialInstance::Setup`, `BuildParameters`, `SunLight` ctor + `Set` |
| G | `Animation*` + `Particle*` + `FrameCopyImageBuffer` | 9 | `AnimationManager::Update`/`Serialize`, all four `FrameCopyImageBuffer` |

### The hazard this wave was supposed to hit, and what actually happened

PLAN §4.2 warned that Wave 6 would be the first to need the detour, because the DLL's own
`Graphics::Renderer` calls `Video::Context` internally and registration redirects only our callers.
**No detour was installed, and none was needed.** Two independent findings retired the worry:
`Video::Context` has NO VTABLE (lane A), so a partial port shares one live object with the DLL
safely as long as the layout is faithful; and `Geometry`'s vtable IS exported, so lane D's ported
ctor stamps the DLL's own `??_7Geometry` byte for byte rather than minting one. The census
`detour` column is still 0 across all 4,632 functions, and PLAN §4.2's closing note still stands
unexercised.

`Renderer::Draw` is the one place the hazard was real, and lane F held it back for exactly that
reason: it writes `Video::Context`'s private fields out to +0x2160 while lane A was pinning that
same layout in another file. It wants its own lane now that A and D have landed.

### Five defects, and one root cause behind three of them

1. `MeshInstance::SetMesh` passed `&field` where retail loads `*field` (crash).
2. `RtInstancedPoolSlot` missed a dereference — `AnimationManager::_singleton` is one star in
   Ghidra's C output and two `MOV`s in the machine code.
3. The same helper zero-extended a `MOVSXD` and shifted logically where retail shifts
   arithmetically, selecting the wrong 0x30-byte bank.
4. `UpdateHierarchy` reassociated a float sum (parent translation folded in after the first term
   instead of last). X was right by accident; only Y and Z were wrong.
5. `PlayAnimation`'s oracle read `layers[count-1]`; the layer array inserts at the FRONT.

**Defects 2, 3 and 4 all came from reading Ghidra's decompiled C where only the disassembly is
authoritative.** The decompile dropped a dereference twice and printed a sign extension as a zero
extension once. That is the wave's most transferable lesson and it belongs in every later wave's
brief.

### Two oracle-design defects, which are not port defects

`MeshInstance()` (30130/30130) and the two `Particle`/`ParticleSystemInstance` ctors (100% each)
reported total divergence while the ports were correct. In every case the oracle memcmp'd a range
containing bytes **neither implementation writes** — retail's ctors leave gaps — so it compared
allocator residue against 0xCD poison and could never agree. Four gaps were measured this way:
`MeshInstance` +0xd9..+0xdf, `Particle` [0x21,0x24) and [0x2c,0x30), `Geometry` [0x14,0x18).

**Rule for Wave 7:** an oracle must compare only the bytes the function under test actually
writes. Walk the store list in the disassembly and exclude every address the stores skip. A
whole-struct memcmp against poison fails 100% of the time and reads like a catastrophic port
defect rather than a measurement artifact. Wave 7 is UI — 1,625 functions of exactly this shape.
