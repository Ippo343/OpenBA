"""tools/ext_icons/hitch.py -- white pictogram for spread_warmth (programmer art, our own).

Same conventions as faa.py: each draw(px) paints the 12 x 12 interior (x, y in [2, 13]) of a 16 x 16 tile.
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


def draw_spread_warmth(px):
    """A house with a warm door, three dark dots trailing off behind it: warmth searched over time."""
    _paint(px, [
        "......#.....",
        ".....###....",
        "....#####...",
        "...#######..",
        "..#########.",
        "...#######..",
        "...###k###..",
        "...##kkk##..",
        "...##kkk##..",
        "............",
        "............",
        ".k...k...k..",
    ])


ICONS = {
    "spread_warmth": ("B", draw_spread_warmth),
}
