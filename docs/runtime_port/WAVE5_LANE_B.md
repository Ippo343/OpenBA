# Wave 5, lane B — Achievement / AchievementInstance / AchievementManager

Following [PLAN.md](PLAN.md) §4.2 and §6. Lane B owns the 19 code exports and 3
data exports the brief listed on these three classes, **plus 4 more the brief
could not see** — `Achievement`'s constructor, destructor, `GetFileVersion` and
`Serialize`, which `code/Game/Game/scenario.cpp:72-78` reaches by RAW ADDRESS
and which therefore never appeared in the `ResolveExport` grep the brief was
built from. Scope total: **23 code exports**, of which 21 are ported.

Files owned and written by this lane — **no edits outside this set**:

| File | What |
|---|---|
| `runtime/types/achievement/AchievementManager.h` | new — `Achievement`, `AchievementInstance`, `AchievementManager`, plus two minimal `…View` structs for lane A's `GoalList`/`GoalInstance` |
| `runtime/src/achievement/achievement_manager.cpp` | 18 ports + `RtAchManagerRegister()` |
| `docs/runtime_port/WAVE5_LANE_B.md` | this file |

§6 corrections this lane owes to files it does not own are in §7, for the
orchestrator to apply.

---

## 0. The headline finding — what `AchievementManager+0x30` is

Nobody in this project had characterised this behaviour, so it is stated here
before anything else.

**`AchievementManager+0x30` is a `Collection::Array<System::Time>` of
ACCUMULATED per-achievement time, and `Update` is a rotating cursor that
evaluates exactly ONE achievement per tick.** Every tick, every not-yet-completed
achievement's slot gets the frame delta added and every completed one's slot is
hard-zeroed. When the cursor (`_lastAchievementUpdate`, +0x40) reaches an
achievement, that achievement's instance is updated with its WHOLE accumulated
total, the slot is zeroed, and the cursor advances with wraparound, skipping
completed entries.

So **each achievement is evaluated once every `_instances._size` ticks, with
`_size` ticks' worth of time** — not once per tick, and not with the frame
delta. That is the entire reason the times array exists, and it is why the
integration run shows `AchievementManager::Update` and
`AchievementInstance::Update` at the same 901 calls while 36 achievements exist:
one instance is touched per tick, in rotation.

Retail inlines all of it, which is why it was not readable before; the profile
build (0x180028cd0) spells the call out as
`AchievementInstance::Update(inst, _updateTimes[_lastAchievementUpdate])`.

The gate above it matters too: **if any user pack file (mod) is mounted,
`Update` returns immediately and not one line of the above runs.** Retail
inlines this as `CMP dword [FileSystem::_singleton+0x38],0 / JG done`; the
profile build names it `IO::FileSystem::AreUserPackFilesEnabled` (0x1800acfc0).
That is the whole mechanism behind `BANISHED_ACH_IGNORE_MODS` and behind
`reference_mods_off_breaks_save_loading.md`'s "achievements need the mods OUT".

---

## 1. Result

| | |
|---|---|
| Ported | **21 of 23** code exports |
| Held back | `AchievementManager::Persist` and `Achievement::Serialize` — **both construct an `IO::Serializer`**, see §5 |
| Not portable before cutover | the 3 DATA exports (`?_singleton@AchievementManager@…`, `?_typeInfo@Achievement@1@…`, `?_typeInfo@AchievementInstance@…`) — see §6 |
| `RtAchManagerRegister()` returns | **21** |
| Registration form | **both forms for every one of the 21**: 21 `RegisterRuntimeExport("<one-line literal>"` and 21 `RegisterRuntimeAddr(0x…)`, verified by count |
| Census | `Achievement` **0 → 34 of 50 called**, after the orchestrator rebuilt the census's address handling (§7.6). 21 of the 34 are this lane's, 13 are lane A's `goal.cpp` |
| Diff sites added | **21** — 10 full oracles (one of them a full 0x58-byte oracle), 11 shadow verdicts |
| `tools/abi_retslot_audit.py` | **0 suspicious of 49**, unchanged |
| `tools/export_name_audit.py` | after lane E's two-adjacent-literals fix: 11 phantoms tree-wide, **none of them this lane's**. All 23 code exports were verified present in the shipped export directory with `pefile` **before any body was written** (§2) |
| Build | `mingw32-make standalone-debug` clean, no new warnings |
| **Measured in a run** | **20 of the 21 have executed with 0 divergences** across two orchestrator passes (§8). The one that has NOT is `~Achievement`. |

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

### The four the brief's grep could not see

`code/Game/Game/scenario.cpp:72-78` resolves these four by
`ResolveAddr(Module::Runtime, …)`, never by name, so a `ResolveExport` grep
misses them entirely and a name-only registration would never reach them:

```
OK  0x18009e240 ord=24    ??0Achievement@0@QEAA@XZ                  [PORTED]
OK  0x18009e180 ord=518   ??1Achievement@0@UEAA@XZ                  [PORTED]
OK  0x18009e170 ord=2608  ?GetFileVersion@Achievement@1@UEBAHXZ     [PORTED]
OK  0x18009e070 ord=3618  ?Serialize@Achievement@1@UEAA_NAEAVStream@IO@@@Z  [HELD BACK, §5]
```

That file uses addresses on purpose, and its own comment says why: the ordinals
it used to use (0x26d/0x26e/0x309) are **stale** in the shipped DLL and resolve
to `~LayoutElement`, `~Light` and `EdgeMeshInstance::operator new`, so
`Scenario_Init` called `~LayoutElement` on a fresh Scenario and corrupted the
heap. That history is the reason this lane registers **both** forms for all 21
ports rather than only the one currently in use.

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
| 19 | `Achievement::Achievement()` | 0x18009e240 | not looked up | 13 stores, no call: stamp Base then External then Achievement vtable, zero +0x08..+0x50 | **full BYTE oracle**, §4.7 |
| 20 | `Achievement::GetFileVersion` | 0x18009e170 | not looked up | `return 3` (virtual, slot 3) | **full** |
| 21 | `Achievement::~Achievement()` | 0x18009e180 | not looked up | re-stamp vtable, free the goal array, release 3 Strings, `External::~External` | shadow — refcount deltas, §4.8 |

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

### 4.7 `Achievement::Achievement()` gets the one real byte oracle

The constructor's writes are confined to `this` and cover all 0x58 bytes, and
it calls nothing. So the DLL's copy can be run into a **poisoned (0xCD) scratch
buffer** and compared byte for byte with `RT_DIFF_BYTES` — no side effect, and
a proof of every byte rather than of an invariant. The poison matters: without
it a byte the DLL writes and we do not would accidentally match a zero.

The two dead vtable stores (Base, then External, then Achievement, all to
`[this+0x00]`) are kept. They are the inlined base constructors, they are the
body, and a detour or a debugger break between them would make the intermediate
values observable. All three vtables are **exported data symbols**
(`??_7Base@Object@@6B@` 0x180154788, `??_7External@Object@@6B@` 0x1801547a0,
`??_7Achievement@0@6B@` 0x180154740), so the port resolves them by name and
stamps the DLL's own vtables instead of cloning slots (PLAN.md §2.2 D).

### 4.8 `~Achievement` — the shadow that checks the dangerous part

Running the DLL's destructor as an oracle is a double free, so instead each of
the three `String` fields is snapshotted as `{data, refcount}` **before**, and
the arithmetic checked after:

* refcount was > 1 → it must now be exactly one less, and the field null;
* refcount was == 1 → the block is gone and unreadable, so only the field is
  checked: it must hold `data - 8`.

That asymmetric leftover (`data - 8` on the freeing branch, null on the sharing
branch) is retail's, and is **not** tidied. Reproducing it exactly is the
point: the one-line bug behind
`reference_load_wild_write_high_dword_7fc.md` was this same idiom applied to
something that was not a String, and the way not to repeat it is to run the
already-ported, already-diffed `RtString_ReleaseInline` on fields whose offsets
came out of the bytes. The goal-array free (`FUN_180004890`) and
`Object::External::~External` remain the DLL's code, called; nothing here
claims to prove their halves.

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

## 5. Held back: the two functions that construct an `IO::Serializer`

**`AchievementManager::Persist` (0x18009e910)** and
**`Achievement::Serialize` (0x18009e070)**. Both fully reversed, both
deliberately not ported, for one shared reason. `Persist`'s body:

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

**`Achievement::Serialize` (0x18009e070) is the same shape and is held for the
same reason.** It builds a Serializer from the stream in the identical inlined
way, then streams `_stringTable` (+0x20), `_title` (+0x28), `_description`
(+0x30), `_spriteSheet` (+0x38), `_spriteName` (+0x40), an int count at +0x50
and the goal array at +0x48, and destroys the codec. Same 12-byte header, same
absence of any oracle, same silent-desync failure mode — and this one is on the
**resource load** path, where a wrong byte count corrupts every subsequent
resource in the pack file rather than one save.

**Unported, every caller keeps the DLL's version, byte for byte unchanged** —
`code/Game/Game/gameupdateloop_serialize.cpp:161` for `Persist`,
`code/Game/Game/scenario.cpp:78` for `Serialize`. Landing either later is a
bounded job for whoever owns the Wave 4 Serializer machinery: both bodies are
recorded here, and the helpers they need (`RemoveAll` 0x18009e9d0,
`Array<InternalPtr>::SetSize` 0x180008d60, `FUN_180009a20`, `FUN_180005110`,
`FUN_180005220`, and a Serializer constructed the Wave 4 way) all exist.

### Does holding these two split the class?

**No, and the reason is specific rather than reassuring.** The lead's worry —
a half-ported class mutating shared state — needs the two implementations to be
able to disagree about state that outlives a call. Here they cannot:

* `Achievement`'s constructor writes **only to `this`** (13 stores, no call, no
  allocation, no registration — read out of the bytes at 0x18009e240).
* Its destructor writes only to `this` plus the one shared heap, which both
  sides already reach through the same `MemoryAllocator::_singleton`; the goal
  array free and `External::~External` are still the DLL's own code, called.
* `GetFileVersion` returns a constant.
* `Serialize`'s state is the stream, and it stays entirely the DLL's.

So an `Achievement` constructed by the DLL and destroyed by our port — or the
reverse — sees identical bytes. That reverse case is not hypothetical: the DLL
builds its own `Achievement`s through `?CreateNewObject@Achievement@1@` /
`?CreateInPlaceObject@…`, which call the constructor **internally, DLL to DLL,
through no export**, and no detour is installed, so those keep using the DLL's
copy no matter what we register (PLAN.md §4.2's standing note that the detour
mechanism has still never been exercised on a port). Only `scenario.cpp`'s
Scenario, an exe-side subclass, comes to ours.

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

### 7.3 `engine/funcs/engine_funcs.cpp:4711` — phantom export name — **FIXED**

**Status: already corrected in the tree** (the line now reads `UEAAX`, and the
name no longer appears in `export_name_audit.py`'s output). Recorded because
this lane found it and because the shape is worth remembering.

```cpp
fn_GoalInstance_Persist = ResolveExport("?Persist@GoalInstance@Achievement@@QEAAXAEAVStream@IO@@@Z");
```

That name is **not in the DLL** and never resolves; `tools/export_name_audit.py`
lists it. The real export is `?Persist@GoalInstance@Achievement@@UEAAXAEAVStream@IO@@@Z`
(`U`, not `Q` — it is virtual), at 0x18009ef70. Currently harmless only because
the next two lines fall back to `ResolveAddr(Module::Runtime, 0x18009ef70)`,
which is the correct address — so the pointer was right and only the name was
dead. Exactly the shape `reference_resolveexport_phantom_names.md` warns about,
with the fallback doing the hiding.

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

### 7.6 `tools/runtime_census.py` address handling — **REPORTED AND FIXED**

**Reported by this lane and lane D independently; fixed by the orchestrator.**
The census used to hardcode the namespace `"internal"` for every row in
`called_addrs`, so a function our tree reaches only by `ResolveAddr` never
counted toward its own namespace, and a function reached by BOTH doors was
counted twice. This lane's three `Achievement` lifecycle ports were the case in
point: the CSV read `addr,0x18009e240,internal,1`, so they were counted, but in
the `internal` row.

The census now resolves every registered/called address back through the
shipped DLL's export directory: an address that names an export lands in that
export's namespace, a function reached by both doors is counted once, and a
port is an orphan only if NEITHER door has a caller. The `internal` bucket fell
262 → 35 and TOTAL called fell 898 → 727, that difference being pure double
counting.

**One position of this lane's was overruled, correctly.** This doc previously
argued the tool should be left alone and that a permanently-listed
insurance registration was "the honest cost". The orchestrator's counter is
better and is recorded here rather than quietly dropped: a list that
permanently contains entries everyone knows to ignore stops being read, and
this is precisely the list that exists to catch
`reference_uncalled_port_is_not_verified.md`. It has to stay clean to stay
useful. The orphan list fell from 11 exports + 72 addresses to 7 exports + 0
addresses, and the 7 remaining are genuine pre-existing orphans from Waves
1/2/4.

### 7.5 A note for whoever ports `Collection::Array` itself

`Array<bool>::SetSize` (0x180008b90) does **not** initialise elements past the
old size; its two 8-byte siblings (0x180008ff0, 0x180008d60) do. That asymmetry
is in the shipped bytes, not a decompiler artefact.
`AchievementManager::SetAchievements` happens to depend on nothing about it,
because `ReadAchievements` immediately writes every slot — but a future port of
`Array<T>` that "fixes" it would be changing behaviour, not fixing a bug.

---

## 8. Measured — and what is still unmeasured

The orchestrator's integration pass (autostart + pause-menu quit, world
teardown, `BANISHED_RT_DIFF=1`) covered the **first 18** ports: 91 sites tree
wide, 24,050,239 calls, 99 divergences all confined to the two pre-existing
Wave 1 sin/cos sites, no new divergent site, clean exit, empty crash log. This
lane's live counts, all `divergences=0`:

| Site | calls |
|---|---|
| `AchievementManager::Update` | 901 |
| `AchievementInstance::Update` | 901 |
| `AchievementInstance::IsMet` | 901 |
| `SetEventHandler` | 2 |
| `SetAchievements` | 1 |
| `GetAchievementCount` | 1 |
| `Reset` | 1 |

`Update`/`IsMet` at 901 each is one per simulation tick, exactly as §4.5
predicts. `SetAchievements` and `Reset` at 1 each confirm the once-per-game
ordering, which means the
`reference_achievementmanager_has_no_list_until_setachievements.md` sequence
held under a real run with our port in the path. `GetAchievementCount`'s
`calls=1` also proves the point of §7.1 in the lead's correction: that function
is resolved NAME-FIRST at `engine_funcs.cpp:2749`, so the name registration
already reaches it.

### The second pass — the scenario ports fired

`BANISHED_AUTOTUTORIAL=1 BANISHED_RT_DIFF=1 BANISHED_RT_INTERNAL_SHADOW=1`
with a pause-menu quit: 133 sites, 5,953,991 calls. This lane's sites, all
`divergences=0`:

| Site | calls |
|---|---|
| `Achievement::Achievement::ctor` | 4 |
| `Achievement::Achievement::GetFileVersion` | 4 |
| `Achievement::Achievement::GetTitle` | 4 |
| `Achievement::Achievement::GetDescription` | 4 |
| `Achievement::Achievement::GetSpriteName` | 4 |
| `AchievementInstance::ctor` | 1 |
| `AchievementInstance::SetAchievement` | 1 |
| `AchievementInstance::IsMet` | 2402 |
| `AchievementInstance::Update` | 2402 |
| `AchievementInstance::HasFailed` | 1201 |
| `AchievementManager::Update` | 1201 |

**The 0x58-byte constructor oracle agreed on every byte, four times, against a
0xCD-poisoned scratch buffer** (§4.7). That was the highest-information site in
the lane and the one with the most to prove; it is now green.

### Still unmeasured — three things, and none of them is carried by the results above

1. **`~Achievement`'s shadow verdict has never run.** The destructor did not
   appear in either pass. Its refcount-delta check (§4.8) is written and
   unexercised, and the constructor's clean byte oracle says nothing about it —
   they are different bodies with different failure modes, and the destructor's
   is the one that touches the heap. Treat `~Achievement` as registered,
   reachable and UNVERIFIED.
2. **The award branch of `AchievementManager::Update`.** All 1,201 + 901 calls
   took the not-met path, so `SetAchievementMet`, the event-handler call and
   the shadow's `IsAchievementAwarded` readback are all unexercised. See §9.
3. **`IsAchievementAwarded` itself** has never appeared in any run's site list.
* The nine shadow verdicts are invariants, not equivalences. §4.3 and §4.5 in
  particular prove very little per call.
* `Persist` (§5) was reversed but not ported and not exercised.
* The profile-build addresses for 14 of the 18 functions were not looked up;
  the ports are transcribed from RETAIL bytes in every case, and the profile
  build was consulted for names and for the two functions where its
  un-inlined form made the retail shape readable (`Update`,
  `GetAchievementInstance`).

## 9. What is still worth running, cheapest first

**1. `IsAchievementAwarded` — free, no game required.** Its only real caller is
the Achievements dialog (`code/Game/Dialog/achievementdialog.cpp:229`, one call
per achievement per refresh). `BANISHED_AUTOMENU_BUTTON=8` opens that dialog
from the FRONT END, so a menu-only run drives `IsAchievementAwarded`,
`GetAchievementCount`, `GetAchievement` and the three String getters with no
world at all. `BANISHED_ACH_TRACE=<n>` reaches it in any run via
`standalone/achievement_probe.cpp:370`.

**2. The award branch — check the REGISTRY before spending a long run.** The
branch is `if (_completed[idx] == 0 && SetAchievementMet(…))`, and `_completed`
is loaded by `ReadAchievements` from `HKCU\Software\Shining Rock Software LLC\
Banished`, section `"Ach"`, key `"<index>"`. **An achievement already flagged
there can never take the branch again, however long the run.** This machine has
historically awarded 25 of 36, so on an unmodified registry only 11 indices are
even eligible — which is a likelier explanation for the empty award path than
run length. Clearing the `Ach*` values makes every already-satisfied
achievement re-award on its next rotation.

**3. THE AWARD RUN — use the STRESS TOWN, not autostart.** A recipe that has
already awarded an achievement natively in this project is recorded in
`project_jack_of_all_trades_closed.md`, with controls:

```
tools/ach_reset.sh
BANISHED_BUILDERS=40 BANISHED_ACH_TRACE=600 BANISHED_ACH_TIME_SCALE=50 BANISHED_PASTURE_AUTOTYPE=Cow,Sheep,Chicken tools/stress_run.sh --label ach --speed 50 --frames 1500
```

Jack of all Trades (Ach5) was met between f=600 and f=1200 on that, with 20 of
its 21 goals passing on REAL measured values and only the 5-year TimeGoal
carried by the declared clock deviation.

**The load-bearing difference from the autostart attempt is the TOWN, not the
clock.** Every achievement in the set is a conjunction, and the half that a
time knob cannot touch is a population or profession threshold. An autostart
town has 23 people, so `population >= 300` can never be met however fast the
clock runs; the stress town starts with 800 standing at frame 0. Accelerating
time cannot award a conjunction whose other half needs a real town — the clock
knob only ever carries the TimeGoal half.

Three operational notes for that run:
* `tools/ach_reset.sh` FIRST. The manager skips already-awarded achievements,
  so an inherited award is indistinguishable from a pass.
* The diff oracle SURVIVES the perf build that `stress_run.sh` uses:
  `standalone/log_strip.h` strips unqualified `printf` only, and all of
  `rt_diff.cpp` writes through `fprintf(stderr, …)`. Read the summary in
  `standalone_error.log`, not `standalone_debug.log`. (This lane's own
  `[RtPort]` registration line IS stripped there — cosmetic; the registrations
  still happen.)
* Keep `--frames` at ~1500. The stress town starves at ~100k ticks at any
  speed, and the award lands around f=600-1200, so a longer run adds risk
  without adding coverage.

**3b. The no-TimeGoal alternative, same town.**
`standalone/ach_clock.cpp`'s own header records that Established and Tenure have
NO TimeGoal — they are a population StatGoal plus `stat26 >= 100 / 200`, and
`BANISHED_ACH_YEAR_BOOST` moves that counter directly. So the stress town
(`make sa-stress`, 800 citizens standing at frame 0) plus
`BANISHED_ACH_YEAR_BOOST=250 BANISHED_ACH_IGNORE_MODS=1 BANISHED_RT_DIFF=1`
should satisfy both goals within a frame or two, and the rotating cursor
(§0) reaches that index within at most `_instances._size` ticks. A few thousand
frames, not a vigil. `BANISHED_ACH_TIME_SCALE=50` is the fallback for the
3-year pair (Miner, Mason), which additionally needs a staffed quarry and mine.

What that would and would not prove: `SetAchievementMet` is the DLL's, not this
lane's, so the gain is the award branch of OUR `Update`, its event-handler call
and its shadow readback through `IsAchievementAwarded` — worth having, but it
is one branch, not a new class of coverage. If it is not cheap, recording those
branches as unexercised in WAVE5.md is the honest outcome and better than
implying the 2,102 not-met calls covered them.

**4. `~Achievement`.** Reached when a Scenario is destroyed — a quit-to-menu or
a second scenario load after the first, rather than the pause-menu quit both
passes used. Worth one run, because it is the only heap-touching body in the
lane.

**5. A save/load round trip.** `Persist` staying on the DLL while `_instances`
is now filled by OUR `Reset` is the one cross-implementation seam this lane
creates. Expect `Reset` `calls=0` on a load-from-save — that is correct, not a
dead port.
