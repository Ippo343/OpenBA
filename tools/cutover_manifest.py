#!/usr/bin/env python3
"""cutover_manifest.py -- Wave 25 lane MANIFEST.

THE PROBLEM THIS SOLVES
-----------------------
41 of the runtime census gap rows carry an `RT_UNPROMOTABLE` tag.  That tag is
an adjudication made **on the DLL-present route**, and `UNPROMOTABLE` is
ROUTE-SCOPED, not permanent: for most of those rows the verdict INVERTS the
moment `Runtime-x64.dll` is no longer loaded, because the storage the row names
ceases to exist and our mint becomes its only owner.

Two rows tagged with the identical words `NONE, EVER` have meant opposite
things before (`reference_unpromotable_is_route_scoped_not_permanent.md`), so
this tool does not read the prose.  It reads the CANONICAL TOKENS
(`[SCOPE: ...]`, `[ADDRESSEE: ...]`) plus the REASON CLASS field, and it
measures two things the prose cannot state:

  * whether the row exists in the MINT at all -- an unminted row is one the
    blanket `BANISHED_RT_MINT=1` switch cannot publish, and at cutover a
    by-name door on it resolves NULL (this is exactly how Wave 24 lane CUTOVER
    found `??_7Stream@Audio@@6B@`); and

  * how many BY-NAME DOORS the tree holds on the symbol.  **NO-OP is not the
    same as NOT-PUBLISHING.**  A row whose promotion could never change a
    dispatch can still be the symbol 13 files ask `ResolveExport` for, and with
    no DLL loaded `ResolveExport` returns NULL for an unpublished row.

OUTPUT
------
  --table      the human manifest (markdown), one row per tagged gap row
  --inc        the generated C++ manifest consumed by
               standalone/cutover_manifest.cpp
  --check      re-derive and FAIL (exit 1) if the checked-in .inc is stale, or
               if any REQUIRED row is not a minted row

WHAT IT DOES NOT DO
-------------------
It does not adjudicate.  Every disposition below is derived from a tag some
lane wrote plus a measurement; where a tag is silent the row is reported
UNDECLARED and the tool says so rather than guessing.
"""

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

TAG_FILE = os.path.join("standalone", "reg_mint_promoted.cpp")
MINT_FILE = os.path.join("standalone", "rt_data_mint_tables.inc")
GEN_INC = os.path.join("standalone", "cutover_manifest.inc")

# Directories a by-name door can live in.  Scoped deliberately: an unscoped
# `grep -rn .` in a wave reads ~8x high because the worktrees are siblings
# (WAVE25_KICKOFF section 1e).
DOOR_DIRS = ["runtime", "code", "standalone", "engine", "dx11"]
# The mint's own files are not doors -- they are the publisher.
DOOR_EXCLUDE = {
    os.path.normpath(TAG_FILE),
    os.path.normpath(MINT_FILE),
    os.path.normpath(os.path.join("standalone", "rt_data_mint.cpp")),
    os.path.normpath(os.path.join("standalone", "cutover_manifest.cpp")),
    os.path.normpath(os.path.join("standalone", "cutover_manifest.inc")),
}


# ---------------------------------------------------------------------------
# 1. The tags
# ---------------------------------------------------------------------------
def parse_tags(root):
    """Same regex runtime_census.py:load_unpromotable uses, plus the tokens.

    Deliberately the SAME expression, so a row this tool can see is exactly a
    row the census can see.  If they ever diverge the two figures would
    silently disagree, which is the shape of defect this project keeps paying
    for.
    """
    path = os.path.join(root, TAG_FILE)
    rows = {}
    with open(path, "r", encoding="utf-8", errors="replace") as fh:
        for lineno, line in enumerate(fh, 1):
            m = re.search(r"RT_UNPROMOTABLE:\s*(\?\S+)\s*\|\s*([^|]+?)\s*\|"
                          r"\s*([^|]+?)\s*\|\s*([^|]+?)\s*(?:\|\s*(\S.*?)\s*)?$",
                          line)
            if not m:
                continue
            sym, slots, doc, expiry, cls = m.groups()
            cls = (cls or "NO-OP").strip()
            scope = re.search(r"\[SCOPE:\s*([^\]]+)\]", expiry)
            addressee = re.search(r"\[ADDRESSEE:\s*([^\]]+)\]", expiry)
            rows[sym] = {
                "symbol": sym,
                "lineno": lineno,
                "slots": slots.strip(),
                "doc": doc.strip(),
                "expiry": expiry.strip(),
                "class": cls,
                "scope": scope.group(1).strip() if scope else None,
                "addressee": addressee.group(1).strip() if addressee else None,
            }
    return rows


# ---------------------------------------------------------------------------
# 2. Which tagged rows are GAP rows, and in which namespace
# ---------------------------------------------------------------------------
def census_gap_rows(root, census_txt=None):
    """Run tools/runtime_census.py --unported and keep the tagged gap rows.

    The census is the population authority: a tag can exist for a row that is
    NOT in the gap (it was promoted, or it is not called), and such a row is
    not this manifest's business.
    """
    if census_txt:
        with open(census_txt, "r", encoding="utf-8", errors="replace") as fh:
            out = fh.read()
    else:
        cmd = [sys.executable, os.path.join(root, "tools", "runtime_census.py"),
               "--unported"]
        p = subprocess.run(cmd, cwd=root, capture_output=True, text=True,
                           errors="replace")
        if p.returncode != 0:
            sys.stderr.write(p.stderr)
            raise SystemExit("runtime_census.py failed")
        out = p.stdout

    gap = {}
    cur = None
    for line in out.splitlines():
        m = re.match(r"^  (export|internal|ordinal)\s+(\S+)\s+(\S+)(\s+\[.*)?$",
                     line)
        if m:
            kind, ns, sym, tag = m.groups()
            cur = sym
            gap[sym] = {"ns": ns, "kind": kind, "tagged": bool(tag),
                        "consumers": []}
            continue
        if cur and line.startswith("           ") and "expires:" not in line:
            files = [f.strip() for f in line.strip().split(",") if f.strip()]
            if files and "/" in files[0]:
                gap[cur]["consumers"] = files
        if not line.startswith("  "):
            cur = None
    return gap


# ---------------------------------------------------------------------------
# 3. Mint coverage
# ---------------------------------------------------------------------------
def parse_mint(root):
    """Symbol -> (bucket, dllVa) from the GENERATED mint tables.

    Only the three initialiser arrays are read; the commentary above them
    names symbols that are NOT rows (adjacent-export annotations), and reading
    those would over-report coverage.
    """
    path = os.path.join(root, MINT_FILE)
    text = open(path, "r", encoding="utf-8", errors="replace").read()
    out = {}
    for name, bucket in (("kMintVtables", "vtable"),
                         ("kMintTypeInfos", "typeinfo"),
                         ("kMintStorage", "storage")):
        m = re.search(re.escape(name) + r"\[\]\s*=\s*\{(.*?)\n\};", text, re.S)
        if not m:
            continue
        for row in re.finditer(r'\{\s*"([^"]+)"\s*,\s*(0x[0-9a-fA-F]+)ull',
                               m.group(1)):
            out[row.group(1)] = (bucket, int(row.group(2), 16))
    return out


# ---------------------------------------------------------------------------
# 4. By-name doors
# ---------------------------------------------------------------------------
def scan_doors(root, symbols):
    """symbol -> sorted list of source files holding the symbol as a LITERAL.

    A literal grep is blind to the same fact spelled through a variable
    (`reference_literal_grep_blind_to_variables.md`), and this tool ANNOUNCES
    that blindness rather than leaving it implicit: see the manifest footer.
    """
    hits = {s: set() for s in symbols}
    wanted = {s: '"' + s + '"' for s in symbols}
    for d in DOOR_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            continue
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith((".cpp", ".h", ".inc", ".c", ".hpp")):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.normpath(os.path.relpath(full, root))
                if rel in DOOR_EXCLUDE:
                    continue
                try:
                    text = open(full, "r", encoding="utf-8",
                                errors="replace").read()
                except OSError:
                    continue
                for sym, needle in wanted.items():
                    if needle in text:
                        hits[sym].add(rel.replace("\\", "/"))
    return {s: sorted(v) for s, v in hits.items()}


# ---------------------------------------------------------------------------
# 5. The disposition -- derived, never asserted
# ---------------------------------------------------------------------------
#   REQUIRED    the tag's own words say the verdict INVERTS at cutover, or the
#               row has by-name doors that would resolve NULL with no DLL.
#   SAFE        publishing changes nothing on any route; no door needs it.
#               (Registered anyway by the blanket switch; recorded as no-action.)
#   WITHHELD    the tag scopes the harm PERMANENT -- cutover does not lift it.
#   CONDITIONAL HARMFUL, scoped DLL-PRESENT, but the tag never states that the
#               row inverts -- so nobody has adjudicated it FOR cutover. A
#               scope token says where the harm was measured; it is not a
#               licence to publish. This bucket exists because the alternative
#               was to call such a row SAFE, which is the flattering answer and
#               therefore the wrong one.
#   UNDECLARED  the tag carries no SCOPE token; this tool will not guess.
INVERTS_RE = re.compile(r"INVERTS AT CUTOVER|REQUIRED at cutover|"
                        r"at cutover .{0,120}?REQUIRED|the row is (then )?REQUIRED|"
                        r"is REQUIRED, not harmful|row is REQUIRED", re.I)


def disposition(tag, doors, minted):
    scope = tag["scope"]
    cls = tag["class"].upper()
    harmful = cls.startswith("HARMFUL")
    inverts = bool(INVERTS_RE.search(tag["expiry"]))

    if scope is None:
        return "UNDECLARED", "no [SCOPE:] token -- this tool will not guess"
    if scope == "PERMANENT":
        if harmful:
            return "WITHHELD", "harm is PERMANENT; cutover does not lift it"
        if doors:
            return ("REQUIRED",
                    "NO-OP is permanent, but %d by-name door(s) resolve NULL "
                    "with no DLL loaded" % len(doors))
        return "SAFE", "NO-OP on every route and no by-name door"
    # scope == DLL-PRESENT: the adjudication is scoped to the DLL being there.
    if inverts:
        return "REQUIRED", "tag states the verdict inverts at cutover"
    if doors:
        return ("REQUIRED",
                "verdict is DLL-PRESENT only and %d by-name door(s) would "
                "resolve NULL at cutover" % len(doors))
    if harmful:
        return ("CONDITIONAL",
                "HARMFUL, scoped DLL-PRESENT, but the tag never states an "
                "inversion -- nobody has adjudicated this row FOR cutover")
    return ("SAFE",
            "NO-OP, scoped DLL-PRESENT; no door and no stated inversion")


def build(root, census_txt=None):
    tags = parse_tags(root)
    gap = census_gap_rows(root, census_txt)
    mint = parse_mint(root)
    tagged_gap = {s: t for s, t in tags.items() if s in gap}
    doors = scan_doors(root, sorted(tagged_gap))

    rows = []
    for sym, tag in sorted(tagged_gap.items(),
                           key=lambda kv: (gap[kv[0]]["ns"], kv[0])):
        d = doors.get(sym, [])
        m = mint.get(sym)
        disp, why = disposition(tag, d, m)
        rows.append({
            "symbol": sym,
            "ns": gap[sym]["ns"],
            "class": tag["class"],
            "scope": tag["scope"] or "(none)",
            "addressee": tag["addressee"] or "(none)",
            "expiry": tag["expiry"],
            "doors": d,
            "mint": m,
            "disposition": disp,
            "why": why,
            "lineno": tag["lineno"],
        })
    return rows, tags, gap, mint


# ---------------------------------------------------------------------------
# 6. Emitters
# ---------------------------------------------------------------------------
def emit_inc(rows):
    out = []
    out.append("// GENERATED by tools/cutover_manifest.py -- DO NOT EDIT BY HAND.")
    out.append("// Regenerate:  python tools/cutover_manifest.py --inc")
    out.append("// Verify:      python tools/cutover_manifest.py --check")
    out.append("//")
    out.append("// One row per RT_UNPROMOTABLE-tagged census gap row.  The")
    out.append("// disposition is DERIVED from the row's own [SCOPE:] token, its")
    out.append("// REASON CLASS and a measured by-name-door count -- never asserted")
    out.append("// here.  standalone/cutover_manifest.cpp consumes this.")
    out.append("")
    n = {}
    for r in rows:
        n[r["disposition"]] = n.get(r["disposition"], 0) + 1
    out.append("// " + " | ".join("%s %d" % kv for kv in sorted(n.items()))
               + "   (of %d rows)" % len(rows))
    out.append("")
    for r in rows:
        mint = r["mint"]
        out.append("// %s  %s / %s  doors=%d  -- %s"
                   % (r["ns"], r["class"], r["scope"], len(r["doors"]),
                      r["why"]))
        out.append('CUTOVER_ROW("%s", CUTOVER_%s, %s, 0x%xull, "%s", %d)'
                   % (r["symbol"], r["disposition"],
                      "true" if mint else "false",
                      mint[1] if mint else 0,
                      r["ns"], len(r["doors"])))
    out.append("")
    return "\n".join(out)


def emit_table(rows):
    L = []
    n = {}
    byns = {}
    for r in rows:
        n[r["disposition"]] = n.get(r["disposition"], 0) + 1
        byns.setdefault(r["ns"], []).append(r)
    L.append("# CUTOVER PUBLISH MANIFEST -- %d tagged gap rows" % len(rows))
    L.append("")
    L.append(" | ".join("%s %d" % kv for kv in sorted(n.items())))
    L.append("by namespace: " + " | ".join(
        "%s %d" % (k, len(v)) for k, v in sorted(byns.items())))
    L.append("")
    L.append("| # | ns | row | class | scope | minted | doors | cutover | why |")
    L.append("|---|----|-----|-------|-------|--------|-------|---------|-----|")
    for i, r in enumerate(rows, 1):
        L.append("| %d | %s | `%s` | %s | %s | %s | %d | **%s** | %s |"
                 % (i, r["ns"], r["symbol"], r["class"], r["scope"],
                    r["mint"][0] if r["mint"] else "**NO**",
                    len(r["doors"]), r["disposition"], r["why"]))
    L.append("")
    unminted = [r for r in rows if not r["mint"]]
    if unminted:
        L.append("## ROWS WITH NO MINTED STORAGE (the blanket switch cannot "
                 "publish these)")
        for r in unminted:
            L.append("- `%s` (%s) -- %s" % (r["symbol"], r["ns"],
                                            r["disposition"]))
        L.append("")
    L.append("## DOORS PER ROW")
    for r in rows:
        if r["doors"]:
            L.append("- `%s`: %s" % (r["symbol"], ", ".join(r["doors"])))
    L.append("")
    L.append("## BLINDNESS THIS TOOL DECLARES")
    L.append("- The door scan matches the symbol as a STRING LITERAL in "
             + ", ".join(DOOR_DIRS) + ". A door spelled through a variable or "
             "a named constant is invisible to it, and it cannot tell you so "
             "(`reference_literal_grep_blind_to_variables.md`). A door count "
             "of 0 is therefore a LOWER bound and never a proof of absence.")
    L.append("- The disposition is derived from tags other lanes wrote. This "
             "tool re-derives the DERIVATION, not the underlying measurement.")
    L.append("- Population: the tagged rows the census reports in the gap. A "
             "tag on a row that is not called, or already promoted, is out of "
             "scope by construction.")
    return "\n".join(L)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--table", action="store_true", help="markdown manifest")
    ap.add_argument("--inc", action="store_true",
                    help="write standalone/cutover_manifest.inc")
    ap.add_argument("--check", action="store_true",
                    help="fail if the checked-in .inc is stale or a REQUIRED "
                         "row is unminted")
    ap.add_argument("--census", help="reuse a saved runtime_census --unported "
                                     "capture instead of re-running it")
    ap.add_argument("--root", default=ROOT)
    args = ap.parse_args()

    rows, tags, gap, mint = build(args.root, args.census)

    if args.inc or args.check:
        text = emit_inc(rows) + "\n"
        path = os.path.join(args.root, GEN_INC)
        if args.check:
            have = ""
            if os.path.exists(path):
                have = open(path, "r", encoding="utf-8").read()
            bad = 0
            if have != text:
                print("STALE: %s does not match a fresh derivation" % GEN_INC)
                bad = 1
            for r in rows:
                if r["disposition"] == "REQUIRED" and not r["mint"]:
                    print("UNMINTED REQUIRED ROW: %s (%s) -- the blanket "
                          "BANISHED_RT_MINT switch cannot publish it and a "
                          "cutover door on it resolves NULL"
                          % (r["symbol"], r["ns"]))
                    bad = 1
                if r["disposition"] == "UNDECLARED":
                    print("UNDECLARED SCOPE: %s (%s) -- close its tag with a "
                          "[SCOPE:] token" % (r["symbol"], r["ns"]))
                    bad = 1
            n = {}
            for r in rows:
                n[r["disposition"]] = n.get(r["disposition"], 0) + 1
            print("cutover_manifest: %d tagged gap rows -- %s"
                  % (len(rows), ", ".join("%s %d" % kv
                                          for kv in sorted(n.items()))))
            return bad
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(text)
        print("wrote %s (%d rows)" % (GEN_INC, len(rows)))

    if args.table or not (args.inc or args.check):
        print(emit_table(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
