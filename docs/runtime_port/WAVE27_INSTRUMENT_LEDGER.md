# WAVE 27 — INSTRUMENT DEFECT LEDGER

**Maintained by the integrator, DURING the wave, not after it.**

Wave 27 forbade every lane from fixing tooling (KICKOFF §4 warning #1). That rule
is only defensible if the defects lanes find are *collected* rather than lost, so
this file is the collection point. **Nothing here has been fixed.** Each entry is
a Wave 28 input.

A defect's **DIRECTION** is recorded because it decides urgency, and the project
has a standing finding about it: an **INFLATING** blind spot manufactures work and
is self-limiting (someone chases a false positive and discovers it); a
**DEFLATING** one makes work look impossible, produces promotions that never
happen, and **nothing detects it**. Deflating entries are the dangerous ones.

| # | instrument | direction | reported by | status |
|---|---|---|---|---|
| 1 | GhidraMCP program selection | **WRONG SOURCE** | FRONTIER | open |
| 2 | `list_segments` section bounds | **WRONG SOURCE** | FRONTIER | open |
| 3 | `rt_deleting_dtor_verify.py` scalar-branch check | INFLATING | GFXMESH | open, corrected in-source |
| 4 | **`disassemble_function` serves the WRONG FUNCTION** | **WRONG SUBJECT** | GFXBIG | open, workaround in force |
| 5 | `runtime_census.py --csv` column name | cosmetic | GFXBIG | open |

---

## 1. GhidraMCP serves the PROFILE DLL while reporting retail — FRONTIER

Three status calls disagree and none announces it:
- `list_open_programs` -> `Runtime-x64.dll`, `is_current: true`
- `get_metadata` -> `Runtime-x64-profile.dll`, **at the same moment**
- `switch_program` -> `{"success": true}` while changing **nothing**

Every disassembly in FRONTIER's first hour came from the profile build. Caught by
an unrelated accident, not by a planned check.

**ONE-CALL DISCRIMINATOR:** `disassemble_function(0x180002190)` — 2 instructions
in RETAIL (`MOV RAX,[0x1801fae10] ; RET`, `?TypeName@StringTable@UI@@`), ~40 in
the PROFILE build.

**WORKAROUND IN FORCE FOR THE REST OF THE WAVE:** pass
`program="Runtime-x64.dll"` explicitly on every call. Do not trust
`switch_program`'s return.

**IT IS SESSION-SCOPED, NOT GLOBAL — and that is the part that makes it nasty.**
GFXMESH ran the discriminator both ways (with `program=` explicit and with the
parameter omitted) and got retail both times, on the same machine, while FRONTIER
was being served the profile build. **So a lane cannot infer its own state from
another lane's result, and "I never touched `switch_program`" is not a defence.**
Every session must run the discriminator itself.

**Why it is not cosmetic:** the profile DLL is the ToolKit build WITH PROFILING
HOOKS, and vtable offsets are measured to differ between builds (IO Encoding
+0x78/98/a0 retail vs +0x90/b0/b8 profile). A slot index from the wrong build
produces a body that compiles, registers, dispatches and calls the wrong function.

**Mitigating fact, and it is LUCK not a control:** FRONTIER re-took all five
affected bodies with `program=` explicit; all were byte-identical to the profile
listings, so no number shipped wrong.

## 2. `list_segments` contradicts the retail PE — FRONTIER

Reports `.text: 180001000-180198bff`, `.rdata: 180199000-1802517ff`. The retail
PE headers say `.text` RVA `0x1000` size `0x1495f1` (ends `0x18014a5f1`), `.rdata`
RVA `0x14b000` size `0xaaa7f`. On the reported bounds the `.rdata` vtable
`0x180154630` is "in `.text`". Downstream of #1, but worth its own line: a
section-bounds answer is exactly the kind of thing a reader takes as ground truth
without a second source. **Take section bounds from the PE.**

## 3. `rt_deleting_dtor_verify.py` prints BAD for correct rows — GFXMESH

`tools/rt_deleting_dtor_verify.py:260-273`. The scalar-branch check knows exactly
**two** shapes (calls `~Class`; or inlines a `~Class` that begins with a vtable
restamp) and its third arm is a bare `else`. `Graphics::EdgeMeshInstance` and
`Graphics::MeshInstance` are a **third** shape — the scalar branch inlines a
`~Class` whose own body has no restamp at all.

**Direction: INFLATING.** 2 false positives, 0 false negatives, both rows
hand-verified instruction by instruction.

**Already handled the right way:** GFXMESH wrote the correction *beside the rows*
in `deleting_dtor.cpp`, so the integrator meets it before meeting the tool's
verdict. That is KICKOFF §4 #9 (put a correction on the surface the next person
will REACH) applied without being asked.

---

## A NON-GHIDRA WITNESS EXISTS AND ONE LANE USED IT — worth generalising

GFXMESH's lane was cheap to clear against #1 for a reason other lanes can copy:

1. **Every export VA was confirmed with `pefile` against the retail file on
   disk, never through Ghidra.** That whole class of constant was never exposed
   to the defect.
2. **The deleting-destructor size and `~Class` constants are re-derived from the
   retail PE image** by `rt_deleting_dtor_verify.py`, which reads the file
   directly — an independent witness that already agreed.

**The generalisable rule: a constant that can be read from the PE should be read
from the PE.** Ghidra is the documentation tool (CLAUDE.md), not the only source
of truth for addresses and sizes. Two sources that share no mechanism are two
sources; two Ghidra calls are one — see
`reference_two_checks_one_hidden_premise.md`.

---

## 4. `disassemble_function` silently serves a DIFFERENT function — GFXBIG

**This is the most dangerous entry in the ledger.** Two overlapping bodies at
`0x1800fdb20`; the tool returned the wrong one under the requested address, with
no warning. The listing was **internally consistent** and **relabelled with the
address that was asked for**.

**IT DEFEATS THE #1 DISCRIMINATOR.** Passing `program="Runtime-x64.dll"` does not
help: you can be reading the correct binary and still be handed the wrong
function out of it. #1 had a tell; this has none. It is also worse than the known
`get_function_metrics` defect (ignores `address=`, falls back to a NAME lookup),
because that one produces something recognisably about another symbol.

**Worst where bodies are near-identical siblings** — template instantiations,
paired classes, stream classes sharing a slot layout. A sibling's body reads as
correct *because it nearly is*. Those are exactly Wave 27's lanes: LOOPS
(`DialogLoopT`/`DialogLoaderLoopT`), ACHIEVE (`Achievement`/`AchievementInstance`,
`Goal`/`GoalInstance`), IOTEXT (`TextFileStream`/`PackageFile`).

**WORKAROUND IN FORCE, verified by the integrator rather than relayed unchecked:**

```
objdump -d --start-address=0x180002190 --stop-address=0x1800021a0         "C:/GOG Games/Banished/Runtime-x64.dll"
  -> mov 0x1f8c79(%rip),%rax   # 0x1801fae10
     ret
```

That is FRONTIER's retail discriminator reproduced through a **completely
different mechanism**. objdump addresses by TRUE VA (image base `0x180000000`),
so the address typed is the address disassembled, and it reads the retail file by
explicit path, so #1 cannot apply either. Broadcast to all five running lanes.

## 5. `runtime_census.py --csv` writes `key`, not `symbol` — GFXBIG

Cosmetic, but it costs a lane one failed `awk` before it notices. Logged so the
Wave 28 fix is one line rather than a rediscovery.

---

## THE PATTERN ACROSS #1, #2, #4 — three defects, ONE tool, one root shape

All three are GhidraMCP, and all three have the same structure: **the tool
answers a question about a SUBJECT it has not verified is the subject you named**
— wrong program (#1), wrong section bounds (#2), wrong function (#4). That is the
project's existing finding `reference_a_screen_that_cannot_name_its_subject.md`,
now with a third instance and a new severity: #4 produces no anomaly at all.

CLAUDE.md says Ghidra is the project's primary DOCUMENTATION tool. It is not a
reliable oracle for addresses, extents or bodies. **The operating rule for the
rest of this wave, and a Wave 28 doctrine candidate:**

> Ghidra for names, comments, types and navigation.
> `objdump` and `pefile` against the retail file on disk for any BYTE, ADDRESS,
> EXTENT or BODY that a port depends on.

Two sources that share no mechanism are two sources; two Ghidra calls are one.
