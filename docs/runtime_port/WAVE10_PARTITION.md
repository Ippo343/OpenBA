# Wave 10 — the remaining surface, scoped and partitioned

Written 2026-09-08 to replace **one-wall-at-a-time** with a plan. The cutover boot
(attempt 23) tells you the *next* wall and nothing about the *tenth*; this document
scopes the whole surface so lanes can run in parallel tomorrow instead of queueing
behind a single fault address.

Regenerate everything here with:

```
python tools/mint_wall_predict.py .            # predicted vtable traps
python tools/wave10_partition.py <walls> <residue>
python tools/runtime_census.py                 # DLL-present coverage
```

Archived output: `evidence/wave10_scope/`.

---

## 1. Three screens, deliberately NOT unioned

A union would read as one confident number and hide that each screen is blind to
something different ([[reference_union_control_hides_a_blind_screen]]).

| screen | what it measures | bound |
|---|---|---|
| **A. Predicted vtable walls** — `mint_wall_predict.py` | RT_MINT_NAMED rows with neither name nor address registered: slots that can only ever trap | **UPPER** — 102 |
| **B. Observed residue** — attempt 23's `[RtUnresolved]` | what the boot actually asked for and did not get | **LOWER** — 25 |
| **C. Census gap** — `runtime_census.py`, `called − ported` | functions our tree already resolves that still fall through to the DLL | ~238, both-ways lossy |

**Why each is only a bound:**
- **A over-scopes.** Registrations spelled through a variable or named constant are
  invisible to a literal scan, so a flagged row may already be registered. The tool
  says so itself. A row it *clears* is genuinely clear.
- **B under-scopes, badly.** It only names what this boot reached before faulting.
  Everything past the first fault is unobserved. **This is the whole reason not to
  plan by re-running the boot**: the residue can never enumerate work it hasn't
  reached, so iterating on it is a sequential walk with no map.
- **C is lossy in both directions.** It undercounts ports (40 gated registrations
  excluded; non-literal registrations invisible — the documented blind spots) and
  it only counts functions our tree *already touches*, so untouched work is absent.

Read together: **A−B is the backlog, A∩B is what to do first, and C says which
namespace the debt lives in.**

## 2. Where the debt is (screen C)

`called − ported`, i.e. code we already reach that still needs the DLL:

| namespace | called | ported | gap |
|---|---|---|---|
| Video | 148 | 108 | **40** |
| UI | 161 | 130 | **31** |
| Graphics | 97 | 66 | **31** |
| Object | 54 | 30 | **24** |
| System | 97 | 75 | 22 |
| IO | 104 | 83 | 21 |
| internal | 126 | 110 | 16 |
| Achievement | 52 | 36 | 16 |
| Engine | 33 | 23 | 10 |
| Save | 24 | 16 | 8 |
| Audio | 30 | 23 | 7 |
| ordinal / Input / Application | | | 4 each |

Total 238. Math, Volume, Picking, Utility are at 100% and need no lane.

## 3. The partition — lanes are CLASS FAMILIES, not namespaces

One class family per lane, because that is what makes lanes independent: two lanes
in the same namespace touch the same files, and Wave 9 lost time three separate
times to two lanes porting the same body or colliding on an overload
([[reference_grep_for_the_port_before_writing_it]]). A family owns its files.

`*` = also in the observed residue, i.e. on the boot path.

### Critical path — must land first, in this order

| lane | family | syms | why first |
|---|---|---|---|
| **V1** | `Video::Device::Adapter` (4) + `::GraphicsInterface` (3) + `Device::AdjustDisplay`/`GetDisplayMode` (2) + `Application::GetInstance` (1) | **10** | `*` **This is the current wall.** `DeviceData::EnumerateInterfaces` (DX11 slot 1) forwards to an absent original and returns null → `EXEC of 0`. Nothing else can be observed past it. |

V1 is small and entirely `Video::Device` — one class family, one lane, no shared
files with anything below.

### Parallel wave — independent of each other, all reachable once V1 lands

| lane | family | walls | obs | notes |
|---|---|---|---|---|
| **UI1** | `UI::UserInterface` | 0 | `*`4 | Update/Draw/ShowDialog/CloseDialog. Boot-path; not a minted slot, so screen A is blind to it. |
| **UI2** | `UI::ElementDescription` (3) + `UI::Element` (4) | 7 | `*`2 | Serialize/CreateInstance observed. |
| **UI3** | `UI::DialogController` (7) + `UI::DialogInstance` (1) | 8 | 0 | |
| **UI4** | `UI::SpriteSheet` | 8 | 0 | Self-contained. |
| **GFX1** | `Graphics::MaterialInstance` (7) + `Graphics::MeshInstance` (6) | 13 | `*`2 | Both observed; the instance pair is coupled. |
| **GFX2** | `Graphics::Renderer::Draw` + `Graphics::AnimationManager::Update` | 0 | `*`2 | Boot-path name doors. Screen A cannot see these. |
| **OBJ1** | `Object::External` (2) + `ExternalFactory::FindOrLoad` (1) + `CompiledExternal` (2) + `Object::Base` (1) | 5 | `*`2 | Touches lane RRES2's `external_find.cpp` — **read it first**, its `Find` is already ported. |
| **ENG1** | `Engine::LoaderLoop` (6) + `DialogLoopT` (2) + `DialogLoaderLoopT` (1) + `UpdateLoop::LoadResources` | 9 | `*`1 | The loop family. |
| **AUD1** | `Audio::Scene::Update` | 0 | `*`1 | Boot-path name door. One symbol; fold into another lane only if someone is idle. |

### Backlog — predicted only, not yet observed; schedule after the parallel wave

| lane | family | walls |
|---|---|---|
| **GFX3** | `Graphics::EdgeMesh` (8) + `EdgeMeshInstance` (3) | 11 |
| **GFX4** | `Graphics::ParticleInstance` (7) + `Particle` (4) + `ParticleSystemInstance` (2) + `AnimationEvent` (2) | 15 |
| **GFX5** | `Graphics::SunLight` | 5 |
| **VID2** | `Video::ImageBuffer` | 7 |
| **ACH1** | `Achievement::Achievement` (5) + `AchievementInstance` (3) + `Goal` (3) + `GoalInstance` (2) | 13 |
| **IO1** | `IO::CompressedStream` | 1 |
| **SYS1** | `System::Debug::_disableHandler` | data, `*`1 |

### Completeness check

The partition is **exhaustive and non-overlapping** against both screens, which is
the property that makes it a plan rather than a list:

- **102 / 102** predicted walls assigned to exactly one lane
  (7+8+8+13+5+9+11+15+5+7+13+1).
- **25 / 25** observed residue names assigned (10+4+2+2+2+2+1+1+1).

Re-run the check after editing any lane; the first draft of this table lost
`Audio::Scene::Update` — 24 of 25 — and the arithmetic is what caught it, not
review.

## 4. Rules that make the parallelism safe

These are Wave 9's scar tissue, not general advice.

1. **A private worktree per lane, path AND branch reported.** A shared checkout got
   branches switched underneath lanes and landed commits on the wrong branch
   ([[reference_parallel_lanes_need_private_worktrees]]).
2. **Grep for the port before writing it.** Three Wave 9 lanes re-ported existing
   bodies; a duplicate registration is last-binding-wins and silently makes one
   body dead code. Grep the **address and the mangled name**, not the class string
   ([[reference_half_registered_port_reads_as_absent]]).
3. **"Closed" ≠ "merged".** Verify with `git branch --no-merged`, never from a
   lane's own write-up ([[reference_lane_closed_is_not_lane_merged]]).
4. **Don't chase the `asked by` address.** It names the *resolver*
   (`Dx11ResolveRuntimeImport`), not the caller — confirmed live in attempt 23.
5. **A count is not a set.** Residue going *up* after a port is the normal shape of
   progress: the closed body's callees replace it. Always set-diff against the
   archived file ([[reference_diff_the_set_not_the_count]]).
6. **Screen A clears a row honestly but flags one only provisionally.** Before
   declaring a lane's work, re-check flagged rows for non-literal registration.

## 5. What this plan does NOT cover

Non-virtual exported functions that no minted slot and no executed path reaches are
in **neither** A nor B, and appear in C only if our tree already calls them. That
residual is not enumerated here and is not zero. It is the reason the cutover boot
stays the **gate** even though it is a poor **planner** — keep running it to confirm
the wall moved, and use this document to decide what to build.
