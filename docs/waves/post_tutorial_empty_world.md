# A new game started after a tutorial builds a world with no models

**Status: OPEN. Reproduced, localised, PRE-EXISTING (not the Runtime port).**
Found 2026-09-07 by the user during Wave 6; investigated at the Wave 6 close.

## Symptom

Start a tutorial, quit it to the main menu, start a New Game. The new world has terrain, water and
the citizens' status icons, and **no trees, no rocks, no ore, no buildings, no citizen models**.

`new -> quit -> new` is CLEAN. The tutorial is what poisons the next world — which matters,
because a tutorial is a **loaded retail save** and a new game is a **generated world**, so they
tear down through different paths.

## Repro (unattended)

```
BANISHED_AUTOTUTORIAL=1 BANISHED_AUTOSTART_GAMES=1 \
BANISHED_SEED=12345 BANISHED_CAMERA_PIN=town,70 \
BANISHED_PAUSEMENU=400 BANISHED_PAUSEMENU_BUTTON=5 \
BANISHED_SCREENSHOT_AT=1000 ./reimpl-x64.exe
```

`BANISHED_AUTOSTART_GAMES` alongside `BANISHED_AUTOTUTORIAL` was added for this bug
(`standalone/autostart.cpp`): after the tutorial's Load click the driver rearms, so a
quit-to-menu (`PAUSEMENU_BUTTON=5`) is followed by a New Game. The old games-loop could only
express `new -> quit -> new`, which does not reproduce it.

The screenshot lands in the SECOND world only if the frame is late enough — the counter is
process-wide across worlds, and world 2 starts around frame 737, so use 1000, not 700.
Convert with `tools/bmp2png.py <in.bmp> <out.png> 3`.

## What is measured

`BANISHED_LOD_CENSUS=1`, same sequence, camera pinned:

| World | meshInstances | drawItems |
|---|---|---|
| A direct new game | **16,137** | 1,341 |
| A new game after a tutorial | **12** | 159 |

**The models are never added to the scene.** This is not culling, not the LOD pick, and not the
draw submission — the world is genuinely built without them. Terrain renders because terrain is
its own path; the status icons render because they are sprites, not meshes.

## Ruled out

* **The Runtime port (Wave 6) is NOT the cause.** With `BANISHED_RT_GRAPHICS=0 BANISHED_RT_VIDEO=0`
  — the whole wave's kill switch — the post-tutorial world is byte-for-byte the same empty scene.
  Screenshots of both arms are indistinguishable.
* Not the `Geometry::Attach`/`Detach` phantom-name fix either: that fix is live in both arms
  (the `BANISHED_RT_*` switches do not disable it), and a third arm with it reverted also matches.

## Where to look next

Something with **process lifetime** survives the tutorial's teardown in a state that stops the next
world from creating model geometry. The shape to suspect is a cache or table that the tutorial
world's teardown frees or empties while a process-lifetime owner still points at it — the same
family as `reference_lazy_init_guard_also_matches_restored.md` and
`reference_rawmaterial_startup_slot4_never_installed.md` (a registry that only ever gets its
entries once).

Concretely, in rough order of suspicion:

1. The `Object::ExternalFactory` / `External` resource table — descriptions and `GraphicsMesh` /
   `Material` externals are `FindOrLoad`ed and process-lifetime. Wave 3 deliberately did NOT port
   `ExternalFactory::Clean`, so retail's own `Clean` runs at teardown; if it drops entries a
   generated world then re-requests, the second world gets null meshes.
2. Whatever the tutorial's save-load path installs that a generated world does not
   (`ScenarioDialog::LoadSelected` swaps `persistence+0x90` to the data directory — if that is not
   swapped back, a new game may look for its resources in the wrong place).
3. The description/type-id caches the exe keeps across worlds.

A cheap first probe: log `FindOrLoad` results for `GraphicsMesh` in world 2 and see whether it
returns null or an empty mesh, which separates "never asked" from "asked and got nothing".
