#!/usr/bin/env python3
"""Symbolize `reimpl-x64.exe + 0xOFFSET` frames from standalone_crash.log.

The VEH handler prints module-relative offsets; nm prints VAs against the PE image
base (0x140000000 for this exe).  offset + base is therefore the symbol VA.

Usage:  python tools/symbolize.py <exe> 0x225C60 0x2267E7 ...
        python tools/symbolize.py <exe> --log <crash.log>
"""
import re
import subprocess
import sys

BASE = 0x140000000


def load(exe):
    out = subprocess.run(["nm", "-C", "--defined-only", exe],
                         capture_output=True, text=True).stdout
    syms = []
    for line in out.splitlines():
        m = re.match(r"^([0-9a-fA-F]{8,16}) [TtWwDdBb] (.+)$", line)
        if m:
            syms.append((int(m.group(1), 16), m.group(2)))
    syms.sort()
    return syms


def lookup(syms, va):
    lo, hi = 0, len(syms) - 1
    best = None
    while lo <= hi:
        mid = (lo + hi) // 2
        if syms[mid][0] <= va:
            best = syms[mid]
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def main():
    exe = sys.argv[1]
    args = sys.argv[2:]
    if args and args[0] == "--log":
        text = open(args[1]).read()
        offs = [int(x, 16) for x in re.findall(r"reimpl-x64\.exe\s+\+ 0x([0-9A-Fa-f]+)", text)]
    else:
        offs = [int(a, 16) for a in args]
    syms = load(exe)
    for off in offs:
        va = BASE + off
        s = lookup(syms, va)
        if s:
            print("+0x%-8X -> %s + 0x%x" % (off, s[1], va - s[0]))
        else:
            print("+0x%-8X -> ?" % off)


if __name__ == "__main__":
    main()
