# A new game started after a tutorial builds a world with no models

**Status: CLOSED 2026-09-07.** One defect in `MapGenerator::Serialize`. Pre-existing, not the
Runtime port. Found 2026-09-07 by the user during Wave 6; localised at the Wave 6 close and
root-caused the same day.

## Symptom

Start a tutorial, quit it to the main menu, start a New Game. The new world had terrain, water and
the citizens' status icons, and **no trees, no rocks, no ore, no buildings, no citizen models**.

`new -> quit -> new` was CLEAN. Only a tutorial first reproduced it.

## Root cause

`MapGenerator::Serialize` (`code/Game/Game/mapgenerator.cpp`, exe `0x1400a2980`) reads the
start-condition data: the citizen-family list, the initial-structure list, and six ExternalRef
pools (scattered resources, cull lists, agriculture plants/trees/livestock, env templates).

Our port carried a **FRESH-LOAD ASSUMPTION**: it allocated a brand-new element buffer on every
pass and zeroed each element before serializing it. That assumption is false. Retail's
`Object::External::Load` re-invokes `Serialize` on an **already-loaded** external on every
`ExternalFactory::FindOrLoad`, and the second pass does not write the names back — so the zero we
had just stored is what survived. Every pool came back with a null name String.

The tutorial is what triggers the second pass: `ScenarioDialog` re-requests `StartConditions.rsc`.
A plain `new -> quit -> new` never asks twice, which is exactly why it was clean.

Downstream, `StartTown_SpawnResources` logged `[startTown::res] begin [0] name="(null)"`,
`StartTown_ResolveTemplate` returned null for all three pools, and the world was generated with no
scattered resources, no initial structures and no starting herd. Nothing was wrong with the
meshes, the scene, the LOD pick or the draw path — the entities were never created.

Same family as `reference_createplaced_placebitarray_zeroed_by_reparse.md` (`_placeBitArray`).

## The fix

The three exe array-resize helpers were read out of Ghidra and ported faithfully:
`FUN_14001b270` (stride 8, `InitialFamily`), `FUN_14001b340` (stride 0x18, `ExternalRef`) and
`FUN_14001b4e0` (stride 0x30, `InitialStructure`). All three begin with

```c
if (newCount != oldCount) { ... }
```

so a re-parse of the same `.rsc` — where the count is unchanged — **does nothing at all**: no
realloc, no re-init, existing elements (including the resolved-template cache at `+0x10`) untouched.
When the count does change, only elements past the old count are default-constructed.

`MapGenerator::Serialize` now mirrors retail on all four points:

* the count goes through a **local** seeded from the struct's current count, not straight into the
  struct field;
* the resize runs **only on the load leg** (`*(int*)(*(void**)serializer + 8) == 0`);
* the resize is a no-op at equal counts (above);
* the element loops serialize **in place** and zero nothing.

## Diagnostics added

* `BANISHED_MAPGEN_TRACE=1` — an ENTER and an EXIT line per `MapGenerator::Serialize` pass per
  instance, with the resource pool's first three names. This is the probe that settled it: pass 1
  printed the three `Template\NaturalResource*.rsc` names, pass 2 printed
  `(null) | (null) | (null)`.
* `BANISHED_MODEL_TRACE=<n>` now takes the log budget as its value (was a fixed 96 lines, which a
  ~16k-model world exhausts long before a second world).

## Repro (unattended)

```
BANISHED_AUTOTUTORIAL=1 BANISHED_AUTOSTART_GAMES=1 \
BANISHED_SEED=12345 BANISHED_CAMERA_PIN=town,70 \
BANISHED_PAUSEMENU=400 BANISHED_PAUSEMENU_BUTTON=5 \
BANISHED_SCREENSHOT_AT=1000 ./reimpl-x64.exe
```

`BANISHED_AUTOSTART_GAMES` alongside `BANISHED_AUTOTUTORIAL` was added for this bug
(`standalone/autostart.cpp`): after the tutorial's Load click the driver rearms, so a
quit-to-menu (`PAUSEMENU_BUTTON=5`) is followed by a New Game.

The screenshot lands in the SECOND world only if the frame is late enough — the counter is
process-wide across worlds, and world 2 starts around frame 737, so use 1000, not 700. Screenshots
land in `~/Documents/Banished/Screenshots/`; convert with `tools/bmp2png.py <in.bmp> <out.png> 3`.

## Verification

`BANISHED_LOD_CENSUS=1`, same sequence, camera pinned:

| World | meshInstances | before |
|---|---|---|
| A direct new game | 16,133 | 16,137 |
| A new game after a tutorial | **16,137** | **12** |

The post-tutorial world 2 now spawns exactly what a direct new game does — 8 structures,
24 citizens, 15,512 scattered resources, 982 environment entities and the 8-head starting herd —
and the direct-new-game path is unchanged.
