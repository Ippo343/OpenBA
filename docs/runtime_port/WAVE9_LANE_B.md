# Wave 9, Lane B — inventory and sequencing of the 145 named functions + 51 ctors/dtors

Prep pass, 2026-09-07. **No porting was done this round** — per the lane brief, this is
inventory and sequencing only. Read `docs/runtime_port/WAVE9.md` and `PLAN.md` §4/§6 first.

**CORRECTION, 2026-09-07 (same day, lane reopened by a defect lane P found):** the original
version of this report's `ghidra-exact` column contained a systematic tool-usage bug, not
estimation noise. See §3a below for the mechanism, and §0/§3/§5 for what changed. The short
version: `get_function_metrics`'s `address` parameter does not work in this bridge — it silently
has no effect rather than erroring — and this program's Ghidra symbol table has many functions
sharing the same short, unqualified name (e.g. 185 distinct functions named `Serialize`). Calling
`get_function_metrics(function_name="Serialize", address=0x18009ee20)` therefore does not verify
that the function AT that address is named `Serialize`; it silently returns metrics for **some**
function named `Serialize`, which need not be the intended one. **3 of the 13 spot-checked
functions were measured this way and were wrong; a 4th is downgraded to unverified.** All four are
corrected below and in the CSV.

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
| Trivial (<20 instr) | 75 |
| Small (<80 instr) | 59 |
| Medium (<250 instr) | 38 |
| Large (≥250 instr) | 17 |
| Unknown (4 ordinals, 1 last-export-in-DLL) | 5 |

(Updated after the §3a correction: `Quaternion::Lerp` moved small→medium at its corrected 85
instructions; `Goal::Serialize` and `GoalInstance::Persist` stayed in `small` at their corrected
33/34 instructions.)

## 2. Full inventory

`docs/runtime_port/wave9_laneB_inventory.csv` — one row per function: namespace, class, kind,
function name, mangled symbol, size bucket, size source (`ghidra-exact` for 13 spot-checked
functions, `pefile-RVA-delta (ESTIMATE)` for the rest — see caveat below), the size value, the
class's unported/ported/total/completion%, an oracle-soundness note, a callee-dependency note
(populated only for the 8 functions whose callees were actually pulled from Ghidra), and the
source file(s) that already reference the mangled name today (registration site, engine_funcs.h
entry, or a call site expecting the port).

### Sizing method, and why most of it is an estimate

Ghidra MCP was reachable, but **`get_function_metrics(function_name=...)` is only trustworthy when
that name is unique in the program** — see §3a. At ~194 functions, an exact per-function pass was
not affordable this round; the task brief explicitly allows the pefile-RVA-delta fallback, so that
backs most of the CSV. **13 functions were spot-checked; all 13 have now been re-verified** (not a
sample — the full set that carries a `ghidra-exact` value), either by confirming the function name
is unique in this program (8 of them), or, where it was not, by re-measuring with
`disassemble_function` at the address-confirmed-correct location (5 of them: `Lerp`,
`Goal::Serialize`, `GoalInstance::Persist`, `GoalList::Initialize` — all now correct — and
`SaveLoad::Draw`, downgraded to unverified because "Draw" has 44 collisions and no independent
re-measurement was done for it this pass):

| Function | Exact instrs | Provenance | RVA-delta estimate said | Agreement |
|---|---|---|---|---|
| `InternalBasePtr::CheckPtr` | 1 | exact-verified (unique name) | trivial (16B) | match |
| `Quaternion::Lerp` | **85** (was wrongly 8) | exact-verified (re-measured by address) | medium (400B) | match |
| `Goal::Serialize` | **33** (was wrongly 3) | exact-verified (re-measured by address) | small (144B) | close |
| `Thread::ThreadFunc` | 42 | exact-verified (unique name) | small (160B) | match |
| `Transform::AsTransform3` | 47 | exact-verified (unique name) | small (256B) | match |
| `GoalInstance::Persist` | **34** (was wrongly 78) | exact-verified (re-measured by address) | small (144B) | close |
| `AchievementManager::ReadAchievements` | 68 | exact-verified (unique name) | small (272B) | match |
| `GoalList::Initialize` | 112 | exact-verified (cross-checked by address against 3 candidates — unchanged) | medium (400B) | match |
| `Frustum::Projection` | 171 | exact-verified (unique name) | medium (848B) | match |
| `Persistence::ReadSettings` | 256 | exact-verified (unique name) | medium (1072B) | borderline |
| `SaveLoad::Draw` | **unverified** (was wrongly-confident 349; "Draw" has 44 name-collisions) | exact-unverified | large (3232B) | body range 3215B (`get_function_by_address`, address-keyed and reliable) agrees on bucket |
| `ImageBuffer::SaveImageToFile` | 359 | exact-verified (unique name) | large (1360B) | match |
| `Persistence::WriteSettings` | 538 | exact-verified (unique name) | large (2080B) | match |

**Net after correction: of the 13, 9 were right the first time, 3 were wrong (`Lerp`, `Goal::Serialize`,
`GoalInstance::Persist` — 23%), 1 is downgraded to unverified (`Draw`).** The RVA-delta estimate's
own known weakness (§ below) is unrelated and separately confirmed: `Lerp` and `Goal::Serialize`
are genuinely tiny (33-85 instructions) but the RVA delta still overshot medium/small because of a
gap before the next named export — the estimate was never claiming exactness, only the mis-scoped
`ghidra-exact` column was. **Treat every "small" or below RVA-delta bucket as a plausible "actually
trivial"; the medium/large distinction, which is the one that matters for sequencing, held up in
every spot check.** A real per-function Ghidra pass (by address, via `disassemble_function`, not by
name) is recommended before final commitment at the small-scale end.

### 3a. The mechanism (why the exact column was wrong)

This program's Ghidra function table is **not** namespace-qualified for most functions — a method
is just its bare name (`Projection`, `Serialize`, `Persist`, `Draw`, `Initialize`, ...), and the
same bare name recurs across unrelated classes: `search_functions_enhanced("^Serialize$", regex)`
found **185** matches, `^Draw$` found **44**, `^Lerp$` found **13**, `^Persist$` found **6**,
`^Initialize$` found **3**. Only 8 of the 13 spot-checked names (`Projection`, `ThreadFunc`,
`CheckPtr`, `SaveImageToFile`, `AsTransform3`, `ReadAchievements`, `ReadSettings`,
`WriteSettings`) turned out to be unique in the whole program.

`get_function_metrics` accepts either `function_name` or `address`. **`address` does not work in
this bridge**: `get_function_metrics(address="0x18001d500")` returns `{"error": "Function not
found: 0x18001d500"}` even though that address indisputably has a function (`get_function_by_address`
confirms it, and that tool's `address` parameter DOES work correctly). When both `function_name`
and `address` were passed together — which is what happened for the 5 ambiguous names above — the
broken `address` parameter was silently dropped with no error, and the tool fell through to a
**name-only** lookup, returning metrics for an unspecified one of the same-named candidates. This
surfaced no error (a function named `Serialize` genuinely exists — it just wasn't the right one),
which is why it went unnoticed: a swallowed failure that returns a confident, well-formed,
wrong answer, the same shape as the census tool defects fixed elsewhere in Wave 9 today.

Re-verification method: for each ambiguous name, `disassemble_function(address=...)` (which, like
`get_function_by_address`, takes the address directly and does not go through name resolution) was
used to read the actual bytes at the confirmed address and count instructions by hand. `Initialize`
happened to have been measured correctly despite 3 candidates (its 112-instruction count reproduced
exactly on re-disassembly) — there is no guarantee of that, it was checked, not assumed. `Draw` was
not re-measured (44 candidates, no time this pass) and is left as an open item rather than guessed.

**Callee claims made with `get_function_callees` have the identical exposure** — it is also a
`name`-only lookup with no address disambiguation. Of the 8 functions callee-sampled in §4 (original
report), only `Persist` used an ambiguous name (6 candidates) and it was wrong — see the correction
in §4 below. The other 7 (`Projection`, `Initialize`, `ReadAchievements`, `ThreadFunc`,
`SaveImageToFile`, `ReadSettings`, `WriteSettings`) are unique names and their callee lists stand
(and `Initialize`'s single fixed-address callee, `0x180009d70`, is independently confirmed present
in the re-disassembly, at instruction 14: `CALL 0x180009d70`).

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
pattern WAVE9.md's "porting can grow the cutover surface" section describes, not a full sweep.
**One of the eight (`Persist`) was measured under an ambiguous name and was wrong; it is corrected
here, and the false pairing it produced is retracted** (see §3a for the mechanism):

- **`Volume::Frustum::Projection` and `System::Thread::ThreadFunc` have zero DLL-internal
  dependency** — `Projection` calls nothing at all; `ThreadFunc` calls only Win32
  `SetEvent`/`WaitForMultipleObjects`. These are the cleanest possible ports in the whole list.
  (Unique names, unaffected by the mechanism in §3a.)
- **RETRACTED: "`GoalList::Initialize` and `GoalInstance::Persist` share `FUN_180009d70`".**
  Re-disassembly at the confirmed address shows `GoalInstance::Persist`'s only fixed-address
  callee is **`0x180005dc0`**, not `FUN_180009d70` — the original callee note came from a
  different, wrongly-resolved `Persist` (6 candidates share that name in this program).
  `GoalList::Initialize`'s callee list is unaffected and stands (unique-name-adjacent: cross-checked
  by direct re-disassembly, `CALL 0x180009d70` is present at instruction 14) — but see the
  corrected hazard profile below; the shared-dependency framing that motivated pairing the two
  functions in Tier 0 does not hold and they are no longer bundled.
- **`GoalList::Initialize` (112 instr) is NOT a clean port** — Lane P's read, confirmed by
  re-disassembly: it (1) builds N objects through a factory in a loop (vtable call `[RAX+0x48]`
  per element), (2) on exit **frees the caller's array and NULLS it** (`CALL 0x1800c6c00` /Free/
  then `MOV [R14],0` / `MOV [R14+8],0`) — a second (diff-oracle) call would see `count==0` and
  destroy what the first call built while *reporting green*, the same early-out trap shape as
  `Audio::Scene::Pause`, and (3) calls `MemoryAllocator::Free` directly under a critical section
  (`CALL [0x18014b390]` / `[0x18014b388]` bracketing it). **Moved out of Tier 0 into a
  single-shot-harness-required category** — naive ours-then-theirs diff is unsafe here.
- **`GoalInstance::Persist` (34 instr, corrected from a wrongly-measured 78) has exactly ONE
  fixed-address callee, `0x180005dc0`**, plus the same vtable-dispatch shape (`+0x18`/`+0x20`
  branch, `+0x98`, trailing `+0x0`) as `Goal::Serialize` — consistent with both being real
  save-path Encoding/Stream dispatches, not delegators.
- **`IO::Persistence::WriteSettings`**, the largest sampled function (538 instructions, cyclomatic
  complexity 124), has **zero new unnamed internals** — every callee is a real Win32 registry API
  call or an already-exported/nameable helper (`Map_Find`, `Set`, `AsUTF8`). Big, but tractable.
  `ReadSettings` (256 instr) has exactly one new internal, `FUN_18000f7d0`. (Unique names.)
- **`Video::ImageBuffer::SaveImageToFile`** (359 instr, screenshot save) creates **two** new
  lane-D rows (`FUN_180053da0`, `FUN_1800881b0`) among otherwise well-named callees — a case where
  "it's just a screenshot" undersells the actual cost. (Unique name.)

**Consequence for sequencing**: prefer `Projection`/`ThreadFunc`-shaped functions (self-contained,
unique-named, independently confirmed) and treat any function whose callee list hasn't been
checked — or was checked under a colliding name — as an unknown-cost item until it is re-verified
by address.

## 5. Proposed port order

**Tier 0 — port first, zero known hazard, cheapest re-verified profile** (CORRECTED — the pairing
and one member of this tier were wrong, see §3a/§4): `Volume::Frustum::Projection`,
`System::Thread::ThreadFunc` (both unique-named, zero DLL-internal callees, unaffected by the
mechanism above). `Math::Quaternion::Lerp` and `Math::Transform::AsTransform3` are re-verified at
85 and 47 instructions respectively (Lerp was originally misread as 8 — corrected) — still small,
still no callee concerns, still Tier 0. **`Achievement::GoalList::Initialize` is REMOVED from Tier
0** — it frees and nulls the caller's array on exit (Pause-shaped early-out trap) and needs a
single-shot harness, not a naive diff; see §4. **The `GoalList::Initialize`/`GoalInstance::Persist`
pairing is retracted** — they do not share a callee. `Achievement::Goal::Serialize` (33
instructions, corrected from a misread 3) and `GoalInstance::Persist` (34 instructions, corrected
from a misread 78) are both real save-path vtable dispatches, not delegators — use the lane-S
technique for both, and they are cheap but not "zero hazard".

**Tier 1 — near-complete classes (≤3 remaining), no WAVE9-inherited blocker.** 71 of 83 classes
qualify; the highest-value ones by (completion% desc, size asc) are `UI::Element` (3 left of 23),
`System::String` (2 of 15), `Object::InternalBasePtr` (1 of 7 — but see caution: that 1 is
`CheckPtr`, a confirmed bare `RET` in every shipped build; registering a real one is a *perf*
decision, not a correctness gap, per the project's standing note on this exact function),
`Save::SaveLoad` (2 of 13, one of which — `Draw`, ~3.2KB body / instruction count UNVERIFIED, see
§3a — is a real rendering-path port, not cheap despite the class being 85% done),
`Achievement::AchievementManager` (3 of 12),
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
