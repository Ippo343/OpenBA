# WAVE 15 PHASE 1 — findings, corrections, and what phase 2 inherits

Written by the integrator as lanes closed. Lanes DOOR (`5049dbc`) and INSTR
(`94ecaf8`) are merged; ORDER is still running.

## Corrections to the record — including to this wave's own documents

**1. The kickoff's Application triage line was false, and I propagated it.**
`WAVE15_KICKOFF_SINGLETONS.md` line 86 read *"Application 26 refs, 24 of them
READS IN UNPORTED BODIES -> HARMFUL"*. `WAVE12_APPLICATION.md` actually says
*"26 in-DLL RIP-relative references, 2 WRITE and 24 READ"* — a count of KINDS.
The kickoff converted a kind-count into a port-state count. Wave 12 never made
the claim.

Measured by lane INSTR: **9 reads in unported bodies, not 24.**

I then repeated the error with a false attribution: `WAVE15_LANE_INSTR.md`
control 2 tells the lane the ground truth is *"26, of which 24 are READS IN
UNPORTED BODIES. From `WAVE12_APPLICATION.md`."* It is not from Wave 12. The
lane was dispatched against a control whose expected value was wrong — and it
scored it RED and found the defect in the CONTROL rather than bending its
measurement to match, which is the outcome the control existed for. The
retraction is recorded at the kickoff itself rather than deleted, so the next
reader sees the reasoning and not just the corrected number.

**The `HARMFUL` verdict on `Application@1` survives untouched.** Wave 12's real
argument never rested on the reader split: we intercept only at DOORS, so all 26
in-DLL sites keep reading the DLL slot regardless of what is ported, because a
ported body's copy INSIDE the DLL still holds its own RIP-relative load. Nine
unported readers is still nine, and INSTR correctly rendered no verdict.

The transferable lesson: a measured number (26/2/24, which reproduces exactly)
sat beside an asserted one and lent it its credibility.

**2. "The disp32 scan overcounts and never undercounts" is NOT established.**
I broadcast that after lane DOOR. It held for DOOR's exe-side rows, where all 9
extra hits were diagnosed as coincidence. Lane INSTR then found the opposite on
the DLL side: 6 references that the disp32 route has and Ghidra does not, and
they are REAL CODE — two `mov rcx,[rip+slot]; jmp impl` thunks and four one-line
accessors, int3-padded and 16-byte aligned, in bytes Ghidra never disassembled
(`get_function_by_address` finds nothing containing them; `get_xrefs_from`
returns nothing).

So both routes have a failure direction: the byte scan manufactures hits at
non-instruction offsets, and `get_xrefs_to` is blind wherever Ghidra's
disassembly does not reach. Neither is "the reliable route".

**3. Consequence for lane DOOR's merged column, carried as a known gap.** DOOR
diagnosed 9 disp32-only hits on the exe side; it confirmed 2 in detail as
mid-body of disassembled functions (benign) and attributed the other 6 to "No
instruction at address". That is exactly the signature INSTR has now shown can
also mean *real code Ghidra never disassembled*. So DOOR's `ghidra_xrefs` column
may undercount by **up to 6 references spread across 7 exe rows**. Bounded and
small, it does not change any door verdict, and `ExternalFactory@Object` — the
row where it would matter most — shows zero disagreement between routes. Phase 2
resolves it by reading those 6 addresses' bytes, the way INSTR resolved its own.

## What phase 2 inherits as measured fact

* **The first real measurement of all 20 rows** (`tools/rt_singleton_refs.py`,
  `docs/runtime_port/wave15/rt_singleton_refs.csv`, 677 in-scope rows + 2188 for
  `MemoryAllocator`). Run it with system Python 3.12 — the project venv has
  `pefile` but no `capstone`, and the script exits 3 rather than degrading to a
  hand-written instruction-form list.
* **Not one LEA, RMW or UNKNOWN-FORM among 2865 references**, and **zero absolute
  8-byte pointers to any of the 20 slots in any section**. Every reference to
  every row is a plain read or write. This closes the kickoff's method item 5 for
  all 20 rows rather than for `SaveLoad` alone.
* **30% of in-scope references have no attributable owner** — 204 of 677. 198 sit
  in unnamed `FUN_` internals that merely FOLLOW an export in address order, and
  nearest-preceding-export attribution would have handed them that export's port
  state. An unexported internal has no name to register, so its read can never be
  moved by a port: **UNKNOWN-OWNER is at least as risky as UNPORTED and must
  never be counted as ported.**
* **Doors** (lane DOOR): 15 of 20 rows imported by the exe, confirmed cell by
  cell; 3 of those (`Application@1`, `Configuration@Video`, `Device@Video`) also
  have independent doors in VideoDX11 and VideoDX9, so a promotion there has
  three doors to intercept. Import-by-ordinal is closed across all three
  binaries. The 5 no-door rows have nothing to promote into (clean NO-OP).
* **REGISTERED IS NOT PROMOTED.** 8 of the 15 door rows carry a
  `RegisterDispatchSlot`, but most are commented no-op passthroughs resolving the
  same DLL address. Only `SaveLoad` is genuinely promoted. Read as promotion
  progress, that column would have made phase 2 look half-done.

## The tier revision the measurement forces

The kickoff's suggested cheap tier was ranked off the crude scan. Measured
in-DLL references, with reads-in-unported / unknown-owner:

| row | refs | unported reads | unknown-owner | note |
|---|---|---|---|---|
| `AchievementManager` | 6 | **0** | 1 | cheapest by every measure |
| `InternalFactory@Object` | 6 | 1 | 0 | |
| `Configuration@System` | 6 | 2 | 0 | no door |
| `Engine@1` | 6 | 4 | 0 | no door |
| `Device@Audio` | 8 | 4 | 0 | no door |
| `Configuration@Audio` | 9 | 2 | 1 | |
| `Modification@IO` | 11 | 2 | 1 | |
| `Device@Input` | 17 | 7 | 0 | |
| `Factory@Object` | 20 | 11 | 2 | |
| `MainLoop@Engine` | 20 | 11 | 0 | |
| `AnimationManager@Graphics` | 19 | 14 | 1 | no door |
| `Persistence@IO` | 24 | 4 | 0 | |
| `Configuration@Input` | 24 | 7 | 5 | |
| `Application@1` | 26 | 9 | 0 | HARMFUL, stands |
| `Configuration@Video` | 63 | 12 | 21 | |
| `FileSystem@IO` | 91 | 55 | 22 | |
| `ExternalFactory@Object` | 151 | 14 | **127** | |
| `Device@Video` | 165 | 60 | 23 | |

Changes from the kickoff's ranking:

* **`ExternalFactory@Object` leaves the cheap tier.** 151 in-DLL references and
  177 exe `.text` sites. Its 14 unported reads look inviting until you see the
  127 UNKNOWN-OWNER — 84% of the row sits in unexported internals that no port
  can ever move. This is the row most likely to be mis-scoped as easy.
* **`AchievementManager` is the strongest candidate**, with a caveat that has to
  travel with it: 0 unported READS, but its dtor WRITE is unported. "Zero
  unported reads" is not "zero unported references".
* **`Persistence@IO`** — 24 refs, only 4 unported reads, 0 unknown-owner — is a
  better second candidate than its raw count suggests.
* `AnimationManager@Graphics` (14 of 19 unported) would have been HARMFUL-shaped
  had it a door at all; it has none, so it is moot.

## Routes still unenumerated — carried, not closed

1. **Computed/indirect reaches.** `lea rax,[rip+base] ... mov rcx,[rax+0x18]`
   references a base, not the slot, and no scan keyed on a slot's own address
   can see it. The 20 slots are packed in one region, so this is not
   hypothetical. **NOT bounded.** This is the largest open hole in phase 1.
2. `GetProcAddress` at run time, by name or `MAKEINTRESOURCE(ordinal)` — no
   string exists for the ordinal form.
3. The IDENTITY of the 204 unattributed owners. They are established to be
   unexported internals; which ones, and whether any is reachable, needs Ghidra
   function boundaries rather than the export table.
4. Nothing outside retail `Runtime-x64.dll` was scanned in-body — the exe and
   video DLLs are covered as import-table door facts only.
