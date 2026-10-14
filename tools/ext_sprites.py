#!/usr/bin/env python3
"""tools/ext_sprites.py -- draw the Extensions programmer-art sprites (NOT RETAIL art).

Writes src/ext/ui/Dialog/Sprite/Extensions/{ExtensionsIcon,OptionsEnhanced,TierFix,TierA,TierB,TierC}.png, 16 x 16
RGBA like the Mods button's "Mod" sprite (the kit's Dialog/Sprite/ButtonIcons/Scenario.png). The
PNGs are committed; rerun this only to regenerate the placeholder art. The user replaces the PNGs
with real art later (tools/ext_resources.sh recompiles them into the sprite sheet).

usage: python tools/ext_sprites.py        (from the repo root; needs Pillow)
"""
import os
from PIL import Image

OUT = os.path.join("src", "ext", "ui", "Dialog", "Sprite", "Extensions")
W = H = 16
CLEAR = (0, 0, 0, 0)
RIM = (42, 42, 42, 255)
FILL = (232, 232, 232, 255)


def icon():
    """A cog with a plus in its hub: light fill on a dark rim (the code-drawn icon it replaces)."""
    img = Image.new("RGBA", (W, H), CLEAR)
    px = img.load()
    cx, cy, s = (W - 1) * 0.5, (H - 1) * 0.5, W * 0.5
    for y in range(H):
        for x in range(W):
            dx, dy = (x - cx) / s, (y - cy) / s
            r2 = dx * dx + dy * dy
            ax, ay = abs(dx), abs(dy)
            diagonal = ax > 0.38 * (ax + ay) and ay > 0.38 * (ax + ay)
            axial = ax < 0.18 or ay < 0.18
            outer = 0.96 if (axial or diagonal) else 0.78
            c = CLEAR
            if r2 <= outer * outer:
                c = RIM
            if r2 <= (outer - 0.14) ** 2:
                c = FILL
            if r2 <= 0.42 ** 2:
                c = RIM
            if r2 <= 0.32 ** 2:
                c = FILL if ((ax < 0.08 and ay < 0.24) or (ay < 0.08 and ax < 0.24)) else RIM
            px[x, y] = c
    return img


def options_enhanced():
    """The Options dialog's "Enhanced" tab icon (block AOMENU): a ball resting in a soft contact
    shadow, white with alpha like retail's 16 x 16 OptionsGraphics / OptionsSound tab icons."""
    img = Image.new("RGBA", (W, H), CLEAR)
    px = img.load()
    for y in range(H):
        for x in range(W):
            a = 0.0
            # the contact shadow: a flat ellipse under the ball, darkest at its centre
            ex, ey = (x - 7.5) / 7.5, (y - 13.0) / 2.2
            e2 = ex * ex + ey * ey
            if e2 <= 1.0:
                a = 0.55 * (1.0 - e2)
            # the ball, lit from the top left: full alpha, dimmer toward the occluded bottom
            bx, by = (x - 7.5) / 5.5, (y - 6.5) / 5.5
            b2 = bx * bx + by * by
            if b2 <= 1.0:
                a = max(a, 1.0 - 0.45 * max(0.0, (bx + by) * 0.5 + 0.2))
            if a > 0.0:
                px[x, y] = (255, 255, 255, int(round(255 * min(1.0, a))))
    return img


# 5 x 7 letters, scaled x2 into a 10 x 14 box.
LETTERS = {
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
}


def glyph(letter, colour):
    """A rounded tile in the tier's colour with the letter in white."""
    img = Image.new("RGBA", (W, H), CLEAR)
    px = img.load()
    for y in range(H):
        for x in range(W):
            corner = (x in (0, W - 1)) and (y in (0, H - 1))
            if corner:
                continue
            edge = x in (0, W - 1) or y in (0, H - 1)
            px[x, y] = RIM if edge else colour
    rows = LETTERS[letter]
    ox, oy = 3, 1
    for j, row in enumerate(rows):
        for i, bit in enumerate(row):
            if bit == "1":
                for dy in range(2):
                    for dx in range(2):
                        px[ox + 2 * i + dx, oy + 2 * j + dy] = (255, 255, 255, 255)
    return img


# Per-entry row icons: the tier's tile with a white pictogram in place of the letter. Each
# tools/ext_icons/<group>.py holds ICONS = {registry id: draw(px)}; draw paints white (or
# RIM-dark) pixels into the 12 x 12 interior x, y in [2, 13] of a 16 x 16 tile.
ICON_SCALE = 3   # saved 48 x 48 (Mod.rsc:imageIcon's size), pixel-tripled so the sheet does not blur it
TIER_COLOUR = {
    "fix": (46, 125, 50, 255),     # green: a fix
    "A": (30, 100, 170, 255),      # blue: live
    "B": (190, 120, 20, 255),      # amber: locked
    "C": (170, 40, 40, 255),       # red: extends the save
}


def tile(colour):
    """The tier's rounded tile: dark rim, colour fill, no letter."""
    img = Image.new("RGBA", (W, H), CLEAR)
    px = img.load()
    for y in range(H):
        for x in range(W):
            if (x in (0, W - 1)) and (y in (0, H - 1)):
                continue
            px[x, y] = RIM if (x in (0, W - 1) or y in (0, H - 1)) else colour
    return img


def entry_icons():
    """{registry id: (tier key, draw)} from every tools/ext_icons/*.py module."""
    import importlib.util
    here = os.path.join(os.path.dirname(os.path.abspath(__file__)), "ext_icons")
    found = {}
    for name in sorted(os.listdir(here)):
        if not name.endswith(".py") or name.startswith("_"):
            continue
        spec = importlib.util.spec_from_file_location(name[:-3], os.path.join(here, name))
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        for entry_id, (tier, draw) in mod.ICONS.items():
            assert entry_id not in found, entry_id
            found[entry_id] = (tier, draw)
    return found


def main():
    os.makedirs(OUT, exist_ok=True)
    icon().save(os.path.join(OUT, "ExtensionsIcon.png"))
    options_enhanced().save(os.path.join(OUT, "OptionsEnhanced.png"))
    glyph("F", TIER_COLOUR["fix"]).save(os.path.join(OUT, "TierFix.png"))
    glyph("A", TIER_COLOUR["A"]).save(os.path.join(OUT, "TierA.png"))
    glyph("B", TIER_COLOUR["B"]).save(os.path.join(OUT, "TierB.png"))
    glyph("C", TIER_COLOUR["C"]).save(os.path.join(OUT, "TierC.png"))
    icons = entry_icons()
    for entry_id, (tier, draw) in sorted(icons.items()):
        img = tile(TIER_COLOUR[tier])
        draw(img.load())
        img = img.resize((W * ICON_SCALE, H * ICON_SCALE), Image.NEAREST)
        img.save(os.path.join(OUT, "Entry_%s.png" % entry_id))
    print("wrote %d sprites to %s" % (6 + len(icons), OUT))


if __name__ == "__main__":
    main()
