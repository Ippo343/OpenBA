#!/usr/bin/env python3
"""Normalize two Ghidra listings and diff the instruction text only.

Ghidra resolves rel32/RIP-relative operands to ABSOLUTE addresses in the
listing, so a text diff after (a) dropping the leading instruction address and
(b) rewriting INTERNAL branch targets as body-relative deltas is exactly the
"resolve every difference to its absolute target first" check
(reference_position_only_diffs_hide_one_body.md).  Any operand naming an
address OUTSIDE the body is left absolute and therefore must match exactly.
"""
import re
import sys


def load(path):
    txt = open(path, encoding='utf-8').read()
    parts = re.split(r'(?=\b1[0-9a-f]{8}:\s)', txt.strip())
    out = []
    for p in parts:
        p = p.strip()
        if not p:
            continue
        m = re.match(r'^([0-9a-f]+):\s*(.*)$', p, re.S)
        assert m, p
        out.append((int(m.group(1), 16), ' '.join(m.group(2).split())))
    return out


def norm(rows):
    lo = rows[0][0]
    hi = rows[-1][0] + 1

    def repl(m):
        v = int(m.group(0), 16)
        if lo <= v < hi:
            return '<body+0x%x>' % (v - lo)
        return m.group(0)

    return [(a - lo, re.sub(r'0x1[0-9a-f]{8}\b', repl, t)) for a, t in rows]


a = norm(load(sys.argv[1]))
b = norm(load(sys.argv[2]))
print("A insns=%d  B insns=%d" % (len(a), len(b)))
bad = 0
for i in range(max(len(a), len(b))):
    ra = a[i] if i < len(a) else (-1, '<none>')
    rb = b[i] if i < len(b) else (-1, '<none>')
    if ra != rb:
        bad += 1
        print("DIFF #%d\n  A +0x%x: %s\n  B +0x%x: %s"
              % (i, ra[0], ra[1], rb[0], rb[1]))
print("differing instructions: %d" % bad)
sys.exit(1 if bad else 0)
