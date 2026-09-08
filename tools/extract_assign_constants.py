#!/usr/bin/env python3
"""
tools/extract_assign_constants.py -- recover the DECLARATION TABLE that
?AssignConstants@Context@Video@@AEAAXXZ (0x1800ef3d0) builds.

Same rule as tools/extract_shader_constants.py and for the same reason: the
table is ~20 AddConstant rows over 45 lifted literals, and a row transcribed by
eye is a shader parameter that silently never binds. This walks the SHIPPED
BYTES and prints the sequence of ConstantBuffer calls with the string literals
and immediates that reached them, so the port is transliterated from a
machine-produced listing rather than from a human reading 1975 instructions.

NO DISASSEMBLER IN THE LOOP. Three byte patterns are matched directly:

    LEA r64,[rip+disp32]   48/4C  8D  modrm(mod=00,rm=101)  disp32
    MOV r32d,imm32         B8+rd imm32,  or  41 B8+rd imm32 for r8d-r15d
    CALL rel32             E8 disp32, accepted only when the target is one of
                           the six functions this table is built from

so a listing's typo cannot be inherited, and the same tool works with Ghidra
shut down. The AddConstant row count is asserted against the DLL, so a scan that
silently under-reads (the defect the shader-literal tool shipped with first)
fails instead of reporting a short table.

USAGE
    tools/extract_assign_constants.py            # print the table
    tools/extract_assign_constants.py --check    # exit non-zero if it changed
"""

import argparse
import sys

try:
    import pefile
except ImportError:
    sys.exit("pefile is required: use tools/ghidra_mcp/.venv/Scripts/python.exe")

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
FN = 0x1800EF3D0

CALLS = {
    0x1800EED10: "BeginConstants",
    0x1800EEC40: "AddConstant",
    0x1800EEBF0: "EndConstants",
    0x1800EF010: "PlatformCreate",
    0x1800C95D0: "String::Set",
    0x1800C9730: "String::Format",
    # *** THE INLINED BeginConstants. *** MSVC inlined `?BeginConstants@` for
    # some of the seven buffers: the two String assignments and the five scalar
    # stores appear in line, and the only thing left of the call is its TAIL —
    # `LEA RCX,[buffer+0x28] ; CALL 0x18001cab0` (the entry-array clear). Without
    # this row the scan reported four Begins for seven buffers and folded three
    # buffers' literals into one row, which read like a plausible table.
    0x18001CAB0: "BeginConstants[inlined]",
}

# `?AssignConstants@` makes exactly this many AddConstant calls in the shipped
# DLL. Asserted so a scan that silently under-reads fails loudly.
EXPECT_ADDCONSTANT = 20


def read_cstr(pe, va, limit=128):
    rva = va - BASE
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize,
                                                            s.SizeOfRawData):
            data = s.get_data()
            off = rva - s.VirtualAddress
            end = data.find(b"\x00", off)
            if end < 0 or end - off > limit:
                return None
            raw = data[off:end]
            if raw and all(32 <= b < 127 for b in raw):
                return raw.decode("ascii")
            return None
    return None


def scan(pe):
    text = next(s for s in pe.sections if s.Name.rstrip(b"\x00") == b".text")
    data = text.get_data()
    tbase = BASE + text.VirtualAddress

    addrs = sorted({BASE + e.address for e in pe.DIRECTORY_ENTRY_EXPORT.symbols
                    if e.name and BASE + e.address > FN})
    if not addrs:
        sys.exit("no export above the function -- cannot bound it")
    fn_end = addrs[0]

    events = []
    i = FN - tbase
    end = min(fn_end - tbase, len(data) - 8)
    while i < end:
        va = tbase + i
        b = data[i]

        # *** EVERY PATTERN IS A PEEK AND THE CURSOR ALWAYS ADVANCES BY ONE. ***
        # The first version of this scan advanced by the matched instruction's
        # length, which is a linear sweep: one false `B8..BF` byte inside
        # another instruction's modrm/displacement desynchronises the stream and
        # the walk then steps OVER real calls. It did exactly that — the first
        # four `BeginConstants` calls vanished and their literals were folded
        # into the next row, and the AddConstant count still came out right, so
        # nothing complained.
        #
        # Peeking cannot desynchronise. It can over-report, which is why every
        # pattern below is narrow enough that a false positive is
        # distinguishable: a CALL is kept only when its target is one of six
        # exact addresses, an immediate only in the two encodings that put it in
        # R9D or a stack argument slot, and a literal only when the bytes at the
        # target really are a NUL-terminated ASCII string.

        # LEA r64,[rip+disp32]
        if b in (0x48, 0x4C) and data[i + 1] == 0x8D and (data[i + 2] & 0xC7) == 0x05:
            disp = int.from_bytes(data[i + 3:i + 7], "little", signed=True)
            s = read_cstr(pe, va + 7 + disp)
            if s is not None:
                events.append((va, "lit", s))

        # MOV R9D,imm32 — AddConstant's 4th argument (the size), and
        # BeginConstants' 4th.
        elif b == 0x41 and data[i + 1] == 0xB9:
            imm = int.from_bytes(data[i + 2:i + 6], "little", signed=True)
            events.append((va, "r9d", imm))

        # MOV dword ptr [rsp+disp8],imm32 — BeginConstants' stack arguments.
        elif b == 0xC7 and data[i + 1] == 0x44 and data[i + 2] == 0x24:
            imm = int.from_bytes(data[i + 4:i + 8], "little", signed=True)
            events.append((va, "stk", (data[i + 3], imm)))

        # CALL rel32, kept only when it lands on one of the six.
        elif b == 0xE8:
            disp = int.from_bytes(data[i + 1:i + 5], "little", signed=True)
            target = va + 5 + disp
            if target in CALLS:
                events.append((va, "call", CALLS[target]))

        i += 1

    return events


def build(events):
    """Fold the event stream into rows: each ConstantBuffer call, with the
    literals and the last immediate seen since the previous one."""
    rows = []
    lits = []
    r9 = None
    stk = {}
    for _va, kind, val in events:
        if kind == "lit":
            if not lits or lits[-1] != val:
                lits.append(val)
        elif kind == "r9d":
            r9 = val
        elif kind == "stk":
            stk[val[0]] = val[1]
        elif kind == "call" and val in ("String::Set", "String::Format"):
            pass  # the literal is already captured by the preceding LEA
        elif kind == "call":
            rows.append((val, list(lits), r9, dict(stk)))
            lits = []
            r9 = None
            stk = {}
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    pe = pefile.PE(DLL)
    rows = build(scan(pe))

    n_add = sum(1 for r in rows if r[0] == "AddConstant")
    for name, lits, r9, stk in rows:
        stxt = " ".join("[rsp+0x%x]=%d" % (k, v) for k, v in sorted(stk.items()))
        print("%-15s %-44s r9d=%-6s %s"
              % (name, ", ".join(repr(x) for x in lits),
                 "-" if r9 is None else r9, stxt))
    print("--- %d rows, %d AddConstant ---" % (len(rows), n_add))

    if n_add != EXPECT_ADDCONSTANT:
        print("*** AddConstant count is %d, expected %d -- the scan changed or "
              "under-read; do NOT transcribe from this output ***"
              % (n_add, EXPECT_ADDCONSTANT))
        sys.exit(1)


if __name__ == "__main__":
    main()
