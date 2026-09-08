#!/usr/bin/env python3
"""tools/addr_door_audit.py -- the ADDRESS-door census.

Every `ResolveAddr(Module::Runtime, 0x...)` in the tree, minus every address any
`RegisterRuntimeAddr(0x...)` registers. What is left is the set of by-address
doors that resolve to NULL once Runtime-x64.dll is gone.

This is the instrument behind docs/runtime_port/wave9_unregistered_addr_doors.csv
(commits ce1d8e9 / 4ef55b6), which was produced ad hoc and not committed. It is
committed here by Wave 9 lane PICK so the next lane can re-run it rather than
re-derive it -- a shared instrument is fixed by ONE owner in ONE commit
(reference_census_cannot_see_the_macro_doors.md).

*** IT IS A WATCH LIST, NOT A DEFECT LIST. *** An unregistered door is only
fatal if it is REACHED with the DLL absent; some are DLL-present-only
diagnostics that stand down correctly at cutover.

THE MINT IS NOW MODELLED (2026-09-08). Lane PICK's first version documented that
its absolute counts disagreed with the curated CSV -- 88 unregistered against 56
-- and said to use it for deltas only. The whole difference was the data mint,
which registers ~500 addresses by walking a generated table through a VARIABLE
(`RegisterRuntimeAddr((uintptr_t)v.dllVa, ...)`), invisible to REG_LIT.
`mint_addresses()` reads those rows out of the generated .inc, so the absolute
count is now usable and the deltas-only caveat is retired.

KNOWN LIMIT that REMAINS, stated rather than hidden: the address must otherwise
be a LITERAL in the call and in the registration. A named constant is invisible
to both halves -- the same blind spot runtime_census.py had for twelve live
ports for a full wave (reference_census_cannot_see_nonliteral_registrations.md),
and the one port_bypass_audit.py had for 20 address-door sites until it was
taught to resolve `constexpr uintptr_t` definitions. The `--nonliteral` flag
reports both halves' invisible sites, so the blind spot is at least MEASURED.
Teaching this scan the same constant resolution is the obvious next fix.

    tools/addr_door_audit.py               # the three counts
    tools/addr_door_audit.py --csv         # the unregistered rows, CSV
    tools/addr_door_audit.py --nonliteral  # sites the literal scan cannot see
"""
import csv
import os
import re
import sys

ROOTS = ["runtime", "code", "engine", "standalone", "dx11"]
EXTS = (".cpp", ".h", ".c", ".hpp")

CALL_LIT = re.compile(r"ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)")
CALL_ANY = re.compile(r"ResolveAddr\s*\(\s*Module::Runtime\s*,")
REG_LIT = re.compile(r"RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)")
REG_ANY = re.compile(r"RegisterRuntimeAddr\s*\(")


def strip_comments_and_strings(text):
    """Blank out C/C++ comments and string/char literals, preserving offsets.

    ADDED 2026-09-08 by Wave 9 lane ADDR, because the scan had no notion of a
    comment and therefore MANUFACTURED a door. `engine/funcs/engine_funcs.cpp`
    around line 3200 contains, inside a comment block that exists precisely to
    explain that the code was DELETED:

        //     ResolveAddr(Module::Runtime, 0x18006f330)   // "might be in exe"

    The scan matched that prose and reported 0x18006f330 as a live, unregistered
    by-address door. It is not a door at all -- there is no call. That is one of
    16 rows on the watch list, i.e. 6% of the wave's remaining address surface
    was an artefact of the instrument, and it pointed a lane at an address the
    comment itself identifies as 0x90 bytes INSIDE the body of an unrelated
    internal (FUN_18006f2a0), which is the worst thing to send someone to port.

    This is the same family as reference_documenting_a_check_can_delete_it.md,
    inverted: there, adding a comment DELETED a real check from a scanner; here,
    a comment INVENTED a defect. Either way the lesson is that a scanner reading
    source as flat text sees prose and code as the same thing.

    Every character of a comment or literal is replaced with a space and every
    newline is kept, so line numbers and columns are byte-for-byte unchanged and
    the caller's line-based reporting needs no adjustment.

    String and char literals are blanked for the same reason: an address in a
    log-format string or a doc string is prose too. No registration or call can
    live inside a literal, so nothing real is lost.
    """
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "/":
            while i < n and text[i] != "\n":
                out.append(" ")
                i += 1
        elif c == "/" and nxt == "*":
            out.append("  ")
            i += 2
            while i < n and not (text[i] == "*" and i + 1 < n and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            out.append("  ")
            i += 2
        elif c in ('"', "'"):
            # Find the closing quote, but NEVER past the end of this line. An
            # unmatched quote in source that this scanner does not model (a
            # digit separator like 1'000, an apostrophe surviving in a macro)
            # would otherwise swallow the remainder of the FILE and silently
            # delete every real door below it -- a scanner failing open in the
            # direction that hides work is exactly the failure being fixed here.
            quote = c
            eol = text.find("\n", i)
            if eol == -1:
                eol = n
            j, closed = i + 1, False
            while j < eol:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == quote:
                    closed = True
                    break
                j += 1
            if not closed:
                out.append(c)          # not a literal after all; keep it
                i += 1
                continue
            out.append(" " * (j - i + 1))
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def scan():
    calls, regs, nonlit = {}, set(), []
    reg_nonlit = []
    for root in ROOTS:
        for dirpath, _dirs, files in os.walk(root):
            for name in files:
                if not name.endswith(EXTS):
                    continue
                path = os.path.join(dirpath, name).replace("\\", "/")
                try:
                    text = open(path, encoding="utf-8", errors="replace").read()
                except OSError:
                    continue
                text = strip_comments_and_strings(text)
                for lineno, line in enumerate(text.splitlines(), 1):
                    lit = list(CALL_LIT.finditer(line))
                    for m in lit:
                        calls.setdefault(int(m.group(1), 16), []).append(
                            "%s:%d" % (path, lineno))
                    if len(CALL_ANY.findall(line)) > len(lit):
                        nonlit.append("%s:%d" % (path, lineno))
                    rlit = list(REG_LIT.finditer(line))
                    for m in rlit:
                        regs.add(int(m.group(1), 16))
                    if len(REG_ANY.findall(line)) > len(rlit):
                        reg_nonlit.append("%s:%d" % (path, lineno))
    return calls, regs, nonlit, reg_nonlit


MINT_TABLES = "standalone/rt_data_mint_tables.inc"
MINT_ADDR = re.compile(r"0x1[0-9a-fA-F]{8}")


def mint_addresses():
    """The addresses the DATA MINT registers, which no literal scan can see.

    rt_data_mint.cpp walks three generated tables and registers every row
    through a VARIABLE:

        RegisterRuntimeAddr((uintptr_t)v.dllVa, (void*)v.table);   // :578
        RegisterRuntimeAddr((uintptr_t)t.dllVa, (void*)t.storage); // :583
        RegisterRuntimeAddr((uintptr_t)s.dllVa, s.storage);        // :588

    so REG_LIT matches none of them and ~500 registered addresses read as
    unregistered. That is not hypothetical: the ad-hoc scan behind ce1d8e9
    reported 94 unregistered doors when the true figure was 62, and 4ef55b6 had
    to correct it -- an instrument committing, while measuring it, the very
    blind spot it was measuring.

    Reading the generated .inc directly is deliberate. Parsing rt_data_mint.cpp
    for the registration calls would just move the blind spot; the .inc IS the
    table those calls walk, so every row in it is registered whenever the mint
    is armed. Erring toward INCLUDING an address is also the safe direction
    here: a false "registered" hides a watch-list row, but a false
    "unregistered" sends a lane to port something that already works, which is
    the more expensive mistake and the one that actually happened.
    """
    try:
        text = open(MINT_TABLES, encoding="utf-8", errors="replace").read()
    except OSError:
        return set(), False
    return {int(m.group(0), 16) for m in MINT_ADDR.finditer(text)}, True


def main():
    calls, regs, nonlit, reg_nonlit = scan()
    minted, mint_found = mint_addresses()
    if not mint_found:
        print("WARNING: %s not found -- mint-registered addresses are NOT "
              "subtracted, so UNREGISTERED below is an OVERCOUNT.\n"
              % MINT_TABLES, file=sys.stderr)
    regs = regs | minted
    unreg = {a: s for a, s in calls.items() if a not in regs}
    if "--csv" in sys.argv:
        w = csv.writer(sys.stdout, lineterminator="\n")
        w.writerow(["address", "call_sites", "files"])
        for a in sorted(unreg):
            w.writerow(["0x%09x" % a, len(unreg[a]), " | ".join(unreg[a])])
        return 0
    if "--nonliteral" in sys.argv:
        print("call sites the literal scan CANNOT see (%d):" % len(nonlit))
        for s in nonlit:
            print("   ", s)
        print("registrations the literal scan CANNOT see (%d):" % len(reg_nonlit))
        for s in reg_nonlit:
            print("   ", s)
        return 0
    print("by-address call sites   %d" % sum(len(v) for v in calls.values()))
    print("distinct addresses      %d" % len(calls))
    print("registered addresses    %d  (%d literal + %d mint-supplied)"
          % (len(regs), len(regs) - len(minted & regs), len(minted & regs)))
    print("UNREGISTERED            %d" % len(unreg))
    print("(non-literal call sites %d, non-literal registrations %d -- "
          "invisible to both halves; see --nonliteral)"
          % (len(nonlit), len(reg_nonlit)))
    by_file = {}
    for a, sites in unreg.items():
        for s in sites:
            by_file[s.rsplit(":", 1)[0]] = by_file.get(s.rsplit(":", 1)[0], 0) + 1
    print("\nconcentrations:")
    for f, n in sorted(by_file.items(), key=lambda kv: -kv[1])[:12]:
        print("  %3d  %s" % (n, f))
    return 0


if __name__ == "__main__":
    sys.exit(main())
