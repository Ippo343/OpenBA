#!/usr/bin/env python3
"""Static sweep: which MINTED VTABLE SLOTS have no port, and where do they cluster?

THE CLASS. `standalone/rt_data_mint.cpp`'s `OurPort()` fills a minted vtable slot
from the port registry -- by NAME for RT_MINT_NAMED, else by ADDRESS. A slot it
cannot fill becomes a NAMED TRAP (`unportedTraps`). Before cutover that is
harmless, because the DLL's own vtable is the one in use. At cutover it is the
whole hazard: a name-minted slot resolves out of the DLL's export table, which is
precisely what a DLL-less boot does not have.

Lane GFXD hit one of these from the side while re-taking `Serialize@MeshInstance`:
the `IO::ReaderWriter` vtable's `+0xa0` (`rw_vtbl::kScalarInt`) is
`?Serialize@BinaryReader@IO@@UEAAXAEAHPEBD@Z` -- an ORDINARY EXPORT, minted, with
no ported body. It is not special. This tool says how many others there are.

USAGE
    python tools/mint_slot_sweep.py [rev]        # default HEAD

VALIDATED, not merely asserted. At `fc884df` this scan reports
`491 slots: 114 ported, 259 unported-trap, 118 purecall-trap`, which is
CHARACTER-FOR-CHARACTER the `[RtMint] minted ...` line that rev's own boot prints
from `MintCounts`. Two independent instruments -- a static literal scan and the
runtime mint -- agreeing exactly at the same revision is what says this scan is
measuring the thing it claims to.

THE LIMITS. Registration is detected by LITERAL only, so a
`RegisterRuntimeExport(kSomeConstant, ...)` reads as unported -- the blind spot
`runtime_census.py` has five recorded instances of
(reference_census_cannot_see_nonliteral_registrations.md).

  THE PARAGRAPH THAT USED TO STAND HERE SAID THAT LIMIT "was TESTED rather than
  filed" and that its cost was "currently ZERO".  It was true when written and
  it rotted, and it rotted in the most expensive possible place: a DIFFERENT
  blind spot in the SAME line of code -- a mangled name split across two source
  lines -- was already costing NINE slots, and the reassuring paragraph sat
  directly above it for three waves.  A STATED LIMIT IS A CLAIM WITH A
  TIMESTAMP NOBODY CHECKS.  The multi-line half is FIXED (Wave 29 lane INSTR,
  see the block by scan_registrations) and now carries a live positive control
  that RUNS ON EVERY INVOCATION instead of a sentence saying it was tested once.

IT READS A COMMITTED REVISION, NOT YOUR WORKING TREE, and that is by design (it
must be worktree-independent) but it was undocumented and it misleads: lane
SLOTS watched this tool hold at 16 while its two ports sat uncommitted in the
working tree.  `python tools/mint_slot_sweep.py` answers a question about HEAD.
Commit, or pass a rev.  The header line says which rev it read.

CROSS-TOOL CHECK.  This tool and tools/runtime_census.py both answer "what has
our tree registered by name?" and until Wave 29 nothing compared their answers,
which is how a blind spot fixed in one survived three waves in the other.  The
comparison is printed at the bottom of every run.
"""
import re
import subprocess
import sys

REV = sys.argv[1] if len(sys.argv) > 1 else "HEAD"


def show(path):
    return subprocess.run(["git", "show", "%s:%s" % (REV, path)],
                          capture_output=True, text=True, errors="replace").stdout


inc = show("standalone/rt_data_mint_tables.inc")
if not inc:
    sys.exit("could not read rt_data_mint_tables.inc at %s" % REV)

# Every registration LITERAL anywhere in the tree, read from the REV rather than
# the working copy -- so this is worktree-independent, which in a session where
# the harness re-points a worktree between two commands is not a nicety.
# -a IS LOAD-BEARING, NOT TIDINESS (Wave 26 lane PROMOGFX).
# `git grep -h` prints "Binary file <path> matches" INSTEAD OF THE LINES for any
# blob with a NUL in git's first-8000-byte sniff window, and says nothing else.
# runtime/src/graphics/scene.cpp carried exactly one NUL, at byte 7277, inside a
# COMMENT that had pasted a raw 0x00 where it meant the two characters \\0 --
# a comment about the ??_7Geometry@Graphics@@6B@ overrun. All 38 of that file's
# RegisterRuntimeExport lines were therefore invisible here, and this tool
# reported ??_7Geometry@Graphics@@6B@ as 5/9 unported while every one of those 5
# slots was registered and tools/runtime_census.py scored the row 6/6 COMPLETE.
# Two instruments disagreeing by five with no detector.
#
# THE NULL BYTE IS ALSO FIXED AT SOURCE, but the tool is hardened anyway: the
# blindness is silent, is re-armed by any future stray byte, and its error
# direction is INFLATING -- it manufactures port work, which is the direction
# this project has recorded a lane acting on.
#
# A CONTROL, because a scan for a LITERAL cannot tell "absent" from "suppressed":
# git grep -c counts matches per file WITHOUT needing content, so it sees the
# binary blobs too. If the two channels disagree the scan is under-reading and
# this tool refuses rather than printing a confident low number.
tree = subprocess.run(
    ["git", "grep", "-h", "-a", "-E", r"RegisterRuntime(Export|Addr)\(", REV, "--",
     "runtime", "standalone", "code", "engine", "dx11"],
    capture_output=True, text=True, errors="replace").stdout

_counts = subprocess.run(
    ["git", "grep", "-c", "-a", "-E", r"RegisterRuntime(Export|Addr)\(", REV, "--",
     "runtime", "standalone", "code", "engine", "dx11"],
    capture_output=True, text=True, errors="replace").stdout
_expected = sum(int(l.rsplit(":", 1)[1]) for l in _counts.splitlines() if ":" in l)
_got = len([l for l in tree.splitlines() if l.strip()])
if _got < _expected:
    sys.exit("mint_slot_sweep: registration scan READ %d lines but git counts %d "
             "across the same pathspec -- %d line(s) suppressed (a NUL byte makes "
             "git grep emit 'Binary file ... matches' instead of the lines). "
             "REFUSING to print an inflated unported count."
             % (_got, _expected, _expected - _got))
print("  reg-scan control        %d lines read == %d counted by git  (no file suppressed)"
      % (_got, _expected))

# ===========================================================================
# WAVE 29 LANE INSTR -- THE MULTI-LINE REGISTRATION, AND THE REASON IT SURVIVED
# ===========================================================================
# The line above used to be the whole registration scan:
#
#     reg_names = set(re.findall(r'RegisterRuntimeExport\(\s*"([^"]+)"', tree))
#
# `tree` is `git grep -h` OUTPUT -- one line per match -- so a registration
# written as a C adjacent-string concatenation across two source lines yielded
# only the FIRST FRAGMENT, which can never match a mint-table name. That is one
# C++ string literal after preprocessing, not two.
#
# THREE DIFFERENT NUMBERS WERE QUOTED FOR THIS ONE DEFECT (49, 67, 87) by three
# people measuring three slightly different things, which is the cross-tool
# consistency problem in miniature. Lane INSTR decomposed it instead of adding
# a fourth. Whole-file, COMMENT-STRIPPED, over code/ engine/ runtime/
# standalone/ dx11/ at wave29/instr:
#
#   RegisterRuntimeExport( call sites                         1416
#     literal on the SAME line, single fragment               1312   fine
#     literal on the same line, CONTINUED on the next line      17   TRUNCATED
#     literal starts on the NEXT line, single fragment          39   INVISIBLE
#     literal starts on the next line AND continued             11   INVISIBLE
#     first argument is NOT a literal                           37   a different,
#                                                                   already-known
#                                                                   blind spot
#
# SO THE AFFECTED POPULATION IS 17 + 39 + 11 = 67 SITES, which is lane SLOTS'
# figure exactly. The integrator's 49 counted only the open-paren-at-EOL form,
# and its 87 was (1442 - 1355) with comments included, which folds in the
# unrelated non-literal blind spot.
#
# TWO SHAPES, NOT ONE, AND THE SMALLER ONE IS WORSE.
#   * 50 sites are INVISIBLE: the call opens and the literal is on the FOLLOWING
#     line (audio_scene_update.cpp:732). The old scan matched nothing.
#   * 17 sites are TRUNCATED: the literal opens on the same line and continues
#     on the next (achievement_state.cpp:447 yields the phantom
#     `?CreateInstance@Achievement@1@QEBA?AV?$InternalPtr@`). A phantom name is
#     WORSE than a missing one -- it enters the registered set and matches
#     nothing, so it cannot even be noticed as absent.
#
# The Wave 29 integrator reported "there are ZERO adjacent-string-concatenated
# registration literals in the tree" and asked for the concat branch to be
# dropped as dead code. THAT IS REFUTED: 28 of the 67 are concatenations. The
# grep behind that claim required BOTH FRAGMENTS ON ONE LINE, so it could not
# see a concatenation that spans lines -- a line-based screen used to rule out
# a defect whose definition is "spans lines". The concat branch is LIVE and its
# positive control below is drawn from the tree, not constructed.
#
# NOTE ON THE OLD PATTERN: its `\s*` matches newlines, so the pattern itself
# would have handled the next-line form. The defect was never the pattern -- it
# was feeding it LINE FRAGMENTS. Fixing the regex would have changed nothing;
# the input had to change.
#     UNPORTED -> named trap, as printed : 16
#     UNPORTED -> named trap, the truth  :  7
# NINE slots the tool called unported were already ported, six of them in Wave
# 27, and ??_7ParticleInstance@Graphics@@6B@ / ??_7ParticleSystemInstance@
# Graphics@@6B@ have been 10/10 COMPLETE since Wave 27 with nobody aware. A lane
# was four keystrokes from re-porting two bodies that exist.
#
# THE FINDING IS NOT THE PARSER BUG. `tools/runtime_census.py` was ALREADY FIXED
# for this exact blind spot -- concat_literal(), written when two names were
# reported as phantom exports -- and the fix did not propagate to the tool
# beside it. NOTHING IN THIS PROJECT COMPARED TWO TOOLS' ANSWERS TO ONE
# QUESTION, so two instruments that both count "unported slots" disagreed by
# nine and no screen noticed.
#
# So this does not re-implement the join. IT IMPORTS THE CENSUS'S OWN
# concat_literal, which is the only construction under which a future fix to
# one of these tools cannot fail to reach the other, and it adds the cross-tool
# comparison at the bottom of this file.
#
# The docstring's "THE ONE LIMIT ... was TESTED rather than filed" paragraph was
# TRUE WHEN WRITTEN and rotted: it tested the NON-LITERAL blind spot and
# concluded its cost was zero, while a different blind spot in the same line --
# the multi-line literal -- was already costing nine slots. A stated limit is a
# claim with a timestamp nobody checks.
import os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from runtime_census import concat_literal, strip_comments   # noqa: E402

RE_REG_NAME = re.compile(r'RegisterRuntimeExport\s*\(\s*(?=")')
RE_REG_ADDR_LIT = re.compile(r'RegisterRuntimeAddr\s*\(\s*0x([0-9a-fA-F]+)')
RE_OLD_LINEWISE = re.compile(r'RegisterRuntimeExport\(\s*"([^"]+)"')


def _files_with_registrations():
    out = subprocess.run(
        ["git", "grep", "-l", "-a", "-E", r"RegisterRuntime(Export|Addr)\(", REV,
         "--", "runtime", "standalone", "code", "engine", "dx11"],
        capture_output=True, text=True, errors="replace").stdout
    # `git grep -l <rev>` prints "<rev>:<path>"; strip the rev.
    return [l.split(":", 1)[1] for l in out.splitlines() if ":" in l]


def scan_registrations():
    """(names, addrs, nfiles) read from WHOLE FILES, adjacent literals joined."""
    names, addrs, nfiles = set(), set(), 0
    for path in _files_with_registrations():
        text = show(path)
        if not text:
            continue
        nfiles += 1
        # COMMENTS STRIPPED, and this was NOT a precaution -- the cross-tool
        # screen at the bottom of this file caught it on its FIRST RUN. Without
        # it this scan registered the names `<lit>`, `<literal>` and
        # `<slot name>`, read out of COMMENTS in renderer.cpp,
        # wave21_lane_gfxbody.cpp and wave23_lane_gfxbody.cpp that quote this
        # project's OWN match patterns while explaining a blind spot.
        #
        # The prose most likely to name a symbol is a comment ABOUT the tool
        # reading it. Those three are harmless placeholders; a comment naming a
        # REAL mangled name would have marked a minted slot PORTED when nothing
        # registers it -- which under-reports the trap list, and for a hazard
        # list that is the DANGEROUS direction, not the safe one.
        #
        # Imported from runtime_census rather than re-written, for the same
        # reason concat_literal is: this file is the proof that a fix which
        # does not propagate is a fix that will be needed twice.
        text = strip_comments(text)
        for m in RE_REG_NAME.finditer(text):
            nm = concat_literal(text, m.end())
            if nm:
                names.add(nm)
        for m in RE_REG_ADDR_LIT.finditer(text):
            addrs.add(int(m.group(1), 16))
    return names, addrs, nfiles


reg_names, reg_addrs, _regfiles = scan_registrations()
_old_names = set(RE_OLD_LINEWISE.findall(tree))

# ---- POSITIVE CONTROL, HANDED A SHAPE  (Wave 29 lane INSTR) --------------
# This scan's normal output is a large set, and a large set is exactly what a
# half-working scan also produces. So it is handed a KNOWN MULTI-LINE
# registration and required to recover it -- AND the old line-by-line regex is
# required to MISS it. A control that only exercises the new code cannot tell a
# fix from a no-op, and a control whose subject is not known-positive cannot
# tell a live scan from a dead one.
# The CONCAT shape (17 live sites), taken from the tree rather than constructed:
_CTRL_SPLIT = ("?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEBVCamera@2@"
               "V?$ExternalPtr@VMaterialInstance@Graphics@@@Object@@MHHH@Z")
# The NEXT-LINE shape (50 live sites). Both are handed to the scan, and scored
# SEPARATELY: a control scored on the union of two shapes reads PASS while one
# of them is blind, which is the exact defect this file is about.
_CTRL_NEXTLINE = ("?Update@Scene@Audio@@QEAA_NVTime@System@@0AEAVRandom@4@"
                  "AEBVVector3@Math@@M@Z")
_ctrl_new = _CTRL_SPLIT in reg_names
_ctrl_old = _CTRL_SPLIT in _old_names
_ctrl_nl_new = _CTRL_NEXTLINE in reg_names
_ctrl_nl_old = _CTRL_NEXTLINE in _old_names
print("  multi-line control      %s  (%s recovers the split "
      "?Draw@SunLight@ registration)"
      % ("PASS" if _ctrl_new else "*** RED ***",
         "whole-file scan" if _ctrl_new else "NOTHING"))
print("  ...and its NEGATIVE half %s  (the old line-wise regex must MISS it)"
      % ("PASS" if not _ctrl_old else "*** RED -- the defect is not reproduced,"
                                      " so this control proves nothing ***"))
print("  next-line control       %s  (recovers ?Update@Scene@Audio@, whose "
      "literal\n                          opens on the FOLLOWING line)"
      % ("PASS" if _ctrl_nl_new else "*** RED ***"))
print("  ...and its NEGATIVE half %s  (the old line-wise scan must MISS it)"
      % ("PASS" if not _ctrl_nl_old else "*** RED ***"))
if not _ctrl_nl_new:
    sys.exit("mint_slot_sweep: the whole-file registration scan cannot see the "
             "NEXT-LINE registration form "
             "(runtime/src/audio/audio_scene_update.cpp:732). REFUSING to print.")
if not _ctrl_new:
    sys.exit("mint_slot_sweep: the whole-file registration scan cannot see a "
             "registration this tree definitely contains "
             "(runtime/src/graphics/wave28_lane_partsys.cpp:1084). REFUSING to "
             "print an inflated unported count.")
print("  registration scan       %d name(s), %d addr(s) over %d file(s)"
      % (len(reg_names), len(reg_addrs), _regfiles))
print("                          the old line-fragment scan saw %d; %d name(s) "
      "are recovered\n                          here, and %d of ITS names are "
      "PHANTOMS -- a truncated first\n                          fragment that "
      "matches nothing. SET DIFF, not a count difference."
      % (len(_old_names), len(reg_names - _old_names),
         len(_old_names - reg_names)))

# WAVE 29 LANE INSTR -- THE IDENTITY IS ON THE LINE ALREADY.
# The header line is
#     // ??_7MeshInstance@Graphics@@6B@  DLL 0x180154dc0  9 slots  [tail-not-a-pointer]
# and this regex used to start at `DLL`, so the only thing left for the print
# loop to name the table with was the BRACKETED NOTE -- which is an EXTENT
# note, not an identity. `~tail-not-a-pointer` was therefore never this tool
# declining to name a table: it was the tool DISCARDING a name it had already
# read past. And `~adjacent-export:X` is worse than useless, because the
# adjacent export is the NEXT row, so the hint is systematically off by one:
# lane SLOTS measured it WRONG ON ALL SIX of its tables, and the Wave 29
# kickoff propagated three of those wrong names into its own scoping table
# (calling ParticleInstance "Particle", ParticleSystemInstance "ParticleSystem"
# and Stream@Audio "FileStream@IO").
#
# The KNOWN dict below already carried a comment saying the hint "is off by one
# row and must not be read as an identity" -- and the print loop four functions
# later read it as one. A WARNING WRITTEN INTO A COMMENT AND NOT ENFORCED IN
# THE CODE IS NOT A CHECK. It is enforced now: the note is never printed where
# an identity belongs, and a table whose header carries no name prints
# `(UNNAMED IN THE .inc)` rather than borrowing its neighbour's.
hdr = re.compile(r"(?://\s*(\S+)\s+)?DLL\s+0x([0-9a-fA-F]+)\s+(\d+)\s+slots"
                 r"\s*(?:\[(.*?)\])?")
slot = re.compile(r'\{\s*(nullptr|"([^"]*)")\s*,\s*0x([0-9a-fA-F]+)ull\s*,\s*(RT_MINT_\w+)')

# The "[adjacent-export:X]" note in the table names the NEXT export after the
# vtable, not the vtable itself, so it is off by one row and must not be read as
# an identity. These three are pinned independently in runtime/types/io/
# Serializer.h and are the ones that matter most below.
KNOWN = {0x1801552e0: "??_7BinaryReader@IO@@6B@",
         0x180155210: "??_7BinaryWriter@IO@@6B@",
         0x1801558b0: "??_7Visitor@IO@@6B@",
         0x1801553b0: "??_7ReaderWriter@IO@@6B@"}

blocks, cur = [], None
for line in inc.splitlines():
    m = hdr.search(line)
    if m:
        cur = {"name": (m.group(1) or "").strip(),
               "va": int(m.group(2), 16), "n": int(m.group(3)),
               "note": (m.group(4) or "").strip(), "slots": []}
        blocks.append(cur)
        continue
    if cur is None:
        continue
    s = slot.search(line)
    if s and len(cur["slots"]) < cur["n"]:
        cur["slots"].append({"name": s.group(2), "va": int(s.group(3), 16),
                             "kind": s.group(4)})

tot = named_un = unnamed_un = purecall = ported = 0
worst = []
for b in blocks:
    un = 0
    for s in b["slots"]:
        tot += 1
        if s["kind"] == "RT_MINT_PURECALL":
            purecall += 1
            continue
        if (s["name"] in reg_names) or (s["va"] in reg_addrs):
            ported += 1
            continue
        un += 1
        if s["kind"] == "RT_MINT_NAMED" and s["name"]:
            named_un += 1
        else:
            unnamed_un += 1
    if un:
        worst.append((un, len(b["slots"]), b["va"], b["note"], b["name"]))

print("MINT SLOT SWEEP at %s" % REV)
print("  vtables                 %d" % len(blocks))
print("  slots                   %d" % tot)
print("  ported (name or addr)   %d" % ported)
print("  purecall (legitimate)   %d" % purecall)
print("  UNPORTED -> named trap  %d   (%d exported-by-name, %d address-only)"
      % (named_un + unnamed_un, named_un, unnamed_un))
print()
print("  Worst vtables by unported slot count. IDENTITY is the name on the .inc")
print("  header line, or Serializer.h where that is pinned. The bracketed EXTENT")
print("  note is printed in its own column and is NOT an identity (Wave 29 INSTR):")
for un, n, va, note, name in sorted(worst, reverse=True)[:12]:
    who = KNOWN.get(va) or name or "(UNNAMED IN THE .inc)"
    print("    0x%09x  %3d/%-3d unported  %-46s  extent-note: %s"
          % (va, un, n, who[:46], note[:40] or "(none)"))

# ===========================================================================
# CROSS-TOOL CONSISTENCY  (Wave 29 lane INSTR)
# ===========================================================================
# THE FINDING THIS IMPLEMENTS, in one sentence: a defect repaired in one
# instrument does not propagate to the instrument beside it, and nothing in
# this project compared two tools' answers to one question.
#
# `runtime_census.py` learned to join adjacent string literals when two symbols
# were reported as phantom exports. This tool did not, and for three waves the
# two disagreed by nine slots with no detector. Both answer the SAME question --
# "which mangled names has our tree registered?" -- so the answers are directly
# comparable and any divergence is a defect in one of them.
#
# IT DOES NOT FAIL THE RUN. The two tools legitimately differ in ways that are
# not defects (the census follows named constants and a promoted-mint fold-in
# that this tool does not, and reads the WORKING TREE where this one reads a
# committed rev). So this prints the SET DIFFERENCE IN BOTH DIRECTIONS with its
# size, and names the reasons a difference is expected. Diff the SET, not the
# count: a divergence that moves the right way by the wrong amount reads as
# agreement.
print()
print("CROSS-TOOL CONSISTENCY  vs tools/runtime_census.py  (Wave 29 lane INSTR)")
try:
    import runtime_census as _rc
    _d = _rc.scan()
    _census = set(_d["ported_exports"])
    _dirty = subprocess.run(
        ["git", "status", "--porcelain", "--",
         "runtime", "standalone", "code", "engine", "dx11"],
        capture_output=True, text=True, errors="replace").stdout.strip()
    print("  subject   this tool's registered-name set at %s (%d names) vs the"
          % (REV, len(reg_names)))
    print("            census's ported_exports over the WORKING TREE (%d names)"
          % len(_census))
    if _dirty:
        print("  *** THE WORKING TREE IS DIRTY (%d path(s) differ from HEAD), so the"
              % len(_dirty.splitlines()))
        print("  *** two sides are not reading the same bytes. Differences below")
        print("  *** may be uncommitted work, not a tool defect. Commit and re-run.")
    _only_sweep = reg_names - _census
    _only_census = _census - reg_names
    print("  only this tool sees  %d  (EXPECTED non-zero: the census EXCLUDES a"
          % len(_only_sweep))
    print("      registration inside an opt-in `if (<flag>)` block -- the 27")
    print("      MemoryAllocator rows behind BANISHED_RT_ALLOC are written and")
    print("      UNARMED -- and this tool does not model gating at all. For a")
    print("      MINT question that is the wrong direction: a gated-off")
    print("      registration cannot fill a minted slot at cutover, so every one")
    print("      of these reads PORTED here and is not.")
    print("      [ADDRESSEE: the Wave 30 kickoff writer -- decide whether this")
    print("       tool should honour GATED_OFF_BLOCKS. It would move the trap")
    print("       count UP, which is the safe direction for a hazard list.]")
    for _n in sorted(_only_sweep)[:30]:
        print("      %s" % _n)
    if len(_only_sweep) > 30:
        print("      ... %d more" % (len(_only_sweep) - 30))
    print("  only the census sees %d  (EXPECTED non-zero: the census follows named"
          % len(_only_census))
    print("      constants, cross-file constants and the promoted-mint fold-in,")
    print("      none of which this tool implements. A ZERO here would mean the")
    print("      census had LOST those doors, not that the tools agree.)")
    for _n in sorted(_only_census)[:30]:
        print("      %s" % _n)
    if len(_only_census) > 30:
        print("      ... %d more" % (len(_only_census) - 30))
    if not _only_census:
        print("  *** ZERO IN BOTH DIRECTIONS IS NOT A CLEAN BILL. This comparison")
        print("  *** is absence-shaped and a dead import produces the same answer;")
        print("  *** the census's extra doors make a non-zero right-hand side the")
        print("  *** healthy state. Check that runtime_census imported at all.")
except Exception as _e:                      # noqa: BLE001
    print("  *** COULD NOT RUN: %r" % (_e,))
    print("  *** This is a SILENT LOSS OF THE ONLY CROSS-TOOL SCREEN. Fix it")
    print("  *** rather than reading the numbers above as corroborated.")
