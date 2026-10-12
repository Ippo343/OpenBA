#!/usr/bin/env python3
"""tools/ext_fonts.py -- write ext.sharp_ui's font variant sources (NOT RETAIL).

usage: python tools/ext_fonts.py            (rewrites src/ext/ui/Font/SharpFont*.rsc)

The game's four fonts (the mod kit's resource/Font/*.rsc: Font 24 px, FontSmall 14, FontMedium 18,
FontLarge 48, face FrancophilSans) are re-compiled by the kit's Tools-x64.exe at every pixel height
the Options UI-scale slider can ask for, so ext.sharp_ui can draw text one texel per device pixel
(src/ext/sharp_ui/sharp_math.h has the rule; this script MIRRORS VariantLow / VariantHigh /
VariantStep, and tests/unit/ext_sharp_ui_test.cpp pins the counts):
  heights = every step-th pixel height from round(H * 2/3) (slider 150%) to round(H * 4/3)
            (slider 75%), H itself excluded; step 1, or 2 for H >= 40.
One source per variant, `Font/Sharp<KitName>_<px>.rsc` (Font -> SharpFont_32, FontSmall ->
SharpFontSmall_19: sharp_math.cpp VariantFontPath): the Font (unnamed object), its material and
texture as named objects in the same file. The compiler rasterises the glyphs into
`Build/<file>c<NN>.png`, packs them into `_imageName` and writes the sheet source `_sheetName`, all
under its /pathres (build/ext_resources/res/), and compiles four objects:
  Font/<file>_resource.crs, Font/<file>_material.crs, Font/<file>_texture.crs (the packed image)
  Build/<file>Sheet_resource.crs
The sheet is the smallest of 128x128, 256x128, 256x256, 512x256, 512x512 with at least 64 * px^2
texels (the kit's own sheets hold its glyphs in 28-57 * px^2; a sheet too small for the glyphs
compiles EMPTY without an error, which tools/ext_resources.sh checks for).
"""
import os
import sys

FONTS = [("Font", 24), ("FontSmall", 14), ("FontMedium", 18), ("FontLarge", 48)]
FACE = "FrancophilSans"
COARSE_FROM = 40
SHEETS = [(128, 128), (256, 128), (256, 256), (512, 256), (512, 512)]


def round_thirds(h, num):
    return (2 * num * h + 3) // 6


def heights(h):
    lo, hi = round_thirds(h, 2), round_thirds(h, 4)
    step = 2 if h >= COARSE_FROM else 1
    return [x for x in range(lo, hi + 1) if (x - lo) % step == 0 and x != h]


def sheet(px):
    for w, h in SHEETS:
        if w * h >= 64 * px * px:
            return w, h
    sys.exit("ext_fonts: no sheet holds %d px" % px)


SOURCE = """// Font/{file}.rsc -- ext.sharp_ui: the game's {kit} ({base} px) at {px} px (NOT RETAIL).
// Written by tools/ext_fonts.py; do not edit by hand. The kit's resource/Font/{kit}.rsc with
// _fontHeight {px}; material and texture as named objects (the kit's Font/{kit}Material.rsc and
// Font/{kit}Texture.rsc shapes).

Font resource
{{
	CharacterSet _characterSet = "Font/CharacterSet.rsc";
	String _fontName = "{face}";
	int _fontHeight = {px};
	String _sheetName = "Build/{file}Sheet.rsc";
	int _sheetWidth = {sw};
	int _sheetHeight = {sh};
	String _materialName = "Font/{file}.rsc:material";
	String _imageName = "Build/ui{file}Image.png";
}}

MaterialInstance material
{{
	Material _material = "Material\\UI\\uiMaterial.rsc";
	Texture _textures
	[
		{{
			String _name = "diffuse";
			ImageBuffer _texture = "Font/{file}.rsc:texture";
		}}
	]
	Constant _constants
	[
	]
}}

ImageBuffer texture : "Texture/UITexture.rsc"
{{
	String _imageName = "Build/ui{file}Image.png";
}}
"""


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(root, "src", "ext", "ui", "Font")
    os.makedirs(out, exist_ok=True)
    for f in os.listdir(out):
        if f.startswith("SharpFont") and f.endswith(".rsc"):
            os.remove(os.path.join(out, f))
    n = 0
    for kit, base in FONTS:
        for px in heights(base):
            sw, sh = sheet(px)
            file = "Sharp%s_%d" % (kit, px)
            text = SOURCE.format(file=file, kit=kit, base=base, px=px, face=FACE, sw=sw, sh=sh)
            with open(os.path.join(out, file + ".rsc"), "w", newline="\n") as fh:
                fh.write(text)
            n += 1
    print("ext_fonts: %d variant sources in %s" % (n, out))


if __name__ == "__main__":
    main()
