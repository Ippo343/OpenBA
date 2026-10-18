#!/usr/bin/env python3
"""tools/ext_rsc_check.py -- compare the exe's provided objects with the kit's compiled .crs.

usage: python tools/ext_rsc_check.py <game dir>/standalone_rsccheck build/ext_resources/WinData/Dialog

The KITFREE cross-check (docs/extensions/KITFREE.md section 5, block RSCTEXT). The exe, run with
`/rsccheck:1`, writes every object of its embedded .rsc text as <File>_<name>.bin
(src/diag/rsc_text_check.cpp: External::Load's stream in the writing direction, dependency list
EMPTY). The kit's .crs (tools/ext_resources.sh) holds the same stream LZF-compressed with the
compile-time dependency entries {source file, FILETIME} the exe cannot know; those are skipped,
the record length is corrected by their size, and everything else must match byte for byte.
Objects the kit did not reach (a template nothing references, e.g. Extensions.rsc:smallNote) have
no .crs and are reported as such. Exit 0 = every compared object matches.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ext_font_check import decode_crs  # noqa: E402


def split_header(b):
    """-> (record header 12 bytes, fixed 12 bytes magic/type/version, dep bytes, rest)."""
    if len(b) < 28:
        raise ValueError("too short")
    header, fixed = b[0:12], b[12:24]
    (ndeps,) = struct.unpack_from("<i", b, 24)
    pos = 28
    for _ in range(ndeps):
        (n,) = struct.unpack_from("<I", b, pos)
        pos += 4
        n &= 0x7fffffff
        pos += n + 8
    return header, fixed, b[24:pos], b[pos:]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    ours_dir, kit_dir = sys.argv[1], sys.argv[2]
    names = sorted(n[:-4] for n in os.listdir(ours_dir) if n.endswith(".bin"))
    bad = 0
    print("%-40s %7s %7s  %s" % ("object", "kit", "exe", "result"))
    for stem in names:
        ours = open(os.path.join(ours_dir, stem + ".bin"), "rb").read()
        kit_path = os.path.join(kit_dir, stem + ".crs")
        if not os.path.exists(kit_path):
            print("%-40s %7s %7d  no kit .crs (not reached by the kit's build)" % (stem, "-", len(ours)))
            continue
        kit = decode_crs(kit_path)
        kh, kf, kdeps, krest = split_header(kit)
        oh, of, odeps, orest = split_header(ours)
        (klen,) = struct.unpack_from("<i", kh, 4)
        (olen,) = struct.unpack_from("<i", oh, 4)
        ndeps = struct.unpack_from("<i", kdeps, 0)[0]
        notes = []
        if kf != of:
            notes.append("type/version differ: kit %s exe %s" % (kf.hex(), of.hex()))
        if klen - (len(kdeps) - 4) != olen:
            notes.append("record length kit %d exe %d (dep bytes %d)" % (klen, olen, len(kdeps) - 4))
        if krest != orest:
            i = next((k for k in range(min(len(krest), len(orest))) if krest[k] != orest[k]), min(len(krest), len(orest)))
            notes.append("body differs at +%d: kit %s exe %s" % (i, krest[i:i + 16].hex(), orest[i:i + 16].hex()))
        if notes:
            bad += 1
            print("%-40s %7d %7d  DIFF %s" % (stem, len(kit), len(ours), "; ".join(notes)))
        else:
            print("%-40s %7d %7d  MATCH (kit has %d dependency entr%s, skipped)" %
                  (stem, len(kit), len(ours), ndeps, "y" if ndeps == 1 else "ies"))
    print("%d object(s), %d differ" % (len(names), bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
