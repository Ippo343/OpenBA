#!/usr/bin/env python3
"""Wave 9 lane UIC scratch probe: for a set of Runtime-x64.dll addresses, report
whether the address is EXPORTED (and under what mangled name) and whether our
tree already registers it (by name or by address).

Not a gate and not a permanent tool -- it exists so that "satisfied" is a query
result rather than a memory, which is the rule WAVE9_LANE_UIF.md set.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pe_exports import exports  # noqa: E402

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ["runtime", "standalone", "engine", "code", "dx11"]


def load_tree():
    blob = []
    for d in SCAN_DIRS:
        p = os.path.join(ROOT, d)
        for dirpath, _, files in os.walk(p):
            for fn in files:
                if not fn.endswith((".cpp", ".h", ".inc", ".c")):
                    continue
                fp = os.path.join(dirpath, fn)
                try:
                    with open(fp, "r", encoding="utf-8", errors="replace") as f:
                        blob.append((os.path.relpath(fp, ROOT), f.read()))
                except OSError:
                    pass
    return blob


def main():
    addrs = [int(a, 16) for a in sys.argv[1:]]
    exp = {BASE + rva: name for rva, name in exports(DLL)}
    tree = load_tree()

    for a in addrs:
        name = exp.get(a)
        print("0x%x  export=%s" % (a, name or "<not exported>"))
        hexpat = re.compile(r"0x0*%x\b" % a, re.I)
        reg_addr = []
        for rel, txt in tree:
            for m in hexpat.finditer(txt):
                line = txt.count("\n", 0, m.start()) + 1
                ctx = txt[max(0, m.start() - 120):m.start() + 60].replace("\n", " ")
                tag = "REGADDR" if "RegisterRuntimeAddr" in ctx else "ref"
                reg_addr.append("      %-7s %s:%d" % (tag, rel, line))
        for r in reg_addr[:12]:
            print(r)
        if not reg_addr:
            print("      (no reference to this address anywhere in the tree)")
        if name:
            hits = []
            for rel, txt in tree:
                idx = 0
                while True:
                    i = txt.find('"' + name + '"', idx)
                    if i < 0:
                        break
                    idx = i + 1
                    line = txt.count("\n", 0, i) + 1
                    ctx = txt[max(0, i - 140):i + 40].replace("\n", " ")
                    tag = "REGNAME" if "RegisterRuntimeExport" in ctx else "namedoor"
                    hits.append("      %-8s %s:%d" % (tag, rel, line))
            for h in hits[:12]:
                print(h)
            if not hits:
                print("      (name never appears as a literal in the tree)")
        print()


if __name__ == "__main__":
    main()
