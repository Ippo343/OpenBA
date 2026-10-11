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
the standalone (the registry has vsync=0; `src/app/frame_limiter.cpp` only runs in
the menu loop), so ~800-1000 draws/s spread ~15 frames across the one 1/60 s tick. With
vsync at 60 Hz it is a single frame at one intermediate position.

**Faithful lever, if it ever needs to go away.** The frame rate (vsync / an in-game
frame cap), not Clean or SetTransform. Alternatively position before AddObject in the
component, which is an exe-side deviation and must be documented as such.

## 2. The residence panel's "go to" button reads past a StorageComponent

**Symptom.** None reliably visible. Clicking a resident's "go to" button on a house panel
walks heap memory past the end of an object; on a lucky heap it centres on a resident,
otherwise it centres on nothing, on the wrong citizen, or faults.

**Retail mechanism.** `ResidenceUI::GoToCitizen` (`0x1400b56a0`, reached from slot 9
`0x1400b5843` on element event 0x12, id 300 + n) finds the house's component through
`0x140006270`, the StorageComponent finder: it compares `Type()` with the cell
`0x1401c7e78` and narrows with IsDerivedFrom. That cell's only writer is
`StorageComponent_RegisterType` (`0x14014fb10`, `LEA RCX,[0x1401c7e78]` at `0x14014fb35`,
name "StorageComponent", factory `0x140038b50` = `MemAlloc(0x40, 8)`). The body then reads
`[comp+0x58]` as a family count and `[comp+0x50]` as a 0x10-stride family array: both lie
past the 0x40-byte object. The rows it means to walk are ResidenceComponent's (cell
`0x1401c7cf0`, factory `MemAlloc(0x60, 8)`, families at +0x50 / +0x58), which
`OnUpdateLayout` (`0x1400b51f0`) walks correctly through the Residence finder.

**Proof it is retail.** Read in Ghidra 2026-10-09 (B6 Block 0, then block UI): the finder's
cell, its RegisterType and the factory size above.

**The port deviates (B6 ruling, known_port_issues row 452).** `src/game/Dialog/residenceui.cpp`
reads the ResidenceComponent's families by name, with a `STANDALONE DEVIATION` comment.
The faithful lever is the StorageComponent finder at that site.

## 3. A tavern visit can count the wrong good as consumed

**Symptom.** None on screen directly. When a citizen's leisure visit spends the last unit
of a good at a building that needs goods (the tavern's ale), the town's "consumed this
year" counter goes up for the category of the NEXT good stored there, not for the good
that was spent. The stock counts themselves are right. Only the consumption statistics
(town stock row +0x0C) drift.

**Retail mechanism.** `HappinessComponent::OnMessage` (`0x14004a780`, message 0 with the
description's `_needsGoods` +0x25 set) walks the building's StorageComponent items
(`0x140006270`), and for the first item whose RawMaterialDescription flags (+0x5c) meet
the description's +0x28 mask with at least one unreserved unit:
- keeps `R14` = the address of that item's row in the storage array (`0x14004a824` /
  `0x14004a82a`, row stride 0x10, the material pointer at +0x00);
- calls `StorageComponent::SubtractItem(storage, material, 1)` (`0x14012d150`,
  `0x14004a8da`), which compacts the array when the row reaches zero;
- then re-reads the material through the same row address (`MOV RDX,[R14]` at
  `0x14004a8e6`) for the category lookup (`0x140088f90`) and bumps that category's
  counter (`INC [row + 0x0C]`, `0x14004a907`).

When the spent unit was the row's last, the row now holds the next item (or the stale
tail of the array), so the counter that moves is that item's category.

**Proof it is retail.** Read in Ghidra on 2026-10-11 by block CAI of the D.4 PORT GAP hunt
(decompile and disassembly), and re-read by block RESIDUE: the material is loaded from
`[R14]` after the call, not from a register saved before it.

**The port keeps it.** `HappinessComponent::OnMessage` (`src/game/Component/happinesscomponent.cpp`, D.4 CAI)
re-reads the row after `SubtractItem`, as retail does. The faithful lever is to load the
material before the call.
