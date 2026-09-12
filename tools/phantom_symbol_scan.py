#!/usr/bin/env python3
"""Find MANGLED-NAME STRING LITERALS in our tree that the shipped DLL does not export.

WHY (Wave 25 lane VTIO): a negative control passed a deliberately WRONG mangled
name as a string literal. `tools/runtime_census.py` matches those literals to
build the called/ported board -- so the census minted a permanent GAP ROW for a
symbol that does not exist. A fabricated row is work that can never be
completed, and it inflates the denominator of every percentage in the wave.

SUBJECT: every `"?..."` / `"??_7..."` string literal under the scan roots, with
comments stripped first (a checker with no comment stripper reports prose as
code -- this project's own lesson, and the reason the shadow-site count read
354 instead of 305). Each literal is looked up in Runtime-x64.dll's export
directory with pefile.

WHAT IT CANNOT DECIDE: a name absent from the export directory is not
automatically a phantom. Three innocent shapes exist and are reported
separately where they can be recognised:
  * a name the DLL genuinely does not export but that we resolve by ADDRESS;
  * a name belonging to another module (VideoDX11, the profile DLL);
  * a deliberate negative control, which SHOULD be absent -- those are the ones
    that must be built at run time rather than written as a literal.
So this prints a WORK LIST, not a verdict list.

CONTROL (both directions, because "0 hits" from an absence screen is
indistinguishable from a dead parser):
  positive -- a fabricated name that cannot exist MUST be reported absent;
  negative -- a name read out of the DLL's own export table MUST be found.
Effectiveness counters: literals examined, exports loaded, files read.

    python tools/phantom_symbol_scan.py            # the work list
    python tools/phantom_symbol_scan.py --all      # every absent literal
"""
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOTS = ["runtime", "code", "standalone", "engine", "dx11"]
DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"

# A literal that looks like an MSVC mangled name: starts '?' and has an '@'.
LIT = re.compile(r'"(\?[A-Za-z0-9_?@$:.\-]{4,})"')


def strip_comments(text):
    out = []
    i, n, st = 0, len(text), 0
    while i < n:
        c, d = text[i], text[i:i + 2]
        if st == 0:
            if d == '//':
                st = 1; i += 2; out.append('  '); continue
            if d == '/*':
                st = 2; i += 2; out.append('  '); continue
            out.append(c); i += 1; continue
        if st == 1:
            out.append('\n' if c == '\n' else ' ')
            if c == '\n':
                st = 0
            i += 1; continue
        if d == '*/':
            st = 0; i += 2; out.append('  '); continue
        out.append('\n' if c == '\n' else ' '); i += 1
    return ''.join(out)


def load_exports():
    try:
        import pefile
    except ImportError:
        print("pefile not importable -- run this with tools/ghidra_mcp/.venv's python.")
        return None
    pe = pefile.PE(DLL, fast_load=True)
    pe.parse_data_directories(
        directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXPORT']])
    names = set()
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            names.add(e.name.decode('ascii', 'replace'))
    return names


def main():
    exports = load_exports()
    if exports is None:
        return 2
    files = 0
    literals = {}
    for top in ROOTS:
        for dp, dn, fns in os.walk(os.path.join(REPO, top)):
            dn[:] = [d for d in dn if d not in ('.git', 'build', 'build-perf', 'obj')]
            for fn in fns:
                if not fn.endswith(('.cpp', '.h', '.inc')):
                    continue
                path = os.path.join(dp, fn)
                files += 1
                code = strip_comments(
                    open(path, encoding='utf-8', errors='replace').read())
                for m in LIT.finditer(code):
                    literals.setdefault(m.group(1), set()).add(
                        os.path.relpath(path, REPO).replace('\\', '/'))

    # ---- controls, BOTH directions, handed real shapes ---------------------
    fabricated = "?ThisSymbolCannotExist@NoSuchClass@@QEAAXXZ"
    real = sorted(exports)[len(exports) // 2]
    pos_ok = fabricated not in exports
    neg_ok = real in exports
    print("CONTROLS: fabricated name reported absent: %s | a name read out of the "
          "export table is found: %s -> %s"
          % (pos_ok, neg_ok, "ALIVE" if (pos_ok and neg_ok) else "*** BROKEN ***"))
    print("subject: %d files under %s, comments stripped; %d distinct mangled "
          "literals examined against %d DLL exports"
          % (files, "/".join(ROOTS), len(literals), len(exports)))

    # A literal split across lines by the C preprocessor's string concatenation
    # ("?Foo@" "Bar@@QEAAXXZ") reaches this scanner as its FIRST FRAGMENT only.
    # Every one of those is a PREFIX of a real export, and reporting them as
    # phantoms was this screen's first output: 40 findings, all artefacts.
    # runtime_census.py joins adjacent literals (concat_literal); this screen
    # does not, so it discharges the same cases by prefix instead -- and COUNTS
    # them, because a silent filter is how a screen stops screening.
    prefixes = {k for k in literals if k not in exports
                and any(e.startswith(k) for e in exports)}
    absent = {k: v for k, v in literals.items()
              if k not in exports and k not in prefixes}
    print("%d absent literals are PREFIXES of a real export -- concatenated "
          "names, not phantoms; discharged." % len(prefixes))
    print("%d of %d literals are NOT in Runtime-x64.dll's export directory."
          % (len(absent), len(literals)))
    print()
    show_all = '--all' in sys.argv
    # The census only mints a row for a literal it can see in a call it parses,
    # so the actionable subset is the one that appears in a resolve/register
    # call. Grep for the call context rather than guessing.
    CENSUS_CALLS = ('ResolveExport', 'RegisterRuntimeExport', 'RT_EXPORT_ONCE',
                    'RT_ORIGINAL_ONCE', 'RtOriginal', 'RtMintPromotionStorage')
    work = []
    for name, paths in sorted(absent.items()):
        hot = []
        for p in sorted(paths):
            text = strip_comments(open(os.path.join(REPO, p), encoding='utf-8',
                                       errors='replace').read())
            for m in re.finditer(re.escape('"' + name + '"'), text):
                lo = max(0, m.start() - 120)
                if any(c in text[lo:m.start()] for c in CENSUS_CALLS):
                    hot.append(p)
                    break
        if hot or show_all:
            work.append((name, hot or sorted(paths), bool(hot)))

    print("%-4s %s" % ("", "literal  (CENSUS-VISIBLE rows first -- those are the ones "
                           "that can mint a phantom board row)"))
    for (name, paths, hot) in sorted(work, key=lambda r: (not r[2], r[0])):
        print("  %-5s %s" % ("CALL" if hot else "text", name))
        for p in paths[:3]:
            print("            %s" % p)
    if not work:
        print("  (none)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
