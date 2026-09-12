# WAVE 23 — LANE ACHIEVE: all 16 Achievement rows

Branch `wave23/achieve`, base `wave23/base` @ `a60351e3`. Private worktree.
**Every census figure in this document is BRANCH-LOCAL** (rule 13). The integrator
re-measures on the merged tip; Wave 21 published from lane reports and had to correct
itself, and in Wave 22 the merged System figure matched neither lane that moved it.

## HEADLINE

**Achievement 36 → 43 ported, 69.2% → 82.7%. Seven of sixteen rows moved; all seven are
BODY rows. Zero vtables promoted and zero DATA rows promoted, and both of those are
findings rather than shortfalls.**

The single most useful thing this lane produced is not the seven bodies. It is the
**stamp-site census in §3**: promotability of a vtable is a measurable property — what
fraction of the DLL bodies that STAMP the table do we own — and it had never been
measured for these four. The answer is 2/7, 3/7, 3/5 and 3/6. None is promotable
DLL-present; `??_7Goal@Achievement@@6B@` is **two bodies** away from being the first one
that is.

A stale blocker was also discharged (§2.2): `?Persist@AchievementManager@` had been held
back since Wave 5 for a reason that stopped being true in Wave 9 and that **lane P
discharged in the identical words for two sibling functions in the same directory**. It
survived fourteen waves because nobody re-read it (rule 3).

| kind | rows | moved | left |
|---|---|---|---|
| BODY | 7 | **6** | 1 (`?Initialize@GoalList@`, held back with a reason, §2.4) |
| DTOR | 1 | **1** | 0 |
| VTABLE | 4 | 0 | 4 (not promotable DLL-present — §3) |
| DATA | 4 | 0 | 4 (promotion would be HARMFUL — §4) |

---

## 0. TASK 0 — THE PRE-WORK GREP (rule 2)

All sixteen names grepped across `code/`, `runtime/`, `standalone/`, `engine/`, `tools/`
**before** a line was written. `?StrCompare@` read as unported for twenty waves because
nobody registered an existing body; this is the check that catches that.

**Result: nothing was already ported. But every one of the sixteen was already KNOWN,
and eleven already had live plumbing pointing at the DLL.**

| row | what the grep found |
|---|---|
| `?CreateInstance@Achievement@1@` | a `ResolveExport` **door** at `achievement_manager.cpp:188`; a measured size note in `AchievementManager.h:117` |
| `?Initialize@GoalList@` | a `ResolveExport` **door** at `achievement_manager.cpp:201` |
| `?Persist@AchievementManager@` | a **live door in game code** — `code/Game/Game/gameupdateloop_serialize.cpp:165`, every save and every load |
| `?ReadAchievements@` | a `ResolveExport` **door** at `achievement_manager.cpp:171` |
| `?RemoveAll@` | **two** doors: `code/Game/Game/game_main.cpp:258` and `runtime/src/engine/engine_lifecycle.cpp:843` |
| `?Serialize@Achievement@1@` | a MINTED, UNPROMOTED vtable slot (`rt_data_mint_tables.inc:628`) |
| `??1AchievementManager@` | a door at `engine_lifecycle.cpp:938`, plus a comment at `:822` naming it "the null store" |
| 4 × `??_7…` | all four already MINTED (`rt_data_mint_tables.inc:1543/1544/1569/1570`), none promoted |
| 4 × DATA | all four already MINTED; `_singleton@` has **two live readers in `code/Game/`** |

`code/Game/` was grepped separately as the brief asked. It holds no Runtime-side
Achievement body; what it holds is `gameupdateloop_serialize.cpp` and
`eventlog_achievement.cpp`, which are **consumers** of exactly these rows.

**The one thing the grep changed about the plan:** every dependency these seven bodies
need was already resolved somewhere in `runtime/src/achievement/`. The lane wrote almost
no new plumbing; it replaced doors.

---

## 1. RE-DERIVING THE BOARD (rule 1)

```
$ tools/runtime_census.py --namespace Achievement     # run from THIS worktree
Achievement  118 total   52 called   36 ported   69.2%
Achievement   16 gap |  BODY 7  CTOR 0  DTOR 1  VTABLE 4  DATA 4  UNKNOWN 0
```

**The kickoff was NOT stale.** 16 rows, 7/1/4/4, 69.2%, exactly as `WAVE23_KICKOFF.md`
§0 and §2 print them. This is recorded because rule 1 says a lane that finds the doc
stale reports that as its headline — and the honest converse is that a lane that finds it
accurate should say so rather than leave the reader guessing.

---

## 2. SECTION ONE — THE BODIES

Six BODY rows and the DTOR ported, in two new files:

* `runtime/src/achievement/achievement_state.cpp` — `??1AchievementManager`,
  `?RemoveAll@`, `?ReadAchievements@`, `?SetAchievementMet@`, `?CreateInstance@Achievement@1@`
* `runtime/src/achievement/achievement_serialize.cpp` — `?Serialize@Achievement@1@`,
  `?Persist@AchievementManager@`

Both are gated by `BANISHED_RT_ACHMGR` with `achievement_manager.cpp` (class atomicity,
PLAN §4.1 decision 5) and wired from `rt_achievement_port.cpp`. Every body was read as
**both** decompiled C and raw disassembly; where they disagreed the disassembly won.

### 2.1 The destructor, and the MIRROR of the trap the brief named

The brief warned that porting a dtor "exactly as prescribed" once swapped a body that
destroys **every** element for one that destroys only the first. The hazard here is the
**opposite**, and the natural C++ instinct gets it wrong in that direction:

> **`??1AchievementManager@Achievement@@QEAA@XZ` destroys NO elements at all.**

Read at `0x18009eab0` the entire body is a null store and three CALLs, each to a leaf
that frees one buffer. No loop, no vtable call:

```
MOV qword ptr [0x1801fc670],0x0       ; _singleton = nullptr, FIRST
ADD RCX,0x30 / CALL 0x180009700       ; ~Array(_updateTimes)  +0x30
LEA RCX,[RBX+0x18] / CALL 0x180008e80 ; ~Array(_instances)    +0x18
LEA RCX,[RBX+0x8]  / JMP  0x180009560 ; ~Array(_completed)    +0x08
```

The `AchievementInstance` objects hanging off `_instances` are destroyed by `RemoveAll`,
which `Shutdown` calls. Adding an element loop "for symmetry" would **double-free every
instance** on the normal Shutdown-then-destruct path. Two orderings are also load-bearing
and are preserved: the singleton is nulled **before** the frees, and the three arrays go
in reverse declaration order.

`QEAA` is the right spelling and it is not a formality: `AchievementManager` has no
vtable at all (its ctor at `0x18009eb00` stamps nothing), so a `??1…@UEAA@XZ` spelling
would resolve NULL forever.

**The three callees are NOT new doors.** `0x180009700` / `0x180008e80` / `0x180009560`
are the same eleven-instruction leaf byte-for-byte, and none is covered by Wave 9 lane
Q's `Array<T>::~Array` port (which registers nineteen addresses but not these three).
Calling them would have opened three new doors; the port **inlines** the leaf instead —
the same argument `array.cpp` itself makes when it says "one C++ function serves all
eleven". **This does not port those three addresses and the lane does not claim it does.**

### 2.2 `?Persist@AchievementManager@` — a fourteen-wave-old blocker that was stale (rule 3)

`achievement_manager.cpp`'s banner held it back in these words:

> "it inlines an IO::Serializer construction, and Wave 4 lane A measured that a
> Serializer ctor CONSUMES OR EMITS A 12-BYTE STREAM HEADER. So it has no runnable
> oracle of any kind: a second construction desyncs the save."

That was true when written (Wave 5) and stopped being true in Wave 9, when lane S built
`runtime/src/io/save_trace.h`: it runs each side against its **own independent synthetic
sink**, so there is no shared stream to desync and each side builds its own codec.

The damning part is not that the blocker aged. It is that **Wave 9 lane P discharged the
identical blocker, stated in the identical words, for `Goal::Serialize` and
`GoalInstance::Persist` — two functions in the same directory, in a file whose banner
says so in its first paragraph.** Nothing propagated the discharge to the third and
fourth members of the same family. The banner in `achievement_manager.cpp` is now
corrected **in place**, and the stale paragraph is kept rather than deleted, as the
evidence for the rule.

### 2.3 What the two stream ports do and do NOT prove

They are wired to `RtSaveTrace::Compare`, and the two oracles are **not the same
strength**. Saying so is the point:

| body | `stopAfter` | why |
|---|---|---|
| `Achievement::Serialize` | 0 (whole body) | every write-path emission is a pure read of the object; the `ExternalPtr` serializers take the `GetReferenceId` branch that lane P already proved const and idempotent (`0x1800bf100` is `QEBA`, a pure lookup, no insert and no counter) |
| `AchievementManager::Persist` | **1 (prefix)** | its tail is a loop of `InternalBasePtr::Serialize`, which `save_trace.h` itself names as mutating "the pointer AND the global factory". Replaying it performs that mutation twice on the one live factory. |

**THE COST OF THE PREFIX ORACLE IS REAL AND IS NOT HEDGED: for `Persist`, everything
after the count record — i.e. every instance record — is not compared.** What it covers
is the half that decides the stream layout.

**Both READING directions are entirely uncovered** by these oracles, which pin the
writing direction. The two `SetSize` calls and the whole load path are transcribed from
the bytes and unverified by any instrument in this lane.

### 2.4 `?Initialize@GoalList@Achievement@@` — HELD BACK, with the reason

The one BODY row not ported. `0x18009f0e0` is not a list copy: its middle is an inlined
`Object::Factory` red-black-tree walk keyed on a type id, a `CreateNewObject` call through
a factory record's vtable slot `+0x20`, and then an inlined `Cast<GoalInstance>` that
walks the parent-type chain. That is **the object creation path** — the subsystem
`WAVE11_KICKOFF.md` scoped as entirely unported (202 `?Create*Object@` exports, zero
ported, all 104 minted factory pointers still abort traps).

Porting it in this lane would have meant porting a slice of `Object::Factory` under a
lane that had no way to run it. It is handed over in §6 with its dependency named rather
than bundled.

Its only caller in the DLL is `Achievement::CreateInstance` — which this lane **did**
port, and which calls it through the existing `ResolveExport` door. So the row stays on
the DLL and nothing regresses.

### 2.5 Hazards kept faithful, not "fixed"

* `?SetAchievementMet@` writes `_completed._data[index]` with **no bounds check and no
  null check** — `MOVSXD RBX,EDX / MOV RAX,[RCX+8] / MOV byte [RBX+RAX],1`, three
  instructions, no test. Kept. Note the **sign extension**: a negative index writes
  *before* the buffer, not at a huge positive offset.
* `?CreateInstance@` writes `*(inst+0x20) = this` **outside** the allocation-failure
  branch (`0x18009e016`), so on a failed allocation retail writes through null. Kept.
* `?CreateInstance@` returns `InternalPtr<>` by value: `RDX` is the hidden return slot
  **and** `MOV RAX,RSI` at `0x18009e04f` returns it in RAX as well. Both reproduced —
  a typedef that omits the slot is a wild write that still returns the right answer.
* The temporary `Array` `CreateInstance` builds is passed **by value** and
  `GoalList::Initialize` **destroys it** (its tail frees the buffer at `0x18009f23c`).
  Nothing here frees it; doing so would double-free on every instance created.
* `?ReadAchievements@` **zeroes each slot before** calling `Persistence::Get`
  (`0x18009ebc4`). `Get` only writes its out-parameter when it finds the key, so dropping
  that store would silently preserve stale awards across a reload.
* `?Persist@` reads the count **after** `RemoveAll` on the read path. Transcribing them
  in the other order would be invisible on the write path and would still be wrong.

### 2.6 The cost this lane ADDED, stated rather than hidden

`called` rose 1314 → 1318 branch-local. Porting these bodies opened **three new
by-address doors** into unexported DLL internals
(`reference_porting_can_grow_the_cutover_surface.md`):

| address | what | why not ported |
|---|---|---|
| `0x180005110` | `Array<ExternalPtr<Goal>>::SetSize` | a Collection internal, no lane owns it; read path only |
| `0x180005220` | serialize N `ExternalPtr<Goal>` | reaches `ExternalFactory::FindOrLoad`, which WAVE9 records as having no possible oracle |
| `0x180009a20` | serialize N `InternalPtr<AchievementInstance>` | same family |

`0x1800042a0` and `0x180004590` were **already** doors and are not new. `0x180008d60` is
not a DLL call at all — Wave 9 lane Q ported and registered it, so `ResolveAddr` lands on
our body.

---

## 3. SECTION TWO — THE FOUR VTABLES

**Verdict: zero promoted, and promoting any of them today would be HARMFUL, not merely
useless.**

Rule 4 says extent, readiness and promotability are three different questions, and that
promotability turns on **owning the function that STAMPS the table**. That is answerable,
and it had not been answered for these four. `get_xrefs_to` on each table address in
Runtime-x64.dll enumerates every stamp site; cross-referencing each against
`RegisterRuntimeAddr` in our tree gives coverage.

### 3.1 The stamp-site census

| vtable | DLL addr | stamp sites | **ours** | unowned stampers |
|---|---|---|---|---|
| `??_7Achievement@0@6B@` | `0x180154740` | 7 | **2** | `CreateNewObject` `0x180001130`, `CreateInPlaceObject` `0x1800011f0`, `Reconstruct` `0x180001270`, `Construct` `0x1800012f0`, copy-ctor `0x180001470` |
| `??_7AchievementInstance@Achievement@@6B@` | `0x180154700` | 7 | **3** | `CreateNewObject` `0x1800018b0`, copy-ctor `0x180001970`, `FUN_180001a30`, `~AchievementInstance` `0x18009e400` |
| `??_7Goal@Achievement@@6B@` | `0x1801546b0` | 5 | **3** | copy-ctor `0x180001be0`, `FUN_180001cf0` |
| `??_7GoalInstance@Achievement@@6B@` | `0x180154678` | 6 | **3** | `CreateNewObject` `0x180001f50`, copy-ctor `0x180001ff0`, `FUN_180002070` |

Ours, for the record: the `Achievement` ctor/dtor (`achievement_manager.cpp`), the
`AchievementInstance` ctor and its slot-0 deleting dtor (`object/deleting_dtor.cpp`) plus
`CreateInstance` (this lane), and the `Goal`/`GoalInstance` ctor/dtor pairs (`goal.cpp`)
plus their deleting dtors.

### 3.2 What that means, route-scoped (rule 5)

**DLL-PRESENT: NOT PROMOTABLE — and actively harmful.** Every unowned stamper writes the
DLL's table with a **RIP-relative LEA inside a DLL body**, which is not a door. Promoting
a table means our ctor stamps *our* array while `CreateNewObject` — the resource-loader's
factory path, i.e. how essentially every live `Achievement` is actually built — stamps
the DLL's. The result is a **split population**: some objects dispatch our slots, some the
DLL's, from one class. That is worse than either alternative, and it is worse than the
plain no-op that most unpromotable rows are.

**AT CUTOVER: promotable and REQUIRED.** The DLL is gone, so every stamper must be ours
anyway; the coverage question dissolves into the ordinary work of porting those bodies.

This is the `_singleton@FileSystem` shape exactly
(`reference_unpromotable_is_route_scoped_not_permanent.md`): two rows both truthfully
tagged "not promotable" can mean opposite things.

### 3.3 Readiness, which is a SEPARATE question and is also not met

None of the four is `COMPLETE` either. Fifteen slot bodies are unregistered:

| vtable | ours/slots | missing slots |
|---|---|---|
| `??_7Achievement@0@6B@` | **4**/9 | `TypeName` `0x180001040`, `Type` `0x180001020`, `Reconstruct` `0x180001270`, `Destruct` `0x180001370`, `Construct` `0x1800012f0` |
| `??_7AchievementInstance@` | 1/4 | `TypeName` `0x1800017c0`, `Type` `0x1800017a0`, `Persist` `0x18009e4a0` |
| `??_7Goal@Achievement@@` | 4/7 | `TypeName` `0x180001b60`, `Type` `0x180001b40`, `GetFileVersion` `0x18009ee10` |
| `??_7GoalInstance@` | 3/7 | `TypeName` `0x180001e60`, `Type` `0x180001e40`, `Startup` `0x18009f000`, `CheckGoalState` `0x18009f060` |

(`??_7Achievement@0@6B@` moved 3/9 → 4/9 this lane: `?Serialize@Achievement@1@` is slot 5
and is now registered.)

**These fifteen were deliberately NOT ported, and that is a judgement, not an omission.**
Nine of the fifteen are one- or two-instruction constant returns (`Type` is `MOV EAX,
[_typeInfo]; RET`; `GetFileVersion@Goal` is `MOV EAX,0x10000; RET`). They would have been
cheap and they would have moved **no census column at all** — an unpromoted vtable's slots
move nothing, as the census says in its own fold-in note — while §3.2 shows the promotion
they lead toward is harmful today. That is precisely the Wave 20 §4 failure mode: work
that is "safest" only because it is pointless. They are handed on in §6 with what they
actually buy.

**`??_7Goal@Achievement@@6B@` is the cheapest real target on the board:** three tiny slots
and **two** stamp-site bodies (a copy ctor and a deleting-dtor variant, both read and both
small) would make it the first promotable Achievement table.

---

## 4. SECTION THREE — THE FOUR DATA ROWS

**Verdict: zero promoted. Each was decided on its OWN `get_xrefs_to` evidence, not by
analogy (rule 6) — they happen to agree, but for four different specific reasons, and one
of those reasons is a port this very lane wrote.**

Wave 22 lane ERRCFG refuted the do-not-promote rule for 3 of its 5 rows by answering each
row separately, so agreement here is reported as a result, not assumed as a policy.

### 4.1 `?_singleton@AchievementManager@Achievement@@0PEAV12@EA` — `0x1801fc670`

`get_xrefs_to`, split:

* **1 READ**: `?Get@AchievementManager@…@SAPEAV12@XZ` `0x1800079d0` — **ported**.
* **5 WRITES**: `~AchievementManager` `0x18009eac6` (**ported, this lane**), the ctor
  `0x18009eb35` (**ported**, `engine_lifecycle.cpp`), inside `Engine::Shutdown`
  `0x1800db698` and inside `Engine::Startup` `0x1800dbe83` (**ported** —
  `?Startup@Engine@1@` is registered), **and `FUN_18004d930` `0x18004d946` — NOT ported.**

`FUN_18004d930` is the AchievementManager **scalar deleting destructor**: it inlines the
whole `~AchievementManager` body (same null store, same three array frees) and then calls
`operator delete`. It is a *separate body* from the `??1…` this lane ported, so the port
does **not** cover it, and it writes the DLL's cell through a hard-coded RVA.

**DLL-present: DO NOT PROMOTE — HARMFUL.** Unpromoted, `RT_DATA_SLOT` falls through to the
DLL's cell and every writer and the one reader agree by construction. Promoted, that one
unported writer nulls a cell nobody reads while the live cell keeps a dangling pointer.
**At cutover: REQUIRED** — two files under `code/Game/` read this symbol at runtime, and
`engine_lifecycle.cpp:126` already records that an unwritten mint row would leave both
readers seeing null forever.

### 4.2 The three `?_typeInfo@…` rows

| row | addr | readers | writers |
|---|---|---|---|
| `?_typeInfo@Achievement@1@` | `0x1801f9898` | 8 | **0** |
| `?_typeInfo@AchievementInstance@` | `0x1801f98d0` | 4 | **0** |
| `?_typeInfo@GoalInstance@` | `0x1801f9940` | 6 | **0** |

The task asked how `REGISTER_TYPE`/the factory publishes these first. **It does not.**
`standalone/rt_data_mint_tables.inc:1607` declares `static RtObject::BaseTypeInfo
kTypeInfoStorage[104]` and **nothing in the tree ever writes it** — the only two
references in `rt_data_mint.cpp` are a range test and a positive-control accessor. So a
promoted `?_typeInfo@` row publishes a **zeroed** `BaseTypeInfo`: type id `0`, name
`nullptr`.

That alone is decisive, and each row adds its own second reason from its own xrefs:

* **`Achievement`** — among its eight readers are four *unported* DLL internals
  (`FUN_180006d10` ×2, `FUN_180007010`, `FUN_180007070`, `FUN_180009300`) plus the
  unported slots `?Type@` `0x180001020` and `?TypeId@` `0x180001030`. All read the RVA
  directly. Promotion strands them on a type id of 0, which breaks `IsDerivedFrom`.
* **`AchievementInstance`** — one of its four readers is **`FUN_180009a20`**, which is the
  N-element instance serializer **this lane's own `?Persist@AchievementManager@` port now
  calls by address**. Promoting this row would break a port written in the same commit.
  That is as un-analogical as a reason gets.
* **`GoalInstance`** — one of its six readers is `?Initialize@GoalList@` at `0x18009f178`,
  the one BODY row this lane held back (§2.4). It reads the RVA to type-check what the
  factory returned.

**All three: DLL-present DO NOT PROMOTE. At cutover, required — and they must be POPULATED
first, which is a separate piece of work nobody has scoped.** That last point is not
Achievement-specific: it applies to all 104 rows of `kTypeInfoStorage` and is the largest
thing this lane found that is out of its own scope.

One incidental fact worth recording, because it cuts against an easy reading of
`reference_basetypeinfo_name_is_content_only.md`: `?TypeName@Goal@Achievement@@` at
`0x180001b60` is `MOV RAX,qword ptr [0x1801f9910]; RET` — `_typeInfo@Goal` **+0x08**. The
`name` field genuinely is read by the `TypeName` accessors; the memory note's claim is
that those accessors are never *called*, which is a different and still-unrefuted claim.

---

## 5. VERIFICATION, ITS NEGATIVE CONTROL, AND THE DENOMINATOR

### 5.1 I COULD NOT GET A RUN. Plainly.

```
$ tools/game_dir_lock.sh status          # bare, never piped
[gamelock] HELD by 'lane-ALLOC' for 1442s      <- start of this lane
...
[gamelock] HELD by 'ENGINE' for 107s           <- end of this lane
```

Checked twice, at both ends of the lane, and held by a *different* lane each time — so
this was not one long hold that might have lapsed, it was continuous contention, exactly
as `WAVE23_KICKOFF.md` §1b predicted. Queueing behind it was not attempted: a blocking
acquire would have been held from the moment it was granted whether or not this lane
noticed (`reference_backgrounded_lock_acquire_is_held_unnoticed.md`), and this lane's
work is binary-and-source throughout. **No `sa-run`, no `sa-run-menu`, no `BANISHED_AUTOMENU_BUTTON=8` Achievements
route, no `RtSaveTrace` execution.** Nothing in this document is a measurement taken from
a running game, and no such measurement is implied anywhere above.

Before writing that, `standalone/` and the `BANISHED_*` list were grepped as rule 8 and
`reference_grep_the_harness_before_writing_unverifiable.md` require. The harness routes
exist and are the right ones — `BANISHED_AUTOMENU_BUTTON=8` (Achievements) and `=10`
(Goals) open the two dialogs, and `BANISHED_RT_SAVE_TRACE` arms the oracles wired into
§2.3. **The blocker is the shared directory, not the absence of a route.** That is the
difference between "unverifiable" and "not verified this lane", and it is the second.

### 5.2 What WAS verified

1. **Build.** `mingw32-make standalone-debug` — clean, zero warnings from either new
   file, `reimpl-x64.exe` links.
2. **All seven bodies are present in the linked image**, checked with `nm`, not inferred
   from the compiler exiting zero:
   `RtAch_AchievementManager_Dtor` `0x1402bc668`, `_RemoveAll` `0x1402bc6b7`,
   `_ReadAchievements` `0x1402bc779`, `_SetAchievementMet` `0x1402bc889`,
   `RtAch_Achievement_CreateInstance` `0x1402bc9ad`, `RtAch_Achievement_Serialize`
   `0x1402bbfc7`, `RtAch_AchievementManager_Persist` `0x1402bc2f9`.
3. **The census moved, and by the right amount in the right column.** 36 → 43 ported,
   gap 16 → 9, and the gap decomposition went `BODY 7 / DTOR 1` → `BODY 1 / DTOR 0`,
   with `VTABLE 4` and `DATA 4` **unchanged** — which is the shape a bodies-only lane
   must produce. A movement in VTABLE or DATA here would have meant something was wrong.

### 5.3 The negative control (rule 9)

A control must corrupt something the probe actually **compares**, or it indicts a sound
instrument; the mirror also bites.

The only instrument that ran is **the census**, and the property it screens for is *did
this symbol get registered*. So the control corrupts exactly that: `RtAchStateRegister`
was temporarily edited to spell `?RemoveAll@AchievementManager@Achievement@@QEAAXZ`
(one character short of the real `@XZ` terminator) — a **well-formed, plausible mangled
name that the DLL does not contain**, which is the real failure mode
(`reference_resolveexport_phantom_names.md`), not an obviously broken string.

* **CAUGHT.** Achievement `ported` fell 43 → 42 and the gap row reappeared under `BODY`.
  The census does discriminate a registration from its absence, and the +7 in §5.2 is not
  an artefact of the tool counting call sites.
* **WHAT IT DID NOT CATCH, and this is the more important half:** the build stayed
  **clean** and the binary still linked with the body present. A phantom name is a silent
  no-op at compile and link time. **So §5.2's items 1 and 2 cannot detect a misregistered
  port at all** — only item 3 can, and only for the name, never for the body's behaviour.

The control was reverted; the figures in this document are from the unmodified tree.

### 5.4 DENOMINATOR, and WHAT I DID NOT CHECK (rules 9, 10)

**Denominator: 16 rows** — the complete Achievement gap on `wave23/base`, re-derived in
§1, not a subset and not a sample.

Not checked, itemised:

* **Runtime behaviour of all seven bodies.** Not one has executed. Correctness rests
  entirely on reading the shipped bytes.
* **Both READING directions** of `Serialize` and `Persist`, including both `SetSize`
  calls — uncovered even by the oracles that are wired.
* **Everything after emission 1 of `Persist`** — the prefix oracle's stated cost.
* **The registry side effect of `SetAchievementMet`.** It writes real `HKCU` values. No
  differential oracle is wired for it *on purpose* (the DLL's copy would perform the write
  a second time), and `tools/ach_reset.sh` was not run because there was no run to reset.
* **Whether `FUN_180001a30`, `FUN_180001cf0` and `FUN_180002070`** (unowned stampers in
  §3.1) are distinct bodies or aliases of the deleting dtors we already own. They were
  read and they look like deleting-dtor variants; they were **not** byte-compared, and a
  raw byte-diff would be misleading anyway
  (`reference_position_only_diffs_hide_one_body.md`).
* **Whether `Engine::Shutdown`'s port is armed on a default run.** §4.1 records
  `?Startup@Engine@1@` as registered; the Shutdown half was read in `engine_lifecycle.cpp`
  but its registration line was not confirmed. It does not change §4.1's verdict — one
  unported writer (`FUN_18004d930`) already settles it — but the row would be wrong if
  quoted as "all writers but one are ours".
* **The fifteen vtable slot bodies** were read for size and shape only; none was ported
  or verified.

---

## 6. RESIDUALS, EACH WITH A NAMED ADDRESSEE (rule 11)

An addressee is a NAME. "The next lane that touches this" never fires.

| # | item | addressee |
|---|---|---|
| R1 | **`?Initialize@GoalList@Achievement@@` `0x18009f0e0`** — the last Achievement BODY row. Blocked on `Object::Factory`'s lookup + `CreateNewObject` + `Cast<>`, i.e. the Wave 11 object-creation path. Do **not** scope it as an Achievement row; it is a creation-path row wearing an Achievement name. | **the integrator**, to place with whichever Wave 24 lane takes `WAVE11_KICKOFF.md`'s creation path |
| R2 | **`??_7Goal@Achievement@@6B@` is two bodies from promotable** — copy-ctor `0x180001be0` and `FUN_180001cf0`, plus three tiny slots (`Type` `0x180001b40`, `TypeName` `0x180001b60`, `GetFileVersion` `0x18009ee10`, all 2-instruction constant returns). It would be the **first** promotable Achievement table. §3.1 is the derivation; re-derive with `get_xrefs_to 0x1801546b0`. | **the integrator**, as a named Wave 24 lane |
| R3 | **`kTypeInfoStorage[104]` is never written by anything.** Every `?_typeInfo@` promotion in *any* namespace publishes a zeroed `BaseTypeInfo` (type id 0, name null). This is not an Achievement problem; it is a precondition on 104 rows across the board, and no wave doc names it. | **the integrator** — this belongs in the Wave 24 kickoff, not in a lane |
| R4 | **`FUN_18004d930`** is AchievementManager's scalar deleting destructor and the **only** unported writer of `?_singleton@AchievementManager@`. Porting it (it inlines the body this lane just wrote, plus `operator delete`) removes the single obstacle in §4.1. | **the integrator**, as a one-body add-on to whichever lane takes R2 |
| R5 | **`achievement_manager.cpp`'s banner was stale for fourteen waves** while the discharge sat in a sibling file in the same directory. The generalisable defect is that discharging a blocker does not propagate to other functions holding it for the same reason. | **the integrator**, for the Wave 23 close doc's lessons section |

## 7. FILES CHANGED

```
NEW  runtime/src/achievement/achievement_state.cpp        (5 bodies)
NEW  runtime/src/achievement/achievement_serialize.cpp    (2 bodies, 2 oracles)
EDIT runtime/src/achievement/rt_achievement_port.cpp      (wire both under BANISHED_RT_ACHMGR)
EDIT runtime/src/achievement/achievement_manager.cpp      (two stale claims corrected in place)
NEW  docs/runtime_port/WAVE23_LANE_ACHIEVE.md
```

Nothing under `runtime/src/system/` or `tools/runtime_census.py` was touched — the four
files lane ALLOC owns (kickoff §1a) are untouched. Merged nothing.
