# Wave 9 lane SYS2 — the System / Object / IO / loop tail of attempt 14

**Scope:** the nine symbols `BANISHED_RT_STRICT` recorded unresolved on attempt 14 that
belong to System, Object, IO, Save, Input, Audio and the loop tail
(`docs/runtime_port/wave9_attempt14_unresolved.txt`).

**Result in one line: 1 export PORTED and its row CLOSED, 1 latent frozen-null resolve
FIXED, 1 shared instrument found blind and FIXED, 7 not ported — and for four of those
seven the stated blocker in WAVE9.md's triage table is WRONG about its own shape, which is
the more useful half of this report.**

---

## 0. Baseline, method and the contention guard

Worktree branched from `29cfaa2` (the census macro-door fix), which is `309c015` (attempt
14) plus one commit. Every number below was re-taken on that tip; nothing from an earlier
census is quoted.

All measurements were taken with the deploy/measure discipline the integrator asked for:
`md5sum` on the deployed exe immediately before and immediately after each run, and the
logs copied out of `C:/GOG Games/Banished` before analysis. **The guard fired.** A control
deploy was refused with `cp: cannot create regular file: Device or resource busy` and the
exe in the game directory was `a812e700abc11500a60ec568007469ed` — a third binary,
neither of mine. The control run was retried after waiting for the directory to free, and
both runs below have a verified before/after md5.

(Note for anyone repeating this: `reimpl-x64.exe` embeds `__DATE__`/`__TIME__` in its boot
banner, so **two builds of identical sources have different md5s.** md5 proves "the file
did not change under me between deploy and read"; it does not prove "this is the same
program as yesterday". Use it for the former only.)

---

## 1. PORTED: `??1SaveLoad@Save@@QEAA@XZ` (retail `0x1800c3c90`, ordinal 666)

`runtime/src/save/saveload.cpp`. Wave 5 lane C held it back with three reasons. Two are
still true. **The third is false, and it is the one that made the holdback look cheap:**

> "the object is a process-lifetime singleton so it is unlikely ever to run"

It has four live call sites in this tree, every one of them followed by a
`fn_MemFreeStatic` of the same pointer — i.e. real deletes of a heap object:

| site | when |
|---|---|
| `code/Game/Dialog/mainmenudialog.cpp:218` | `ReturnToMenu`, no-game branch |
| `code/Game/Dialog/mainmenudialog.cpp:226` | `ReturnToMenu`, in-game branch |
| `code/Game/Game/gameupdateloop.cpp:3140` | `GameUpdateLoop::Shutdown` |
| `code/Game/Tutorial/tutorialui.cpp:81` | tutorial teardown |

Quitting to the menu runs it. At cutover the resolve is NULL and
`fn_SaveLoad_Dtor(saveLoad)` is a call through zero on the way back to the main menu.

**The full chain, disassembled at all five addresses** (not decompiled —
`reference_ghidra_c_output_vs_disassembly.md`):

```
~SaveLoad             0x1800c3c90   38 instrs
  -> FUN_1800329c0    0x1800329c0   Array<SaveInfo>::~Array, stride 0x58
       -> FUN_1800c3f40  0x1800c3f40   ~SaveInfo (per element)
            -> FUN_1800331f0 0x1800331f0  Array<ModInfo>::~Array, stride 0x20
                 -> FUN_1800341e0 0x1800341e0  destroy N ModInfo in place
```

All five are transcribed. Three registered by address as well as the export by name.

**Two facts the transcription produced that were not in the tree:**

* `FUN_1800341e0` releases the `System::String` at `ModInfo+0x08` and **then** the one at
  `+0x00`. That is an independent confirmation of the ModInfo layout
  `runtime/types/save/SaveLoad.h` pins from `CheckMod` — two unrelated bodies agreeing on
  name-at-0, path-at-8.
* `~SaveInfo` pins **four offsets the header explicitly calls unmodelled**, and the port
  reaches them as raw displacements rather than renaming the header (PLAN §6 step 8):
  `+0x10` a String slot, `+0x18` a pointer to a *polymorphic* object whose vtable slot 0 is
  called with `EDX=1` (the scalar deleting destructor), `+0x28` a raw heap block, and
  `+0x40` an **int32 ownership flag — both `+0x18` and `+0x28` are torn down only when it
  is `>= 0`**, the same `-1`-is-empty convention `Sprite::index` uses at `+0x08`.

**ABI trap avoided, and it is worth naming because registering by address is what exposes
it.** Retail's `RCX` at `0x1800331f0` is `saveInfo + 0x48` and at `0x1800329c0` is
`saveLoad + 0x50` — the **array header**, not the object containing it. The first draft of
this port passed the `SaveInfo*`. That is invisible while nothing calls the registered
address and a wrong-pointer crash the day something does. The ported helpers now take an
explicit 0x10-byte `RtArrayHeader`.

**Mangling.** `QEAA`, the non-virtual spelling. `SaveLoad` has no vtable (the ctor writes
an int at `+0x00`; the DLL exports no `??_7SaveLoad@Save@@6B@`), so `UEAA` here would have
been a phantom name resolving NULL in silence —
`reference_phantom_dtor_mangling_qeaa_vs_ueaa.md`. Verified present with `pefile` against
the shipped export table before the line was written: `0x1800c3c90`, ordinal 666.

### Oracle: NONE, and that is a property of the function

Everything it does is a free. "Call ours, call theirs, compare" double-frees every block.
There is no scratch-copy form either, and that is the part worth writing down: **a
0x90-byte copy of the live `SaveLoad` aliases the SAME strings and the SAME array buffer**,
so running the DLL's body on the copy destroys the real object's heap before ours runs.
That is why the three oracle shapes this file already uses (FULL DIFF, SCRATCH-COPY,
SCRATCH-OBJECT) all fail here — not for lack of trying one.

What backs it instead, and the numbers actually measured:

**Execution witness.** `BANISHED_SAVELOAD_DTOR_TRACE=1` prints one line per call, because
`calls=0` over a green line is this project's canonical unverified port
(`reference_uncalled_port_is_not_verified.md`) and a destructor deserves the same test even
without the comparison half.

Run: `BANISHED_AUTOSTART=1 BANISHED_PAUSEMENU=600 BANISHED_PAUSEMENU_QUIT=120
BANISHED_SAVELOAD_DTOR_TRACE=1 BANISHED_RT_DIFF=1 BANISHED_RT_STRICT=1`, exe
`c8164f8886be06143b65d49b42a2b1ad` before and after.

```
[SaveLoadDtor] ran on 000002322c2bb378 (call #1)
EXIT=0   standalone_crash.log: 0 bytes
[RtDiff] ==== 389 sites, 14222696 calls, 1 divergences in 1 sites ====
```

The run reached gameplay, opened the pause menu at frame 601, dispatched
`OnCommand(id=6)` and exited 0. `calls=1`, not 0.

**The one divergence is NOT this port, and that was measured rather than argued.** A
control run of the SAME harness on an exe built from the base commit with both of this
lane's source changes stashed out (`0ddcdeca6754e93209b433b0efa40775`, verified unchanged
before and after) produces the identical divergence with identical values:

```
control: 387 sites, 14285528 calls, 1 divergences in 1 sites
         DIVERGENCE Object::Factory::Startup [probe, not diffable]  call #1
           ours=chain=416 nodes=415 badCrc=0  theirs=chain=416 nodes=416 badCrc=0
lane:    389 sites, 14222696 calls, 1 divergences in 1 sites
         DIVERGENCE Object::Factory::Startup [probe, not diffable]  call #1
           ours=chain=416 nodes=415 badCrc=0  theirs=chain=416 nodes=416 badCrc=0
```

The two-site difference between the runs is `Math::Transform::Identity()` and
`Video::Context::SetInverseScale`, both path-dependent and neither touched here (full site
set-diff taken, not eyeballed).

### ⚠ HANDOFF: an UNOWNED divergence in `Object::Factory::Startup`, live today

`Object::Factory::Startup`'s shadow invariant (`runtime/src/object/type_system.cpp:975`)
reports **`chain=416 nodes=415`** — the reflection chain has 416 entries and the factory's
type tree ends up with 415 nodes. One type is being dropped on every boot. It reproduces on
the untouched base commit, it is not in any `docs/runtime_port/*.md` or `*.txt` in the
tree, and this lane does not own it. Flagging it because a standing `1 divergences`
line is exactly how a real defect becomes background noise.

---

## 2. FIXED: `??_7Thread@System@@6B@` — a frozen NULL, not a missing mint

**The mint has always carried this row.** `standalone/rt_data_mint_tables.inc:1250`,
`kVSlots42`, DLL `0x180155f20`, 2 slots (`_purecall`, then
`?OnCompleted@Thread@System@@UEAAXXZ`). So the symbol is not on
`unsatisfied_export_scan`'s list and never was. It appeared on attempt 14's `RT_STRICT`
list for a different reason.

`runtime/src/system/thread_win.cpp` cached it in a plain function-local static:

```cpp
static void* v = ResolveExport("??_7Thread@System@@6B@");
```

That is the exact defect attempt 14 fixed for the allocator's three by-address call sites,
one file over. **The ordering is provable from source, no run required:**

```
RtDataMintInit()                       rt_data_mint.cpp:610
  MintVtables / MintTypeInfos / MintStorage
  MintConstructedStatics()             :615
    ConstructLoader()                  :514  -> calls our ??0Thread@System@@QEAA@I@Z
                                              -> RtSystemThreadVftable()  <-- resolves HERE
  if (BANISHED_RT_MINT) RegisterMintedData()   :627  <-- registers the row HERE
```

The mint's own `ConstructLoader` constructs a `System::Thread` **before the mint registers
anything**. At cutover the first call returned NULL and the static froze it for the life of
the process. Attempt 14's `standalone_error.log` shows it as the **first `[RtUnresolved]`
line of the entire run** (line 6, above every other symbol).

Fixed by putting both resolves on the `RtResolveCached` contract (cache a hit, retry a
miss, name it once): `RT_DATA_SLOT` for the vtable, `RT_EXPORT_ONCE` for
`?ThreadFunc@Thread@System@@CAIPEAX@Z` (which has a port, so a frozen answer there would
be a bypassed port rather than only a miss).

### Honest limits of this fix, stated because they matter

* **It does not remove the FIRST miss.** `ConstructLoader` runs before the mint's
  registration by construction, so `??_7Thread@System@@6B@` will very likely still appear
  **once** on an attempt-15 `RT_STRICT` census. That census records *asks*, not
  *failures-at-use*. What the fix removes is the *permanence*.
* **How load-bearing the frozen null was is not proven.** Every `Thread` subclass in this
  tree re-stamps its own vtable immediately after construction (as retail's do), so a null
  base vtable during construction is overwritten before any virtual dispatch. The one place
  it is read back is `~Thread`, which writes `self->vtable = RtSystemThreadVftable()` and
  *then* signals `hStop` — and `RtSystem_Thread_ThreadFunc` dereferences `self->vtable` and
  calls slots 0 and 1. In the ordinary path the worker wakes on `hStop` (index 1) and exits
  the loop without reading it. So the failure case is a **race**, not the ordinary path. I
  am not claiming a crash was fixed; I am claiming a latent null was removed and a
  `RT_STRICT` row was made recoverable.
* **A no-op with the DLL present**, which is why it could not be verified by a run: both
  forms resolve to the DLL's vtable identically. The verification is the source-level
  ordering proof above.

### ⚠ RECOMMENDED ONE-LINE FIX FOR THE INTEGRATOR (the mint is yours, not mine)

The root cause is the ordering inside `RtDataMintInit`, not the caching. `RegisterMintedData()`
is called *after* `MintConstructedStatics()`, but `ConstructLoader` is itself a **consumer**
of minted data — it looks up `??0Thread@System@@QEAA@I@Z` and `??_7Loader@Engine@@6B@`, and
through the Thread ctor it asks for `??_7Thread@System@@6B@`. Moving `RegisterMintedData()`
to sit between `MintStorage()` and `MintConstructedStatics()` closes the hole at the root
and would make the Thread vtable resolve on its *first* try. `MintVtables`/`MintTypeInfos`/
`MintStorage` all run before that point, so the tables are complete when it would register.
I did not make that edit: lane BOOT2's banner records that the integrator owns the mint.

---

## 3. FIXED: `tools/unsatisfied_export_scan.py` could not see `RT_DATA_SLOT`

Found by accident and it invalidates any before/after taken across a door conversion.

The scan's `DOOR` regex listed `RT_EXPORT_ONCE | RT_ORIGINAL_ONCE | ResolveExportRaw |
ResolveExport | RtOriginal` — **but not `RT_DATA_SLOT`**, which is the same
`RtResolveCached` macro family with a literal name. Converting one
`ResolveExport("??_7Thread@System@@6B@")` into `RT_DATA_SLOT(void*, "…")` moved the scan's
denominator **789 → 788 with nothing having been satisfied by anything.** A change that
makes a door *safer* must not be able to make it *invisible to the instrument that counts
doors* — `reference_documenting_a_check_can_delete_it.md`, one tool over.

With `RT_DATA_SLOT` added, **the true denominator is 812, not 789.** 23 names tree-wide
were resolving through a door this scan could not see. That is the fourth instrument in
this wave found blind to a macro door (`runtime_census.py` was the other three), and it is
worth stating as a pattern rather than a bug: **every door-counting instrument in this tree
needs its macro list audited against `runtime/funcs/rt_port.h`, not extended one macro at a
time as each is discovered.**

### The accounting, on the fixed tool, both sides

| | denominator (names through a door) | no port and no mint | of those, ≥1 LIVE site |
|---|---|---|---|
| before (source changes stashed) | **812** | 149 | 146 |
| after | **812** | **148** | **145** |

Set diff, `before` → `after`:

```
< ??1SaveLoad@Save@@QEAA@XZ
```

Exactly one name removed, **nothing added**, denominator **unmoved**. That is the standard
lane FIND set. `??_7Thread@System@@6B@` correctly appears in neither list — it has a mint
entry, so §2 is not claimed here.

Other static gates, after:

* `tools/port_bypass_audit.py` — **0 bypasses on both doors** (`ADDRESS door: 209 sites, 0
  reach a function we have ALREADY ported by name`; `NAME door: 429 raw-door sites, 0 are a
  ported function on a live path`). Address-door buckets `ADDR-REGISTERED=33 DETOURED=3
  GUARDED-FALLBACK=1 NO-EXPORT=96 UNPORTED=76`.
* `tools/export_name_audit.py` — 660 distinct names, **1 not in the DLL**, and it is the
  pre-existing doc-comment `?_singleton@...` in `runtime/funcs/rt_resolve_cache.cpp:8`, not
  a registration.
* `tools/runtime_census.py` (post-`29cfaa2`) — **958 called / 666 ported / 69.5%**, from
  the integrator's post-fix baseline of 958 / 69.4%. `called` unmoved (the name was already
  counted), `ported` +1.

---

## 4. NOT PORTED (7), with the honest reason for each

Four of these seven have a stated blocker in WAVE9.md's triage table that is **wrong about
its own shape**. `reference_holdback_blockers_are_stale_check_them_first.md` says to
re-check the blocker before porting; doing that is most of this section's value.

### 4.1 `?Update@Scene@Audio@@` — **the stated oracle blocker is DISSOLVED; what is left is size**

WAVE9.md rows 2-4 hold the three Audio `Update` bodies on a shared root cause: *"every path
ends in a STORED COMPLETION CALLBACK (`CALL RAX`), which is why the bounded state cannot be
snapshot/restored"*, plus for `Scene::Update` specifically *"it consumes `System::Random`,
so even a perfect device fake needs the RNG snapshotted too"*. Both halves are now stale:

* **The device-level fake exists and works.** `standalone/rt_orphan_probe.cpp:466`
  `ProbeAudioScene` runs a `SynthScene` (a real `Audio::Scene` + 256 `Channel` + 4096
  `EffectSlot`, seeded deterministically) through **five** Scene mutators, and it defeats
  the callback problem exactly the way the brief predicted — **`Device::Stop` is detoured to
  a recorder for the duration**, so the sentinel platform-channel pointers are never
  dereferenced. It even carries the two controls this project insists on: an untouched
  third scene `C` as an effectiveness control (which *earned its keep* — phase 1 gave
  `PauseEffect` 64 green cases of which 0 changed anything) and a negative control (a
  `StopAll` mutated to work on `i` instead of `idx` passed 264 cases before the aliased
  case was added).
* **The RNG is not ambient state, it is a parameter.** The mangled name says so:
  `?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@AEBVVector3@Math@@M@Z` —
  `AEAVRandom@4@` is a `Random&` argument. The probe already seeds both sides identically.

**Also stale: WAVE9.md rows 5-7** (`StopAll` `0x1800df150`, `Pause` `0x1800df070`,
`StopEffect3D`) are listed as "PORT with 2-4". They are **already ported and swept** by
`ProbeAudioScene`. The table has not been updated.

**Why I still did not port it: 438 instructions**, driving 3D positional audio math and
effect lifecycle across two helper calls per channel. That is a lane, not a slot at the end
of another lane, and porting it half-verified is worse than leaving it named. **The correct
next step is a sixth entry in `ProbeAudioScene`'s `kFns[]`, not more analysis.**

### 4.2 `?Update@Device@Input@@` — **not "the hardest single item"; it is 61 instructions with a 565-instruction tail**

WAVE9.md row 16 describes it as *"Four unexported, unnamed internal helpers plus
DirectInput8 COM pointers past `Device+0x11D0` (unmapped)"* and calls it the hardest single
item in the wave. Disassembled, `Update` itself (`0x1800e1490`) is **61 instructions** and
is a four-call shell:

```
Update(this, const Time& t):
    zero a 0x68 x 0x18 buffer on the stack
    if (PollKeyboard(this, buf1))          0x1800e1c60    98 instrs   <- DirectInput8 COM
        DispatchEvents(this, this+0x6c0,  buf1, 0x69, t)  0x1800e0fc0  234 instrs
    zero a 0x0c x 0x18 buffer
    if (PollMouse(this, buf2))             0x1800e1e10   192 instrs   <- DirectInput8 COM
        DispatchEvents(this, this+0x1098, buf2, 0x0d, t)  0x1800e0fc0
    <a DEAD 4 x 0x11 loop: see below>
    tail JMP                               0x1800e2130    41 instrs
```

So the true job is **626 instructions across five bodies**, the COM is confined to the two
pollers, and `Update` itself is trivial. Porting `Update` alone at cutover produces a shell
calling three unregistered addresses — which is a *precise* statement of the blocker, and a
different one from the table's.

**A finding inside it, of the `dirAlias` family in reverse.** At `0x1800e1560` retail emits
a `4 x 0x11` loop consisting only of `DEC ECX ; LEA RAX,[RAX+0x18] ; JNS` — **no memory
access at all**, the result discarded. Unlike lane FIND's `dirAlias` (a local that looked
dead and was a side-effect carrier), this one really is dead: there is no helper to mutate
through, no shared buffer, and the two sibling zeroing loops above it *do* carry
`MOV qword ptr [RAX-0x6],RSI` stores that this one does not. A port may omit it, and should
say so where it does — the rule is "check whether the helper mutates through a shared
buffer", and here there is no helper.

### 4.3 `?FindOrLoad@ExternalFactory@Object@@` and `?Clean@ExternalFactory@Object@@`

**`Clean` is `0x1800be310`, ~140 instructions. `FindOrLoad` is `0x1800bf320`.** WAVE9.md
rows 27-28 are right that neither has an oracle — both rewrite the one shared resource AVL
tree, and a private replica is exactly what a stateful port must not keep. That still
stands and this lane did not dissolve it.

**But there is a second blocker nobody has recorded, and it is live at cutover right now,
with or without a port.**

`standalone/filename_runtime_hook.cpp:566` `InstallFindOrLoadWatch()` **byte-patches a
detour onto the DLL's own bodies** at `0x1800bf320` and `0x1800bc620`, wrapping both in one
recursive `CRITICAL_SECTION` (`g_resCS`). The file's own comment says why, and it is not a
diagnostic — it is a shipped fix:

> our loader calls `FindOrLoad` concurrently (confirmed: 2 distinct tids during the menu
> load). `FindOrLoad` / `External::Load` are NOT thread-safe: `FindOrLoad` mutates the
> factory's tree … the factory cache (racy Add -> `FindOrLoad` returns NULL for a real
> resource). Default ON.

Attempt 14 already shows it standing down:

```
[FNHook] could not resolve ExternalFactory::FindOrLoad
[FNHook] could not resolve External::Load
[ResLoad] serialized: FindOrLoad=0000000000000000 External::Load=0000000000000000 (recursive CS)
```

**So the DLL-less boot has NO resource-loader serialization today**, and it has already
reached `[AutoStart] ENABLED`, i.e. the phase where the two-thread race lives. That is a
correctness hole independent of any port.

**And it gets worse the moment `FindOrLoad` IS ported**, in a way that is easy to miss:
`DetourRuntimeFunc` is handed `ResolveAddr(Module::Runtime, 0x1800bf320)`, and
`ResolveAddr` **consults the port registry FIRST**
(`reference_resolveaddr_consults_port_registry_first.md`). So a registered port at that
address makes the detour installer try to patch *our own GCC prologue* against the 14 MSVC
prologue bytes it expects. It will not match, and the failure is not a crash — it is one
line, `[FNHook] … prologue mismatch — refusing to patch`, and a silently unserialized
loader.

**→ Whoever ports `FindOrLoad` and `External::Load` MUST take `g_resCS` inside the ported
bodies** (or move the lock to the call sites), or the menu-load race the detour exists to
fix comes back. This is the single most important thing in this report for the integrator.

### 4.4 `?SaveText@External@Object@@` — blocker re-checked and it **HOLDS**

WAVE9.md row 26 blocks it on `IO::TextFileStream`, which WAVE4.md says in as many words it
did not port. Re-checked rather than assumed (the rule that has been right three for three
recently): `TextFileStream` appears in exactly **two places in the entire tree, both
comments** — `runtime/src/object/external.cpp:46` and
`runtime/types/object/External.h:85`. There is no port, no type header, no registration.
`SaveText` drives the `+0x28 Serialize(IO::Stream&)` slot with one. **The blocker is
current.** Needs `IO::TextFileStream` first; that is a lane.

### 4.5 `?WriteSettings@Persistence@IO@@` (`0x1800b88e0`, ~330 instructions)

Wave 4 lane C held `ReadSettings` and `WriteSettings` together on *"no safe oracle exists,
both mutate the real HKCU registry"*. Lane BOOT2 already showed that sentence was **true of
`WriteSettings` and false of `ReadSettings`** and ported the latter. This lane did not port
the former, and the reason is size plus the standing registry hazard, not a new blocker.

Decoded structure, handed on so the next lane does not redo it:

```
phase 1  Registry reg(&reg, nullptr)                    0x1800b9500
         in-order walk of the settings Set<Pair<String,String>>
           (node: key String @+0x00, value String @+0x08,
            parent @+0x10, left @+0x18, right @+0x20)
           for each: two String->WideString conversions  0x1800cab50
                     RegSetValueExW(hKey, wideKey, 0, REG_SZ,
                                    wideValue, 2 + 2*wcslen)   IAT 0x18014b018
                     release both temporaries
         RegCloseKey                                     IAT 0x18014b030
phase 2  Registry reg(&reg, nullptr)  again
         loop RegEnumValueW(hKey, idx++, nameBuf, &cch, …) IAT 0x18014b000
           bail on non-zero, or cch > 0x400
           WideString -> String       0x1800caf40 then 0x1800cacd0
           Map find                   0x18002ace0
           if NOT found: append the wide name to a growing Array<String>
                         (the R12/R13 array, grown 3x on the doubling branch)
         then for each collected name: delete it            IAT 0x18014b008
         RegCloseKey; tear the array and both Registry strings down
```

**An oracle IS available and this lane did not have the budget to build it, which is a
different sentence from "no safe oracle exists".** Both `Registry` constructions go through
the exported `??0Registry@IO@@QEAA@PEBD@Z` with a null path argument, so a differential run
against a **scratch key** is reachable by giving the two sides different `Registry` objects
rather than by mutating `HKCU\…\Banished`: seed a synthetic settings map, point one side at
`HKCU\Software\…\Banished_oracle_A` and the other at `…_B`, run ours and theirs, then read
both keys back with `RegEnumValueW` and compare the *value sets*. The player's real key is
never opened. That is the `reference_synthetic_subject_beats_a_fake.md` shape and it is the
recommended approach.

### 4.6 `?LoadResources@UpdateLoop@Engine@@` (`0x1800dd7d0`, ~280 instructions)

Blocked on §4.3, hard. It calls `?FindOrLoad@ExternalFactory@Object@@` (`0x1800bf320`)
**three times** and `0x1800c0390` after each, walks a 0x80-stride table off
`0x1801fc6b0`, and builds a `String` by hand from a `.rdata` literal at `0x180157c14`. It
cannot be ported before `FindOrLoad`, and porting it after inherits `FindOrLoad`'s
serialization problem in full.

---

## 5. Cutover surface: what this lane changed

**Removed from the surface (1):** `??1SaveLoad@Save@@QEAA@XZ`.

**Made recoverable rather than permanently frozen (1):** `??_7Thread@System@@6B@` — see the
honest limits in §2; expect it to still appear **once** on an attempt-15 census, and see the
recommended `RegisterMintedData()` re-ordering that would remove even that.

**New addresses registered, so the integrator can see them (3, all by address, all onto
this lane's own transcriptions — these SHRINK the surface rather than growing it):**

| address | what | why registered |
|---|---|---|
| `0x1800329c0` | `Array<SaveInfo>::~Array` | nothing reaches it by address today; registered so a future ported `UpdateList`/`LoadMetaData` reaches our body and not a null |
| `0x1800c3f40` | `~SaveInfo` | same |
| `0x1800331f0` | `Array<ModInfo>::~Array` | same |

Plus `0x1800c3c90` as the second door onto the exported dtor.

**No new by-address dependency on DLL internals was created.** Every free in the ported
chain goes through this file's existing `MemFree()` accessor (`RT_EXPORT_ONCE` on
`?MemFree@MemoryAllocator@System@@SAXPEAX@Z`), which is our port. Retail's array-buffer
frees are written out as `Lock(alloc+0x640); ?Free@(alloc, p); Unlock()` — which is what
`?MemFree@` *is*, so this is a composition identity, not a substitution.

---

## 6. Files touched

```
runtime/src/save/saveload.cpp        ~SaveLoad + 4 helper bodies, registration, witness
runtime/src/system/thread_win.cpp    two resolves onto the RtResolveCached contract
tools/unsatisfied_export_scan.py     RT_DATA_SLOT added to the DOOR regex
docs/runtime_port/WAVE9_LANE_SYS2.md this file
```

## 7. Open items handed on, in priority order

1. **`FindOrLoad`/`External::Load` serialization is ALREADY GONE at cutover** (§4.3). Live
   correctness hole today, independent of any port, in a phase attempt 14 has reached. And
   porting `FindOrLoad` without moving the lock into the body makes it *silently* worse.
2. **`Object::Factory::Startup` drops one type on every boot** — `chain=416 nodes=415`,
   reproduced on the untouched base commit, unowned, unrecorded (§1).
3. **`RegisterMintedData()` runs after its own consumer** `MintConstructedStatics()`
   (§2). One-line move; the integrator owns the file.
4. **Audit every door-counting instrument's macro list against `rt_port.h` in one pass**
   (§3), rather than discovering them one macro at a time. `unsatisfied_export_scan.py`
   was the fourth.
5. **WAVE9.md's triage table is stale for Audio rows 5-7** (already ported and swept) and
   **wrong about the shape** of rows 2-4 and row 16 (§4.1, §4.2).
6. `?Update@Scene@Audio@@` next step is a **sixth `kFns[]` entry in `ProbeAudioScene`**, not
   more analysis.
