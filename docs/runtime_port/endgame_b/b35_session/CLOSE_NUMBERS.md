# B.3.5 CLOSE NUMBERS — the step's measurements, for the STATUS block

Measured by `b35-close` on its own branch at the end of the closing pass, on the
tree that carries every class block (A–H2, GC, VC, DL), every caller batch (I1–I4,
J, K1, K2) and this pass's six commits. Every number below is from a script run on
THIS tree, not carried over from a brief. The scripts are in the session
scratchpad; each measurement says what it counts, because several of these numbers
are only comparable to the start-of-step figures if the unit is the same.

---

## 1. `reinterpret_cast` — LINE counts per directory

`grep -rc` semantics (LINES containing the token, summed), over `*.cpp *.h *.inc`,
`build*/` and `obj/` excluded. This is the B.4 metric: a conversion that bridges a
`void*` or a POD receiver onto a real class is REQUIRED to spell it
`reinterpret_cast`, so the number is expected to RISE during B.3 and to fall only
in B.4 when the receivers themselves become typed.

| directory | start of step | now | delta |
|---|---:|---:|---:|
| `code` | 23,466 | **23,760** | +294 |
| `runtime` | 6,607 | **7,009** | +402 |
| `standalone` | 2,891 | **2,889** | −2 |
| `engine` | 823 | **756** | −67 |
| `dx11` | 149 | **168** | +19 |
| `tests` | 120 | **243** | +123 |
| **TOTAL** | **34,056** | **34,825** | **+769** |

Reading it: `runtime` and `code` rose because that is where the bridges are —
every converted call site on a `void*` receiver adds one. `engine` FELL by 67,
which is the interesting direction and is this pass's: deleting 42
`engine_funcs.{h,cpp}` door globals removed their `reinterpret_cast<fn_X_t>(...)`
resolver lines. `tests` rose with the new ABI probes. `standalone` and `dx11` are
flat to within a handful.

## 2. `// B.3.5 bridge` — LINE counts per directory

The marker every converted call site carries, so the B.4 pass can find them.

| directory | lines |
|---|---:|
| `code` | 368 |
| `runtime` | 508 |
| `standalone` | 6 |
| `engine` | 2 |
| `dx11` | 26 |
| `tests` | 1 |
| **TOTAL** | **911** |

`engine`'s 2 are both this pass's (`Camera::SetCameraToWorld` in the inline
camera-snap helper, plus its caller note). `dx11`'s 26 are block J's seam
dispatches.

## 3. The caller surface vs B35_RULES §7's table

§7 counted CALL SITES with the literals joined across line wraps. Same unit here:
one site = one call whose string argument, after concatenation, names a
`@Video@@` or `@Graphics@@` symbol. §7's rows (b) and (c).

| binding | §7 total | now | code/Game | engine | standalone | runtime/src | dx11/src |
|---|---:|---:|---:|---:|---:|---:|---:|
| (b) `ResolveExport("…@Video@@/@Graphics@@…")` | 246 | **108** | 21 (was 78) | 17 (was 74) | 35 (was 36) | 33 (was 45) | 2 (was 13) |
| (c) `RT_EXPORT_ONCE(…@Video@@/@Graphics@@…)` | 147 | **54** | 0 | 0 | 0 | 54 (was 147) | 0 |
| (a) `fn_Video_*`/`fn_Graphics_*` engine_funcs doors | 61 sites | **0 sites** | 0 | — | — | — | — |

(a) is the one that went to zero. §7 measured 61 call sites through 21 declared
globals; the sweep that matters counts by BINDING, not by name, and found 76
globals binding a Video/Graphics symbol. Batch b35-K2 deleted 16; this pass's
commit 1 deleted 38 more, commit 2 the three whose last reader was a `%p` in a
diagnostic printf, commit 4 `fn_Camera_SetCameraToWorld` (the last row bound BY
ADDRESS, which is why three name-anchored sweeps walked past its four callers),
and commit 5 `fn_EdgeMesh_Set` and `fn_InterpolatedState_Transform_Lerp` with
their ports. **60 of the 76 retired in this pass.**

Re-run on the final tree, the sweep reports **17 globals binding a Video/Graphics
symbol and ZERO at zero callers**:

- **16 are CTOR doors**, kept by B35_RULES §11 / B4b-2 — no Video/Graphics class
  has a real constructor, so the doors and their call sites stay:
  `fn_EdgeMesh_TriangleVertex_Ctor` (11), `fn_ImageBuffer_Ctor` (10),
  `fn_Graphics_Camera_Ctor` (10), `fn_MaterialInstance_Ctor` (6),
  `fn_Graphics_Renderer_Ctor` (6), `fn_Graphics_Scene_Ctor` (6),
  `fn_EdgeMesh_EdgeVertex_Ctor` (5), `fn_Graphics_MeshInstance_Ctor` (5),
  `fn_Graphics_Geometry_Ctor` (5), `fn_Graphics_SunLight_Ctor` (4),
  `fn_Video_VertexBuffer_Ctor` (4), `fn_Video_IndexBuffer_Ctor` (4),
  `fn_Graphics_ParticleSystemInstance_Ctor` (3), `fn_EdgeMesh_Ctor` (2),
  `fn_EdgeMeshInstance_Ctor` (2), `fn_Graphics_Particle_Ctor` (2).
- **1 is not a Video/Graphics door at all**: `fn_UI_Draw` (5) binds
  `?Draw@UserInterface@UI@@QEAAXAEAVContext@Video@@@Z`, where `@Video@@` appears
  only in a PARAMETER type. A `UI::UserInterface` member. Batches K2 and I4 made
  the same call; recorded a third time so the next sweep does not re-litigate it.

**No non-ctor Video/Graphics door global is left in `engine_funcs`.** The build
proves it: extern and definition die together, so a surviving reader anywhere is a
compile error, not a silent EXEC of 0.

`standalone` barely moved (36 → 35) because its remaining hits are
`?_singleton@Device@Video@@` and `?_singleton@Configuration@Video@@` static-storage
reads, which §9 keeps until the class-static conversion at the END of B.3.

## 4. Hand-inlined `(*(void***)` sites

| directory | §7 (e) | now |
|---|---:|---:|
| `runtime/src/video` | 27 | **4** |
| `runtime/src/graphics` | 22 | **7** |
| `dx11/src` | 0 | **0** |
| `code/Game` | 69 (plausible-receiver) | **18** |

§7's own item 9 flagged (e) in `code/Game` as a PLAUSIBLE-receiver count over a
keyword-selected file set, not a proven-receiver count, so the 69 → 18 comparison
is indicative and the 18 is the exact count of the token today. The `runtime/src`
numbers are like-for-like. The 11 left in `runtime/src/{video,graphics}` are the
ones whose receiver is a `*Data` companion or an unproved pointer; the nine
`ReaderWriter` int64 slot calls B35_RULES §11 holds back are separate and are not
in this token count.

## 5. `tools/runtime_census.py` — the Video and Graphics rows

The tool prints per-namespace rows; both, verbatim from this tree:

```
namespace         total   called   ported   detour  called covered  ceiling
Graphics            599       92       70        0   76.1%   95.7%
Video               573       79       54        0   68.4%   77.2%
```

Population: 1288 rows (1113 resolve-door/detour rows + 190 minted-vtable slot rows
from the 20 promoted vtables of 152 minted).

## 6. Unit tests

**358 tests, 0 failures** (`mingw32-make unit`, exit 0). 352 at the start of this
pass; the six new ones are `tests/unit/graphics_interpolatedstate_abi_test.cpp`.

## 7. Video/Graphics classes declared as REAL classes

Counted as a `class`/`struct` with a definition inside a `namespace Video {` or
`namespace Graphics {` block in `runtime/types/{video,graphics}/*.h`.

| namespace | real classes |
|---|---:|
| `Video` | **27** |
| `Graphics` | **32** |
| **TOTAL** | **59** |

`Video` (27): Configuration, ConstantBuffer, ConstantBufferData, Context,
ContextData, Device, DeviceData, ImageBuffer, ImageBufferData, IndexBuffer,
IndexBufferData, PixelProgram, PixelProgramData, RenderBuffer, RenderBufferData,
RenderState, RenderStateData, ShaderProgram, VertexBuffer, VertexBufferData,
VertexLayout, VertexLayoutData, VertexProgram, VertexProgramData, VideoProgram,
VideoProgramData, Viewport.

`Graphics` (32): AmbientLight, Animation, AnimationEvent, AnimationGroup,
AnimationManager, AnimationState, Camera, EdgeMesh, EdgeMeshInstance,
FrameCopyImageBuffer, GeneratedImageBuffer, Geometry, GraphicsMesh,
**InterpolatedState**, Light, Material, MaterialInstance, MeshInstance, Particle,
ParticleInstance, ParticleSystem, ParticleSystemInstance, ReflectionImageBuffer,
Renderer, RendererDrawGroup, RendererDrawItem, RendererDrawable,
RendererSceneEntry, Scene, SceneNode, SpotLight, SunLight.

`InterpolatedState` is NEW in this pass (closing item 4d). The count includes the
nested `Device::{Adapter, DisplayMode, RefreshRate, GraphicsInterface}` only where
they are declared at namespace scope; nested classes are folded into their outer
row by the counter and are not double counted.

## 8. Declared-undefined members per class

Class scope tracked by BRACE DEPTH so nested classes keep their outer name;
pure-virtual (`= 0`), `= default`, `= delete` and inline-bodied declarations are
excluded, since none of those is a missing definition. "Defined" means a
`Class::Member(` definition exists under `runtime/src`.

**425 declared-undefined members across 56 classes, out of 715 declarations
examined across 62 classes.**

| class | declared | undefined |
|---|---:|---:|
| `Video::Context` | 71 | **47** |
| `Video::Configuration` | 42 | **40** |
| `Video::Device` | 59 | **34** |
| `Graphics::GraphicsMesh` | 29 | **23** |
| `Video::ImageBuffer` | 45 | **21** |
| `Graphics::EdgeMesh` | 23 | **20** |
| `Graphics::MeshInstance` | 27 | **17** |
| `Graphics::Animation` | 18 | **16** |
| `Graphics::Material` | 18 | **16** |
| `Graphics::MaterialInstance` | 22 | **15** |
| `Video::VertexLayout` | 20 | **15** |
| `Graphics::AnimationState` | 16 | **13** |
| `Graphics::AnimationGroup` | 12 | **10** |
| `Video::RenderBuffer` | 17 | **10** |
| `Graphics::Geometry` | 18 | **9** |
| `Video::VideoProgram` | 20 | **9** |
| `Graphics::AnimationManager` | 15 | **8** |
| `Video::ConstantBuffer` | 12 | **8** |
| …42 more classes | | 6 or fewer each |

TWO MEASUREMENT CAVEATS, stated rather than buried. The brace tracker mis-nests
`Material.h`'s two classes under `VideoProgram` (they are `Graphics::Material` and
`Graphics::MaterialInstance`; corrected by hand in the table above), and it counts
the new `InterpolatedState::Lerp` as undefined because that definition is a
header-inline template specialisation under `runtime/types`, not a
`Class::Member(` under `runtime/src`. Both are matcher artifacts. **425 is
therefore a CEILING, right to within those few rows, not an exact count** — which
is the only claim the instrument supports.

The related audit, done at the same time: all **145** `not ported` / `NOT PORTED`
notes in the 19 `runtime/types/{video,graphics}/*.h` headers were matched against
the definitions under `runtime/src`. **Zero name a member that is actually
defined.** Eleven were flagged and all eleven were false positives of the matcher,
read by hand — see commit 6's message for each one's artifact.

## 9. Gate and runtime, at the close

```
mingw32-make -j12 standalone-debug   exit 0
mingw32-make -j12 standalone-perf    exit 0
mingw32-make -j12 unit               exit 0, 358 tests / 0 failures
python tools/dispatch_slot_map.py --check   exit 0, 109 ResolveExport
    registrations, 0 name mismatches, 0 addresses registered more than once
python tools/cutover_manifest.py --gate     OK, 59 rows
python tools/rt_mint_gen.py --check         UP TO DATE (152 vtables,
    104 BaseTypeInfo, 48 storage)
```

New-game route, under the game-dir lock, foreground
(`BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120`):

```
exit 0
standalone_crash.log      0 bytes
standalone_hang.log       absent
[RtResolve] 0 misses — every cached resolve in this run found its symbol.
[RtMint] TRAP             0 occurrences
[ExtLoad] calls=5686 openFileFail=3 openCompFail=0 nullSer=0 badMagic=0
          created=5683 reused=0 notExternal=0 badVer=0 sized=5683 deps=3816
          serialized=5683 committed=0 retNull=3
"[GUL::Load] redundant dispatch"  0 occurrences
```

Resave gate (`tools/resave_gate/resave_gate.sh Gate`):

```
[gate] exit status: 0
[gate] crash log bytes: 0
[gate] verdict: PASS (resave == golden, 21075114 bytes)
```

The golden `.md5` came back byte-identical to HEAD
(`757e48a15eac71eff097508da169c23c`); the only working-tree change the gate made
to it was line endings, reverted with `git checkout --`.
