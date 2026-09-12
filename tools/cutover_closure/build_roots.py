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

    with open(os.path.join(OUT, "roots.txt"), "w") as f:
        f.write("\n".join("%x" % a for a in sorted(roots)) + "\n")
    with open(os.path.join(OUT, "ported.txt"), "w") as f:
        f.write("\n".join("%x" % a for a in sorted(ported)) + "\n")
    print(f"roots={len(roots)} ported={len(ported)} unresolved={len(missing)}")
    for m in missing[:20]:
        print("  unresolved:", m)
    print("next: run tools/cutover_closure/CutoverClosure.java in Ghidra (program Runtime-x64.dll)")


if __name__ == "__main__":
    main()
