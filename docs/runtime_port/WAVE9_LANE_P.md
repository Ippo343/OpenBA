# Wave 9, Lane P — tier 0 of lane B's inventory

2026-09-07, branch `wave/upgrade-path-complete`, round 2. Read
[WAVE9.md](WAVE9.md), [WAVE9_LANE_B.md](WAVE9_LANE_B.md) and [PLAN.md](PLAN.md) §4/§6 first.

**This lane wrote code and did not build or run anything.** Lane R owns the exe, the game
directory and the object tree this round. Everything below is verified statically:
each file was read out of the shipped `Runtime-x64.dll` in Ghidra
(disassembly, not decompiled C — `reference_ghidra_c_output_vs_disassembly.md`), and the
sources were checked with `g++ -std=c++17 -Wall -Wextra -fsyntax-only` using the Makefile's
own flags and include paths (syntax only, no object files, nothing written into
`standalone/build*`). **Zero warnings across all eight touched translation units.**

## Result in one line

**Six of the seven tier-0 targets ported; one deferred with evidence.** Five of the six land
with a running oracle; **`ThreadFunc` lands with an execution witness and no oracle**, and may
land unverified — §4 of the oracle section is explicit about what would have to come back
empty for that to be the outcome. Two of the six were
Wave 5 holdbacks, not new work. **Three classes are now complete** — `Volume::Frustum`,
`System::Thread`, `Achievement::Goal` — and a fourth, `Achievement::GoalInstance`, is complete
too. **Two new lane-D rows created, both named.**

## Per-function

| Function | Mangled / retail addr | Instrs (read) | Class after | New lane-D rows |
|---|---|---|---|---|
| `Volume::Frustum::Projection` | `?Projection@Frustum@Volume@@QEBA?AVMatrix@Math@@_N@Z` `0x1800f9dc0` | 171 | **COMPLETE 6/6** | none |
| `System::Thread::ThreadFunc` | `?ThreadFunc@Thread@System@@CAIPEAX@Z` `0x1800ca440` | 42 | **COMPLETE 5/5** | none |
| `Math::Quaternion::Lerp` | `?Lerp@Quaternion@Math@@QEBA?AV12@AEBV12@AEBVScalar@2@@Z` `0x18001d500` | **85** (not 8) | 5/5 for the members our tree calls | none |
| `Math::Transform::AsTransform3` | `?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ` `0x18001fcd0` | 47 | 11/11 | none |
| `Achievement::Goal::Serialize` | `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` `0x18009ee20` | **33** (not 3) | **COMPLETE 5/5** | `FUN_1800042a0` |
| `Achievement::GoalInstance::Persist` | `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` `0x18009ef70` | **34** (not 78) | **COMPLETE 7/7** | `FUN_180005dc0` |
| `Achievement::GoalList::Initialize` | `?Initialize@GoalList@Achievement@@QEAAX…@Z` `0x18009f0e0` | 112 | **DEFERRED — 3/4** | (would be `FUN_180009d70`) |

Files: `runtime/src/math/wave9_lane_p.cpp` (new), `runtime/src/achievement/goal_serialize.cpp`
(new), the `Projection` body appended to `runtime/src/volume/frustum.cpp`, the `ThreadFunc`
body appended to `runtime/src/system/thread_win.cpp`. Registrations in
`runtime/src/math/rt_math_port.cpp`, `runtime/src/volume/volume_port_init.cpp`,
`runtime/src/system/thread_win.cpp` and `runtime/src/achievement/rt_achievement_port.cpp`.
No `Makefile` change is needed — `RUNTIME_SRC_SRCS` is a wildcard.

## Five corrections to lane B's CSV, and why they matter

Lane B's inventory is a good document and this is not a complaint about it — it says in its
own §3 that most sizes are estimates. The problem is that four of the rows below are marked
**`ghidra-exact`**, which is the column a downstream lane trusts.

0. **The other three `ghidra-exact` rows this lane touched are exactly right** — `Projection`
   171, `ThreadFunc` 42, `AsTransform3` 47, each confirmed by counting the shipped bytes. So
   the column is not worthless; it is unreliable in a specific place, which is worse, because
   most of it checks out.
1. **`Quaternion::Lerp` is 85 instructions, not 8.** It is a shortest-path *normalised* lerp
   with a `sqrtf` call and eight callee-saved XMM registers — a body that cannot be 8
   instructions on any reading. The CSV also calls it a "trivial one-liner (1 call)".
2. **`Goal::Serialize` is 33 instructions, not 3**, and the CSV's guess that it "likely
   delegates to a shared base Serialize" is wrong: it builds an `IO::Serializer` on its own
   stack, asks the stream's `IO::Encoding` for a codec, drives two field serializers and
   destroys the codec. Ported from the 3-instruction reading it would have been a forwarder
   that emitted nothing — a silent save desync, this project's most expensive failure mode.
   **Note the shape of both errors: 85 → "8", 33 → "3".** The two rows lane B itself flagged
   as "badly overestimated by RVA-delta, trust the exact count instead" are precisely the two
   whose exact count is the real one with its digits truncated. That is a tooling artefact, not
   a judgement error — and `get_function_metrics` refuses both of these addresses outright
   today (`{"error": "Function not found: 0x18001d500"}`), so whatever produced the numbers was
   not a clean lookup. Anything in that column below two digits deserves a second look.
3. **`GoalInstance::Persist` is 34 instructions, not 78, and does not share `FUN_180009d70` with `GoalList::Initialize`.**
   The CSV names three internals for it (`FUN_180006d10`, `FUN_180009d70`, `FUN_180009fa0`);
   the disassembly at `0x18009ef70` calls **none of them**. Its only direct callee is
   `FUN_180005dc0`. The size error is in the other direction from the two above. This matters beyond bookkeeping because *the brief made that shared
   dependency the reason to port the two together* — "that is a measured finding, not a
   convenience". It was not measured against the bytes, and the pairing it justified is not
   the pairing that turned out to be right (the real pair is `Serialize` + `Persist`, which
   share a *shape*, not a callee).
4. **`GoalList::Initialize`'s oracle note is wrong.** The CSV says "Default:
   `BANISHED_RT_DIFF` is sound (no known mutate-twice/queue/callback/save hazard identified
   this pass)". It has all three of the hazards that matter. See the deferral below.

The pattern is the one WAVE9.md already names: *this project's failures are not usually
unknown facts, they are known facts that did not reach the next wave.* An `exact` label that
was not exact is the transport failure in its cheapest possible form.

## The deferral: `GoalList::Initialize`

**Deferred, with evidence, not skipped.** Reading `0x18009f0e0` end to end:

```
FUN_180009d70(this, src->count);              // resize THIS list
for (i = 0; i < this->count; ++i) {           // NOTE: this->count, not src->count
    id   = src->data[i]->vtable[+0x48]()      // the goal's instance type id
    node = BST_find(*(void**)0x1801fc6e8, id) // inlined walk of the Factory tree;
                                              // NO not-found guard — retail derefs 0
    inst = node[+0x10]->vtable[+0x20]()       // CreateInstance
    if (!IsA(inst, *(uint*)0x1801f9940)) inst = nullptr;   // inlined base-chain walk
    this->data[i] = inst;
    inst[+0x18]   = src->data[i];
    inst->vtable[+0x20]()                     // per-instance init
}
if (src->data) {                              // and then it EATS ITS ARGUMENT
    EnterCriticalSection(*(void**)0x1801fc718 + 0x640);
    MemoryAllocator::Free(*(void**)0x1801fc718, src->data);   // 0x1800c6c00
    LeaveCriticalSection(...);
}
src->data = 0; src->count = 0;
```

Three independent reasons a naive `BANISHED_RT_DIFF` ours-then-theirs is unsound here:

* **It creates N real objects through the factory.** Each `CreateInstance` allocates and
  registers a handle-table slot. Running both sides creates 2N and leaks N.
* **It frees the caller's array and nulls it.** The second call therefore sees `src->count == 0`
  and resizes the list to zero — *destroying what the first call built* — while reporting a
  clean comparison, because there is nothing left to differ. This is exactly the
  `Audio::Scene::Pause` trap WAVE9.md row 5-7 describes: an early-out that makes a replay
  compare nothing and report green.
* **It takes the allocator's own critical section and calls `MemoryAllocator::Free` directly**
  (`0x1801fc718 + 0x640`, `0x1800c6c00`), which is Lane E's territory and PLAN §3's "the heap
  cannot be half-ours".

It needs a bespoke oracle — snapshot the source array, run one side, restore, run the other,
compare the produced list *and* the handle table — which is a lane, not a slot. Its one new
lane-D row (`FUN_180009d70`, the `Collection::Array` resize helper) is *not* the reason to
defer; the mutation is.

`Achievement::GoalList` therefore stands at 3 of 4 and is the one tier-0 class this lane did
not close.

## New lane-D rows: two, named

`FUN_1800042a0` (reached by `Goal::Serialize`) and `FUN_180005dc0` (reached by
`GoalInstance::Persist`). They are **two instantiations of the same template body**,
`Object::ExternalBasePtr::Serialize`, for two pointee types — the decompilations are
line-for-line identical apart from the `_typeInfo` compared against. Neither appears in
`wave9_residue_gameplay.txt`, so both are genuinely new rows in the hardest bucket, the same
growth WAVE9.md measured when `KeyMap::RegisterInput` moved the census 823/504 → 825/505.

They are **called by address, not ported** — the same posture `ClearAll`, `RegisterInput` and
`Audio::Scene::Serialize` already take toward `Collection` internals: what is needed from them
is an entry point, not a body.

### Do the ports reach the read path? At runtime YES; under the oracle NO.

This was asked directly and the first answer in this document was wrong, so here it is from the
bytes. **There are TWO `FindOrLoad` call sites in that template body, selected by two unrelated
tests** — an earlier draft here said "on the reading side (Encoding slot `+0x10` returns 3)",
which conflated them and understated the problem:

| # | Guard | What it does |
|---|---|---|
| 1 | `encoding->vtable[+0x10]() == 3` | **Not "reading".** A THIRD mode — the resource-reference VISIT pass (WAVE3.md finding 5, `IO::VisitorStream` / `??_7Visitor@IO@@6B@`). `GetPath` → `FindOrLoad` → `IsDerivedFrom`, **result discarded**; called for the side effect of forcing the resource to load. |
| 2 | `stream->direction == 0`, in the common tail | **The ordinary READING path.** `FindOrLoad` and **stores the result into `*param_1`** — the actual resolution of the reference, on every save load. |

So the honest answer in two halves:

* **At runtime, `Goal::Serialize` and `GoalInstance::Persist` DO reach `FindOrLoad`** — site 2,
  every time a save is loaded. They are one body handling both directions
  (`direction == 0 ? CreateReader : CreateWriter`), so the load path is not optional.
  `FindOrLoad` is on the **plain load path, not an exotic mode**, and whoever ports
  `FUN_1800042a0` / `FUN_180005dc0` inherits WAVE9.md rows 27-28's "no possible oracle"
  **squarely**. That is written into `goal_serialize.cpp`'s banner, where they will meet it.
* **Under the lane-S oracle, neither site is reachable**, which is why the two ports above are
  verifiable and the two internals are not. `BuildSink` pins `stream.direction = 1` (writing)
  and binds all 16 Encoding slots to a thunk returning the stub `ReaderWriter` **pointer** —
  never 3. Both guards fail on both sides of every comparison. Lane S's result rests on the
  write path being a pure read of the object, and that property is intact here.

**The read path of these two ports is therefore UNVERIFIED**, exactly as it is for every other
Serialize in the tree, and no claim above should be read as covering it.

**The one replay hazard on the write path, checked rather than hedged.** That path emits
`ExternalFactory::GetReferenceId(_singleton, ext)` as a uint. If that assigned an id on first
use, a replay would emit N then N+1 and the harness would manufacture a divergence it caused
itself — the shape of the longjmp/destructor defect lane S found in its own prefix stop. It
does not: `?GetReferenceId@ExternalFactory@Object@@QEBAIPEBVExternal@2@@Z` (`0x1800bf100`) is
**`QEBA`, const**, and its body is a pure lookup — build a `Filename`, find it in the map at
`this+0x10`, return `[node+0x14]`, else `-1`. No insert, no counter. **The write path is
idempotent and the replay is sound**, so no prefix stop is needed for either port.

The other four ports create **no** new rows. `Projection` calls nothing at all; `ThreadFunc`
calls only `WaitForMultipleObjects` and `SetEvent` (through the DLL's IAT, i.e. kernel32 —
we call kernel32 directly, which is the same function); `AsTransform3`'s single callee is
Wave 1's own `AsMatrix`; `Lerp`'s single callee is `sqrtf`, which goes through the existing
`RtLibm` seam.

## Census

`tools/runtime_census.py` after this lane's edits: **827 called / 545 ported / 65.9%**, with
`Volume` and `Picking` at **100%** and `Math` at 95.2%. **That absolute number is not this
lane's alone** — lane R and a `runtime/src/collection/` lane have uncommitted edits in the
same tree. This lane's own contribution is **+6 ported, +2 called** (the two `ExternalBasePtr`
internals above), i.e. the same shape WAVE9.md warns about: *`called` is not a denominator
counting down to zero.*

Static audits, all clean, all run after the edits:

```
tools/export_name_audit.py      652 distinct ResolveExport names; 0 are NOT in Runtime-x64.dll
tools/abi_retslot_audit.py      0 suspicious of 48 by-value-returning exports
tools/resolve_fallback_audit.py 163 guarded fallbacks; 0 do NOT point at their own name
tools/port_bypass_audit.py      345 by-address resolve sites; 0 reach a function ported by name
```

The last one is the relevant new check: every one of these six ports is registered by **name
and by address** where an in-tree caller uses the address form, so none of them introduces the
port-bypass defect lane R found in `scene.cpp` (a port reaching another port by address and
walking past it into the DLL).

## Oracles: the command line and the harness path, per function

`calls=` is the column to read, not `divergences=` —
`reference_uncalled_port_is_not_verified.md`.

**Five of the six are diff-able and have a caller a normal harness run reaches. `ThreadFunc`
is neither, and §4 says so rather than rounding it up.** An earlier draft of this section
opened "every port below is diff-able and every one has a caller that a normal harness run
reaches", which was false of `ThreadFunc` in both halves and contradicted its own entry four
paragraphs later. Corrected after the lead flagged the same over-read upstream: lane 0's
residue pass shows `?ThreadFunc@Thread@System@@CAIPEAX@Z` being **resolved**, and *resolved is
not called*. The residue instrument answers "what does the process ASK the DLL for" — the
right question for cutover, where an unresolvable import is fatal whether or not it is
called, and the wrong one for verification.

### 1. `Volume::Frustum::Projection` — `RT_DIFF_BYTES`, 64 bytes

```
BANISHED_RT_DIFF=1 mingw32-make sa-run
```
**Path:** `Graphics::Camera::SetFrustum` (`runtime/src/graphics/renderer.cpp:287/290`) is the
only caller and calls it on every camera frustum change. It is in **both** residue runs
(`wave9_residue_autostart.txt:100`, `wave9_residue_gameplay.txt:111`), so the menu alone
reaches it — `sa-run-menu` suffices, and gameplay exercises both the `swapZUpToYUp` and the
plain branch. Expect a non-trivial `calls=`; a zero would mean the registration missed.

All four arithmetic paths (ortho/perspective × the `zeroToOneDepth` bool) matter, and the bool
comes from `RtVideoDeviceUsesZeroToOneDepth()`, so one run exercises only one value of it. To
cover the other, add `BANISHED_RT_DIFF=1` to a run on the other depth convention — or accept
that half the body is unexercised and say so, which is the honest reading if the device does
not change.

### 2. `Math::Quaternion::Lerp` — `RT_DIFF_BYTES`, 16 bytes

```
BANISHED_RT_DIFF=1 mingw32-make sa-run
# if it diverges, attribute libm rounding before suspecting the port:
BANISHED_RT_DIFF=1 BANISHED_RT_LIBM=dll mingw32-make sa-run
```
**Path:** `Graphics::Geometry::GetInstanceTransform` (`runtime/src/graphics/scene.cpp:1154`),
which runs per interpolated geometry instance per frame — the highest call count of anything
in this lane. `sa-stress` is the volume test.

The second command line is not optional advice: this body ends in `1/sqrt(len)` through
`RtLibm::Sqrt`, and a last-bit libm difference is the one divergence here that would **not**
be a port defect. If the site goes silent under `BANISHED_RT_LIBM=dll`, the difference was
rounding.

### 3. `Math::Transform::AsTransform3` — `RT_DIFF_BYTES`, 48 bytes

```
BANISHED_RT_DIFF=1 mingw32-make sa-run
```
**Path:** four exe call sites through `fn_Transform_AsTransform3`
(`code/Game/Game/gameupdateloop.cpp:1499`, `gameupdateloop_load.cpp:643`,
`geometry_draw.cpp:142`, `Dialog/toolbardialog.cpp:1002`) — the camera-to-world setup and
`Geometry::CollectDraw`. It is the one member of this lane already present in the gameplay
residue **by address** (`wave9_residue_gameplay.txt:221`, `addr:0x18001fcd0`), which is why it
is registered by address as well as by name.

All 48 bytes are written by both sides (four `Vector3`s, no padding), so there is no
unwritten-byte trap here — `reference_oracle_must_not_compare_unwritten_bytes.md` does not
apply.

### 4. `System::Thread::ThreadFunc` — **no diff oracle and no `calls=`**; witness + A/B instead

```
mingw32-make sa-run                              # ours (default)
BANISHED_RT_THREAD=0 mingw32-make sa-run         # the DLL's, everything else identical
```
It blocks on kernel events and dispatches into caller-supplied virtual work; calling it twice
runs the work twice and drains the events. There is no scratch stand-in either. This is the
same position `thread_win.cpp`'s banner already takes for the other seven `Thread` /
`CriticalSection` members, and this port does not pretend otherwise.

**There is also no `calls=` column, and that gap needed closing on its own.** `RT_DIFF_BEGIN`
is unusable here for a concrete reason: `runtime/funcs/rt_diff.cpp` has **no synchronisation at
all** — a bare `RtDiffSite* g_sites[]`, a bare `g_siteCount++`, a bare `site->calls++`. This
body runs on a worker thread, so a diff site in it would be a data race in every thread the
game creates and would produce a count nobody could trust.

So the port carries an **execution witness** instead:

```
BANISHED_RT_THREADFUNC_TRACE=1 mingw32-make sa-stress
BANISHED_RT_THREADFUNC_TRACE=1 mingw32-make sa-run          # gameplay
```
It prints **one line the first time each worker dispatches work**, naming the `Thread` object
and its vtable — so `standalone_error.log` says *which* workers ran, not merely that some did.
Bounded by the thread count (a handful). **Zero lines means zero dispatches**, and that is the
signal to read. It says nothing about whether the body is right; it says only that it ran.

Its second line is a real measurement rather than a tautology: retail **reloads** the vtable
between slot 0 and slot 1 (`0x1800ca48c`), and the trace reports the first time that reload
observes a *different* pointer. Never appearing means the reload is dead in practice and we
know it; appearing means our fidelity to it is load-bearing and a port that cached the vtable
would call the wrong `OnCompleted`.

**Which harness path?** Thread creation is the path, and lane D's `_beginthreadex` at
`0x18007fbfc` is how every `System::Thread` is made, so it exists in principle: the async
pathfinder (`code/Game/Game/pathfinder_seed.cpp`) and the game-teardown worker
(`standalone/keystone_a_gamedtor.cpp`) are both `System::Thread` subclasses. `sa-stress`
is the obvious candidate and gameplay + teardown the obvious moment. **But this lane could not
run anything, so that is a reasoned expectation, not an observation, and the witness above
exists precisely so that whoever runs it does not have to take my word for it.** If the trace
comes back empty, **this port has landed unverified** and should be recorded that way — Wave 5
landed four ports unexercised with written justifications and that was the right call.

If it does dispatch, the A/B is then meaningful because the observable is coarse and loud, not
subtle: a wrong `ThreadFunc` is a hang at teardown or citizens that never path. Run `sa-run` to
gameplay and `sa-stress` under both settings and compare.

**One thing to check first, because it is the only way this port can be silently inert:** the
constructor now asks for the worker entry **by name** through the port-aware `ResolveExport`
rather than `ResolveExportRaw`. If that edit were reverted, the port would register, the census
would count it, and every thread in the process would still run the DLL's body. Confirm
`[RtPort] System:` reports one more export than before, and that `BANISHED_RT_THREAD=0`
actually changes behaviour.

### 5-6. `Goal::Serialize` and `GoalInstance::Persist` — lane S, not `RT_DIFF`

```
BANISHED_RT_SAVE_TRACE=1 BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=900 BANISHED_PAUSEMENU_QUIT=120 \
  mingw32-make sa-run
```
Both are `SAVE-PATH`, so `BANISHED_RT_DIFF` is the wrong tool — a naive ours-then-theirs would
move the real stream cursor twice, which is exactly why Wave 5 held them back. They use
`RtSaveTrace::Compare` (`runtime/src/io/save_trace.h`, built and proved by lane S at 32,772
identical emissions on `Audio::Scene::Serialize`): both bodies replay into **two independent
synthetic sinks** in the **writing** direction and the **emission sequence** is compared. No
prefix stop is needed — neither body mutates the object it serializes on the write path.

**The probe is in situ**, at the top of each ported body, on the first 16 real calls, guarded
by a re-entrancy flag (`Compare` calls `ours`, which is that same body, and `Compare` is not
reentrant — its sinks and recorders are function-level statics). So `calls=` is automatic:
whatever path reaches the real function is what runs the comparison.

**Path:** both appear in **both** residue runs — `Persist` twice
(`wave9_residue_gameplay.txt:106`, `wave9_residue_autostart.txt:95`) and `Serialize` once
(`:124` / `:112`) — resolved from `engine/funcs/engine_funcs.cpp:4800`,
`standalone/reg_goal.cpp:73` and `standalone/address_registry.cpp:1240`. The achievement/goal
machinery is loaded at menu time, and a save/load exercises the records:
```
BANISHED_RT_SAVE_TRACE=1 BANISHED_AUTOLOAD=Save9 mingw32-make sa-run
```
Note `reference_mods_off_breaks_save_loading.md`: the `.pkm` files must be **in** WinData for
autoload to work.

**Two caveats stated rather than buried.**

* `RtSaveTrace::Compare` hands each side `&sink.serializer`, because its first subject took an
  `IO::Serializer&`. **Both functions here take an `IO::Stream&`** — the manglings say
  `AEAVStream@IO@@` and the bodies read the parameter at `+0x08` (direction) and `+0x10`
  (encoding), which are `Stream` fields. Passing the `Serializer` straight through would read
  16 bytes past a 16-byte object. The ports therefore wrap each side in an adapter that
  recovers `Serializer.stream` (`+0x00`). That defeats `Compare`'s own `ours == theirs` guard —
  the guard that exists to catch a vacuous pass on an unregistered port — so the equivalent
  check is made explicitly instead, on whether `ResolveExport(<name>)` hands back our body.
  A `[RtSaveTrace] FAIL … the port is NOT registered` line is that check firing.
* Retail's bodies carry SEH state (`MOV [RSP+0x20],-2`), so if a field serializer threw, the
  codec's deleting destructor would still run. Our ports have no `try`. Nothing in this family
  throws in practice, but that is an assumption and it is written down here rather than left
  for someone to rediscover.

## The "resolved is not called" caveat, applied to the rest of this lane

Lane 0's recovered entries include `??_7Thread@System@@`, `??_7Goal@Achievement@@` and
`??_7GoalInstance@Achievement@@` — vtables in three classes this lane completed. They moved
from "not reached by any door" to "asked for", **not** to "executed", and nothing in this
lane's verification leans on them:

* No port in this lane stamps a vtable. The `Goal` / `GoalInstance` / `Thread` vtable stores
  live in Wave 5's `goal.cpp` and Wave 2's `thread_win.cpp` ctors, untouched here.
* The five ports with a running oracle are verified by `calls=` / emission counts from their
  own instrumentation, not by a residue row. `Projection`, `Serialize` and `Persist` do appear
  in the residue, and that is cited above as evidence a *resolve site exists and is live* —
  which is what it proves. Their actual verification is the oracle, and each has one that
  executes at the call site.
* The one place where the distinction bites is `ThreadFunc`, handled in §4 above.

## Comments corrected at their source (PLAN §6 step 8)

* `runtime/src/system/thread_win.cpp` banner — said ThreadFunc "is intentionally left as the
  DLL's own … not ported". Now records the port, and keeps the sentence about the layout being
  the reason it was safe.
* `runtime/src/achievement/goal.cpp` banner and its registration block — said `Goal::Serialize`
  and `GoalInstance::Persist` "stay on the DLL" because "no oracle can run both implementations
  without double-consuming the stream". Corrected to record that this is true of the READ path
  and false of the WRITE path, with the old claim kept in one line so a reader who meets it
  elsewhere recognises it.
* `runtime/src/volume/volume_port_init.cpp` — the "11 exports (Frustum x5)" banner is now 12/x6.
* `runtime/src/graphics/renderer.cpp:135` said Projection "is NOT ported by Wave 1 … so today
  this always lands in the DLL". Corrected — the mechanism it describes (`ResolveExport`,
  which consults the port registry first) is unchanged and is exactly how the call site now
  finds the port; only the claim about what is on the other end was stale. Lane R also has
  this file open this round, so the edit is deliberately confined to those two comment lines.
* `docs/runtime_port/wave9_laneB_inventory.csv` — the four rows above are wrong. Not edited for
  the same reason (it is lane B's artefact and another round-2 lane may be reading it); the
  corrections are in §"Four corrections" here and in each port's own file banner, which is
  where a reader of the code will meet them.

## What lane P did not do

* Did not build. Did not run the game. Did not touch `standalone/build*` or the game directory.
* Did not port `GoalList::Initialize` — deferred with the evidence above.
* Did not port `FUN_1800042a0` / `FUN_180005dc0`; they are called by address and handed to
  lane D as two named rows.
