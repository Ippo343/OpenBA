#!/usr/bin/env python3
"""Step 1 of the cutover closure: turn the census into a root set.

    tools/ghidra_mcp/.venv/Scripts/python.exe tools/cutover_closure/build_roots.py

Runs tools/runtime_census.py --csv, then maps every row to a Runtime-x64.dll
address (export rows through the DLL's real export table with pefile, address
rows as-is) and writes:

    docs/runtime_port/closure/census.csv   the census rows this run used
    docs/runtime_port/closure/roots.txt    one hex address per line
    docs/runtime_port/closure/ported.txt   the subset the census marks ported

Step 2 is CutoverClosure.java (run it in Ghidra against Runtime-x64.dll); step
3 is report.py.  See README.md.
"""
import csv
import os
import subprocess
import sys

import pefile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(REPO, "docs", "runtime_port", "closure")
DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000


def main():
    os.makedirs(OUT, exist_ok=True)
    census = os.path.join(OUT, "census.csv")
    subprocess.check_call([sys.executable, os.path.join(REPO, "tools", "runtime_census.py"),
                           "--csv", census], stdout=subprocess.DEVNULL)

    pe = pefile.PE(DLL, fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    by_name, by_ord = {}, {}
    for e in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if e.name:
            by_name[e.name.decode()] = BASE + e.address
        by_ord[e.ordinal] = BASE + e.address

    roots, ported, missing = set(), set(), []
    for r in csv.DictReader(open(census)):
        k, key = r["kind"], r["key"]
        addr = None
        if k == "addr":
            addr = int(key, 16)
        elif k == "export":
            addr = by_name.get(key)
        elif k == "ordinal":
            addr = by_ord.get(int(key))
        if addr is None:
            missing.append(key)
            continue
        roots.add(addr)
        if r["ported"] == "1":
            ported.add(addr)

    # The minted vtable surface (2026-09-12). At cutover every exported vtable
    # is rebuilt by standalone/rt_data_mint.cpp from rt_data_mint_tables.inc,
    # and a slot the registry cannot fill becomes a named trap. Those slot
    # targets are reachable by definition -- retail dispatches through them --
    # but the census never names them, so the first closure said "0 to port"
    # while the gate kept dying one class per boot. Every non-purecall slot
    # address in the mint table is therefore a root.
    mint = os.path.join(REPO, "standalone", "rt_data_mint_tables.inc")
    mint_slots = 0
    if os.path.exists(mint):
        import re as _re
        text = open(mint, encoding="utf-8", errors="ignore").read()
        slot_re = _re.compile(r'\{ (nullptr|"[^"]*"), 0x([0-9a-fA-F]+)ull, (RT_MINT_\w+) \}')
        for _name, addr, kind in slot_re.findall(text):
            if "PURE" in kind:
                continue
            roots.add(int(addr, 16))
            mint_slots += 1

    with open(os.path.join(OUT, "roots.txt"), "w") as f:
        f.write("\n".join("%x" % a for a in sorted(roots)) + "\n")

    # The ported set comes from the LIVE registry when a cutover run has left
    # one behind (tools/cutover_run.sh arms BANISHED_RT_CENSUS=1, which prints
    # every registered export/address as "[RtCensus] ..." to stderr).  The
    # census's source parse undercounted it by ~600 rows on 2026-09-12 because
    # it guesses at env gates; the process does not guess.
    live_log = os.path.join(REPO, "docs", "runtime_port", "cutover", "latest", "standalone_error.log")
    source = "census source parse"
    if os.path.exists(live_log):
        import re
        log = open(live_log, encoding="utf-8", errors="ignore").read()
        live_exports = set(re.findall(r"\[RtCensus\] export (\S+)", log))
        live_addrs = set(int(a, 16) for a in re.findall(r"\[RtCensus\] addr (0x[0-9a-fA-F]+)", log))
        live_ords = set(int(o) for o in re.findall(r"\[RtCensus\] ordinal (\d+)", log))
        if live_exports or live_addrs:
            live = set(live_addrs)
            for n in live_exports:
                if n in by_name:
                    live.add(by_name[n])
            for o in live_ords:
                if o in by_ord:
                    live.add(by_ord[o])
            ported = live
            source = f"live registry ({live_log})"
    with open(os.path.join(OUT, "ported.txt"), "w") as f:
        f.write("\n".join("%x" % a for a in sorted(ported)) + "\n")
    print(f"roots={len(roots)} (incl. {mint_slots} minted vtable slots) ported={len(ported)} (from {source}) unresolved={len(missing)}")
    for m in missing[:20]:
        print("  unresolved:", m)
    print("next: run tools/cutover_closure/CutoverClosure.java in Ghidra (program Runtime-x64.dll)")


if __name__ == "__main__":
    main()
