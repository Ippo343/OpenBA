#!/usr/bin/env python3
"""Check every `ResolveExport(name); if (!x) ResolveAddr(Module::Runtime, addr)`
fallback in the tree: is `addr` the entry point of the function `name` denotes?

WHY THIS EXISTS

The tree is full of the idiom

    fn = (T)ResolveExport("?Mangled@Name@@...");
    if (!fn)
        fn = (T)ResolveAddr(Module::Runtime, 0x1800xxxxx);

The fallback is dead as long as the name resolves, so a wrong address in it is
invisible. But `ResolveExport` on a name the DLL does not contain returns NULL
SILENTLY (`reference_resolveexport_phantom_names.md` -- six found by Wave 2, two
of them re-diagnosed by Wave 5 and fixed again by Wave 8), and at that instant
the fallback fires. Whatever is at that address runs.

Wave 9 lane D audited all 166 of them by hand. 161 were correct. Five were not:

  engine/funcs/engine_funcs.cpp:2855   ?FindCheckBoxes@RadioElement@UI@@QEAAXXZ
      lives at 0x180047c00 (1 param). The fallback says 0x1800d5750, which IS a
      real exported entry point -- the PRIVATE 2-param overload
      ?FindCheckBoxes@RadioElement@UI@@AEAAXV?$InternalPtr@VElement@UI@@@Object@@@Z.
      The typedef is void(*)(void*) and the caller passes one argument, so this
      one does NOT fault: it runs the wrong overload with garbage in RDX and does
      radio-group tree surgery with it.
  engine/funcs/engine_funcs.cpp:3144   0x18006a3e0 is mid-body of FUN_18006a390
  code/Game/Player/fencedtool.cpp:373  0x18008f530 is mid-body of FUN_18008f38c
  code/Game/Player/fencedtool.cpp:378  0x18008f880 is mid-body of FUN_18008f38c
  code/Game/Player/tool_base_toolui.cpp:96  0x180000000+kRVA_Dtor lands in .rdata

THE CHECK THIS SCRIPT MAKES, AND WHY IT IS NOT THE OBVIOUS ONE

"Is the fallback address a plausible function?" is NOT sufficient -- all four
of the wrong .text addresses above are in .text, and one of them is a genuine
exported entry point. The check has to be

    is `addr` the entry point of the function THIS MANGLED NAME denotes?

which for an overload set means comparing the FULL mangling, not the class and
method. That is exactly what an export-table lookup does, so no Ghidra is needed:
the export directory maps mangled name -> RVA, and the comparison is `==`.

Note the shape of the failure this replaces. `engine/funcs/engine_funcs.cpp:4060`
already carries the comment

    "...but a fallback that is only ever taken when something is already wrong is
     exactly where a wrong address hides."

and it sits above a fallback that is CORRECT, while the five defective ones a few
hundred lines away were never checked. A true comment that checks nothing is this
project's documented failure mode; a script that runs is the fix.

AFTER CUTOVER

This script reads the shipped Runtime-x64.dll's export directory. Wave 9 lane G
deletes that DLL, and on that day this audit stops being able to answer its own
question -- at exactly the moment the fallbacks stop being dead, because with no
DLL there is no `ResolveExport` to succeed. The replacement is not another
checker: it is that every one of these fallback branches must be GONE before
lane G, so that there is nothing left to audit. Until then, `--quiet` gates.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/resolve_fallback_audit.py
    ... --dll "C:/GOG Games/Banished/Runtime-x64.dll"
    ... --all       list the correct sites too, not just the defects
    ... --csv PATH  write every site to CSV
    ... --quiet     exit code only (0 = every fallback address is correct)

VERDICTS
    CORRECT           addr == the export table's VA for that exact mangled name
    WRONG-ADDRESS     the name exists, at a DIFFERENT address than the fallback
    PHANTOM-NAME      the name is not in the export table at all, so this fallback
                      is already the live path (see export_name_audit.py)
    PHANTOM-ORDINAL   the same, for a ResolveOrdinal primary
    COMPUTED-ADDRESS  the address argument is an expression, not a hex literal
    UNRESOLVED-NAME   the primary's name is a symbol this script could not follow

KNOWN BLIND SPOT, stated rather than papered over: a fallback whose guard tests a
DIFFERENT pointer from the one being assigned -- `if (fn_X) ... else <ResolveAddr>`
-- has no name to pair with, so the name-vs-address check cannot run on it. One
such site exists (code/Game/Player/tool_base_toolui.cpp:96, guarded on
`fn_DialogInstance_Dtor`); it is caught here only because it ALSO computes its
address. A future one that does not would be missed. Widening the pairing to
"any nearby name" is not the fix -- that is TRAP 1 below, and it reintroduces
wrong names on right addresses.

EXIT CODES
    0  every guarded fallback address is the entry point of its own name
    1  at least one non-CORRECT site
    2  the DLL is not there to check against (see AFTER CUTOVER above)
"""
import argparse, csv, os, re, sys

DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
IMAGE_BASE = 0x180000000
# .claude holds abandoned agent worktrees: stale copies of the tree whose hits
# are not real and drown the live ones. Same list as export_name_audit.py.
SKIP_DIRS = {".git", ".claude", "build", "build-perf", "build-og", "obj", ".venv", "ToolKit"}
SCAN_DIRS = ("code", "engine", "runtime", "standalone", "dx11")

ADDR = re.compile(r"ResolveAddr\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)\s*\)")
# A THIRD RULE, and it costs nothing. The address argument must be a bare hex
# literal. Exactly one site in the tree is not
# (code/Game/Player/tool_base_toolui.cpp:96, `0x180000000 + kRVA_Dtor`), and it is
# a defect: that RVA lands in .rdata, while ??1DialogInstance@UI@@UEAA@XZ is at
# 0x180114d40. A computed address also defeats every other check here, because
# there is no literal to compare -- so the rule is "do not compute it", not "try
# harder to evaluate it". Zero false positives measured on the whole tree.
COMPUTED = re.compile(r"ResolveAddr\(\s*Module::Runtime\s*,\s*([^)]*?[+\-][^)]*?)\)")
# The assignment target of the statement the ResolveAddr sits in.
TARGET = re.compile(r"([A-Za-z_][\w:.>\-\[\]]*)\s*=\s*[^=]")
RESOLVE_NAME = re.compile(
    r'Resolve(Export|Ordinal)(?:Raw)?\(\s*("(?:[^"\\]|\\.)*"|[A-Za-z_]\w*|\d+)\s*\)')


def export_tables(dll):
    """mangled name -> VA, and ordinal -> VA, from the shipped export directory."""
    import pefile
    pe = pefile.PE(dll)
    byname, byordinal = {}, {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        va = IMAGE_BASE + e.address
        if e.name:
            byname[e.name.decode()] = va
        byordinal[e.ordinal] = va
    return byname, byordinal


def source_files(root="."):
    for d in SCAN_DIRS:
        for base, dirs, files in os.walk(os.path.join(root, d)):
            dirs[:] = [x for x in dirs if x not in SKIP_DIRS]
            for f in files:
                if f.endswith((".cpp", ".h", ".cc", ".hpp")):
                    yield os.path.join(base, f)


def statement_start(lines, i):
    """First line of the logical statement ending at line index `i`. A fallback is
    routinely wrapped across three or four lines by clang-format."""
    s = i
    while s > 0 and not lines[s - 1].rstrip().endswith((";", "{", "}", ":")) and (i - s) < 8:
        s -= 1
    return s


def find_sites(root="."):
    """Yield every GUARDED fallback: a ResolveAddr(Module::Runtime, addr) whose
    assignment target was previously assigned from ResolveExport/ResolveOrdinal,
    and which sits under a null-guard on that same target (or in an else arm).

    TRAP 1 -- PAIR BY ASSIGNMENT TARGET, NOT BY PROXIMITY. Lane D's first pass
    matched "nearest ResolveExport name within N lines" and produced 170
    candidates with names attached to the WRONG addresses, because a file that
    resolves twenty pointers in a row has a name on every line. Requiring that
    the name and the address be assigned to the SAME variable produced 166 clean
    pairs, and it is the difference between a checker and a rumour.

    TRAP 2 -- AN UNCONDITIONAL ResolveAddr IS NOT A FALLBACK. Most of the
    by-address calls in `runtime/` are deliberate: header-only Collection
    templates have no export to resolve by name, so calling them by address is
    the ONLY option, and they are commented as such (see
    runtime/src/ui/element.cpp:226-244, eighteen lines of justification above one
    such call, which a widened context window flagged as a defect during the
    audit and which is not one). Those must not be reported: on the Wave 9 lane D
    numbers a checker that cannot tell them apart cries wolf over 34 addresses --
    the whole Collection::Array cluster.
    """
    for path in source_files(root):
        try:
            lines = open(path, encoding="utf-8", errors="surrogateescape").read().split("\n")
        except OSError:
            continue
        whole = "\n".join(lines)
        for i, line in enumerate(lines):
            m = ADDR.search(line)
            if not m:
                continue
            addr = int(m.group(1), 16)
            s = statement_start(lines, i)
            stmt = " ".join(x.strip() for x in lines[s:i + 1])
            tm = TARGET.search(stmt)
            if not tm:
                continue                      # not an assignment: not this idiom
            target = tm.group(1)
            base = target.split(".")[-1].split(">")[-1].split("::")[-1]

            # TRAP 1: the primary must assign the SAME target.
            #
            # *** THE LOOKBACK MUST STOP AT A FUNCTION BOUNDARY. *** Wave 9 lane
            # GFX3 found this producing WRONG-ADDRESS rows that were pure
            # artefacts: it paired `?AsMatrix@Transform@Math@@` from one
            # forwarder with `0x18005e5e0` (GridAdd) in a LATER one, and
            # `?SetText@EditElement@UI@@` with `0x1800c7ad0` (MemFree). Both read
            # as a serious defect -- "a wrong address in a fallback" is exactly
            # what this tool exists to find -- and neither was real.
            #
            # Two compounding causes, both fixed here:
            #   * the window was 40 RAW lines, so COMMENTS ate it. Nine lines of
            #     comment moved a primary out of range and silently changed a
            #     row's classification -- the same shape as
            #     reference_documenting_a_check_can_delete_it, where adding a
            #     comment deleted a call site from a checker's fixed window.
            #   * every forwarder in this tree names its local `fn`, so pairing
            #     on a bare identifier matches ACROSS function boundaries by
            #     construction.
            #
            # A `}` in column 0 ends a function in this codebase, so the walk
            # stops there. Comment-only lines are skipped rather than counted,
            # so documenting a call site can no longer change its verdict.
            back_lines = []
            k = s - 1
            while k >= 0 and len(back_lines) < 40:
                raw = lines[k]
                if raw[:1] == "}":
                    break                      # previous function ends here
                t = raw.strip()
                if t and not t.startswith("//"):
                    back_lines.append(t)
                k -= 1
            back = " ".join(reversed(back_lines))
            kind = name = None
            for am in re.finditer(re.escape(base) + r"\s*=\s*(.{0,200}?);", back):
                nm = RESOLVE_NAME.search(am.group(1))
                if nm:
                    kind, name = nm.group(1), nm.group(2)
            if name is None:
                continue

            # TRAP 2: it must be GUARDED. An unconditional ResolveAddr is a
            # deliberate by-address call, not a fallback. Accept `if (!x)`,
            # `if (x == nullptr)` and an `else` arm; allow a `d.` / `p->`
            # qualifier on the target (dx11/src/renderstatedata.cpp:322 is
            # `if (!d.baseDtor)` and IS a real fallback).
            # The guard sits within a few lines above the statement -- but count
            # only CODE lines. A comment block between the `if` and the
            # ResolveAddr used to push the guard out of a fixed 6-line window, and
            # the site then vanished from this audit ENTIRELY rather than being
            # reported: 166 sites silently became 165. Wave 9 lane R hit that with
            # its own explanatory comment while fixing defect 1, which is the worst
            # possible way to find it -- documenting a fallback deleted it from the
            # checker. A site this script drops is indistinguishable from a site
            # someone fixed, so blank and comment-only lines do not count toward
            # the window.
            win_src, j = [], i
            while j >= 0 and len(win_src) < 7:
                t = lines[j].strip()
                if t and not t.startswith("//"):
                    win_src.append(t)
                j -= 1
            win = " ".join(reversed(win_src))
            q = r"(?:[A-Za-z_]\w*\s*(?:\.|->)\s*)?" + re.escape(base)
            guarded = (re.search(r"if\s*\(\s*!\s*" + q + r"\s*\)", win)
                       or re.search(r"if\s*\(\s*" + q + r"\s*==\s*(nullptr|NULL|0)\s*\)", win)
                       or re.search(r"\belse\b", win))
            if not guarded:
                continue

            yield dict(file=path.replace(os.sep, "/"), line=i + 1, target=target,
                       kind=kind, raw_name=name, addr=addr, source=whole)


def resolve_name(site, byname, byordinal):
    """(mangled name or None, expected VA or None, verdict)."""
    raw = site["raw_name"]
    if raw.startswith('"'):
        name = raw[1:-1]
    elif raw.isdigit():
        va = byordinal.get(int(raw))
        return f"ordinal {raw}", va, ("CORRECT" if va == site["addr"]
                                      else "PHANTOM-ORDINAL" if va is None else "WRONG-ADDRESS")
    else:
        # A named constant: `const char* const kSym_X = "?Mangled@..."`. Naming the
        # mangling once and using it at both the RT_ORIGINAL_ONCE site and the
        # registration is good practice (WAVE9.md census defect 3), so resolve it
        # rather than giving up on it.
        d = re.search(r"\b" + re.escape(raw) + r"\s*(?:\[\])?\s*=\s*\"((?:[^\"\\]|\\.)*)\"",
                      site["source"])
        if not d:
            return None, None, "UNRESOLVED-NAME"
        name = d.group(1)
    va = byname.get(name)
    if va is None:
        # The name is not in the export table at all: export_name_audit.py's
        # territory, but report it here too -- a phantom name is precisely the
        # trigger that makes this fallback FIRE.
        return name, None, "PHANTOM-NAME"
    return name, va, "CORRECT" if va == site["addr"] else "WRONG-ADDRESS"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--root", default=".")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--csv")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    if not os.path.exists(args.dll):
        # See "AFTER CUTOVER" above. Say which failure this is; do not pass.
        print(f"resolve_fallback_audit: {args.dll} not found.\n"
              "  This audit compares fallback addresses against the shipped DLL's export\n"
              "  table. With no DLL there is nothing to compare against -- and with no DLL\n"
              "  every one of these fallbacks is LIVE, because ResolveExport can no longer\n"
              "  succeed. The fix is to delete the fallback branches, not to skip the check.",
              file=sys.stderr)
        return 2

    byname, byordinal = export_tables(args.dll)
    rows = []
    for path in source_files(args.root):
        try:
            lines = open(path, encoding="utf-8", errors="surrogateescape").read().splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines):
            m = COMPUTED.search(line)
            if m and not ADDR.search(line):
                rows.append(dict(file=path.replace(os.sep, "/"), line=i + 1, target="",
                                 export="(address is computed, so no name can be checked)",
                                 fallback_addr=m.group(1).strip(), export_addr="",
                                 verdict="COMPUTED-ADDRESS"))
    for site in find_sites(args.root):
        name, va, verdict = resolve_name(site, byname, byordinal)
        rows.append(dict(file=site["file"], line=site["line"], target=site["target"],
                         export=name or site["raw_name"],
                         fallback_addr=f"0x{site['addr']:x}",
                         export_addr=f"0x{va:x}" if va else "",
                         verdict=verdict))
    rows.sort(key=lambda r: (r["verdict"] == "CORRECT", r["file"], r["line"]))
    bad = [r for r in rows if r["verdict"] != "CORRECT"]

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf8") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)

    if not args.quiet:
        print(f"{len(rows)} guarded ResolveExport->ResolveAddr fallbacks; "
              f"{len(bad)} do NOT point at their own name\n")
        for r in (rows if args.all else bad):
            print(f"  {r['verdict']:<16} {r['file']}:{r['line']}")
            print(f"      {r['export']}")
            print(f"      fallback {r['fallback_addr']}   name lives at "
                  f"{r['export_addr'] or '(not in the export table)'}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
