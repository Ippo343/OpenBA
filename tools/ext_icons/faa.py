"""tools/ext_icons/faa.py -- white pictograms for the foliage AA entries (programmer art, our own).

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


def draw_foliage_aa(px):
    """A leaf on a stem, a dark vein: smooth foliage edges."""
    _paint(px, [
        ".....##.....",
        "....####....",
        "...##k###...",
        "..###k####..",
        "..###k####..",
        "..####k###..",
        "...###k##...",
        "...###k##...",
        "....##k#....",
        ".....#k.....",
        ".....#......",
        ".....#......",
    ])


def draw_max_shadow_res(px):
    """A tree and its shadow lying on the ground: the sun's shadow map."""
    _paint(px, [
        "............",
        "....##......",
        "...####.....",
        "..######....",
        "....##..kk..",
        "....##.kkkk.",
        "....##kkkkk.",
        "....##.kkkk.",
        "....##..kk..",
        "............",
        "############",
        "############",
    ])


ICONS = {
    "foliage_aa": ("A", draw_foliage_aa),
    "max_shadow_res": ("A", draw_max_shadow_res),
}
