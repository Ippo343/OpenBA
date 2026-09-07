#!/usr/bin/env python3
"""Runtime-x64.dll port census: called vs ported vs total, per namespace.

The exe port's progress question was "how many of the addresses our tree asks
for are registered". The runtime port's is the same shape, with three numbers
instead of two:

    total   — functions the DLL actually contains, per namespace
              (from docs/runtime_port/survey_ghidra_census.md, a live Ghidra
              count; this script does not talk to Ghidra)
    called  — the distinct functions OUR tree calls today, i.e. the dependency
              surface we must satisfy before cutover
              (scanned live from code/, engine/, runtime/, standalone/, dx11/)
    ported  — of those, the ones a RegisterRuntimeExport / RegisterRuntimeAddr /
              RegisterRuntimeOrdinal call has taken over

"called" is the honest denominator for the port: it is what has to work for
`reimpl-x64.exe` to boot without the DLL. "total" is the denominator for a
complete DLL reimplementation, which is not the stated goal (PLAN.md §4.1
decision 1: static link, no drop-in DLL).

Usage:
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/runtime_census.py
    ... --csv docs/runtime_port/census_<stamp>.csv    write the per-function rows
    ... --unported                                    list what is called but not ported
    ... --namespace UI                                restrict to one namespace
"""

import argparse
import collections
import csv
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ["code", "engine", "runtime", "standalone", "dx11"]
SKIP_DIR_NAMES = {"build", "build-og", "build-perf", "obj", ".git", "__pycache__", ".venv"}
SOURCE_EXT = (".cpp", ".h", ".hpp", ".c")

# --- call sites into the DLL -------------------------------------------------
# These match only up to the OPENING quote; the mangled name itself is read by
# concat_literal() below, which absorbs any further string literals that
# follow separated only by whitespace. A mangled name split across two source
# lines is one C++ string literal after preprocessing, not two -- a plain
# `"([^"]+)"` capture reads only the first fragment and silently truncates it.
# (Confirmed live: gameupdateloop_load.cpp:71 SetAchievement, and
# pickingcomponent.cpp:171 AddMesh, both reported as phantom exports by
# export_name_audit.py before this fix.)
STRING_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')


def concat_literal(text, pos):
    """Read the C++ string literal starting at `pos` and absorb any further
    literals that immediately follow (separated only by whitespace) into one
    joined name, exactly as the C++ preprocessor concatenates adjacent string
    literals. Returns the joined content with quotes stripped."""
    parts = []
    i = pos
    while True:
        m = STRING_LITERAL.match(text, i)
        if not m:
            break
        parts.append(m.group(0)[1:-1])
        i = m.end()
        j = i
        while j < len(text) and text[j] in " \t\r\n":
            j += 1
        if j < len(text) and text[j] == '"':
            i = j
            continue
        break
    return "".join(parts)


RE_EXPORT = re.compile(r'ResolveExport(?:Raw)?\s*\(\s*(?=")')
RE_ORDINAL = re.compile(r'ResolveOrdinal(?:Raw)?\s*\(\s*(\d+)')
RE_ADDR = re.compile(r'ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)')
RE_ADDR_RAW = re.compile(r'ResolveRuntimeRaw\s*\(\s*(0x[0-9a-fA-F]+)')

# --- ports -------------------------------------------------------------------
RE_REG_EXPORT = re.compile(r'RegisterRuntimeExport\s*\(\s*(?=")')

# A RegisterRuntimeExport whose first argument is NOT a string literal. This
# regex exists because of a real, silent under-count: Wave 4 lane D registered
# its six FileSystem/Modification ports through `kName*` constants, which
# RE_REG_EXPORT cannot see through, and the census reported IO 30/41 while 36
# were in fact registered. Nothing failed and nothing warned -- the ports were
# live in the process and invisible to the tool the project treats as its
# authority over any agent's "all N ported" claim.
#
# This is the mirror image of reference_uncalled_port_is_not_verified.md: there
# a port is counted but never runs; here a port runs but is never counted. Both
# end in a census that does not describe reality, so both must be loud.
RE_REG_EXPORT_NONLITERAL = re.compile(
    r'RegisterRuntimeExport\s*\(\s*(?!")([A-Za-z_][A-Za-z0-9_:]*)')
RE_REG_ORDINAL = re.compile(r'RegisterRuntimeOrdinal\s*\(\s*(\d+)')
RE_REG_ADDR = re.compile(r'RegisterRuntimeAddr\s*\(\s*(0x[0-9a-fA-F]+)')

# --- detours (a port that also took over the DLL's own internal callers) -----
RE_DETOUR_ADDR = re.compile(r'InstallRuntimeDetour(?:Checked)?\s*\(\s*"([^"]*)"\s*,\s*(0x[0-9a-fA-F]+)')
RE_DETOUR_EXPORT = re.compile(r'InstallRuntimeDetourExport\s*\(\s*"([^"]*)"\s*,\s*"([^"]+)"')

# Function counts per namespace, from the live Ghidra census recorded in
# docs/runtime_port/PLAN.md §3.1. Update both together.
TOTAL_FUNCTIONS = {
    "UI": 1625, "Graphics": 599, "Video": 573, "IO": 407, "System": 262,
    "Math": 228, "Object": 191, "Audio": 137, "Achievement": 118, "Engine": 108,
    "Collision2D": 76, "Volume": 64, "Input": 63, "Picking": 49,
    "Pathfinding": 45, "Save": 37, "Application": 26, "Utility": 16, "Debug": 8,
}


def namespace_of(mangled):
    """Outermost namespace of an MSVC mangled name: ?Method@Class@Namespace@@...

    The qualified name is a list of components, inner-to-outer, and the LAST
    one is the namespace. Parsing it needs the actual MSVC grammar, not a
    split on "@@", because of two rules that interact:

      * A NAME FRAGMENT is terminated by '@'.
      * A BACK-REFERENCE is a single digit indexing the fragments already seen
        in this name, and it is NOT terminated by '@'.
      * The component list itself is terminated by an empty component, i.e. a
        bare '@'.

    So the familiar "@@" is fragment-terminator plus list-terminator, and a
    name whose innermost component is a back-reference ends its scope with a
    SINGLE '@'. `?GetDescription@Achievement@1@QEBA?AVString@System@@XZ` is
    Achievement::Achievement::GetDescription returning a System::String — and
    splitting it on "@@" hands back "System", filing three Achievement methods
    under System. This walks the grammar instead.
    """
    if not mangled.startswith("?"):
        return "?"
    i, n = 1, len(mangled)
    if i < n and mangled[i] == "?":
        # Operator name: "?X" (??0 = ctor, ??1 = dtor, ??Y = operator+ ...) or
        # the extended "?_X" form. It is not a name fragment, so it must not
        # enter the back-reference table — skip it and start the scope list at
        # the next component.
        i += 1
        if i < n and mangled[i] == "_":
            i += 1
        i += 1
        fragments, resolved = [], [""]   # placeholder for the "method name"
    else:
        fragments, resolved = [], []
    while i < n:
        c = mangled[i]
        if c == "@":            # empty component: end of the qualified name
            break
        if c.isdigit():         # back-reference, one character, no terminator
            idx = int(c)
            resolved.append(fragments[idx] if idx < len(fragments) else c)
            i += 1
            continue
        j = mangled.find("@", i)
        if j < 0:
            return "?"          # unterminated fragment: not a name we can read
        frag = mangled[i:j]
        fragments.append(frag)
        resolved.append(frag)
        i = j + 1

    # resolved[0] is the method name; the rest are scopes, outermost last.
    scopes = resolved[1:]
    if not scopes:
        return "?"
    if scopes[-1] in TOTAL_FUNCTIONS:
        return scopes[-1]
    for comp in reversed(scopes):
        if comp in TOTAL_FUNCTIONS:
            return comp
    return "?"


def iter_sources():
    for top in SCAN_DIRS:
        root_dir = os.path.join(REPO, top)
        if not os.path.isdir(root_dir):
            continue
        for dirpath, dirnames, filenames in os.walk(root_dir):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
            for fn in filenames:
                if fn.endswith(SOURCE_EXT):
                    yield os.path.join(dirpath, fn)


# Files whose RegisterRuntime* calls are OPT-IN: their Register function only runs
# when an env var arms it and returns 0 otherwise, so the registrations are
# present in the source but NOT live. A census that counted them would say
# "System 95% ported" for a wave in which the allocator has never executed a
# single instruction -- the same overstatement as a diff site reporting
# divergences=0 on calls=0 (WAVE1.md defect 4). They are reported separately.
GATED_OFF_FILES = {
    "runtime/src/system/allocator.cpp",       # BANISHED_RT_ALLOC, verdict NO-GO
    "runtime/src/system/allocator_win.cpp",
}


def strip_comments(text):
    """Blank out C/C++ comments, preserving length so byte offsets still line up.

    Added 2026-09-06 (Wave 5). The census reported a port row literally named
    `<literal>` -- a function that does not exist -- because four source files
    document this tool's own match pattern inside a comment. The regex matched
    the comment text. One fake row in the tool this project treats as
    authoritative over any agent's "all N ported" claim is exactly the kind of
    small wrongness that makes the true rows less believable, so it is fixed
    here rather than by rewording the four comments: a reworded comment fixes
    the four that exist, this fixes the fifth nobody has written yet.

    Comment bodies are replaced with SPACES rather than deleted so that every
    match offset stays valid for concat_literal(), which reads forward from
    m.end() to join adjacent string literals.

    String and character literals are tracked, so a "//" or "/*" inside one (a
    URL, a Windows path) is not mistaken for the start of a comment.
    """
    BACKSLASH = chr(92)
    out = list(text)
    i, n = 0, len(text)
    state = None          # None | '"' | "'" | '//' | '/*'
    while i < n:
        c = text[i]
        if state is None:
            if c == '"' or c == "'":
                state = c
            elif c == '/' and i + 1 < n and text[i + 1] == '/':
                state = '//'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
            elif c == '/' and i + 1 < n and text[i + 1] == '*':
                state = '/*'
                out[i] = out[i + 1] = ' '
                i += 2
                continue
        elif state == '"' or state == "'":
            if c == BACKSLASH:
                i += 2
                continue
            if c == state:
                state = None
        elif state == '//':
            if c == '\n':
                state = None
            else:
                out[i] = ' '
        elif state == '/*':
            if c == '*' and i + 1 < n and text[i + 1] == '/':
                out[i] = out[i + 1] = ' '
                state = None
                i += 2
                continue
            if c != '\n':
                out[i] = ' '
        i += 1
    return ''.join(out)


def scan():
    called_exports, called_addrs, called_ordinals = {}, {}, {}
    ported_exports, ported_addrs, ported_ordinals = {}, {}, {}
    gated_exports, gated_addrs = {}, {}
    detoured = {}
    nonliteral_regs = {}   # file -> [identifier, ...] the census cannot resolve

    for path in iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue

        # Match against CODE only. A comment that quotes this tool's own match
        # pattern used to produce a phantom port row -- see strip_comments().
        text = strip_comments(text)

        for m in RE_EXPORT.finditer(text):
            name = concat_literal(text, m.end())
            if name:
                called_exports.setdefault(name, set()).add(rel)
        for m in RE_ORDINAL.finditer(text):
            called_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_ADDR.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)
        for m in RE_ADDR_RAW.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)

        # A registration inside an OPT-IN file is NOT a live port: its
        # Register function is only called when an env var arms it, and it
        # returns 0 otherwise. Counting it would repeat the exact overstatement
        # this census exists to prevent -- see GATED_OFF_FILES.
        gated = rel in GATED_OFF_FILES

        for m in RE_REG_EXPORT.finditer(text):
            name = concat_literal(text, m.end())
            if name:
                (gated_exports if gated else ported_exports).setdefault(name, set()).add(rel)
        for m in RE_REG_EXPORT_NONLITERAL.finditer(text):
            # `const` is the parameter type in the DECLARATIONS of
            # RegisterRuntimeExport itself (rt_port.h, resolve.h,
            # resolve_standalone.cpp), not a call passing a constant.
            if m.group(1) == "const":
                continue
            # rt_selftest.cpp deliberately registers a FAKE name and one real
            # one to prove the seam works end to end. It is a self-test, not a
            # port, and counting it would be wrong -- so it is excluded rather
            # than left to emit a permanent warning that trains readers to
            # ignore this block.
            if rel == "standalone/rt_selftest.cpp":
                continue
            nonliteral_regs.setdefault(rel, []).append(m.group(1))
        for m in RE_REG_ORDINAL.finditer(text):
            ported_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_REG_ADDR.finditer(text):
            (gated_addrs if gated else ported_addrs).setdefault(int(m.group(1), 16), set()).add(rel)

        for m in RE_DETOUR_ADDR.finditer(text):
            detoured.setdefault(int(m.group(2), 16), set()).add(rel)
        for m in RE_DETOUR_EXPORT.finditer(text):
            detoured.setdefault(m.group(2), set()).add(rel)

    return dict(
        nonliteral_regs=nonliteral_regs,
        gated_exports=gated_exports, gated_addrs=gated_addrs,
        called_exports=called_exports, called_addrs=called_addrs,
        called_ordinals=called_ordinals, ported_exports=ported_exports,
        ported_addrs=ported_addrs, ported_ordinals=ported_ordinals,
        detoured=detoured,
    )


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", help="write per-function rows to this CSV")
    ap.add_argument("--unported", action="store_true",
                    help="list every called-but-not-ported function")
    ap.add_argument("--namespace", help="restrict the listings to one namespace")
    args = ap.parse_args()

    d = scan()

    rows = []  # kind, key, namespace, ported, detoured, callers
    for name, files in d["called_exports"].items():
        rows.append(("export", name, namespace_of(name),
                     name in d["ported_exports"], name in d["detoured"], sorted(files)))
    for addr, files in d["called_addrs"].items():
        rows.append(("addr", "0x%x" % addr, "internal",
                     addr in d["ported_addrs"], addr in d["detoured"], sorted(files)))
    for ordn, files in d["called_ordinals"].items():
        # An ordinal carries no name, so no namespace can be inferred from it —
        # its own bucket, not lumped in with names that failed to parse.
        rows.append(("ordinal", str(ordn), "ordinal",
                     ordn in d["ported_ordinals"], False, sorted(files)))

    # Ports with no caller in our tree are worth surfacing: for the exe port a
    # registration nobody asks for was usually a vestigial resolve, and the same
    # trap exists here.
    called_export_names = set(d["called_exports"])
    orphan_exports = sorted(set(d["ported_exports"]) - called_export_names)
    called_addr_set = set(d["called_addrs"])
    orphan_addrs = sorted(set(d["ported_addrs"]) - called_addr_set)

    per_ns = collections.defaultdict(lambda: {"called": 0, "ported": 0, "detoured": 0})
    for kind, key, ns, ported, detoured, files in rows:
        b = per_ns[ns]
        b["called"] += 1
        b["ported"] += 1 if ported else 0
        b["detoured"] += 1 if detoured else 0

    print("Runtime-x64.dll port census")
    print("=" * 78)
    print("%-14s %8s %8s %8s %8s  %s" %
          ("namespace", "total", "called", "ported", "detour", "called covered"))
    print("-" * 78)

    order = sorted(per_ns, key=lambda n: (-per_ns[n]["called"], n))
    tot_called = tot_ported = tot_detour = 0
    for ns in order:
        b = per_ns[ns]
        total = TOTAL_FUNCTIONS.get(ns, "")
        pct = (100.0 * b["ported"] / b["called"]) if b["called"] else 0.0
        print("%-14s %8s %8d %8d %8d  %5.1f%%" %
              (ns, total, b["called"], b["ported"], b["detoured"], pct))
        tot_called += b["called"]
        tot_ported += b["ported"]
        tot_detour += b["detoured"]

    print("-" * 78)
    pct = (100.0 * tot_ported / tot_called) if tot_called else 0.0
    print("%-14s %8d %8d %8d %8d  %5.1f%%" %
          ("TOTAL", sum(TOTAL_FUNCTIONS.values()), tot_called, tot_ported, tot_detour, pct))
    print()
    print("total  = functions in the DLL (PLAN.md §3.1 Ghidra census; blank for "
          "non-namespaced internals)")
    print("called = distinct functions our tree resolves today")
    ng = len(d["gated_exports"]) + len(d["gated_addrs"])
    if ng:
        print()
        print("%d registration(s) are present in the source but NOT counted above:" % ng)
        for f in sorted(GATED_OFF_FILES):
            n = sum(1 for v in list(d["gated_exports"].values()) + list(d["gated_addrs"].values())
                    if f in v)
            if n:
                print("  %-40s %d  (opt-in; its Register function returns 0 unless armed)" % (f, n))
        print("  A gated registration is not a port. Arm it, measure it, then move it.")
    nl = d.get("nonliteral_regs") or {}
    if nl:
        total = sum(len(v) for v in nl.values())
        print()
        print("*** WARNING: %d RegisterRuntimeExport call(s) pass a NON-LITERAL name."
              % total)
        print("*** These ports are LIVE in the process but INVISIBLE to this census,")
        print("*** so every number above UNDERSTATES what is actually registered.")
        for f in sorted(nl):
            print("  %-44s %s" % (f, ", ".join(sorted(set(nl[f])))))
        print("  Fix by spelling the mangled name as a literal at the call site.")
        print("  (Wave 4 lane D hit this: 6 ports registered through kName* constants")
        print("   reported as IO 30/41 when 36 were registered. Nothing warned.)")
        print()
    print("ported = of those, taken over by a RegisterRuntime* call")
    print("detour = of those, ALSO patched in the DLL so its own internal callers "
          "come to us")

    if orphan_exports or orphan_addrs:
        print()
        print("Ports with no caller in our tree (%d exports, %d addresses) — check "
              "each is not\na vestigial resolve:" % (len(orphan_exports), len(orphan_addrs)))
        for n in orphan_exports:
            print("  export  %s" % n)
        for a in orphan_addrs:
            print("  addr    0x%x" % a)

    if args.unported:
        print()
        sel = [r for r in rows if not r[3]]
        if args.namespace:
            sel = [r for r in sel if r[2] == args.namespace]
        print("Called but not ported (%d):" % len(sel))
        for kind, key, ns, _ported, _detoured, files in sorted(sel, key=lambda r: (r[2], r[1])):
            print("  %-8s %-12s %s" % (kind, ns, key))
            print("           %s" % ", ".join(files[:4]) + (" …" if len(files) > 4 else ""))

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["kind", "key", "namespace", "ported", "detoured", "callers"])
            for kind, key, ns, ported, detoured, files in sorted(rows, key=lambda r: (r[0], r[2], r[1])):
                w.writerow([kind, key, ns, int(ported), int(detoured), ";".join(files)])
        print()
        print("wrote %s (%d rows)" % (args.csv, len(rows)))

    return 0


if __name__ == "__main__":
    sys.exit(main())
