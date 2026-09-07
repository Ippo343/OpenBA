#!/usr/bin/env python3
"""Check every ResolveExport("...") name in the tree against the DLL's real export table.

WHY THIS EXISTS

`ResolveExport` takes an MSVC mangled name. A name that is one character wrong,
or that spells a return type the compiler never emitted, does not fail loudly --
it returns null. Almost every call site in this tree then falls back to
`ResolveAddr(Module::Runtime, 0x1800xxxxx)`, so the pointer ends up non-null and
the game works. The bug is invisible until the day someone ports that function:
`RegisterRuntimeExport` is keyed on the NAME, so a call site that resolves by
ADDRESS never sees the port. The port registers, the census counts it, the diff
harness reports `calls=0`, and the summary looks green -- WAVE1.md defect 4.

Wave 1 found four of these by hand (Vector2::Length / LengthSquared / Normalize,
Vector3::operator==). Wave 2 found two more the same way
(Filename::AdjustDelimiters is AEAA -- a private member -- not QEAA;
Filename::GetPathIdentifer returns Filename `?AV12@`, not String). This script
finds the rest in one pass instead of one wave at a time.

A hit is NOT automatically a defect: some are deliberate "this name does not
exist so take the fallback" idioms, and a few are truncated on purpose. But
every hit is a function that a future wave cannot port through its name, so each
one needs a verdict, not a shrug.

USAGE
    tools/ghidra_mcp/.venv/Scripts/python.exe tools/export_name_audit.py
    ... --dll "C:/GOG Games/Banished/Runtime-x64.dll"
    ... --quiet     exit code only (0 = no unknown names)
"""
import argparse, collections, os, re, sys

DEFAULT_DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
# .claude holds abandoned agent worktrees: stale copies of the tree whose hits
# are not real and drown the live ones.
SKIP_DIRS = {".git", ".claude", "build", "build-perf", "build-og", "obj", ".venv", "ToolKit"}
PATTERN = re.compile(r'Resolve(?:Export|ExportRaw)\(\s*(?=")')
STRING_LITERAL = re.compile(r'"(?:\\.|[^"\\])*"')


def concat_literal(text, pos):
    """Read the C++ string literal starting at `pos`, then absorb any further
    literals that follow separated only by whitespace -- adjacent string
    literals are concatenated by the C++ preprocessor, so a mangled name split
    across two source lines is one name, not two. Returns the joined content
    (quotes stripped)."""
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


def exported_names(dll):
    import pefile
    pe = pefile.PE(dll)
    return {e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}


def scan(root="."):
    found = collections.defaultdict(list)
    for base, dirs, files in os.walk(root):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for f in files:
            if not f.endswith((".cpp", ".h")):
                continue
            path = os.path.join(base, f)
            try:
                text = open(path, encoding="utf-8", errors="surrogateescape").read()
            except OSError:
                continue
            for m in PATTERN.finditer(text):
                name = concat_literal(text, m.end())
                if not name:
                    continue
                line = text[:m.start()].count("\n") + 1
                found[name].append(f"{path.replace(os.sep, '/')}:{line}")
    return found


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dll", default=DEFAULT_DLL)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    exported = exported_names(args.dll)
    used = scan()
    missing = {n: locs for n, locs in used.items() if n not in exported}

    if not args.quiet:
        print(f"{len(used)} distinct ResolveExport names; "
              f"{len(missing)} are NOT in {os.path.basename(args.dll)}\n")
        for name in sorted(missing):
            print(f"  {name}")
            for loc in sorted(missing[name]):
                print(f"      {loc}")
    return 1 if missing else 0


if __name__ == "__main__":
    sys.exit(main())
