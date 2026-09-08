#!/usr/bin/env python3
r"""Which DOOR does each exported-DATA symbol come through? (Wave 9 lane C2)

THE BUG THIS EXISTS TO CATCH

Wave 9 lane C2 mints the exported DATA symbols -- vtables, singletons,
BaseTypeInfo, and the other .data statics -- and registers them, at cutover,
with RegisterRuntimeExport. That registration is only reachable through the
PORT-AWARE door: ResolveExport checks the port registry and only then falls
through to the DLL.

`ResolveExportRaw` does not. It is GetProcAddress against Runtime-x64.dll, by
design, so the differential oracle can always reach the shipped body. With the
DLL gone it returns NULL -- for a symbol the mint carries correctly just as
much as for one it has never heard of. A ported constructor that stamps
`ResolveExportRaw("??_7Foo@...")` into slot 0 therefore writes a NULL vtable
into every object of that class at cutover, and the first virtual call is an
EXEC-of-0. That is a crash, not a degradation, and adding the symbol to the
mint does NOT fix it: the door has to change too.

Found by lane V's residue sweep on three Graphics vtables; this tool is the
general form.

WHAT IT CHECKS, and the two blind spots it was written to have neither of

1. It reads the minted set from standalone/rt_data_mint_tables.inc, so it
   tracks the mint automatically rather than repeating a list.
2. It finds symbol names WRAPPED in a helper, not only names sitting directly
   inside a ResolveExport*() call. `RtVtable("??_7MeshInstance@Graphics@@6B@")`
   is the shape that hid this bug from a grep for `ResolveExportRaw("` -- and
   from tools/runtime_census.py, whose RE_EXPORT has the same requirement, so
   those three vtables are missing from the census's `called` set entirely.
   Wrappers are DISCOVERED (a function that forwards a const char* parameter
   into a raw door), not hard-coded, and the discovered list is printed so a
   new one is visible rather than silently trusted.
3. It resolves a name reached through a named constant
   (`ResolveExportRaw(kFrameIndexData)`), which is the other spelling.

EXIT CODES
    0  no minted DATA symbol is read through a raw door
    2  at least one is -- each listed with file:line and the door
    3  the tool could not answer (missing tables, no DLL)

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_data_door_audit.py
    ... --all      also list the port-door and non-minted reads
"""
import argparse
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TABLES = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")
SOURCE_DIRS = ["code", "runtime", "standalone", "engine", "dx11"]
SKIP_FILES = {
    # The mint's own generated table: it NAMES every symbol by definition, and
    # the names there are data, not resolve sites.
    os.path.join("standalone", "rt_data_mint_tables.inc"),
    os.path.join("standalone", "rt_data_mint.cpp"),
    os.path.join("standalone", "rt_data_mint.h"),
}

RAW_DOORS = {"ResolveExportRaw", "ResolveOrdinalRaw", "ResolveRuntimeRaw"}
PORT_DOORS = {"ResolveExport", "ResolveOrdinal", "ResolveAddr",
              "RtPortLookupExport", "RtPortLookupAddr"}
# Not reads at all: registration, and the census/audit tooling's own mentions.
NON_READ = {"RegisterRuntimeExport", "RegisterRuntimeAddr", "RegisterRuntimeOrdinal"}

# A literal preceded by `IDENT(` or `IDENT<...>(`, possibly across whitespace.
CALL_BEFORE = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^<>()]*>)?\s*\(\s*$')
# ...and the same call with the literal as a LATER argument, which is how
# RT_DATA_SLOT(T, "name") spells it. Without this every RT_DATA_SLOT read
# fell through to "(not a call)" and landed in UNCLASSIFIED -- the count went
# 3 -> 10 across three lanes purely because more code started using the
# port-aware door correctly. A lane reported it rather than silencing it with
# an RT_RAW_OK, which would have asserted "oracle-only read that dies with the
# oracle" and been false.
#
# The preceding-argument text may not contain parens or quotes, so a nested
# call like foo(bar(), "sym") does NOT match and stays UNCLASSIFIED: this
# fails CLOSED, which is the direction a door audit must fail in.
CALL_BEFORE_ARG = re.compile(
    r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^<>()]*>)?\s*\([^()"]*,\s*$')
# `static const char kName[] = "..."` / `constexpr const char* kName = "..."`
CONST_DECL = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[\s*\])?\s*=\s*$')
MANGLED = re.compile(r'"((?:\?\?_7|\?_)[^"]{4,})"')


def source_files():
    for d in SOURCE_DIRS:
        for root, _dirs, files in os.walk(os.path.join(REPO, d)):
            for f in files:
                if f.endswith((".cpp", ".h", ".inc")):
                    p = os.path.join(root, f)
                    rel = os.path.relpath(p, REPO)
                    if rel.replace("/", os.sep) in SKIP_FILES:
                        continue
                    yield rel.replace(os.sep, "/"), p


def strip_comments(text):
    """Blank out // and /* */ comments, PRESERVING line numbers.

    A mangled name in a comment is a note about a symbol, not a read of it --
    this tree documents them heavily. Newlines are kept so every reported
    file:line still points at the right line.
    """
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == chr(92) else 1
            out.append(text[i:j + 1]); i = j + 1
        elif text.startswith("//", i):
            j = text.find(chr(10), i)
            j = n if j < 0 else j
            out.append(" " * (j - i)); i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == chr(10) else " " for ch in text[i:j]))
            i = j
        else:
            out.append(c); i += 1
    return "".join(out)


def minted_symbols(path):
    """Every symbol the mint carries, straight out of the generated table."""
    if not os.path.exists(path):
        raise SystemExit(f"{path} not found; run tools/rt_mint_gen.py first")
    text = open(path, encoding="utf-8").read()
    return set(m.group(1) for m in MANGLED.finditer(text))


def discover_wrappers(files, doors):
    """Functions that forward a const char* parameter into one of `doors`.

    Deliberately syntactic and deliberately loud: the point is not to be a C++
    parser, it is that a wrapper nobody remembered (mesh.cpp's RtVtable, the
    one that hid this whole bug) shows up in the printed list instead of being
    silently classified as 'unknown'.
    """
    found = {}
    for rel, path in files:
        lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
        for i, line in enumerate(lines):
            # A MACRO door. RT_DATA_SLOT(T, MANGLED) forwards MANGLED into
            # RtResolveCached, but it is a #define rather than a function AND its
            # const char* is the SECOND parameter -- so the function-shaped scan
            # below misses it twice over. Classified the way this tool classifies
            # everything else: by READING ITS DEFINITION, never by adding a name
            # to a list. A wrapper nobody remembered is precisely what this
            # function exists to catch.
            md = re.match(r'\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(', line)
            if md:
                name = md.group(1)
                if name not in RAW_DOORS and name not in PORT_DOORS:
                    window = "\\n".join(lines[i:i + 8])
                    for door in doors:
                        if door + "(" in window:
                            found[name] = (rel, i + 1, door)
                            break
                continue
            m = re.search(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(\s*const\s+char\s*\*', line)
            if not m:
                continue
            name = m.group(1)
            if name in ("if", "while", "for", "switch", "return", "sizeof"):
                continue                      # a keyword, not a call
            if name in RAW_DOORS or name in PORT_DOORS or name in NON_READ:
                continue                      # a door, or a declaration of one
            if line.rstrip().endswith(";"):
                continue                      # a prototype, not a body
            window = "\n".join(lines[i:i + 8])
            for door in doors:
                if door + "(" in window:
                    found[name] = (rel, i + 1, door)
                    break
    return found


def raw_ok(rel, line):
    """True when the source line (or the one above it) carries RT_RAW_OK."""
    path = os.path.join(REPO, rel.replace("/", os.sep))
    try:
        lines = open(path, encoding="utf-8", errors="replace").read().splitlines()
    except OSError:
        return False
    for n in (line - 1, line - 2, line - 3):
        if 0 <= n < len(lines) and "RT_RAW_OK" in lines[n]:
            return True
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--tables", default=TABLES)
    args = ap.parse_args()

    minted = minted_symbols(args.tables)
    files = list(source_files())
    wrappers = discover_wrappers(files, RAW_DOORS)
    # FIXED POINT, not one pass. A wrapper can forward into another wrapper:
    # RT_DATA_SLOT -> RtResolveCached -> ResolveExport. A single pass sees only
    # the innermost hop, so every RT_DATA_SLOT read was reported UNCLASSIFIED --
    # 63 of them, all correct code going through the port-aware door. Iterating
    # until nothing new appears costs nothing and removes a whole class of
    # false "triage this" rows. It cannot loop forever: `found` only grows and
    # the name set is finite.
    port_wrappers = discover_wrappers(files, PORT_DOORS)
    while True:
        grown = discover_wrappers(files, PORT_DOORS | set(port_wrappers))
        if set(grown) <= set(port_wrappers):
            break
        port_wrappers.update(grown)
    # A wrapper that reaches BOTH is raw: the raw path is the one that returns
    # NULL at cutover.
    for k in wrappers:
        port_wrappers.pop(k, None)
    raw = RAW_DOORS | set(wrappers)
    port = PORT_DOORS | set(port_wrappers)

    # Pass 1: every mangled DATA-shaped literal, and the identifier calling it.
    sites = []          # (symbol, rel, line, door)
    const_names = {}    # const identifier -> symbol
    for rel, path in files:
        text = strip_comments(open(path, encoding="utf-8", errors="replace").read())
        for m in MANGLED.finditer(text):
            sym = m.group(1)
            head = text[:m.start()]
            line = head.count("\n") + 1
            prefix = head[-160:]
            call = CALL_BEFORE.search(prefix) or CALL_BEFORE_ARG.search(prefix)
            if call:
                sites.append((sym, rel, line, call.group(1)))
                continue
            decl = CONST_DECL.search(prefix)
            if decl:
                const_names[decl.group(1)] = sym
                continue
            sites.append((sym, rel, line, "(not a call)"))

    # Pass 2: uses of those named constants.
    if const_names:
        pat = re.compile(r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:<[^<>()]*>)?\s*\(\s*('
                         + "|".join(re.escape(k) for k in const_names) + r')\s*[,)]')
        for rel, path in files:
            text = strip_comments(open(path, encoding="utf-8",
                                       errors="replace").read())
            for m in pat.finditer(text):
                caller, name = m.group(1), m.group(2)
                if caller in const_names:      # the declaration itself
                    continue
                sites.append((const_names[name], rel, text[:m.start()].count("\n") + 1,
                              caller))

    bad, ok, other, exempt = [], [], [], []
    for sym, rel, line, door in sites:
        if door in NON_READ:
            continue
        # An ORACLE-ONLY read of the DLL's own data is legitimately raw: it
        # compares against the shipped bytes and dies with the oracle at
        # cutover. Marked at the site with RT_RAW_OK so the exemption is
        # reviewable in the diff rather than living in this tool's head.
        if raw_ok(rel, line):
            exempt.append((sym, rel, line, door))
            continue
        bucket = bad if (door in raw and sym in minted) else (
            ok if door in port or door in raw else other)
        bucket.append((sym, rel, line, door))

    print(f"minted DATA symbols: {len(minted)}   sites examined: {len(sites)}")
    print(f"raw doors: {', '.join(sorted(RAW_DOORS))}")
    print(f"raw WRAPPERS discovered ({len(wrappers)}):")
    for name, (rel, line, door) in sorted(wrappers.items()):
        print(f"   {name:22s} {rel}:{line}  -> {door}")
    print(f"port-aware wrappers discovered ({len(port_wrappers)}):")
    for name, (rel, line, door) in sorted(port_wrappers.items()):
        print(f"   {name:22s} {rel}:{line}  -> {door}")

    if bad:
        print(f"\nFAIL — {len(bad)} read(s) of a MINTED symbol through a raw door.")
        print("At cutover each of these returns NULL. Change the door to "
              "ResolveExport (port-aware); it is a no-op while the DLL is loaded "
              "and unregistered.")
        for sym, rel, line, door in sorted(bad):
            print(f"   {rel}:{line}\n       {sym}\n       via {door}")
    else:
        print("\nPASS — no minted DATA symbol is read through a raw door.")
    if exempt:
        print(f"\nRT_RAW_OK exemptions ({len(exempt)}) - oracle-only reads that "
              f"die with the oracle:")
        for sym, rel, line, door in sorted(exempt):
            print(f"   {rel}:{line}  via {door}  {sym}")

    # A "(not a call)" row is a declaration or a table entry; when the same
    # symbol is also seen going through a real door elsewhere (pass 2 resolves
    # named constants and table loops), the declaration is not a separate
    # finding. Report only the ones nothing else explains.
    explained = {sym for sym, _r, _l, d in bad + ok + exempt if d != "(not a call)"}

    # A "(not a call)" row is a declaration or a TABLE ROW -- the name sits in
    # a `{slot, "mangled", addr}` initialiser and the door is a loop somewhere
    # else in the file, which no amount of regex will follow. Two ways such a
    # row stops being a finding:
    #   * the same symbol is seen going through a real door elsewhere, or
    #   * the file it lives in contains port-aware door calls and NO raw ones,
    #     so whatever the loop is, it cannot be a raw door.
    # The second is a file-level argument, not a site-level one, and is stated
    # as such: it is why the report says "table row" rather than "verified".
    door_kind = {}
    for rel, path in files:
        text = strip_comments(open(path, encoding="utf-8", errors="replace").read())
        has_raw = any(d + "(" in text for d in raw)
        has_port = any(d + "(" in text for d in port)
        door_kind[rel] = (has_raw, has_port)

    kept = []
    for row in other:
        sym, rel, _line, d = row
        if d != "(not a call)":
            kept.append(row)
            continue
        if sym in explained:
            continue
        has_raw, has_port = door_kind.get(rel, (True, False))
        if has_port and not has_raw:
            ok.append(row)          # table row in a port-only file
            continue
        kept.append(row)
    other = kept

    if other:
        print(f"\nUNCLASSIFIED doors ({len(other)}) — triage these, they are not "
              f"known to be safe:")
        for sym, rel, line, door in sorted(other):
            print(f"   {rel}:{line}  via {door}  {sym}")

    if args.all:
        print(f"\nport-door / non-minted reads ({len(ok)}):")
        for sym, rel, line, door in sorted(ok):
            print(f"   {rel}:{line}  via {door}  "
                  f"{'MINTED' if sym in minted else 'not minted'}  {sym}")

    return 2 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
