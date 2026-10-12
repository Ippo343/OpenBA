# EYECAM hand-in (2026-10-11)

The first extension: `ext.eye_camera`, class A (no serialized state, no sim effect), default OFF,
display name "Eye-level camera", description "Follow a citizen at eye level, looking where they
look." Spec: `INVENTORY.md` §5.1. Static gate only: the game was NOT run by this block
(coordinator's rule). Everything below about how it looks is unverified until the by-hand run (§7).

## 1. What exists

| File | What |
|---|---|
| `src/ext/eye_camera/eye_math.h` / `.cpp` | The pure math, standard C++ only: the conventions, the constants (§4), `ModelYawFromQuaternion`, `Forward`, `ComputePose` (feet + heading + eye height + distance -> eye and look-at), `ZoomDistance`, `BlendFactor`, `Smooth` + `Snap` / `Step`. |
| `src/ext/eye_camera/eye_camera.h` / `.cpp` | The engine glue: `EyeCamera::Active()`, `Apply(pc)` (Update step 6), `ApplyFrame(pc, alpha, fovY, far)` (Draw step 4), `Zoom(zoomDir)` (camera input). The weak reference, the follow detection, the model sample, the terrain clamp. |
| `src/ext/registry.h` / `.cpp` | `Ext::Extensions::eye_camera` and the table row (Tier::Extension, Class::A, default false, formatVersion 0). |
| `tests/unit/ext_eye_camera_test.cpp` | 6 tests of the pure math (§6). `tests/unit/sources.cmake` links `eye_math.cpp`. |

No hook vtable, no function pointer, no getenv, no `BANISHED_*` flag, no door, no new cast.

## 2. The three seams (the only edits outside `src/ext`)

Each is `if (Ext::Ext().eye_camera && EyeCamera::Active()) <ours> else <retail's lines, unchanged>`,
with an `// EXT SEAM` comment; the toggle is read once per seam call, never in a loop.

| Site | Lines | Ours | Retail's arm |
|---|---|---|---|
| `src/game/Game/gameupdateloop.cpp`, `GameUpdateLoop::Update` step 6 (once per update tick) | 977-989 | `EyeCamera::Apply(*playerCtrl)` | `CalcTransform` -> `AsTransform3` -> `camera.SetCameraToWorld` |
| `src/game/Game/gameupdateloop.cpp`, `GameUpdateLoop::Draw` step 4 (once per frame) | 1335-1340 | `EyeCamera::ApplyFrame(*m_playerController, alpha, 40, 512)` | `UpdateCamera(40, 2.0, 512)` (0x140098660) |
| `src/game/Game/camera_input.cpp`, `PlayerToolManager::UpdateCamera` (R17's file) | 172-176 | `zoomDir = EyeCamera::Zoom(zoomDir)` (moves the eye's distance, returns 0) | nothing: retail's zoom path below runs with zoomDir as computed |

Plus one include line per file. **The Draw seam does more than the near plane** INVENTORY §5.1
named: it also puts the eye transform on the camera, at the renderer's `alpha`. Reason: step 6
runs once per update tick, the renderer draws the citizen interpolated between its previous and
current transform at `alpha`; a camera set only at step 6 would sit at the tick's transform while
the citizen is drawn behind it, a visible judder at a 0.1 near plane, at every game speed. Step 6
still sets the eye too (the last frame's), so the tick's later readers (picking, the audio
listener) see the eye camera rather than retail's for one tick.

## 3. How the follow engages and disengages

`EyeCamera::Active()` is the whole test (eye_camera.cpp):
1. Retail's follow flag, `PlayerToolManager +0x38` (FollowDialog::OnOpened sets it, OnClosed
   clears it; nothing else writes it). Off -> disengage, retail's camera.
2. The frontmost open `FollowDialog` in `UserInterface::_dialogs` (walked tail -> head, skipping
   anything in `_removedDialogs`). The frontmost one because the UI updates back to front, so its
   `OnUpdate` writes retail's target last: we follow the citizen retail's camera follows.
3. On a NEW dialog (weak-ref identity differs) we latch it and its citizen's entity
   (`dialog->citizen->_entity`) as weak references; the dialog's raw `citizen` pointer is read only
   at that moment. The smoothing is reset and the first pose SNAPS to the citizen.
4. The latched entity must still resolve (below) and not be marked for removal
   (`_entityFlags` bit 10). Otherwise `Active()` is false: retail's follow (its FollowDialog keeps
   writing PlayerController's target).

Engaged = the follow dialog is open and the extension is on. Disengaged by everything that ends
retail's follow today: the dialog's close button (id 100), Escape (key 8, FollowDialog::OnUpdate),
the citizen panel closing (CitizenUI::OnClosed closes the follow dialog), or the toggle switched off
in the Extensions dialog (live, class A). On disengage the camera is retail's on the very next
step: `PlayerController`'s target / yaw / pitch / distance and `PlayerToolManager`'s +0x3C / +0x40
were never written by this code, so retail's camera is exactly where retail's follow left it.
Retail's own writes continue meanwhile (FollowDialog::OnUpdate moves the target to the citizen,
the camera input still smooths pitch / distance toward their unchanged targets, Q/E still turn
retail's yaw): all of that is retail's state, invisible while the eye owns the camera.

**Must-not kept:** no write of `PlayerController::transform / target / yaw / pitch / distance`
(Serialize 0x1400988c0) nor of `PlayerToolManager` +0x3C / +0x40 (Serialize 0x140102920). The +0x00
transform IS written, by retail's own `CalcTransform()`, which `Apply` still calls at step 6 so
the serialized +0x00 stays exactly what retail would store. The only other write is the camera
member (+0x34, not serialized) through `SetCameraToWorld` and `UpdateCamera`.

**The weak reference** (R1): `{pointer, InternalFactory slot index, generation}` taken from the
object; it resolves only while `InternalFactory::_objects[index]` still holds that pointer with
that generation (`~Internal` swap-removes the slot, so a destroyed object is never found there),
and the pointer is dereferenced only after the slot names it. Tested every step (every
`Active()` call, three per frame).

## 4. Constants (`eye_math.h`)

| Constant | Value | Why |
|---|---|---|
| eye height | 0.88 units | The shipped male citizen mesh (`data0.pkg`, `Models\Citizen\CitizenMaleMesh_resource.crs`, bounding box read from the compiled resource: z 0.002 .. 0.946, x +-0.45 T-pose, y -0.072 .. 0.089) is 0.946 units tall, so 1 unit is ~1.85 m and a 1.6 m eye is ~0.87; eyes at 0.93 of the height = 0.88. A constant, not the live bounds (an animated bound moves with the arms); children use it too (see §7). |
| face forward | 0.10 | the first-person eye sits just ahead of the face (the mesh's front is ~0.09) |
| distance | 0 .. 3.0, default 0.9, step 0.3 | 0 = first person; 3.0 units ~5.5 m over the shoulder; the wheel / ZoomIn / ZoomOut keys move it by sign (zoom in = closer). Kept across follows for the session. |
| rise | 0.30 per unit backed off | the head sits ~17 deg under the view centre at any distance |
| look ahead | 6.0 units, level at eye height | |
| near plane | 0.1 (retail 2.0) | far stays 512, fov stays 40 |
| smoothing | position tau 0.06 s, heading tau 0.25 s, distance tau 0.10 s | exponential, `1 - exp(-dt/tau)` on the real frame time (steady_clock), so frame-rate independent (unit-tested); the heading turns the short way round |
| snap | a jump > 4 units, or > 0.5 s without a frame | teleport / reload / the toggle off and on |
| terrain clearance | 0.15 above the heightmap | a backed-off eye over a slope (`Map::GetTerrainPos` with flag 1: the heightmap only, clamped to the map) |

The heading: the model's rotation is the move tick's pure Z rotation
`(0, 0, -sin(Y/2), cos(Y/2))` with `Y = heading + CharacterDescription::_f24`
(citizenai_executor.cpp, CharacterComponent_MoveTick_1094c0); `ModelYawFromQuaternion` inverts it
and `_f24` is subtracted, giving the heading whose `Vector3::FromYawPitch(H, 0)` is the walking
direction. The position and the rotation are `ModelComponent::meshInstance._transform` (the
Geometry's InterpolatedState) lerped prev -> current at the renderer's `alpha`: the model's
interpolated node-to-world for the root, not the sim cell and not `ModelComponent::transform`
(the per-tick gameplay copy).

## 5. INVENTORY §5.1's risks and what I did

| Risk | What I did |
|---|---|
| Near plane 2.0 clips at eye height | 0.1 while engaged (the Draw seam calls retail's `UpdateCamera(40, 0.1, 512)`); retail's 2.0 otherwise. Depth precision: 512/0.1 = 5120, worth a look at distant terrain / water edges (§7). |
| Terrain chunk / status-icon culling, `statusIconDist`, assume a top-down camera | Not changed. Culling uses the camera's own frustum, which is now the eye's: things behind the eye are culled, which is right. Status icons are placed by their own distance setting from the camera; at eye level more icons may be in range and big in view (§7). PerfX GEOM_CULL / ANIM_CULL (perf build only) cull off-frustum citizens: the followed citizen is in front of the eye or contains it, so it is not culled; the 6-unit margin covers its first frame. |
| `CalcWindFactor` / the audio scene use \|camera - target\| | Not changed (audio only, class A stays class A). The listener is (target.xy, camera z) and the wind factor is \|camera - target\|: at eye level both are near the citizen, so ambient wind is quieter and nearby 3D sounds are louder. Arguably the right sound for the view; a by-hand listen (§7). |
| The followed citizen enters a building (model hidden) | Hidden = `ModelComponent::modelId < 0` (out of the scene). `ApplyFrame` skips the sample and keeps the last eye transform; when the citizen reappears the smoothing resumes (a walk out of a door is under the 4-unit snap, so it glides; farther, it snaps). Hidden at the moment of engaging: retail's follow until the model shows. |
| The followed citizen is removed (R1) | Weak reference tested at every `Active()`; dead or marked for removal -> `Active()` false -> retail's follow, which then ends the usual way (CitizenUI closes the follow dialog). The dialog's raw `citizen` pointer is never read after the latch. |
| Step 6 runs per sim step; high game speed | Step 6 runs once per update tick (outside the speed loop), and the eye is rebuilt per FRAME at the renderer's alpha from the Geometry's interpolated transform, so it moves exactly as the drawn citizen does at every speed. (PerfX GEOMXF_LASTONLY, perf build: the geometry push happens on the last speed iteration only -- the same transform the renderer draws.) |
| R17 (camera_input.cpp's pan basis from yaw) | Not fixed (the brief). The hook does NOT depend on it: it reads no basis and replaces only the zoom direction; while engaged, WASD still pans retail's target, which FollowDialog::OnUpdate overwrites each frame (retail's follow). |

## 6. Results

- `tools/build.sh build debug --jobs 2`: exit 0. Warnings: the clean baseline build of the base
  (a6d61e20d) has 109; the incremental build after this block's edits printed 1, in
  `src/game/Dialog/residenceui.cpp:243` (unused `endRow`, pre-existing, recompiled because
  registry.h changed). None in a new or touched file: 0 new warnings.
- `tools/build.sh unit debug --jobs 2`: `1019 tests, 0 failures` (6 new `ext_eye_camera_*`:
  wrap_degrees, yaw_from_model_rotation (against `Transform::FromYawPitchOffset`, the move tick's
  quaternion, q / -q, and a lerped transform), forward_matches_engine_heading (against
  `Vector3::FromYawPitch` / `Heading`), pose, zoom, smoothing (snap, frame-rate independence, the
  short way round +-180, the teleport snap)).
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target`.
- `python tools/castmetric.py src | tail -1`: `# castmetric total: 2` (no cast added; the one
  `static_cast<FollowDialog*>` is a checked hierarchy downcast after `IsA`, not counted).

## 7. What the coordinator must look at by hand (`tools/build.sh run-menu`, then a new game)

1. Extensions dialog: a fourth row "Eye-level camera", class A, "live", off by default; on, OK.
   `ext.eye_camera=1` in settings.cfg. The gate (`tools/build.sh run`, `/profile:faithful`) logs
   `ext.eye_camera=0 class A`.
2. Click a citizen, press Follow: the view drops to the citizen's eyes slightly behind the head
   (distance 0.9), looking where they walk; no snap when they turn (heading smoothed), no judder
   while they walk at speed 1, 2, 3 (alpha interpolation).
3. Mouse wheel / zoom keys: toward first person (0) and back out to ~3 units over the shoulder;
   retail's zoom must NOT have changed when the follow ends.
4. The eye height looks like a person's (0.88 units): if it is at the knees or above a roof, the
   unit scale guess is wrong (the constant is in eye_math.h).
5. First person (distance 0): the citizen's own head / hat / carried tool -- does any of it fill the
   view? Near plane 0.1: any clipping of the ground at the feet, z-fighting on distant terrain,
   water, roads.
6. Close the follow three ways: the dialog's close button, Escape, closing the citizen panel.
   Each must give back retail's camera at retail's pitch / distance over the citizen (the same
   view the faithful follow ends with), with no jump in zoom.
7. The citizen enters a house / workplace: the view holds at the door; when they come out it
   glides back to the eyes.
8. The followed citizen dies (or is removed some other way) while followed: retail's follow
   takes over, no crash, no frozen camera.
9. Toggle the extension off in the Extensions dialog (pause menu) while following: retail's follow
   at once; on again: the eye snaps back.
10. Save while following with the extension on, load in the faithful profile: the camera
    loads where retail's would (target at the citizen, retail's pitch / distance). A resave diff
    of that save against one made with the extension off at the same moment shows only the
    sim's own drift, never the camera block.
11. Children: the eye stays at the adult 0.88 (children are scaled meshes); judge whether that
    reads wrong.
12. Status icons / particles / the wind at eye level: anything huge, missing or loud.
13. Two citizen panels both following: the frontmost follow dialog wins (as retail's target).

## 8. Stress comparison (the coordinator fills)

| Profile | simIters/frame or fps | crash / hang log | exit |
|---|---|---|---|
| faithful (`/profile:faithful`, label eyecam_faithful) | sim 0.17 ms/iter, update p95 2.17 ms, 140 fps | none | 0 |
| everything on (`/profile:all`, label eyecam_all; nothing followed) | sim 0.17 ms/iter, update p95 2.33 ms, 140 fps | none | 0 |

The stress town follows no citizen, so with the extension on and nothing followed the only cost
is `Ext::Ext().eye_camera` plus the follow-flag test, three times a frame.
