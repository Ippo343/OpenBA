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
    blob = rd(img, va, 0x100)
    # `TEST DL,2` (F6 C2 02) then the JZ that splits array from scalar. The
    # branch is rel8 in every one of the 37 (the array half is under 0x40
    # bytes); rel32 is accepted so the check does not depend on that.
    t = blob.find(b"\xf6\xc2\x02")
    if t < 0:
        return {"err": "no TEST DL,2 -- not the deleting-destructor shape"}
    j = t + 3
    if blob[j] == 0x74:
        out["scalar"] = va + j + 2 + struct.unpack_from("<b", blob, j + 1)[0]
        j += 2
    elif blob[j:j + 2] == b"\x0f\x84":
        out["scalar"] = rel32_target(img, va + j, 6)
        j += 6
    else:
        return {"err": "TEST DL,2 is not followed by a JZ"}
    arr = blob[j: out["scalar"] - va]

    # EVERY field below is anchored to a preceding instruction rather than found
    # by scanning for an opcode byte. A bare scan is wrong here and the first
    # draft of this script proved it: `CALL 0x18007ec98` from 0x18004d8b0
    # encodes as E8 BA 13 03 00, and the BA inside that displacement read as a
    # second `MOV EDX,imm32`. Same trap as
    # reference_ghidra_c_output_vs_disassembly.md one level down -- bytes are
    # not instructions until something says where they start.

    # count = *(int32*)(this - 8)   ->  MOV R8D,[RCX-8] = 44 8B 41 F8
    p_cnt = arr.find(b"\x44\x8b\x41\xf8")
    out["count_from_cookie"] = p_cnt >= 0
    # LEA R9,[rip+d]  ->  4C 8D 0D d32   (the per-element destructor)
    m = [i for i in range(len(arr) - 6) if arr[i:i + 3] == b"\x4c\x8d\x0d"]
    out["n_lea_r9"] = len(m)
    out["dtor"] = rel32_target(img, va + j + m[0], 7) if len(m) == 1 else None
    # MOV EDX,imm32 (the element size) sits immediately after whichever of those
    # two the compiler emitted second -- the only difference between the three
    # prologue variants.
    p_edx = -1
    if m and p_cnt >= 0:
        p_edx = max(m[0] + 7, p_cnt + 4)
    out["mov_edx_anchored"] = p_edx >= 0 and arr[p_edx] == 0xBA
    out["size"] = struct.unpack_from("<I", arr, p_edx + 1) [0] if out["mov_edx_anchored"] else None
    # ... and the CALL to the iterator after it, past the optional RDI spill
    # (`MOV [RSP+0x30],RDI` = 48 89 7C 24 30) the no-unwind-frame variants
    # emit there.
    out["calls"] = []
    if out["mov_edx_anchored"]:
        q = p_edx + 5
        if arr[q:q + 5] == b"\x48\x89\x7c\x24\x30":
            q += 5
        if arr[q] == 0xE8:
            out["calls"].append(rel32_target(img, va + j + q, 5))
    # LEA RCX,[RBX-8] then CALL operator delete; LEA RAX,[RBX-8] is the return.
    p_free = arr.find(b"\x48\x8d\x4b\xf8")
    out["free_adjusts"] = p_free >= 0
    if p_free >= 0 and arr[p_free + 4] == 0xE8:
        out["calls"].append(rel32_target(img, va + j + p_free + 4, 5))
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


def is_memfree_shaped(img, va):
    """True when `va` is the compiler's `operator delete`:

        if (p) { Enter(alloc->cs); Free(_singleton, p); Leave(alloc->cs); }

    A raw byte compare against 0x1800c7ad0 does NOT work and the first draft of
    this script failed all 37 rows because of it: the four copies differ in the
    RIP-relative displacements of `MOV RBX,[_singleton]` and the two IAT calls,
    which is exactly the reason lane D had to hash MNEMONICS rather than bytes.
    So this resolves each RIP operand to its absolute target and compares those.
    """
    b = rd(img, va, 0x5C)
    if b[0:3] != b"\x48\x85\xc9" or b[3] not in (0x74, 0x0F):
        return False
    want_data = {0x1801FC718}                 # ?_singleton@MemoryAllocator@System@@
    want_iat = {0x18014B390, 0x18014B388}     # Enter/LeaveCriticalSection
    got_data, got_iat, got_call = set(), set(), set()
    for i in range(len(b) - 6):
        if b[i:i + 3] in (b"\x48\x8b\x1d", b"\x48\x8b\x0d"):
            got_data.add(rel32_target(img, va + i, 7))
        elif b[i:i + 2] == b"\xff\x15":
            got_iat.add(rel32_target(img, va + i, 6))
    for i in range(len(b) - 4):
        if b[i] == 0xE8:
            got_call.add(rel32_target(img, va + i, 5))
    return (want_data <= got_data and want_iat <= got_iat
            and 0x1800C6C00 in got_call)     # ?Free@MemoryAllocator@System@@


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
            if r["n_lea_r9"] != 1 or not r["mov_edx_anchored"]:
                problems.append("not the single-parameter shape (%d LEA R9, "
                                "MOV EDX anchored=%s)"
                                % (r["n_lea_r9"], r["mov_edx_anchored"]))
            if not r["count_from_cookie"]:
                problems.append("count is not read from [this-8]")
            if EHVEC not in r["calls"]:
                problems.append("array branch does not call the EH vector iterator "
                                "0x%x (calls %s)" % (EHVEC, [hex(c) for c in r["calls"]]))
            for c in r["calls"]:
                if c == EHVEC:
                    continue
                if not is_memfree_shaped(img, c):
                    problems.append("array-branch free 0x%x is not the "
                                    "operator delete shape MemFree 0x%x has"
                                    % (c, MEMFREE_CANON))
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
