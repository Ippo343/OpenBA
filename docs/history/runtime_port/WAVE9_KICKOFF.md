# Wave 9 — consolidated kickoff state (2026-09-08)

All six unmerged wave9 lane branches are now on `wave/upgrade-path-complete`.
Rollback point: **`backup/pre-wave9-consolidate-20260908`** (= 9d725e7).

## What landed, in merge order

| # | Branch | Nature |
|---|--------|--------|
| 1 | `wave9/lane-retslot` | CLOSED lane. 1,274 insertions, `tools/retslot_exe/`. Purely additive. |
| 2 | `wave9/lane-engio-work` | Docs only: re-measure on clean trees + a withdrawn claim. |
| 3 | `wave9/lane-gfxr-work` | Merge commit carrying lane COLL (introsort, AVL probe, stricmp) + the addr_door_audit comment stripper. |
| 4 | `wave9/lane-evm` | **WIP, NOT VERIFIED.** EnumerateVideoModes chain. |
| 5 | `wave9/lane-rres2` | `ExternalFactory::Find`. Rung 2 §5 deliberately empty — transcription, not verification. |
| 6 | `wave9/lane-serial` | **WIP, UNVERIFIED, NOT MEASURED.** Integrator's rescue of the author's uncommitted work. |

Merged at the user's explicit direction, with the park's warnings restated in each
commit message rather than dropped.

## Three park notes that were STALE — do not plan off the park doc alone

1. **lane-serial "has nothing committed / its tip is its own base."** False as of
   consolidation: `fc430e6` carries 1,745 insertions (`serialize_family.cpp` + probe
   + two tools). A merge was *not* the no-op the park predicted.

2. **lane-rres2 "BLOCKED on an overload NAME COLLISION with lane C."** No collision
   exists at this tip. The two symbols are distinct overloads:
   - `?Find@ExternalFactory@Object@@QEBAPEBVExternal@2@...` — public const, returns `const External*` (`external.cpp:747`)
   - `?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@...` — private, returns `External*` (`external_find.cpp:281`)

   Different mangled names ⇒ independent `RegisterRuntimeExport` calls, no
   last-binding-wins shadowing, and the link is clean. (`AEAA` is *private*, not
   unexported — the usual trap.)

3. **The three lanes the park called "open pick-ups" were not the only unmerged
   work.** Three *finished* lanes (retslot, engio, gfxr/coll) had never reached the
   wave branch at all — `tools/retslot_exe/` did not exist here despite the lane
   being recorded CLOSED. "Lane closed" and "lane merged" are different facts.

## Verification actually performed

- **Debug build green** (`mingw32-make standalone-debug`) at the six-lane tip.
  Confirmed the new sources are genuinely in the link, not skipped:
  `external_find.o`, `serialize_family.o`, `enumerate_video_modes.o`,
  `video_mode_sort.o`, `introsort.o`, `msvc_stricmp.o`, `avl_trees_probe.o`.
  A green build over uncompiled files would have proved nothing about the collision.
- **Smoke run green.** `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900
  BANISHED_PAUSEMENU_QUIT=120`: booted to RUNNING, built the world, seeded 6/6
  houses, opened the pause menu at frame 901, dispatched Quit, exited on its own.
  `standalone_crash.log` 0 bytes.

## Two things NOT verified — read before trusting this tip

- **The RELEASE target was already broken before this consolidation** and still is.
  `mingw32-make standalone` fails: `standalone/log_strip.h` defines
  `printf(...)` as a macro, which `std::printf` call sites break on.
  `runtime/src/collection/array_destroy.cpp:942` fails and is **untouched by these
  merges** — it fails at `backup/pre-wave9-consolidate-20260908` too. Lane COLL adds
  further instances in `array.cpp`. Pre-existing defect, not consolidation fallout,
  but it means only the *debug* target is green.
- **The `[RtUnresolved]` residue was NOT measured.** This run logged 0 names, and
  that is the instrument being inapplicable, not a clean gate: `RecordResolveMiss`
  only fires when a resolve misses, and Runtime-x64.dll was present. Attempt 22's
  17 names come from the **DLL-absent** cutover configuration. Per the park, the
  static export scan is not the gate; the pair is the runtime residue plus the
  mint-wall prediction — both still owed for attempt 23.

## Still true from the park

Lane EVM's refresh-rate comparator divides both sides by B's denominator. That is a
transcribed retail bug and must stay: at `den == 0` the divisions produce inf/NaN and
COMISS unordered, so they are load-bearing for user-visible video-mode ordering.
"Fixing" it changes behaviour.
