#!/usr/bin/env python3
"""tools/ext_font_check.py -- prove ext.sharp_ui's font compile used the game's typeface.

usage: python tools/ext_font_check.py <compiled WinData dir> <compiler res dir>

Called by tools/ext_resources.sh after the font targets compile. Exit 0 = every check passed; 1 =
a check failed (the message names the font and what to do).

WHY. The kit's compiler (Tools-x64.exe) rasterises a Font resource from an INSTALLED Windows
typeface named by `_fontName` ("FrancophilSans"). When that face is not installed, Windows
substitutes another face SILENTLY and the compiler reports success: the variants would then be a
different typeface in the game's metrics. The typeface is therefore a machine dependency of the
compile step, like the kit itself.

THE PROOF. The same compile builds the kit's own Font/Font.rsc (a dependency of
Dialog/Extensions.rsc). With FrancophilSans installed, its compiled glyph tables are byte-for-byte
the game's shipped Font\\Font_resource.crs (data0.pkg, decoded 2026-10-12): the bytes from the face
name to the end of the object -- the height, the sheet / material / image names, the codepoint
table, the per-glyph advances and offsets -- hash to SHIPPED_FONT_SHA256 below. A substituted face
changes the advances, so the hash differs. Then every compiled variant must name the face and its
own height, and every variant sheet must hold all the glyphs (a sheet too small compiles EMPTY).

.crs FORMAT (read from the compiler's output; the engine's IO::CompressedStream): u32 blockCount,
then blockCount x {u32 dataOffset, u32 (uncompressedSize << 16 | compressedSize)} (0x8000 = a full
32 KB window), each block LZF-compressed (System::Compression::Decompress).
"""
import glob
import hashlib
import os
import re
import struct
import sys

FACE = b"FrancophilSans\x00"
SHIPPED_FONT_SHA256 = "527f317509172bd0e80b04717d5d9839608ac9876678babc1121efe7760c2326"
SHIPPED_FONT_TAIL = 2453


def lzf(src, n_out):
    out = bytearray()
    i = 0
    while i < len(src):
        c = src[i]
        i += 1
        if c < 32:
            out += src[i:i + c + 1]
            i += c + 1
        else:
            ln = c >> 5
            if ln == 7:
                ln += src[i]
                i += 1
            off = ((c & 0x1f) << 8) + src[i] + 1
            i += 1
            if off > len(out):
                raise ValueError("bad back-reference")
            for _ in range(ln + 2):
                out.append(out[-off])
    if len(out) != n_out:
        raise ValueError("block size %d, expected %d" % (len(out), n_out))
    return bytes(out)


def decode_crs(path):
    d = open(path, "rb").read()
    count = struct.unpack_from("<I", d, 0)[0]
    out = b""
    for b in range(count):
        offset, packed = struct.unpack_from("<II", d, 4 + 8 * b)
        comp, uncomp = packed & 0xffff, packed >> 16
        if uncomp == 0:
            uncomp = 0x10000
        block = d[offset:offset + comp]
        out += block if comp == uncomp else lzf(block, uncomp)
    return out


def fail(msg):
    print("ext_font_check: FAILED -- " + msg)
    sys.exit(1)


def main():
    dat, res = sys.argv[1], sys.argv[2]
    base = os.path.join(dat, "Font", "Font_resource.crs")
    if not os.path.isfile(base):
        fail("%s was not compiled (it is the typeface witness)" % base)
    d = decode_crs(base)
    at = d.find(FACE)
    if at < 0:
        fail("Font/Font_resource.crs does not name FrancophilSans")
    tail = d[at:]
    digest = hashlib.sha256(tail).hexdigest()
    if len(tail) != SHIPPED_FONT_TAIL or digest != SHIPPED_FONT_SHA256:
        fail("the kit's Font/Font.rsc compiled to glyph tables that are NOT the game's "
             "(sha256 %s..., %d bytes): the typeface FrancophilSans is not installed for the user "
             "running the compiler, or another face answers to that name. Install FrancophilSans "
             "(the kit's README: 'The game uses the font FrancophilSans'), delete "
             "build/ext_resources/ and run tools/ext_resources.sh again." % (digest[:12], len(tail)))

    base_sheet = os.path.join(res, "Build", "FontSheet.rsc")
    glyphs = len(re.findall(r"_name\s*=", open(base_sheet, encoding="latin-1").read()))
    variants = sorted(glob.glob(os.path.join(dat, "Font", "SharpFont*_resource.crs")))
    if not variants:
        fail("no Font/SharpFont*_resource.crs under %s" % dat)
    for v in variants:
        name = os.path.basename(v)[:-len("_resource.crs")]
        px = int(name.rsplit("_", 1)[1])
        vd = decode_crs(v)
        at = vd.find(FACE)
        if at < 0:
            fail("%s does not name FrancophilSans" % name)
        height = struct.unpack_from("<i", vd, at + len(FACE))[0]
        if height != px:
            fail("%s has height %d, expected %d" % (name, height, px))
        sheet = os.path.join(res, "Build", name + "Sheet.rsc")
        n = len(re.findall(r"_name\s*=", open(sheet, encoding="latin-1").read()))
        if n != glyphs:
            fail("%s: its sheet holds %d glyphs, the game's font %d (the sheet is too small: "
                 "tools/ext_fonts.py SHEETS)" % (name, n, glyphs))
        for part in ("material", "texture"):
            p = os.path.join(dat, "Font", "%s_%s.crs" % (name, part))
            if not os.path.isfile(p) or os.path.getsize(p) == 0:
                fail("%s is missing or empty" % p)
        p = os.path.join(dat, "Build", name + "Sheet_resource.crs")
        if not os.path.isfile(p) or os.path.getsize(p) == 0:
            fail("%s is missing or empty" % p)
    print("ext_font_check: typeface FrancophilSans = the game's (Font/Font.rsc glyph tables "
          "byte-identical to data0.pkg); %d variants, each %d glyphs" % (len(variants), glyphs))


if __name__ == "__main__":
    main()
