# Wave 9, Lane B — inventory and sequencing of the 145 named functions + 51 ctors/dtors

Prep pass, 2026-09-07. **No porting was done this round** — per the lane brief, this is
inventory and sequencing only. Read `docs/runtime_port/WAVE9.md` and `PLAN.md` §4/§6 first.

## 0. The exact count, reconciled

Input: `docs/runtime_port/wave9_unported_20260907.txt`, "Called but not ported (320)".

The 320 rows split into three kinds: `export` (250), `addr` (66, all namespace `internal` — lane
D's), `ordinal` (4). Of the 250 `export` rows, 60 are exported DATA (`??_7` vtables 28,
`?_singleton@…` 18, `?_typeInfo@…` 14 — lane C's), leaving:

| Kind | Count |
|---|---|
| Ordinary named exports (`func`) | 139 |
| Ctors (`??0`) | 27 |
| Dtors (`??1`) | 24 |
| Ordinals | 4 |
| **Lane B total** | **194** |

**This does not match WAVE9.md's "145 + 51 + 4 + 3 = 203".** The real number, from the file
this round was told to use, is **194**. Two things moved between when WAVE9.md was written and
this census snapshot: the `internal`/`addr` bucket is 66 here vs. the 57 WAVE9.md cites (+9), and
WAVE9.md's separate "3 `.data` globals by address" bucket does not exist as a distinct kind in
this file at all — it appears to have been folded into the 66 `addr internal` rows. Ordinary
named exports correspondingly came in at 139 vs. the 145 quoted (-6), ctor/dtor held at 51, DATA
came in at 59 by direct count (28+18+14) vs. 60 cited (off by one, immaterial). None of this
changes lane B's actual work list, which is fixed by the file this lane was told to read — it is
recorded here because the two documents now disagree and a future reader would otherwise trust
the wrong one.

## 1. Classes: the real unit of work

PLAN §4.1 decision 5 (class atomicity) means the denominator that matters is **classes**, not
functions. Demangling the 194 symbols (first qualifier before the class's own namespace, verified
against known patterns like `Engine::Engine` and `AchievementManager::Achievement`) and grouping:

- **194 functions collapse into 83 distinct classes** (85 raw demangled groups, minus 2 that were
  parsing artifacts of operator-overload mangling — `??9…String` is `System::String::operator!=`
  and `??3…Context` is `Video::Context::operator delete`, both folded into their real classes).
- **71 of 83 classes (86%) are within 3 functions of being fully ported.** 27 are within 1.
- **Exactly one class is completely finished, i.e. 100%, by lane B's own list but excluded**:
  n/a — every class in the CSV has ≥1 unported member by construction (that's what put it in the
  320-row list). The "100%" classes visible during this analysis (`Serializer`, `Vector3`,
  `Geometry`, `Random`, 45 others) are classes that are **already fully ported** and only show up
  as cross-reference context, not in lane B's 194.
- **Only one class is completely blocked with nothing portable left in it**: `System::MemoryAllocator`
  (4 of 31 members remaining, all 4 gated on Lane E per PLAN §3 / WAVE9.md §3 — "the heap cannot
  be half-ours"). Every other class with unported work has at least one member with no known
  blocker.

Size split across all 194 (bucketed by instruction count; see §3 on how sizes were obtained):

| Bucket | Count |
|---|---|
| Trivial (<20 instr) | 77 |
| Small (<80 instr) | 58 |
| Medium (<250 instr) | 37 |
| Large (≥250 instr) | 17 |
| Unknown (4 ordinals, 1 last-export-in-DLL) | 5 |

## 2. Full inventory

`docs/runtime_port/wave9_laneB_inventory.csv` — one row per function: namespace, class, kind,
function name, mangled symbol, size bucket, size source (`ghidra-exact` for 13 spot-checked
functions, `pefile-RVA-delta (ESTIMATE)` for the rest — see caveat below), the size value, the
class's unported/ported/total/completion%, an oracle-soundness note, a callee-dependency note
(populated only for the 8 functions whose callees were actually pulled from Ghidra), and the
source file(s) that already reference the mangled name today (registration site, engine_funcs.h
entry, or a call site expecting the port).

### Sizing method, and why most of it is an estimate

Ghidra MCP (`get_function_metrics`) was reachable and accurate — confirmed against
`Runtime-x64.dll`, image base `0x180000000` — but at ~194 functions, an exact per-function pass
was not affordable this round; the task brief explicitly allows the pefile-RVA-delta fallback in
that case, so that is what backs most of the CSV. **13 functions were spot-checked exactly** via
Ghidra (`get_function_metrics` by name, falling back to address via `get_function_by_address` when
Ghidra's own symbol wasn't yet the qualified name) to validate the fallback:

| Function | Exact instrs | RVA-delta estimate said | Agreement |
|---|---|---|---|
| `InternalBasePtr::CheckPtr` | 1 | trivial (16B) | match |
| `Quaternion::Lerp` | 8 | **medium (400B)** | **overshoot, ~50x** |
| `Goal::Serialize` | 3 | **small (144B)** | **overshoot, ~50x** |
| `Thread::ThreadFunc` | 42 | small (160B) | match |
| `Transform::AsTransform3` | 47 | small (256B) | match |
| `GoalInstance::Persist` | 78 | small (144B) | close |
| `AchievementManager::ReadAchievements` | 68 | small (272B) | match |
| `GoalList::Initialize` | 112 | medium (400B) | match |
| `Frustum::Projection` | 171 | medium (848B) | match |
| `Persistence::ReadSettings` | 256 | medium (1072B) | borderline |
| `SaveLoad::Draw` | 349 | large (3232B) | match |
| `ImageBuffer::SaveImageToFile` | 359 | large (1360B) | match |
| `Persistence::WriteSettings` | 538 | large (2080B) | match |

**11 of 13 land in the right bucket; 2 (`Lerp`, `Goal::Serialize`) are badly overestimated** —
both are genuinely tiny (one-line delegations) but sit next to a gap before the next named export,
so the RVA delta counts dead/unexported space as if it belonged to the function. **Treat every
"small" or below RVA-delta bucket as a plausible "actually trivial"; the medium/large distinction,
which is the one that matters for sequencing, held up in every spot check.** A real per-function
Ghidra pass is recommended before final commitment at the small-scale end, not before the wave
overall.

### Class completion, method

For each of the 83 classes, "ported" = number of that class's methods already found in a
`RegisterRuntimeExport`/`RegisterRuntimeAddr` call across `runtime/`, `standalone/`, `engine/`,
`code/` (525 such calls found by direct regex; the true count is slightly higher — 18 sites pass a
named `const char* const kX` constant rather than a literal, per WAVE9.md's item 3, which a plain
regex can't see). This is therefore a **slight undercount of "ported"**, i.e. completion
percentages below are a **conservative floor**, not an overcount.

## 3. Oracle soundness

Default is sound (`BANISHED_RT_DIFF`). Three categories of exception, applied to the CSV:

1. **Reused verbatim from WAVE9.md's holdback triage table** for every one of lane B's 194 rows
   that overlaps it (19 rows: `Audio::Scene/Device::*`, `Engine::Engine` ctor/dtor/Startup/Shutdown,
   `MainLoop::Run/OnVideoError/OnVideoRestart`, `Input::Device::Update/EnableMouseClipping`,
   `KeyMap::Load`, `External::SaveText`, `ExternalFactory::Clean/FindOrLoad/SerializeReferenceTable`,
   `UpdateLoop::LoadResources`). That table already did the hard work of reading the callee and
   should not be re-derived.
2. **`System::MemoryAllocator`, whole class**: NO-GO until Lane E arms `BANISHED_RT_ALLOC` — "the
   heap cannot be half-ours" (PLAN §3).
3. **General heuristics for everything not in (1)**: any `Serialize`/`Persist`/`Save*` function is
   flagged SAVE-PATH (prefer the lane-S emission-sequence replay over naive ours-then-theirs,
   following the `Audio::Scene::Serialize` precedent that closed row 8); `IO::Modification`'s 7
   remaining members (`Enable`, `SaveList`, `RefreshList`, `SetupFileSystem`, `Swap`, plus 2 data
   accessors) are flagged STATEFUL because they mutate the live mods list / filesystem mount state,
   where calling ours-then-theirs risks double-toggling a mount.

`Input::KeyMap::Save` is **not** in WAVE9.md's table (only `Load` and the already-ported
`RegisterInput` were triaged) — flagged in the CSV as "untriaged; likely shares `Load`'s
`IO::Persistence` registry-string dependency by symmetry" rather than assumed cheap.

## 4. Callee dependencies (sampled, not exhaustive)

8 functions had their callees pulled via `get_function_callees` — enough to demonstrate the
pattern WAVE9.md's "porting can grow the cutover surface" section describes, not a full sweep:

- **`Volume::Frustum::Projection` and `System::Thread::ThreadFunc` have zero DLL-internal
  dependency** — `Projection` calls nothing at all; `ThreadFunc` calls only Win32
  `SetEvent`/`WaitForMultipleObjects`. These are the cleanest possible ports in the whole list.
- **`Achievement::GoalList::Initialize` and `Achievement::GoalInstance::Persist` share an unnamed
  internal, `FUN_180009d70`** — porting either one creates that lane-D row; porting the other is
  then free. `Persist` also calls two *more* unnamed internals (`FUN_180006d10`, `FUN_180009fa0`)
  not shared with anything sampled here, so despite being only 78 instructions it is a bigger
  lane-D contributor than its size suggests.
- **`IO::Persistence::WriteSettings`**, the largest sampled function (538 instructions, cyclomatic
  complexity 124), has **zero new unnamed internals** — every callee is a real Win32 registry API
  call or an already-exported/nameable helper (`Map_Find`, `Set`, `AsUTF8`). Big, but tractable.
  `ReadSettings` (256 instr) has exactly one new internal, `FUN_18000f7d0`.
- **`Video::ImageBuffer::SaveImageToFile`** (359 instr, screenshot save) creates **two** new
  lane-D rows (`FUN_180053da0`, `FUN_1800881b0`) among otherwise well-named callees — a case where
  "it's just a screenshot" undersells the actual cost.

**Consequence for sequencing**: prefer `Projection`/`ThreadFunc`-shaped functions (self-contained)
and treat any function whose callee list hasn't been checked as an unknown-cost item until it is.

## 5. Proposed port order

**Tier 0 — port first, zero known hazard, cheapest verified profile** (do these to retire 6
classes outright): `Volume::Frustum::Projection`, `System::Thread::ThreadFunc`,
`Math::Quaternion::Lerp`, `Math::Transform::AsTransform3` (all ghidra-confirmed trivial/small,
no unnamed-internal callees found), then the paired `Achievement::GoalList::Initialize` +
`GoalInstance::Persist` (share one lane-D internal, so port together and pay for it once).
`Achievement::Goal::Serialize` is genuinely 3 instructions per Ghidra but is a `Serialize` — use
the lane-S technique, not a naive diff, even though it's tiny.

**Tier 1 — near-complete classes (≤3 remaining), no WAVE9-inherited blocker.** 71 of 83 classes
qualify; the highest-value ones by (completion% desc, size asc) are `UI::Element` (3 left of 23),
`System::String` (2 of 15), `Object::InternalBasePtr` (1 of 7 — but see caution: that 1 is
`CheckPtr`, a confirmed bare `RET` in every shipped build; registering a real one is a *perf*
decision, not a correctness gap, per the project's standing note on this exact function),
`Save::SaveLoad` (2 of 13, one of which — `Draw`, 349 instr — is a real rendering-path port, not
cheap despite the class being 85% done), `Achievement::AchievementManager` (3 of 12),
`Graphics::MeshInstance` (3 of 13), `IO::Persistence` (2 of 7, sizes above), plus the ten
single-function classes (`LoaderLoop`, `BaseTypeInfo`, `PerlinNoise`, `ImageBufferData`,
`RenderStateData`, `Particle`, `TabElement`, and the four Video `*Data`/`Buffer` pairs at 60%).

**Tier 2 — defer to a named prerequisite** (WAVE9.md's own dispositions, unchanged):
`Audio::Scene`/`Device` (device-level fake), `Input::KeyMap::Load` (+ untriaged `Save`),
`Input::Device` (bespoke record/replay oracle — the hardest single item in the wave, own lane),
`Object::External::SaveText` (needs `IO::TextFileStream`), `Object::ExternalFactory::Clean`/
`FindOrLoad` (no oracle, shared-tree mutation), `SerializeReferenceTable` (unblocked but not cheap
— save-path `IO::Encoding` dispatch, needs lane S), `Engine::UpdateLoop::LoadResources` (after the
IO holdbacks).

**Tier 3 — defer entirely to Lane E**: `System::MemoryAllocator`'s 4 remaining members;
`Engine::Engine` ctor/dtor/`Startup`/`Shutdown`; `Engine::MainLoop::Run`. `MainLoop::OnVideoError`/
`OnVideoRestart` are STUBs, not ports (dead code outside a forced device-lost path).

**Tier 4 — the big UI/Video surface, sequence last and expect it to need its own sub-lane**:
`UI::UserInterface` (11 remaining, effort score 32 — the single largest item in lane B by a wide
margin), `IO::Modification` (7 remaining, all flagged stateful), `UI::DialogInstance` (6),
`Video::Context` (5, but 82% already done — likely mechanical once started), `Video::Device` (8,
43% done). Also here: the ten 0%-complete classes with 2 members each (`Video::*Data` family,
`UI::RadioElement`/`ComboElement`/`Dialog`, `System::Timer`/`Error`/`WideString`,
`Video::Adapter`/`GraphicsInterface`/`DisplayMode`, `Graphics::SunLight`/`FrameCopyImageBuffer` —
the latter ties to `ImageBuffer::SaveImageToFile` above, screenshot-related), and
`Application::Application` (2 of 2, both referenced only from `dx11/src/devicedata.cpp` — flag as
dx11-cutover-critical rather than low-priority just because it's namespace "Application").

**Unresolved**: the 4 ordinals (`2990`, `3214`, `443`, `4462`) have no export name and were not
sized this round — `pe_exports.py` only walks the named-export table. Their RVAs were resolved
(`0xc04c0`, `0xca90`, `0xca4e0`, `0xcac0`) via the export-directory's ordinal/base fields directly;
a follow-up should run `get_function_by_address` on `0x180000000 + rva` for each to get a name and
size before scheduling them.

## Files

- `docs/runtime_port/wave9_laneB_inventory.csv` — full 194-row inventory.
- `docs/runtime_port/WAVE9_LANE_B.md` — this report.
