# WAVE 19 lane PERSIST — `IO::Persistence`, save enumeration and load kickoff

Worktree `C:/code/GIT/wt-w19-persist`, branch `wave19/persist`, cut from
`wave19/base` @ `20aa588`. Every figure below was measured on this branch;
nothing is inherited.

**All 7 rows ported, registered by name AND address, and verified against the
shipped DLL.** `called` grew by **3 internal addresses**, named in §6.

---

## 0. Headline, in one paragraph

The blocker the brief predicted **does not exist**. The three `Begin*` methods
were expected to share a stored completion callback — the single root cause
behind all six Wave 8 Audio holdbacks. Reading all three bodies and both
private enqueue helpers as **disassembly**, there is not one `CALL RAX` and not
one indirect call through an object field anywhere in the cluster. The
asynchrony is a `System::Thread` embedded at `Persistence+0x10`, driven by a
command word, a status word and a `ResetEvent`/`SetEvent` pair. That IS one
shared root cause for the three rows — it is simply a friendlier one, and it is
what made a synthesised whole-object oracle possible where Audio's callback
made one impossible. **One finding, not three.**

---

## 1. The seven rows

| row | retail | ord | ported | registered | verified |
|---|---|---|---|---|---|
| `?GetSaveCount@Persistence@IO@@QEBAHXZ` | `0x1800b7c60` | 2884 | yes | name + addr | scalar oracle, 2 of 2, control 1 of 1 |
| `?GetSaveInfo@Persistence@IO@@QEBA?AUSaveGameInfo@12@H@Z` | `0x1800b7bd0` | 2888 | yes | name + addr | by-value oracle, 6 of 6, control 3 of 3 |
| `?GetSaveFilename@Persistence@IO@@AEBA?AVFilename@System@@AEBVString@4@@Z` | `0x1800b7c80` | 2887 | yes | name + addr | 10-input sweep, 10 of 10, control 10 of 10 |
| `?BeginDelete@Persistence@IO@@QEAA_NAEBVString@System@@@Z` | `0x1800b7ab0` | 1638 | yes | name + addr | whole-object, 2 issue + 2 refusal |
| `?BeginLoad@Persistence@IO@@QEAA_NAEBVString@System@@HPEBUSaveBlock@12@@Z` | `0x1800b7970` | 1647 | yes | name + addr | whole-object, 1 issue + 1 refusal |
| `?BeginGetSaveGameInfo@Persistence@IO@@QEAA_NAEBVString@System@@H@Z` | `0x1800b7870` | 1642 | yes | name + addr | whole-object, 2 issue + 2 refusal, both controls |
| `??1Persistence@IO@@QEAA@XZ` | `0x1800b7fb0` | 647 | yes | name + addr | **NOT VERIFIED** — §7 |

Every address was checked against the shipped export table with `pefile` before
a line was written, so none is a phantom.

Files:
* `runtime/src/io/persistence_saves.cpp` — six rows
* `runtime/src/io/persistence_saves_win.cpp` — `BeginGetSaveGameInfo` (the only
  member that reaches the Win32 event API in its own body)
* `runtime/src/io/persistence_saves_probe.cpp` — the oracle, registers nothing
* `runtime/types/io/Persistence.h` — layout, extended
* kill switch `BANISHED_RT_PERSAVES=0`, separate from `BANISHED_RT_PERSISTENCE`
  (which gates the boot-path settings half)

---

## 2. What the tree already had — the grep came first

Per §2 of the plan, the first hour was grep, not disassembly. Result:

* `persistence.cpp`, `persistence_startup.cpp`, `persistence_registry_win.cpp`
  exist and cover the **settings** half (Get/Set ×2, GetSavePath, ReadSettings,
  WriteSettings). **None of my seven was among them.**
* All seven were reached through `RT_EXPORT_ONCE` **doors** in
  `runtime/src/save/saveload_update.cpp` (and `engine_lifecycle.cpp:1001` for
  the dtor) — exactly the `?Commit@CompressedStream@` shape §2 warns about: a
  name that appears in the tree and is not a body. `ResolveExport` falls through
  silently, so those doors were reaching the DLL.
* `runtime/types/io/Persistence.h` was already pinned for `+0x00`, `+0x08`,
  `+0x90`, and — decisively — already carried `SaveBlock` and `SaveGameInfo`
  with the hidden-return-slot warning spelled out. **That saved the whole
  layout derivation for the two by-value rows.**

**Nothing in this lane duplicates an existing body.**

### The stated blockers were re-checked, per doctrine

`saveload_update.cpp` carries prose around several of these doors. Every claim
that could have blocked the lane was re-checked against the shipped bytes
rather than believed. The one that mattered — the expectation of a stored
completion callback — was **false**, and §0 says how that was settled.

---

## 3. Layout: every field read TWICE

`Persistence.h` now pins `+0x10..+0x8f`. The offsets are not from one reader.
Each was read once **Persistence-relative** in a public entry point and once
**worker-relative** (`rdi = this+0x10`) in a private enqueue helper, and the two
agree — the table is in the header banner. That matters because a field pinned
by readers that all test the same thing cannot be trusted
(`reference_a_writer_beats_readers_agreeing_on_a_sign.md`); here the two views
are structurally independent, since one addresses through `Persistence*` and the
other through the embedded worker.

`??0Persistence@IO@@` (`0x1800b8020`) is what settles the embedding: it runs
`??0Thread@System@@QEAA@I@Z(this+0x10, 0x10000)` and then stamps a derived
vtable (`0x180155860`) over `this+0x10`. `IO::Persistence` **embeds** a
Thread-derived save/load worker; it is not one.

---

## 4. The oracle — and the two defects it found in ITSELF first

`BANISHED_RT_PERSIST_PROBE=1`. Evidence:
`docs/runtime_port/wave19_persist/probe_run.txt` (and the full stderr beside it).

**Why synthesised.** Three of the seven mutate the singleton and wake a worker
thread, so ours-then-theirs on the live object issues two requests for one call.
That is not "no oracle" — the whole issued state is inside the object. Two
byte-identical scratch `Persistence` images; ours runs on one, the shipped body
on the other; the whole `0x98` bytes are compared.

**The one comparison exception, stated so it can be checked.** `+0x40`
(`pendingFilename`) is compared by **content**, not position. Both sides
allocate their own String buffer, so a raw memcmp of that qword would fail 100%
of calls and read like a catastrophic port failure. Everything else is
position-compared, including bytes neither side should touch.

### Results — every green with its denominator

```
whole-object (0x98 bytes, +0x40 by content)  control 2 of 2 caught | 5 agreed, 0 diverged, 5 inert, of 10 applicable
GetSaveFilename content                      control 10 of 10      | 10 agreed, 0 diverged, 0 inert, of 10 applicable
GetSaveInfo by-value struct                  control 3 of 3        | 6 agreed, 0 diverged, 0 inert, of 6 applicable
GetSaveCount scalar                          control 1 of 1        | 2 agreed, 0 diverged, 0 inert, of 2 applicable
VERDICT: clean | 0 divergences | 5 inert cases
```

Controls print **before** any green, and the verdict refuses to read a green
unless every screen has a control that read non-zero.

### The two defects the probe found in itself

Both are recorded rather than quietly fixed, because the way each was wrong is
the reusable part.

**(a) A union denominator turned a fully effective control into a half-blind
one.** CTRL-VAL first printed **"3 of 6 caught"**. The mutant ignores the
refusal branch, so on the three `flags=0` cases it is byte-identical to the
port — those cases were **inapplicable**, not missed. Scored on applicable
cases only it is **3 of 3**. This is `reference_union_control_hides_a_blind_screen.md`
in its inverted form: the memory records a union hiding a *blind* screen; here
a union was slandering a *working* one. The rule is the same — score each
control on its own applicable cases.

**(b) One screen had no positive control at all, and the verdict did not
notice.** `GetSaveCount` printed **"control 0 of 0 caught"** beside two greens.
The verdict predicate named three screens; there were four. A screen with no
control is indistinguishable from a working one from outside — §6.6's "no
difference found" vs "I measured nothing", in its most literal form. Fixed by
adding CTRL-CNT and by making the predicate a fold over every screen rather
than a hand-written conjunction.

### Effectiveness, reported by the probe itself

> the whole-object screen compares 152 bytes, of which the subjects write at
> most **8** (command, blockCount and the filename pointer). The other 144
> bytes are a SCRIBBLE check, not a behaviour check.

That is deliberate. Quoting "152 bytes compared" as coverage would overstate
this screen by a factor of nineteen.

### The 5 INERT cases are the refusal branch, and they ARE verified

The `flags=1` cases write nothing on either side, so the byte screen measures
nothing — the probe says so rather than counting them green. What verifies them
is the pair of *negative* claims (`ours == theirs == false`, and neither side
wrote a byte), with **CTRL-OBJ/refuse** as the control: a mutant that issues
anyway is **caught at firstDiff=76 = `+0x4c`**, the command word, by the
mechanism the control names. Same shape as the `Application::PlatformDestruct`
row that was verified to do nothing.

### A second, independent comparison

The ported bodies also carry in-body `RT_DIFF` oracles that call the shipped
export and compare. Under `BANISHED_RT_DIFF=1` these reported **0 divergences**
over the probe's traffic — a different comparator, different code path, same
verdict. These are real oracles, not `RT_DIFF_SHADOW` self-checks: they resolve
through `RT_ORIGINAL_ONCE`, which deliberately bypasses the port registry.

---

## 5. `calls=` — and the control that stopped me quoting the wrong number

A `BANISHED_RT_DIFF=1` run **with the probe on** printed:

```
IO::Persistence::GetSaveFilename   calls=17  divergences=0
IO::Persistence::GetSaveCount      calls=2   divergences=0
IO::Persistence::GetSaveInfo       calls=6   divergences=0
IO::Persistence::BeginGetSaveGameInfo / BeginDelete / BeginLoad   calls=0
```

Read alone, the first three look like live-route evidence. **They are not.**
The control — the same run with the probe **off**
(`docs/runtime_port/wave19_persist/live_diff_noprobe_error.log`) — prints **no
row at all** for any of the six: the site never registers, because the body
never runs.

**So `calls=17/2/6` were 100% my own probe, and the live gameplay route
(`BANISHED_AUTOSTART=1` → world → pause menu → quit) reaches none of the six.**
Quoting them as live coverage would have been precisely the error the brief
warned about.

**The three counts reconcile exactly with the probe**, which is the second,
independent route to the same conclusion: `GetSaveFilename` = 5 issue subjects
at `flags=0` + 10 sweep inputs + 2 mutants that call the real body = **17**;
`GetSaveCount` = 2 non-mutant cases = **2**; `GetSaveInfo` = 2 flag states x 3
indices = **6**. Not one call is unaccounted for, so not one came from the game. Verification for all six rests on the synthesised oracle, which
is the sanctioned position and not a weaker one.

`calls=0` for the three `Begin*` in the probe-on run is the harmless one of the
four causes, and it was **checked, not assumed**: `rt_diff.cpp:84` increments
`calls` inside `RtDiffReport`, which only the comparison macros call, while
`RT_DIFF_BEGIN` merely *registers* the site. So a `Begin*` body that ran with no
oracle arm — an issue is not replayable in place — prints `calls=0` while being
fully live. That same mechanic is what makes the probe-off control decisive: a
row that does not print **at all** never even registered, so the body never ran.
The probe, not this counter, is the `Begin*` evidence.

**Why no live route was driven:** the machine has **no save games at all**
(`C:/GOG Games/Banished/Save` does not exist), so the menu Load list is empty
and `BANISHED_AUTOLOAD` has nothing to name. Producing a save is lane work in
its own right and is left explicitly undone — see §7.

---

## 6. Effect on the identity, and the set-diff of what entered `called`

Measured with `tools/runtime_census.py --namespace IO`, run in **both** trees
(base `20aa588` in a temporary detached worktree, and this branch):

```
base  20aa588 :  IO  407 total   122 called   83 ported   ceiling 97.5%
                 identity: 83 >= 122 - 3 = 119   ->  36 SHORT
this branch   :  IO  407 total   122 called   90 ported   ceiling 97.5%
                 identity: 90 >= 122 - 3 = 119   ->  29 SHORT
```

**`+7 ported`, `called` unchanged at 122, 36 SHORT → 29 SHORT.** All seven rows
left the census's called-but-not-ported list (39 → 32, of which 3 are the
UNPROMOTABLE singletons); verified by name, not by count.

**Set-diff of what entered `called`** — a set, never a percentage. `called` grew
by **3**, all in the census's `internal` bucket (164 → 167), and all three are
mine:

```
ENTERED called:   0x18002a930   Collection clear, the +0x60 block-descriptor array
                  0x1800b7390   the private BeginDelete enqueue helper (command 2)
                  0x1800b7740   the embedded worker's destructor
LEFT called:      (none)
```

Three further addresses this lane calls — `0x1800b7460` (the BeginLoad enqueue
helper), `0x18000f7d0` (the settings-map dtor) and `0x180024aa0` (the +0x70
array clear) — were **already** in `called`, so they are not in the diff. All
six are genuinely unexported DLL internals: absent from the export table **by
address**, checked, not inferred from an `AEAA` in a mangled name. They are
called by address on exactly the precedent `persistence.cpp` set for
`Map_Find`/`Map_Insert`.

The two *exported* dependencies (`?Format@String@System@@`,
`?AdjustDelimiters@Filename@System@@`) are reached through `RT_EXPORT_ONCE`,
the port-registry-aware door, and both are already ported — so no name entered
`called` either.

---

## 7. What this lane does NOT claim

1. **`??1Persistence@` is PORTED, NOT VERIFIED.** No oracle. Ours-then-theirs
   destroys one object twice, and the synthesised-scratch route is unavailable
   because the worker destructor (`0x1800b7740`) joins and closes a real
   thread — a fabricated worker there is a crash, not an experiment. The body
   is a three-step transcription (release `_savePath`, worker dtor, settings-map
   dtor) with one documented deviation: retail's tail-`jmp` into `0x18000f7d0`
   is written as a plain call. **This is the lane's one unverified row.**
2. **No live gameplay route was driven for any of the six**, and §5 gives the
   control that proves it. The synthesised oracle is the evidence.
3. **The whole-object screen discriminates on at most 8 of the 152 bytes it
   compares.** The remaining 144 are a scribble check. A defect that writes a
   *correct-looking* value into one of those 8 fields for the wrong reason is
   inside the screen; a defect in the worker thread's later behaviour is not.
4. **The worker thread itself is untouched and unverified.** This lane ports
   the ISSUE side only. What the worker does with command 1/2/3 after the
   `SetEvent` is out of scope and still entirely the DLL's.
5. **`?BeginGetSaveGameList@` (command 4, `0x1800b7b30`), `?GetStatus@`
   (`0x1800b7c40`) and `?GetDataSize@` (`0x1800b7b80`) are NOT in this lane** and
   were not ported, though all three were read while pinning the layout and all
   three are small. They are the obvious next cell.
6. **`tools/abi_retslot_audit.py` is GREEN and BLIND to this lane's two
   by-value rows.** It reports `0 suspicious of 61 by-value-returning exports
   checked` with `77 UNAUDITED`, and its own banner says it cannot see a resolve
   through a macro door — which is how both `GetSaveInfo` and `GetSaveFilename`
   are reached. Its green is real and it is **not** evidence about these two
   rows. What is: the disassembly (`0x1800b7bd0`'s first instruction spills RDX,
   not an int), the two caller typedefs in `saveload_update.cpp` (both correctly
   carry the slot — read, not assumed), and the probe's by-value screen.
7. **`tools/dispatch_slot_map.py --check` is at 0 mismatches of 110** — but this
   lane registers no dispatch slots, so that is a hygiene check, not a result.
8. The probe's `Format()` uses the **raw** DLL door deliberately (it builds the
   oracle's inputs and is not the subject). If `String::Format` were itself
   wrong, both sides would receive the same wrong inputs and the probe would not
   see it. That is a real, bounded blind spot; `String::Format` is verified by
   Wave 2, not here.

---

## 8. One thing found that is not this lane's to fix

`BANISHED_RT_ORPHAN_PROBE=1` **dies at boot on this base**: ACCESS_VIOLATION,
`WRITE of 0x648`, inside `ProbeSaveLoadGenerateSaveName` reached from
`RtOrphanProbe()` in `Init()` — the allocator singleton does not exist yet.
That independently reproduces the fault
`WAVE19_INTEGRATOR_CORRECTIONS.md` **C4** records as fixed on `wave14/genname`
(`3e08528`) and explicitly **not merged** here.

This lane worked around it rather than fixing it: the probe has its own call
site in `code/Game/Game/camera_input.cpp` and its own env var, so a wave-14
holdback does not become a wave-19 blocker. **The C4 decision is still open**,
and this is a second, independent reason to take it.
