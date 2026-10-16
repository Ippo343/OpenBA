# FAA-SHADOW -- "Max shadow resolution" (`ext.max_shadow_res`)

Block FAA-SHADOW of `docs/extensions/FOLIAGE_AA.md` §4 / §7. Branch `worktree-agent-ac4b1635b189d86a7`
(from main `40983144a`, main `dce7b2eb2` merged before the final build; commit prefixed `ext:`; **not merged**).

## What shipped

- **The row** `ext.max_shadow_res`: class A, default OFF, "Max shadow resolution" -- "Shadow Resolution
  High builds a 4096 shadow map instead of 2048." (`src/ext/registry.{h,cpp}`: one field, one row; no
  menu entry, that is FAA-MENU).
- **The three-step truth (spike S8).** The retail **Shadow Resolution** combo is High / Medium / Low =
  `Configuration::shadowScalar` 0 / 1 / 2 = 2048 / 1024 / 512 (`Width()` = raw `0x800 >> scalar`). With the
  row on, the raw size is `0x1000` **only when `shadowScalar == 0`**: High = 4096, Medium / Low stay
  1024 / 512. The Shadow *Quality* combo (`shaderDetail`) is untouched.
- **The seam** (DESIGN.md rule 5): `static const bool* Graphics::SunLight::_extMaxShadowRes`, beside
  `_extShadowFitView` (`src/engine/graphics/SunLight.h`, defined in `sunlight.cpp`). Null by default.
  `Ext::InitFromSettings` (`src/ext/registry_boot.cpp`) points it at the row's live bool once, unless the
  profile is faithful (then it stays null). The engine reads it in two places:
  - `SunMapRawSize()` (`sunlight.cpp`): the raw size `InitFields` passes to both `Create` calls (depth and
    the capability-gated colour target). Faithful cost: one null test. `Set`'s colour rebuild branch takes the
    depth's raw size, so the two attachments always match.
  - `SunLight::AddDrawItems` (`wave28_lane_partsys.cpp`): `if (_extMaxShadowRes != nullptr)
    ExtSyncMaxShadowRes();`. Faithful cost: one null test per frame. When the size the row and the combo ask
    for differs from the map's raw size, it rewrites the two descriptors' `_width` / `_height` and raises
    `Device::settingsChanged`. The next `BeginCommands` runs retail's own rebuild (`DeviceData_Resize` ->
    `ReleaseDynamic` / `RecreateDynamic`), the path a combo change already takes. So a row toggle, a combo
    change (Options writes the scalar, `ApplyChangedSettings` raises the same flag) and a device reset (which
    re-creates from the stored descriptor) all go through retail's code. `Width()`, the RenderBuffer extent,
    the PCF `texelSize` (`1/Width()`) and the texel snap need no change: raw >> scalar is right at every step.
- **The diagnostic** (kept, like the other `[` lines): one `[ShadowRes]` stderr line each time the map is
  built (keyed on the render buffer's companion and its extent; the allocator can reuse a companion's address,
  so a rebuild at an unchanged size may print nothing). It is silent under the faithful profile.

## Evidence (debug build, exe md5 `060aab49ac5658170230a5fecdefecba` for the captures and the second memory pair)

All runs: `tools/game_dir_lock.sh acquire faashadow` around every deploy and run, `assert-held` with the exe
before every log read, 0 `*.pkm` mounted, every run reached the game (`[startTown::DIAG]`), crash log 0 bytes,
killed by PID. `settings.cfg` backed up first (md5 `f7fad13a98f2841d45d86a7967bfb79f`) and restored
byte-identical (md5 checked after).

**Six steps** (the user's own settings with `VideoShadowScalar` and `ext.max_shadow_res` set per run):
```
row off, scalar 0: [ShadowRes] sun map 2048x2048 (raw 2048, VideoShadowScalar=0, ext.max_shadow_res=0, texelSize 0.00048828)
row off, scalar 1: [ShadowRes] sun map 1024x1024 (raw 2048, VideoShadowScalar=1, ext.max_shadow_res=0, texelSize 0.00097656)
row off, scalar 2: [ShadowRes] sun map 512x512 (raw 2048, VideoShadowScalar=2, ext.max_shadow_res=0, texelSize 0.00195312)
row on,  scalar 0: [ShadowRes] sun map 4096x4096 (raw 4096, VideoShadowScalar=0, ext.max_shadow_res=1, texelSize 0.00024414)
row on,  scalar 1: [ShadowRes] sun map 1024x1024 (raw 2048, VideoShadowScalar=1, ext.max_shadow_res=1, texelSize 0.00097656)
row on,  scalar 2: [ShadowRes] sun map 512x512 (raw 2048, VideoShadowScalar=2, ext.max_shadow_res=1, texelSize 0.00195312)
```
(`raw` is the stored `_width` in decimal: 2048 = retail's 0x800.)

**The gate run** `tools/build.sh run` (`/profile:faithful`): clean, in game, crash log 0, no `[ShadowRes]`
line (the seam is null).

**Device reset with the row on** (`BANISHED_FORCE_DEVICE_LOST=6000`, the user's settings, AO and the render
thread on): `[DeviceLost] device-lost round trip COMPLETE`, the game kept running (frame 13216), crash log 0.
The map is re-created from the stored 0x1000 descriptor by `RecreateDynamic`. No new `[ShadowRes]` line
followed, because the companion came back at the same address and size (see the diagnostic note above).

**RenderDoc, in-game frame 300** (`tools/foliage_aa/run_arm.sh`, `LANE=faashadow`, seed 12345,
`BANISHED_CAMERA_PIN=town,120`, every other row and fix off; the in-game capture, not the menu file).
Read with the replay API (`scratchpad texel.py`; the RenderDoc MCP's cbuffer reader returned zeros):
```
shadow_rowon.rdc   texture ResourceId::2589 4096 4096 D24S8_TYPELESS
shadow_rowon.rdc   1877 _PixelConstants_ _pc_.texelSize [0.000244140625, ...]   (= 1/4096; terrain, main pass)
shadow_rowon.rdc   1193 _PixelConstants_ _pc_.texelSize [0.000244140625, ...]
shadow_rowon.rdc   1877 srv ResourceId::2589 4096 4096 D24S8_TYPELESS
shadow_rowoff.rdc  1877 _PixelConstants_ _pc_.texelSize [0.00048828125, ...]    (= 1/2048)
shadow_rowoff.rdc  1877 srv ResourceId::2589 2048 2048 D24S8_TYPELESS
```
The sun map is the only 4096 texture in the frame. **No colour target is built on DX11:** `DeviceData`
slot 6 ("supports a colour shadow target") is `xor al,al; ret` (`devicedata_stubs.cpp`), so the colour
`Create` sites are dead on this backend. They follow the same size where a backend takes them.

**Row OFF == faithful (`tools/trace_oracle`).** Four captures from one exe: `faith1` and `faith2`
(`/profile:faithful`, the control pair), `rowoff` (per-entry profile, every row and fix 0, so the seam is
installed and points at false) and `rowon` (the same, with `ext.max_shadow_res=1`). Selftest on `rowoff`:
19/19 positive controls caught, 9/9 negative controls clean.
- `rowoff` vs `faith1` (control `faith1`/`faith2`): **every screen 0 findings.** S1 70/70 shared (0
  only-either side), S2 30/30, S5 66/66, S6 5/5, S3 floored 0, S4 0. rc 0.
- `rowon` vs `faith1`: S1 84, S2 14, S5 2 (1 texture on each side: the sun map's 4096 desc against the
  2048 one), S3 0, S4 0, S6 0. The configurations that bind the sun map change identity, and nothing else
  shows up.

## Memory delta (row on vs off, scalar 0, the user's settings, ~20 s in game)

GPU dedicated usage (`\GPU Process Memory(pid_*)\Dedicated Usage`, sampled while running):

| run | row off | row on |
|---|---|---|
| pair 1 | 415.9 MB | 477.1 MB |
| pair 2 | 424.6 MB | 477.4 MB |
| pair 3 | 415.8 MB | 477.2 MB |

**+58 MB on average** (the D24S8 map is 16 -> 64 MB = +48 MB by the format; the rest is driver placement).
The off steps track the format too: scalar 0 -> 1 -> 2 measured 415.9 -> 402.7 -> 398.0 MB (predicted -12,
then -3). The process working set / private bytes do not move with the row (the texture is in video memory;
private bytes ranged 744..855 MB in either arm). Cost besides memory: 4x the shadow-pass fill at High; not
measured (no stress run in this block, as instructed).

## Not exercised

- **A live toggle and a live combo change** were not driven through the Options dialog (no unattended
  route sets a combo or an Extensions row). Both reach the map through `Device::settingsChanged` ->
  `DeviceData_Resize` -> `RecreateDynamic`, which the boot rebuilds (AO's readable-depth request) and the
  forced device reset did exercise with the 4096 map in place. FAA-MENU's by-hand check should flip the row
  and the combo once and read the `[ShadowRes]` line.
- With `ext.render_pipe` on, a toggle that lands while the render thread executes the previous frame can draw
  that one frame with the new `Width()` (the texel constant) against the old texture: the same one-frame
  window retail has when the scalar changes under a drawn frame.

## Content

`git diff --stat 40983144a HEAD` was reviewed: C++ sources (`src/engine/graphics/{SunLight.h,sunlight.cpp,
wave28_lane_partsys.cpp}`, `src/ext/{registry.h,registry.cpp,registry_boot.cpp}`) and this markdown plus
FOLIAGE_AA.md. **Nothing retail entered the tree:** no blob, no DXBC, no `.srsl` / `.rsc` copy, no capture,
no texture. The captures stay in `C:/GOG Games/Banished/rdoc_foliage/shadow_*.rdc`. The run scripts and
traces are in the session scratchpad, outside the tree.
