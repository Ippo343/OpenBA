# EXECSELF -- where `ActionStack::Execute`'s self time goes (2026-10-12)

Research block. Outcome: **reasoned stop, no code change.** The self time is two cache
misses on the pointer chase CharacterComponent -> action array -> Action node; the
instructions themselves are already the minimum, the "three walks with `_status` stores"
of ELPROF section 6 item 4 do not happen in the steady state, and the one store that does
happen (before a precheck) is observable by the callee. The lever is memory latency
(a software prefetch in the tick walk), not a restructuring of the function.

## 1. Data

- Sampler file and exe: the ELPROF second run on the -O2 perf exe of `7f77a506a`
  (`scratchpad/elprof2/`, copied, originals untouched). Tag 4 (EntityList tree phase):
  **12,486 samples**. The perf exe carries DWARF (`.debug_line` present), so `addr2line -i`
  maps the leaf addresses to source lines.
- `ActionStack::Execute` = `0x14002c3d0 .. 0x14002c570` (next symbol
  `CharacterComponent_MoveTick_1094c0`). Leaf samples inside: **1,401 = 11.2% of the phase**
  (ELPROF: 10.9-11.6%).
- The sampler (`src/diag/win/sampler.cpp`) suspends the thread and reads RIP, so a sample
  lands on the instruction AFTER the one that stalled retirement (the usual one-instruction
  skid): the instruction before a hot address is the culprit. Every hot address below has
  zero samples on the load itself and the bulk on its successor.

## 2. Instruction-level attribution (leaf samples inside Execute)

| sample addr | stalled instruction | source | samples | % of Execute | % of phase |
|---|---|---|---|---|---|
| `0x14002c425` | `0x14002c421 cmpb $0x0,0x18(%rbx)` -- first touch of the Action node | `citizenai_executor.cpp:158` `if (!self->Get()->_status)` | 1,059 | 75.6% | 8.5% |
| `0x14002c3eb` | `0x14002c3e7 mov (%rax,%rcx,8),%rbx` -- load of `actions[current]` from the heap array | `:152` / `Internal.h:272` `actions[iVar1].Get()` | 203 | 14.5% | 1.6% |
| `0x14002c3d1`, `0x14002c3da` | prologue (`push`, `mov %rcx,%rsi`); `current` itself is a cache hit (MoveTick just read it) | `:146` | 52 | 3.7% | 0.4% |
| `0x14002c440` | `0x14002c43d mov (%rbx),%rax` -- the vptr load (same cache line as `_status`, already in) | `:104` CallMain | 42 | 3.0% | 0.3% |
| `0x14002c55f..562` | epilogue `pop`/`ret` | `:212` | 27 | 1.9% | 0.2% |
| `0x14002c434..44f` | main loop: `_next` load, call return, `cmp $1` | `:175-176` | 16 | 1.1% | 0.1% |
| `0x14002c4e3`, `0x14002c553` | pop phase / tail test | `:189`, `:211` | 2 | 0.1% | -- |
| precheck loop `0x14002c481..496` | `movb $0x1,0x18(%rbx)`, `call *0x28(%rax)` | `:162-165` | **0** | 0% | 0% |

**90.1% of the self time is the two loads that miss** (the node line 75.6%, the array slot
14.5%). Everything a restructuring could touch -- prologue, epilogue, the vptr load, the loop
control -- is 137 samples, 1.1% of the phase, and most of it is call overhead.

The compiled steady-state path (one entry, one node, `_status` already set, main returns 1)
is 22 instructions: load `current`, load `actions._data`, load the slot, two tests, the
`_status` byte test, vptr load, three argument moves, `call *0x30(%rax)`, `cmp $1`, the
`_next` load and test, re-read `current`, sign test, epilogue. `InternalPtr::Get()` is
already a single load (CheckPtr is a bare RET, inlined away), `CallMain` is a plain virtual
call, and there are no `_status` stores on this path.

Where the 1,364 inclusive samples of the main call go (callee under `0x14002c44c`):
`MoveTo::Execute` 1,284, `PlayAnimation::Execute` 65, `PerformWork::Execute` 13, other 2.
Precheck (`0x14002c491`) 69 inclusive, exit (`0x14002c506`) 5, the pop's DeletePtr 11,
`Clear()` 10.

The same chase continues into the callee: `MoveTo::Execute` (now 9.4% self) has 184
samples (1.5% of the phase) on its first two instructions (the indirect-call redirect /
first fetch), 58 + 45 on the loads of `this+0x28` (the CharacterComponent pointer) and
`+0x60`, and 276 on `PerfX::TerrainGet` (the map cell of the next step) -- also misses.

## 3. Answers

### (a) Which lines / instructions carry the self time

The first load from the Action node (`_status` at node+0x18, line 158) -- 75.6% -- and the
load of the running entry from the action array (line 152) -- 14.5%. Not the `_status`
stores (none execute in the steady state; the precheck loop has 0 leaf samples), not the
`Get()` chains (one load each, and `_next` is in the node's already-fetched line: 5
samples), not the vtable loads (42 samples; the vptr shares the `_status` line), not branch
mispredicts on the phase structure (they would put samples on the branch targets
`0x14002c43d` / `0x14002c481`; those have 2 and 0). It is cache misses on the per-citizen
objects: ELPROF's 2,630 entities per iteration touch several MB per pass, so every
citizen's CharacterComponent, its 128-byte action array and its Action node (a `MoveTo` is
0x480 bytes) have been evicted from L1/L2 by the time the walk comes back to it on the
next iteration.

### (b) How many nodes, how often each phase runs

- **Entries per stack**: the executor only ever runs the top entry (`actions[current]`);
  lower entries are reached only after a pop. The array is allocated by the first push
  (16 slots, doubled when full; `ActionStackPush` 0x1400c54e0).
- **Nodes per entry**: one. The only writer of `Action::_next` in the game is
  `work_act_hunt.cpp:176` (the hunt chains a second action); every other push stores a
  single node. Consistent with the profile: the `_next` reload/test at `0x14002c430` has 5
  samples against 1,059 on the first node.
- **Precheck**: runs only when the top node's `_status` is clear, i.e. on the first
  Execute after a push (a new node starts with `_status = false`) and after a pop or a
  push-over exposes an entry whose chain `ActionStackWalkClear` cleared. Once per action
  lifetime, plus once per resume. Sampled: 0 leaf samples in the loop, 69 inclusive
  against 1,364 for main.
- **Main**: once per node per Execute -- every sim iteration, every citizen with a
  non-empty stack (MoveTick calls Execute only when `current >= 0`).
- **Pop**: only when main (or precheck) returns 0, i.e. once per finished action; 1 leaf
  sample.
- The outer do/while repeats only after a pop or a precheck/main return other than 0/1/2
  (no break flag); in the steady state it runs once.

(No game instrumentation: derived from the code and the leaf/inclusive split above.)

### (c) Is there a byte-identical restructuring with less work? No.

1. **There is no redundant work on the hot path to remove.** The ELPROF note ("re-walks the
   node list three times per iteration and stores `_status` on every node each time") is
   not what the function does in the steady state: the `_status` byte test skips the
   precheck walk, the pop walk runs only on a 0 return, and no store executes. Skipping
   the `_status` test would not save the miss either: the next instruction loads the vptr
   from the same cache line.
2. **The `_status = true` store before each precheck is observable by the callee**, so it
   can be neither dropped nor moved after the call. A precheck (or main) that pushes a
   sub-action goes through `ActionStackPush` -> `ActionStackWalkClear` (0x1400c4f10,
   `action_pf_ctors.cpp`) on the running entry, which is the node being prechecked: it
   tests `_status`, clears it and calls the node's `Exit` (slot 7) only if it is set.
   Other readers of `_status`: `ActionStack::UpdateAnimation` (0x1400c4f80, slot 6 only
   when set), `CitizenAI_ActionFlush_c4bb0` (sets it), the walk-clear copies in
   `work_act_clear.cpp:121` and `work_act_consume.cpp:134`, and `Action::Persist` (it is
   serialized). So `_status` is retail-visible state, and in any case the store sits on
   the cold path (0 samples).
3. **`current` must be re-read after every call** (main can push, pop or `Clear()`), and
   the pop walk's `_status` test decides the `Exit` calls -- both already minimal.
4. The only non-miss saving available is compiler-level: inlining `Execute` into its one
   hot caller would remove the prologue/epilogue/tail-call (~79 samples, 0.6% of the
   phase). Not worth a change on its own.

So the restructuring item (ELPROF 6.4) is closed: there is nothing to restructure. No unit
test was written because no new body exists to compare against retail's.

## 4. The memory-latency lever (what would help, not built)

The miss is inherent to the data: walk entity -> component -> `actions._data[current]` ->
node -> callee fields. Removing it from `Execute` alone would only move it into
`MoveTo::Execute`, which touches the same line. What hides it is issuing the loads earlier.

- **Layout changes are not available.** `Action`, `ActionStack` and `CharacterComponent` are
  pinned retail layouts; each node is a separate `InternalMemAlloc` at push time (`MoveTo`
  0x480 bytes, 8-byte aligned), freed on pop, so allocation order bears no relation to walk
  order -- a per-class arena would make nodes dense but not in walk order. A dense side
  array of "top node per citizen" would remove the array miss (14.5% of Execute, 1.6% of
  the phase) but needs invalidation on every push / pop / Clear / Persist: real risk for
  little gain.
- **Software prefetch in the tick walk (recommended next step, PerfX, behaviour-free).**
  `prefetch` has no architectural effect, so it needs no shadow check -- only an A/B. In the
  WALKCACHE `TickList::Cached` loop the next candidate components are known one entity
  ahead. A staged prefetch for CharacterComponent candidates: at entity i prefetch the
  component of entity i+2; at entity i+1 (its line now arriving) read `actions._data` and
  `current` and prefetch the slot; at the tick of entity i+2 read the slot and prefetch the
  node (`+0x00` line, plus `+0x40` for MoveTo's `+0x60` fields). Upper bound: Execute's miss
  share (90% of 11.2% = ~10% of the tree phase) plus part of MoveTo's node-field and
  TickComponents' own component-load misses; realistic expectation a few percent of the
  tree phase. It is a change to the walk (WALKCACHE's code), not to this function, and must
  be measured on a CLEAN machine (perf build, heavy town, sampler + COMPPROF=2), so it is
  left as the user's call.

## 5. Files

- `docs/extensions/handins/EXECSELF.md` (this file); `docs/extensions/STATUS.md` (entry).
- Analysis script (scratchpad, not committed): `execself_scripts/hot.py` -- leaf histogram
  over an address range, `addr2line -i` + `objdump` around each hot address.
- No source change; `src/game/Component/citizenai_executor.cpp` is untouched.
