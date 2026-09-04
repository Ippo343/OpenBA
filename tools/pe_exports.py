#!/usr/bin/env python3
"""Tiny PE export-table reader: exports(path) -> sorted list of (rva, name).

Used by sample_report.py to attribute Runtime-x64.dll sample addresses to the nearest
export when the sampler recorded them as module+offset.
"""
import bisect
import struct


def exports(path):
    with open(path, "rb") as f:
        data = f.read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b"PE\0\0"
    coff = e_lfanew + 4
    nsec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    magic = struct.unpack_from("<H", data, opt)[0]
    assert magic == 0x20B, "PE32+ only"
    dd_off = opt + 112  # export directory entry (index 0)
    exp_rva, exp_size = struct.unpack_from("<II", data, dd_off)
    secs = []
    so = opt + opt_size
    for i in range(nsec):
        s = so + i * 40
        vsize, vaddr, rsize, raddr = struct.unpack_from("<IIII", data, s + 8)
        secs.append((vaddr, max(vsize, rsize), raddr))

    def rva2off(rva):
        for vaddr, size, raddr in secs:
            if vaddr <= rva < vaddr + size:
                return raddr + (rva - vaddr)
        raise ValueError("rva %x not in any section" % rva)

    ed = rva2off(exp_rva)
    (_, _, _, _, _, _, nfuncs, nnames, afuncs, anames, aords) = struct.unpack_from("<IIHHIIIIIII", data, ed)
    funcs = struct.unpack_from("<%dI" % nfuncs, data, rva2off(afuncs))
    names = struct.unpack_from("<%dI" % nnames, data, rva2off(anames))
    ords = struct.unpack_from("<%dH" % nnames, data, rva2off(aords))
    out = []
    for i in range(nnames):
        o = ords[i]
        if o >= nfuncs:
            continue
        rva = funcs[o]
        if exp_rva <= rva < exp_rva + exp_size:
            continue  # forwarder
        no = rva2off(names[i])
        end = data.index(b"\0", no)
        out.append((rva, data[no:end].decode("ascii", "replace")))
    out.sort()
    return out


class Resolver:
    def __init__(self, path):
        self.ex = exports(path)
        self.rvas = [r for r, _ in self.ex]

    def nearest(self, off):
        i = bisect.bisect_right(self.rvas, off) - 1
        if i < 0:
            return None, off
        rva, name = self.ex[i]
        return name, off - rva


if __name__ == "__main__":
    import sys
    r = Resolver(sys.argv[1])
    for a in sys.argv[2:]:
        print(hex(int(a, 16)), r.nearest(int(a, 16)))
