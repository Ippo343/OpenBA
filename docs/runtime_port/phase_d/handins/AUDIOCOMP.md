# AUDIOCOMP hand-in (Phase D.4, branch `d4-audiocomp`, base `e9f7a978f`)

The retail helper `Entity_FindAudioComponent` (Application-x64.exe 0x140002280) and its five
`PORT GAP` arms, plus `AmbientEmitterComponent::OnRemove`; and the trace of the crowd ambience
(`Audio/Effects/CrowdEffect.rsc`) the user could not hear.

**Static gate only.** The game was never run from this worktree. Nothing in this block has been heard;
the listen checklist at the end is the coordinator's.

## The finder

0x140002280 was read in full (decompile and disassembly). It is the `FindComponent<T>` template
instance for the AmbientEmitterComponent type cell 0x1401c3e60: walk `+0x30[i]`, CheckPtr
(IAT 0x140152ab0), `Type()` (vtable +0x10) against the cell, `IsDerivedFrom` (IAT 0x140152a98),
the InternalBasePtr ctor (IAT 0x140152a90) into the return slot. The same shape as 0x140002ed0
and 0x140002f90, which the tree already spells `FindComponent<T>()`. So there is no new body:
the port is `_entity->FindComponent<AmbientEmitterComponent>()` at every caller, recorded in
`src/game/Entity/ComponentEntity.h` and `src/game/Component/exe_bodies.h` (the B4 deletion note).

## Commits

| commit | title | what it makes audible |
|---|---|---|
| `45f4d04a0` | port: Entity_FindAudioComponent 0x140002280 is FindComponent<AmbientEmitterComponent> | nothing (two header comments) |
| `ed2d2ed22` | behaviour: the residence crowd loop (RemoveResident 0x14012a550 / AddResident 0x14012a710 audio arms) | the crowd murmur at every house with at least one resident (row 1290); it stops when the last one leaves |
| `6cf370045` | behaviour: the building fire loop starts and stops with the fire (Ignite 0x140049610 / Extinguish 0x1400494c0) | the FireEffect crackle at a burning building, from Ignite to Extinguish |
| `7fbd73120` | behaviour: the tornado loop follows the funnel and takes its opacity as volume (Tick 0x140057cd0) | the already-playing tornado loop moves with the funnel and fades in / out with the particle |
| `a1505e8f8` | behaviour: AmbientEmitterComponent::OnRemove stops every emitter slot (0x140042b90 -> 0x140042d70) | silence when a building with a live emitter is marked for removal |
| `(this commit)` | docs: rows 1288, 1290; this hand-in | docs only |

## The arms, consumer by consumer (read before wiring)

| consumer | retail | what retail does with the component | reachable in the standalone |
|---|---|---|---|
| `ResidenceComponent::AddResident` 0x14012a710 | 0x14012a764 find; 0x14012a7a4 `Vector3::operator==` (import 0x140152a70, the second PORT GAP in the block, Runtime 0x1800150a0 inline in Math.h); 0x14012a898 `PlaySlot(1)` | when the house is empty: compare the ModelComponent translation (+0x158) with the cached position, MoveEmitter every live slot and cache it when they differ, PlaySlot(1), slot [1] volume 1.0 (SetEmitterVolume on a live handle) | yes: `CitizenComponent::SetHome` (citizen_assignworkplace.cpp) calls it for every move-in |
| `ResidenceComponent::RemoveResident` 0x14012a550 | 0x14012a6ba find | when the house is now empty: RemoveEmitter(slot [1]), StopEffect3D when the effect has no emitter left, handle = 0 | yes: the same caller, and eviction on OnRemove |
| `FireComponent::Ignite` 0x140049610 | 0x140049985-0x1400499b5 | find emitter + ModelComponent, `MoveTo(translation)` (0x140042960), `PlaySlot(FireDescription+0x2c)` (0x140042c60) | yes (the fire route of `disaster_route.sh`) |
| `FireComponent::Extinguish` 0x1400494c0 | 0x1400494e9-0x14004953c | RemoveEmitter of slot [+0x2c], StopEffect3D when none left, handle = 0 | yes |
| `TornadoComponent::Tick` 0x140057cd0 | 0x140057f09 and 0x140057f52 (two finds) | `MoveTo(position)`; then slot [0]: volume = emitters[0].instance opacity, SetEmitterVolume when it changes on a live handle | yes (the tornado route) |
| `AmbientEmitterComponent::OnRemove` 0x140042b90 -> 0x140042d70 | slot 7, broadcast by `MarkForRemoval` 0x1400c2ea0 to components with flag bit 1 (the emitter ctor sets 0x43) | every slot: RemoveEmitter at the cached position, StopEffect3D when none left, handle = 0 | yes (any demolition / burn-down) |

Every retail read of the finder's result is unguarded and so is the tree's now. The templates
justify it (ToolKit/resource/Template): all 20 templates that carry a FireDescription or a
ResidenceDescription carry an `ambientemitter` (entry 0 FireEffect, `_addOnCreate = false`; the
three residences add CrowdEffect as entry 1); Tornado.rsc carries TornadoEffect, added on create.
`OnAdded` 0x140042ba0 (ported) sizes the slot array from the description, which is what the B4
deletion note feared was missing. The residence arms keep the file's pre-existing `AudioScene()`
null test (documented in its helper); `OnRemove` reads `GameUpdateLoop::_singleton->m_audioScene`
as retail reads `[0x1401c3dd8]+0x78`.

Not touched: the DebugUI arm at `src/game/Dialog/debugui.cpp:271` (not in this block's files; its
audio group is a separate construction, not just the finder).

## The crowd ambience (row 1290)

`CrowdEffect.rsc` is referenced by exactly three templates: `WoodHouse.rsc`, `StoneHouse.rsc`,
`Hostel.rsc`, as entry [1] of their `ambientemitter` with `_addOnCreate = false`. TownHall, Market
and Tavern list FireEffect alone. Retail starts the loop in `AddResident` when the first resident
moves in and stops it in `RemoveResident` when the last leaves: the "townsfolk" the user expected
at the town hall are the murmur of occupied houses. Our port ran neither arm (the finder gap), so no
house ever started it. Fixed by the residence commit; one `Crowd.wav` loop (radius 10, inner 5),
one emitter per occupied house on the shared effect id.

## Gate

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1000 / 0 (main at base e9f7a978f) |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new (debug), 0 new (perf) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK, exit 0 |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

## LISTEN owed (the coordinator, by hand)

1. **Fire.** Let a building catch fire (`disaster_route.sh` fire, or wait). Expect the crackle AT the
   building, from the flames appearing to the fire going out, then silence. A ruin that keeps
   crackling, or a crackle at the map origin, is a finding.
2. **Tornado.** The loop should move with the funnel and fade with it (it was fixed at the origin
   before). Row 1276's silent loop tail is unchanged.
3. **Crowd.** Build a house and let a family move in: a murmur within about 10 units of the house,
   none at the town hall. Evict / lose the last resident: silence. Several houses share one effect,
   so the murmur should not stack in volume per house beyond the 3D mix.
4. **Removal.** Demolish an occupied house or a burning building: its loop stops at the mark.
5. `grep 'PlayPlatform ATTENDANCE' standalone_debug.log`: voicesCreated should still equal placed.

## Left

- The `SetSlot` comment in `ambientemittercomponent.cpp` ("DEVIATION FROM RETAIL ... OnAdded is a
  no-op stub") is stale since OnAdded was ported; the on-demand PlaySlot it describes is still in the
  code. Not changed here (AmbientEnvironmentComponent's path, outside this block).
- `debugui.cpp:271` audio arm (above).
