#!/usr/bin/env python3
"""Wave 9 lane UIC: byte-diff Runtime bodies against a body this tree already
ports, and resolve every differing offset back to the ABSOLUTE operand it
encodes -- so "one body serves them all" is a measurement, not an impression.
The same argument WAVE9_LANE_UIF.md §3.4 made for the twelfth J1 address.

The displacement fields are LOCATED BY PATTERN rather than hardcoded: a
hardcoded offset table is exactly the kind of input that silently decodes the
wrong four bytes and then reports a confident mismatch (this script's own first
version did that and called two identical bodies different).
"""
import sys

# 115-byte bodies, read with Ghidra read_memory.
REF_ADDR = 0x180033960   # registered J1 (runtime/src/collection/array.cpp)
REF = "40574883ec3048c7442420feffffff48895c24484889742450488bf9488b314885f67436488b1d8d8d1c004881c34006000048895c2440488bcbff15f079110090488bd6488b0d6d8d1c00e85032090090488bcbff15ce79110033c048890748894708488b5c2448488b7424504883c4305fc3"
CANDS = [
    (0x180009460, "40574883ec3048c7442420feffffff48895c24484889742450488bf9488b314885f67436488b1d8d321f004881c34006000048895c2440488bcbff15f01e140090488bd6488b0d6d321f00e850d70b0090488bcbff15ce1e140033c048890748894708488b5c2448488b7424504883c4305fc3"),
    (0x180039440, "40574883ec3048c7442420feffffff48895c24484889742450488bf9488b314885f67436488b1dad321c004881c34006000048895c2440488bcbff15101f110090488bd6488b0d8d321c00e870d7080090488bcbff15ee1e110033c048890748894708488b5c2448488b7424504883c4305fc3"),
]

# (byte pattern that introduces a 4-byte displacement, human name).
# Order matters only for the report; each is searched for independently and
# every occurrence is taken.
PATTERNS = [
    (bytes.fromhex("488b1d"), "RIP-rel  MOV RBX,[MemoryAllocator::_singleton]"),
    (bytes.fromhex("488b0d"), "RIP-rel  MOV RCX,[MemoryAllocator::_singleton]"),
    (bytes.fromhex("ff15"),   "RIP-rel  CALL [EnterCriticalSection/LeaveCriticalSection]"),
    (bytes.fromhex("e8"),     "rel32    CALL MemoryAllocator::Free"),
]


def fields(b):
    """[(disp_off, next_off, name)] for every relocatable operand in the body."""
    out = []
    for pat, name in PATTERNS:
        start = 0
        while True:
            i = b.find(pat, start)
            if i < 0:
                break
            start = i + 1
            # `e8` is one byte and appears inside other encodings; only accept
            # it where the 5 bytes decode to a call landing inside the DLL.
            if pat == b"\xe8":
                if i + 5 > len(b):
                    continue
                if not (0 <= i and b[i - 1] in (0x00, 0x90) if i else False):
                    pass
            out.append((i + len(pat), i + len(pat) + 4, name))
    out.sort()
    return out


def decode(base, b):
    res = []
    for disp_off, next_off, name in fields(b):
        d = int.from_bytes(b[disp_off:disp_off + 4], "little", signed=True)
        res.append((disp_off, name, base + next_off + d))
    return res


rb = bytes.fromhex(REF)
ref = decode(REF_ADDR, rb)
print("reference 0x%x  (%d bytes, %d relocatable operands)" % (REF_ADDR, len(rb), len(ref)))
for off, name, tgt in ref:
    print("    +0x%02x  %-58s -> 0x%x" % (off, name, tgt))
print()

ok = True
for cbase, chex in CANDS:
    cb = bytes.fromhex(chex)
    cand = decode(cbase, cb)
    diffs = [i for i in range(min(len(rb), len(cb))) if rb[i] != cb[i]]
    inside = set()
    for off, _, _ in cand:
        inside |= set(range(off, off + 4))
    stray = [i for i in diffs if i not in inside]
    same_targets = ([t for _, _, t in ref] == [t for _, _, t in cand]) and \
                   ([o for o, _, _ in ref] == [o for o, _, _ in cand])
    print("candidate 0x%x: %d bytes, %d differ, %d OUTSIDE a displacement"
          % (cbase, len(cb), len(diffs), len(stray)))
    print("    differing offsets: %s" % diffs)
    for (o, name, tgt), (_, _, rtgt) in zip(cand, ref):
        mark = "OK " if tgt == rtgt else "***"
        print("    %s +0x%02x %-54s -> 0x%x" % (mark, o, name, tgt))
    verdict = (len(rb) == len(cb)) and not stray and same_targets
    print("    VERDICT: %s\n" % ("SAME BODY" if verdict else "NOT THE SAME BODY"))
    ok = ok and verdict

sys.exit(0 if ok else 1)
