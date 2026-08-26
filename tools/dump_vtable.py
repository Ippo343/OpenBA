#!/usr/bin/env python3
"""Dump a vtable straight out of Application-x64.exe's .rdata.

Reads the PE section table, maps a Ghidra VA (image base 0x140000000) to a file
offset, and prints consecutive qwords for as long as they look like .text
pointers (0x140001000 .. 0x140150200).  That "looks like code" test is what
bounds the table: retail vtables are followed by either zero padding or the next
object's RTTI pointer, neither of which lands in .text.

Usage:  python tools/dump_vtable.py 0x14015b020 [0x1401...]
"""
import struct
import sys

EXE = r"C:\GOG Games\Banished\Application-x64.exe"
IMAGE_BASE = 0x140000000
TEXT_LO = 0x140001000
TEXT_HI = 0x140150200


def load_sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    secs = []
    off = pe + 24 + optsz
    for i in range(nsec):
        s = data[off + i * 40:off + (i + 1) * 40]
        name = s[:8].rstrip(b"\0").decode()
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", s, 8)
        secs.append((name, vaddr, vsize, raddr, rsize))
    return secs


def va_to_off(secs, va):
    rva = va - IMAGE_BASE
    for name, vaddr, vsize, raddr, rsize in secs:
        if vaddr <= rva < vaddr + max(vsize, rsize):
            return raddr + (rva - vaddr)
    return None


def main():
    data = open(EXE, "rb").read()
    secs = load_sections(data)
    for arg in sys.argv[1:]:
        va = int(arg, 16)
        off = va_to_off(secs, va)
        print("== vtable @ 0x%x (file 0x%x)" % (va, off or 0))
        if off is None:
            print("   not in any section")
            continue
        i = 0
        while True:
            q = struct.unpack_from("<Q", data, off + i * 8)[0]
            if not (TEXT_LO <= q < TEXT_HI):
                print("   [%2d] +0x%02x  0x%016x  <- stop (not .text)" % (i, i * 8, q))
                break
            print("   [%2d] +0x%02x  0x%x" % (i, i * 8, q))
            i += 1
            if i > 64:
                print("   ... (64 slot cap)")
                break


if __name__ == "__main__":
    main()
