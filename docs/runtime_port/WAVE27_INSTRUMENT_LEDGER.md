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
