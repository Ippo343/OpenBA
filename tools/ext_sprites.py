#!/usr/bin/env python3
"""tools/ext_sprites.py -- draw the Extensions programmer-art sprites (NOT RETAIL art).

Writes src/ext/ui/Dialog/Sprite/Extensions/{ExtensionsIcon,TierFix,TierA,TierB,TierC}.png, 16 x 16
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


def main():
    os.makedirs(OUT, exist_ok=True)
    icon().save(os.path.join(OUT, "ExtensionsIcon.png"))
    glyph("F", (46, 125, 50, 255)).save(os.path.join(OUT, "TierFix.png"))    # green: a fix
    glyph("A", (30, 100, 170, 255)).save(os.path.join(OUT, "TierA.png"))     # blue: live
    glyph("B", (190, 120, 20, 255)).save(os.path.join(OUT, "TierB.png"))     # amber: locked
    glyph("C", (170, 40, 40, 255)).save(os.path.join(OUT, "TierC.png"))      # red: extends the save
    print("wrote 5 sprites to", OUT)


if __name__ == "__main__":
    main()
