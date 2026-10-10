# CUTOVER ATTEMPT 26 — the wall moved, and the manifest worked

Run by the integrator on the merged Wave 25 tip, after all eight lanes.
`tools/cutover_attempt26.sh`, exe md5 `6007cf98576b57a65f070ed8c6b095e0`.
Runtime-x64.dll moved aside, **restored and verified present (2,762,752 bytes)** by the
script's unconditional EXIT trap.

## 1. THE WALL, NAMED

```
[RtMint] FATAL: minted BaseTypeInfo::factoryFn is not ported
[RtMint]   vtable : ?_typeInfo@StringTable@UI@@0VBaseTypeInfo@Object@@A
[RtMint]   slot   : 0
[RtMint]   target : ?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ
```

**This is not an EXEC-of-0.** It is the mint's own per-slot trap firing and **naming itself** —
the thing those traps were built for. `standalone_crash.log` is 0 bytes because nothing
crashed: the process failed fast, deliberately, with the symbol printed.

**The wall MOVED, and by a long way.** Attempt 25 died in audio at
`??0Stream@Audio@@QEAA@XZ`, called from our own `Audio::Device` ctor. Attempt 26 shows
**zero references to `Stream@Audio` in the error log** — Wave 24 lane SMALLBODY's port of that
ctor (and its conversion of retail's RIP-relative LEA stamp for `??_7Stream@Audio@@6B@` into a
real door) carried the boot past it.

Boot now reaches, in order: Engine::Startup complete · MainLoop constructed · Game ctor done
(`t=569ms`) · KeyMap singleton · InputConfig → KeyMap · **50 default input bindings populated
and Load applied** · `StringTable typeId = 3224705508` → trap.

That is deep into game initialisation, well past the video and audio subsystems that stopped
attempts 23, 24 and 25.

## 2. THE MANIFEST WORKED, FIRST TIME, WITH NO HAND-EDITING

Lane MANIFEST's generator ran at cutover and **detected DLL absence itself** — no new env var,
exactly as designed:

```
[CutoverManifest] ==== DLL ABSENT (cutover) ==== 46 tagged gap rows
[CutoverManifest] published 44, withheld 1, conditional 1, unminted 0, failed 0 (of 46)
```

Those are **precisely the numbers MANIFEST projected while it was still writing the thing**,
and they held after lane DATA added 14 more tagged rows *after* MANIFEST had finished
measuring — which is what the generator (rather than a list) was for.

It also pre-empted its own worst case, in the log, before the fault:

```
[CutoverManifest] WITHHELD  UI  ??_7Dialog@UI@@6B@
[CutoverManifest]   ^ NOT published, and 5 by-name door(s) in this tree WILL resolve NULL.
                     If attempt 26 faults reaching this symbol, that is this line,
                     not a new discovery.
```

It did not fault there. But a wall that announces itself *in advance* and is then ruled out is
worth more than one diagnosed afterwards.

## 3. WHAT THIS DOES NOT ESTABLISH

- **The wall after this one is NOT named.** A cutover boot is a GATE, not a planner — it can
  only name what it reached before faulting. Expect attempt 27 to find a new name, not a list.
- **One run.** No control run was taken (attempt 25's lane took one; this did not).
- **`BANISHED_RT_MAINLOOPFRAME=0` was set**, on lane BODY's advice, so its newly default-on
  `?Frame@MainLoop@Engine@@` could not confound a new failure. **Attempt 27 should re-run
  without it** — the wall may differ, and that difference is information.
- Nothing here says the 44 published rows are *correct*, only that publishing them did not
  fault. No differential oracle exists at cutover.
- `[VideoInterface] *** DETOUR FAILED ***` did **not** appear as a false alarm this run; lane
  SWEEP's fix made the check ask the registry.

## 4. NEXT

`?CreateNewObject@StringTable@UI@@SAPEAVBase@Object@@XZ` is a factory body, slot 0 of
`?_typeInfo@StringTable@UI@@`. It is one function. The trap is the mint's, so the class was
minted before this method was ported — the ordinary shape, and the ordinary fix.

**Do not read "one function" as "one function to cutover."** That is exactly the reasoning the
gate cannot support.

---

# CORRECTION, 2026-09-12 (Wave 26 lane ATTEND; verified independently by the integrator)

**§2 above is BACKWARDS on its central claim, and this section supersedes it.**

§2 says the manifest generator "ran at cutover and detected DLL absence itself", that its
`46 tagged gap rows` were "precisely the numbers MANIFEST projected", and that those numbers
"held after lane DATA added 14 more tagged rows *after* MANIFEST had finished measuring —
which is what the generator (rather than a list) was for."

**The generator was never re-run.** `standalone/cutover_manifest.inc` was STALE at the moment
attempt 26 ran, and was still stale on `wave26/base`:

```
$ git log --oneline -1 -- standalone/cutover_manifest.inc
33206d7f Wave 25 lane MANIFEST: the tagged rows as a cutover publish manifest

$ tools/ghidra_mcp/.venv/Scripts/python.exe tools/cutover_manifest.py --check
STALE: standalone\cutover_manifest.inc does not match a fresh derivation
cutover_manifest: 60 tagged gap rows -- CONDITIONAL 1, REQUIRED 58, WITHHELD 1
```

**46 was the PRE-DATA count.** The fresh derivation is **60** — missing rows are *exactly*
lane DATA's 14. So the very fact §2 cites as proof the generator worked (the numbers matching
MANIFEST's projection) is the symptom that it never ran: they matched **because they were the
old numbers**.

## Why this survived the wave

Integrator-queue item C2 said, in as many words: *"Expect `published 44, withheld 1,
conditional 1, unminted 0, failed 0`, and **re-derive rather than trusting those numbers**,
since DATA's 14 tags landed after MANIFEST measured."*

The instruction to re-derive was written, correct, and addressed to the integrator — **and the
matching number was read as confirmation instead.** An expected figure that arrives on time is
the hardest kind of staleness to see, because agreement with a prediction is exactly what a
working generator would also produce.

**This is a known family here** (`reference_a_prescription_read_as_a_completion`,
`reference_a_generated_file_goes_stale_with_no_detector`): a generator whose input set is
"what the tree references today" is a cache with no invalidation, and a wave doc naming a fix
reads as proof the fix landed. The new member of the family is sharper: **a number that
MATCHES its prediction can be matching because it is STALE, and the match is then evidence of
the defect rather than against it.** `--check` existed the whole time and answers in one
second; nobody ran it.

## What else §1 and §2 must no longer be read as establishing

- **Attempt 26 ran with `BANISHED_RT_MINT=1`** (`tools/cutover_attempt26.sh:42`), against
  integrator-queue item **C3**. Lane ATTEND settled the mechanism from source:
  `RegisterMintedData()` runs **22 lines before** the manifest and walks all three mint tables
  **with no skip list**, so the blanket switch published `??_7Dialog@UI@@6B@` by name *and* by
  address. The log's `registered 202 by name` sits **above** `withheld 1`.
- Therefore §2's closing sentence — that a wall which "announces itself in advance and is then
  ruled out is worth more than one diagnosed afterwards" — **ruled out nothing**. The WITHHELD
  row's 5 by-name doors were never in force to begin with, because the row was published.
- **The blanket switch and the manifest are not substitutes**: the switch publishes 202 rows,
  the manifest covers only the 60 tagged ones. The gap is ~104 rows.

## What still stands from attempt 26

The **wall itself** is real and reproduced: `?CreateNewObject@StringTable@UI@@` is stable
across 3 runs and 2 exes, and `BANISHED_RT_MAINLOOPFRAME=0` was measured NOT load-bearing
(own control arm, identical wall with and without). The boot genuinely does reach deep into
game init. None of that depended on the manifest.
