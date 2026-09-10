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

# Anchored on the SCRIPT, never on the current directory: two checkouts' copies
# run from one shell must not both measure whichever tree the shell happens to
# be sitting in.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# WIDENED 2026-09-10 (Wave 18 lane GFXIF). This audit used to read exactly two
# hardcoded files -- engine/funcs/engine_funcs.{cpp,h} -- and print
# "0 suspicious of 49 by-value-returning exports" with nothing in the banner or
# the output saying so. A grep for by-value-returning mangled names in runtime/,
# standalone/ and dx11/ returns 131 distinct symbols it had never looked at, and
# lane GFXIF was one step from citing that green for a file it cannot see.
# A screen that cannot name its own subject invites exactly that.
ROOTS = ("engine", "runtime", "standalone", "dx11", "code")
EXTS = (".cpp", ".h", ".hpp", ".inc")

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


def sources():
    """Every scanned file, as (repo-relative path, text). Sorted, so the subject
    set this prints is stable run to run."""
    out = []
    for root in ROOTS:
        base = os.path.join(REPO, root)
        if not os.path.isdir(base):
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [d for d in dirnames
                           if d not in ("build", "build-perf", ".git")]
            for fname in filenames:
                if not fname.endswith(EXTS):
                    continue
                full = os.path.join(dirpath, fname)
                rel = os.path.relpath(full, REPO).replace("\\", "/")
                out.append((rel, open(full, encoding="utf-8",
                                      errors="replace").read()))
    return sorted(out)


def main():
    files = sources()
    if not files:
        # A scanner that reads nothing prints a confident green. Refuse instead.
        print("ERROR: scanned 0 files -- the subject set is empty, so a clean "
              "result here would be meaningless.", file=sys.stderr)
        return 2

    # Typedefs are collected from EVERY scanned file, not one header: a typedef
    # and the assignment that uses it routinely live in different files.
    tdefs = {}
    for rel, text in files:
        for m in RE_TYPEDEF.finditer(text):
            tdefs.setdefault(m.group(2), (m.group(1).strip(),
                                          m.group(3).strip(), rel))

    checked, bad, unresolved = 0, [], []
    for rel, text in files:
        for fn, tname, mangled in RE_ASSIGN.findall(text):
            m = RE_BYVAL.search(mangled)
            if not m:
                continue
            if tname not in tdefs:
                # Counted and named, never silently dropped -- an unfound
                # typedef is an UNCHECKED by-value return, not a clean one.
                unresolved.append((rel, fn, tname))
                continue
            checked += 1
            ret, args, tdef_rel = tdefs[tname]
            is_static = m.group(1) == "SA"
            need = 1 if is_static else 2
            n = arity(args)
            if n < need:
                bad.append((rel, fn, mangled, ret, args, n, need, is_static))

    for rel, fn, mangled, ret, args, n, need, is_static in bad:
        print(f"SUSPICIOUS {fn}   [{rel}]")
        print(f"    export : {mangled}")
        print(f"    typedef: {ret}(*)({args})")
        print(f"    arity {n}, needs >= {need} "
              f"({'static: hidden slot in RCX' if is_static else 'member: this in RCX, hidden slot in RDX'})")
        print()

    if unresolved:
        print(f"UNCHECKED ({len(unresolved)}) -- by-value return whose typedef "
              f"this scanner could not find. NOT a clean verdict:")
        for rel, fn, tname in unresolved:
            print(f"    {fn}  (typedef {tname} not found)   [{rel}]")
        print()

    # SUBJECT SET, always printed. The previous version's one-line output could
    # not distinguish "nothing is wrong" from "nothing was looked at".
    # COVERAGE is computed BEFORE the headline is printed, because the headline
    # has to carry it. This block used to sit AFTER the headline, which is what
    # made the headline quotable on its own -- see the note below.
    RE_ANY = re.compile(r'"(\?[^"]*@@(?:QEBA|QEAA|SA|IEAA|IEBA|MEAA|UEAA|AEBA'
                        r'|AEAA)\?A[VU][^"]*)"')
    present = set()
    for rel, text in files:
        for m in RE_ANY.finditer(text):
            present.add(m.group(1))
    seen = {mg for rel, text in files
            for fn, tname, mg in RE_ASSIGN.findall(text)
            if RE_BYVAL.search(mg) and tname in tdefs}
    missed = len(present - seen)

    print(f"subject: {len(files)} file(s) under {'/, '.join(ROOTS)}/ "
          f"({len(tdefs)} fn_*_t typedefs collected)")
    # THE HEADLINE CARRIES ITS OWN QUALIFIER, DELIBERATELY.
    #
    # This line used to read "<n> suspicious of <checked> by-value-returning
    # exports checked, <u> unchecked (enum returns excluded)" and the UNAUDITED
    # count lived two lines below it. Both numbers were correct and the pair was
    # reconcilable -- "unchecked" means a door was matched but its arity could
    # not be read, "UNAUDITED" means no readable door was reached at all -- but
    # side by side they read as contradictory, and worse, the headline could be
    # lifted on its own as "0 suspicious, 0 unchecked" and reported as a clean
    # sweep of a subject that was 58% invisible.
    #
    # That is WAVE19_IO.md 6.1: quote a green WITH its denominator; "N
    # suspicious" alone is quotable out of context, "N of M, K unaudited" is
    # not. This tool was ALREADY a good citizen -- it sized its own blind spot
    # and named the doors it cannot read, which is more than most -- so this
    # closes a QUOTABILITY gap, not a measurement one. Nothing about what is
    # measured changed. Do not split these back onto separate lines.
    print(f"{len(bad)} suspicious of {checked} by-value-returning exports "
          f"checked ({len(unresolved)} unchecked, enum returns excluded); "
          f"{missed} of {len(present)} distinct names UNAUDITED")
    print(f"COVERAGE: {len(present & seen)} of {len(present)} distinct "
          f"by-value-returning mangled names in the tree reach a door this "
          f"scanner can read; {missed} do NOT and are UNAUDITED.")
    print("BLIND SPOT: it matches a literal ResolveExport(\"...\") assignment "
          "bound to a findable fn_*_t typedef. A resolve through a macro door "
          "(RT_EXPORT_ONCE / RT_DATA_SLOT), a named constant, or a direct call "
          "with no typedef is invisible here.")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
