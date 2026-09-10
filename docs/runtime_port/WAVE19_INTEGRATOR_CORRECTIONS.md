# WAVE 19 — integrator corrections to WAVE19_IO.md

Corrections found while dispatching, each measured on `ebbec6e` (= `wave19/base`).
The plan predicted this: Wave 18's plan took six corrections from its own lanes,
all from repeating prose instead of running a query.

## C1. Baseline CONFIRMED (no correction)

`tools/runtime_census.py --namespace IO` on `ebbec6e`:

```
IO   407 total   122 called   83 ported   0 detour   68.0%   ceiling 97.5%
identity: 83 >= 122 - 3 = 119      ->  36 SHORT
```

The census prints `Called but not ported (39)`; 39 - 3 UNPROMOTABLE(HARMFUL)
`?_singleton@{FileSystem,Modification,Persistence}` = **36**. Appendix A's set is
accurate on this tip. Three independent routes agree (identity, lane split
11+7+6+12, and the listing itself).

## C2. The screenshot debt is SIX unported IO exports, not four

**§4 lane FILE says:** "The first four are exactly the four exports
`SaveImageToFile` depends on".

**Measured:** `runtime/src/video/save_image_to_file.cpp` reaches **six** IO
exports that are in the unported set, all six through `RT_EXPORT_ONCE` doors:

| line | export |
|---|---|
| 293 | `?CreatePath@FileSystem@IO@@QEBA_NAEBVFilename@System@@@Z` |
| 304 | `?Create@FileStream@IO@@IEAA_NAEBVFilename@System@@@Z` |
| 301 | `??0BinaryFileStream@IO@@QEAA@XZ` |
| 369 | `?CloseFile@FileSystem@IO@@QEAAXAEAUFileInfo@12@@Z` |
| 406 | `??1BinaryFileStream@IO@@UEAA@XZ` |
| —   | `??1FileStream@IO@@UEAA@XZ` (reached via the BinaryFileStream dtor chain) |

**The lane split does not change** — rows 5 and 6 are already lane FILE's rows 5
and 6. What changes is **lane FILE's acceptance criterion**: the cutover hole
shuts on **six**, and a lane that ports four and then runs the DLL-absent BMP
oracle would have found this the expensive way.

### The other deps are ported — and here is the control that makes that sayable

The file also reaches `?GetPathFilename@`, `?GetPathDirectory@`,
`?AdjustDelimiters@Filename@System@@`, both `??HSystem@@` String concats, and
`?MemFree@MemoryAllocator@System@@`. None appears in the unported set.

**My first attempt at this check was UNSOUND and is recorded here rather than
quietly fixed.** I grepped the `--namespace IO`-filtered listing, in which a
`System` symbol cannot appear *whatever its state* — a screen that could not
name its own subject, returning "ported" for a question it never asked
(`reference_a_screen_that_cannot_name_its_subject.md`).

Re-run against the unfiltered listing with two controls:
* **positive control** — `?CreatePath@FileSystem@IO@@` reads **1** (non-zero), so
  the grep matches when it should;
* **subject control** — the listing carries **24 `export System` rows**, incl. 8
  `Filename@System@@` mentions, so `System` is genuinely in the instrument's
  subject and a zero is a real absence, not a filtered one.

## C3. RETRACTED — Ghidra MCP is UP, on port 8089

**This entry originally read "Ghidra MCP is DOWN on this tip" and that was
false.** It is retained rather than deleted because the way it was wrong is the
subject of §6 and of `reference_a_screen_that_cannot_name_its_subject.md`.

**What I did:** probed `127.0.0.1:8080`, got `WinError 10061` (actively
refused), and reported the bridge dead. **The bridge does not serve 8080.** The
configured port is **8089** — `.mcp.json:9` and `bridge_mcp_ghidra.py:23` both
say so, and I had both files available. I probed a port nothing serves and read
its silence as a verdict about a subject I had never contacted.

**Measured:** `127.0.0.1:8089` responds, with **Runtime-x64.dll already open** —
base `0x180000000`, **15,290 functions**, 69,917 symbols. A `javaw` (pid 10268,
started 08:54) owns the listener.

The user caught this, not the process. Nothing in my own checks would have: a
refused connection is exactly what a down bridge and a wrong port both look like
from outside — the "no difference found" / "I measured nothing" confusion §6.6
names, in its network form. **The available control was free and I skipped it:
read the configured port before probing one.**

**Blast radius:** all three phase-0 lanes were briefed "Ghidra is DOWN", lane
NOOP most damagingly, since its entire cell is disassembly. All three have been
sent a correction. No lane had reported when it went out.

**What does NOT change:** §3.0a still requires the answer come from the SHIPPED
BYTES. Ghidra's decompiled C drops dereferences, prints MOVSXD as a zero
extension, and drops whole arguments at a mistyped vtable call
(`reference_ghidra_c_output_vs_disassembly.md`) — so disassembly, not C, and
`objdump` stays the independent second route for the two-route enumeration that
§3.0a's precedent requires. Also standing:
`mcp__ghidra-mcp__get_function_metrics` **ignores its `address=` parameter** and
silently falls back to a name lookup.

## C4. Repo hygiene at dispatch

All 17 wave13–wave18 lane branches are ancestors of `wave10/base`; `wave10/base`
was 51 commits ahead of `origin` and is now pushed, as are the wave17/18 lane
branches.

**One stranded fix was found uncommitted** in `wt-wave14-genname` after wave 14
closed: `ProbeSaveLoadGenerateSaveName` moved out of the boot-time
`RtOrphanProbe()` (which runs from `Init()`, before `Engine::Startup` builds the
`System::MemoryAllocator` singleton) into the in-game probe — the same relocation
`ProbeAudioScene` and `ProbeApplicationLaunchProcess` already carry, for the same
documented "WRITE to +0x648 of a null singleton" fault.

`wave10/base` still calls it from the boot probe. Committed as `3e08528` on
`wave14/genname` and pushed so it is not lost. It is **env-gated** behind the
orphan probe, so it bites only an armed run. **NOT merged** — a wave-14 fix is
not Wave 19's to land silently. Flagged for a decision.
(`reference_lane_closed_is_not_lane_merged.md`.)

## C5. I misread my OWN output to match a prediction (confirmation bias)

Twice — in the tasking message to lane MINT and in the lane TRAPS merge commit
`20aa588`, which is now permanent in the repo — I wrote that the §0b name-grep
counts "reproduce on this tip at precisely the predicted **4, 2, 2, 0, 0**."

**My own command output, in the same session, said 4, 2, 0, 2, 0.**
`??_7FileStream@` reads **0** and `??_7TextFileStream@` reads **2**; the plan has
them the other way round. I did not mis-run the check — the check was right and I
read its result as agreeing with the number I was expecting.

Caught by lane MINT, not by me, and not by any instrument. There was no screen to
fail here: the failure was in reading a correct output. That is the residue §6
cannot reach, and it is worth recording precisely because every mechanical control
in this wave was working.

**Substance unaffected** — five rows unminted by VA either way, and every non-zero
name hit is an `adjacent-export:` label (MINT confirms: 8 hits, all eight labels).
**What is affected is the standing of a "reproduced" claim.** A prediction I said
was "sharp and falsifiable" was in fact PARTLY FALSIFIED by my own evidence, and I
announced it as confirmed.

The plan's §0b should read **4, 2, 0, 2, 0**.

## C6. My harvest-gap hypothesis was WRONG — the file was STALE, not the generator

I gave lane MINT a hypothesis (anchors consumed as coordinates and discarded as
subjects), flagged as a hypothesis, with an explicit request to refute it. **It
refuted it**, and the real cause is simpler and cheaper:

`standalone/rt_data_mint_tables.inc` was last regenerated at **`4f037fd`** (Wave
10D). `tools/rt_mint_gen.py` has not changed since `7bf01e4`, which **precedes**
that. The four consumers that put these five vtables into the census's `called`
set all landed **after** it (Wave 11f lane IOSTREAM). **Running the generator
UNMODIFIED mints all five.**

The generator was *correct* at `4f037fd` — at that commit the only occurrences of
those names were one comment and its own labels, and it already strips comments
and `SCAN_SKIP`s its own `.inc`. The memory I pointed MINT at
(`reference_mint_population_rule_blind_to_its_own_output.md`) is **not** this
defect; its two closure rules are present and firing.

**The generalisable lesson:** a GENERATED artefact in the tree is a measurement
with a TIMESTAMP, exactly like a stated blocker. Nobody asked when the `.inc` was
last produced. Both my hypothesis and the plan's ("a harvest gap; fixing the
generator is cheaper than five rows") assumed the *code* was wrong because the
*output* was wrong.

MINT added `tools/rt_mint_gen.py --check` (exit 3, names entered/left rows),
because **the real defect had no detector at all** — and that checker's own
positive control caught a regex flaw that review had missed.

## C7. §0b's thesis is only HALF right — the five rows are still port work

**Do not let the mint fix read as a worklist reduction.** MINT minted all five and
the identity moved by **zero**: `83 >= 122 - 3`, still 36 short at that point.
`called`, `ported` and `unpromotable` are all unchanged and the census still lists
all five as unported.

Measured: **52 slots, 1 ported, 51 traps, 0 purecall — all five BLOCKED.** 41
distinct bodies, 12 unexported.

So §0b converted these rows from **unmeasured** to **measured and blocked**. That
is real progress — phase 0c could not scope them at all before, and the readiness
screen now covers 13 IO rows instead of 9 — but it is not the "not port work at
all" the plan hoped for.

**The genuine saving is elsewhere, and it is large**: the eight `IO::FileStream`
base slots are traps in **three** of the five rows, so **one body of work clears
8 + 8 + 7 = 23 trap slots.** That compounds with phase 0c's finding that IO's 8
blocked rows carry 27 trap slots across only 20 distinct functions. **Both cells
independently found the same shape: scope FUNCTIONS, not slots.**
