#!/usr/bin/env python3
"""Cross-build body matcher: retail Runtime-x64.dll -> Runtime-x64-profile.dll.

WHAT IT IS FOR
--------------
All 33 address-keyed `internal` census rows are genuinely unexported in RETAIL
(re-derived by tools/int33_export_screen.py, 4/4 controls).  The ToolKit
`Runtime-x64-profile.dll` ships ~5,182 exports against retail's 4,926, so a body
unexported in retail may be EXPORTED (and therefore NAMED) there.

Wave 23 lane INTERNAL recorded this route as unavailable and was precise about
why: *"It is a different build at different addresses ... Matching by opcode hash
across the two builds is the sound method and it is a TOOL THIS LANE DID NOT
HAVE TIME TO BUILD. This is a TODO, not a blocker."*  This is that tool.

HOW IT MATCHES
--------------
Take the retail body's bytes; wildcard every byte belonging to a rel32 branch/
call target or a RIP-relative displacement -- the fields that MUST differ between
two builds at different addresses -- and scan the profile build's .text for the
surviving fixed bytes.  This is the same masked-pattern technique
tools/extptr_family_sweep.py uses within one binary.

THE ANSWER IT GIVES IS NOT GUARANTEED TO BE YES.  A profile build is a DIFFERENT
COMPILE: if it inlines differently, or emits an instrumentation prologue, the
fixed bytes do not survive and the body is unmatchable by this method.  So the
CONTROL is not decorative -- it measures the technique's RECALL on bodies whose
answer is already known, and a low recall means a miss says nothing.

    python tools/int33_profile_match.py            # the 21 .text rows
    python tools/int33_profile_match.py --control  # recall on known-name bodies
    python tools/int33_profile_match.py 0x180028760
"""
import sys

import pefile
from capstone import CS_ARCH_X86, CS_MODE_64, Cs, CS_OP_IMM
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

RETAIL = r"C:\GOG Games\Banished\Runtime-x64.dll"
PROFILE = r"C:\code\GIT\untitled_rev_eng_project\ToolKit\bin\x64\Runtime-x64-profile.dll"

TEXT_ROWS = [
    0x180004590, 0x180005220, 0x180009A20, 0x180010F00, 0x1800269A0,
    0x180028760, 0x18002C6A0, 0x18002F170, 0x1800332D0, 0x180034010,
    0x1800493D0, 0x18004BDE0, 0x180052640, 0x180053F00, 0x180075B20,
    0x180075D20, 0x180078950, 0x180078AD0, 0x18007F050, 0x18007F8A8,
    0x18007FBFC,
]


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(path, fast_load=True)
        self.pe.parse_data_directories(
            [pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.exports = {}
        d = getattr(self.pe, "DIRECTORY_ENTRY_EXPORT", None)
        if d:
            for e in d.symbols:
                if e.name:
                    self.exports[self.base + e.address] = e.name.decode()
        self.text = None
        for s in self.pe.sections:
            if s.Name.rstrip(b"\x00") == b".text":
                self.text = (self.base + s.VirtualAddress, s.get_data())
                break

    def read(self, va, n):
        rva = va - self.base
        for s in self.pe.sections:
            end = s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)
            if s.VirtualAddress <= rva < end:
                return s.get_data(rva, n)
        raise SystemExit("bad va 0x%x" % va)


def _md():
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    return md


def extent(img, va, cap=4096):
    data = img.read(va, cap)
    for i in _md().disasm(data, va):
        if i.mnemonic in ("ret", "jmp"):
            nxt = i.address + i.size - va
            if nxt < len(data) and data[nxt] in (0xCC, 0x90):
                return i.address + i.size - va
            if i.mnemonic == "ret":
                return i.address + i.size - va
    return None


def masked(img, va, n):
    """(bytes, mask) -- mask[i] False means 'wildcard, this byte encodes a distance'."""
    data = bytearray(img.read(va, n))
    mask = bytearray(b"\x01" * n)
    for i in _md().disasm(bytes(data), va):
        off = i.address - va
        if off >= n:
            break
        for op in i.operands:
            if op.type == CS_OP_IMM and (
                i.mnemonic.startswith("j") or i.mnemonic in ("call", "loop")
            ):
                # rel32/rel8 occupies the tail of the instruction
                w = 4 if i.size >= 5 else 1
                for k in range(i.size - w, i.size):
                    mask[off + k] = 0
            elif op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                # disp32 sits immediately before any trailing immediate; wildcard
                # the whole 4-byte window conservatively by scanning for it
                for k in range(max(0, i.size - 8), i.size):
                    mask[off + k] = 0
    return bytes(data), bytes(mask)


def find(img, pat, mask):
    """All .text offsets in img matching the masked pattern. Returns VAs."""
    tbase, tdata = img.text
    fixed = [i for i, m in enumerate(mask) if m]
    if not fixed:
        return []
    # anchor on the longest run of fixed bytes for speed
    runs, cur = [], []
    for i in fixed:
        if cur and i == cur[-1] + 1:
            cur.append(i)
        else:
            if cur:
                runs.append(cur)
            cur = [i]
    if cur:
        runs.append(cur)
    anchor = max(runs, key=len)
    needle = pat[anchor[0]:anchor[-1] + 1]
    hits, pos = [], 0
    n = len(pat)
    while True:
        j = tdata.find(needle, pos)
        if j < 0:
            break
        pos = j + 1
        start = j - anchor[0]
        if start < 0 or start + n > len(tdata):
            continue
        cand = tdata[start:start + n]
        if all(cand[i] == pat[i] for i in fixed):
            hits.append(tbase + start)
    return hits


def match(retail, profile, va, verbose=True):
    n = extent(retail, va)
    if not n:
        return va, None, [], 0
    pat, mask = masked(retail, va, n)
    wild = mask.count(0)
    hits = find(profile, pat, mask)
    names = [(h, profile.exports.get(h)) for h in hits]
    return va, n, names, wild


def report(retail, profile, addrs):
    named = 0
    for va in addrs:
        a, n, hits, wild = match(retail, profile, va)
        if n is None:
            print("  0x%x  <no terminator found>" % a)
            continue
        if not hits:
            print("  0x%x  %4dB (%d wild)  NO MATCH in profile .text" % (a, n, wild))
            continue
        for h, nm in hits:
            print("  0x%x  %4dB (%d wild)  -> profile 0x%x  %s"
                  % (a, n, wild, h, nm or "(matched, but not an export there)"))
            if nm:
                named += 1
    print("\n  rows given a NAME by the profile build: %d of %d" % (named, len(addrs)))
    return named


def control(retail, profile):
    """RECALL: bodies whose name is already known in BOTH builds.

    Without this, "NO MATCH" and "the matcher is broken" print the same thing.
    The subject is deliberately a MIXED bag of sizes, because a matcher can have
    high recall on large bodies and none on small ones.
    """
    print("CONTROL -- recall on bodies exported (and therefore named) in RETAIL")
    subjects = []
    for va, nm in sorted(retail.exports.items()):
        if retail.text[0] <= va < retail.text[0] + len(retail.text[1]):
            n = extent(retail, va)
            if n and n >= 32:
                subjects.append((va, nm, n))
        if len(subjects) >= 60:
            break
    ok = miss = wrongname = 0
    for va, nm, n in subjects:
        _, _, hits, _ = match(retail, profile, va)
        pnames = [x[1] for x in hits if x[1]]
        if not hits:
            miss += 1
        elif nm in pnames:
            ok += 1
        else:
            wrongname += 1
    tot = len(subjects)
    print("  subjects (retail .text exports, >=32B): %d" % tot)
    print("  matched in profile AND same name : %d  (%.1f%% recall)"
          % (ok, 100.0 * ok / tot if tot else 0))
    print("  matched but a DIFFERENT/absent name: %d" % wrongname)
    print("  NO MATCH at all                   : %d" % miss)
    print("\n  READ THIS BEFORE READING ANY 'NO MATCH' ABOVE: a miss is only")
    print("  evidence of absence to the extent this recall is high.")
    return ok, tot


# ---------------------------------------------------------------------------
# SECOND, LOOSER MATCHER: the MNEMONIC SEQUENCE.
#
# The masked-byte matcher above measures at ~10% recall (see control()), which
# is too low for a miss to mean anything.  This one throws away ALL operands and
# compares only the opcode sequence -- Wave 23's "opcode hash", which survives a
# different register allocation and a different constant pool.  It cannot survive
# a different INLINING decision, so it is still not expected to be perfect; the
# same recall control is run against it and the number is what licenses (or
# refuses) any conclusion drawn from a miss.
# ---------------------------------------------------------------------------
def mnemonics(img, va, n):
    return tuple(i.mnemonic for i in _md().disasm(img.read(va, n), va)
                 if i.address - va < n)


def profile_mnemonic_index(profile):
    """{mnemonic-sequence-prefix -> [(va, name)]} over the profile's EXPORTS."""
    idx = {}
    tbase, tdata = profile.text
    for va, nm in profile.exports.items():
        if not (tbase <= va < tbase + len(tdata)):
            continue
        n = extent(profile, va)
        if not n or n < 32:
            continue
        seq = mnemonics(profile, va, n)
        idx.setdefault(seq, []).append((va, nm))
    return idx


def main(argv):
    retail, profile = Image(RETAIL), Image(PROFILE)
    print("retail  : %d exports, .text %d bytes" % (len(retail.exports), len(retail.text[1])))
    print("profile : %d exports, .text %d bytes\n" % (len(profile.exports), len(profile.text[1])))
    if "--population" in argv:
        # ------------------------------------------------------------------
        # THE RECALL-FREE QUESTION, and the one that actually settles this.
        #
        # Both matchers above have a recall ceiling (10% masked-byte, 32%
        # mnemonic), so neither can carry "0 of 21 matched". This screen does not
        # match anything: it asks what the profile build's EXTRA exports ARE. If
        # none of them is the kind of symbol our rows are, then no matcher at any
        # recall could have named them, and the route is closed for a STRUCTURAL
        # reason rather than a tooling one.
        # ------------------------------------------------------------------
        import collections
        import re
        rn = set(retail.exports.values())
        pn = set(profile.exports.values())
        extra = sorted(pn - rn)
        print("retail-only export names : %d" % len(rn - pn))
        print("profile-only export names: %d" % len(extra))
        b, other = collections.Counter(), []
        for n in extra:
            if "SAPEAX_KPEBDH" in n or "SAPEAX_KHPEBDH" in n:
                b["debug operator new/delete taking (file, line)"] += 1
            elif "@Utility@@" in n:
                b["ToolKit-only Utility:: (FBX / mesh / image compile)"] += 1
            elif re.search(r"RSCCompiler|TextReader|TextWriter|ExternalCompiler"
                           r"|CompiledExternal", n):
                b["ToolKit-only resource compiler / text IO"] += 1
            elif re.search(r"@Debug@|DisableAssert|FileChangeThread", n):
                b["ToolKit-only debug / assert / dev helpers"] += 1
            elif re.search(r"MemoryAllocator", n):
                b["allocation TRACING (debug-build allocator)"] += 1
            elif re.search(r"@BinaryReader@IO@@|@BinaryWriter@IO@@|@Visitor@IO@@"
                           r"|@ReaderWriter@IO@@|@Serializer@IO@@", n):
                b["IO codec slots that retail has but does not EXPORT"] += 1
            else:
                b["UNCLASSIFIED"] += 1
                other.append(n)
        for k, v in b.most_common():
            print("  %5d  %s" % (v, k))
        if other:
            print("\n  UNCLASSIFIED (%d):" % len(other))
            for n in other:
                print("     ", n)
        # The only bucket that could ever name one of our rows is one holding
        # anonymous template instantiations. Say so explicitly.
        coll = [n for n in extra if "?$Array@" in n or "?$Collection" in n
                or "ExternalPtr@" in n]
        print("\n  profile-only exports that are Collection/ExternalPtr template")
        print("  instantiations -- the shape 19 of the 21 .text rows are: %d" % len(coll))
        return 0
    if "--control" in argv:
        control(retail, profile)
        return 0
    if "--mnemonic" in argv:
        print("building mnemonic index over the profile build's exports ...")
        idx = profile_mnemonic_index(profile)
        print("  %d distinct sequences from %d indexable exports\n"
              % (len(idx), sum(len(v) for v in idx.values())))
        # RECALL CONTROL FIRST, so no reader reaches the verdict without it.
        subs, ok = 0, 0
        for va, nm in sorted(retail.exports.items()):
            if not (retail.text[0] <= va < retail.text[0] + len(retail.text[1])):
                continue
            n = extent(retail, va)
            if not n or n < 32:
                continue
            subs += 1
            hit = idx.get(mnemonics(retail, va, n))
            if hit and any(h[1] == nm for h in hit):
                ok += 1
            if subs >= 200:
                break
        print("CONTROL (mnemonic): %d/%d retail .text exports recovered "
              "by name = %.1f%% recall" % (ok, subs, 100.0 * ok / max(subs, 1)))
        print()
        named = 0
        for va in TEXT_ROWS:
            n = extent(retail, va)
            hit = idx.get(mnemonics(retail, va, n)) if n else None
            if hit:
                named += 1
                print("  0x%x -> %s" % (va, hit))
            else:
                print("  0x%x  no mnemonic-sequence match among profile exports" % va)
        print("\n  named: %d of %d" % (named, len(TEXT_ROWS)))
        return 0
    addrs = [int(a, 16) for a in argv if a.startswith("0x")] or TEXT_ROWS
    print("SUBJECT: %d retail .text rows" % len(addrs))
    report(retail, profile, addrs)
    print()
    control(retail, profile)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
