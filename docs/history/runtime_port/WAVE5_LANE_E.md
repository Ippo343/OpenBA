# Wave 5 lane E — scope and tooling

Owns `tools/runtime_census.py`, `tools/export_name_audit.py`, this doc. No port code.

## 1. Two tool blind spots fixed: multi-line C++ string-literal concatenation

`ResolveExport("...")` and `RegisterRuntimeExport("...")` call sites sometimes split the mangled
name across two adjacent C++ string literals, e.g.:

```cpp
ResolveExport("?SetAchievement@AchievementInstance@Achievement@@QEAAX"
              "V?$ExternalPtr@VAchievement@1@@Object@@@Z");
```

The C++ preprocessor concatenates these into one string. Both `tools/runtime_census.py`'s
`RE_EXPORT`/`RE_REG_EXPORT` regexes and `tools/export_name_audit.py`'s `PATTERN` used
`"([^"]+)"` — a single-literal capture — so they silently read only the FIRST fragment.
`export_name_audit.py` then reported the truncated fragment as a name "NOT in
Runtime-x64.dll", i.e. a false-positive phantom. This is the mirror image of the tool's own
stated purpose (catching real phantom names) and undermines trust in its true positives.

**Fix**: both scripts now match only up to the opening quote, then a shared `concat_literal()`
helper walks forward absorbing any further string literals separated only by whitespace —
exactly what the preprocessor does — before the name is used. No other matching logic changed.

### Before / after — export_name_audit.py

```
before: 626 distinct ResolveExport names; 20 are NOT in Runtime-x64.dll
after:  625 distinct ResolveExport names; 15 are NOT in Runtime-x64.dll
```

5 names were previously truncated phantoms; concatenation resolves 5 of the 20 (2 truncated
call sites of `?CreateElement@Dialog@UI@@...` collapsed to the SAME correct name, hence 626→625
distinct names as well as 20→15 missing):

| Truncated (before) | File:line | Real export? |
|---|---|---|
| `?SetAchievement@AchievementInstance@Achievement@@QEAAX` | `code/Game/Game/gameupdateloop_load.cpp:71` | yes |
| `?AddMesh@Scene@Picking@@QEAAIV?$ExternalPtr@VPickingMesh@Picking@@@Object@@` | `code/Game/Component/pickingcomponent.cpp:171` | yes |
| `?Create@FrameCopyImageBuffer@Graphics@@UEAA_NW4ImageType@ImageBuffer@Video@@W4Usage@45@` | `code/Game/Game/w2_takescreenshot.cpp:150` | yes |
| `?CreateElement@Dialog@UI@@QEBA?AV?$InternalPtr@VElement@UI@@@` (2 call sites, `engine/funcs/engine_funcs.cpp:2186` and `standalone/address_registry.cpp:1416`) | both merge to one correct name | yes |

### Before / after — runtime_census.py

```
                     before                          after
namespace          total  called  ported  %        called  ported   %
UI                  1625     137       0   0.0%       136       0    0.0%
Object               191      41      27  65.9%        41      28   68.3%
TOTAL               4632     879     217  24.7%        878     218   24.8%
```

- **UI called 137→136**: the two truncated `CreateElement` call sites were two distinct
  (wrong) names before; now one correct name — the denominator was OVERSTATED by 1, not
  understated.
- **Object ported 27→28** (called stays 41): one of the 41 Object-namespace `ResolveExport`
  calls was matching a truncated name that is NOT what `RegisterRuntimeExport` used, so it
  showed as unported; concatenation fixes the name to match the real registration. This is a
  genuine correction in the port's favor — Wave 3's Object port covers one more of its own
  called functions than previously credited.
- No other namespace's called/ported numbers moved. The gated-registration warning (allocator
  opt-in files) and the non-literal-registration warning logic are untouched and still fire
  correctly (verified: neither before nor after run currently has a live non-literal hit in the
  tree, and the allocator gated-count of 27 is identical before/after).
- One pre-existing, unrelated artifact survives unchanged in both runs: three source comments
  (`runtime/src/io/filesystem.cpp:350`, `runtime/src/io/serializer.cpp:609`,
  `runtime/src/object/type_system.cpp:488`) contain the literal text
  `` `RegisterRuntimeExport("<literal>"` `` inside a comment explaining this very tool
  limitation, which the regex has always matched (both before and after this fix) and reports
  as a phantom-ish `export  <literal>` orphan entry. Not a regression; flagged for whoever next
  touches those comments.

## 2. Authoritative phantom `ResolveExport` list (post-fix), by namespace

15 names remain that are genuinely absent from `Runtime-x64.dll`'s export table. Each needs a
verdict (deliberate fallback vs. real defect) from the lane that owns the namespace — this lane
only certifies that they are real absences, not concatenation artifacts.

**Achievement (4)**
- `??1Goal@Achievement@@QEAA@XZ` — `code/Game/Goal/statgoal.cpp:403`, `code/Game/Goal/timegoal.cpp:157`
- `??1GoalInstance@Achievement@@QEAA@XZ` — `code/Game/Goal/timegoal.cpp:122`
- `?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z` — `engine/funcs/engine_funcs.cpp:4711`
- `?~Goal@Achievement@@QEAA@XZ` — `code/Game/Goal/statgoal.cpp:407`

(Note: the REAL exported dtors are `??1Goal@Achievement@@UEAA@XZ` / `??1GoalInstance@Achievement@@UEAA@XZ`
— `QEAA` vs `UEAA`, i.e. non-virtual vs virtual calling-convention decoration, the same family of
mistake WAVE2 found for `Filename::AdjustDelimiters`. `Persist`'s exported form is `UEAAXAEAVStream@IO@@@Z`
too. These 4 call sites resolve null and presumably fall back to `ResolveAddr` — worth lane A/B
confirming a fallback exists at each site.)

**UI (6)**
- `?AddController@DialogInstance@UI@@QEAAXIHHPEBVExternal@Object@@AEBVString@System@@@Z` — `engine/funcs/engine_funcs.cpp:3481`
- `?BringDialogToFront@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z` — `engine/funcs/engine_funcs.cpp:3252`
- `?CloseDialog@UserInterface@UI@@QEAAXAEAV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z` — `engine/funcs/engine_funcs.cpp:1603`
- `?GetFirstContainedDialog@UserInterface@UI@@QEAA?AV?$InternalPtr@VDialogInstance@UI@@@Object@@HH@Z` — `engine/funcs/engine_funcs.cpp:2905`
- `?GetReference@Dialog@UI@@QEBA?AV?$ExternalPtr@VDialogReference@UI@@@Object@@XZ` — `engine/funcs/engine_funcs.cpp:2947`
- `?SetText@EditElement@UI@@QEAAXVString@2@@Z` — `engine/funcs/engine_funcs.cpp:2497`

**Graphics (2)**
- `?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z` — `code/Game/Action/action_base_complete.cpp:383`
- `?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z` — `code/Game/Action/action_base_complete.cpp:385`

**Save (2)**
- `?CheckMod@SaveLoad@Save@@QEAAHHI@Z` — `engine/funcs/engine_funcs.cpp:4523`
- `?SetSaveName@SaveLoad@Save@@QEAAXV?$String@D@System@@H@Z` — `engine/funcs/engine_funcs.cpp:4531`

**Audio (1)**
- `?PlayEffect@Scene@Audio@@QEAIV?$ExternalPtr@VSoundEffect@Audio@@@Object@@W4SoundType@12@VTime@System@@@Z` — `engine/funcs/engine_funcs.cpp:2949`

(`?AA` vs the likely-correct `?A` return-thunk decoration on `PlayEffect` looks like the same
class of defect as WAVE1/WAVE2's return-type mistakes; not confirmed against the real export
string, flagged for the Audio owner when that namespace is worked.)

**Not namespaced** (pre-existing, unrelated to Wave 5): `<literal>` — three doc comments (see
§1) that literally contain the text `RegisterRuntimeExport("<literal>"`, not a real call site.

## 3. Collision2D / Pathfinding verdict: the zero is TRUE, not a measurement artifact

**Pathfinding: TRUE zero — reimplemented natively in the exe, never calls the DLL.** Our tree's
A* / pathfinding logic (`code/Game/Action/action_pf_avl.cpp`, `action_pf_geom.cpp`, the AVL
node-pool/rotation family, plus callers in `citizenai_*`, `Work/*`) is a from-scratch
transcription of the EXE's own pathfinding code (`Application-x64.exe`), resolved via
`ResolveAddr(Module::Exe, ...)`. The one Runtime symbol named `fn_Pathfinding_FindPath`
(`engine/funcs/engine_funcs.h:5087`) is a documented MISNOMER for exe function `FUN_1400bf790`
(a citizen work-action-chain dispatcher), not a call into `Runtime-x64.dll`'s
`Pathfinding::Scene`. The DLL's `Pathfinding::Scene::GeneratePath` etc. (48 exports,
`0x18006c970`–`0x18010f980`) are never referenced anywhere in `code/`, `engine/`, `runtime/`,
`standalone/`.

**Collision2D: TRUE zero — genuinely unused.** No source file references `Collision2D`,
`CollisionMesh`, `CollideCircle`, or `SweptCircle` except one comment
(`code/Game/Component/pickingcomponent.cpp:342`) that explicitly identifies
`Collision2D::Scene::RemoveMesh` (`0x18010c3c0`) by name and says "wrong one" — i.e. a prior
engineer already found this namespace's address range while working `Picking::Scene::RemoveMesh`
and deliberately did NOT use it. Whatever gameplay purpose `Collision2D::Scene` serves in retail
(circle/mesh sweep contacts — looks like citizen-citizen or projectile collision), our tree does
not reach it on any path exercised so far.

**Evidence method**: got the 80 (Collision2D) + 48 (Pathfinding) real export addresses from the
DLL's export table (`pefile`, filtered by `runtime_census.namespace_of()`), and checked (a) exact
address equality against all 246 `ResolveAddr`/`ResolveRuntimeRaw` call sites in our tree — 0
matches — and (b) every one of the ~127 addresses that merely fall inside the same broad
`[0x180064000, 0x180110000)` code region (a red herring: that span holds thousands of unrelated
functions from many other namespaces) resolves, per this project's own existing source comments,
to Achievement/Save/Input/Modification/IO/Graphics/System/Engine/Audio/Video/Object functions —
never to Collision2D or Pathfinding.

## 4. FLAGGED — functions in Wave 5's namespaces reached by address, not by name

The census buckets every `ResolveAddr(Module::Runtime, 0x...)` call under `"internal"`, not its
real namespace, because the tool has no name to classify by. Cross-referencing all 246 such
addresses against the DLL's export table found **20 that are exact addresses of Achievement /
Save / Picking exports** — real Wave 5 functions our tree calls that a scope list built purely
by grepping `ResolveExport` call sites would miss. Reading every one of the 20 call sites
(not just grepping for the name, which is where an earlier pass on this same data went wrong —
a missing leading `?` in a hand-typed grep pattern hid five real name-based resolves) splits
them three ways:

**(a) Already ported, just mis-bucketed by the census (6)** — lane A's `goal.cpp` already
registers `GoalList::GetGoal`, `Goal` ctor/dtor, `GoalInstance::HasFailed`/`IsMet`/dtor via
`RegisterRuntimeAddr`. Not a gap, just invisible to the "Achievement" row of the printed table.

**(b) Deliberately absent, documented (2)** — `Goal::Serialize` (`0x18009ee20`) and
`GoalInstance::Persist` (`0x18009ef70`): `goal.cpp`'s own banner says both are "DELIBERATELY
ABSENT — see WAVE5_LANE_A.md §4". Not a gap.

**(c) Already reached by name — safe, no scope gap (5)** — each of these resolves
`ResolveExport(name)` FIRST and only falls back to the literal address if that fails, at ONE
call site, into ONE function pointer: `GetOperation@SaveLoad` (`engine_funcs.cpp:2426-2431`),
`GetSaveState@SaveLoad` (`engine_funcs.cpp:2469-2474`), `IsAchievementAwarded@AchievementManager`
(`engine_funcs.cpp:2648-2654`), `GetAchievementCount@AchievementManager`
(`engine_funcs.cpp:2748-2753`), `GetAchievement@AchievementManager` (`engine_funcs.cpp:2754-2759`).
A name-based `RegisterRuntimeExport` port already reaches all of these; registering the address
too is harmless insurance, not new scope.

**(d) Genuinely new gaps — address-only, no `ResolveExport` anywhere in the tree (5):**

| Address | Function | Call site | Belongs to |
|---|---|---|---|
| `0x18009e070` | `?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z` (base `Achievement::Achievement` class) | `code/Game/Game/scenario.cpp:78` | Lane B |
| `0x18009e170` | `?GetFileVersion@Achievement@1@UEBAHXZ` | `code/Game/Game/scenario.cpp:76` | Lane B |
| `0x18009e180` | `??1Achievement@0@UEAA@XZ` (base dtor) | `code/Game/Game/scenario.cpp:74` | Lane B |
| `0x18009e240` | `??0Achievement@0@QEAA@XZ` (base ctor) | `code/Game/Game/scenario.cpp:72` | Lane B |
| `0x180125290` | `?RemoveMesh@Scene@Picking@@QEAAXI@Z` | `code/Game/Component/floorcomponent.cpp:308`, `code/Game/Component/pickingcomponent.cpp:344` (also has an exe-dispatch-slot fallback, never a Runtime export-name resolve) | Lane D |

Re-checked with a broader grep for any name-based resolve of the base `Achievement` ctor / dtor /
`GetFileVersion` / `Serialize` anywhere in the tree — none exists. `RemoveMesh` likewise has no
name-based call site anywhere. None of `runtime/src/achievement/achievement_manager.cpp` or
`runtime/src/picking/scene.cpp` mention these addresses — both files are still pure scaffolds
(`SCAFFOLD ONLY`, 12 lines, register nothing) as of this writing, so no lane has yet made a scope
decision that would conflict. Each needs `RegisterRuntimeAddr` at its literal address —
`RegisterRuntimeExport` alone will never intercept these call sites.

**(e) Split-brain risk — worse than address-only, and the sharpest finding in this doc:**
`Scene::AddMesh@Picking` (`0x1801252e0`) has THREE call sites and they do not agree on how to
resolve:
- `code/Game/Component/citizenai_executor.cpp:406` — address-only
- `code/Game/Component/floorcomponent.cpp:303` — address-only
- `code/Game/Component/pickingcomponent.cpp:171` — NAME-only (the literal fixed in §1), no
  address fallback

If lane D registers `AddMesh` by `RegisterRuntimeExport(name)` alone, `pickingcomponent`'s citizen
pick-mesh calls land on our port while `floorcomponent`'s and `citizenai_executor`'s calls keep
hitting the live DLL's `Scene::AddMesh` — two callers handing out mesh ids from the SAME
`Picking::Scene` object through two different implementations (PLAN §4.1 decision 5's "ported
half-way with two implementations in play"). This needs `RegisterRuntimeAddr(0x1801252e0, ...)`
**and** the export name registered together, not just one. `AddMesh` was already counted in the
Picking=8 headline (it resolves fine today via the name-only call site), so it is not a *new*
function to add to scope — it is a registration-completeness hazard on an existing one.

## 5. Corrected picture (manual, not a tool change)

The census intentionally does not attribute `ResolveAddr` calls to a namespace (there's no name
to parse), so the numbers below are a manual reconciliation, not something `runtime_census.py`
now reports on its own. Only bucket (d) above is genuinely new scope; bucket (c) is already
reached by name and adds no new port work:

| Namespace | Census "called" (export-name only) | + genuine new gaps (§4d) | True minimum called |
|---|---|---|---|
| Achievement | 41 | +4 (base ctor/dtor/GetFileVersion/Serialize) | **45** |
| Save | 20 | +0 (both already reached by name, §4c) | **20** |
| Picking | 8 | +1 (`RemoveMesh`) | **9** |
| Collision2D | 0 | +0 (confirmed true zero, §3) | **0** |
| Pathfinding | 0 | +0 (confirmed true zero, §3) | **0** |

Wave 5's real scope is therefore **74 functions, not 69**: Achievement 45, Save 20, Picking 9 —
plus the `AddMesh` dual-registration hazard in §4e, which is not a count change but must be
fixed before cutover regardless.
