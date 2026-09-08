#!/usr/bin/env python3
"""
Wave 9 lane K -- verify the deleting-destructor family against Runtime-x64.dll.

The 37 slot-0 entries of lane C2's minted vtables are all ONE compiler-generated
body (MSVC's ??_E VECTOR deleting destructor, not the ??_G scalar one the brief
assumed), parameterized by exactly two constants: the per-class destructor and
the element size.

This script does not trust anyone's transcription of those two constants: it
re-reads every one of the 37 bodies out of the shipped DLL, extracts the
constants from the machine code, and checks them against the table in
runtime/src/object/deleting_dtor.cpp. It is the PER-ADDRESS half of this lane's
oracle -- the runtime probe verifies the BODY, this verifies the 37 ROWS.

    tools/ghidra_mcp/.venv/Scripts/python.exe tools/rt_deleting_dtor_verify.py

Exit 0 = every row agrees with the DLL. Non-zero = a real finding.
"""
import os
import re
import struct
import sys

DLL = r"C:\GOG Games\Banished\Runtime-x64.dll"
BASE = 0x180000000
EHVEC = 0x18007EC98          # MSVC "eh vector destructor iterator"
# The ten addresses the compiler emitted for operator delete; every one is
# byte-identical to ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0).
MEMFREE_CANON = 0x1800C7AD0

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "..", "runtime", "src", "object", "deleting_dtor.cpp")


def load():
    import pefile
    pe = pefile.PE(DLL, fast_load=True)
    img = bytearray(pe.OPTIONAL_HEADER.SizeOfImage)
    for s in pe.sections:
        d = s.get_data()
        img[s.VirtualAddress:s.VirtualAddress + len(d)] = d
    return bytes(img)


def rd(img, va, n):
    return img[va - BASE: va - BASE + n]


def rel32_target(img, va, oplen):
    """Target of an instruction at va whose last 4 bytes are a rel32."""
    d = struct.unpack_from("<i", rd(img, va + oplen - 4, 4))[0]
    return va + oplen + d


def scan(img, va):
    """Extract the parameterization of one deleting destructor at va.

    Reads only the ARRAY branch [entry, JZ target), which is the half whose
    shape is identical in all 37 and which carries both constants.
    """
    out = {}
    blob = rd(img, va, 0x80)
    # The one JZ rel32 (0F 84) in the prologue splits array from scalar.
    j = blob.find(b"\x0f\x84")
    if j < 0:
        return {"err": "no JZ rel32 in the first 0x80 bytes"}
    out["scalar"] = rel32_target(img, va + j, 6)
    arr = blob[j + 6: out["scalar"] - va]

    # count = *(int32*)(this - 8)   ->  MOV R8D,[RCX-8] = 44 8B 41 F8
    out["count_from_cookie"] = b"\x44\x8b\x41\xf8" in arr
    # LEA R9,[rip+d]  ->  4C 8D 0D d32   (the per-element destructor)
    m = [i for i in range(len(arr) - 6) if arr[i:i + 3] == b"\x4c\x8d\x0d"]
    out["n_lea_r9"] = len(m)
    out["dtor"] = rel32_target(img, va + j + 6 + m[0], 7) if len(m) == 1 else None
    # MOV EDX,imm32  ->  BA imm32       (the element size)
    m2 = [i for i in range(len(arr) - 4) if arr[i] == 0xBA]
    out["n_mov_edx"] = len(m2)
    out["size"] = struct.unpack_from("<I", arr, m2[0] + 1)[0] if len(m2) == 1 else None
    # the CALL rel32 in the array branch: the iterator, then operator delete
    out["calls"] = [rel32_target(img, va + j + 6 + i, 5)
                    for i in range(len(arr) - 4) if arr[i] == 0xE8]
    # LEA RCX,[RBX-8] before the free, LEA RAX,[RBX-8] as the return value
    out["free_adjusts"] = b"\x48\x8d\x4b\xf8" in arr
    out["returns_adjusted"] = b"\x48\x8d\x43\xf8" in arr
    # TEST SIL,1 / TEST DL,1 -- the free is conditional on bit 0
    out["free_conditional"] = (b"\x40\xf6\xc6\x01" in arr) or (b"\xf6\xc2\x01" in arr)

    # scalar branch: does it CALL the same destructor (K1), or restamp inline?
    sb = rd(img, out["scalar"], 0x60)
    out["scalar_calls_dtor"] = False
    for i in range(len(sb) - 4):
        if sb[i] == 0xE8 and rel32_target(img, out["scalar"] + i, 5) == out["dtor"]:
            out["scalar_calls_dtor"] = True
            break
    # LEA RAX,[rip+vt] ; MOV [RCX],RAX  -> the vtable the scalar branch restamps
    out["scalar_vtable"] = None
    if sb[0:3] == b"\x48\x8d\x05" and sb[7:10] == b"\x48\x89\x01":
        out["scalar_vtable"] = rel32_target(img, out["scalar"], 7)
    return out


def dtor_vtable(img, va):
    """The vtable a per-class destructor restamps, if it is the first thing it
    does -- true for every restamp-only dtor and for every one that restamps
    before touching members."""
    b = rd(img, va, 0x40)
    for off in (0, 5, 9, 0x12, 0x16):        # entry, or past the usual prologues
        if b[off:off + 3] == b"\x48\x8d\x05" and b[off + 7:off + 10] == b"\x48\x89\x01":
            return rel32_target(img, va + off, 7)
    return None


def parse_table():
    """The rows as the port states them: RT_DELDTOR(addr, dtor, size, name)."""
    txt = open(SRC, encoding="utf-8").read()
    rows = re.findall(
        r'RT_DELDTOR\(\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*,'
        r'\s*(0x[0-9a-fA-F]+)\s*,\s*"([^"]*)"\s*\)', txt)
    return [(int(a, 16), int(d, 16), int(s, 16), n) for a, d, s, n in rows]


def main():
    img = load()
    rows = parse_table()
    print("rows in deleting_dtor.cpp: %d" % len(rows))
    bad = 0
    if len(rows) != 37:
        print("*** expected 37 rows, found %d" % len(rows))
        bad += 1
    k1 = k2 = 0
    for addr, dtor, size, name in rows:
        r = scan(img, addr)
        problems = []
        if r.get("err"):
            problems.append(r["err"])
        else:
            if r["dtor"] != dtor:
                problems.append("dtor: DLL says 0x%x, table says 0x%x"
                                % (r["dtor"] or 0, dtor))
            if r["size"] != size:
                problems.append("elemSize: DLL says 0x%x, table says 0x%x"
                                % (r["size"] or 0, size))
            if r["n_lea_r9"] != 1 or r["n_mov_edx"] != 1:
                problems.append("not the single-parameter shape (%d LEA R9, %d MOV EDX)"
                                % (r["n_lea_r9"], r["n_mov_edx"]))
            if not r["count_from_cookie"]:
                problems.append("count is not read from [this-8]")
            if EHVEC not in r["calls"]:
                problems.append("array branch does not call the EH vector iterator "
                                "0x%x (calls %s)" % (EHVEC, [hex(c) for c in r["calls"]]))
            for c in r["calls"]:
                if c == EHVEC:
                    continue
                if rd(img, c, 0x5C) != rd(img, MEMFREE_CANON, 0x5C):
                    problems.append("array-branch free 0x%x is NOT byte-identical to "
                                    "MemFree 0x%x" % (c, MEMFREE_CANON))
            for f in ("free_adjusts", "returns_adjusted", "free_conditional"):
                if not r[f]:
                    problems.append("array branch: %s is false" % f)
            # the scalar branch must reach the SAME destructor, by call or inline
            if r["scalar_calls_dtor"]:
                k1 += 1
            elif r["scalar_vtable"] is not None:
                dv = dtor_vtable(img, dtor)
                if dv != r["scalar_vtable"]:
                    problems.append("scalar branch restamps 0x%x but its dtor 0x%x "
                                    "restamps 0x%s"
                                    % (r["scalar_vtable"], dtor,
                                       ("%x" % dv) if dv else "?"))
                k2 += 1
            else:
                problems.append("scalar branch neither calls 0x%x nor restamps a vtable"
                                % dtor)
        print("%s0x%09x  size=0x%-4x dtor=0x%09x  %s"
              % ("OK  " if not problems else "BAD ", addr, size, dtor, name))
        for p in problems:
            print("        %s" % p)
            bad += 1
    print("")
    print("call-the-dtor (K1): %d    restamp-inline (K2/K3): %d" % (k1, k2))
    print("problems: %d" % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
