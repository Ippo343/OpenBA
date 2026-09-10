# WAVE 15 LANE INSTR — the shared reference enumerator

**Worktree** `C:/code/GIT/wt-wave15-instr`, **branch** `wave15/instr`.
Deliverables, all committed on that branch:

| file | what it is |
|---|---|
| `tools/rt_singleton_refs.py` | the instrument |
| `docs/runtime_port/wave15/rt_singleton_refs.csv` | 677 rows — every in-DLL reference to the 19 in-scope `?_singleton@` slots |
| `docs/runtime_port/wave15/rt_singleton_refs_memoryallocator.csv` | 2,188 rows — `MemoryAllocator@System`, out of scope for the wave, kept out of every summary and control |
| `docs/runtime_port/wave15/rt_singleton_refs_run.txt` | the archived console output of the run these numbers come from |

Run it with the SYSTEM Python 3.12 (`python tools/rt_singleton_refs.py`), not the
project venv: the venv has `pefile` but no `capstone` and no `pip`. The script
refuses to run without capstone rather than degrading to a hand-written
instruction-form list, because a missed form is a silently dropped reference and
that is the false-SAFE direction.

The run archived here exits **5** — a code that exists precisely so a broken
instrument and a stale ground truth cannot share one. Every REFERENCE fact is
green; one ground truth's CLASSIFICATION does not reproduce, for a reason that
is a finding rather than a defect (control 2 below).

## What it does

Three routes, plus a fourth cross-check:

* **SCAN** — candidate generation that is complete by construction. Every
  RIP-relative reference encodes a 4-byte disp32 followed by 0/1/2/4 immediate
  bytes and then the end of the instruction, so for every byte offset `p` and
  every trailing size `t`, `base + p + 4 + t + disp32(p)` is a candidate. No
  instruction form can escape it. 2,865 candidates over the 20 slots.
* **BACKWARD** — capstone decodes one instruction at every start in `[p-15, p-1]`
  and keeps the decode whose own computed RIP target is the slot. The decoder
  computes the end of the instruction, so the imm32 quirk is structural, not a
  special case.
* **SWEEP** — an independent linear disassembly of `.text` (restarting one byte
  later on an undecodable byte) collecting the same references, and supplying the
  instruction boundaries that settle BACKWARD's prefix ambiguity.
* **GHIDRA** — `get_xrefs_to` per slot, read straight off the MCP bridge's HTTP
  endpoint (`http://127.0.0.1:8089/`), so the comparison is automated per row
  rather than pasted.

Measured on this run: **2,865 candidates → 2,865 confirmed, 0 unconfirmed,
0 UNKNOWN-FORM, 0 rejected as interior bytes, and BACKWARD vs SWEEP differ by
zero references in both directions.** 51 bytes of `.text` were undecodable by the
linear sweep and none of them carried a candidate.

## Control 1 — SaveLoad, scored on its own: **GREEN**

All five references reproduce at the five known VAs, with the right kind, the
right owning export and the right port state:

```
0x1800c3d9b WRITE ??0SaveLoad@Save@@QEAA@XZ            PORTED+REGISTERED  (R1+R2)
0x1800c3ca6 WRITE ??1SaveLoad@Save@@QEAA@XZ            PORTED+REGISTERED  (R1+R2)
0x1800dc3e9 READ  ?OnVideoRestart@MainLoop@Engine@@    PORTED+REGISTERED  (R1+R2)
0x1800dcc1d READ  ?OnVideoError@MainLoop@Engine@@      PORTED+REGISTERED  (R1+R2)
0x180030d40 READ  ?Get@SaveLoad@Save@@SAPEAV12@XZ      UNPORTED
```

The imm32 quirk is not taken on trust. `0x1800c3ca6` is
`mov qword ptr [rip+0x138a3f], 0`, trailing immediate **4 bytes**, and the script
carries a **negative control that goes RED on demand**: candidate generation
restricted to `t=0` finds **4 of the 5** and misses exactly that reference,
reproducing the historical 4-vs-5 disagreement. If that control ever stops going
red, control 1 is not to be believed.

**One miss, and its mechanism, on the way to green.** The first version scored
`?Get@SaveLoad@` as PORTED-UNREGISTERED, not UNPORTED, because a screen that asks
"does this mangled name appear in our sources?" cannot tell a PORT from a
CALL-SIDE DOOR. The name appears in `standalone/rt_orphan_probe.cpp:2131` as the
constant the probe resolves *so it can call retail's `Get`*. The fix is a door
screen: a mention is port evidence only when it is not the argument of
`ResolveExport` / `ResolveFn` / `RT_EXPORT_ONCE` / … , judged for a named constant
by how the IDENTIFIER is used rather than where it is defined. The same defect
was scoring three `?PlatformShutdown@Device@Input@@` reads as ported; they are
named only inside `ResolveFn(...)` and `RT_EXPORT_ONCE(...)`.

## Control 2 — Application, scored in two halves: count/kind **GREEN**, classification **RED**

* **Count and kind: GREEN.** 26 references, 2 WRITE at exactly `0x18009f5b8`
  and `0x1800dbbe2`, 24 READ. Reproduces WAVE12_APPLICATION.md exactly.
> **INTEGRATOR NOTE (added at merge; the lane had already stopped editing).**
> This section attributes the false "24 reads in unported bodies" to
> `WAVE12_APPLICATION.md`. **It is not from Wave 12** — grep that file and you
> get zero hits. Wave 12 wrote a KIND count, *"26 in-DLL RIP-relative
> references, 2 WRITE and 24 READ"*, which this lane reproduced GREEN. The
> conversion of that kind-count into a port-state count happened at
> `WAVE15_KICKOFF_SINGLETONS.md:86`, and the integrator then repeated it with a
> false citation to Wave 12 in this lane's own brief — so the lane was
> dispatched against a control whose expected value was wrong, and scored it RED
> rather than bending its measurement to fit. The lane's MEASUREMENT (9) stands
> and is unaffected; only the attribution is wrong. Authority:
> [WAVE15_PHASE1_FINDINGS.md](../WAVE15_PHASE1_FINDINGS.md).

* **Classification split: RED, and the ground truth is what is wrong.** The truth
  says 24 reads in unported bodies. Measured today: **9**. The other 15 reads sit
  in bodies that are ported AND registered, and this is not drift since Wave 12 —
  it was already false on the day it was written:

| read owner | reads | registered by |
|---|---|---|
| `?PlatformStartup@Device@Input@@AEAA_NXZ` | 6 | `7ded21a` **2026-09-08** |
| `?PlatformStartup@Device@Video@@AEAA_NXZ` | 3 | `070b546` **2026-09-08** |
| `?PlatformShutdown@Device@Video@@AEAAXXZ` | 2 | `070b546` **2026-09-08** |
| `?PlatformUpdate@Device@Input@@AEAAXXZ` | 1 | `7ded21a` **2026-09-08** |
| `?WindowProc@Application@1@…` | 1 | `7ded21a` **2026-09-08** |
| `?EnableCursor@UserInterface@UI@@QEAAX_N@Z` | 1 | `a3de8bd` **2026-09-08** |
| `?Get@Application@1@SAPEAV11@XZ` | 1 | `74110c0` 2026-09-09, Wave 12's OWN lane A |

WAVE12_APPLICATION.md closed on 2026-09-09 (`06624c0`). Fourteen of those fifteen
registrations predate it by a day, and the fifteenth is Wave 12's own work. So
"26 in-DLL references, 2 WRITE and 24 READ" was measured (it reproduces perfectly)
and "24 of them READS in bodies nobody has ported" was **asserted alongside it,
never measured** — the reference count carried the classification's credibility.
That is the shape `reference_a_predicate_nobody_evaluates.md` describes.

The nine reads that ARE in unported bodies today:

```
0x1800daf67  ?Update@DialogLoaderLoopT@Engine@@
0x1800dc2ad  ?Update@LoaderLoop@Engine@@
0x1800e1b56  ?PlatformShutdown@Device@Input@@   (x3: …1b56, …1b8f, …1bc2)
0x1800e1c89  ?PlatformGetKeyboardState@Device@Input@@
0x1800e1e3e  ?PlatformGetMouseState@Device@Input@@   (x3: …1e3e, …1e61, …20d0)
```

**This lane renders no verdict on `Application`.** Nothing here says the HARMFUL
verdict is wrong: nine unported readers is still nine, and a ported body's copy
inside the DLL still holds its own RIP-relative load. What it says is that the
NUMBER in that verdict's reasoning does not describe today's tree, and phase 2
must classify from measurement, not quote it.

## Control 3 — Ghidra, a route that CAN disagree, and it DID

Run per row against Ghidra's own xref database. Excluded from every row before
comparing: the `.rdata` export-directory DATA reference and the entry-point
sweep — neither is a `.text` reference.

**17 of 19 rows agree exactly. Three rows disagree, all in the same direction
(Ghidra finds FEWER), 6 references total, all only-MINE, zero only-GHIDRA.**

| row | mine | ghidra | only-MINE |
|---|---|---|---|
| `Configuration@Input` | 24 | 23 | `0x18000dd50` |
| `Configuration@Video` | 63 | 62 | `0x18000f050` |
| `FileSystem@IO` | 91 | 87 | `0x1800293b0`, `…bd`, `…e0`, `…ef` |

**Cause, established not assumed:** `get_function_by_address` returns *no function
at or containing* any of the six, and `get_xrefs_from` on them returns nothing.
Ghidra's analysis never disassembled those bytes, so its xref database has
nothing to report there — a coverage gap in the external route, not a false
positive in this one. Both of my routes decoded them independently, and reading
the bytes settles it:

```
18000dd50  488b0d110a1f00   mov rcx, [rip+0x1f0a11]     ; Configuration@Input
18000dd57  e9d42f0d00       jmp 0x1800e0d30             ; -> int3 padding, 16-byte aligned
1800293b0  488b05f1321d00   mov rax, [rip+0x1d32f1]     ; FileSystem@IO
1800293b7  0fb65019         movzx edx, byte [rax+0x19]  ; a 4-instruction accessor
```

Two are `mov rcx,[rip+slot]; jmp impl` thunks, four are one-line accessors
reading a byte field off the singleton. All six are int3-padded, 16-byte aligned
real code; none is an export start. They are unexported internals nothing in the
binary references, which is presumably why Ghidra's analysis never reached them.

**Attribution disagreements are reported too, and they changed the output.**
Ghidra names the CONTAINING FUNCTION, which is an independent check on this
tool's nearest-preceding-export attribution — the same error class
`rt_mint_writer_audit.py` carves the CRT initialisers out for. Of the 677
in-scope references, **198 sit in functions Ghidra has as unnamed `FUN_…`**,
i.e. in UNEXPORTED internals that merely follow an export in address order.
Crediting those to the preceding export would have handed them that export's
port state. They are now `owner_port_state = UNKNOWN-OWNER`, with the nearest
export kept in `owning_export` and the reason in `attribution`. With the six
Ghidra-invisible sites that is **204 of 677 = 30%**.

For phase 2 this is a structural fact, not bookkeeping: an unexported internal
has no name to register, so its RIP-relative read can never be moved by a port.
Treat UNKNOWN-OWNER as at least as risky as UNPORTED, never as ported.

## The comment stripper's control — a count it must NOT move

`--no-strip-comments` re-runs both port-state screens on unstripped source:

* **Reference facts: 677 and 2,188 rows, set difference ZERO.** The count that
  must not move did not move — those come from the binary.
* **In-scope port states: zero rows moved.**
* **Out-of-scope CSV: exactly 2 rows moved**, both owned by
  `??1GoalList@Achievement@@QEAA@XZ`, UNPORTED → PORTED-UNREGISTERED. All four
  occurrences of that name in the tree are COMMENTS
  (`code/Game/Goal/statgoal.cpp:415`, `runtime/types/achievement/Goal.h:147/154/171`).
  Without the stripper the tool reads prose as code, exactly as
  `reference_uncommented_checker_manufactures_work.md` describes.

## The measured table — the FIRST MEASUREMENT of these rows

The kickoff's per-row column is a calibrated upper-bound scan and it asks twice
that its numbers not be quoted as measurements. These are measurements.
`reads-in-UNPORTED` counts READ/RMW references whose owning export is neither
ported nor registered; `unknown-owner` counts references whose owner could not be
attributed (see control 3) and is NOT included in the previous column.

```
symbol                             total  READ WRITE  RMW  LEA  UNK   unported-reads / unknown-owner
MemoryAllocator@System              2188  2185     3    0    0    0    1787 / 0   [OUT OF SCOPE]
Device@Video                         165   163     2    0    0    0      60 / 23
ExternalFactory@Object               151   149     2    0    0    0      14 / 127
FileSystem@IO                         91    89     2    0    0    0      55 / 22
Configuration@Video                   63    56     7    0    0    0      12 / 21
Application@1                         26    24     2    0    0    0       9 / 0
Configuration@Input                   24    17     7    0    0    0       7 / 5
Persistence@IO                        24    23     1    0    0    0       4 / 0
Factory@Object                        20    18     2    0    0    0      11 / 2
MainLoop@Engine                       20    19     1    0    0    0      11 / 0
AnimationManager@Graphics             19    17     2    0    0    0      14 / 1
Device@Input                          17    16     1    0    0    0       7 / 0
Modification@IO                       11     6     5    0    0    0       2 / 1
Configuration@Audio                    9     2     7    0    0    0       2 / 1
Device@Audio                           8     7     1    0    0    0       4 / 0
AchievementManager@Achievement         6     1     5    0    0    0       0 / 1
Configuration@System                   6     4     2    0    0    0       2 / 0
Engine@1                               6     4     2    0    0    0       4 / 0
InternalFactory@Object                 6     5     1    0    0    0       1 / 0
SaveLoad@Save                          5     3     2    0    0    0       1 / 0
```

Against the kickoff's estimates the ranking mostly holds and several values do
not: `ExternalFactory` 151 (est. ~188), `FileSystem` 91 (~113), `Configuration@Video`
63 (~76), `Device@Video` 165 (~206), but `Persistence` 24 (~30), `Configuration@Input`
24 (~27) and `Configuration@Audio` **9** (~8) sit differently in the order, and
`MemoryAllocator` is 2,188 (~2,700). The 4–5x calibration factor was reasonable;
the individual values were not measurements and should not be re-quoted.

Three further facts that fall out of the table:

* **Not one LEA, RMW or UNKNOWN-FORM anywhere.** All 2,865 references across all
  20 rows are plain reads and writes. The kickoff's method item 5 asks for
  pointer-valued references; there are none in `.text`.
* **And none outside `.text` either.** The script scans every section for an
  absolute 8-byte pointer equal to a slot VA: **0 hits**, all sections, all 20
  slots. Wave 14 established this for `SaveLoad`; it now holds for all 20.
* **`AchievementManager` has 0 unported reads** — its single read is its own
  `?Get@`, ported+registered. Of the 5 writes, one (`??1AchievementManager`, the
  dtor at `0x18009eac6`) is in an UNPORTED body and one is unknown-owner, so
  "0 unported reads" is not "0 unported references". It is the smallest row in
  the set and by these numbers the least entangled; that is an observation, not
  a verdict.

## Six PORTED-UNREGISTERED review flags

`?AdjustDisplay@Device@Video@@QEAAXXZ` owns 6 reads of `Configuration@Video` and
is named in our sources in a non-door, non-registration context while nothing
registers it. That is the exact signature of the half-registered port
(`reference_half_registered_port_reads_as_absent.md`), so it is flagged rather
than classified. It is a REVIEW flag, not a verdict — this lane does not open
the file.

## Routes I could NOT enumerate — named, not implied

1. **Anything outside retail `Runtime-x64.dll`.** The scan covers that one
   binary. `Application-x64.exe`, `VideoDX11-x64.dll`, `VideoDX9-x64.dll` and
   `Runtime-x64-profile.dll` are NOT scanned by this tool. The exe's readers go
   through the import table and are DOOR facts — lane DOOR's column, and the
   reason the kickoff's "exe door" column exists.
2. **Computed / indirect reaches.** A body that materialises a nearby address and
   then loads through a register (`lea rax,[rip+base]` … `mov rcx,[rax+0x18]`)
   references `base`, not the slot, and no scan keyed on the slot's own address
   can see it. This is not a hypothetical class — the 20 slots are packed in one
   region — and I have not bounded it.
3. **`GetProcAddress` at run time**, by name or by
   `MAKEINTRESOURCE(ordinal)`. There is no string to find for the ordinal form.
   Unclosable by static means, and stated rather than pretended closed.
4. **Ghidra's DATA and EXTERNAL rows** were deliberately excluded from the
   comparison (export directory, entry-point sweep). If either ever carried a
   real reference this tool would not see it.
5. **The identity of the 198 + 6 unattributed owners.** I established that they
   are unexported internals; I did not name them or establish whether any is
   reachable. Phase 2 needs Ghidra function boundaries for those, not the export
   table.
6. **No game run was needed and none was taken.** Every number here is static,
   off the binaries and the tree. `tools/game_dir_lock.sh` was not touched.

## What phase 2 gets

A per-reference CSV it can filter, with the columns the classification actually
needs: `symbol, slot_va, ref_va, kind, owning_export, owner_port_state`, plus
`port_state_screens` (which screen fired — never score a control on the union),
`route`, `trailing_imm`, `disasm`, `ghidra_kind`, `ghidra_function`,
`attribution`. Re-running is one command and takes ~25 seconds with the Ghidra
route, ~20 without.
