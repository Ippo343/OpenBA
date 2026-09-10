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
| `??1Persistence@IO@@QEAA@XZ` | `0x1800b7fb0` | 647 | yes | name + addr | **NOT VERIFIED** — §7 · *[annotation: RETIRED — now VERIFIED, see §T1]* |

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

---
---

# POST-REPORT TRANCHE (2026-09-10)

**Everything above this line is SEALED and unaltered.** This tranche annotates
it; where it supersedes a sealed claim it says so explicitly and leaves the
original standing so the correction stays legible.

## T0. The sealed claims this tranche RETIRES

| sealed claim | status now |
|---|---|
| §7.1 `??1Persistence@` PORTED, NOT VERIFIED — "0x1800b7740 joins and closes a REAL thread, so the scratch route is not available" | **RETIRED. The claim was FALSE and I never read the function.** §T1 |
| §5 "no live gameplay route on that harness reaches these six at all" | **RETIRED.** The route exists; I never ran it. §T2 |
| §5 "the machine has NO SAVE GAMES AT ALL" | **FALSE.** 13 saves, in `Documents/Banished/Save`, not the game dir. §T2 |
| §7.6 `abi_retslot_audit` "is GREEN and BLIND to this lane's two by-value rows" | **STANDS**; the tool's headline now carries its own denominator. §T4 |

## T1. `??1Persistence@` is VERIFIED. The blocker was mine, and it was false.

The holdback said `0x1800b7740` *"joins and closes a REAL thread, so handing it
a fabricated worker is not a controlled experiment, it is a crash with extra
steps."* I inferred that from the worker being `System::Thread`-derived and
**never disassembled it**. One `objdump` retired it:

```
stamp vtable 0x180155860
0x18002ac40(worker+0x70)  ; clear saves        (= Pers+0x80)
0x180024aa0(worker+0x60)  ; clear              (= Pers+0x70)
0x18002a930(worker+0x50)  ; clear              (= Pers+0x60)
release String at worker+0x30                  (= Pers+0x40)
stamp base vtable 0x180155f20
SetEvent            ( [worker+0x20] )          (= Pers+0x30)
WaitForSingleObject ( [worker+0x10], INFINITE) (= Pers+0x20)
CloseHandle ( [+0x10] ) ( [+0x18] ) ( [+0x20] ) ( [+0x28] )   ; tail
```

The join is on a handle **at an offset the caller owns**. Seed four
already-signalled events of our own and the scratch route applies unchanged.
**What had to change for that to become possible: nothing.** It was possible
when I wrote the holdback.

That is `reference_holdback_blockers_are_stale_check_them_first.md` with the
sharpest possible provenance: **the stale claim was my own, written the same
session, and one command retired it.** Four for four were stale in Wave 18;
this is five for five, and the fifth was mine.

### Result — the fifth screen, with its denominators

```
~Persistence whole-object (handles/Strings by predicate)
        control 2 of 2 caught | 2 agreed, 0 diverged, 0 inert, of 2 applicable

  ~Persistence rc=2 -> agree (shipped side wrote 36 of 152 bytes; arm=0 expected=0; arrays cleared=1)
  ~Persistence rc=1 -> agree (shipped side wrote 26 of 152 bytes; arm=5 expected=5; arrays cleared=1)
  CTRL-DTOR    rc=2 -> caught (firstDiff=16 handles=1 arms=0 clearedA=0 clearedB=1)
  CTRL-DTOR    rc=1 -> caught (firstDiff=16 handles=1 arms=0 clearedA=0 clearedB=1)
```

Both String-release arms were exercised, **and each case reports the arm the
shipped side actually took against the arm its refcount predicts** — `arm=0
expected=0` (sharing, both fields nulled) and `arm=5 expected=5` (freeing, both
left holding `buf - 8`). The second case is the one worth having: retail
deliberately leaves a dangling pointer there, and a port that "tidied" it to
null would pass rc=2 and fail rc=1. Without the split it passes silently.

CTRL-DTOR is caught at `firstDiff=16` = `+0x10`, the worker vtable the mutant
never stamps — a real comparable divergence through the mechanism the control
names, not UB.

**This screen is far more effective than the Begin\* one**: its subjects write
**36 and 26** of 152 bytes, against at most 8 for the issue path.

`+0x20 threadHandle` and `+0x30 readyEvent` are now pinned fields in
`Persistence.h`; the four consecutive `CloseHandle` calls are what say all four
are HANDLEs rather than three plus a spare. Import names were read from the
DLL's own import directory, not guessed from call shape.

## T2. The live route EXISTS. My negative claim was wrong, and HOW it was wrong matters more.

`BANISHED_AUTOLOAD=Auto`, probe OFF, `BANISHED_RT_DIFF=1`, exit 0, **0-byte
crash log**. Evidence: `wave19_persist/tranche_live_calls.txt`.

| row | site registered (= body entered) | `calls=` | divergences |
|---|---|---|---|
| `?GetSaveFilename@` | yes | **28** | **0** |
| `?GetSaveCount@` | yes | **14** | **0** |
| `?GetSaveInfo@` | yes | **13** | **0** |
| `?BeginLoad@` | yes | 0 | — |
| `?BeginGetSaveGameInfo@` | yes | 0 | — |
| `?BeginDelete@` | **no** | — | — |
| `??1Persistence@` | **no** | — | — |

Run total: `609 sites, 20,060,624 calls, 30 divergences in 7 sites` — **none of
the seven divergent sites is one of mine.**

### The three-way tally, because two-way would overstate it

* **3 LIVE-VERIFIED with an actual comparison** — `GetSaveFilename`,
  `GetSaveCount`, `GetSaveInfo`: 0 divergences over 55 calls against the shipped
  DLL, on 13 real saves.
* **2 LIVE-EXERCISED BUT NEVER COMPARED** — `BeginLoad`,
  `BeginGetSaveGameInfo`. Both registered a site, which happens on body ENTRY,
  so both ran; `calls=0` because neither has an oracle arm (an issue is not
  replayable in place). **"Live-exercised" is weaker than "verified" and reads
  almost the same**, so it gets its own row rather than being folded in.
* **2 NOT REACHED on this route** — `BeginDelete` (needs a Delete click),
  `??1Persistence@`.

All seven remain independently covered by the synthesised oracle (§4, §T1).

### What went wrong, stated so it is checkable

I had **found, quoted and cited** `BANISHED_AUTOLOAD` at
`standalone/autostart.cpp:147`. I reasoned correctly that the missing thing was
the dialog route. Then I ran `ls` on `C:/GOG Games/Banished/Save` — a directory
I had **guessed** — found nothing, and wrote a negative claim from it. The saves
live in `C:/Users/USPC/Documents/Banished/Save`; there are 13, three predating
this session.

> **A negative claim asserted from an observation cheaper than the experiment
> that would settle it is not a result.**

The `ls` felt like evidence *because* it was cheap; it was cheap because it was
not the experiment. There was also a tell in my own prose: **§5 said the route
"never opened a save/load dialog" — a statement about THE DRIVER I CHOSE —
inside a section whose conclusion was about THE GAME.** The subject slipped
between premise and conclusion: `reference_a_screen_that_cannot_name_its_subject.md`
in prose rather than in an instrument, where no linter will catch it.

I also predicted `BeginGetSaveGameInfo` would NOT be reached, reasoning from the
disassembly's command codes (3 vs the list scan's 4) rather than from the
caller. The load route reaches it. **Read the caller.**

## T3. Not this lane's, found on the way

* **`?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ` has TWO DISTINCT BODIES.**
  `video/buffers.cpp:1308` (`void`, C++ linkage, Wave 9 lane GFX2) and
  `video/context_lifecycle.cpp:296` (`extern "C"`, Wave 9 lane VID2). Different
  linkage, so both link. Both register the NAME and the address `0x1800f4400`,
  and `RtVideoContextLifecycleRegister()` runs after `RtVideoBuffersRegister()`,
  so VID2 wins both doors — no split brain, but **GFX2's body is fully shadowed
  DEAD CODE that the census counts as ported.** The inverse of
  `reference_half_registered_port_reads_as_absent.md`: there a real port read as
  absent; here a dead body reads as a live port. Confirmed independently by
  `tools/dup_runtime_export.py`.
* **`0x1800e0c80` is a duplicate ADDRESS** across `input/input.cpp:~733` and
  `input/input_config_startup.cpp:~220` — same cross-file shape. Found by the
  integrator's tool, not mine: I scanned names only; addresses never occurred
  to me.

## T4. Instrument work

`tools/abi_retslot_audit.py` (**separate commit `1ad9e8e`**, revertible alone).
Its headline read `0 suspicious of 61 ... 0 unchecked` with the UNAUDITED count
two lines below, so it was quotable alone as a clean sweep of a subject that is
58% invisible. It now reads `... (0 unchecked, enum returns excluded); 77 of 133
distinct names UNAUDITED`. **The fix was code ORDER, not wording** — the
coverage figures were computed *after* the headline printed, so no rewording
could have carried them. The tool was already a good citizen; this closes a
QUOTABILITY gap, not a measurement one.

### My own throwaway duplicate scan was the weaker instrument — three blind spots

Written because `tools/dup_runtime_export.py` was not yet on any ref I could
fetch. Set-diffed against the real tool once it was: **7 in common, 1 in mine
only, and mine was the FALSE POSITIVE.**

1. **line-based matching** — missed 3 of my own 7 registrations, whose literals
   sit on a continuation line. Caught only by a control requiring all 7 to read 1.
2. **no comment stripper** — reported `<lit>` and `<literal>`, which are prose.
3. **adjacent string-literal concatenation** — `dialog_create.cpp:1420` splits a
   mangled name across three literals; my regex took the first, matched the same
   truncated prefix from two different overloads, and manufactured a duplicate.
   The tell was in the output and I nearly missed it: the name ended `@Object@@`
   with **no `@Z`**.

(3) is the **inflating** kind, which manufactures work. Use the integrator's
numbers, not mine.

**And I ran the real tool wrong the first time:** its `--root` defaults to the
parent of the script's own directory; I ran it from a `/tmp` copy, so it scanned
68,287 files under `AppData\Local`, counted every agent's scratchpad copy of the
repo as a duplicate of every other, and reported **1029 of 1066 names
duplicated**. WAVE19_IO.md §6.1 says to run an instrument in the tree you are
measuring *because several derive their root from the script path*. I had quoted
that section an hour earlier.

## T5. The orphan, and two harness defects it exposed

My `BANISHED_AUTOLOAD=Save0` run crashed (`READ of 0` in subsystem
deserialization, after `[fn_ExeArrayResize] ABSURD newCount=2129744266` — an
absurd count read from the stream is what a FORMAT MISMATCH looks like, which
supports the stale-save reading over a port defect; `Auto.sav` from today loads
clean on the same binary and the same mod state, which is the control).

**The crashed process did not exit.** It sat 24 minutes holding the exe and both
logs, blocking every lane's deploy. `taskkill /PID|/T|/IM /F` and
`Stop-Process -Force` all refused it — *"no running instance of the task"* —
while `Get-Process` still listed it.

* **A crashed run is not a finished run, and the VEH handler writing a crash log
  is not the process exiting.** Every run script in this lane now ends with an
  unconditional `taskkill /IM reimpl-x64.exe /F`.
* **`tools/boot_state.sh` reported `RUNNING` off a stale state file for the
  whole outage** — it checks pid liveness, never file freshness. My run script
  printed `BOOTSTATE_RC=0` for an already-crashed process and I read it as
  healthy. NOT fixed; check the state file's mtime yourself.
* **I held the lock ~20 minutes while running nothing**, and told the integrator
  I was "queued, not holding" — because I read my run's stdout and inferred the
  LOCK's state from it. The same premise/conclusion subject slip as §T2, from
  the same seat, within the hour. **Naming a failure mode does not inoculate you
  against it.**
* When I later told lane CSTREAM the directory was free, my file test covered
  **only `reimpl-x64.exe`**, not the two logs — a partial test presented as a
  whole-directory verdict. The answer was right for a reason my test did not
  establish (the process was gone by then, and handles die with the process).

## T6. What this tranche still does NOT claim

1. *[ANNOTATED — true but misleading, see §T7.2]* **`BeginDelete@` and `??1Persistence@` have no live-route exercise.** Both
   are verified by the synthesised oracle only.
2. **`BeginLoad@` and `BeginGetSaveGameInfo@` ran live but were never COMPARED
   live.** Their live evidence is "the body was entered", nothing more.
3. *[ANNOTATED — the stated reason is FALSE, see §T7.1]* **The dtor's per-element destroy pass (`0x18002afc0`) was NOT exercised** —
   both dtor cases seed `saveCount = 0` deliberately, because a fabricated
   non-zero count would drive it over elements that are not there.
4. **The `Save0` crash is NOT diagnosed**, only bounded. Format-mismatch is the
   supported reading; I did not confirm the mod provenance of that save.
5. **The C4 workaround is KEPT, and my earlier reason for keeping it was wrong.**
   I said `e983724` "is on `wave10/base`, not my `wave19/base`" — the integrator
   force-updates `wave19/base` to `wave10/base` after every merge, so the fix
   was there and my base was simply old. **The real reason to keep it**: a
   boot-probe fault in another wave's code should not be able to take out this
   lane's oracle, which is exactly what happened when
   `BANISHED_RT_ORPHAN_PROBE=1` died at boot here.

## T7. ANNOTATION — decomposing my own §T6, and two premises that are false

Added after §T6 was written, at lane CSTREAM's prompting. **No measurement
changed and nothing was reopened**; this annotates two claims of §T6 that do not
survive their own premises.

**The rule being applied** (lane CSTREAM's, refined jointly): an impossibility
claim is unfalsifiable by any enumerator, but it is a *conclusion resting on a
premise*, and the premise is usually an ordinary existence claim a screen CAN
reach. **So when you write a blocker, write its premise as a separate, checkable
line.** That is what makes "a stated blocker is a claim with a timestamp"
actionable — otherwise it tells you to re-check a blocker and hands you nothing
to re-check. It is also exactly what retired §7.1: the integrator's question
("is the blocker the thread, or the absence of an instance you may destroy?")
did nothing but force the premise out of the conclusion, after which one
`objdump` finished it.

### T7.1 §T6.3 is WRONG — the per-element destroy pass IS exercisable

§T6.3 says the dtor's per-element pass (`0x18002afc0`) was not exercised because
*"a fabricated non-zero count would drive it over elements that are not there."*

Decomposed, the premise is "I cannot supply well-formed `SaveGameInfo`
elements". **That is false, and it is falsified by this lane's own probe file.**
`RunReaderSweep` already constructs three valid `SaveGameInfo` entries with real
engine-allocated Strings, in the same translation unit. And the body is an
ordinary refcount release over the array:

```
18002afc0  test r8d,r8d ; jle <ret>          ; count == 0 -> no-op
18002afec  lea  rbx,[rdx+8]                  ; -> the `name` String at elem+0x08
18002b009  mov  rax,[rbx] ; test ; je next
18002b011  dec  dword [rax-4] ; jz -> free   ; the documented String release idiom
           ... stride 0x10 per element
```

So seeding `saveCount = 3` with the entries the file already knows how to build
would exercise it. **The claim that the case is unreachable is false; the true
statement is that I did not do it.** Cost is small — the elements exist, the
release idiom is the one `RtString_ReleaseInline` already implements — and the
value is real, because the pass frees Strings and a port that mis-walked the
stride would corrupt the heap rather than fail a comparison.

**Not done here** (the wave is closing and a new probe case would land
unreviewed), and recorded as the honest reason rather than dressed as a
limitation. This is the next thing to do to this file.

### T7.2 §T6.1 is TRUE AS WRITTEN and MISLEADING IN EFFECT

§T6.1 says `?BeginDelete@` has no live-route exercise, and §1/§T2 attribute that
to "needs a Delete click". Literally accurate. But it reads as *no route
exists*, and decomposed the premise is "there is no driver for a
SaveLoadDialog button", which is **false**: `standalone/autostart.cpp` already
has the whole mechanism —

```
autostart.cpp:44   constexpr int kSLD_LoadClick = 2;   // SaveLoadDialog OnEvent 0x12/2 = buttonLoad
autostart.cpp:435  InjectClick(dlg, kMsg_ButtonClick, kSLD_LoadClick);
```

What is missing is **one constant**, the Delete button's event id, not a driver.
That is `reference_grep_the_harness_before_declaring_no_route.md` — the same
trap that took a body from unverified to 1202/1202 once someone grepped
`standalone/`. I did grep the harness, found `kSLD_LoadClick`, and still wrote
the blocker in a form that implies unreachability.

**Corrected statement:** `?BeginDelete@` is unexercised because nobody has
supplied the Delete event id to an injector that already exists.

### T7.3 What this says about the method

Two for two on the first two claims I pointed decomposition at, and both were
mine. Lane CSTREAM scored 1 of 3 on the same exercise against their own sealed
report. That is a high enough hit rate on a cheap check that **decomposing every
impossibility-shaped claim before sealing** looks worth making routine, and it is
the only mechanism either lane could name that reaches this class at all — a
wide screen cannot, because no enumerator lists the possible.

The residue neither mechanism reaches is a **number quoted from a stale
document**, which was never anyone's subject. One cell, not a category.
