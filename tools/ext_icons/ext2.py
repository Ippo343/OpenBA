"""tools/ext_icons/ext2.py -- white pictograms for six Extensions entries (programmer art).

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


def draw_gather(px):
    """A funnel with dots dropping into it: gather only what is in view."""
    _paint(px, [
        "#..#..#..#..",
        ".#..#..#..#.",
        "............",
        "############",
        ".##########.",
        "..########..",
        "...######...",
        "....####....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        "............",
    ])


def draw_walk_batch(px):
    """A walking stick figure: citizens take a shortcut above 1x speed."""
    _paint(px, [
        ".....##.....",
        ".....##.....",
        "....####....",
        "...######...",
        ".....##.....",
        ".....##.....",
        "....##.##...",
        "...##...##..",
        "..##.....##.",
        ".##.......##",
        "............",
        "............",
    ])


def draw_walk_batch_grid(px):
    """A 2 x 2 grid with a footprint in two cells: the walkers, batched over the grid."""
    _paint(px, [
        "############",
        "###..#.....#",
        "####.#.....#",
        "#.#..#.....#",
        "###..#.....#",
        "############",
        "#....#.....#",
        "#....#.##..#",
        "#....#.###.#",
        "#....#..#..#",
        "#....#.##..#",
        "############",
    ])


def draw_render_pipe(px):
    """A monitor in front of a second window: rendering on a second thread."""
    _paint(px, [
        ".....#######",
        ".....#.....#",
        ".....#.....#",
        ".....#.....#",
        "########...#",
        "#kkkkkk#...#",
        "#kkkkkk#...#",
        "#kkkkkk#...#",
        "#kkkkkk#....",
        "########....",
        "...##.......",
        "..######....",
    ])


def draw_gather_threads(px):
    """Three prongs joined to one stem: one line splitting into three threads."""
    _paint(px, [
        ".##..##..##.",
        ".##..##..##.",
        ".##..##..##.",
        ".##..##..##.",
        ".##########.",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        ".....##.....",
        "............",
    ])


def draw_quick_screens(px):
    """An hourglass with dark sand in the lower bulb: loading that finishes early."""
    _paint(px, [
        "############",
        ".##########.",
        "..########..",
        "...######...",
        "....####....",
        ".....##.....",
        ".....##.....",
        "....####....",
        "...######...",
        "..##kkkk##..",
        ".##kkkkkk##.",
        "############",
    ])


ICONS = {
    "draw_gather": ("A", draw_gather),
    "walk_batch": ("A", draw_walk_batch),
    "walk_batch_grid": ("B", draw_walk_batch_grid),
    "render_pipe": ("A", draw_render_pipe),
    "gather_threads": ("A", draw_gather_threads),
    "quick_screens": ("A", draw_quick_screens),
}
