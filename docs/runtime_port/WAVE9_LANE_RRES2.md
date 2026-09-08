# Wave 9 lane RRES, rung 2 — `ExternalFactory::Find`

Branch `wave9/lane-rres2`, cut from `e447a7c` (the integrator's merge of rung 1).
Worktree `.claude/worktrees/rres2-089e6eb4`.

**Assignment:** `?Find@ExternalFactory@Object@@AEAAPEAVExternal@2@AEBVFilename@System@@@Z`
— retail `0x1800be0e0`, ordinal 2336, 165 instructions. Rung 1's write-up named
it "the cheapest next rung by a wide margin"; this is that rung.

**Status:** ported, registered, built (`rc=0`), source-side door set-diff
measured against a pre-registered prediction. **The live cell has NOT been run**
— see §5, which is written as an empty section on purpose rather than omitted.

---

## 1. The prediction, VERBATIM, written before any instrument was run

Rung 1 predicted its door set-diff *after* the fact and got it wrong in the
optimistic direction; the set-diff caught it. The integrator asked that the
prediction be recorded as a prediction, in the document, with an explicit
stopping condition, so that a move cannot be rationalised afterwards. This is
the file as it was written, unedited:

```
LANE RRES FOLLOW-ON RUNG (ExternalFactory::Find, 0x1800be0e0)
DOOR SET-DIFF PREDICTION, written BEFORE the instruments were run.

Base for the diff: e447a7c (the integrator's merge of lane RRES).
Tree under test: wave9/lane-rres2 working tree, built rc=0.

The previous rung got this wrong in the optimistic direction and the set-diff
caught it. The lesson was "a port cannot remove a name from a set the name was
never in", so this time the prediction is derived from THAT rule rather than
from the number of bodies ported.

1. unsatisfied_export_scan
     names with neither port nor mint      117 -> 117   (no change)
     of those, outside a diff guard        114 -> 114   (no change)
     SET-DIFF                              CLOSED 0, OPENED 0
   WHY: `?Find@ExternalFactory@Object@@AEAA...` is not resolved through a door
   anywhere in our source today -- nothing of ours calls it -- so porting it
   cannot remove it from a list it is not on. And this file opens no new name
   door: the only reach into the shipped body is the detour trampoline, and the
   installer resolves its target RAW inside standalone/runtime_detour.cpp,
   which is not a new site in my code.
     The one number that MAY move is the "distinct names resolved through a
   door" total (855), if the scanner counts my RegisterRuntimeExport literal.
   It did move by +1 on the previous rung for exactly that reason.

2. addr_door_audit
     by-address call sites                 unchanged
     distinct addresses                    unchanged  (no new address door)
     registered addresses                  +1         (0x1800be0e0)
     UNREGISTERED                          unchanged

3. port_bypass_audit                       unchanged in every bucket
   WHY: no new ResolveAddr(Module::Runtime, ...) and no new ResolveRuntimeRaw
   site. The previous rung's +2 UNPORTED came from the self-test's PushPath /
   PopPath raw doors; this rung has no self-test of that shape.

4. rawdoor_audit                           sites unchanged, UNGUARDED-CALL 0

5. detour_target_audit                     clean

6. runtime_census.py, Object row           191 / 50 / 30, unchanged
   WHY: the `ported` column counts against the `called` column, and Find is not
   in the called set for the same reason it is not on list 1.

STANDING INSTRUCTION TO MYSELF: if any of 1-6 moves other than as predicted,
STOP and find out why before writing a word of the report. A surprise in a
prediction this mechanical means the model of the instrument is wrong, and the
instrument is what the cutover gate is read from.
```

### Outcome: all six lines held

Both halves are one within-tree comparison — my four files present versus a
clean detached worktree at `e447a7c` — same instrument version, minutes apart.

| instrument | without | with | predicted | held? |
|---|---:|---:|---|---|
| `unsatisfied_export_scan` neither port nor mint | 118 | 118 | no change | yes |
| ... outside a diff guard | 115 | 115 | no change | yes |
| **SET-DIFF of that name set** | — | — | CLOSED 0 / OPENED 0 | **yes** |
| `unsatisfied_export_scan` names resolved | 862 | 862 | "may move +1" | held (it did not) |
| `addr_door_audit` by-address call sites | 303 | 303 | unchanged | yes |
| `addr_door_audit` distinct addresses | 212 | 212 | unchanged | yes |
| `addr_door_audit` registered addresses | 1420 | 1421 | **+1** | yes |
| `addr_door_audit` UNREGISTERED | 14 | 14 | unchanged | yes |
| `port_bypass_audit` every bucket | — | — | unchanged | yes |
| `rawdoor_audit` sites / UNGUARDED-CALL | 669 / 0 | 669 / 0 | unchanged / 0 | yes |
| `detour_target_audit` | clean | clean | clean | yes |
| `runtime_census` Object row | 191/52/30 | 191/52/30 | unchanged | yes |

The predicted absolutes for lines 1 and 6 were stale (117/114 and 50) because
other lanes merged between rung 1 and this measurement. **The predicted DELTAS
were right, which is the only part a set-diff asserts** — and it is why the
prediction was written as deltas.

### The one movement I did NOT predict

`runtime_census`'s **"Ports with no caller in our tree"** list moved 95 exports
/ 205 addresses → 96 / 206, naming this export and this address. That is the
tool correctly recording the port; the surprise was that I had not known the
section existed. Per the standing instruction I stopped and read the tool before
writing anything, and §4 is what that produced.

---

## 2. What the body is

```
External* ExternalFactory::Find(const Filename& path)     // private, AEAA
```

`AEAA` is PRIVATE, not unexported — this DLL exports its private members, and
the name above is ordinal 2336 in the retail export directory, verified with
`pefile` before the body was written rather than inferred from the mangling
(`reference_aeaa_is_private_not_unexported.md`).

Structure: normalise a copy of `path`; take its identifier from the RAW
argument; if the identifier is empty append `":resource"`; make a second
normalised copy; descend `this->_resources` through lane RES's `Set::Find`
(`0x18002dd70`); return `node->element.external` (+0x08) or null.

**Nothing new is opened.** `MemFree`, `AdjustDelimiters`, `GetPathIdentifer` and
`String::operator+(const char*)` were already ours; `0x18002dd70` is lane RES's.
That is why this rung was chosen.

### It finishes explaining a trap lane RES paid a run to find

Lane RES recorded that `Set::Find` **requires a delimiter-adjusted key** — a raw
`/` path silently misses, identically in the shipped body — and that "retail
calls `AdjustDelimiters` FOUR times before it descends". This is that function,
and the four are now individually accounted for. They are **two pairs**, each
one `Filename x = y` whose `operator=` already ends in `AdjustDelimiters`,
followed by one redundant explicit re-normalisation:

```
0x1800be140 / 0x1800be145   on `key`    after `key = path`
0x1800be244 / 0x1800be24d   on `probe`  after `probe = key`
```

So the "four calls" are not four normalisations of four strings. Three of the
four are no-ops on an idempotent function. All four are transcribed anyway.

### Three redundancies kept rather than tidied

* **`probe`** is a second `Filename` copied from an already-normalised `key`
  purely to be handed to the descent.
* **the identifier is taken from the RAW argument**, not from the normalised
  copy. It cannot matter — `AdjustDelimiters` does not touch `':'` — and it is
  written the way retail writes it, because "it cannot matter" is how a port
  acquires a difference nobody can find later.
* **`":resource"` at `0x180155ae0` is not the `"resource"` at `0x180155af0`**
  that `ExpandRelativePath` uses for the same defaulting decision. Two strings
  for one concept, four bytes apart; using the wrong one produces a key that
  matches nothing the factory ever inserted. Read out of `.rdata`, not guessed.

### Retail uses its caller's shadow space as scratch

`MOV RBP,RSP` sits after four pushes, so the three `Filename` locals at
`[RBP+0x28/0x30/0x38]` are the **caller's** 32-byte home space. Legal in the MS
x64 ABI. It matters once: when the diff replays the original through the
trampoline, the original scribbles on this function's own home space, which the
compiler has already reserved for the call. Noted because a reader checking
offsets against the listing will otherwise read `[RBP+0x28]` as an argument.

---

## 3. The oracle, and controls chosen by asking what makes them fire

`Find` is **read-only on the tree**: it allocates and releases only its own three
`Filename`s and returns a pointer it did not create. So the original replays on
the same arguments and the comparison is **pointer equality** — not text, not
bytes. Two independent descents of the same live AVL tree must arrive at the
same node or the port is wrong. That is a stronger predicate than rung 1 had.

`theirs` is the **detour trampoline**, never `RtOriginal`: with the detour
installed, `GetProcAddress` returns the patched address and an "original" call
would re-enter our own body forever
(`reference_rtoriginal_is_getprocaddress.md`). With no detour there is no route
to the original at all, and the site makes **no comparison** rather than
comparing ours to ours.

Rung 1 shipped two negative controls that could not fire. These three were each
chosen by asking first what makes them fire:

| control | perturbation | fires on |
|---|---|---|
| `1` | append `":resources"` instead of `":resource"` | every `identDefaulted` call that would have hit |
| `2` | descend on the UNSUFFIXED key | the same population, other direction |
| `3` | return the NODE instead of `node->external` | **every hit** — cannot fail to fire if anything is ever found |

---

## 4. A census defect, with its measured cost and error direction

Found while investigating the unpredicted movement in §1, and reported to the
integrator rather than fixed — `tools/` is not this lane's file to edit
mid-lane.

**`runtime_census.py`'s `detour` column can never show a detour on the one class
of body a detour exists for.** `merged` is populated only from `called_exports`
and `called_addrs`, so a port whose sole caller lives inside the DLL never
enters `rows`, and therefore cannot appear in `called`, `ported` **or `detour`**
for its namespace. The tool *does* detect the detour — `RE_DETOUR_ADDR` matches
both `InstallRuntimeDetour` call sites in `standalone/res_path_detour.cpp`
across their line breaks, tested directly, and `d["detoured"]` holds
`0x1800bd360` and `0x1800be0e0` — and then has nowhere to put it.

**Measured cost: 2 rows today.** The Object namespace reads `detour 0` with two
live detours in it. **Error direction: OPTIMISTIC** — it under-reports how much
of the DLL's own call graph we have taken over, which is the dangerous
direction.

The same section is also where a body like this one surfaces at all, and it is
**framed as a suspect list**: "Ports with no caller in our tree … check each is
not a vestigial resolve". A detoured, live, differentially-verified port sits
there indistinguishable from dead code. A tool that records a fact and files it
under the wrong heading is harder to catch than one that misses it.

This corrects rung 1's claim that the headline instruments are "structurally
incapable" of showing a lane in this position. They are not incapable; the
summary table is flat and the list that names us is framed as suspicion.

---

## 5. LIVE VERIFICATION — NOT YET RUN

Deliberately an empty section rather than an omitted one.

The body has **no caller in our tree**, so registration alone gives it
`calls=0`. The live cell needs the game-dir lock, and the lock is the
integrator's attempt-22 bottleneck; the ruling was attempt 22 first, this
second. Planned, in one window:

* **Cell A** — `BANISHED_RT_RESFIND_DETOUR=1 BANISHED_RT_DIFF=1`, autostart +
  pause-menu + quit. Expect `calls≈22.6k`, `worked==calls`, `divergences=0`,
  and `identDefaulted > 0` (a run in which the `":resource"` arm is never taken
  has not exercised the only interesting branch, and the report must say so).
* **Cell B** — `BANISHED_RT_RESFIND=0`. Expect the `[ResFind]` line absent and
  the `[RtPort] Object` count one lower.
* **Cell C** — registered but NOT detoured. Expect `calls=0`. **This ships
  first-class; if the window is tight, coverage breadth is cut, not the
  control.** An honest `calls=0` printed beside a live one is what makes the
  `calls=` column interpretable at all.

Until those exist, **this body is transcription plus a source-side set-diff, and
nothing more.** It is registered and it compiles; that is not verification.

---

## 6. Files and switches

```
runtime/src/object/external_find.h     NEW   declarations + counters
runtime/src/object/external_find.cpp   NEW   the body, portable (no <windows.h>)
runtime/src/object/rt_object_port.cpp  +6    registration hook
standalone/res_path_detour.cpp         +52   a SECOND detour target
```

| switch | effect |
|---|---|
| `BANISHED_RT_RESFIND=0` | kills the door (A/B baseline) |
| `BANISHED_RT_RESFIND_DETOUR=1` | patches the DLL's `0x1800be0e0`. **Off by default** |
| `BANISHED_RT_RESFIND_NEGCTL=<1..3>` | forces one negative control |

The detour prologue is **12 bytes**, boundary counted in Ghidra
(`40 55 / 53 / 56 / 57 / 48 8B EC / 48 83 EC 38` = 2+1+1+1+3+4). Its
`MOV RBP,RSP` **is** an RSP capture and is **not** the hazardous kind: it
happens after four pushes, and RBP addresses only this frame and the caller's
home space, both of which the trampoline reproduces when CALLed normally.
Written down because "the installer handles RSP captures" and "this RSP capture
is safe" are different statements, and only the second is true by inspection.

---

## 7. Handed on

1. **`FindOrLoad`'s unported callees are 3 → 2** once this lands:
   `0x1800be570` `Add` and `0x1800bc620` `External::Load`. State it that way —
   the static export scan is not a cutover gate (the integrator measured two
   whole lanes, 8 ported exports including one verified on 22,595 live calls,
   and both headline instruments were flat).
2. **`Add` (`0x1800be570`, 123 instructions)** needs `0x1800bb090` (type still
   unidentified), `0x180006a10` and `0x180006250` `External::GetPath`, plus one
   virtual call through `External` vtable slot `+0x28`. Its insert
   (`0x18002ca00`) is already lane RES's.
3. **`External::Load` (`0x1800bc620`)** is unchanged on the `IO::Encoding`
   vtable at `0x1801554a8` — lane ENC's ground.
4. **The detour pattern is now two-for-two.** Any Runtime body whose only caller
   is another Runtime body can be given full live coverage and a real oracle
   this way, in one file and one env var.
