#!/usr/bin/env python3
"""Find PORTS THAT ARE BYPASSED: a call site that asks `ResolveAddr(Module::Runtime,
0x1800xxxxx)` for a function we have ALREADY ported and registered BY NAME.

WHY THIS EXISTS

`ResolveAddr(Module::Runtime, addr)` consults the ADDRESS registry only
(`RtAddrReg()`, standalone/resolve_standalone.cpp). A port registered with
`RegisterRuntimeExport("?Mangled@...", &Ours)` lives in a different map, so a
caller that asks by ADDRESS walks straight past it into the shipped DLL.

Nothing complains. The port is live, registered, and unused:

  * `BANISHED_RT_DIFF` never sees those calls -- it wraps our registered body,
    which is not the one running -- so the port reports `calls=0` or reports a
    call count that omits this site entirely.
  * `tools/runtime_census.py` folds address -> exported name through the DLL's
    export directory, so it sees the name, sees the registration, and counts the
    function PORTED.
  * At cutover the DLL is gone and the address resolves to nothing, so a
    function every progress number calls done becomes a missing symbol.

Wave 9 lane 0 found three of these by running the process under
`BANISHED_RT_STRICT=1` and diffing the residue against the census. This script
answers the follow-up question -- *are there more?* -- from source, so it can gate
in CI rather than needing a nine-cell harness matrix.

It is the third hat of `reference_uncalled_port_is_not_verified.md`: not a port
with no callers, but a port whose callers go around it.

THE FIX AT A FLAGGED SITE

Change the CALL SITE to ask by NAME:

    fn = (T)ResolveExport("?Mangled@Name@@...");   // hits the port registry

NOT "make ResolveAddr fall back to the export map". That papers over the site,
still needs the shipped DLL's export directory to map address -> name, and cannot
work at all once the DLL is deleted. Asking by name is the only form that
survives cutover.

WHAT IS DELIBERATELY NOT FLAGGED

A by-address call to something the DLL does NOT export is correct and often the
only option -- `Collection::Array<T>` and `Collection::Set<T>` are header-only
templates with zero exports in either build, so `runtime/src/ui/element.cpp:226`
calls one by address on purpose, with eighteen lines of comment saying so. Those
sites have no name to ask for; they are counted and listed under --all, never
flagged.

Likewise a site whose address IS registered with `RegisterRuntimeAddr` (the
address door is served) or carries an installed detour (the shipped bytes jump to
us) reaches the port and is not a bypass.

AND -- the correction that took this number from 35 to 5 -- a GUARDED FALLBACK is
not a bypass either:

    fn = (T)ResolveExport("?Mangled@...");     // <- this hits the port
    if (!fn) fn = (T)ResolveAddr(Module::Runtime, 0x1800xxxxx);   // dead

The primary asks by name, so the port IS reached; the address branch never runs.
A first version of this script flagged all 30 of those and the count read as an
order-of-magnitude finding. They are `tools/resolve_fallback_audit.py`'s
territory (166 sites, all of which lane F must DELETE before cutover), and this
script reuses that script's pairing logic to recognise them rather than
re-deriving it. A bypass is an UNGUARDED by-address call.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/port_bypass_audit.py
    ... --all       list every by-address site and its verdict, not just bypasses
    ... --csv PATH  write every site to CSV
    ... --quiet     exit code only

EXIT CODES
    0  no ported function is reached by address
    1  at least one bypass
    2  the shipped DLL is not there to map addresses onto names (the same
       refusal-to-answer as runtime_census.py: with no export directory this
       script cannot tell a bypass from a header-only template call, and a
       checker that returns 0 when it cannot check is how this project got here)
"""
import argparse, csv, importlib.util, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)

ADDR_SITE = re.compile(
    r"Resolve(?:Addr\(\s*Module::Runtime\s*,|RuntimeRaw\()\s*(0x[0-9a-fA-F]+)\s*\)")


def load_census():
    """Import runtime_census.py as a module and reuse its scanner.

    Sharing the scanner rather than re-implementing it is the point: the census
    has been wrong about what counts as a live registration five separate times
    (gated files, gated `if` blocks, non-literal names, dx11's own resolver,
    address-only bucketing). A second copy of that logic would drift, and the
    two tools would disagree about which ports exist.
    """
    spec = importlib.util.spec_from_file_location(
        "runtime_census", os.path.join(HERE, "runtime_census.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def load_fallback_audit():
    spec = importlib.util.spec_from_file_location(
        "resolve_fallback_audit", os.path.join(HERE, "resolve_fallback_audit.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def guarded_fallback_sites():
    """{(relpath, line)} of every guarded ResolveExport->ResolveAddr fallback.

    Straight out of resolve_fallback_audit.find_sites(), because the two scripts
    must agree about what a fallback is. If that pairing logic changes, this
    exclusion changes with it.
    """
    fb = load_fallback_audit()
    out = set()
    for site in fb.find_sites(REPO):
        rel = os.path.relpath(site["file"], REPO).replace(os.sep, "/")
        out.add((rel, site["line"]))
    return out


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--csv")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    census = load_census()
    exports = census.load_export_addrs()   # exits 2 if it cannot answer
    scan = census.scan()
    ported_names = set(scan["ported_exports"])
    ported_addrs = set(scan["ported_addrs"])
    detoured = {k for k in scan["detoured"] if isinstance(k, int)}
    detoured_names = {k for k in scan["detoured"] if not isinstance(k, int)}
    fallbacks = guarded_fallback_sites()

    rows = []
    for path in census.iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        try:
            text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        text = census.strip_comments(text)
        for m in ADDR_SITE.finditer(text):
            addr = int(m.group(1), 16)
            line = text.count("\n", 0, m.start()) + 1
            name = exports.get(addr, "")
            if (rel, line) in fallbacks:
                verdict = "GUARDED-FALLBACK"   # dead; resolve_fallback_audit owns it
            elif not name:
                verdict = "NO-EXPORT"      # header-only template &c -- correct
            elif addr in ported_addrs:
                verdict = "ADDR-REGISTERED"
            elif addr in detoured or name in detoured_names:
                verdict = "DETOURED"
            elif name in ported_names:
                verdict = "BYPASS"
            else:
                verdict = "UNPORTED"       # ordinary DLL call, lane B/C/D work
            rows.append((verdict, rel, line, "0x%x" % addr, name))

    rows.sort(key=lambda r: (r[0], r[1], r[2]))
    bad = [r for r in rows if r[0] == "BYPASS"]

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["verdict", "file", "line", "address", "export_name"])
            w.writerows(rows)

    if not args.quiet:
        counts = {}
        for r in rows:
            counts[r[0]] = counts.get(r[0], 0) + 1
        print("%d by-address Runtime resolve sites; %d reach a function we have "
              "ALREADY ported by name" % (len(rows), len(bad)))
        print("  " + "  ".join("%s=%d" % kv for kv in sorted(counts.items())))
        print()
        for verdict, rel, line, addr, name in rows:
            if verdict != "BYPASS" and not args.all:
                continue
            print("  %-15s %s:%d" % (verdict, rel, line))
            print("      %s   %s" % (addr, name or "(not exported)"))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
