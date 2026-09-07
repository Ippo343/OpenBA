# Wave 5 — Algorithm packages (Achievement + Save + Picking)

In progress on `wave/upgrade-path-complete`, following [PLAN.md](PLAN.md) §4.2.

## Scope correction, stated up front

PLAN §4.2 lists Wave 5 as "Collision2D, Picking, Pathfinding, Achievement, Save". The census
reports **Collision2D 0 and Pathfinding 0 called** — our tree never resolves a single function in
either namespace, despite the DLL holding 76 and 45 of them. Lane E is establishing whether that
zero is true or a measurement artifact before the wave claims those two namespaces as "done".

The wave's real scope is therefore the **69 functions our tree actually calls**:
Achievement 41, Save 20, Picking 8.

| Lane | Scope | File |
|---|---|---|
| A | `Achievement::Goal` / `GoalInstance` / `GoalList` (20) | `runtime/src/achievement/goal.cpp` |
| B | `Achievement` / `AchievementInstance` / `AchievementManager` (21) | `runtime/src/achievement/achievement_manager.cpp` |
| C | `Save::SaveLoad` / `Save::SavedState` (19) | `runtime/src/save/saveload.cpp` |
| D | `Picking::Scene` / `Picking::PickingMesh` (8) | `runtime/src/picking/scene.cpp` |
| E | Scope + census/audit tooling (no port code) | `tools/runtime_census.py`, `tools/export_name_audit.py` |

Kill switches: `BANISHED_RT_ACHIEVEMENT` / `_GOAL` / `_ACHMGR`, `BANISHED_RT_SAVE` / `_SAVELOAD`,
`BANISHED_RT_PICKING` / `_PICKSCENE`.

## Pre-wave controls (measured 2026-09-06, commit 1a86921 — scaffolding only, 0 Wave 5 ports)

Taken BEFORE any lane registered anything, so a post-wave comparison means something. Both on the
`-O0` `standalone-debug` exe.

| Run | Result |
|---|---|
| `BANISHED_AUTOSTART=1 BANISHED_RT_DIFF=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=60 BANISHED_PAUSEMENU_BUTTON=6` | **76 sites, 24,752,689 calls, 101 divergences in 2 sites.** Clean exit, `standalone_crash.log` empty. |
| `BANISHED_AUTOLOAD=Save9 BANISHED_RT_DIFF=1` + the same self-quit | **83 sites, 29,638,857 calls, 54 divergences in 3 sites.** World built (`[GUL::Load] EXIT — world built`), clean exit, `standalone_crash.log` empty. |

The divergent sites are the SAME pre-existing Wave 1 1-ULP family in both runs, and nothing else:
`Math::Transform::FromYawPitchOffset` (44), `Math::Vector3::FromYawPitch` (9) and
`Math::Vector3::Heading` (1) — all sin/cos/atan2-fed under our own libm. Any NEW divergent site
after Wave 5 lands is a Wave 5 defect.

### One harness trap this wave paid for, recorded so the next reader does not

`BANISHED_MODAL_ANSWER=1` **breaks the `BANISHED_AUTOLOAD` path**. Wave 4 lane H made
`SaveLoad_ModalShowDialog` answer the `modsMissing` prompt with **2** (confirm) on any unattended
run, and `BANISHED_MODAL_ANSWER` overrides that in *either* direction — so passing `1`, which is
the value CLAUDE.md's *mods menu* recipe uses for a different dialog, cancels the load. The symptom
is indistinguishable from the stall lane H fixed: the Load click fires, `[AutoStart] AUTOLOAD
sequence fired` is logged, and the process then loops in `MenuDLT::Draw` forever with no world
created. **The correct invocation passes no `BANISHED_MODAL_ANSWER` at all.**
