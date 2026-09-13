# Retail bugs

Behaviours that are faithful to the shipped game and therefore left alone by the port.
The port's job is to match retail; these get revisited only after the finalized build
(DLL-absent, all bodies ours) is the baseline. Until then a "fix" here would be a
deviation that the cutover gate and the A/B runs would have to explain.

Each entry: what a player sees, why retail does it (with addresses), how it was proven
to be retail and not the port, and the faithful lever if we ever change it.

## 1. Status icons slide in from the map origin for one sim tick

**Symptom.** A building status icon ("work stopped", "no workers", ...) appears at the
world origin (the map centre) and slides to its building. Reported 2026-09-13 as
"icons spawn at the town center then move to the field".

**Retail mechanism.**
- `StatusIconComponent` slot 11 (`0x14012bf40`) calls `Scene::AddObject` FIRST, then
  slot +0x40 (`StatusIconComponent_OnRemove`, `0x14012bbd0`) which positions the
  embedded `Graphics::Geometry` through `Geometry::SetTransform` (`0x1800155a0`).
- `SetTransform` rolls the old transform (identity, from the ctor `0x1800a4270`) into
  `_prevTransform` and takes the new one.
- `Geometry::Clean` (`0x1800a4420`) is two-phase: the tick's `Scene::Clean` only turns
  dirty bit 0 into "rolled-pending" bit 3; the NEXT tick's Clean copies current into
  previous. So for one full tick previous = origin, current = building.
- The icon's Draw (`0x1400a9c10`) renders
  `InterpolatedState<Transform>::Lerp(alpha)` with the frame's interpolation alpha, so
  every frame drawn inside that tick lands somewhere on the origin-to-building line.

**Proof it is retail, not the port.** Draw trace on 2026-09-13, DLL present vs absent:
identical pattern in both (prev=(0,0,0), alpha sweeping 0..1, flags=0x18, then rolled),
60 sim updates/s in both. Every body in the chain was read against the Ghidra decompile
(SetTransform, Clean, ctor, slot 11, OnRemove, Draw) and the ports match.

**Why it is visible in our build and barely in retail.** In-game rendering is uncapped in
the standalone (the registry has vsync=0; `standalone/frame_limiter.cpp` only runs in
the menu loop), so ~800-1000 draws/s spread ~15 frames across the one 1/60 s tick. With
vsync at 60 Hz it is a single frame at one intermediate position.

**Faithful lever, if it ever needs to go away.** The frame rate (vsync / an in-game
frame cap), not Clean or SetTransform. Alternatively position before AddObject in the
component, which is an exe-side deviation and must be documented as such.
