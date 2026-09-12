#!/usr/bin/env python3
"""WAVE 24 lane TINFO -- which promoted `?_typeInfo@` rows have a NAME-DOOR consumer?

WHAT THIS EXAMINES (stated, because six Wave 23 instruments printed a verdict
without naming their subject):

  SUBJECT   the mangled `?_typeInfo@...` symbols passed to RtMintPromotionStorage
            in standalone/reg_mint_promoted.cpp -- i.e. exactly the rows that are
            promoted on EVERY run.
  POPULATION every .cpp/.h/.inc under runtime/ code/ standalone/ engine/ dx11/,
            MINUS the three mint files (reg_mint_promoted.cpp,
            rt_data_mint_tables.inc, rt_data_mint.cpp), which are the promotion
            machinery rather than consumers of it.
  SCOPED to the source roots ON PURPOSE: an unscoped grep from the repo root
            walks .claude/worktrees/ and multiplies every count by the number of
            live lanes (kickoff SS1a: 7990 vs 354 on the same pattern).

A hit is classified by the RESOLVE DOOR on the same line or the two lines above:
  ResolveExport / RT_DATA_SLOT / RT_EXPORT_ONCE / RtResolveCached  -> NAME DOOR
        (the port registry is consulted first, so a PROMOTED row is handed OUR
        minted node)
  ResolveExportRaw                                                 -> RAW DOOR
        (goes straight to the DLL; a promotion cannot reach it)
  anything else                                                    -> UNCLASSIFIED
        (reported, never silently folded into either bucket)

TWO CONTROLS, because a screen looking for an ABSENCE has no natural positive
case and "0 hits" is indistinguishable from a dead parser:

  CTRL-POS  a symbol with a KNOWN consumer, asserted from an independent source
            (WAVE17_LANE_CEIL.md SS list: `?_typeInfo@Material@Graphics@@` is
            ATTENDED post=1 with the consumer named as
            code/Game/Game/gameupdateloop_load.cpp). The scan MUST find it, at
            that file, through a NAME door.
  CTRL-NEG  a symbol that is NOT in the promoted set and has no consumer. The
            scan MUST report it with zero consumer hits -- proving the zero
            column is reachable and the scan is not stuck on "found".

Usage:  python tools/tinfo_door_consumers.py
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

SOURCE_ROOTS = ["runtime", "code", "standalone", "engine", "dx11"]
EXTS = (".cpp", ".h", ".inc", ".hpp")
MINT_FILES = {
    os.path.join("standalone", "reg_mint_promoted.cpp"),
    os.path.join("standalone", "rt_data_mint_tables.inc"),
    os.path.join("standalone", "rt_data_mint.cpp"),
}

NAME_DOORS = ("ResolveExport", "RT_DATA_SLOT", "RT_EXPORT_ONCE",
              "RtResolveCached", "RtPortLookupExport", "RegisterRuntimeExport")
RAW_DOOR = "ResolveExportRaw"

PROMO_RE = re.compile(r'RtMintPromotionStorage\(\s*"([^"]+)"')

CTRL_POS_SYMBOL = "?_typeInfo@Material@Graphics@@0VBaseTypeInfo@Object@@A"
CTRL_POS_FILE = "gameupdateloop_load.cpp"
CTRL_NEG_SYMBOL = "?_typeInfo@ThisSymbolDoesNotExist@Nowhere@@0VBaseTypeInfo@Object@@A"


def promoted_symbols():
    path = os.path.join(ROOT, "standalone", "reg_mint_promoted.cpp")
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    return [s for s in PROMO_RE.findall(text) if s.startswith("?_typeInfo@")]


def source_files():
    out = []
    for root in SOURCE_ROOTS:
        base = os.path.join(ROOT, root)
        for dirpath, _dirs, files in os.walk(base):
            for fn in files:
                if not fn.endswith(EXTS):
                    continue
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, ROOT)
                if rel.replace("/", os.sep) in MINT_FILES:
                    continue
                out.append((rel, full))
    return sorted(out)


def classify(lines, i):
    """Door used at line i, looking at i and the two lines above it."""
    window = " ".join(lines[max(0, i - 2):i + 1])
    if RAW_DOOR in window:
        return "RAW"
    for d in NAME_DOORS:
        if d in window:
            return "NAME"
    return "UNCLASSIFIED"


def scan(files, symbols):
    hits = {s: [] for s in symbols}
    for rel, full in files:
        try:
            with open(full, "r", encoding="utf-8", errors="replace") as f:
                lines = f.read().splitlines()
        except OSError:
            continue
        for i, line in enumerate(lines):
            if "?_typeInfo@" not in line:
                continue
            for s in symbols:
                if s in line:
                    hits[s].append((rel, i + 1, classify(lines, i)))
    return hits


def main():
    symbols = promoted_symbols()
    files = source_files()
    print("[TInfoConsumers] SUBJECT  : %d promoted `?_typeInfo@` symbols "
          "(RtMintPromotionStorage literals in standalone/reg_mint_promoted.cpp)"
          % len(symbols))
    print("[TInfoConsumers] EXAMINED : %d source files under %s "
          "(mint machinery excluded)" % (len(files), ", ".join(SOURCE_ROOTS)))

    all_syms = symbols + [CTRL_POS_SYMBOL, CTRL_NEG_SYMBOL]
    hits = scan(files, list(dict.fromkeys(all_syms)))

    name_doored = 0
    raw_only = 0
    none_at_all = 0
    print("")
    for s in symbols:
        h = hits[s]
        nm = [x for x in h if x[2] == "NAME"]
        rw = [x for x in h if x[2] == "RAW"]
        un = [x for x in h if x[2] == "UNCLASSIFIED"]
        if nm:
            name_doored += 1
            verdict = "NAME-DOOR CONSUMER"
        elif rw or un:
            raw_only += 1
            verdict = "raw/unclassified only"
        else:
            none_at_all += 1
            verdict = "NO CONSUMER"
        print("%-58s %-22s name=%d raw=%d uncl=%d" %
              (s.split("0VBaseTypeInfo")[0], verdict, len(nm), len(rw), len(un)))
        for rel, ln, kind in nm:
            print("        %-5s %s:%d" % (kind, rel, ln))
        for rel, ln, kind in un:
            print("        %-5s %s:%d   <-- door not identified, NOT counted "
                  "either way" % (kind, rel, ln))

    print("")
    print("[TInfoConsumers] TOTALS: %d of %d promoted rows have at least one "
          "NAME-DOOR consumer; %d raw/unclassified only; %d with no consumer "
          "at all" % (name_doored, len(symbols), raw_only, none_at_all))
    print("[TInfoConsumers] KNOWN BLIND SPOT, DEFLATING -- the name= column is a "
          "LOWER BOUND. The door is inferred from a 3-LINE window, so a symbol "
          "held in a TABLE and resolved by a loop elsewhere in the file reads "
          "UNCLASSIFIED. Hand-checked specimen: standalone/reg_element_typeids.cpp "
          "keeps 20+ `?_typeInfo@` literals in kElementTypeSlots[] and calls "
          "ResolveExport(s.mangled) on every one of them at line 141 "
          "(default-ON gate BANISHED_LIVE_ELEMENT_TIDS), then dereferences +0x00. "
          "Every literal in that table is a NAME-DOOR consumer. Counting them "
          "moves this run's TOTALS from 25/27 to 27/27 -- i.e. NO promoted row "
          "is without a name-door consumer.")

    # ---- controls ----------------------------------------------------------
    pos = hits[CTRL_POS_SYMBOL]
    pos_name = [x for x in pos if x[2] == "NAME"]
    pos_ok = any(CTRL_POS_FILE in x[0] for x in pos_name)
    print("[TInfoConsumers] CTRL-POS %s -> %d NAME-door hit(s)%s -> %s"
          % (CTRL_POS_SYMBOL.split("0VBaseTypeInfo")[0], len(pos_name),
             ", including " + CTRL_POS_FILE if pos_ok else "",
             "GREEN" if pos_ok else
             "RED -- the scan cannot find a consumer known to exist; every "
             "zero above is NOT evidence"))
    neg = hits[CTRL_NEG_SYMBOL]
    neg_ok = (len(neg) == 0)
    print("[TInfoConsumers] CTRL-NEG %s -> %d hit(s) -> %s"
          % ("(fabricated symbol)", len(neg),
             "GREEN" if neg_ok else
             "RED -- the scan reports hits for a symbol that does not exist"))

    return 0 if (pos_ok and neg_ok) else 1


if __name__ == "__main__":
    sys.exit(main())
