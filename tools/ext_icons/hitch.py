"""tools/ext_icons/hitch.py -- white pictograms for the hitch rows: spread_warmth, async_save_compress,
drain_split, background_save (programmer art, our own).

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


def _floppy(label):
    """A floppy disk: white body, a dark shutter with its white slot at the top, and a dark label
    (rows 6-10) carrying `label`, three 10-wide rows of the label's own '#' / 'k' marks."""
    assert len(label) == 3 and all(len(r) == 10 for r in label), label
    return [
        "##########..",
        "##kkkkkk###.",
        "##kkkk#k####",
        "##kkkk#k####",
        "##kkkkkk####",
        "############",
        "#kkkkkkkkkk#",
        "#" + label[0] + "#",
        "#" + label[1] + "#",
        "#" + label[2] + "#",
        "#kkkkkkkkkk#",
        "############",
    ]


def draw_async_save_compress(px):
    """A floppy disk, two white chevrons on its label pointing at each other: the save, squeezed."""
    _paint(px, _floppy([
        "k#kkkkkk#k",
        "kk##kk##kk",
        "k#kkkkkk#k",
    ]))


def draw_drain_split(px):
    """A house, then a hammer, a dark arrow from one to the other: the job pass a frame after the
    housing pass."""
    _paint(px, [
        "..#.........",
        ".###........",
        "#####..####.",
        ".###...#####",
        ".#k#...####.",
        ".#k#.....#..",
        ".###.....#..",
        ".........#..",
        "............",
        "........k...",
        "..kkkkkkkk..",
        "........k...",
    ])


def draw_background_save(px):
    """A floppy disk with a white pause sign on its label: the town holds still while it saves."""
    _paint(px, _floppy([
        "kkk#kk#kkk",
        "kkk#kk#kkk",
        "kkk#kk#kkk",
    ]))


ICONS = {
    "spread_warmth": ("B", draw_spread_warmth),
    "async_save_compress": ("A", draw_async_save_compress),
    "drain_split": ("B", draw_drain_split),
    "background_save": ("A", draw_background_save),
}
