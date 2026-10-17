# FIXPILOT hand-in (2026-10-11)

The three recorded retail bugs (`docs/retail_bugs.md` §1-§3) as tier-1 fixes
(`docs/extensions/DESIGN.md` §2, hook rule 1): one branch at the lever, retail's path in the
else arm, `Fix().<id>` read once at the top of the function. All three default on.

Commits on top of `d5bc23b57`:

| Commit | Title |
|---|---|
| `c675d0d81` | `ext: FIXPILOT stub registry (superseded by EXTSCAFFOLD at merge)` -- **drop at merge** |
| `b86c42fe8` | `fix: icon_slide -- position the status icon before Scene::AddObject` |
| `e487d6d00` | `fix: residence_goto -- the go-to button reads the ResidenceComponent's families` |
| `916b43f38` | `fix: tavern_counter -- count the category of the good actually spent` |
| (this) | `docs: FIXPILOT hand-in; retail_bugs.md Fix lines` |

**The stub.** `src/ext/registry.{h,cpp}`: `struct FixToggles { icon_slide, residence_goto,
tavern_counter }`, all `true`, and `const FixToggles& Fix()`. CMakeLists globs `src/ext/*.cpp`
(recursive) into both `OpenBA` targets and appends `src/` (last) to `BANISHED_INCLUDES` so
sites write `#include "ext/registry.h"`. EXTSCAFFOLD's registry must provide the same names
(`Fix().icon_slide`, `.residence_goto`, `.tavern_counter`) and the same include path; if it
does not append `src/` to the include list, the three sites and `src/ext/fixes/tavern_counter.h`
need it (the unit test includes `ext/fixes/tavern_counter.h` through `BANISHED_INCLUDES` too).

## fix.icon_slide (retail_bugs §1)

- **Site.** `src/game/Component/statusiconcomponent.cpp`: `ShowAfterLoad` (slot 11,
  retail `0x14012bf40`) and `SetFlag` (retail `0x14012c3f0`).
- **Lever quoted.** "position before AddObject in the component, which is an exe-side
  deviation and must be documented as such."
- **Faithful arm.** Retail's order: `Scene::AddObject`, then Tick (slot 8, `0x14012bbd0`)
  positions through `Geometry::SetTransform` (`0x1800155a0`); one tick of previous = origin.
- **Fixed arm.** `StatusIcon_PlaceIcon` (Tick's positioning tail alone) runs first, then the
  retail sequence unchanged. `AddObject` sees dirty bits, its `Clean` turns bit 0 into the
  rolled bit 3; the tick's `Scene::Clean` (`0x1801023f0`, which cleans every live node with
  dirty bits) copies current into previous before a frame draws it. The retail Tick that
  follows sets the same transform, which `SetTransform` treats as no change.
- **Refactor carried with it.** Tick's tail (description read, MultiObject lookup,
  model-bounds / map-cell position, `SetTransform`) moved unchanged into
  `StatusIcon_SetIconTransform(c, entity, model)`; Tick calls it after its PerfX shortcuts. The
  one ordering change: the description read (`GetStatusIconDescription`, a pure read) now
  happens after the PerfX early returns instead of before.
- **Test.** None in unit (needs the scene and a component). Owed to the gate: a new game with
  `fix.icon_slide` on should show a "no workers" icon appearing at its building, no slide from
  the map centre; faithful profile unchanged.
- **Doubt (scope).** retail_bugs.md names slot 11 only, but slot 11 is the POST-LOAD re-show
  (EntityList `0x1400c4730`). Icons that appear in a running game ("work stopped", "no
  workers") go through `SetFlag` `0x14012c3f0`, which has exactly slot 11's shape (AddObject,
  then `CALL [RAX+0x40]`; re-read in Ghidra 2026-10-11). Fixing slot 11 alone would leave the
  reported symptom in place, so the same one-line branch is at `SetFlag` too. If the coordinator
  wants slot 11 strictly, drop the two `SetFlag` hunks. Not covered: Tick's own add branch
  (the icon setting switched on in options adds and positions in one call, same order); it is
  per-tick code and was left alone (hook rule 4 / "nothing else moved").

## fix.residence_goto (retail_bugs §2, known_port_issues row 452)

- **Site.** `src/game/Dialog/residenceui.cpp` `ResidenceUI::GoToCitizen`, retail `0x1400b56a0`.
- **Lever quoted.** "The faithful lever is the StorageComponent finder at that site."
- **Re-read in Ghidra** (Application-x64.exe, 2026-10-11): `Entity_FindComponent_Storage`
  (`0x140006270`) on the dialog's entity, `[comp+0x58]` family count, `[comp+0x50]` family array
  stride 0x10 with each family's count at +8, the resident `[[fam+0] + r*8]`, its +0x10 entity
  and DialogController +0x18 handed to `CenterOnEntity` `0x140103040`.
- **Faithful arm.** `entity->FindComponent<StorageComponent>()` reinterpreted as the
  ResidenceComponent layout (`residentSlots` at +0x50 / +0x58, static_asserted in
  `ResidenceComponent.h`), then the same walk: the raw read past the 0x40-byte object, unguarded.
- **Fixed arm.** `entity->FindComponent<ResidenceComponent>()`: the B6 STANDALONE DEVIATION,
  now the fix. The walk below the one-line choice is shared.
- **Test.** None in unit (needs a live entity, UI and the tool manager).
- **Doubt.** The faithful arm will not land on the same bytes as retail: what follows a 0x40
  block is our allocator's neighbour / header, not retail's MemAlloc heap, and under a redzone
  allocator diagnostic it reads the redzone. It is not guarded (a fault there is retail's
  behaviour); expect a centre-on-nothing or a fault with `fix.residence_goto=0`. The faithful
  gate does not click this button, so it should not see it.

## fix.tavern_counter (retail_bugs §3)

- **Site.** `src/game/Component/happinesscomponent.cpp` `HappinessComponent::OnMessage`, retail
  `0x14004a780`; the branch is in `Ext::Fixes::TavernSpendOne` (`src/ext/fixes/tavern_counter.h`,
  header-only template so it is unit-testable), called at the lever with the toggle read at the
  top of OnMessage.
- **Lever quoted.** "The faithful lever is to load the material before the call."
- **Faithful arm.** `SubtractItem(row->_resource, 1)` (`0x14012d150`), then re-read
  `row->_resource` (retail `MOV RDX,[R14]` at `0x14004a8e6`) for `ResourceCategory`
  (`0x140088f90`).
- **Fixed arm.** Load `row->_resource`, subtract it, count its category.
- **Test.** `tests/unit/fix_tavern_counter_test.cpp`, 3 tests over a storage array that compacts
  like SubtractItem: last unit spent (faithful counts the next good, fixed the spent one, stock
  identical), unit left (both count the spent one), last row spent (faithful re-reads the stale
  tail = the same good).
- **Doubt.** None in the logic. The material is a raw `ComponentDescription*`, so loading it
  early copies a pointer, no reference count.

## Build and checks (this worktree, nothing run)

- `tools/build.sh build debug --jobs 2`: exit 0, `OpenBA.exe` linked. 109 warning lines,
  none in the changed files except `residenceui.cpp:243` `unused variable 'endRow'`, which is in
  `OnUpdateLayout`, pre-existing, not touched. (The worktree needed
  `git submodule update --init --depth 1 third_party/SDL` first.)
- `tools/build.sh unit debug --jobs 2`: `1005 tests, 0 failures` (1002 + the 3 new).
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target` (R14-R16 skipped, no
  stepclose log).
- Not run (coordinator's): the gate, resave, castmetric, the everything-on boot / stress / quit.
  The faithful gate needs `fix.*=0`, which the stub cannot set (no settings.cfg keys): with the
  stub the default build runs all three fixes ON.
