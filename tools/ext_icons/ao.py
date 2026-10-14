"""tools/ext_icons/ao.py -- the white pictogram for the ambient-occlusion entry (programmer art).

Each draw(px) paints the 12 x 12 interior (x, y in [2, 13]) of a 16 x 16 tile. Rows are 12 chars:
'#' = white, 'k' = the dark accent, '.' = leave the tier colour showing.
"""

WHITE = (255, 255, 255, 255)
DARK = (42, 42, 42, 255)


def _paint(px, rows):
    assert len(rows) == 12 and all(len(r) == 12 for r in rows), rows
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == "#":
                px[2 + i, 2 + j] = WHITE
            elif ch == "k":
                px[2 + i, 2 + j] = DARK


def draw_ambient_occlusion(px):
    """A box standing in a corner, the creases darkened: ambient occlusion."""
    _paint(px, [
        "k...........",
        "k...........",
        "k...........",
        "k...####....",
        "k...####....",
        "k..k####....",
        "k..k####....",
        "k..k####....",
        "k.kk####k...",
        "kkkkkkkkkk..",
        "############",
        "############",
    ])


ICONS = {
    "ambient_occlusion": ("A", draw_ambient_occlusion),
}
