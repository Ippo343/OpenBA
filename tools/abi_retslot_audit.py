#!/usr/bin/env python3
"""Audit: every by-value-returning Runtime export must have room for its hidden return slot.

WHY THIS EXISTS
---------------
Under the MSVC x64 ABI a function returning a class or struct BY VALUE does not
return it in a register. The caller passes a hidden pointer to a caller-owned
return slot, and the callee writes the object through it and returns that same
pointer in RAX. The slot is an ARGUMENT: it occupies RCX for a static/free
function, or RDX for a member (after `this` in RCX).

If our fn_* typedef omits that parameter, the call still compiles and still
"works" often enough to hide: the callee writes its result through whatever
happened to be left in that register by the previous call, and RAX comes back
pointing at the same address, so a caller that only reads the result back
through RAX sees perfectly valid data. What it does NOT see is the 0x30 bytes
it just wrote over unrelated memory.

That is exactly the shape of the fn_Math_Transform3_Identity bug found on
2026-09-06 (engine/funcs/engine_funcs.h): declared `void*(*)()`, it scribbled an
identity matrix through a stale RCX on every menu scene load, and stayed
invisible for months because the corrupted bytes were then read back as a
correct identity matrix.

WHAT IT CHECKS
--------------
For every `fn_X = reinterpret_cast<fn_X_t>(ResolveExport("<mangled>"))`, decide
from the mangled name whether the return is a class/struct by value (`?AV` /
`?AU` after the calling-convention code), and require the typedef's arity to be
at least 1 (static: hidden slot) or 2 (member: this + hidden slot).

Enum returns (`?AW4`) come back in EAX and are deliberately NOT flagged.

Usage:  tools/ghidra_mcp/.venv/Scripts/python.exe tools/abi_retslot_audit.py
Exit code 1 if anything is suspicious, so it can gate a wave.
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP = os.path.join(REPO, "engine", "funcs", "engine_funcs.cpp")
HDR = os.path.join(REPO, "engine", "funcs", "engine_funcs.h")

# `?A` introduces a returned user-defined type; V = class, U = struct (both by
# memory), W4 = enum (by register, not our problem).
RE_BYVAL = re.compile(r'@@(QEBA|QEAA|SA|IEAA|IEBA|MEAA|UEAA|AEBA|AEAA)\?A[VU]')
RE_ASSIGN = re.compile(r'(fn_\w+)\s*=\s*reinterpret_cast<\s*(fn_\w+_t)\s*>\(\s*ResolveExport\(\s*"([^"]+)"', re.S)
RE_TYPEDEF = re.compile(r'typedef\s+(.+?)\(\s*\*\s*(fn_\w+_t)\s*\)\s*\(([^)]*)\)\s*;', re.S)


def arity(args):
    args = args.strip()
    if args in ("", "void"):
        return 0
    depth, n = 0, 1
    for ch in args:
        if ch in "<(":
            depth += 1
        elif ch in ">)":
            depth -= 1
        elif ch == "," and depth == 0:
            n += 1
    return n


def main():
    cpp = open(CPP, encoding="utf-8", errors="replace").read()
    hdr = open(HDR, encoding="utf-8", errors="replace").read()
    tdefs = {m.group(2): (m.group(1).strip(), m.group(3).strip())
             for m in RE_TYPEDEF.finditer(hdr)}

    checked, bad = 0, []
    for fn, tname, mangled in RE_ASSIGN.findall(cpp):
        if tname not in tdefs:
            continue
        m = RE_BYVAL.search(mangled)
        if not m:
            continue
        checked += 1
        ret, args = tdefs[tname]
        is_static = m.group(1) == "SA"
        need = 1 if is_static else 2
        n = arity(args)
        if n < need:
            bad.append((fn, mangled, ret, args, n, need, is_static))

    for fn, mangled, ret, args, n, need, is_static in bad:
        print(f"SUSPICIOUS {fn}")
        print(f"    export : {mangled}")
        print(f"    typedef: {ret}(*)({args})")
        print(f"    arity {n}, needs >= {need} "
              f"({'static: hidden slot in RCX' if is_static else 'member: this in RCX, hidden slot in RDX'})")
        print()
    print(f"{len(bad)} suspicious of {checked} by-value-returning exports "
          f"(enum returns excluded)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
