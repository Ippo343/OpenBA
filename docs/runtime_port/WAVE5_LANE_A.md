# Wave 5, Lane A — Achievement::Goal / GoalInstance / GoalList

Following [PLAN.md](PLAN.md) §4.2 and §6, and the shape set by
[WAVE4_LANE_C.md](WAVE4_LANE_C.md). Lane A owns the `Achievement::Goal`,
`Achievement::GoalInstance` and `Achievement::GoalList` exports our tree calls,
plus their layouts.

Files owned and written by this lane:

| File | What |
|---|---|
| `runtime/types/achievement/Goal.h` | new — `Goal`, `GoalInstance`, `GoalList`, the two vtable slot tables |
| `runtime/src/achievement/goal.cpp` | the 13 ports + `RtAchGoalRegister()` |
| `docs/runtime_port/WAVE5_LANE_A.md` | this file |

No file outside that set was edited. §5 and §6 list defects and stale comments
found elsewhere; this lane reports them and fixes none of them.

---

## 1. Result

| | |
|---|---|
| Real exports in scope | **15** — the brief listed 19 names, **4 of which do not exist** (§5) |
| Ported | **13** |
| Held back | **2** — `Goal::Serialize`, `GoalInstance::Persist` (§4) |
| `RtAchGoalRegister()` returns | **13** |
| Census after the lane | `Achievement 118 total / 44 called / **13 ported** / 29.5%` |
| Diff sites added | **13** — 9 full oracles, 1 full-byte oracle, 3 shadow self-checks |
| Ports with zero callers | **0** — every one of the 13 export names is resolved by our tree today (verified against `runtime_census.py`'s "no caller" list, which names none of them) |
| Build | `mingw32-make standalone-debug` clean, no warnings |
| `tools/abi_retslot_audit.py` | 0 suspicious of 49 by-value-returning exports |
| **Measured in a run** | **yes, by the orchestrator** — see the box below |

> **STATUS, corrected 2026-09-06 after the orchestrator's integration pass.**
> This section originally read "NOTHING IN THIS LANE HAS EXECUTED", which was
> true when written and is no longer. The orchestrator's autostart +
> pause-menu-quit run (full world teardown, `BANISHED_RT_DIFF=1`, with the §5
> destructor fix live in `863e4f9`) reported **91 sites, 24,050,239 calls, 99
> divergences — all 99 in the two pre-existing Wave 1 sin/cos sites, no new
> divergent site.** This lane's live counts: `Goal::Goal` 139,
> `GoalInstance::GoalInstance` 9, `~GoalInstance` 9, `GoalInstance::Update` 240,
> `GoalList::AreGoalsMetExcept` 240, `Goal::GetInstanceType` 125, every one at
> `divergences=0`.
>
> Two limits on what that proves, kept deliberately visible:
> * The counts above are the orchestrator's, reported to this lane. This lane
>   did not run the game and has not read the log.
> * **Six of the 13 sites are not in that list**, so their `calls=` is unknown
>   or zero for this run — including `Goal::GetText`, `GetGoalCount`,
>   `GoalList::GetGoal`, `IsMet` and `HasFailed`, which need the Goals dialog
>   opened, and `GoalInstance::GetGoal`, which needs `BANISHED_ACH_TRACE`. A
>   clean run is not coverage of the sites it never entered
>   (`reference_uncalled_port_is_not_verified.md`). §7 lists what reaches each.
> * `~Goal` reported no count in the summary sent to this lane; and it is the
>   one site that stays self-check-only end to end even when it does run (§4).

Kill switch: `BANISHED_RT_GOAL=0` (already wired by
`runtime/src/achievement/rt_achievement_port.cpp`).

---

## 2. The 15 functions

All addresses read live in Ghidra 2026-09-06. Retail = `Runtime-x64.dll`, image
base `0x180000000`. Every name below was checked against the **shipped DLL's
export directory with `pefile` before a line of code was written**
(`reference_resolveexport_phantom_names.md`: a name the DLL does not contain
returns NULL silently and the `ResolveAddr` fallback hides it).

| Export | Ordinal | Retail | Ported | Oracle |
|---|---|---|---|---|
| `??0Goal@Achievement@@QEAA@XZ` | 207 | `0x18009ed50` | yes | **full, byte compare** |
| `??1Goal@Achievement@@UEAA@XZ` | 600 | `0x18009edb0` | yes | shadow self-check |
| `??0GoalInstance@Achievement@@QEAA@XZ` | 209 | `0x18009ef00` | yes | shadow self-check |
| `??1GoalInstance@Achievement@@UEAA@XZ` | 601 | `0x18009ef40` | yes | shadow self-check |
| `?GetGoalCount@GoalList@Achievement@@QEBAHXZ` | 2684 | `0x180001700` | yes | full |
| `?GetGoal@GoalList@Achievement@@QEBA?AV?$InternalPtr@VGoalInstance@Achievement@@@Object@@H@Z` | 2683 | `0x180001710` | yes | full |
| `?GetGoal@GoalInstance@Achievement@@QEBA?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ` | 2682 | `0x180001fe0` | yes | full |
| `?GetInstanceType@Goal@Achievement@@UEBA?AVType@Object@@XZ` | 2724 | `0x18009eeb0` | yes | full |
| `?GetText@Goal@Achievement@@QEBA?AVString@System@@XZ` | 2958 | `0x180001ba0` | yes | full, with temp release |
| `?IsMet@GoalInstance@Achievement@@QEBA_NXZ` | 3141 | `0x18009eed0` | yes | full |
| `?HasFailed@GoalInstance@Achievement@@QEBA_NXZ` | 3046 | `0x18009eec0` | yes | full |
| `?AreGoalsMetExcept@GoalList@Achievement@@QEBA_NV?$InternalPtr@VGoalInstance@Achievement@@@Object@@@Z` | 1609 | `0x18009f070` | yes | full |
| `?Update@GoalInstance@Achievement@@UEAAXAEBVTime@System@@AEBVGoalList@2@@Z` | 4492 | `0x18009f010` | yes | full, by save-restore |
| `?Serialize@Goal@Achievement@@UEAA_NAEAVStream@IO@@@Z` | 3692 | `0x18009ee20` | **no** | §4 |
| `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z` | 3324 | `0x18009ef70` | **no** | §4 |

None of these 15 addresses is exported under more than one name (checked across
the whole export directory), so no body here is folded with another — the
question WAVE4_LANE_C.md had to answer for `GetSavePath`.

### The profile build was not needed, and that is worth saying

PLAN §6 step 1 says to locate in the profile build first, because its strings
carry names. Every function in this family is **already exported under its full
MSVC mangled name in retail**, and those manglings spell out the return types,
the parameter types and the const-ness — `?GetGoal@GoalInstance@...?AV?$ExternalPtr@VGoal@Achievement@@@Object@@XZ`
names `ExternalPtr<Goal>` outright. There was nothing left for the profile build
to name, and its offsets must not be used
(`reference_encoding_vtable_offsets_differ_between_builds.md`). **Every offset,
vtable slot and instruction ordering in this lane came out of the RETAIL bytes.**

---

## 3. The layouts, and the evidence

Full commentary with the pinning instruction next to each field is in
`runtime/types/achievement/Goal.h`; every offset carries a `static_assert`.

### `Achievement::Goal` — 0x30, derives `Object::External`

| Off | Field | Pinned by |
|---|---|---|
| `+0x00` | `Object::External` base (vtable, path, refs) | ctor `0x18009ed50` stamps `??_7Base`, `??_7External`, then `??_7Goal`; `~Goal` tail-jumps `??1External@Object@@UEAA@XZ` |
| `+0x20` | `ExternalPtr<UI::StringTable>` | `?GetStringTable@Goal@...` (`0x180001b90`) is the single instruction `*retval = [this+0x20]`, and its MANGLED NAME supplies the type |
| `+0x28` | `System::String` (the goal text) | `GetText` (`0x180001ba0`) copies it and bumps `[data-4]`; `~Goal` runs the String release idiom on it |

`sizeof == 0x30` is not inferred, it is **measured**: the constructor's six qword
stores cover `0x00..0x2f` and stop.

### `Achievement::GoalInstance` — 0x20, derives `Object::Internal`

| Off | Field | Pinned by |
|---|---|---|
| `+0x00` | `Object::Internal` base | ctor `0x18009ef00` `CALL 0x1800bfd10` (`??0Internal@Object@@QEAA@XZ`) |
| `+0x10` | `uint32` flag set — bit 0 met, bit 1 failed | `IsMet` (`&1`), `HasFailed` (`>>1 &1`), `Update` (`OR 1` / `AND ~1` / `OR 2`) — three independent readers |
| `+0x14` | **real padding** | the ctor zeroes `+0x10` as a DWORD (`MOV dword ptr [RBX+0x10],EAX`) and `+0x18` as a QWORD. Nothing writes `+0x14`; it holds allocation residue |
| `+0x18` | `ExternalPtr<Achievement::Goal>` | `GetGoal` (`0x180001fe0`), and `CheckGoalState` (`0x18009f060`) dereferences it and calls THAT object's vtable `+0x40` — so it is a live pointer, not a handle |

### `Achievement::GoalList` — 0x10, and it is EXACTLY one `Collection::Array`

`GoalList` is `Collection::Array<InternalPtr<GoalInstance>>` and nothing else.
`GetGoalCount` returns the `int` at `+0x08` (the Array's `_size`); `GetGoal`
indexes `((void**)*this)[index]` at an 8-byte stride off `+0x00`; the destructor
(`0x18009f420`) zeroes `+0x00` and `+0x08` as two qwords, clearing `_size` and
`_capacity` together, which is the Wave 2 `Collection::Array` teardown shape.
`Initialize` (`0x18009f0e0`) hands `this` straight to the Array resize helper
`FUN_180009d70`.

Two facts about it that are easy to get backwards and are load-bearing:

* **`GoalList` is not an `Object`.** No vtable, no type id, and its destructor is
  exported as `??1GoalList@Achievement@@QEAA@XZ` — **Q**, non-virtual, not **U**.
  (That is also why the corresponding `Goal`/`GoalInstance` `QEAA` destructors
  do *not* exist: those two classes are polymorphic. See §5.)
* **The list OWNS its instances.** `~GoalList` calls each element's vtable slot 0
  with flag 1 — the scalar *deleting* destructor — before freeing the buffer.
  That is the call path through which the §5 defect fires.

The pointer kinds are asymmetric on purpose: the list holds **Internal**
handles to instances, each instance holds an **External** pointer to its Goal
resource.

### Vtables — read out of retail `.rdata`, not the profile build

`??_7GoalInstance@Achievement@@6B@` = `0x180154678`,
`??_7Goal@Achievement@@6B@` = `0x1801546b0`. The gap is `0x38`, so
GoalInstance's table is exactly 7 slots and Goal's begins where it ends — the
same "the next type's table terminates this one" reading as
`reference_vtable_slot_count_from_the_name_string.md`. Slots this lane
dispatches through, each corroborated by a retail `CALL`:

| Class | Byte off | Target | Corroborating call |
|---|---|---|---|
| GoalInstance | `+0x00` | `0x1800020c0` scalar deleting dtor | `~GoalList` `0x18009f420` does `[0](p,1)` |
| GoalInstance | `+0x18` | `0x18009ef70` `Persist` | |
| GoalInstance | `+0x20` | `0x18009f000` `Startup` | |
| GoalInstance | `+0x28` | `0x18009f010` `Update` | |
| GoalInstance | `+0x30` | `0x18009f060` `CheckGoalState` | `Update` does `CALL [RAX+0x30]` |
| Goal | `+0x28` | `0x18009ee20` `Serialize` | |
| Goal | `+0x48` | `0x18009eeb0` `GetInstanceType` | `GoalList::Initialize` does `CALL [..+0x48]` |

All four `??_7` symbols this lane stamps (`Base`, `External`, `Goal`,
`GoalInstance`) are **exported data symbols**, so the ports install the DLL's own
tables and never own one — the Wave 3 finding that PLAN §4.3 risk 3's
vtable-ownership worry is a per-class question, not a general blocker. An object
this lane constructs is byte-identical to one the DLL constructs and dispatches
into the DLL for every slot not ported.

---

## 4. What each diff site can and cannot prove

### The nine ordinary full oracles

`GetGoalCount`, `GetGoal`×2, `GetInstanceType`, `IsMet`, `HasFailed`,
`AreGoalsMetExcept` are read-only on both sides, so "call ours, call theirs,
compare" is the Wave 1 shape with nothing to qualify. The three that return a
class by value run the original into a **private scratch slot**, never into the
caller's.

`GetText` is the `WAVE4_LANE_C.md` `GetSavePath` case: the body ADDREFS, so the
oracle call takes a second reference, and the port releases it again before
comparing. Both sides hand back the **same block**, so the comparison is on the
raw pointers — exact, and strictly stronger than comparing contents.

### `Goal::Goal` is a genuine FULL BYTE oracle

The constructor calls nothing and touches no global: it is six qword stores into
`this` covering `0x00..0x2f` exactly. So the original is run into a private
`0x30` scratch buffer pre-filled with `0xCC` (so a field the original leaves
untouched shows as a difference rather than as an accidental match on zero) and
the two objects are compared byte for byte. This would catch a wrong vtable, a
missed store, or wrong store residue. It is the strongest site in the lane.

### The three shadow SELF-CHECKS, and why they are weaker

`~Goal`, `GoalInstance::GoalInstance` and `~GoalInstance` cannot run the
original at all, and — unlike `Persistence::Set`, where WAVE4_LANE_C could
shadow through the DLL's own already-proven `Get` — **no DLL reader export can
confirm the result**. (The DLL's whole read-only handle-table surface is
`?Get@InternalFactory@Object@@SAPEAV12@XZ`, `?IsPtrNull@`/`?IsPtrValid@
InternalBasePtr@Object@@`; none of them answers "was this object's slot removed
correctly?". See the amendment below, which is where the answer actually is.)

* `~Goal` releases a refcounted `String` that is SHARED with every other copy, so
  even a scratch stand-in would decrement the same refcount. Running the
  original as well would double-free.
* `GoalInstance::GoalInstance` calls `Object::Internal::Internal`, which claims a
  slot in the global handle table. A scratch construction would consume one, and
  the scratch object's `factoryIndex`/`generation` would legitimately differ, so
  even a byte compare of the base would be meaningless.
* `~GoalInstance` releases that slot; running the original too would release it
  twice.

So those three sites verify **only what the body itself is responsible for** —
the vtable re-stamp, the three field stores, and (for `~Goal`) that the String
release took exactly one of its two legal branches. They say nothing about the
base-class halves. This is stated plainly rather than dressed up: a self-check is
weaker evidence than an oracle, and these three are the weakest evidence in the
lane.

#### Amendment (post-integration-run): the base halves CAN be covered, and now are

The paragraph above was right that this lane cannot build an oracle for the base
halves, and **wrong to leave it there** — because Wave 3 already built one.
`RtObject_Internal_Ctor_Shadow` / `_Dtor_Shadow`
(`runtime/src/object/internal.cpp:655` / `:681`, armed with
`BANISHED_RT_INTERNAL_SHADOW=1`, reporting under `BANISHED_RT_DIFF=1`) are
predict-and-verify wrappers on exactly these two base calls. They do not use a
DLL reader — they read the `InternalFactory` struct directly — and they check
what a self-check cannot: `factoryIndex == oldSize`, the table's `_size` and
predicted `_capacity`, `_data[oldSize] == self` on construct; and on destruct
`_size == last`, `generation == 0`, and the swap-remove fixup
(`_data[idx] == moved && moved->factoryIndex == idx`). Wave 3 ran them clean at
93,387 calls each.

**This lane's original lookup choice stepped straight past them.** The three
base-call helpers used `RT_ORIGINAL_ONCE`, i.e. `GetProcAddress`, which bypasses
the runtime registry by design — so `~GoalInstance` would have been the one path
in the tree reaching `~Internal` that the handle-table oracle could not see, and
that is the path the §5 destructor fix has just made live. Fixed: the three
helpers now resolve through `ResolveAddr(Module::Runtime, ...)` (registry-aware,
cached in a function-local static, so one lookup per site per process). The
reasoning, and what it gives up, is in the block comment at the top of
`goal.cpp`.

Net effect on the evidence: with `BANISHED_RT_INTERNAL_SHADOW=1
BANISHED_RT_DIFF=1`, `GoalInstance::GoalInstance` and `~GoalInstance` are
covered by a real oracle on the base half and a self-check on the derived half
(the vtable stamp and the two field stores, which is genuinely all this lane's
code does). `~Goal`'s base half reaches Wave 3's default-on
`RtObject_External_Dtor` port, which has no oracle of its own — so **`~Goal`
remains self-check-only end to end**, and that one should be recorded as such.

### `Update` IS a full oracle, by save-restore

The only state `Update` writes is the `+0x10` dword. `CheckGoalState` is a
`const` member (`UEBA` in its mangling) and every override in our tree is a
query, so calling it twice is safe. The site therefore runs the ORIGINAL first,
captures the resulting flags, restores the dword, then runs ours for real and
compares. Both calls go through the same virtual, so this compares two
implementations rather than comparing ours to itself. The cost — one extra
`CheckGoalState` per goal per tick — is paid only under `BANISHED_RT_DIFF=1`.

### Why `Goal::Serialize` and `GoalInstance::Persist` are HELD BACK

Not because they are hard. Both are ~15 instructions and are fully reversed
below, so a later lane can port them without redoing any of this work. They are
held back because **there is no oracle**, and the brief's own rule applies: an
unmeasured port is not a port, and a stream port that is wrong is the single most
expensive defect class this project has (`reference_slot3_wrapper_must_resolve_first.md`,
and `standalone/reg_goal.cpp`'s comment on slot `0x140152868` — a skipped
`GoalInstance::Persist` base call desynchronised every retail-authored save from
that object onward, and our own saves could not see it because our save leg
skipped the same bytes).

Concretely: running both implementations would **double-consume the stream**;
the stream is compressed, so it cannot be rewound to re-run; and there is no
scratch stream to run the original against because the byte content depends on
live `ExternalFactory` state. `RT_DIFF_SHADOW` would only be able to assert that
our own body did what our own body does.

Both stay on the DLL and both keep working exactly as today: `reg_goal.cpp`
already resolves `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z`
(the **UEAA** name — the correct one) into the exe dispatch slot `0x140152868`,
and `address_registry.cpp:1240` resolves `Goal::Serialize` the same way.

**The reversed bodies, so the next lane starts from here.** Both share an
identical eight-instruction prologue that builds a stack `IO::Serializer`:

```
    Serializer ser;                       // 0x10 bytes: {IO::Stream* stream; Encoding* enc;}
    ser.stream = param_1;                 // the Stream&
    ser.enc    = nullptr;
    ReaderWriter* rw = *(ReaderWriter**)((char*)param_1 + 0x10);   // Stream+0x10
    void** rwvt = *(void***)rw;
    ser.enc = (*(int*)((char*)param_1 + 8) == 0)                   // Stream+0x08 = direction
                ? rwvt[3](rw, &ser, 0)     // vtable +0x18 — READ  leg
                : rwvt[4](rw, &ser, 0);    // vtable +0x20 — WRITE leg
```

then the payload, then `if (ser.enc) ser.enc->vt[0](ser.enc, 1);` — the
encoding's scalar *deleting* destructor, so the encoding is heap-allocated per
call and owned by the caller.

```
Goal::Serialize (0x18009ee20)  -> returns true (MOV AL,1) unconditionally
    FUN_1800042a0(this + 0x20, &ser);   // ExternalPtr<UI::StringTable>::Serialize
    System::String::Serialize(this + 0x28, &ser);   // 0x1800c9930

GoalInstance::Persist (0x18009ef70)  -> void
    FUN_180005dc0(this + 0x18, &ser);   // ExternalPtr<Achievement::Goal>::Serialize
    ser.enc->vt[0x98](ser.enc, this + 0x10, 0);     // Serialize(uint&)
```

Field order is therefore **ExternalPtr first, payload second** in both, which is
the byte order a load must consume.

Two measurements worth banking from that reversing:

1. **`FUN_1800042a0` and `FUN_180005dc0` are the SAME FUNCTION, twice.** They are
   two MSVC instantiations of `Object::ExternalPtr<T>::Serialize`, byte-identical
   apart from the `__typeInfo` global each hands to `Base::IsDerivedFrom` — one
   for `UI::StringTable`, one for `Achievement::Goal`. Whoever names one has
   named both. (This is the template-instantiation duplication PLAN §4.3 risk 1
   warns about, showing up outside `Collection` for once.)
2. **The encoding vtable offset used here is `+0x98`, read out of retail.** It
   matches `reference_encoding_vtable_offsets_differ_between_builds.md`'s retail
   triple (`+0x78` bool / `+0x98` uint / `+0xa0`) and NOT the profile build's
   (`+0x90`/`+0xb0`/`+0xb8`). `ExternalPtr::Serialize` uses `+0x78` for its
   "is non-null" bool and `+0x98` for its reference id, which independently
   corroborates two of those three from a body in this lane's own scope.

---

## 5. THE PHANTOM NAMES — verdict, and a LIVE defect outside this lane

`tools/export_name_audit.py` flagged two names. Checked directly against the
shipped DLL's export directory with `pefile`, **four** of the brief's names are
absent:

```
MISSING  ??1Goal@Achievement@@QEAA@XZ
MISSING  ??1GoalInstance@Achievement@@QEAA@XZ
MISSING  ?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z
MISSING  ?~Goal@Achievement@@QEAA@XZ
OK   ord=600   0x18009edb0  ??1Goal@Achievement@@UEAA@XZ
OK   ord=601   0x18009ef40  ??1GoalInstance@Achievement@@UEAA@XZ
OK   ord=3324  0x18009ef70  ?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z
```

**Why, in one line each:**

* `??1X@@QEAA@XZ` is the mangling of a **non-virtual** destructor. `Goal` and
  `GoalInstance` are polymorphic, so MSVC emits and exports `??1X@@UEAA@XZ` — the
  `Q`→`U` difference is the whole bug. (`GoalList`, which really is non-virtual,
  *does* export `??1GoalList@Achievement@@QEAA@XZ` — the control case.)
* `?Persist@...QEAA...` is the same mistake on a member that is virtual (`UEAA`).
* `?~Goal@Achievement@@QEAA@XZ` **is not a valid MSVC mangling at all.** MSVC
  spells a destructor `??1`, never `?~`. This name can never resolve in any
  build, and it was written as a *fallback for the failure of the first name* —
  a fallback that had no chance of working.

### The live defect: a skipped base destructor, in three places

| Call site | Name asked for | Fallback | Result |
|---|---|---|---|
| `code/Game/Goal/statgoal.cpp:404` | `??1Goal@...QEAA@XZ` | `:408` `?~Goal@...QEAA@XZ` | `s_fn_GoalDtor` is **NULL forever** |
| `code/Game/Goal/timegoal.cpp:158` | `??1Goal@...QEAA@XZ` | none | `s_fn_GoalDtor` is **NULL forever** |
| `code/Game/Goal/timegoal.cpp:123` | `??1GoalInstance@...QEAA@XZ` | none | `s_fn_GoalInstanceDtor` is **NULL forever** |

**It is a LEAK, not a crash** — every one of the three is guarded
(`if (s_fn_GoalDtor) s_fn_GoalDtor(self);`), so the null pointer is never
called. There is no EXEC-of-0 here. What happens instead is that the base
destructor silently does not run, and then `fn_MemFreeStatic` frees the object
anyway. Severity is **not the same in all three**:

* `statgoal.cpp:404` / `timegoal.cpp:158` (`~Goal` skipped) — the goal's text
  `String` reference and `Object::External`'s path `Filename` are never released,
  and `External`'s `0x10`-stride owning array at `+0x10` is never freed. A
  bounded per-Goal leak on teardown. Goals are resources loaded once per game, so
  this is small.
* **`timegoal.cpp:123` (`~GoalInstance` skipped) is materially worse and I would
  raise it above the other two.** That path skips `Object::Internal::~Internal`
  (`0x1800bfdb0`), whose body is the InternalFactory **swap-remove**: it zeroes
  the generation, moves the last live entry into this object's slot, fixes up
  that moved object's `factoryIndex`, and shrinks the table. Skipping it leaves
  a handle-table entry pointing at memory that is freed on the very next line,
  and leaves the factory count too high — and `InternalBasePtr::CheckPtr` is a
  bare `RET` in every shipped build
  (`reference_checkptr_is_a_ret_in_every_build.md`), so nothing will ever
  detect the stale handle. That is the
  `reference_internalfactory_swapremove_wild_write.md` family exactly.
  `TimeGoal_Slot11` is the **vtable slot 0** of `TimeGoalInstance`
  (`code/Game/Goal/timegoalinstance.cpp:290`) — the scalar *deleting* destructor
  — and `GoalList::~GoalList` calls precisely that, with flag 1, for every
  element. So this fires on **every** teardown of a goal list that contains a
  time goal: every tutorial teardown, every quit-to-menu, every achievement
  reset.

### What this lane did NOT do about it, deliberately

The port registers the **correct** names, `??1Goal@Achievement@@UEAA@XZ` and
`??1GoalInstance@Achievement@@UEAA@XZ`. It does **not** also register the three
phantom spellings, even though doing so would make `ResolveExport` at those call
sites return our port and would make the leak disappear today. Two reasons:

1. It would fix the symptom by **inventing an export the DLL never had**, which
   is a trap for the next reader and would make `export_name_audit.py` stop
   reporting a name that is genuinely wrong at its source.
2. The fix belongs at the call site, and `code/Game/Goal/*.cpp` is not this
   lane's to edit (the brief says so explicitly).

**The fix, for whoever owns those files:** change the three `ResolveExport`
arguments to the `UEAA` spellings and delete the `?~Goal@...` fallback at
`statgoal.cpp:407-409`. Nothing else needs to change — the signatures and the
call shapes are already correct, and with this lane's port registered those
names now resolve to our implementations. After the change, drop the
`if (s_fn_...)` guards too: retail calls its base destructor unconditionally, and
the guard is exactly the "faithful crash covered with a null check" that
CLAUDE.md forbids — here it converted a hard failure into a silent leak that has
been live for months.

---

## 6. Comments to correct (PLAN §6 step 8) — all OUTSIDE this lane's files

None of these are in files this lane owns, so none were changed. Each is a claim
this lane **disproved with the shipped binary**, with the file and line.

| Where | The stale claim | What is true |
|---|---|---|
| `engine/funcs/engine_funcs.h:5241` | `// ?~Goal@Achievement@@UEAA@XZ — destroys Goal base` | Not a valid MSVC mangling. The symbol is `??1Goal@Achievement@@UEAA@XZ`, ordinal 600, `0x18009edb0`. The code beneath it (`engine_funcs.cpp:4686`) already uses the right name; only the comment is wrong — but this is very likely the comment `statgoal.cpp:408`'s dead fallback was copied from. |
| `engine/funcs/engine_funcs.h:5246` | `// ?Goal@Achievement@@QEAA@XZ — default ctor` | Same shape. The symbol is `??0Goal@Achievement@@QEAA@XZ`, ordinal 207. |
| `engine/funcs/engine_funcs.cpp:4711` | resolves `?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z` | **Phantom.** This resolve returns NULL on every run; `fn_GoalInstance_Persist` is populated only by the `ResolveAddr(Module::Runtime, 0x18009ef70)` fallback on the next line. It works today **by accident of that fallback** — the exact "the ResolveAddr fallback hides it" mechanism `reference_resolveexport_phantom_names.md` describes. Correct name: `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z`, ordinal 3324. Not urgent (the fallback is correct), but it should be spelled right before someone deletes the fallback. |
| `engine/funcs/engine_funcs.h:5260-5263` | `// Achievement::GoalInstance::Persist (Runtime 0x18009ef70)` with no mangled name | Add the `UEAA` name, since the `.cpp` next to it uses a phantom. |
| `code/Game/Goal/statgoal.cpp:396-397` | `// The base dtor is at a dispatch slot; we call it via the parent vtable / Since ~Goal is called from the dtor, we need its address` | Neither sentence describes what the code does. `~Goal` is a plain export (ordinal 600), not a dispatch slot, and the code resolves it by name, not through a vtable. |
| `code/Game/Goal/statgoal.cpp:406` | `// Fallback: try ordinal` | The fallback does not try an ordinal; it tries a second, invalid, mangled name. If an ordinal fallback were wanted it would be `ResolveOrdinal(600)`. |

Nothing in `PLAN.md` or `CLAUDE.md` was disproved by this lane. One PLAN claim
was **confirmed** for a new namespace: §4.3 risk 3's vtable-ownership problem
does not bite here either, because all four `??_7` symbols involved are
exported (§3).

---

## 7. What a run should show, and what would falsify this lane

For the orchestrator's integration pass. Expected `RtDiffSummary()` shape under
`BANISHED_RT_DIFF=1`:

| Site | Expect `calls=` | Reached by |
|---|---|---|
| `Achievement::Goal::GetInstanceType` | > 0 on any game load with goals | the DLL's own `GoalList::Initialize` calls it through the exe subclass vtable slot 9, which `code/Game/Goal/goal_exe_helpers.cpp:214` fills with `ResolveExport(...)` — i.e. **with this port**. This is the first site in the whole runtime port to be entered from INSIDE DLL code without a detour. |
| `Achievement::Goal::Goal` / `~Goal` | > 0 per goal construct/destroy | `engine_funcs.cpp:4686/4692` → `buildinggoal.cpp`, `populationgoal.cpp`, `finishtutorialgoal.cpp` |
| `GoalInstance::GoalInstance` | > 0 | `timegoalinstance.cpp:244`, `finishtutorialinstance.cpp:314` |
| `GoalInstance::~GoalInstance` | > 0 | `finishtutorialgoal.cpp:212`. **Note `timegoal.cpp:123` will NOT show up here** — that is the §5 defect, and a `calls=` count lower than the number of TimeGoalInstances destroyed is the observable signature of it. |
| `GoalList::GetGoalCount` / `GetGoal` / `Goal::GetText` | > 0 whenever the Goals dialog is opened | `engine_funcs.cpp:2625/2724/2728` |
| `GoalInstance::IsMet` / `HasFailed` | > 0 | `engine_funcs.cpp:2631/2637` |
| `GoalList::AreGoalsMetExcept` / `GoalInstance::Update` | > 0 during a tutorial or achievement run | exe dispatch slots `0x140152878` / `0x140152880` (`standalone/reg_goal.cpp:80,82`) |
| `GoalInstance::GetGoal` | 0 unless `BANISHED_ACH_TRACE` is set | `standalone/achievement_probe.cpp:74` is its only caller — **this one is expected to read `calls=0` on an ordinary run, and that is not evidence it is right.** |

A run that reports every site at `calls=0` means the wave did not register (check
`BANISHED_RT_GOAL`/`BANISHED_RT_ACHIEVEMENT`) — not that the ports are clean.
`docs/runtime_port/WAVE1.md`'s wrong `Quaternion::operator*` survived a whole
wave that way.

Suggested runs: `sa-run` with `BANISHED_RT_DIFF=1` (front end + new game),
`BANISHED_AUTOTUTORIAL` (drives `Update` and `AreGoalsMetExcept` hard, and is the
save that exercises `Persist` on the held-back side), and a quit-to-menu so the
`~GoalList` teardown path runs.
