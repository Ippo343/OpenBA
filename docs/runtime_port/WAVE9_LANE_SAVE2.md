# Wave 9 lane SAVE2 — the `Save::SaveLoad` driver, the `IO::Persistence` write path, and six by-address stragglers

**Scope:** four unsatisfied NAME doors (`?UpdateLoad@`, `?UpdateLoadFromFile@`, `?UpdateSave@`
on `Save::SaveLoad`, and `?WriteSettings@Persistence@IO@@`) plus six unassigned by-address
doors from `tools/addr_door_audit.py --csv`.

**Result in one line: 3 functions PORTED and verified against the shipped DLL (5 registrations,
2 address-door rows and 1 name-door row removed, nothing added), 3 stragglers CLOSED with a
written reason, 3 name doors HELD BACK with a measured cost, and one retail finding that
changes what half of `WriteSettings` IS.**

Branch `wave9/lane-save2`, worktree branched from `ba7bc60` (the integration tip that merged
lane PKG3 and recorded attempt 17).

---

## 0. Verdict table — one line per assigned row

| Row | Verdict |
|---|---|
| `?WriteSettings@Persistence@IO@@QEBAXXZ` (0x1800b88e0) | **PORTED** — 147 emissions per side, 0 mismatches, negative control CAUGHT |
| `0x18000f7d0` `Set<Pair<String,String>>::Clear` | **PORTED** (+ its two helpers 0x18000f890 / 0x18000fa90) — 3 comparisons, 0 mismatches, negative control CAUGHT. **Boot-path row**: the ported `ReadSettings` calls it, unguarded, on every `Engine::Startup` |
| `0x18001e540` `??0Matrix@Math@@QEAA@AEBVVector4@1@000@Z` | **PORTED** — 24 diffed calls, 0 divergences. It was an ADDRESS door on an EXPORTED, unported symbol; the call site now asks by name |
| `?UpdateLoad@SaveLoad@Save@@` (0x1800c2690) | **BLOCKED** — no oracle, and its closure is 14 new rows (§5) |
| `?UpdateLoadFromFile@SaveLoad@Save@@` (0x1800c2560) | **BLOCKED** — same, decoded in full below |
| `?UpdateSave@SaveLoad@Save@@` (0x1800c2a80) | **BLOCKED** — same, plus a virtual through `[obj+0x38]` |
| `0x1800042a0` / `0x180005dc0` `ExternalBasePtr::Serialize` | **BLOCKED on `ExternalFactory::FindOrLoad`** — write half is oracle-covered today, read half is not, and it is one call deep into a function with no possible oracle. Full decode handed on (§6) |
| `0x180155860` (Persistence vtable) | **CLOSED — MINT ROW, not a port.** 2 slots, contents harvested (§7); drop-in entry for `EXTRA_VTABLES_BY_ADDR` supplied. The integrator owns the mint |
| `0x180157c60` (Audio Device per-record vtable) | **CLOSED — MINT ROW, not a port.** 7 slots, contents harvested (§7) |
| `0x1800dd3c0` `?Run@MainLoop@Engine@@` | **CLOSED — already-tracked holdback**, not a new row (§8). Deliberately NOT converted to a name door |

---

## 1. Measurements

One DLL-present run under `tools/game_dir_lock.sh acquire lane-SAVE2 900` … `release`, exe
`4fae090a92d56b69034f59cdced4e695` verified before and after, logs copied out of
`C:/GOG Games/Banished` before the lock was released. `WinData/*.pkm` was **0 throughout and
was not touched.**

Gated on an identity line read from **the run's own log**, not the on-disk md5
(WAVE9.md, "the advisory lock nobody was told about"):

```
standalone_debug.log:74  [RtPort]   Collection::Set Clear: 3 addresses registered
standalone_error.log:6   [RtMathTransform] Matrix column-ctor sweep: 24 cases
```

Command: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
BANISHED_RT_DIFF=1 BANISHED_RT_SETCLEAR_PROBE=1 BANISHED_RT_WRITESETTINGS_PROBE=1`.
`EXIT=0`, `standalone_crash.log` 0 bytes.

```
[RtDiff] ==== 484 sites, 17,602,114 calls, 1 divergences in 1 sites ====
[RtDiff]   Math::Matrix::Matrix(4 x const Vector4&)   calls=24   divergences=0
[RtDiff]   IO::Persistence::WriteSettings             calls=1    divergences=0
```

**The one divergence is not this lane's.** It is `Object::Factory::Startup [probe, not
diffable] ours=chain=416 nodes=415`, the unowned row lane SYS2 flagged as a handoff and
reproduced on an untouched base commit. It is unchanged here.

### `Set<Pair<String,String>>::Clear`

```
[SetClear] empty      OK    ours[blocks=0  bytes=0   root0=1 count=0]  theirs[blocks=0  bytes=0   root0=1 count=0]
[SetClear] balanced7  OK    ours[blocks=21 bytes=784 root0=1 count=0]  theirs[blocks=21 bytes=784 root0=1 count=0]
[SetClear] shared5    OK    ours[blocks=13 bytes=496 root0=1 count=0]  theirs[blocks=13 bytes=496 root0=1 count=0]
[SetClear] negative-control skip-right: CAUGHT (blocks=12 vs retail 21)
[SetClear] comparisons=3 mismatches=0
[SetClear] effectiveness (inside our bodies): recurse=13 null-child=18 strRelease-freed=30 strRelease-shared=2
```

### `IO::Persistence::WriteSettings`

```
[WriteSettings] negative-control name/data swap: CAUGHT
[WriteSettings] corpus: stale-name seed PLANTED and removed
[WriteSettings] emissions ours=147 theirs=147 mismatches=0 dropped(ours=0 theirs=0)
[WriteSettings] effectiveness (inside our body): RegSetValueExW=146 staleNamesFound=1 RegDeleteKeyW=1
```

### Instruments, before → after

| Instrument | before | after | set-diff |
|---|---|---|---|
| `tools/addr_door_audit.py --csv` | 42 rows | **40 rows** | `-0x18000f7d0`, `-0x18001e540`; **nothing added** |
| `tools/unsatisfied_export_scan.py` | 844 names / 130 unsatisfied | 845 / **129** | `-?WriteSettings@Persistence@IO@@QEBAXXZ`; **nothing added**. The +1 denominator is `?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`, a NEW name door opened by `WriteSettings`'s array — already ported, so it is satisfied and does not appear in the residue |
| `tools/port_bypass_audit.py` | — | **0 bypasses on both doors** (address door 302 sites, name door 470 sites) |
| `tools/export_name_audit.py` | — | 667 names, 1 not in the DLL — the pre-existing doc comment in `rt_resolve_cache.cpp:8`, not a registration |
| `tools/runtime_census.py` | 998 / 741 / 74.2% | **998 / 744 / 74.5%** | `called` **UNMOVED**, `ported` +3 |

`called` unmoved is the number worth reading: none of these three ports grew the cutover
surface (`reference_porting_can_grow_the_cutover_surface.md`). The two `Clear` helpers have no
caller in our tree, so they are registered and correctly absent from `called`.

---

## 2. PORTED: `?WriteSettings@Persistence@IO@@QEBAXXZ` (0x1800b88e0, ord 4561, 538 instructions)

`runtime/src/io/persistence_registry_win.cpp`, beside lane BOOT2's `ReadSettings`.

Wave 4 lane C held BOTH halves back on one sentence — *"No safe oracle exists. Both mutate the
real HKCU\…\Banished registry key."* Lane BOOT2 showed it was true of this half and false of
the other, ported the other, and recorded the write half as still blocked on "size plus the
standing registry hazard". Lane SYS2 then decoded it and designed a two-scratch-key oracle.

### THE FINDING: retail's "prune stale settings" pass has never pruned anything

**IAT slot `0x18014b008` is `RegDeleteKeyW`, not `RegDeleteValueW`** — read out of
Runtime-x64.dll's import directory with `pefile`, not inferred from what the loop is for:

```
0x18014b000 ADVAPI32.dll RegEnumValueW
0x18014b008 ADVAPI32.dll RegDeleteKeyW      <-- the delete pass
0x18014b010 ADVAPI32.dll RegQueryValueExW
0x18014b018 ADVAPI32.dll RegSetValueExW
```

Phase 2 enumerates registry **values**, collects the names that are absent from the settings
map, and then asks Windows to delete a **subkey** of each of those names. No such subkey
exists, so every call returns `ERROR_FILE_NOT_FOUND` and nothing is ever deleted. The whole
second half of the function is a no-op with a side effect only on the allocator.

That is transcribed exactly as it is, and the port carries a comment saying so. It must not be
"corrected": switching to `RegDeleteValueW` would start deleting real registry values the
shipped game leaves alone — a behaviour change dressed as a fix, on the player's own registry.
The probe's own cleanup *does* use `RegDeleteValueW`, and the source says why the two differ.

### Two more things a tidy rewrite gets wrong

* **The name is the KEY and the data is the VALUE.** Retail loads `RDX = wide(node+0x00)` as
  `lpValueName` and passes `[RSP+0x20] = wide(node+0x08)` as `lpData`, computing `cbData` by
  walking the DATA buffer (`2 + 2*len`, NUL included). Swap them and the settings still
  round-trip through `ReadSettings`, mirrored — a defect that survives every end-to-end test.
  This is the exact swap the negative control below injects.
* **The growth rule is ×3, not ×2.** Phase 2's inlined `Array<WideString>` grows
  `capacity = newSize`, and then `capacity = newSize * 3` when `newSize == oldSize + 1 &&
  oldSize != 0` (`LEA EDX,[RSI + RSI*2]` at 0x1800b8d2f). So the first append allocates exactly
  one slot and every later one triples.

### ORACLE — an emission-sequence comparison that writes NOTHING, anywhere

This replaces SYS2's scratch-key design and is both simpler and stronger: **intercept the two
mutating ADVAPI32 calls and compare what each side WOULD have written.**

* **ours** — the body routes `RegSetValueExW`/`RegDeleteKeyW` through `EmitSet`/`EmitDelete`,
  which record into the armed recorder and call the real API otherwise.
* **theirs** — the DLL reaches both through its own IAT. The probe swaps `0x18014b018` and
  `0x18014b008` for recording thunks for the duration of the shipped call and restores them.
  That is `standalone/main.cpp:706-760`'s MessageBoxW-in-the-IAT pattern on two slots.

Everything else runs for real on both sides — same key opened, same values enumerated, same map
searched — so **the population is the real settings map (146 entries), not a synthetic one**,
and neither side touches the player's registry. Replay is sound because `WriteSettings` is
`QEBA`: it walks the map and emits, and mutates nothing it reads (the lane-S write-path
finding, one function further on).

**The corpus control earned its keep on the first run.** That run reported
`staleNamesFound=0` — every enumerated value was in the map, so phase 2's array append and its
delete pass never ran and the comparison covered phase 1 only. The probe now **plants one value
under a name no setting collides with**, before the comparison, and removes it with
`RegDeleteValueW` afterwards. `staleNamesFound=1 RegDeleteKeyW=1` on the second run: the branch
is now exercised on both sides rather than reported as unreachable.

**The negative control has to run before the seed is removed**, and getting that wrong the
first time would have made it pass for the wrong reason: with the seed already gone the swapped
run emits 146 records against retail's 147, and the *count* difference alone reports CAUGHT
without the swap ever being seen. The seed is now removed only after all three runs.

**What the oracle cannot see:** the allocation trace. Both sides allocate WideString
temporaries and (in phase 2) a growing array; the recorder observes neither, so a port that
leaked every temporary would compare clean. Stated rather than left implied.

---

## 3. PORTED: `Set<Pair<String,String>>::Clear` (0x18000f7d0) + 0x18000f890 + 0x18000fa90

`runtime/src/collection/set_clear.cpp`, probe in `set_clear_probe.cpp`, registered from the
Collection namespace's one init hook behind `BANISHED_RT_SETCLEAR`.

**This is a boot-path row.** Lane BOOT2's ported `ReadSettings` opens with `Map_Clear(this)`
through an address door it deliberately left unguarded, with the comment *"with the DLL absent
it hands back 0 and the check would turn 'no port for Map_Clear' into SILENCE"*. So at cutover
it is an `EXEC-of-0` inside `Engine::Startup`. No lane owned it.

Three bodies, a post-order recursive destroy:

```
0x18000f7d0  Clear(this)          n=root; if(n){Sub(this,n->left); Sub(this,n->right);
                                  Elem(n); NodeFree(junk,n);} root=0; count=0
0x18000f890  Sub(ignored, n)      if(!n) return; Sub(l); Sub(r); Elem(n); NodeFree(junk,n)
0x18000fa90  Elem(n)              release the String at n+0x08, THEN the one at n+0x00
```

Two facts worth carrying:

* **`0x18000fa90` IS `array_destroy.cpp`'s Body B** with one element and the offsets
  `{0x08, 0x00}` in retail's own order — including the asymmetry that leaves the slot holding
  NULL when the string survives and `p - 8` when it does not. It is **called, not restated**,
  so the two cannot drift; `array_destroy_internal.h`'s "only two files include this" comment
  is corrected in place to three, with the reason.
* **Neither helper reads its first argument,** and retail proves it by passing junk: at
  0x18000f8d1 and 0x18000f812 `RCX` holds whatever the element destructor's epilogue left. Same
  shape as `FUN_180023060` (`reference_read_the_callee_before_holding_back_on_an_arg.md`).

### ORACLE — differential on the allocator's own counters, two synthetic trees

A destructor cannot be replayed and a scratch COPY aliases the same heap (SYS2's `~SaveLoad`
write-up). The way out is not to copy one subject but to **build two**: identical private trees,
destroyed side by side. Everything `Clear` does that outlives it is heap traffic, and the
allocator counts its own — `allocCount` (+0x638) and `allocatedBytes` (+0x630), both pinned by
Wave 2 with `static_assert`s. Snapshot around each side and the deltas say exactly how many
blocks and bytes that side handed back.

Corpus: empty (the `root == 0` arm, which still stores both zeroes), a 7-node balanced tree, and
a 5-node tree where **two nodes share one value String** — without that third shape every
release takes the same arm and the run is
`reference_uniform_population_is_not_a_verification.md` again. Measured:
`strRelease-freed=30 strRelease-shared=2`, so both arms ran.

**What it cannot see:** the counters are a SUM, so a pre-order destroy that also visits every
node once compares equal. Named rather than hidden; the order is four instructions of the
transcription and is checked by reading.

---

## 4. PORTED: `??0Matrix@Math@@QEAA@AEBVVector4@1@000@Z` (0x18001e540)

`runtime/src/math/transform.cpp`, with `Math::Matrix`'s other members.

`code/Game/Game/weather_particle_draw.cpp:130` reached it by ADDRESS under a comment saying
there was *"no verified mangled name to hand"*. **There is one**, and it is in the shipped
export table at exactly that RVA. So this was an address door onto an exported symbol — the
shape `tools/port_bypass_audit.py` exists to catch the moment anybody ports it. The call site
now asks by name; the address stays registered so a future by-address caller cannot walk past
the port.

39 MOVs, four 16-byte column copies into +0x00/+0x10/+0x20/+0x30, `this` in RAX at the RET.
The **fourth column arrives on the stack** (`MOV RAX,[RSP+0x28]`) because it is the fifth
argument counting `this` — the "a pointer param typed `int` is only wrong from the 5th arg on"
trap, one register over.

**It had ZERO live callers in a full autostart-to-pause-menu run**, so the port's diff site
never registered: `calls=0` over a green line, this project's canonical unverified port. A
24-quadruple sweep from the registration hook turns that into `calls=24 divergences=0`. The
corpus deliberately includes negative zero, denormals and both infinities: a pure 16-dword copy
cannot mangle them, and if one came back different the port would be doing arithmetic it must
not do. The sweep is safe from a `*PortInit` for a reason that does **not** generalise (lane
PKG2's `null+0x640`): this body allocates nothing and touches no singleton.

---

## 5. HELD BACK: the three `SaveLoad::Update*` arms — with the cost measured

`runtime/src/save/saveload.cpp:876/879/882`, reached from the ported `SaveLoad::Update`'s
five-way dispatch.

They are the arms of a per-frame state machine on `SaveLoad+0x18`, each of which performs the
real save/load/enumerate work exactly once and mutates both `this` and `IO::Persistence`'s
request queue. `UpdateLoadFromFile` (0x1800c2560, 68 instructions) is decoded in full here so
the next lane does not redo it:

```
phase 0:  Persistence::BeginGetSaveGameInfo(_singleton, &this->_name /*+0x70*/, 2); phase = 1
phase 1:  b = alloc(0x10); b[0]=1; b[1]=0; this->+0x40 = b
          SavedState::SetupLoad(b, &block)
          if (!(persistence->+0x18 & 1)) {
              Persistence::GetSaveFilename(persistence, &b, &this->_name)
              FUN_1800b7460(persistence+0x10, &b, 1, &block)
          }
          release b;  phase = 2
phase 2:  this->+0x00 = 0
          this->+0x08 = (int64)(0.0 / Timer::_secondsPerTick)
          this->+0x18 = 0;  this->+0x48 = -1
```

**A hazard for whoever ports phase 2**, because it is this project's own recorded failure mode:
`0.0 / _secondsPerTick` is 0 for any non-zero divisor — but `0.0 / 0.0` is NaN and `CVTTSD2SI`
turns that into `INT64_MIN`, not 0. `reference_port_must_reproduce_side_effects.md` is exactly
that: Timer's port kept its own statics and `MainLoop::Run` divided by the DLL's zero. A port of
this arm inherits the same dependency on `?_secondsPerTick@Timer@System@@` being **published**,
not merely computed.

**Why they are not ported, stated as a count rather than as a feeling.** The three bodies
between them call 18 further symbols. Four are already ported (`String::operator=`,
`operator+(String,String)`, `Filename::AdjustDelimiters`, `FileSystem::GetDataDirectory`).
That leaves:

| unported EXPORTS (6) | unexported internals (8) |
|---|---|
| `?BeginGetSaveGameInfo@Persistence@IO@@` 0x1800b7870 | 0x180011640, 0x180011460 |
| `?SetupLoad@SavedState@Save@@` 0x1800c0c60 | 0x1800b7460, 0x1800b75c0 |
| `?GetSaveFilename@Persistence@IO@@` 0x1800b7c80 | 0x1800101e0, 0x180026ef0 |
| `?SaveMetaData@SaveLoad@Save@@` 0x1800c1490 | 0x180032320, 0x18007eefc |
| `?GetAutoSaveName@SaveLoad@Save@@` 0x1800c1440 | |
| `?GenerateSaveName@SaveLoad@Save@@` 0x1800c1250 | |

**Porting the three drivers replaces 3 named residues with 14 new rows**, 8 of which have no
export name at all — the exact trade lane PICK's transitive-closure rule says to count before
writing a body. And `UpdateSave` additionally dispatches through `[obj+0x38]`, a
game-supplied callback, so its behaviour is not even contained in the DLL.

**No oracle exists for any of the three**: they consume a request queue and write a save file,
so a replay does the work twice on the player's own save, and a shadow has nothing to compare
because everything they produce they produce by calling something else. Same shape as
`MainLoop::Run` and `ExternalFactory::FindOrLoad`.

**Recommended disposition:** they want a `Save`/`Persistence` lane that starts at
`BeginGetSaveGameInfo` and `SetupLoad` and works upward, not a slot at the end of another lane.
The dispatch that reaches them is already on the port-aware door (`RT_EXPORT_ONCE`), so all
three follow a port the day one lands.

---

## 6. HELD BACK: `ExternalBasePtr::Serialize` (0x1800042a0 and 0x180005dc0) — decode handed on

Two instantiations of ONE template body, reached by address from
`runtime/src/achievement/goal_serialize.cpp:130/136`. Confirmed here by disassembly that they
are line-for-line identical apart from **one constant**: the type-id global they hand to
`IsDerivedFrom` is `0x1801fae08` at 0x1800042a0 and `0x1801f9908` at 0x180005dc0. So one ported
body parameterised on that ADDRESS serves both, and the two registrations differ only in it.

The body has three arms, selected by two unrelated tests:

```
if (stream->encoding->vtable[+0x10]() == 3)            // the VISIT pass
    ext = FindOrLoad(_singleton, GetPath(*self));  IsDerivedFrom(ext, typeId);  // result DISCARDED
else if (ExternalFactory::_singleton->+0x40 != 0)      // reference-table mode
    id = *self ? GetReferenceId(_singleton, *self) : -1
    ... rw->vtable[+0x98](rw, &id, 0)                  // emit / consume the uint id
    ... on read: GetReferenceObject(_singleton, id) and adopt it
else                                                   // the plain path
    b = (*self != 0);  rw->vtable[+0x78](rw, &b, 0)    // a bool
    if (b) { on write: String::Serialize(path); on read: read the path }
    if (stream->direction == 0)                        // READING
        ext = FindOrLoad(_singleton, &path)
        *self = (ext && IsDerivedFrom(ext, typeId)) ? ext : 0
```

**Why it is not ported.** The WRITE half is verifiable today and for free — lane S's sink pins
`direction = 1` and never returns 3, so the existing `Achievement::Goal::Serialize` and
`GoalInstance::Persist` `RtSaveTrace::Compare` sites already drive it end to end and would
verify a port of it with no new harness. The READ half is not: its output is `*self`, produced
by `?FindOrLoad@ExternalFactory@Object@@`, which WAVE9.md rows 27-28 record as having **no
possible oracle** (it rewrites the one shared resource AVL tree) and which lane SYS2 found
carries a second, live hazard — `standalone/filename_runtime_hook.cpp:566` byte-patches a
serialization detour onto the DLL's own `FindOrLoad`, and a registered port at that address
makes the detour installer refuse to patch and silently un-serialize the loader.

So a port here would be half-verified over a dependency that is itself unverifiable, and on the
read path it would convert an `EXEC-of-0` at 0x1800042a0 into an `EXEC-of-0` one frame deeper,
in the same load. **The right sequencing is `FindOrLoad` first** (with `g_resCS` moved into the
ported body, per SYS2's §4.3), then these two, then the write half verifies itself through the
oracle that already exists.

If a later lane wants the read half diffed rather than transcription-checked, the shape is
available and cheap: a stub `Stream` with `direction = 0` and a scripted `ReaderWriter` whose
String slot returns a fixed path. `FindOrLoad` on the same path twice returns the same cached
pointer, so the replay is value-identical — the same idempotence argument WAVE8_5 used to
accept the full compare for `SerializeReferenceTable`.

---

## 7. CLOSED — MINT ROWS: 0x180155860 and 0x180157c60, with their contents

Both are **unexported vtables**, not functions. `runtime/src/engine/singleton_ctors.cpp:444`
and `:732` stamp them into the `IO::Persistence` and `Audio::Device` singletons, and both call
sites already carry a correct comment saying they will be null at cutover and that the row
belongs to whoever owns the mint. **That is still the right owner** — `tools/rt_mint_gen.py`
generates `standalone/rt_data_mint_tables.inc` wholesale, six lanes are running, and lanes
BOOT2 and SYS2 both recorded that the integrator owns it.

What this lane adds is the harvest, so the edit is drop-in. The mechanism already exists:
`EXTRA_VTABLES_BY_ADDR` in `tools/rt_mint_gen.py` ("a vtable needs CONTENTS and an ADDRESS, not
a name").

**`0x180155860` — `IO::Persistence`'s vtable, 2 slots.**

```
slot 0  0x1800b7260   unexported — Persistence's override of Thread's pure virtual
slot 1  0x18000c590   ?OnCompleted@Thread@System@@UEAAXXZ   (the base, NOT overridden)
```

Count proved two ways, not one. (a) Slot 2 onward is ASCII text (`"true"`, `"Shining Rock
Software LLC"`, `"Software\%s\%s"`) — .rdata literals, not pointers, which is the generator's
own "not a valid address inside the DLL image" test. (b) It matches `??_7Thread@System@@6B@`,
which is 2 slots (`_purecall`, `OnCompleted`) — Persistence overrides slot 0 and inherits slot 1,
exactly as a 2-slot subclass vtable must.

**`0x180157c60` — the `Audio::Device` per-record vtable, 7 slots.** Stamped at `+0x00` of each
of 64 `0x20`-byte records at `Device+0xa98`.

```
slot 0  0x180050200     slot 3  0x180050210     slot 6  0x180050230
slot 1  0x1800501f0     slot 4  0x1800501d0
slot 2  0x1800501c0     slot 5  0x180050220
```

All seven unexported, all inside 0x1800501c0..0x180050240. Count proved by the same test: slot 7
onward is a table of 64-bit constants and then the ASCII `"SoundEffect"`, and the next exported
symbol is `??_7SoundEffect@Audio@@6B@` at 0x180157ce0, well past the end.

---

## 8. CLOSED: `0x1800dd3c0` `?Run@MainLoop@Engine@@` — an already-tracked holdback

`code/Game/Game/game_main.cpp:176`. It IS exported
(`?Run@MainLoop@Engine@@QEAAXPEAVUpdateLoop@2@@Z`) and it IS a fatal null at cutover — but it is
not a new row. `runtime/src/engine/engine.cpp` holds it back explicitly, with three compounding
reasons (no oracle of any kind: it is called once per process and does not return until the game
quits; two of its four callees are unexported DLL internals) and it is WAVE9.md triage row 9.

**Deliberately NOT converted to a name door.** That would move it from this lane's audit to
`unsatisfied_export_scan.py`'s list and change nothing else — with no port and no mint it is the
same null call either way (`reference_converting_a_door_is_not_satisfying_it.md`). It would also
make this one call site inconsistent with its two siblings at 0x1800dd6b0 / 0x1800dd640, which
`game_main.cpp` reaches by address and which `engine.cpp` registers by address for exactly that
reason. Left as it is, and reported as a row that is already owned.

---

## 9. Comments corrected at their source (PLAN §6 step 8)

| Where | Was | Now |
|---|---|---|
| `runtime/src/io/persistence.cpp:328` | `[RtPort] IO::Persistence: %d/7 exports registered (ReadSettings/WriteSettings held back -- see WAVE4_LANE_C.md)` | names a holdback that no longer exists AND attributed it to a denominator that never counted either function; now says which file registers them |
| `runtime/src/io/persistence_registry_win.cpp:229` | `0x18000f7d0 Map_Clear — NOT registered by any lane. Reaches the DLL.` | PORTED, registered by address; the call site is unchanged, what changed is which body answers |
| same file, registration block | `NOT registered, and not ported: ?WriteSettings@…` | ported and registered; and the sentence's second half ("the one function in this pair that calls RegSetValueExW and RegDeleteKeyW") is exactly WHY it turned out to be portable |
| `runtime/src/collection/array_destroy_internal.h:4` | "Only two files include this" | three, and the third is the reuse the header exists to enable |
| `code/Game/Game/weather_particle_draw.cpp:128` | "no verified mangled name to hand" (of the by-address group holding 0x18001e540) | there is one; the ctor moved into the name group |

---

## 10. Files touched

```
runtime/src/collection/set_clear.cpp            NEW  Clear + 2 helpers, registration, counters
runtime/src/collection/set_clear_probe.cpp      NEW  the allocator-counter differential probe
runtime/src/collection/array.cpp                     init hook + probe arming
runtime/src/collection/array_destroy_internal.h      "two files" -> three, with the reason
runtime/src/io/persistence_registry_win.cpp          WriteSettings, its probe, registration, 2 comment fixes
runtime/src/io/persistence.cpp                       the stale [RtPort] banner
runtime/src/math/transform.cpp                       Matrix column ctor + its sweep + registration
code/Game/Game/weather_particle_draw.cpp             address door -> name door
docs/runtime_port/WAVE9_LANE_SAVE2.md           NEW  this file
```

New env switches, both opt-in and both off by default:
`BANISHED_RT_SETCLEAR_PROBE`, `BANISHED_RT_WRITESETTINGS_PROBE`.
New kill switch, default-on like its neighbours: `BANISHED_RT_SETCLEAR`.

---

## 11. Handed on, in priority order

1. **`Map_Clear` is on the boot path and is now ours.** If the cutover boot moves past
   `ReadSettings` and dies deeper, this is the rung that moved.
2. **`ExternalFactory::FindOrLoad` gates more than its own row** — §6 here, and SYS2 §4.3's
   `g_resCS` warning. Two by-address doors in `goal_serialize.cpp` and the whole
   achievement load path sit behind it.
3. **The two mint rows in §7 are harvested and drop-in.** They are the only thing standing
   between the ported `Persistence` and `Audio::Device` constructors and a null vptr at cutover.
4. **`Object::Factory::Startup` still reports `chain=416 nodes=415`** on every boot. Unowned,
   reproduced again here on a run whose only other divergence count is zero. SYS2 flagged it;
   it is still flagged.
5. **The three `Update*` arms want a Save/Persistence lane**, not a slot — §5 names the 14 rows
   and the two entry points to start from.
