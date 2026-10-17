# TS-CORE -- `ext.billboard_shadow` "Stable tree shading" (hand-in, 2026-10-16)

Blocks TS-SPIKE + TS-CORE + TS-MENU of `docs/extensions/TREE_SHADOW.md` §5. The design is option (e),
TREE_SHADOW.md §4, which the coordinator approved, with these decisions: blueberry shares the fix; the
row is `ext.billboard_shadow`, class A, default OFF; the carrier is `textureProjection` (cb2 rows
6..9); the engine seam is a null-by-default `const bool*` read in `SunLight::DrawLight`.

- **Branch:** `worktree-agent-af0b0326b58439bc0`, fast-forwarded to main `cf16b65ff` first.
- **Commits:** `2a49a0174` (the TREE_SHADOW.md design) and `d8f3d731f` (the code). Not merged.
- **Spike verdict: PASS.** The spike's prototype is the general rewriter itself. It met every
  done-condition, so the CORE work went on from the same code.

## 1. What was built

| Piece | Files | What |
|---|---|---|
| The rewriter (pure) | `src/render/dx11/billboard_shadow/receiver_rewrite.{h,cpp}` | See "The rewriter, in detail" below. |
| The stage (D3D) | `src/render/dx11/billboard_shadow/billboard_shadow_stage.{h,cpp}`, `billboard_shadow_backend.h` | See "The stage, in detail" below. |
| D3D seam | `src/render/dx11/shaderprograms.cpp` | `VideoDX11::g_extBillboardShadow`, null by default. With the row off, `VertexProgramData::Bind`, `PlatformCreate` and `~VertexProgramData` each pay one atomic load and a test. |
| Engine seam | `src/engine/graphics/SunLight.h`, `sunlight.cpp`, `lights_vtable.cpp` | `SunLight::_extBillboardShadow` (`const bool*`, null by default and under the faithful profile). See "The engine seam, in detail" below. |
| Registry row | `src/ext/registry.{h,cpp}`, `registry_boot.cpp` | `ext.billboard_shadow`, class A, default OFF. Label "Stable tree shading". Description "Leaf cards read the shadow where they cast it: no line turning with the view." (77 characters, sized like the other rows). Boot installs the engine seam once (null under faithful) and runs `BillboardShadow::Sync`. Every toggle (`SetAndRecord`) runs `Sync` too. |
| The extension | `src/ext/billboard_shadow/billboard_shadow.{h,cpp}` | `Sync()` is `BbsBackend::Request(Ext().billboard_shadow)`. |
| Menu (TS-MENU) | `src/ext/billboard_shadow/enhanced_entry.cpp`, `src/ext/enhanced_tab/enhanced_tab.{h,cpp}`, `src/ext/ui/Dialog/EnhancedOptions.rsc` | A fifth row "Stable tree shading" in the Enhanced tab: combo Off / On (string table `offOnItems`; table height 4 -> 5). Apply calls `SetAndRecord` when the row changes. This follows the FAA-MENU pattern. |
| Unit tests | `tests/unit/ext_billboard_shadow_test.cpp`, `tests/unit/sources.cmake` | See "The unit tests, in detail" below. |
| Harness | `src/game/Game/camera_input.cpp`, `src/app/options.h` (comment only) | `BANISHED_CAMERA_PIN` takes an optional yaw: `x,y,dist,yaw` or `town,dist,yaw`. It pins the heading for the azimuth pairs. This extends an existing flag; no new flag was added. |
| Run script | `tools/billboard_shadow/run_arm.sh` | A copy of `tools/foliage_aa/run_arm.sh` that writes to `rdoc_tree/` under lane `tsspike`. |

### The rewriter, in detail

**What it matches.** A blob must meet all of these:

- It is a vs_4_x program and its container round-trips byte-identical.
- It has no flow control, no indexable temps, no extended opcodes, and `ret` appears exactly once,
  as the last instruction.
- There is one SHADOW OUTPUT: the output register written from `cb2[10..13]`.
- That output's component-exact backward slice reads `cb0[4..6]`, and never reads `cb0` with a
  relative index.
- `cb2` is declared with at least 10 rows and `cb0` with at least 8.

**What it emits.**

- `dcl_temps T` becomes `2T`.
- At the first slice instruction that depends on `cb0[4..7]`, it inserts `mov r[N+T], r[N]` for every
  temp.
- From that point, it clones every slice instruction beside the original. In the clones:
  - temps are renamed, including the temps inside relative indices (`cb3[r8.w + k]`);
  - `cb0[4..7]` becomes `cb2[6..9]`;
  - the instructions that write the shadow output are replaced by their clones.
- Every emit re-parses itself and checks its checksum and instruction count, or it is dropped.

### The stage, in detail

- It keeps one copy per `VertexProgramData` variant that matches.
- Copies are made at `PlatformCreate` once the row is on, or lazily at the first bind.
- A copy is bound only while `SunLight::_extBillboardShadowLive` is set. That flag is the emit's
  latch: `DrawLight` sets it, on the same thread, right after it writes the rows.
- A device change releases every copy; they are re-made lazily.
- An off request is applied at the next vertex bind: everything is released and the seam is cleared.
- One mutex guards the state.
- Each unmatched candidate is logged once: `[BillboardShadow] untouched <hash> v<i>: <why>`.

### The engine seam, in detail

When the row is on, `DrawLight` writes into `Context::LightStaging::_unwritten60` (cb2 rows 6..9).
What it writes is the sun group camera's `view.Inverse()`, built exactly as `DrawItemList` builds its
`view` and inverted exactly as `Context::SetCamera` inverts it. So the floats are the shadow draws'
`gc.worldToCamera`, bit for bit. It then sets dirty bit 0x2000 and the latch. When the row goes off,
it zeroes the rows once.

**The mod caveat.** No retail shader reads `textureProjection` and no retail body writes it; in
retail it is always zero. A mod's vertex shader that read `lc.textureProjection` while the sun light
is bound would see the sun camera's worldToCamera instead of zero, but only while this row is on.
Turning the row off restores zero. The row is OFF by default, so a player who never turns it on
sees no difference.

### The unit tests, in detail

The blobs are synthetic vs_4_0 programs. There are three tests:

- **Match and meaning.** Three filler lengths, each run with and without a renamed relative `cb3`
  read. A tiny SM4 interpreter runs the original and the rewritten program and checks:
  - `o0` and `o1` are unchanged;
  - `o3` equals the original's `o3` with `cb0[4..7] := cb2[6..9]`;
  - `o3` really moved.
- **Refusals.** It must refuse:
  - a blob with no shadow output;
  - a shadow output that does not depend on the camera rows;
  - `cb2` declared too small;
  - flow control;
  - a relative `cb0` read;
  - a pixel shader with the same body;
  - a broken checksum.
- **The registry row.**

## 2. The exact rewrite (spike question: instructions, cb2 size)

The game has one billboard colour vertex shader: `BillboardMaterial.srsl:Normal_vs`, checksum
`a88a6da1754867b0`, 4260 B. The four DETAIL variants are byte-identical: one hash, three copies
rewritten (v1..v3). DETAIL0 has no shadow output and is refused by the same grammar, as are
`Depth_vs` and every other vertex shader. The run logs no other candidate.

Its declarations are `cb0[12]`, `cb2[14]` and `cb3[208]` (dynamic). Rows 6..9 are inside `cb2[14]`,
so no declaration had to grow.

The log line:

```
[BillboardShadow] rewrote a88a6da1754867b0 v1: 4260 B -> 5740 B (temps 8 -> 16, shadow o3,
  45 slice instructions cloned from #34, 12 worldToCamera reads -> cb2[6..9])
```

What RenderDoc's disassembly of the bound copy shows (eid 1101 of `ts_on_y0.rdc`, 153 lines):

- 8 seeding `mov`s after instruction 20.
- Each `Billboard()` instruction is paired with its clone. Two examples:
  - `mad r12.xyz, _lc_.textureProjection[0].xyzx, r11.xxxx, r12.xyzx` beside
    `mad r4.xyz, _gc_.worldToCamera[0].xyzx, r3.xxxx, r4.xyzx`;
  - `mov r13.x, cb3[r8.w + 0].w` beside `mov r5.x, cb3[r0.w + 0].w`.
- The clip position still comes from `r0`, as in retail.
- `o3` now comes from `r8`: `add o3.xyzw, r8.xyzw, _lc_.shadowProjection[0][3].xyzw`.
- The driver accepted it: `CreateVertexShader` returned S_OK, with 0 create failures in every run.

## 3. Evidence, per done-condition

**Machine and setup**

- Windows 10, NVIDIA, 2560x1377, 4x MSAA, `VideoShaderDetail=3`, sun map 2048.
- Debug exe md5 `0161c766...`. That build is commit `d8f3d731f`; the commit changed no code after
  the build.
- Every run held `game_dir_lock.sh` as `tsspike`, checked with `assert-held` against this build.
- `settings.cfg` was backed up at the grant (md5 `b365a548...`) and restored byte-identical at the
  release, checked with `cmp` and md5. The game had written `VideoSettings=true` on the last run;
  that was put back.
- Arm configs:
  - row off and row on: every `ext.*` and `fix.*` row 0 except `ext.billboard_shadow`, plus
    `GameShowWeather=false`;
  - faithful: `profile=faithful`.
- Common settings: seed 12345, `BANISHED_FORCE_SPEED=10`.
- Captures, PNGs and logs are in `C:/GOG Games/Banished/rdoc_tree/` and were never committed.

**Why weather is off.** The first summer frames came out overcast with rain, which drops the sun
shadow to near nothing. Two runs at the same seed and frame came out with different weather, so it
is not deterministic. `GameShowWeather=false` (retail's own Options checkbox) keeps the sun out for
the look pairs. `[BillboardShadow]` behaves the same with weather on (frame 20000 run, `ts_on_y0`).

| Done-condition | Result |
|---|---|
| **Spike: the instructions and the cb2 size** | §2. |
| **Spike: the carrier is free** | Row off: cb2 rows 6..9 are all zero in a main-pass draw (eid 806 of `ts_off_y0_f12k`). Row on: they hold the sun basis. At canopy colour draw eid 1101, `lc` bytes 96..159 are hex-identical to the shadow-pass billboard draw eid 668's `gc` bytes 64..127 (`01 00 00 bf 8c 75 e1 be ...`). The colour pass therefore reads the caster's exact matrix. |
| **Spike: the two-angle camera-pin pair** (`town,25,0` and `town,25,90`, summer frame 12000, crops of the same oak and birch grove: `crop_{off,on}_y{0,90}.png`) | Row off shows the user's defect at both yaws. Every canopy has a bright cap above a sharp, nearly straight boundary, with the rest dark. The boundary runs one way at yaw 0 and another way at yaw 90, so it turns with the view. Row on, at both yaws: no cap and no line, only card-sized dappling lit from the sun side, with the same pattern from both headings. The ground shadows of the canopies are visibly the same off and on (the caster is retail's). |
| **Spike: acne on the top cards** | None. A 2x zoom of the top cards row on (`zoom_on_y0.png`) shows lit card patches and no texel-scale stripes or speckle. Retail's depth bias is enough: the receiver position is the caster position, so they differ only by ulp-level differences in instruction order between `Depth_vs` and the clone. |
| **Spike: trace_oracle, only the canopy colour draws change** | Row on vs row off, control off/off2, all at `town,25,0` frame 12000. **Raw:** S1 66 shared / 3 one-sided per side, S2 28 / 2 per side, S3 / S4 / S5 / S6 = 0. The one-sided keys are the rewritten vertex shader `DXBC:5740:a632eac3` in 8 draws, which pair with the two billboard pixel shaders (deciduous 2788 B, evergreen 2596 B). **Mapped** (each rewritten vertex shader replaced by the retail one, `build/ts/map_vs.py`, not in the tree): **0 findings on all six screens**. S1 69/69, S2 30/30, S5 64/64, S6 5/5, every S3 verdict field 0.0%. Nothing but the vertex shader of those 8 draws differs. |
| **Row off == faithful** | `profile=faithful` capture of this exe (frame 2000, `town,25`) vs FAA-CORE's base `faa_off_base` (control: `faa_off_base` / `faa_off_ours`): **0 findings on every screen**. S1 69/69 shared, S2 30/30, S5 64/64, S6 5/5, S3 every verdict field 0.0%. The log shows `ext.billboard_shadow=0` and 0 `[BillboardShadow]` lines. |
| `tools/build.sh run` (the gate's faithful autostart) | Clean: autostart new game, frame 10130 in game, crash log 0 bytes, `ext.billboard_shadow=0`, 0 `[BillboardShadow]` lines. |
| **Live toggle, one session** (`ts_live_toggle.error.log`) | Main menu, row off at boot, `BANISHED_AUTOMENU_BUTTON=4` opens Options; the Enhanced tab is the restored tab, and real mouse clicks drove it. Off -> On: `request on: seam set`, `Apply: item=1 active=1`, `rewrote ...`, `first rewritten bind`. On -> Off: `request off`, `stage reset (off): 72 blobs seen, 3 rewritten, 0 create failures, 3202 of 86453 vertex binds rewritten`, `stage off: copies released, seam cleared`. Off -> On again works. Crash log 0. The menu backdrop's autumn orchard shows the terminator bands with the row off (`live_off_c.png`) and an even, dappled canopy with it on (`live_on_c.png`). |
| **Device lost, row on, render thread on** (`ts_devlost_user.error.log`) | The user's own settings (render_pipe, AO, foliage_aa, max_shadow_res 8192, everything on) plus the row, `BANISHED_FORCE_DEVICE_LOST=3000`. Injected on the render thread, `recreate OK`, `stage reset (device changed): 120 blobs seen, 3 rewritten, 0 create failures, 3269 of 94873 vertex binds rewritten`, `device-lost round trip COMPLETE`, then `first rewritten bind` on the new device. Ran to frame 17353, crash log 0. |
| **render_pipe on and off** | Off: every look and oracle arm (`ext.render_pipe=0`). On: the device-lost run above (`ext.render_pipe=1`, `DrawLight` and the vertex binds both on the render thread; 3269 rewritten binds before the loss, rebinds after). |
| Unit tests | `tools/build.sh unit`: **1232 tests, 0 failures**, including the three `ext_bbs_*` tests. |
| Acceptance | `tools/acceptance/acceptance.sh`: 0 lines off target. |
| Enhanced-tab entry, settings round trip | `[EnhancedTab] Enhanced tab added: tabs=5 entries=4`, `[BillboardShadowMenu] combo=... item=0`. Apply records `ext.billboard_shadow` through `SetAndRecord` (Persistence) and the next Open reads the live value. The loose `.crs` were compiled by `tools/ext_resources.sh` (font check passed) and deployed. |
| **settings.cfg round trip** (`ts_roundtrip.error.log`) | Combo On + Apply. The game rewrote `settings.cfg` itself, and the key moved into its sorted place as `ext.billboard_shadow=1`. A fresh boot on that file logged `ext.billboard_shadow=1 class A`, `request on: seam set`, the rewrite, `first rewritten bind`, and opened the combo at `item=1`. Crash log 0. |
| **Stress pair** (perf exe `5317e685...`, `STRESS_EXT`, compact town, speed 10, 6000 frames, RTSS / Afterburner not running) | Both legs CLEAN per the runner's machine-load check: `ts_bbs_off2` (`perf/results/20261016_200741_*`) and `ts_bbs_on` (`20261016_200442_*`). Steady state, off -> on: sim 0.16 -> 0.16 ms/iter, draw CPU 0.97 -> 0.98 ms, frame 1.10 -> 1.11 ms, **911 -> 901 fps (-1.1%, min window 893 -> 864)**. This is within run-to-run noise at ~900 fps: about 45 extra vertex-shader instructions per vertex on the billboard draws only. A first off leg (`20261016_200234_ts_bbs_off`, 843 fps) ran while another agent's compiler was active (BUSY) and is not used. |

## 4. The look, for TS-LOOK

- **Brightness.** With the row on, the canopy is darker on average than retail's top cap, and
  brighter than retail's dark lower band. The light falls in patches where a card is not covered,
  toward the sun, by another card. That is the light-facing caster's own self-shadow, now
  consistent.
- **The menu orchard.** In autumn the row-on canopies read as evenly lit.
- **The summer oaks** at `town,25`: noticeably more interior shade than retail's lit cap.
- **The user's ruling** may want this, or a bias toward lighter. A future tweak could push the
  receiver a fraction of a card toward the sun: one immediate in the clone. It is not built.
- **Seasons.** The season cutoff runs identically in caster and receiver (the pixel shaders are
  untouched). Winter's bare canopies have nothing to shade.

## 5. Nothing retail entered the tree

- `git diff --stat main..HEAD`: 24 files, all source, docs, tests and one script. No blobs, no
  `.srsl` / `.rsc` copies, no captures, no textures.
- The test blobs are assembled token by token in the test.
- The only `.rsc` touched is our own `src/ext/ui/Dialog/EnhancedOptions.rsc`.
- TREE_SHADOW.md cites the kit's `BillboardMaterial.srsl` by file and function and quotes none of
  it.
- The rewriter reads the user's installed bytecode at run time and writes nothing out.

## 6. Merged with main (`3395b162a`)

main had moved: `ext.seamless_loops` (block SEAMLESS) had been merged. The conflicts were all
"both sides added a row", and both were kept:

- the registry rows and fields;
- the boot seam installs;
- the Enhanced-tab contributions;
- `EnhancedOptions.rsc`, where the tab now has six rows, "Stable tree shading" last.

Checks after the merge:

- The debug build is clean, and the unit tests are **1239 / 0**.
- Resources were recompiled and deployed. The menu shows six rows, with the row restored as On from
  settings (`entries=5`).
- An in-game run under the user's settings plus the row (render_pipe on): `rewrote ...`,
  `first rewritten bind`, frame 7495, crash log 0.

`settings.cfg` is back to md5 `b365a548...`. The game dir holds this branch's debug exe and its
loose `.crs`.
