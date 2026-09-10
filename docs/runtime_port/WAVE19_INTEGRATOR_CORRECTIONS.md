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
