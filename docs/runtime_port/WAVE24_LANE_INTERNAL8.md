# WAVE 24 LANE INTERNAL8 — the 8 real bodies inside the 45 address-keyed unknowns

Branch `wave24/internal8`, cut from `wave24/base` at `f5d70de2`. Private worktree.

> **EVERY FIGURE IN THIS DOCUMENT IS BRANCH-LOCAL**, measured in this lane's
> isolated worktree with the command printed beside it. Wave 21 published a
> wave-level number assembled from seven such figures and it was wrong. The
> integrator must re-derive on the merged tip.

---

## 0. HEADLINE

**Seven of the eight bodies are ported, registered and building clean. The
eighth was ported too — and the census still prints it as a gap row, for a
reason that is a KNOWN CENSUS BLIND SPOT and not a missing port.**

| # | row | where it lives now |
|---|---|---|
| 1 | `0x1800c3db0` qsort comparator | `runtime/src/save/saveload_saveinfo.cpp` |
| 2 | `0x1800c3de0` `SaveInfo::SetThumbnail` | `runtime/src/save/saveload_saveinfo.cpp` |
| 3 | `0x1800b7390` worker enqueue, DELETE | `runtime/src/io/persistence_worker_win.cpp` |
| 4 | `0x1800b7460` worker enqueue, LOAD | `runtime/src/io/persistence_worker_win.cpp` |
| 5 | `0x1800b75c0` worker enqueue, SAVE | `runtime/src/io/persistence_worker_win.cpp` |
| 6 | `0x1800b7740` worker destructor | `runtime/src/io/persistence_worker_win.cpp` |
| 7 | `0x1800c04c0` `Factory::GetTypeIdForFileExtension` (ordinal 2990) | `runtime/src/object/factory_file_extension.cpp` |
| 8 | `0x180028760` `Array<Mod>` introsort, 1381 B / 422 insns | **NOT PORTED.** §6 |

So: **7 bodies written, 6 census rows promoted**, one ported body the census
cannot see, and one body left standing with its size measured and its reason
stated. The eighth row in the original "8 bodies" count is `0x180028760`; it is
the single largest body in the bucket and this lane did not reach it.

---

## 1. WHAT I MEASURED, AND THE COMMANDS THAT RE-DERIVE IT

```
tools/runtime_census.py --unported          # run from THIS worktree root
```

| | at lane start (`f5d70de2`) | at lane end |
|---|---:|---:|
| called | 1319 | **1321** |
| ported | 1159 | **1165** |
| ported % | 87.9% | **88.2%** |
| gap rows | 160 | **156** |
| `internal` gap | 45 | **41** |
| `ordinal` gap | 4 | 4 |
| "a function to reimplement" | 22 of 160 | 22 of 156 |

**Read the `called` column, not only `ported`.** `called` went UP by 2. That is
not noise and it is not someone else's edit — **this lane added two address
doors that did not exist in the tree before** (§4). A lane that reported only
"ported +6" would have hidden its own cost. The gap fell by 4, not 6, for
exactly that reason.

**The 45 were still the 45 at lane start, minus one, plus four.** Re-derived
rather than inherited:

```
tools/runtime_census.py --csv w24.csv
awk -F, '$3=="internal" && $4=="0"{print $2}' w24.csv | sort
```

vs WAVE23_LANE_INTERNAL.md's 42:

* `0x180009c30` (AVL `SetNodeFree`) **left** the gap — ported since Wave 23.
* Four **entered**, all of them Wave 23's Achievement lane opening new address
  doors: `0x180004590` (`ExternalPtr<UI::SpriteSheet>::Serialize`),
  `0x180005110` (`Array<ExternalPtr<Goal>>::SetSize`), `0x180005220` and
  `0x180009a20` (element-serialize walkers). All four are named in
  `runtime/src/achievement/achievement_serialize.cpp`'s own comments — the same
  "printed UNKNOWN, already named in our source" shape Wave 23 described, one
  wave later and from a different lane. **The mechanism reproduces; it is not a
  one-off.**
* None of the four is one of the 8 bodies: three are `Collection` spec entries
  and one is an already-existing serialize door.

**All seven B4 bodies and ordinal 2990 were still present and still unported at
lane start.** Verified by set membership on the list above, not by reading the
Wave 23 table.

---

## 2. MY PROBE SHAPE (kickoff §1c), NAMED BEFORE THE WORK

I am building **three different shapes, because the eight rows are not one
population**, and saying which applies to which is the point of the section:

| shape | rows | why |
|---|---|---|
| **INPUT SWEEP** against the shipped export | `0x1800c3db0` | the only PURE row: two const pointers in, an int out, no state. |
| **REAL two-sided oracle** (`RT_DIFF_SCALAR` vs `RT_ORIGINAL_ONCE`) | `0x1800c04c0` | `QEBA`, const, side-effect-free: it walks a tree it never writes. The shipped body can be run on the same inputs. |
| **SHADOW site** for lane DIFF's armed sweep | the other five | every one mutates live state; running the shipped body "to compare" would enqueue a second request, upload a second time, or join an already-joined thread. |

**Why the other shapes do not apply**, since Wave 23 §7 item 7 says ten lanes
each concluded "no run, therefore STRUCTURAL":

* **write-path replay** — there is no `Serialize` anywhere in these eight.
* **synthesised oracle** — buildable for the three enqueue bodies (their whole
  issued state is a String, some ints and two event handles) but **not** for
  the worker destructor, which joins a real thread. An oracle covering three of
  four rows while the fourth is the risky one is the shape this project keeps
  mistaking for coverage, so I did not build the partial one and call it done.
* **two-sided disassembly check** is the fallback and I did not need it as the
  primary for any row.

### 2.0 LABELLING — every site declares its own class (lane DIFF's correction)

Mid-lane, lane DIFF's armed sweep (99.7M calls, 630 sites) measured that **at
least 286 of 630 existing sites (45%) cannot catch a shared misunderstanding of
retail** — 115 SELF-CHECK, 38 ATTENDANCE, and **133 structurally incapable of
printing anything but `divergences=0`** — and that **none of them said so**. A
site that cannot fail reports GREEN, which is worse than no site.

Every site this lane adds therefore **declares its class in the string the sweep
prints**, and names the route its "theirs" side takes:

| site | class, as printed | route to the shipped body |
|---|---|---|
| `IO::Persistence::worker::EnqueueDelete @0x1800b7390` | `[SELF-CHECK]` | **none** — and the THEIRS description says so in words |
| `…::EnqueueLoad @0x1800b7460` | `[SELF-CHECK]` | none |
| `…::EnqueueSave @0x1800b75c0` | `[SELF-CHECK]` | none |
| `…::~worker @0x1800b7740` | `[SELF-CHECK]` | none |
| `Save::SaveInfo::SetThumbnail @0x1800c3de0` | `[SELF-CHECK]` | none |
| `Object::Factory::GetTypeIdForFileExtension` | `[ORACLE:name-door]` | **mangled NAME via `RT_ORIGINAL_ONCE` → `RtOriginal` → `GetProcAddress`** |
| the comparator sweep | input sweep, own reporter | **`ResolveRuntimeRaw(0x1800c3db0)`** |

**Neither of the two real instruments reaches the shipped body through
`ResolveAddr`.** For an address-keyed row that is the whole question:
`ResolveAddr` consults the port registry FIRST, and both of those addresses are
registered by this lane, so `ResolveAddr` would compare the port with itself.
That is precisely the 115-site shape DIFF counted, and a probe with it was dark
for five waves.

I did **not** copy the `IO::MemoryStream::MemoryStream` template (a whole-buffer
`memcmp` across a vptr at +0x00 that MUST differ — permanently red, checks
nothing), and I used **no** `RT_DIFF_REPLAY_PRE/POST` (zero adopters; the
documented invert mode is inert).

### 2.1 The sweep, and its two controls

`RtSaveInfoComparatorSweep()` (`saveload_saveinfo.cpp`), armed by
`BANISHED_RT_SAVEINFO_SWEEP`. 2,500 ordered pairs from 10 sort keys x 5 save
indices on each side.

* **THEIRS is `ResolveRuntimeRaw(0x1800c3db0)`, NOT `ResolveAddr`.** This file
  *registers* that address, and `ResolveAddr` consults the port registry FIRST
  — using it would compare the port with itself and pass unconditionally. That
  exact defect kept a Wave 5 probe dark for five waves.
* **EFFECTIVENESS counter** — `decisive` counts the pairs that reached the
  sortKey comparison at all. Every pair with a negative `saveIndex` returns 1
  from the first two instructions, so a sweep made only of those would be 100%
  green while testing two `CMP`s. `decisive == 0` prints "this run proves
  NOTHING" in those words.
* **NEGATIVE control** — a *signed*-compare mutant of the same body, pushed
  through the **same** comparison path. The mutation changes the value the probe
  actually compares (the return code), which is the requirement a
  corrupted-but-uncompared control fails. And the report carries
  `differed-from-retail=N` as **one** number, not two: "the probe caught it" and
  "the mutant disagreed with retail" are the *same test by construction* here,
  and printing them as two figures would be a union control dressed as
  corroboration. If that number is 0 the control is reported **INCONCLUSIVE**,
  not passing.
* The key set deliberately straddles 2^63 (`0x7FFF...FE`, `0x7FFF...FF`,
  `0x8000...00`, `0x8000...01`) because that is the **only** region where signed
  and unsigned disagree — i.e. the negative control is aimed at the mutation,
  not sprinkled.

**I DID NOT RUN IT.** This lane has no game-directory lock, by design. The sweep
is a deliverable, not a result.

---

## 3. EXTENTS — how each body was bounded, and why not from `.pdata`

| row | extent | bounded by |
|---|---:|---|
| `0x1800c3db0` | 34 B (`…3db0`–`…3dd1`, then `CC`) | **raw image bytes** — no `.pdata`, no Ghidra function object |
| `0x1800c3de0` | 332 B (tail `JMP [LeaveCriticalSection]`) | Ghidra function body |
| `0x1800b7390` | 188 B (tail `JMP [SetEvent]`) | Ghidra function body |
| `0x1800b7460` | 343 B (tail `JMP [SetEvent]`) | Ghidra function body |
| `0x1800b75c0` | 364 B (tail `JMP [SetEvent]`) | Ghidra function body |
| `0x1800b7740` | 183 B (tail `JMP [CloseHandle]`) | Ghidra function body |
| `0x1800c04c0` | 181 B, two `RET` sites | Ghidra function body |
| `0x180028760` | 1381 B / 422 insns | Ghidra function body (NOT ported) |

**No extent in this table came from `.pdata`, and one of them could not have.**
`.pdata` understates: MSVC chains fragments through `UNW_FLAG_CHAININFO` and it
is the **ENTRY** fragment that reads `chained=False`, so a chained function
looks *complete* when you look it up by its own address — measured at 20.4x on a
row in this same bucket. Understating makes work look cheap and truncates a port
silently. And 52.9% of this DLL's `.text` exports have no `.pdata` at all, which
is exactly the case for the comparator: a 34-byte leaf that Ghidra has no
function object for either. Its extent is the instruction bytes up to the `C3`
followed by `CC` padding.

**The RTTI screen was not used anywhere in this lane.** `Runtime-x64.dll` is
`/GR-`; a screen with no positive case in the binary can only return 0, and a 0
from a dead screen reads exactly like a real negative. The one vtable question
that came up (`0x180155860`, stamped by the worker destructor) is answered by
the *stamp itself* — the body writes that address into `*this` — not by a
structural test.

---

## 4. WHAT THIS LANE COST — the cutover surface grew by 2

Porting a body faithfully makes its callees **ours to satisfy at cutover**.
Two addresses now have a literal door in the tree that did not before:

| address | what it is | status |
|---|---|---|
| `0x18002a9d0` | `Array<void*>::SetSize(n)` — the blockPtr array | **NEW row**, unported |
| `0x18002a810` | `Array<{id,offset,size}>::SetSize(n)`, stride 0xc | **NEW row**, unported |

and two more that were already rows and are still unported are now reached from
one more place: `0x18002a930` and `0x18002ac40` (both `~Array` clear-on-null).

All four are `Collection::Array<T>` instantiations against the parameterised
cores this tree already owns (`SetSizeCore` / `ArrayDtorImpl`), i.e.
**ArraySpec/DestroySpec table entries, not function bodies**.
**ADDRESSEE: the Wave 25 lane that takes WAVE23_LANE_INTERNAL.md residual #6.**
That residual currently says 14 spec entries; **it is 16 now**, and the two
extra are this lane's debt.

### 4.1 The instrument nearly agreed with what was convenient

The first draft routed all five `Array` doors through one helper taking a
`uintptr_t` parameter. `runtime_census.py` finds address doors by grepping for
the **literal** `ResolveAddr(Module::Runtime, 0x…)`, so it reported `called`
**unchanged at 1319** — i.e. "this lane added no cutover surface", which is what
I would have liked to publish. Rewriting the five doors to spell their literal
inline moved `called` to 1321 and made the cost visible.

**The number that was wrong was the flattering one, and nothing in the build or
the census flagged it.** This is the same blind spot the project has recorded
four times (`reference_literal_grep_blind_to_variables.md`); it is recorded a
fifth time here because it fired on a *new* file written by someone who had read
the warning.

---

## 5. ORDINAL 2990 — ported, registered, and STILL a gap row

`?GetTypeIdForFileExtension@Factory@Object@@QEBAIPEBD@Z` is ported, registered
**by name and by address**, and the census still prints `ordinal 2990` unported.
That is the census, not the port, and it is verified rather than asserted:

* `standalone/resolve_standalone.cpp:900` — `ResolveOrdinal` misses
  `RtOrdinalReg()`, then **falls through to `MangledForOrdinal(ordinal)` and
  looks the NAME up in `RtExportReg()`**. `kOrdinalNames` carries a row for
  2990. So the two exe call sites (`buildcomponent.cpp`,
  `residencecomponent.cpp`) reach this lane's body today.
* **A positive control for the claim, in-population:** ordinals `3214`
  (`Video::Device::Lock`) and `4462` (`::Unlock`) are *also* in that table,
  *also* ported by name since Wave 9, and *also* still printed as `ordinal` gap
  rows. Three rows with the same shape; mine is the fourth. If my reading were
  wrong, those three would have cleared.

This is **WAVE23_LANE_INTERNAL.md residual #1**, addressed to the Wave 24
integrator: fold the ordinal→export-name lookup into `runtime_census.py`. It
removes 3 phantoms and refiles 1 into `Object`. I did not do it: `runtime_census.py`
is a shared instrument, and editing it mid-wave moves numbers other lanes are
quoting. **The evidence that it is the right fix is now four rows, not one.**

---

## 6. WHAT I DID NOT ESTABLISH — plainly

1. **`0x180028760` (`Array<Mod>` introsort, 1381 B / 422 insns) IS NOT PORTED.**
   It is the largest body in the bucket by a factor of four and I ran out of
   lane before it. Wave 23 already checked and *rejected* the tempting shortcut:
   `runtime/src/collection/introsort.cpp` transcribes the packfile introsort
   `0x1800264a0` (165 insns / 523 B) and **does not** cover this one. I did not
   re-derive that negative; I am carrying it forward as a stated blocker, which
   means **the next lane must re-check it** — that is the rule, including for
   blockers that are one wave old.
2. **I ran no game.** No lock, by design. No `calls=` column, no attendance, no
   divergence count. **Nothing in this document claims any of these seven bodies
   has ever executed.** Six of the seven have never had their shadow predicate
   evaluated even once.
3. **The five shadow sites are SELF-CHECKS, not oracles** — and they now say
   so in their own site names (§2.0), so the next sweep classifies them
   honestly instead of counting them as coverage. They re-read the
   object after the body has run and assert the fields the shipped listing says
   must hold. That catches a wrong offset, a wrong constant and a dropped store.
   It does **not** catch a shared misunderstanding of retail: a `divergences=0`
   on these five means "the stores landed where the file says", full stop.
4. **`SetThumbnail`'s `Create` arguments are verified by TRANSCRIPTION ONLY.**
   All eleven are immediates in the shipped body and the shadow site cannot see
   them. I say so in the file rather than letting a green site imply more.
5. **I did not build the cross-build opcode-hash matcher** (Wave 23 residual #3,
   which would name all 30 unexported internals at once from
   `Runtime-x64-profile.dll`). Not needed for these eight, still open.
6. **The non-literal census blind spot is still unmeasured in general.** §4.1 is
   one instance found by accident inside my own file, not a survey.
7. **I did not verify that `0x1800b7740`'s thread join terminates in our
   standalone.** I re-checked the *port* blocker (§7) and found it was a
   verification blocker, not a port blocker. That is not the same as having
   watched a shutdown complete.

---

## 7. A STALE BLOCKER, RE-CHECKED AND DISCHARGED

Wave 19 recorded `0x1800b7740` as held back because it "joins and closes a real
thread" (`persistence_saves_probe.cpp:499`). Re-checked on this tip:

* the claim about the **body** is TRUE — `0x1800b77c3` is
  `WaitForSingleObject(handle, INFINITE)` on the worker thread;
* and it was never a **port** blocker. Its only caller is
  `??1Persistence@IO@@QEAA@XZ`, which Wave 19 **already ported** and which
  **already reaches this body by address**. Leaving it unported did not avoid
  the join; it only meant the join ran in the DLL's copy.
* The join is bounded: `SetEvent(readyEvent)` at `0x1800b77b6` signals the
  worker to exit **one instruction earlier**.

So the blocker was real about the **oracle** (which is why this row is a shadow
site and not a replay) and wrong about the **port**. That is six-plus instances
in three waves of a discharge that did not propagate.

---

## 8. THINGS FOUND IN THE BODIES THAT A TIDIER PORT WOULD HAVE LOST

Each of these is transcribed in the source with the instruction address:

1. **The comparator is NOT a total order.** A row with `saveIndex < 0` compares
   GREATER in *both* positions — `cmp(blank,x)==1` **and** `cmp(x,blank)==1`.
   That is a genuine violation of the strict weak ordering `qsort` requires, and
   it is retail's behaviour; `UpdateList` survives it only because state 1 sorts
   the sub-range that excludes the one such row. Reproduced, violation included.
2. **The comparator's key compare is UNSIGNED** (`JA` + `SBB`). The key is a
   FILETIME-shaped `uint64` whose top bit is live; a signed compare reverses
   every pair straddling 2^63. That is precisely what the sweep's negative
   control mutates.
3. **`0x1800b75c0` writes NO command word.** `MOV qword [RSI+0x38],RDI` is one
   8-byte zero store covering `status` **and** `command`, and nothing afterwards
   sets a command — the caller does. Reading it as "status = 0" and adding a
   `command = 4` would have invented a store that is not in the bytes.
4. **`SetThumbnail` faults on allocation failure in the shipped binary.** The
   one `if` it has guards only the *constructor*; it then stores null at
   `row+0x18` and dereferences it one instruction later. Transcribed as written.
5. **An MSVC ctor returns `this` in RAX; our port of that ctor returns `void`.**
   Retail's `RDI = RAX` after the `ImageBuffer` ctor call is a no-op for retail
   and would be **reading a register the callee does not set** for us. The port
   carries the allocation pointer in a local instead. This would have worked
   perfectly while the DLL body was in use and broken at cutover.
6. **`0x1800c04c0`'s ascent exits with `node = node->parent`, not `node = p`.**
   The loop advances both; the two coincide only on one of the two exit paths.
7. **String refcount assignment mutates the field to `block` before the free**
   (`ADD qword [RDI+0x30],-0x8`, then reload, then free) and the non-freeing arm
   writes NULL into a field the next instruction overwrites. Both reproduced;
   the allocator watchdog can see the first.
8. **The worker destructor does not null its four handle fields.** The shadow
   predicate asserts what retail *leaves*, not what would be tidy — asserting
   `== nullptr` there would assert a store retail does not make.

---

## 9. HOW TO A/B THIS LANE

Three independent kill switches, each chosen so a symptom maps to one file:

| switch | turns off |
|---|---|
| `BANISHED_RT_PERSWORKER=0` | the four worker bodies (the Begin* callers fall back into the DLL through `ResolveAddr`) |
| `BANISHED_RT_SAVEINFO=0` | the comparator + `SetThumbnail` |
| `BANISHED_RT_FILEEXT=0` | `Factory::GetTypeIdForFileExtension` |

`BANISHED_RT_SAVEINFO_SWEEP=1` runs the comparator input sweep (stderr).
`BANISHED_RT_DIFF=1` arms the six shadow sites and the one real oracle.

Build: `mingw32-make standalone-debug` — **clean**, no new warnings from these
three files.

---

## 10. RESIDUALS, EACH WITH A NAMED ADDRESSEE

| # | item | addressee |
|---|---|---|
| 1 | **`0x180028760`** (`Array<Mod>` introsort, 1381 B) — the one unported body of the eight. Its "not covered by `introsort.cpp`" blocker is one wave old and **must be re-checked, not inherited**. | **Wave 25, a named lane** |
| 2 | Fold `ordinal → export name` into `runtime_census.py`. §5 now has **four** rows of evidence, not one. | **Wave 24 integrator** (Wave 23 residual #1) |
| 3 | The ArraySpec/DestroySpec residual is **16 entries, not 14** — `0x18002a9d0` and `0x18002a810` were added by this lane. | **Wave 25 lane taking Wave 23 residual #6** |
| 4 | **Exercise the six shadow sites and the one oracle.** They have never been evaluated. | **lane DIFF**, this wave |
| 5 | **Run `BANISHED_RT_SAVEINFO_SWEEP=1` once.** It is self-contained, needs no game state, and it is the only genuine input-sweep oracle this lane produced. | **lane DIFF**, this wave |
| 6 | The four rows that ENTERED the `internal` bucket this wave (`0x180004590`, `0x180005110`, `0x180005220`, `0x180009a20`) are all named in `achievement_serialize.cpp`'s comments and are all `Collection` spec work or an existing door. **The bucket refills every wave**; a census that read the classifier columns would stop this recurring. | **Wave 24 integrator** (Wave 23 residual #2) |
