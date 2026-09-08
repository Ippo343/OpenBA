#!/usr/bin/env python3
"""tools/resolve_cache_audit.py -- audit how Runtime resolves are CACHED.

WHY
---
At cutover Runtime-x64.dll is gone, so every ResolveExport / ResolveExportRaw /
ResolveOrdinal / ResolveRuntimeRaw / ResolveAddr(Module::Runtime, ...) can
return null. What happens next depends entirely on HOW the result was stored:

  FROZEN   a function-local `static` caches the resolve. The FIRST caller fixes
           the answer for the whole process -- a null is unrepairable, and a
           registration that lands later cannot fix it.
  RETRY    `if (!g_x) g_x = Resolve(...)` re-resolves on every call, so a
           registration that lands later still works. This is the correct form
           and it ALREADY EXISTS in the tree (runtime/src/object/internal.cpp,
           runtime/src/object/type_system.cpp).
  DIRECT   the result is used without being cached at all.

ResolveAddr(Module::Exe, ...) is deliberately NOT counted: the exe does not go
away at cutover, so those doors do not change their answer.

PARSING NOTE (reference_census_three_blind_spots.md): an earlier census tool in
this project silently missed multi-line string literals and multi-line calls.
This one joins physical lines into logical STATEMENTS (comment-stripped, split
on ';' and '{' '}') before matching, so a resolve broken across three lines --
which most of them are -- is seen exactly once.

USAGE
  python tools/resolve_cache_audit.py            # summary + per-bucket listing
  python tools/resolve_cache_audit.py --frozen   # only the FROZEN sites
  python tools/resolve_cache_audit.py --selfres  # names we resolve AND register
  python tools/resolve_cache_audit.py --count    # one line, for A/B of the denominator
"""

import os
import re
import sys

ROOTS = ["code", "runtime", "engine", "standalone", "dx11"]
SKIP_DIR_PARTS = (
    os.sep + "build", os.sep + ".claude", os.sep + "obj", os.sep + ".git",
)

# The doors whose answer changes when the DLL is absent.
#
# THE MACROS ARE COUNTED TOO, and that is not decoration. RT_ORIGINAL_ONCE,
# RT_EXPORT_ONCE and RT_DATA_SLOT expand to exactly this idiom, so 282 call
# sites were invisible to the first version of this tool -- and converting a
# hand-rolled site to a macro would have made it VANISH from the audit, which
# reads identically to a clean pass. That has happened twice in this wave
# (reference_documenting_a_check_can_delete_it.md). Counting the macro spellings
# conserves the denominator: a conversion shows up as a bucket MOVE.
DOOR = re.compile(
    r"\b(ResolveExportRaw|ResolveExport|ResolveOrdinal|ResolveRuntimeRaw)\s*\("
    r"|\bResolveAddr\s*\(\s*Module::Runtime\b"
    r"|\bRT_ORIGINAL_ONCE\s*\(|\bRT_EXPORT_ONCE\s*\(|\bRT_DATA_SLOT\s*\("
)
# Macro spellings whose caching contract is fixed by the macro body, so the
# call site's own text does not decide the bucket.
MACRO_RETRY  = re.compile(r"\bRT_EXPORT_ONCE\s*\(|\bRT_DATA_SLOT\s*\(")
MACRO_FROZEN = re.compile(r"\bRT_ORIGINAL_ONCE\s*\(")
# A function-local static: `static T x = ...` indented inside a body. We do not
# try to prove it is function-local from the brace depth alone -- a file-scope
# `static T x = Resolve(...)` initialiser has the SAME defect (resolved once,
# before main in that case), so both are counted FROZEN and the listing shows
# the file:line for a human to read.
FROZEN = re.compile(r"\bstatic\b[^=]*=")
# The retry idiom, in the two spellings the tree uses.
RETRY = re.compile(r"\bif\s*\(\s*!\s*[A-Za-z_][A-Za-z0-9_]*\s*\)")

REGISTER = re.compile(r"\bRegisterRuntimeExport\s*\(\s*\"([^\"]+)\"")
RESOLVE_NAME = re.compile(
    r"\b(?:ResolveExportRaw|ResolveExport)\s*\(\s*\"([^\"]+)\"")


def strip_comments(text):
    """Remove // and /* */ comments without disturbing line count or strings."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"' or c == "'":
            q = c
            out.append(c)
            i += 1
            while i < n:
                out.append(text[i])
                if text[i] == "\\":
                    i += 1
                    if i < n:
                        out.append(text[i])
                        i += 1
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n":
                i += 1
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            i += 2
            while i + 1 < n and not (text[i] == "*" and text[i + 1] == "/"):
                if text[i] == "\n":
                    out.append("\n")   # keep line numbering
                i += 1
            i += 2
            continue
        out.append(c)
        i += 1
    return "".join(out)


def statements(text):
    """Yield (line_number, logical_statement). Joins physical lines."""
    line = 1
    buf = []
    start = 1
    for ch in text:
        if ch == "\n":
            line += 1
            buf.append(" ")
            continue
        buf.append(ch)
        if ch in ";{}":
            s = "".join(buf).strip()
            if s:
                yield start, s
            buf = []
            start = line
    s = "".join(buf).strip()
    if s:
        yield start, s


def walk():
    for root in ROOTS:
        for dirpath, dirnames, filenames in os.walk(root):
            full = os.sep + dirpath
            if any(p in full for p in SKIP_DIR_PARTS):
                dirnames[:] = []
                continue
            for fn in filenames:
                if fn.endswith((".cpp", ".h")):
                    yield os.path.join(dirpath, fn).replace("\\", "/")


def main():
    only_frozen = "--frozen" in sys.argv
    only_self = "--selfres" in sys.argv
    only_count = "--count" in sys.argv

    frozen, retry, direct = [], [], []
    registered = set()
    resolved_in_runtime = {}

    for path in walk():
        try:
            raw = open(path, "r", encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        text = strip_comments(raw)
        for lineno, stmt in statements(text):
            for m in REGISTER.finditer(stmt):
                registered.add(m.group(1))
            if not DOOR.search(stmt):
                continue
            # A statement can hold more than one door; count the STATEMENT once
            # per bucket, which is what "a site" means for this audit.
            # The macro's own body fixes the contract, so it wins over whatever
            # the call site's text happens to look like. rt_port.h itself is
            # where the macros are DEFINED, not used; skip it so the definition
            # is not counted as a site.
            if path.endswith("runtime/funcs/rt_port.h"):
                continue
            if MACRO_RETRY.search(stmt):
                bucket = retry
            elif MACRO_FROZEN.search(stmt):
                bucket = frozen
            elif RETRY.search(stmt):
                bucket = retry
            elif FROZEN.search(stmt):
                bucket = frozen
            else:
                bucket = direct
            snippet = re.sub(r"\s+", " ", stmt)[:110]
            bucket.append((path, lineno, snippet))
            for m in RESOLVE_NAME.finditer(stmt):
                resolved_in_runtime.setdefault(m.group(1), []).append(
                    (path, lineno))

    if only_count:
        print("resolve-cache audit: FROZEN=%d RETRY=%d DIRECT=%d (total doors=%d)"
              % (len(frozen), len(retry), len(direct),
                 len(frozen) + len(retry) + len(direct)))
        return 0

    if only_self:
        # Requirement 4: a name we RESOLVE and also REGISTER is a potential
        # self-resolve. ResolveExport consults the port registry FIRST
        # (reference_resolveaddr_consults_port_registry_first.md), so such a
        # site reaches OUR port, not the DLL -- and if the resolving code IS
        # that port, it recurses to stack death. ResolveExportRaw is exempt:
        # it deliberately bypasses the registry.
        print("== names that are BOTH resolved and registered ==")
        hits = 0
        for name in sorted(resolved_in_runtime):
            if name in registered:
                hits += 1
                print("  %s" % name)
                for p, l in resolved_in_runtime[name]:
                    print("      resolved at %s:%d" % (p, l))
        print("total: %d" % hits)
        return 0

    print("== FROZEN (a null is cached for the life of the process): %d ==" % len(frozen))
    for p, l, s in sorted(frozen):
        print("  %s:%d  %s" % (p, l, s))
    if not only_frozen:
        print()
        print("== RETRY (re-resolves; the correct form): %d ==" % len(retry))
        for p, l, s in sorted(retry):
            print("  %s:%d  %s" % (p, l, s))
        print()
        print("== DIRECT (not cached): %d ==" % len(direct))
        for p, l, s in sorted(direct):
            print("  %s:%d  %s" % (p, l, s))
    print()
    print("TOTALS  frozen=%d retry=%d direct=%d  doors=%d"
          % (len(frozen), len(retry), len(direct),
             len(frozen) + len(retry) + len(direct)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
