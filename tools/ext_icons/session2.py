"""tools/ext_icons/session2.py -- white pictograms for seamless_loops and billboard_shadow (programmer art, our own).

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


def draw_seamless_loops(px):
    """A loop arrow closing on itself around a small waveform, a dark speaker on the left: an ambient sound loop."""
    _paint(px, [
        "............",
        "...#######..",
        "..#.......#.",
        ".#.........#",
        ".#.k.......#",
        ".#kk..#.#..#",
        ".#kk.######.",
        ".#.k..#.#..#",
        ".#.........#",
        "..#.......##",
        "...####.###.",
        "........##..",
])


def draw_billboard_shadow(px):
    """A round deciduous crown on a trunk, a dark dappled shadow of scattered blobs under it."""
    _paint(px, [
        "...######...",
        "..########..",
        ".##########.",
        ".##########.",
        ".##########.",
        "..########..",
        "....####....",
        ".....##.....",
        ".....##.....",
        "............",
        ".kk.k.kk.kk.",
        "k.kk.kk.k..k",
    ])


ICONS = {
    "seamless_loops": ("A", draw_seamless_loops),
    # billboard_shadow: no row since better_trees folded it in (2026-10-17); its drawing is kept above
}
