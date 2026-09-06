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
RE_EXPORT = re.compile(r'ResolveExport(?:Raw)?\s*\(\s*"([^"]+)"')
RE_ORDINAL = re.compile(r'ResolveOrdinal(?:Raw)?\s*\(\s*(\d+)')
RE_ADDR = re.compile(r'ResolveAddr\s*\(\s*Module::Runtime\s*,\s*(0x[0-9a-fA-F]+)')
RE_ADDR_RAW = re.compile(r'ResolveRuntimeRaw\s*\(\s*(0x[0-9a-fA-F]+)')

# --- ports -------------------------------------------------------------------
RE_REG_EXPORT = re.compile(r'RegisterRuntimeExport\s*\(\s*"([^"]+)"')
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

    Components run inner-to-outer, so the LAST one before the '@@' is the
    namespace. Two wrinkles, both of which silently produced '?' buckets until
    they were handled:

      * BACK-REFERENCES. A single digit is an index into the table of name
        fragments already seen in this name: `?IsPaused@Application@1@…` is
        Application::Application::IsPaused.
      * NO `@@` TERMINATOR. The scope list ends at a single `@`, and the usual
        `@@` is that terminator plus an empty scope. Names like
        `?IsPaused@Application@1@QEAA_NXZ` have no `@@` at all, so splitting on
        it drags the signature into the component list.

    Rather than write a demangler for a census, the fallback is to scan the
    components from the outside in for the first one that names a namespace we
    know. That is a heuristic — it would misfile a method literally named after
    a namespace — but it is confined to names the strict parse already failed,
    and the alternative is an unattributed bucket. Anything that still does not
    resolve is reported as '?', never bucketed somewhere merely plausible.
    """
    if not mangled.startswith("?"):
        return "?"
    body = mangled.lstrip("?").split("@@", 1)[0]
    parts = [p for p in body.split("@") if p]
    if not parts:
        return "?"

    resolved, fragments = [], []
    for p in parts:
        if len(p) == 1 and p.isdigit():
            idx = int(p)
            resolved.append(fragments[idx] if idx < len(fragments) else p)
        else:
            fragments.append(p)
            resolved.append(p)

    if resolved[-1] in TOTAL_FUNCTIONS:
        return resolved[-1]
    for comp in reversed(resolved):
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


def scan():
    called_exports, called_addrs, called_ordinals = {}, {}, {}
    ported_exports, ported_addrs, ported_ordinals = {}, {}, {}
    detoured = {}

    for path in iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue

        for m in RE_EXPORT.finditer(text):
            called_exports.setdefault(m.group(1), set()).add(rel)
        for m in RE_ORDINAL.finditer(text):
            called_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_ADDR.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)
        for m in RE_ADDR_RAW.finditer(text):
            called_addrs.setdefault(int(m.group(1), 16), set()).add(rel)

        for m in RE_REG_EXPORT.finditer(text):
            ported_exports.setdefault(m.group(1), set()).add(rel)
        for m in RE_REG_ORDINAL.finditer(text):
            ported_ordinals.setdefault(int(m.group(1)), set()).add(rel)
        for m in RE_REG_ADDR.finditer(text):
            ported_addrs.setdefault(int(m.group(1), 16), set()).add(rel)

        for m in RE_DETOUR_ADDR.finditer(text):
            detoured.setdefault(int(m.group(2), 16), set()).add(rel)
        for m in RE_DETOUR_EXPORT.finditer(text):
            detoured.setdefault(m.group(2), set()).add(rel)

    return dict(
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
