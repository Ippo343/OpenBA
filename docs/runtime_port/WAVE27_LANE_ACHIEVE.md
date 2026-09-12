# WAVE 27 — LANE ACHIEVE

Branch `wave27/achieve`, cut from `wave27/base` at `c6f30c24`.

Scope as briefed: the four `Achievement` vtable rows, `GoalList::Initialize`,
`??_7Stream@Audio@@6B@` 1/2, `Audio::Scene::Update`, and the question of whether
the `?_singleton@AchievementManager@Achievement@@` blocker could be retired.

**Everything below is separated into MEASURED and INFERRED. Every figure was
re-derived on this branch; none is quoted from a previous wave doc.**

---

## 1. HEADLINE — and it is NOT the percentage

**MEASURED.** `tools/runtime_census.py`, run on `wave27/base` before and on this
branch after:

| | before | after |
|---|---|---|
| `??_7Achievement@0@6B@` | 4 / 9 | **9 / 9 COMPLETE** |
| `??_7AchievementInstance@Achievement@@6B@` | 1 / 4 | **4 / 4 COMPLETE** |
| `??_7Goal@Achievement@@6B@` | 4 / 7 | **7 / 7 COMPLETE** |
| `??_7GoalInstance@Achievement@@6B@` | 3 / 7 | **7 / 7 COMPLETE** |
| Achievement BODY gap rows | 1 | **0** |
| Achievement namespace | 52 called / 45 ported | 54 called / 47 ported |
| TOTAL | 1372 / 1237 = 90.2% | 1377 / 1239 = **90.0%** |

**THE TOTAL PERCENTAGE WENT DOWN AND THAT IS THE EXPECTED SHAPE, NOT A
REGRESSION.** `called` is the count of Runtime symbols our tree references, and
this lane's bodies reference five symbols the tree did not reference before (the
four `?_typeInfo@…@Achievement@@` nodes read through `RT_DATA_SLOT`, plus
`?_singleton@Factory@Object@@`). So `called` grew by 5 while `ported` grew by 2,
and a ratio with `called` in both halves moved down —
`reference_the_ceiling_percentage_is_a_moving_target.md` and
`reference_porting_can_grow_the_cutover_surface.md`, both in one number.

**Score this lane on the identity: all four Achievement vtable rows went from
incomplete to COMPLETE, and the namespace's BODY gap went to zero.** That is the
entire Achievement vtable backlog of the Wave 27 kickoff, closed.

**COMPLETE IS THE STATIC HALF OF READINESS ONLY.** It says promoting would put
no trap in a live table. **This lane promoted nothing** and makes no claim that
any of the four rows is promotable — see §5.

---

## 2. INSTRUMENT DEFECTS OBSERVED, NOT FIXED

Per kickoff §4 #1. Nothing here was fixed; all porting continued.

### 2.1 `disassemble_function` served a DIFFERENT FUNCTION under the requested address

**MEASURED, found by this lane independently before the integrator's warning
arrived.**

* `mcp__ghidra-mcp__disassemble_function(address="0x18009f0e0")` returned a
  listing whose entry is **0x18009f090**, running to 0x18009f368 — a body that
  allocates 0x100 bytes via `CALL 0x180098fd0`, stamps vtable `0x1801a9310`, and
  touches fields at +0xe0/+0xe8/+0xf0/+0xf8. Nothing to do with `GoalList`.
* `mcp__ghidra-mcp__get_function_by_address(address="0x18009f0e0")` on the SAME
  address, same session, answered correctly:
  `Function: Initialize at 18009f0e0 … Body: 18009f0e0 - 18009f264`.
* Same failure at `0x180009d70` (returned entry `0x180009c20`).
* Same failure at `0x180006d10`: returned a 138,926-character listing, i.e. some
  much larger function.

`tools/ghidra_mcp/bridge_mcp_ghidra.py` — the `disassemble_function` endpoint.
This is the same family as the already-recorded
`reference_ghidra_mcp_address_param_silently_ignored.md` (`get_function_metrics`
ignores `address=` and falls back to a NAME lookup). The returned content here is
consistent with a name-lookup fallback: `Initialize` is a name dozens of classes
share.

### 2.2 The profile/retail question — CHECKED, and this lane was on RETAIL

**MEASURED.** The integrator's discriminator,
`disassemble_function(0x180002190)`, returned the **retail** two-instruction
body `MOV RAX,qword ptr [0x1801fae10] ; RET`, not the profile build's
~40-instruction comparison function. Confirmed a second way from the shipped
bytes: `48 8b 05 79 8c 1f 00 c3` at 0x180002190, whose RIP displacement resolves
to 0x1801fae10 against the next instruction at 0x180002197.

So no body in this lane is profile-derived. **This is a control, not luck**: see
§2.4 for the reason it could not have been.

### 2.3 `list_segments` — not re-tested

The integrator reports `list_segments` contradicts the retail PE headers. **This
lane did not use it and did not test the claim.** Recorded as received, not as
corroborated.

### 2.4 WHAT THIS LANE DID INSTEAD, and the control that makes it sound

Because 2.1 was found before any body was written, **every single body in this
lane was decoded from the SHIPPED RETAIL FILE ON DISK**, by two independent
mechanisms and then a third:

1. `pefile` `get_data(va - ImageBase, n)` over
   `C:/GOG Games/Banished/Runtime-x64.dll`, raw hex.
2. `objdump -D -b binary -m i386:x86-64 --adjust-vma=<va>` over that extracted
   blob.
3. After the integrator's second warning, the exact form they verified:
   `objdump -d --start-address=… --stop-address=… "C:/GOG Games/Banished/Runtime-x64.dll"`.

All three agree on every body. Neither Ghidra defect can reach any of them: the
file is named by explicit path (so program selection cannot apply) and addressed
by true VA (so a name fallback cannot apply).

**THE SIBLING-SWAP CONTROL — the integrator's specific worry, and it is
answerable.** The warning is that `Achievement`/`AchievementInstance` and
`Goal`/`GoalInstance` are near-identical pairs whose slot bodies are adjacent, so
being handed a sibling's body would produce a listing that reads as correct. That
failure has a natural tell here, and the tell is negative:

```
TypeName@Achievement          0x180001040 -> node 0x1801f98a0
TypeName@AchievementInstance  0x1800017c0 -> node 0x1801f98d8
TypeName@Goal                 0x180001b60 -> node 0x1801f9910
TypeName@GoalInstance         0x180001e60 -> node 0x1801f9948
```

**Four addresses, four DISTINCT nodes**, and each of the four is independently
confirmed to be that class's own `?_typeInfo@` row by
`standalone/rt_data_mint_tables.inc` lines 1853 / 1857 / 2021 / 2025 — a
generated artefact this lane did not produce. A sibling swap would have made two
of the four name the same node, and it did not. The `Type@` quartet
(0x1801f9898 / 0x1801f98d0 / 0x1801f9908 / 0x1801f9940) is the same control run
again, and also passes.

That is a real control and not a reassurance: it has a way to fail and it was
checked in the direction of failure.

---

## 3. WHAT WAS PORTED — per slot and per body

Four new files, all under `runtime/src/achievement/`, all registered through
`rt_achievement_port.cpp` on the existing kill switches.

### 3.1 `achievement_typeinfo_accessors.cpp` — 11 slots

| slot | retail | ported | registered | verified how |
|---|---|---|---|---|
| `?Type@Achievement@1@` | 0x180001020 | yes | name + addr | RT_DIFF_SCALAR vs the shipped export, full |
| `?TypeName@Achievement@1@` | 0x180001040 | yes | name + addr | RT_DIFF_SCALAR (pointer), full |
| `?Type@AchievementInstance@` | 0x1800017a0 | yes | name + addr | RT_DIFF_SCALAR, full |
| `?TypeName@AchievementInstance@` | 0x1800017c0 | yes | name + addr | RT_DIFF_SCALAR (pointer), full |
| `?Type@Goal@Achievement@@` | 0x180001b40 | yes | name + addr | RT_DIFF_SCALAR, full |
| `?TypeName@Goal@Achievement@@` | 0x180001b60 | yes | name + addr | RT_DIFF_SCALAR (pointer), full |
| `?Type@GoalInstance@` | 0x180001e40 | yes | name + addr | RT_DIFF_SCALAR, full |
| `?TypeName@GoalInstance@` | 0x180001e60 | yes | name + addr | RT_DIFF_SCALAR (pointer), full |
| `?GetFileVersion@Goal@Achievement@@` | 0x18009ee10 | yes | name + addr | RT_DIFF_SCALAR, full. `MOV EAX,0x10000` — **65536**, and NOT the 4 / 0x1000 the Graphics siblings return |
| `?Startup@GoalInstance@` | 0x18009f000 | yes | name + addr | **none, and none exists.** Retail is one byte (`RET 0`): no return value, no argument read, no side effect. Stated rather than implied |
| `?CheckGoalState@GoalInstance@` | 0x18009f060 | yes | name + addr | **no diff probe, by decision.** It is one virtual dispatch; calling the DLL's copy as a second opinion would run the EXE subclass's override a second time on live town state, which is a behaviour change, not an observation |

**MEASURED, and it corrects a plausible reading of `Goal.h`:** the vtable slot
`CheckGoalState` dispatches through is `+0x40` (`48 ff 60 40`), which is slot 8
of `??_7Goal@Achievement@@6B@` — and that slot is **PURECALL in retail**
(`rt_data_mint_tables.inc` `kVSlots29[8]`, 0x18007ec28). Not a decode error:
`Achievement::Goal` declares it pure and the EXE's `StatGoal` / `TimeGoal` /
`PopulationGoal` / `BuildingGoal` / `FinishTutorialGoal` override it. So the call
always lands in the exe. `Goal.h`'s `GoalVtbl` namespace does not list slot 8
(it lists only what its own lane dispatched through), so the constant is pinned
in the new file beside the instruction it came from, with a `static_assert`
tying it to the `+0x48` that `Goal.h` did pin.

### 3.2 `achievement_vtable.cpp` — 4 slots

| slot | retail | ported | registered | verified how |
|---|---|---|---|---|
| `?Destruct@Achievement@1@` | 0x180001370 | yes | name + addr | **none.** Three instructions, all of them one virtual dispatch. Replaying it runs a second destructor on a live object |
| `?Reconstruct@Achievement@1@` | 0x180001270 | yes | name + addr | **indirect, one level down.** The thirteen constructor stores are not re-transcribed — the body calls the existing `RtAch_Achievement_Ctor`, which already carries a full-byte oracle into poisoned scratch. The virtual slot-0 call is NOT covered |
| `?Construct@Achievement@1@` | 0x1800012f0 | yes | name + addr | same, plus retail's own `TEST RCX,RCX` null return, reproduced |
| `?Persist@AchievementInstance@` | 0x18009e4a0 | yes | name + addr | **PREFIX oracle, `stopAfter=1`. See the caveat below — it is weaker than the header asks for and this doc does not pretend otherwise** |

**MEASURED — `AchievementInstance::Persist` is the outlier of the Serialize
family, in three ways, all confirmed twice from the retail bytes:**

* it builds **TWO nested codecs over one stream**, and
* the OUTER one is created with **version argument 1** (`41 b8 01 00 00 00` at
  0x18009e4c9) where every other member of this family passes 0 — which is
  precisely why `MakeCodec` in the two sibling files, which hardcodes 0, is NOT
  reused; and
* there is a real **early return**: `if (rw->GetVersion() == 0) { rw->Skip();
  return; }` (slots +0x08 and +0x10), which skips the achievement pointer AND
  the entire goal list.

The three profile-sensitive `ReaderWriter` slots `Serializer.h` warns about
(+0x98 / +0xA0 vs the profile build's +0xb0 / +0xb8) were re-read from the retail
file for this body specifically: `ff 90 a0 00 00 00` — `+0xA0`, `kScalarInt`.
Correct for retail.

**THE ORACLE'S LIMIT, stated because a partial oracle that reads like a full one
is worse than none.** `save_trace.h` says to choose `stopAfter` as "the emission
index of the last call before the first store, read off the disassembly".
**I could not derive that index and did not guess it.** The emissions before the
mutating tail include however many
`ExternalPtr<Achievement>::Serialize` (0x180006d10) makes, and that body branches
on `Encoding::GetVersion()==3` into paths with different record counts.
`stopAfter=1` is therefore a **floor I chose for safety**. What it checks: the
codec construction, the version argument, the two-codec nesting and the early
return. What it does NOT check: the achievement-pointer record, the count record,
and every instance record. A second limit: if the synthetic sink's `GetVersion()`
returns 0, both sides take the Skip path and the comparison is vacuous — the
run's own `[RtSaveTrace]` emission counts are the effectiveness control that
distinguishes that case
(`reference_probe_needs_an_effectiveness_control.md`).

### 3.3 `goal_list.cpp` — the BODY row

`?Initialize@GoalList@Achievement@@QEAAXV?$Array@V?$ExternalPtr@VGoal@Achievement@@@Object@@@Collection@@@Z`,
retail 0x18009f0e0, body to 0x18009f264. **Ported, registered by name AND by
address** (the address matters more: every consumer found so far reaches it by
address — `achievement_manager.cpp:657` transcribes `CALL 0x18009f0e0` and
`code/Game/Game/gameupdateloop_load.cpp:1660` describes the same site).

**Verified how: a SHADOW PREDICATE over the post-state, NOT a replay, and the
reason is specific rather than "no oracle possible"** (a claim this project has
had wrong four times across three waves). Every call allocates `n` new
`GoalInstance`s out of the global `Object::InternalFactory`, calls `Startup()` on
each, and frees the caller's temporary buffer. A second run would allocate a
second set into the one live factory, leak them, `Startup()` them again, and
double-free the temp. **There is no prefix that stops before the first mutation,
because the first iteration IS the first mutation** — which is why even the
prefix mechanism used in 3.2 does not apply.

The shadow checks three invariants, each naming its own failing term: (a) the
destination was sized to `n` and the source array was consumed; (b) no element is
null (a null means the dynamic cast failed, which retail then crashes on); (c)
every instance's `_goal` at +0x18 holds the pointer the source array held at the
SAME index, against a snapshot taken before the buffer was freed. (c) is the term
that would catch the real failure mode — a transposed or off-by-one index in the
ownership move — and it needs no second run because it compares our output
against a snapshot of our own input. **It is not a comparison against retail and
this lane does not claim one.**

### 3.4 `achievement_manager_ctor.cpp` — `??0AchievementManager@Achievement@@`

Retail 0x18009eb00. **Ported, registered by name and by address.** Verified with
a **full-byte oracle** into a 0xCD-poisoned scratch buffer — the strongest
verification in this lane, and possible because the ctor's writes are confined to
`this`.

**MEASURED, and it is the positive evidence for a claim `AchievementManager.h`
made without a writer:** the last store is `89 41 40` — a **DWORD** `MOV
[RCX+0x40],EAX`, not a qword. So **+0x44 is never written**, and the poisoned
oracle shows `0xCDCDCDCD` there on BOTH sides, which is positive evidence rather
than an absence of evidence. The header's `_pad44 // never read or written` now
has its writer.

The ctor's one side effect outside `this` is `MOV [0x1801fc670],RCX` —
`_singleton = this`, **last and unconditional**. Running retail's copy on scratch
would leave the singleton pointing at a dead stack buffer, so the oracle
snapshots and restores the cell around the call. **That restore has a named
limit and an addressee — see §6.2.**

---

## 4. DID THIS LANE RETIRE THE `?_singleton@AchievementManager@` BLOCKER?

**NO. It retired exactly HALF of the expiry clause, and the tag STANDS.** Said
explicitly because the brief asked for an explicit answer.

The tag (`standalone/reg_mint_promoted.cpp:2714`) names **two** unported writers:

1. `??0AchievementManager@Achievement@@` — **now PORTED and REGISTERED**
   (§3.4). This half is satisfied. The ported ctor writes the same cell, through
   the same `RT_DATA_SLOT` door, that the already-ported destructor nulls — so
   ctor and dtor cannot disagree about which cell holds the manager whichever way
   the row is promoted.
2. "a store inside `??0DialogLoopT@Engine@@`'s range" — **still unported.** That
   is `??_7DialogLoopT@Engine@@6B@`'s family, owned in Wave 27 by **lane LOOPS**,
   and kickoff §3 forbids this lane from touching it.

So the two-owners-one-object hazard the tag describes **still holds**, and the
row must NOT be promoted on the strength of the ctor alone. What changed is that
the expiry clause now has one outstanding condition instead of two.

**The correction is written beside the tag itself**, in
`standalone/reg_mint_promoted.cpp`, not only here — kickoff §4 #9, because a lane
doc is not the surface the next person reaches. It carries its own ADDRESSEE:
**lane LOOPS of Wave 27, or whichever lane next owns
`??0DialogLoopT@Engine@@`**, with an instruction to re-derive the writer set
rather than trust the count.

---

## 5. PROMOTABILITY — four COMPLETE rows, zero promoted

**INFERRED, and deliberately left as an open question rather than answered.**
`reference_a_green_readiness_screen_is_not_a_route.md`: COMPLETE says the table
is safe to stamp, not that anything dispatches ours. Promotability turns on
owning the function that STAMPS the table, which no source scan settles.

This lane did not investigate the stampers for the four Achievement tables and
does not claim any of the four is promotable. What it establishes is the
precondition: **all four are now COMPLETE, so a lane that does own the stamper
has nothing left to port before promoting.** Four rows moved out of "needs body
work" into "needs a route".

One relevant measured fact for whoever takes that on:
`?Serialize@Achievement@1@` (slot 5) and `?Persist@GoalInstance@` were already
registered before this lane, and `achievement_serialize.cpp` records that
registering them "changes nothing until that table is promoted". That is still
true of all fifteen slots.

---

## 6. THE TWO CORRECTIONS THE INTEGRATOR SENT — both addressed

### 6.1 The `Array<ExternalPtr<Goal>>` refcount prescription DOES NOT APPLY HERE

The brief warned that the POD-element `Array<T>` recipes in
`achievement_manager.cpp` are "the wrong template" for the refcounted-element
`Array<ExternalPtr<Goal>>` in `GoalList::Initialize`, and a port following the
sibling recipe "leaks or double-releases a refcount with no byte oracle catching
it". The integrator then corrected the premise: the siblings are not a separate
template, the non-trivial-element machinery already exists, and the right action
is **one new `ArraySpec` row on the existing engine, not a new template**.

**MEASURED ANSWER: neither the original warning nor the correction describes
this body, and no `ArraySpec` row was added or needed.** Two facts, both from the
retail bytes:

* **The source array is never resized.** `GoalList::Initialize` resizes the
  DESTINATION (`this`), whose element type is `InternalPtr<GoalInstance>` — 8
  bytes, non-owning, a plain byte-copy element. The refcounted
  `Array<ExternalPtr<Goal>>` parameter is only read and then freed. So the
  refcounted-element machinery is not on any path this body takes.
* **Retail's teardown of that refcounted array is the POD one.**
  0x18009f201–0x18009f24e is a bare `if (p) { Enter; Free; Leave }` plus two
  zeroing stores. **No per-element loop, no release of any kind.** Compare
  `Achievement::~Achievement`, which DOES call the real
  `Array<ExternalPtr<Goal>>::~Array` — because there the elements are still
  owned.

The reason retail can do that is the interesting part: 0x18009f1e6
(`48 89 41 18`) **MOVES** each `Goal*` out of the source array into the
`GoalInstance`'s `_goal` field, with no addref and no release. Ownership
transfers element by element, and what is left at the tail is a buffer of
pointers nobody owns — a POD buffer.

**So the trap was real and it points the OPPOSITE way from the warning.** The
hazard was not "reused the POD recipe"; it was **"reached for the refcounted
recipe because the element type's NAME looks refcounted"**. Releasing those
elements would have over-released every `Goal` in the list. Both directions are
settled by the same two instructions, and neither by the element type's name —
which is the same lesson as
`reference_never_written_needs_by_whom.md`: the behaviour belongs to the WRITER,
not to the type.

The port uses `rtcoll::MemFree` (declared, not `#include`d — `array_internal.h`
states only two files include it), which array.cpp documents as exactly
`if (p) { Enter(alloc+0x640); Free(alloc,p); Leave; }`, instruction for
instruction the teardown retail inlines here.

**Honest note the integrator asked for, applied even though it turned out not to
bite: no refcount behaviour is verified by anything in this lane, because no
refcount is touched by anything in this lane.**

### 6.2 The Ghidra warnings — see §2.2 and §2.4

Checked, and this lane was already immune by construction, with a control (the
four-distinct-nodes test) that had a way to fail.

---

## 7. AUDIO — NOT PORTED, with the stated blockers re-checked as instructed

The brief said to re-check any holdback's stated blocker before recording a new
one, and that the six Wave 8 AUDIO holdbacks share a stored-completion-callback
root cause. **Both were done. Neither changed the outcome, and the callback hint
did not apply to either item.**

### 7.1 `??_7Stream@Audio@@6B@` — still 1/2. The unported slot is `?Run@Stream@Audio@@`

**MEASURED.** Of the table's two slots, `?OnCompleted@Thread@System@@` is
already ported and registered (`runtime/src/system/thread_win.cpp:451`).
The remaining one is `?Run@Stream@Audio@@UEAAXXZ`.

**Its size, measured:** `get_function_by_address` reports body
`0x1800a0f60 – 0x1800a1667` = **1800 bytes**. Re-read from the retail file, it is
the streaming worker thread's main loop: it indexes a table off the
`0x1801fe760` singleton at `+0xa98` with a 0x20 stride, builds an `IO::Stream`
on the stack (stamping `??_7Stream@IO@@6B@` at 0x1801549a0 then 0x180154960),
resolves a package path through `0x1801fc6a8`, runs the `System::String`
refcount idiom four separate times, and calls into the filesystem.

**NOT a stored-completion-callback blocker** — the hint the brief offered does not
describe it. It is simply out of this lane's budget after the Achievement work,
and it is a worker-thread body with real file IO, which is the shape
`audio_stream_win.cpp`'s own banner already rules out all five probe shapes for.
**Recorded as NOT PORTED — out of budget, not newly blocked.**

`audio_stream_win.cpp:8` already says in as many words that "the run body
(`?Run@Stream@Audio@@`) is NOT in" that lane's scope. That statement is still
accurate; this lane did not change it.

### 7.2 `?Update@Scene@Audio@@` — NOT PORTED, and its blocker was ALREADY re-checked

**MEASURED — and the re-check the brief asked for had already been performed and
recorded, which is itself the answer.** `docs/runtime_port/WAVE24_LANE_SMALLBODY.md`
line 143 re-checked this exact holdback and wrote:

> *"a replay that draws twice from `System::Random` diverges by construction…
> needs snapshot/restore of the Random state" — `audio.cpp:17`.* **YES, and it is
> an ORACLE blocker only.** … **NOT A WALL.** Not ported — out of budget, not
> blocked.

Size, measured: 0x1800df6c0, **1816 bytes**, ~150 decompiled lines
(`WAVE24_LANE_SMALLBODY.md:113`). So the position is unchanged from Wave 24: it
is not blocked, it is large, and two 1800-byte bodies did not fit behind four
COMPLETE vtables, a BODY row and the manager ctor.

**The brief's `Random&` caution is correct and stands for whoever takes it**: any
port must draw from retail's generator in retail's order or divergence is
guaranteed and will not look like a port bug.

---

## 8. RESIDUALS — every one with a named addressee

| # | residual | addressee |
|---|---|---|
| 1 | **`?_singleton@AchievementManager@` tag's second writer.** The store inside `??0DialogLoopT@Engine@@`'s range is still unported, so the tag stands. Correction already written beside the tag. | **Lane LOOPS of Wave 27**, or whichever lane next owns `??0DialogLoopT@Engine@@`. Re-derive the writer set; do not trust the count |
| 2 | **`?Run@Stream@Audio@@`** (0x1800a0f60, 1800 bytes) — the last slot of `??_7Stream@Audio@@6B@`. Not blocked, out of budget | **The Wave 28 kickoff writer.** Note `audio_stream_win.cpp`'s existing HANDOVER to mint `??_7Stream@Audio@@6B@` is a SEPARATE item and is still open |
| 3 | **`?Update@Scene@Audio@@`** (0x1800df6c0, 1816 bytes) — oracle blocker only, not a wall, per Wave 24 lane SMALLBODY | **The Wave 28 kickoff writer.** Unblocks both Audio DATA reader sets |
| 4 | **`disassemble_function` returns the wrong function for an address** (§2.1). Three reproductions in this lane. NOT FIXED | **The Wave 28 tooling decision.** Until then: `objdump -d --start-address=… --stop-address=… "C:/GOG Games/Banished/Runtime-x64.dll"` addresses by true VA and cannot be fooled |
| 5 | **`AchievementInstance::Persist`'s `stopAfter=1` is a floor, not the derived index** (§3.2). Everything after the first emission is uncompared | **Whichever lane next ports `ExternalPtr<T>::Serialize` (0x180006d10)** — its emission count is the missing input, and once known the `stopAfter` here should be raised to it |
| 6 | **`achievement_manager_ctor.cpp`'s oracle must be deleted when the singleton row is promoted** (§3.4). It restores the cell through `RT_DATA_SLOT`; retail's ctor writes the DLL's cell by hard-coded RVA. Same cell while unpromoted, two cells after. Guarded by `BANISHED_RT_DIFF`, so inert on a default run — but "inert by default" is not "safe once promoted" | **The lane that first promotes `?_singleton@AchievementManager@Achievement@@0PEAV12@EA`**, in the same commit |
| 7 | **Four new address doors**, each commented at its call site in the kickoff §4 #10 shape: `0x180006d10` (`ExternalPtr<Achievement>::Serialize`), `0x180009d70` (`Array<InternalPtr<GoalInstance>>::SetSize`), `0x180009fa0` (the N-element `InternalPtr<GoalInstance>` serializer). They GROW the cutover surface | **The cutover lane that first loads a game with achievements mounted.** `0x180009d70` is reached UNCONDITIONALLY by `GoalList::Initialize` — it is an `Array<8>` SetSize, i.e. the shape `array.cpp` already has eleven instantiations of, so it is a cheap ArraySpec row for a `collection`-owning lane |
| 8 | `runtime/src/achievement/README.md` still says "**Ported here:** _nothing yet_" while the directory holds eight ported files. Cosmetic, not corrected here to keep this diff to one subject | **The Wave 27 integrator**, at close |

---

## 9. BUILD

`mingw32-make standalone-debug` — **exit 0**, `standalone/build/reimpl-x64.exe`
32M. The pre-existing `cutover_manifest.inc` COSMETIC staleness warning is
present (a by-name door count moved — this lane added doors); the gate itself
reports `OK (60 rows; row set and every disposition match a fresh derivation)`,
so nothing the process acts on changed.

### 9.1 A run WAS attempted and was correctly abandoned — MEASURED

`tools/game_dir_lock.sh acquire ACHIEVE 600` was run properly (not piped, not
backgrounded, per the two recorded hazards). **The grant never came: lane LOOPS
held the lock for the whole 600-second window and was actively running** — its
`reimpl-x64.exe` was live (`exe_pids='35304'`, `last_write=0s`) and the deployed
exe was another lane's binary (md5 `1782e3eff984d103571a388e69afc860`).

Two things were then done rather than assumed:

1. The blocking acquire was **explicitly killed**, because a backgrounded acquire
   that is later granted leaves a lock held by someone who never notices
   (`reference_backgrounded_lock_acquire_is_held_unnoticed.md`). Killing it is not
   tidying: leaving it running would have taken the lock away from the next lane
   at an arbitrary moment.
2. `status` was re-read afterwards and confirms the lock is **still held by
   LOOPS, not by ACHIEVE**.

So there was no clobbered run and no run at all. This is the outcome the lock
exists to produce, and the alternative — deploying over a live lane's exe — is
precisely the Wave 9 failure that produced "one lane's first measurement was
silently taken against a DIFFERENT lane's binary".

**NOT RUN, and therefore NOT CLAIMED:** no in-game run, no
`BANISHED_AUTOMENU_BUTTON=8` Achievements-menu route, no `BANISHED_RT_DIFF` or
`BANISHED_RT_SAVE_TRACE` pass. **Every oracle described above is WIRED, not
EXERCISED.** Eleven of the fifteen slots are on unpromoted vtables and so cannot
dispatch to us at all today, which is the reason a run would have measured
nothing about them — but that reasoning does not extend to `GoalList::Initialize`
or `??0AchievementManager@`, which are reachable by address, and for those two
the honest statement is that the probes are unrun.

**ADDRESSEE for that gap: the Wave 27 integrator**, whose merge build and
cutover attempt 30 is the first run these bodies will see. The two probes worth
watching in that run are
`[RtDiff] Achievement::AchievementManager::ctor` (full byte oracle — a
divergence there means the 0x48-byte layout is wrong) and
`[RtDiff] Achievement::GoalList::Initialize` (shadow — `firstWrongGoal >= 0`
means the ownership move is transposed).
