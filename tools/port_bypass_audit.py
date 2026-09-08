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

THE SECOND DOOR: ResolveExportRaw
=================================

A port can be bypassed through the NAME door too, and the address sweep above
cannot see it by construction. `ResolveExportRaw(name)` takes the mangled name
and **deliberately skips the port registry**, so that `RT_ORIGINAL_ONCE`,
`RtOriginal` and body-inspection checks can reach the SHIPPED implementation.
That deliberate skip is the whole point of the door — and it is a silent bypass
anywhere else.

Wave 9 lane P hit exactly that porting `System::Thread::ThreadFunc`: leaving the
`Thread` ctor on `ResolveExportRaw` would mean "the port registers, the census
counts it, and every thread still runs the DLL's body".

So the second pass sweeps every direct `ResolveExportRaw` / `ResolveOrdinalRaw`
call site whose name we have ported, and classifies it:

  OK-DATA       the name is exported DATA (`??_7…`, `?_singleton@…`,
                `?_typeInfo@…`, `?_first…`, `?_seconds…`). There is no body to
                port, so raw is the only thing raw could mean. Lane C mints these.
  OK-UNPORTED   we have not ported that name, so raw and non-raw would reach the
                same DLL function. (It still returns null at cutover — that is
                the raw-door verdict list, a separate item, not a bypass.)
  OK-ORACLE     the name IS ported, but the site is oracle machinery: inside a
                diff/save-trace guard, a probe, a self-test, a detour installer,
                or a body-byte check. Legitimate — this is what the door is for.
  BYPASS        the name is ported and the site is a live call path. Fix it by
                switching to `ResolveExport`.

`RT_ORIGINAL_ONCE` (268 sites) and `RtOriginal<>` (182) are NOT swept: both are
oracle machinery by definition, documented as such in `runtime/funcs/rt_port.h`,
and sweeping them would bury the four-or-so real findings under 450 rows that are
correct by construction. What they DO leave behind is a live `RtOriginal` that
returns null once the DLL is gone; that is the raw-residue verdict list, which is
a different question from "is this port bypassed".

**How the classification is paired, and its one honest limit.** The gate is looked
for in the ENCLOSING FUNCTION's body, brace-matched — not in a fixed line window.
A window is what let a twelve-line comment silently delete a site from
`resolve_fallback_audit.py` (166 -> 162, and the vanished row was the one just
fixed). The limit of function-granularity: a function that does oracle work AND
has a live raw call on another path classifies as OK-ORACLE. The verdict therefore
prints the gate that decided it, so the claim is checkable; with the ported set
this small (9 sites at the time of writing) every one is meant to be read.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/port_bypass_audit.py
    ... --all       list every site and its verdict, not just bypasses
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


# --- the name door -----------------------------------------------------------

RAW_SITE = re.compile(r"Resolve(?:ExportRaw|OrdinalRaw)\s*\(\s*")
# The resolver itself and the two headers that DECLARE these entry points.
RESOLVER_FILES = {"runtime/funcs/rt_port.h", "standalone/resolve.h",
                  "standalone/resolve_standalone.cpp"}
# Exported DATA prefixes: a vtable, a singleton slot, a BaseTypeInfo, a constant.
DATA_PREFIXES = ("??_7", "?_singleton@", "?_typeInfo@", "?_first", "?_seconds")
# Markers that make an enclosing function oracle machinery rather than a live path.
ORACLE_MARKERS = ("rt_diff_on__", "RtSaveTrace::", "RtDiffEnabled", "RT_DIFF",
                  "RtDiffReport", "Verify", "memcmp", "RtOriginal",
                  "InstallRuntimeDetour")
ORACLE_FN_WORDS = ("verify", "probe", "selftest", "prologue", "trace", "detour",
                   "original", "shadow")
# A thin accessor whose whole job is to hand back exported DATA -- `RtVtable`,
# `RtDataExport<T>`, `…SingletonSlot`. Its argument is the caller's literal, so
# the name is not visible here; the function's own name is what says "data".
DATA_FN_WORDS = ("vtable", "singleton", "typeinfo", "dataexport", "slot")
NAME_CONST = re.compile(
    r'\b(?:static\s+)?(?:const|constexpr)\s+char\s*\*\s*(?:const\s+)?'
    r'([A-Za-z_]\w*)\s*(?:\[\])?\s*=\s*"((?:[^"\\]|\\.)*)"')
# Any `name(args) {` that is not a control statement. NOT anchored to the start
# of a line: `extern "C" void RtAudio_Scene_SerializeTraceProbe(void*) {` is a
# function definition too, and a start-anchored pattern missed it -- which made
# two save-trace probes classify as BYPASS in this script's first run, exactly
# the "your first number will be too big" failure. A lambda cannot match: `](…)`
# has no name character before the paren.
FN_DEF = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*"
                    r"(?:const\s*)?(?:noexcept\s*)?\{")
NOT_A_FN = {"if", "for", "while", "switch", "catch", "return", "sizeof", "do"}


def enclosing_function(text, pos):
    """(name, body) of the brace-matched function definition containing `pos`.

    Brace matching rather than a line window, for the reason in the module
    docstring: a window's answer changes when someone adds a comment. The
    INNERMOST containing definition wins, so a helper defined inside a namespace
    block is attributed to the helper.
    """
    best = None
    for m in FN_DEF.finditer(text, 0, pos):
        if m.group(1) in NOT_A_FN:
            continue
        open_brace = text.rindex("{", m.start(), m.end())
        depth, i, n = 0, open_brace, len(text)
        while i < n:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        if open_brace < pos < i:
            if best is None or open_brace > best[0]:
                best = (open_brace, m.group(1), text[open_brace:i])
    return (best[1], best[2]) if best else ("", "")


def sweep_name_door(census, ported_names):
    rows = []
    for path in census.iter_sources():
        rel = os.path.relpath(path, REPO).replace(os.sep, "/")
        if rel in RESOLVER_FILES:
            continue
        try:
            raw_text = open(path, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        text = census.strip_comments(raw_text)
        consts = {m.group(1): m.group(2) for m in NAME_CONST.finditer(text)}
        for m in RAW_SITE.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            name = census.concat_literal(text, m.end())
            if not name:
                ident = re.match(r"([A-Za-z_]\w*)\s*\)", text[m.end():])
                name = consts.get(ident.group(1), "") if ident else ""
            fn, body = enclosing_function(text, m.start())
            gate = next((g for g in ORACLE_MARKERS if g in body), "")
            if not gate:
                w = next((w for w in ORACLE_FN_WORDS if w in fn.lower()), "")
                gate = ("fn name: " + w) if w else ""
            if not name:
                # A name this script cannot follow (a parameter, a table walked
                # in a loop). Inside oracle machinery that is expected and needs
                # no verdict; anywhere else it is a row a HUMAN must read, which
                # is why it is reported rather than assumed benign.
                dw = next((w for w in DATA_FN_WORDS if w in fn.lower()), "")
                verdict, why = (("OK-ORACLE", gate) if gate
                                else ("OK-DATA", "fn name: " + dw) if dw
                                else ("UNRESOLVED-NAME", "name not a literal"))
            elif name.startswith(DATA_PREFIXES):
                verdict, why = "OK-DATA", "exported data, no body to port"
            elif name not in ported_names:
                verdict, why = "OK-UNPORTED", "not ported; raw == non-raw today"
            else:
                verdict = "OK-ORACLE" if gate else "BYPASS"
                why = gate or "live path"
            rows.append((verdict, rel, line, fn, name, why))
    return rows


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
    name_rows = sweep_name_door(census, ported_names)
    name_rows.sort(key=lambda r: (r[0], r[1], r[2]))
    bad = [r for r in rows if r[0] == "BYPASS"]
    name_bad = [r for r in name_rows if r[0] in ("BYPASS", "UNRESOLVED-NAME")]

    if args.csv:
        with open(args.csv, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["door", "verdict", "file", "line", "address_or_fn",
                        "export_name", "note"])
            for verdict, rel, line, addr, name in rows:
                w.writerow(["address", verdict, rel, line, addr, name, ""])
            for verdict, rel, line, fn, name, why in name_rows:
                w.writerow(["name", verdict, rel, line, fn, name, why])

    if not args.quiet:
        def tally(rs):
            c = {}
            for r in rs:
                c[r[0]] = c.get(r[0], 0) + 1
            return "  ".join("%s=%d" % kv for kv in sorted(c.items()))

        print("ADDRESS door: %d ResolveAddr(Module::Runtime, 0x…) sites; "
              "%d reach a function we have ALREADY ported by name"
              % (len(rows), len(bad)))
        print("  " + tally(rows))
        for verdict, rel, line, addr, name in rows:
            if verdict != "BYPASS" and not args.all:
                continue
            print("  %-16s %s:%d" % (verdict, rel, line))
            print("      %s   %s" % (addr, name or "(not exported)"))

        print()
        print("NAME door: %d direct ResolveExportRaw/ResolveOrdinalRaw sites; "
              "%d are a ported function on a live path"
              % (len(name_rows), len([r for r in name_rows if r[0] == "BYPASS"])))
        print("  " + tally(name_rows))
        for verdict, rel, line, fn, name, why in name_rows:
            if verdict not in ("BYPASS", "UNRESOLVED-NAME") and not args.all:
                continue
            print("  %-16s %s:%d  (in %s)" % (verdict, rel, line, fn or "?"))
            print("      %s   [%s]" % (name or "(name not a literal)", why))
    return 1 if (bad or name_bad) else 0


if __name__ == "__main__":
    sys.exit(main())
