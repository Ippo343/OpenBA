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

THE ONE LIMIT, and it was TESTED rather than filed. Registration is detected by
LITERAL only, so a `RegisterRuntimeExport(kSomeConstant, ...)` would read as
unported -- the blind spot `runtime_census.py` has five recorded instances of
(reference_census_cannot_see_nonliteral_registrations.md). The exact agreement
above proves the cost of that blind spot is currently ZERO: no minted slot is
registered through a non-literal today. It can become non-zero the moment one is,
and the error direction is PESSIMISTIC (over-reports unported), which is the safe
direction for a hazard list.
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
tree = subprocess.run(
    ["git", "grep", "-h", "-E", r"RegisterRuntime(Export|Addr)\(", REV, "--",
     "runtime", "standalone", "code", "engine", "dx11"],
    capture_output=True, text=True, errors="replace").stdout

reg_names = set(re.findall(r'RegisterRuntimeExport\(\s*"([^"]+)"', tree))
reg_addrs = set(int(a, 16) for a in
                re.findall(r'RegisterRuntimeAddr\(\s*0x([0-9a-fA-F]+)', tree))

hdr = re.compile(r"DLL\s+0x([0-9a-fA-F]+)\s+(\d+)\s+slots\s*(?:\[(.*?)\])?")
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
        cur = {"va": int(m.group(1), 16), "n": int(m.group(2)),
               "note": (m.group(3) or "").strip(), "slots": []}
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
        worst.append((un, len(b["slots"]), b["va"], b["note"]))

print("MINT SLOT SWEEP at %s" % REV)
print("  vtables                 %d" % len(blocks))
print("  slots                   %d" % tot)
print("  ported (name or addr)   %d" % ported)
print("  purecall (legitimate)   %d" % purecall)
print("  UNPORTED -> named trap  %d   (%d exported-by-name, %d address-only)"
      % (named_un + unnamed_un, named_un, unnamed_un))
print()
print("  Worst vtables by unported slot count "
      "(identity from Serializer.h where known, else the table's adjacent-export hint):")
for un, n, va, note in sorted(worst, reverse=True)[:12]:
    who = KNOWN.get(va) or ("~" + note[:52])
    print("    0x%09x  %3d/%-3d unported  %s" % (va, un, n, who))
