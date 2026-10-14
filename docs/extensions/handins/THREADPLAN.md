# THREADPLAN -- the multithreading session (opened 2026-10-14)

The coordinator's plan for the work the user approved in `THREADRESEARCH.md`'s Decision section: rank 1 (the pipelined
emit, a render thread) and rank 2 (the parallel cull inside `ext.draw_gather`'s walk, a 6-8-core feature). Ranks 3 and 4,
the full decoupling and the snapshot walk are rejected and not revisited here. This sheet holds the decisions taken on
the research's eight open questions, the block list with its order and gates, and the measurement protocol. Progress goes
to `docs/extensions/STATUS.md`; each block's facts go to its own hand-in beside this file.

## 1. Decisions on THREADRESEARCH §6 (coordinator's defaults, 2026-10-14; the user may overrule any row)

| # | question | decision |
|---|---|---|
| 1 | rank 1's class | **A**, live toggle, drained at a Frame boundary. It is presentation-only and sim-exact (§1.6); a per-game lock would cost the player a reload for nothing |
| 2 | one frame of extra latency | **accepted** at depth 1 (0.6-1.7 ms here, ~1-8 ms on a 4-core part). RENDERPIPE-1 reports the measured hand-off latency; the registry text says "one frame of display latency" so the player knows |
| 3 | the speed-50 step hitch | **out of scope** (both schemes rejected by the user) |
| 4 | thread counts as settings | **derived, no numeric keys.** The registry stays bools (`ext.render_pipe`, `ext.gather_threads`); every count comes from the §7.4 rules in one header (`src/ext/cpu/roster.h`). The ONE new settings key is `sys.cpu_limit` (a measurement aid, not a feature) |
| 5 | path workers on 4 cores | **agreed**: count from the affinity mask (physical cores), `clamp(n - 2 - (render_pipe ? 1 : 0), 1, 4)`, BELOW_NORMAL priority. The pool is already class B (timing of path results); the rule changes only when results land |
| 6 | logical vs physical | **physical cores inside the process mask** (`GetLogicalProcessorInformationEx` on Windows; `sched_getaffinity` + `/sys/devices/system/cpu/cpuN/topology/core_id` on Linux); fallback = popcount of the mask when the topology query fails. A helper never shares a core with the main or render thread by the count alone |
| 7 | a ThreadSanitizer Linux flavour | **deferred**: the hypervisor is off (WSL2 unavailable this session); recorded as a follow-up in NEXT_SESSION when the session closes |
| 8 | GPU headroom | the **speed-50 CPU-side gain is the headline**; the speed-10 column is reported with "GPU permitting". One RenderDoc pass-timing capture only if time is left after the A/B |

## 2. Blocks, in order

| block | what | depends on | agent | gate |
|---|---|---|---|---|
| **CPULIMIT** | `sys.cpu_limit` (settings key -> process affinity mask at boot, before any thread), `src/ext/cpu/` (topology: logical / physical cores inside the mask; the roster rules of THREADRESEARCH §7.4 as pure functions), path workers count from the roster + BELOW_NORMAL, the mask / core count in the `[BootState]` line and the stress report header, unit tests of the rules | -- | opus, worktree | full gate (resave golden unchanged: the key writes no state) |
| **RENDERPIPE-0** | the split of `GameUpdateLoop::Draw` + `Renderer::Draw` into pass 1 (main) and the packet (pass 2 + post-scene + Present), run SINGLE-THREADED in the pipelined order (pass 1 of N+1, then the packet of N; drain before a step and at every §1.5 transition), two Renderer instances, the main-owned frame index, the recorded context writes, the step-only `SunLight::Set` / `UpdateCamera` rule, the COMPPROF=4 shadows `RenderPipe` / `RenderPipeFrame` / `RenderPipeIdem`, the harness draw column split into main ms / packet ms, registry row `ext.render_pipe` (A, default OFF) | -- (runs beside CPULIMIT; disjoint files) | opus, worktree | full gate; then the coordinator's shadow runs (heavy + compact, speed 10 + 50) at 0 and a uProf pass at COMPPROF=0 for the pass-1 share `a` |
| **CULLPAR** | rank 2: `ext.gather_threads` (A, default OFF; implies `draw_gather`), subtree tasks concatenated in DFS order, spinning helpers woken once per frame, side array pre-sized, per-task counters, helper count from `Cpu::CullHelpers` (0 on 4 cores), unit test of the walk on random grids with 1-3 helpers, the `DrawGather` shadows unchanged | CPULIMIT merged (the roster header) | opus, worktree | full gate; shadows 0; A/B at the 8-CPU mask (0 / 1 / 2 helpers) and the P4 mask (confirms 0) |
| **RENDERPIPE-1** | the render thread: the packet runs on one event-parked thread, the fence before every step, the hand-off wait, the drains (resize, device-lost re-entry, PaintTerrain, loop change, toggle off, eye camera), the device lock moving with the packet, the path-worker rule with `render_pipe` on, the hand-off wait counter and latency in the stress report | RENDERPIPE-0 gated with shadows 0; CPULIMIT merged | opus, worktree | full gate; the shadows with the thread on; the A/B of §4 |
| **A/B + close** | the §4 series, the hand-ins' Results sections, STATUS, NEXT_SESSION, memory | everything above | coordinator | -- |

Two agents build at once at most (`--jobs 2` each); the coordinator alone runs the game, the shadows, the gates and the
A/Bs (`feedback_parallel_port_agent_hygiene`, `feedback_one_game_run_at_a_time`).

## 3. Rules every block follows (in the brief, because agents do not see CLAUDE.md)

- The faithful path is untouched: an ext arm at an existing seam or a null-by-default static pointer seam (DESIGN §3 hook
  rule 5); object proof per touched faithful TU by WALKGRID §3's objdump method (build perf at the base first, keep the
  objects, compare after). One null test per Frame is the allowed residue.
- No toggle read in a hot loop; no function-pointer tables, no virtual hooks.
- No null guard on a faithful crash; no `getenv` (settings through `IO::Persistence`, toggles through `Ext::Ext()`); no
  `BANISHED_*` flag; no doors (`tools/acceptance/acceptance.sh` at target).
- Static gate only in the worktree: `tools/build.sh build debug / perf` and `tools/build.sh unit debug` at `--jobs 2`,
  0 new warnings against `docs/runtime_port/phase_d/runs/warn_baseline_{debug,perf}.txt` (gate.sh's `norm()` shape),
  `tools/castmetric.py` at 2, `tools/abi_audit.py` OK. Never run the game; never `taskkill /IM`; never `git stash`,
  amend, rebase or push; no sub-agents; merge main before the final build.
- Commit titles `ext:` / `perf:` / `build:` + what; the body names files, the object-proof verdicts, what was run.
  Hand-in at `docs/extensions/handins/<BLOCK>.md`: design as built, deviations from THREADRESEARCH, the proofs, open
  items for the coordinator.

## 4. Measurement protocol (coordinator)

- **Before any fps number:** Afterburner stopped by PID first, then RTSS, then RTSSHooksLoader64 (`tasklist | grep -i rtss`
  empty); the mods parked in `_mods_off` before any gate; the hypervisor is OFF already; `machine load: CLEAN` in every
  report (no agent building or testing: A/Bs are sequenced between blocks).
- **Masks** (`sys.cpu_limit` in `settings.cfg`, printed by the exe): P4 = four physical cores, one thread each (the CPU ids
  from the topology print, not assumed); S4 = two physical cores with both SMT threads; unrestricted. 9800X3D: one CCD, the
  L3 is shared under every mask; the clock, L3 size and GPU are stated beside every masked number as what the mask cannot
  emulate (THREADRESEARCH §7.3).
- **Legs** (`tools/stress_run.sh`, interleaved x2): faithful vs faithful + `render_pipe`; everything on vs everything on +
  `render_pipe`; heavy and compact towns; speed 10 and 50; at P4, S4, unrestricted. Read fps, main-thread draw ms, packet /
  render ms, hand-off waits per frame, and sim ms per iteration (must not move). CULLPAR adds helpers 0 / 1 / 2 at the 8-CPU
  mask and confirms 0 at P4.
- **Shadows** (`BANISHED_COMPPROF=4`): `RenderPipe` (hashes at hand-off vs after the packet), `RenderPipeFrame` (frame
  index, alpha, shader detail equal the sequential order's), `RenderPipeIdem` (the skipped `SunLight::Set` / `UpdateCamera`
  would have written the same bytes); every existing shadow with the rows on. A nonzero shadow is a finding, never noise.
- **Exactness:** the resave golden `971aaec2cf95` unchanged with every row (class A writes nothing); `worldcmp.sh` unchanged.
