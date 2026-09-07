# Wave 5, lane B — Achievement / AchievementInstance / AchievementManager

Following [PLAN.md](PLAN.md) §4.2 and §6. Lane B owns the 19 code exports and 3
data exports the brief listed on these three classes.

Files owned and written by this lane — **no edits outside this set**:

| File | What |
|---|---|
| `runtime/types/achievement/AchievementManager.h` | new — `Achievement`, `AchievementInstance`, `AchievementManager`, plus two minimal `…View` structs for lane A's `GoalList`/`GoalInstance` |
| `runtime/src/achievement/achievement_manager.cpp` | 18 ports + `RtAchManagerRegister()` |
| `docs/runtime_port/WAVE5_LANE_B.md` | this file |

§6 corrections this lane owes to files it does not own are in §7, for the
orchestrator to apply.

---

## 1. Result

| | |
|---|---|
| Ported | **18 of 19** code exports |
| Held back | `?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z` — see §5 |
| Not portable before cutover | the 3 DATA exports (`?_singleton@AchievementManager@…`, `?_typeInfo@Achievement@1@…`, `?_typeInfo@AchievementInstance@…`) — see §6 |
| `RtAchManagerRegister()` returns | **18** |
| Census (`tools/runtime_census.py --namespace Achievement`) | Achievement **0 → 31 of 49 called (63.3%)**; 18 of those 31 are this lane's, 13 are lane A's `goal.cpp`. Verified by counting one-line `RegisterRuntimeExport("` literals per file: goal.cpp 13 + achievement_manager.cpp 18 = 31 |
| Diff sites added | **18** — 9 full oracles, 9 shadow verdicts |
| `tools/abi_retslot_audit.py` | **0 suspicious of 49**, unchanged |
| `tools/export_name_audit.py` | none of this lane's 22 names is a phantom; all 19 code exports verified present in the shipped export directory with `pefile` **before any body was written** (§2) |
| Build | `mingw32-make standalone-debug` clean, no new warnings |
| **Measured in a run** | **NOTHING. No `sa-run`, no `sa-stress`, no `BANISHED_RT_DIFF=1` pass.** The brief reserves integration runs to the orchestrator. Every claim below is static: decompile + disassembly + a clean compile. Read none of it as "observed agreeing." |

---

## 2. The 22 symbols — export verification

`pefile` over the shipped `C:\GOG Games\Banished\Runtime-x64.dll`, run before
writing code, because a `ResolveExport` name the DLL does not contain returns
NULL silently, the port still registers, and the diff then reports `calls=0`
forever (`reference_resolveexport_phantom_names.md`). **0 MISSING of 19.**

```
OK  0x1800079d0 ord=2403  ?Get@AchievementManager@Achievement@@SAPEAV12@XZ
OK  0x1800079b0 ord=2434  ?GetAchievementCount@AchievementManager@Achievement@@QEBAHXZ
OK  0x1800079c0 ord=3091  ?IsAchievementAwarded@AchievementManager@Achievement@@QEBA_NH@Z
OK  0x18009e8a0 ord=2433  ?GetAchievement@AchievementManager@Achievement@@QEBA?AV?$ExternalPtr@VAchievement@1@@Object@@H@Z
OK  0x18009e8f0 ord=2435  ?GetAchievementInstance@AchievementManager@Achievement@@QEBA?AV?$InternalPtr@VAchievementInstance@Achievement@@@Object@@H@Z
OK  0x18009e6b0 ord=3879  ?SetEventHandler@AchievementManager@Achievement@@QEAAXP6AXH@Z@Z
OK  0x18009e860 ord=3826  ?SetAchievements@AchievementManager@Achievement@@QEAAXV?$ExternalPtr@VExternalList@Object@@@Object@@@Z
OK  0x18009e5c0 ord=3572  ?Reset@AchievementManager@Achievement@@QEAAXXZ
OK  0x18009e6c0 ord=4470  ?Update@AchievementManager@Achievement@@QEAAXAEBVTime@System@@@Z
OK  0x18009e910 ord=3321  ?Persist@AchievementManager@Achievement@@QEAAXAEAVStream@IO@@@Z   [HELD BACK]
OK  0x18009e3b0 ord=26    ??0AchievementInstance@Achievement@@QEAA@XZ
OK  0x18009e440 ord=3824  ?SetAchievement@AchievementInstance@Achievement@@QEAAXV?$ExternalPtr@VAchievement@1@@Object@@@Z
OK  0x18009e340 ord=4469  ?Update@AchievementInstance@Achievement@@QEAAXAEBVTime@System@@@Z
OK  0x18009e2f0 ord=3140  ?IsMet@AchievementInstance@Achievement@@QEBA_NXZ
OK  0x18009e2b0 ord=3045  ?HasFailed@AchievementInstance@Achievement@@QEBA_NXZ
OK  0x180001960 ord=2686  ?GetGoalList@AchievementInstance@Achievement@@QEBAAEBVGoalList@2@XZ
OK  0x1800013e0 ord=2973  ?GetTitle@Achievement@1@QEBA?AVString@System@@XZ
OK  0x180001420 ord=2564  ?GetDescription@Achievement@1@QEBA?AVString@System@@XZ
OK  0x1800013a0 ord=2931  ?GetSpriteName@Achievement@1@QEBA?AVString@System@@XZ
DATA 0x1801fc670 ord=4798  ?_singleton@AchievementManager@Achievement@@0PEAV12@EA
DATA 0x1801f9898 ord=4823  ?_typeInfo@Achievement@1@0VBaseTypeInfo@Object@@A
DATA 0x1801f98d0 ord=4824  ?_typeInfo@AchievementInstance@Achievement@@0VBaseTypeInfo@Object@@A
```

Same pass also verified the six dependencies this lane calls but does not port:
`??0Internal@Object@@QEAA@XZ` 0x1800bfd10, `?IsDerivedFrom@Base@Object@@QEBA_NI@Z`
0x1800c0390, `?ReadAchievements@AchievementManager@…` 0x18009eb90,
`?SetAchievementMet@AchievementManager@…` 0x18009eca0,
`?CreateInstance@Achievement@1@…` 0x18009df80, `?Initialize@GoalList@Achievement@@…`
0x18009f0e0, plus the exported vtable `??_7AchievementInstance@Achievement@@6B@`
0x180154700 (ord 1383) and `?AreUserPackFilesEnabled@FileSystem@IO@@QEBA_NXZ`
0x1800acfc0.

**No export folding**: none of the 19 addresses is exported under a second name.

### Hidden return slots — five, all handled, all cross-checked

Counted against the MANGLED name, never against Ghidra's guess
(`reference_hidden_retval_and_int_pointer_args.md`). Five of the 19 return a
class by value and therefore take the return slot as an explicit argument:

| Export | Return | ABI |
|---|---|---|
| `GetAchievement` | `ExternalPtr<Achievement>` | RCX=this, **RDX=retval**, R8D=index |
| `GetAchievementInstance` | `InternalPtr<AchievementInstance>` | RCX=this, **RDX=retval**, R8D=index |
| `GetTitle` / `GetDescription` / `GetSpriteName` | `String` | RCX=this, **RDX=retval** |

`GetGoalList` returns a *reference* (`AEBVGoalList`) — a plain pointer in RAX,
no slot. Every existing typedef in our tree already agrees:
`engine_funcs.h:1533` (`fn_AchievementManager_GetAchievement_t`), `:1508`,
`:1514`, `:1519`, and `standalone/achievement_probe.cpp:42`
(`GetAchInstance_t`). `tools/abi_retslot_audit.py` reports 0 suspicious of 49,
unchanged by this lane — consistent with the by-hand check, not a substitute
for it.

---

## 3. Layouts

Full evidence per field is in `runtime/types/achievement/AchievementManager.h`;
the summary:

```
Achievement            : Object::External, 0x58
  +0x20 stringTable  +0x28 title  +0x30 description
  +0x38 spriteSheet  +0x40 spriteName  +0x48 goals : Array<ExternalPtr<Goal>>

AchievementInstance    : Object::Internal, 0x28   (CreateInstance allocates exactly 0x28)
  +0x10 goalList : GoalList (== Array<InternalPtr<GoalInstance>>)
  +0x20 achievement : ExternalPtr<Achievement>
  vtable = ??_7AchievementInstance@Achievement@@6B@ = 0x180154700 (exported)

AchievementManager     : 0x48, a process singleton
  +0x00 _achievements          ExternalPtr<ExternalList>
  +0x08 _completed             Array<bool>
  +0x18 instances              Array<InternalPtr<AchievementInstance>>   (our name)
  +0x28 eventHandler           void(*)(int)                             (our name)
  +0x30 updateTimes            Array<System::Time>                      (our name)
  +0x40 _lastAchievementUpdate int
```

`_achievements`, `_completed` and `_lastAchievementUpdate` are the ENGINE's own
names, out of the profile build's assert strings in
`Achievement\AchievementManager.cpp`:
`"Only a single achievement manager can be created!"` and
`"!_completed[_lastAchievementUpdate]"` / `"Error, updating a complete achievement!"`.
The other three are ours and are labelled as ours in the header.

Two independent whole-struct confirmations, not just per-field reads:

* the default ctor (retail 0x18009eb00, profile 0x180028b40) zeroes exactly
  eight qwords +0x00..+0x38 and one dword at +0x40 in **both** builds;
* the profile copy ctor (0x180029a40) walks the whole object and needs a deep
  Array copy at exactly +0x08, +0x18 and +0x30, a raw qword at +0x00 and +0x28,
  and a raw dword at +0x40. No field missed, none left over.

**Cross-build offsets were checked, not assumed** — the standing rule from
`reference_encoding_vtable_offsets_differ_between_builds.md`. The profile build
is used here for NAMES only; every number in the header and the port came out
of the retail bytes.

---

## 4. The 18 ports

Retail addresses are `Runtime-x64.dll`; where a profile address was actually
looked up it is given, and where it was not, this table says so rather than
guessing (an earlier draft of the source attributed `Get` to profile
0x180029a40, which is the **copy constructor** — corrected before landing).

| # | Export | Retail | Profile | What it does | Oracle |
|---|---|---|---|---|---|
| 1 | `AchievementManager::Get` | 0x1800079d0 | not looked up | `return _singleton` | **full** — pointer compare |
| 2 | `GetAchievementCount` | 0x1800079b0 | not looked up | `_achievements->_count`, UNGUARDED deref | **full** |
| 3 | `IsAchievementAwarded` | 0x1800079c0 | not looked up | `_completed[i] != 0`, no bounds check | **full** |
| 4 | `GetAchievement` | 0x18009e8a0 | not looked up | `_achievements->_resources[i]`, filtered by `Base::IsDerivedFrom(obj, Achievement::_typeInfo[0])` | **full** — theirs into a scratch slot |
| 5 | `GetAchievementInstance` | 0x18009e8f0 | 0x1800293a0 | `_instances[i]`; a raw pointer copy, no refcount, no handle check | **full** |
| 6 | `AchievementInstance::GetGoalList` | 0x180001960 | not looked up | `return this+0x10` | **full** |
| 7 | `AchievementInstance::IsMet` | 0x18009e2f0 | not looked up | every goal has bit0 set and bit1 clear; an EMPTY list is not met | **full** |
| 8 | `AchievementInstance::HasFailed` | 0x18009e2b0 | not looked up | any goal has bit1 set | **full** |
| 9-11 | `Achievement::GetTitle` / `GetDescription` / `GetSpriteName` | 0x1800013e0 / 0x180001420 / 0x1800013a0 | not looked up | copy the String at +0x28/+0x30/+0x40 **with an AddRef** | **full**, with the oracle releasing its own temporary (the Wave 4 `GetSavePath` shape) |
| 12 | `??0AchievementInstance` | 0x18009e3b0 | 0x1800283a0 | `Object::Internal::Internal`, stamp the exported vtable, zero the GoalList and the achievement ptr; returns `this` | shadow (§4.1) |
| 13 | `AchievementInstance::SetAchievement` | 0x18009e440 | not looked up | store +0x20, copy the achievement's goal array into a temp, `GoalList::Initialize` | shadow (§4.2) |
| 14 | `AchievementInstance::Update` | 0x18009e340 | not looked up | dispatch every goal through vtable +0x28 with `(time, this->goalList)` | shadow, **weak** (§4.3) |
| 15 | `AchievementManager::SetEventHandler` | 0x18009e6b0 | not looked up | one store to +0x28 | shadow — field readback |
| 16 | `AchievementManager::SetAchievements` | 0x18009e860 | not looked up | store the list, size `_updateTimes` and `_completed`, `ReadAchievements()` | shadow — DLL `GetAchievementCount` agrees with both array sizes |
| 17 | `AchievementManager::Reset` | 0x18009e5c0 | not looked up | size `_instances`, fill with `Achievement::CreateInstance` | shadow (§4.4) |
| 18 | `AchievementManager::Update` | 0x18009e6c0 | 0x180028cd0 | the rotating updater (§4.5) | shadow, mostly weak (§4.5) |

### 4.1 The constructor's shadow

Running the DLL's constructor a second time to compare would take a **second
handle-table entry**, so it is not done. The verdict is instead: the vtable we
stamped equals the exported `??_7AchievementInstance@Achievement@@6B@`, the
GoalList and achievement pointer are zero, and the base constructor left a
non-zero generation. That proves the object is SHAPED like retail's; it does
not prove the handle-table entry, which is Wave 3's opt-in
`BANISHED_RT_INTERNAL_SHADOW` oracle's job. This port calls the Internal ctor
**through the registry** (`ResolveExport`) precisely so that oracle still sees
the object — see §4.6.

### 4.2 `SetAchievement`

Retail's byte order matters and is preserved: `MOV [RCX+0x20],RDX` happens
FIRST, before the goal-array copy that would fault on a null achievement. The
temporary `Array<ExternalPtr<Goal>>` is **not** destroyed by this function,
because `GoalList::Initialize` takes it by value and the MS x64 ABI makes the
CALLEE destroy a by-value class parameter — retail has no cleanup there either.
Shadow verdict: the stored pointer is the one we were handed and
`goalList._size == achievement->goals._size`. A goal count of 0 is what makes
`IsMet` return false forever, so that is the invariant worth asserting.

### 4.3 `AchievementInstance::Update` — the weakest site, stated plainly

Dispatching a tick into N goal instances produces no value to compare, and it
cannot be run twice because each goal instance accumulates state. The shadow
verdict is therefore only "the loop visited exactly `_size` goals". **That is
an execution counter, not an equivalence proof.** For this site the diff
summary's `calls=` column means "how often this ran", not "how often ours
agreed with theirs" (`reference_uncalled_port_is_not_verified.md` in reverse:
here the count IS present but the comparison is empty).

### 4.4 `Reset` is the INSTANCE BUILDER

Not a "clear the awards" call. It sizes `_instances` to the achievement count
and fills every slot with `Achievement::CreateInstance`; nothing else in the
engine ever writes that array on a new game. The comment at
`code/Game/Game/gameupdateloop_load.cpp`'s step 30b already says this, and this
lane's decompile confirms it word for word.

It cannot be diffed by running it twice: a second run allocates a whole second
set of instances and leaks the first (`SetSize` to the same size is a no-op that
frees nothing). Shadow verdict: right length, no null slot, and every instance
points back at the achievement that built it.

### 4.5 `AchievementManager::Update` — what it actually does

The retail body inlines `AchievementInstance::Update` and `::IsMet`; the
profile build (0x180028cd0) spells both out, which is what made the shape
readable. In order:

0. **If any user pack file (mod) is mounted, do nothing at all.** Retail inlines
   this as `CMP dword [FileSystem::_singleton+0x38],0 / JG done`; the profile
   build names it `IO::FileSystem::AreUserPackFilesEnabled`. This is the whole
   mechanism behind `BANISHED_ACH_IGNORE_MODS` and behind
   `reference_mods_off_breaks_save_loading.md`'s "achievements need the mods
   OUT" — with a mod mounted, not one line below ever runs.
1. Add the frame delta to `_updateTimes[i]` for every not-yet-completed
   achievement; hard-zero the slot of every completed one.
2. Walk `_lastAchievementUpdate` forward past completed achievements, counting.
   If it skips all of them, every achievement is done — return.
3. Update the ONE instance at the cursor **with its whole accumulated time**,
   zero that slot, and if the instance is now met and not already flagged, call
   `SetAchievementMet` and fire the event handler.
4. Advance the cursor, wrapping.

**So each achievement is evaluated once every `_size` ticks, with `_size`
ticks' worth of accumulated time** — which is the entire reason the
`_updateTimes` array exists. Nothing in our tree documented that; it is the one
genuinely non-obvious thing this lane recovered.

Two transcription notes. Retail peels the first iteration of the skip loop into
an `if` around a `do…while`; the profile build's un-peeled `while` is used here
because the two are provably the same loop (`n` is the same at every exit) and
the profile's reads correctly. And retail RE-READS `_instances._size` for the
final wrap rather than reusing the register from the top of the function, so
this does too.

Shadow verdict: the cursor stayed inside `[0, _size)`, and on a tick that
awarded something, the DLL's own `IsAchievementAwarded` reads back true. Most
ticks award nothing and therefore prove only the cursor invariant — said here
rather than left for a reader to discover.

### 4.6 One deliberate departure from Wave 4 lane C's resolution rule

Lane C's rule (`runtime/src/io/persistence.cpp`) is that a port should ask the
DLL directly (`ResolveAddr`) for its own dependencies, so it is correct
standing alone. This lane follows that for the three `Collection::Array<T>::SetSize`
instantiations (0x180008b90 / 0x180008ff0 / 0x180008d60) and the array
copy-assign (0x180003970) — none of which is exported or owned by any lane.

It deliberately does **not** follow it for four dependencies that touch state
another lane owns: `Object::Internal::Internal`, `Base::IsDerivedFrom`,
`ReadAchievements` and `SetAchievementMet`. Those go through `ResolveExport`,
i.e. through the registry. Going around the registry for the Internal
constructor would make Wave 3's shadow replica of the handle table MISS every
`AchievementInstance` and report a false divergence; for shared state there has
to be exactly one owner.

---

## 5. Held back: `AchievementManager::Persist` (0x18009e910)

Fully reversed, deliberately not ported. The body:

```
Serializer s;                      // INLINED, on the stack: {Stream* , ReaderWriter*}
s._stream = stream;
s._rw = (stream->direction == 0) ? stream->encoding->vtable[+0x18](…)   // reader
                                 : stream->encoding->vtable[+0x20](&s, 0, …);  // writer
if (stream->direction == 0) RemoveAll(this);          // load: drop every instance
int count = _instances._size;
s._rw->vtable[+0xa0](s._rw, &count, 0);               // serialize the count
if (stream->direction == 0) Array<InternalPtr>::SetSize(&_instances, count);
FUN_180009a20(&s, count, _instances._data);           // per element: InternalBasePtr::Serialize
                                                      // + an inlined IsDerivedFrom(AchievementInstance)
                                                      // filter that NULLS a wrong-typed slot
s._rw->vtable[0](s._rw, 1);                           // deleting dtor
```

**Why it is held back, and it is not squeamishness about size:**

1. **No oracle exists, of any kind.** Wave 4 lane A measured that constructing
   an `IO::Serializer` CONSUMES OR EMITS A 12-BYTE HEADER — three 4-byte reads
   or writes in `BinaryReader`/`BinaryWriter`'s constructor plus a further 4 in
   `~BinaryReader` (`runtime/types/io/Serializer.h`, "CONSTRUCTING A SERIALIZER
   MOVES THE STREAM CURSOR"). Running the DLL's `Persist` as a differential
   oracle would emit a second header and desync the save from that point on.
   A shadow verdict is no better: the state it would check IS the stream.
2. **The failure mode is the worst one this project has.** A wrong `Persist`
   consumes the wrong number of bytes and surfaces as a corrupt record on some
   innocent later component — `reference_slot3_wrapper_must_resolve_first.md`,
   twice, and `project_tutorial2_absorbresource_dead_init.md`.
3. It is the only function in this lane's 19 whose correctness cannot be argued
   from its own body: the two ReaderWriter factory slots at +0x18/+0x20 and the
   serialize slot at +0xa0 are vtable displacements, and this project has a
   standing finding that those differ between retail and the profile build.

**Unported, every caller keeps the DLL's version, byte for byte unchanged**
(`code/Game/Game/gameupdateloop_serialize.cpp:161` is the only one). Landing it
later is a bounded job for whoever owns the Wave 4 Serializer machinery: the
body above is complete, and the four helpers it needs (`RemoveAll` 0x18009e9d0,
`Array<InternalPtr>::SetSize` 0x180008d60, `FUN_180009a20`, and a Serializer
constructed the Wave 4 way) all exist.

---

## 6. The three DATA exports

`?_singleton@AchievementManager@Achievement@@0PEAV12@EA` (0x1801fc670) and the
two `?_typeInfo@…` symbols are **variables, not functions**; `RegisterRuntimeExport`
substitutes a function body and there is nothing to substitute. Same finding as
Wave 4 lane D's `FileSystem::_singleton` and Wave 3's exported type-info
symbols. They stay the DLL's until cutover, and this lane READS all three by
name (`ManagerSingletonCell`, `AchievementTypeId`) rather than by address, so
nothing here needs the address table.

Note the mangling digit: `_singleton` here is `0PEAV12@EA` (private static),
the same shape as `Factory::_singleton`, not the `2PEAV12@EA` that
`Modification::_singleton` uses. `standalone/reg_unreg_lane1.cpp:207` and
`code/Game/Game/gameupdateloop_serialize.cpp:189` already spell it correctly.

---

## 7. Comments to correct (PLAN.md §6 step 8) — all OUTSIDE this lane's files

Nothing here was fixed by this lane; each is for the owner or the orchestrator.

### 7.1 CONFIRMED, not corrected — the two recorded claims the brief flagged

Both re-derived from the shipped bytes and both are **right**:

* `reference_achievementmanager_has_no_list_until_setachievements.md` — exact.
  `GetAchievementCount` (0x1800079b0) is literally
  `return *(int*)(*(void**)this + 0x28)`, i.e. `this->_achievements->_count`.
  The `this` is fine; `_achievements` is the null one, and 0x28 is the
  ExternalList's count field. The port reproduces the unguarded deref.
* `code/Game/Game/gameupdateloop_load.cpp` step 30b's long comment on
  `Reset` — correct in every particular, including that `SetAchievements`
  does NOT size `_instances`.

### 7.2 `runtime/types/io/FileSystem.h` — a named field is missing (lane D's file)

Line 73's `_unknown2C` comment says the +0x30/+0x38 pair is "a
package-filename array+count (**NOT read by anything in this file**)". True of
that file, but the +0x38 dword IS read — by `AchievementManager::Update`, every
tick, as the gate that switches achievements off entirely. It now has a name
from the profile build: it is the count behind
`?AreUserPackFilesEnabled@FileSystem@IO@@QEBA_NXZ` (retail 0x1800acfc0), and
retail inlines the call as `CMP dword [RAX+0x38],0 / JG`.

Suggested: name +0x30/+0x38 `userPackFiles` / `userPackFileCount` with that
export cited. This lane reads it through
`RtAchievement::kFileSystemUserPackFileCountOffset` in the meantime and says
why at the constant.

### 7.3 `engine/funcs/engine_funcs.cpp:4711` — a phantom export name (live)

```cpp
fn_GoalInstance_Persist = ResolveExport("?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z");
```

That name is **not in the DLL** and never resolves; `tools/export_name_audit.py`
lists it. The real export is `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z`
(`U`, not `Q` — it is virtual), at 0x18009ef70. Currently harmless only because
the next two lines fall back to `ResolveAddr(Module::Runtime, 0x18009ef70)`,
which is the correct address — so the pointer is right and the name is dead.
Exactly the shape `reference_resolveexport_phantom_names.md` warns about, with
the fallback doing the hiding. Lane A's territory (`GoalInstance`), reported
here because this lane's audit pass found it.

### 7.4 `tools/runtime_census.py` — a self-inflicted phantom row

The census's "ports with no caller" list contains a row literally named
`<literal>`. It comes from the census matching its own explanatory text in five
source comments that quote the pattern `RegisterRuntimeExport("<literal>"` —
`runtime/src/io/filesystem.cpp:350`, `io/serializer.cpp:609`,
`object/type_system.cpp:488`, `save/saveload.cpp:821` (and this lane's file,
which was reworded to stop contributing). Cosmetic, but it is one fake row in
the tool this project treats as authoritative over any agent's claims, so it is
worth either rewording those four comments or teaching the census to skip
comment lines.

### 7.5 A note for whoever ports `Collection::Array` itself

`Array<bool>::SetSize` (0x180008b90) does **not** initialise elements past the
old size; its two 8-byte siblings (0x180008ff0, 0x180008d60) do. That asymmetry
is in the shipped bytes, not a decompiler artefact.
`AchievementManager::SetAchievements` happens to depend on nothing about it,
because `ReadAchievements` immediately writes every slot — but a future port of
`Array<T>` that "fixes" it would be changing behaviour, not fixing a bug.

---

## 8. What this lane did NOT verify

* **Nothing has executed.** No `sa-run`, no `sa-stress`, no
  `BANISHED_RT_DIFF=1`. The 18 diff sites are written but have never been
  registered at runtime, so every `calls=` is unknown and the divergence count
  is not zero — it is *unmeasured*.
* The nine shadow verdicts are invariants, not equivalences. §4.3 and §4.5 in
  particular prove very little per call.
* `Persist` (§5) was reversed but not ported and not exercised.
* The profile-build addresses for 14 of the 18 functions were not looked up;
  the ports are transcribed from RETAIL bytes in every case, and the profile
  build was consulted for names and for the two functions where its
  un-inlined form made the retail shape readable (`Update`,
  `GetAchievementInstance`).

## 9. What the orchestrator should run

`BANISHED_RT_DIFF=1` on a run that reaches gameplay and awards at least one
achievement — the achievement-heavy harness (`BANISHED_ACH_TIME_SCALE`,
`BANISHED_ACH_TRACE`) is the right vehicle, since eight of the 18 sites are
only reachable once a game is running and `Reset`/`SetAchievements` fire
exactly once each per game. Specifically worth checking in the summary:

* every site has a non-zero `calls=` except `Persist` (absent) — a zero there
  means a phantom name or a dead call path, not a clean port;
* `Achievement::AchievementManager::Update` should show roughly one call per
  simulation tick;
* `SetAchievements` and `Reset` should show exactly 1 each on a new game, and
  `Reset` **0** on a load-from-save (that path goes through `Persist`, which is
  still the DLL's);
* a save/load round trip, because `Persist` staying on the DLL while
  `_instances` is now filled by OUR `Reset` is the one cross-implementation
  seam this lane creates.
