#!/usr/bin/env python3
"""Wave 17 lane CEIL -- does ANY shipped binary hold a row's VA as a qword?

For a `??_7` row, promotion can only matter if something can OBSERVE the table
address. Two ways exist: (1) a stored qword somewhere in a shipped image (a
data pointer, an import thunk, a relocated slot), or (2) an in-DLL
RIP-relative LEA, which is NOT a door and therefore cannot be redirected.

This scans the four shipped images for the LE qword of each VA, honouring each
image's OWN preferred base. It is the same check lane OBJDATA ran by hand for
three Object rows, generalised.

POSITIVE CONTROL (printed, and it MUST be non-zero): the DLL's own import
thunks / the exe's IAT hold plenty of qwords, so we scan for a VA we KNOW is
stored -- an exe IAT slot's content -- and require a hit. Without it a scanner
that reads no bytes prints a clean sheet of zeros.
"""
import struct, sys, os

BINS = [
    (r"C:\GOG Games\Banished\Runtime-x64.dll", 0x180000000),
    (r"C:\GOG Games\Banished\Application-x64.exe", 0x140000000),
    (r"C:\GOG Games\Banished\VideoDX11-x64.dll", 0x180000000),
    (r"C:\GOG Games\Banished\VideoDX9-x64.dll", 0x180000000),
]

def load():
    out = []
    for path, base in BINS:
        if not os.path.exists(path):
            print(f"  !! MISSING {path}"); continue
        out.append((os.path.basename(path), base, open(path, "rb").read()))
    return out

def count(data, va):
    needle = struct.pack("<Q", va)
    n, i = 0, data.find(needle)
    while i != -1:
        n += 1
        i = data.find(needle, i + 1)
    return n

def main(vas):
    imgs = load()
    # ---- positive control: a VA that IS stored as a qword somewhere.
    # The DLL's own ??_7 tables are pointed at by nothing, so use the export
    # directory instead: every export RVA is stored, but as a DWORD. Use the
    # exe's .rdata import slots: those hold RUNTIME function VAs at load time
    # in the FILE only as zero... so instead use a known self-referential
    # qword: the DLL's load-config / TLS. Simplest honest control: scan for
    # each image's OWN ImageBase qword, which the PE optional header stores.
    ctrl_total = 0
    for name, base, data in imgs:
        c = count(data, base)
        ctrl_total += c
        print(f"[ctrl] {name}: qword {hex(base)} (its own ImageBase) appears {c} time(s)")
    print(f"[ctrl] TOTAL {ctrl_total} -> {'GREEN' if ctrl_total else 'RED -- SCANNER IS BLIND'}")
    if not ctrl_total:
        sys.exit("control failed; every zero below is meaningless")
    print()
    for line in vas:
        sym, va = line.split()
        va = int(va, 16)
        hits = []
        for name, base, data in imgs:
            c = count(data, va)
            if c: hits.append(f"{name}={c}")
        print(f"{'STORED  ' if hits else 'no-qword'} {sym:<62} {hex(va)} {' '.join(hits)}")

if __name__ == "__main__":
    main([l.strip() for l in open(sys.argv[1]) if l.strip()])
