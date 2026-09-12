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
| 3 | `rt_deleting_dtor_verify.py` scalar-branch check | INFLATING | GFXMESH **+ GFXPART, independently** | open, corrected in-source twice |
| 4 | **`disassemble_function` serves the WRONG FUNCTION** | **WRONG SUBJECT** | GFXBIG | open, workaround in force |
| 5 | `runtime_census.py --csv` column name | cosmetic | GFXBIG | open |
| 6 | **WAVE26_CLOSED.md §2 item 6 is STALE** | **DOC, propagated by the integrator** | IOTEXT | corrected here |
| 7 | `dialog.cpp:1471` phantom-address comment (since Wave 9) | INFLATING | IOTEXT | fixed in-source by IOTEXT |
| 8 | **`addr_door_audit.py:47` blind to a wrapped address** | **DEFLATING** | GFXMESH | open — hid a door for 21 waves |
| 9 | `get_function_metrics` ignores `address=` | WRONG SUBJECT | GFXMESH (known) | open, pre-existing |

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
will REACH) applied without being asked. GFXPART did the same for its row,
naming the exact message the tool prints.

### 3a. THE INTEGRATOR-ONLY FINDING: TWO lanes hit that bare `else` with DIFFERENT shapes

Neither lane could see this, and it is the whole reason this ledger is kept
during the wave rather than assembled at merge time.

- **GFXMESH**, on `Graphics::EdgeMeshInstance` and `Graphics::MeshInstance`: the
  scalar branch inlines a `~Class` **whose own body has no restamp at all**.
- **GFXPART**, on `Graphics::Light`: the scalar branch is **devirtualised to
  `~Geometry`** — a different mechanism entirely.

Both lanes described their own finding as "a **third** shape". **They are a third
and a fourth**, and each lane, reasoning correctly from what it could see,
under-reported the defect's size by one. So the tool does not have one missing
arm; its shape enumeration is **open-ended**, and two of the four known shapes
were discovered on the same afternoon by lanes that never spoke.

**Consequence for the Wave 28 fix: do NOT fix this by adding a third arm.** The
evidence says a closed enumeration is the wrong model for that check. Either the
check verifies the *property* (does the scalar branch destroy the object exactly
once) rather than matching shapes, or it reports UNRECOGNISED SHAPE instead of
`BAD` — the direction distinction this ledger opens with. As written, its bare
`else` converts "I have never seen this" into "this is wrong", which is the
`reference_a_screen_that_cannot_name_its_subject.md` family again.

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

**Independently corroborated by GFXPART**, which ran the #1 discriminator on both
call paths and then re-confirmed **all ten** of its load-bearing regions with
`objdump` against the retail file — no discrepancy. Its own conclusion, reached
without seeing this ledger: *"two of three defects are in `disassemble_function`
and neither is visible in its output — tell decompilation lanes to use objdump as
the primary witness."* That is the rule below, arrived at from the other end.

CLAUDE.md says Ghidra is the project's primary DOCUMENTATION tool. It is not a
reliable oracle for addresses, extents or bodies. **The operating rule for the
rest of this wave, and a Wave 28 doctrine candidate:**

> Ghidra for names, comments, types and navigation.
> `objdump` and `pefile` against the retail file on disk for any BYTE, ADDRESS,
> EXTENT or BODY that a port depends on.

Two sources that share no mechanism are two sources; two Ghidra calls are one.

---

## 6. A STALE FACT IN A CLOSING DOC, AND THE INTEGRATOR PROPAGATED IT — IOTEXT

`WAVE26_CLOSED.md` §2 item 6 states that two of three vptr checks in
`file_stream_probe.cpp` "have been structurally false since Waves 19/20". **They
were already FIXED** — by Wave 26 lane PROMOIO at `d668f883`; all three checks now
use `RtResolveExportForProbe`. IOTEXT verified this by reading the file.

**The integrator copied that stale claim into IOTEXT's lane brief**, complete with
an instruction not to repair the probe. The brief was wrong about the state of the
tree at the moment it was written.

**This is the documented failure mode, committed by the person who had just
written the warning against it** — `reference_cheap_wins_list_is_the_stalest_section.md`,
and Wave 26's own closing correction about filtering a table through
`RT_UNPROMOTABLE` *before* it reaches a brief. The parallel is exact: a
five-wave-old tag went unconsulted there; a one-wave-old fix went unconsulted
here. **The lesson does not transfer by being written down.**

**What limited the damage** was hedging rather than rigour: the brief said *"note
that you re-confirmed it, or that you found it already fixed"*, which gave the
lane a licence to contradict it. That is luck about phrasing, not a control.
**The control would have been one `git log` on the file.**

**ADDRESSEE: whoever writes the Wave 28 kickoff.** Any claim about the state of a
FILE that is copied from a closing doc must be re-read from the file. A closing
doc records what was true at seal time, and a lane merged *after* the section was
drafted can falsify it inside the same wave.

## 7. `dialog.cpp:1471` asserted a ported address that never existed — IOTEXT

Since Wave 9 the comment claimed `0x180035350` was "one of the 37 lane K ported".
It was not. **The address existed only in that comment and in the mint table (as
the trap).** Three further `(NOT ported)` comments in `deleting_dtor.cpp` were
also stale. All corrected in-source by IOTEXT.

Same family as `reference_uncommented_checker_manufactures_work.md`: prose that
names an address gets read as evidence about code. A comment is not a registration.

---

## A QUESTION THIS WAVE CLOSED BY MEASUREMENT

The standing warning that **IO Encoding vtable offsets differ between the retail
and profile builds** (`+0x78/98/a0` vs `+0x90/b0/b8`) was the wave's sharpest
retail/profile hazard, and it sat in exactly the namespace defect #1 could have
corrupted. IOTEXT settled it **at the raw-byte level, from the retail file on
disk**:

```
ff 90 a0 00 00 00    kScalarInt   -> +0xa0   (NOT +0xb8)
ff 50 78             kScalarBool  -> +0x78   (NOT +0x90)
```

**Every `ReaderWriter` slot the tree carries is the RETAIL offset.** That is a
measurement against the shipped bytes, not an argument from which program Ghidra
had open — so it holds regardless of defect #1, which is what makes it worth
recording as closed rather than as reassurance.

---

## 8. `addr_door_audit.py:47` cannot see an address passed to a WRAPPER — GFXMESH

**DEFLATING, and it hid a real cutover obligation for twenty-one waves.**

The audit matches a literal address at a resolve call. It is blind to the same
address handed to a *wrapper* function. GFXMESH noticed only because respelling
its own door changed the audit's answer — and that respelling exposed that

> **`mesh.cpp:201` has held an UNREGISTERED address door at `0x1800136f0` since
> Wave 6.**

Invisible for the whole of its existence. Nothing was wrong with the code; the
screen simply could not see it, and a clean audit was read as "no doors here".

This is the **sixth or seventh** instance of one family —
`reference_literal_grep_blind_to_variables.md`, `reference_census_cannot_see_the_macro_doors.md`,
`reference_census_cannot_see_nonliteral_registrations.md` — and the family's
defining property is again on display: **it never announces the blindness.**

**Direction matters here more than anywhere else in this ledger.** Entries 3 and 7
INFLATE: someone chases a false positive and finds the truth. This one DEFLATES —
it reports that work does not exist. Nothing detects that except an accident, and
this time the accident was 21 waves late. **Wave 28 must treat a DEFLATING screen
as unproven until it is handed a known positive** —
`reference_control_must_be_handed_a_known_positive.md`.

**The door itself is now a cutover obligation** — see the merge checklist.

## 9. `get_function_metrics` ignores `address=` — GFXMESH (pre-existing)

Silently falls back to a NAME lookup. Already recorded as
`reference_ghidra_mcp_address_param_silently_ignored.md`; re-confirmed this wave.
Listed only so the Ghidra count in this ledger is honest: **four** GhidraMCP
defects were live during Wave 27 (#1, #2, #4, #9), and all four are the same shape
— the tool answers about a subject it has not verified is the one you named.

---

## A LANE CAUGHT A DEFECT IN ITS OWN WORK, AND THE DETECTOR GENERALISES

GFXMESH registered its 14th body as `UEBA` — by analogy with the virtual slots
beside it — when it is a public **non-virtual const** member, `QEBA`. A phantom
name: `ResolveExport` returns NULL silently and the `ResolveAddr` fallback hides
it (`reference_resolveexport_phantom_names.md`).

**How it was caught, and this is the reusable part:**

> **A registration that does not move the census is a registration of a name that
> does not exist.**

That is a free, universal check on every port in the project, it requires no new
tool, and it caught an error that reading the code did not. It belongs in the Wave
28 kickoff as a step, not as a lesson. Note what it implies about the reverse
case too: the lane had to *attribute* its census delta to notice a row missing
from it — an unexplained `called` movement is a finding, not noise.
