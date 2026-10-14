"""tools/ext_icons/ext1.py -- white pictograms for six Extensions rows (programmer art, NOT RETAIL).

Each row is 12 characters over the 12 x 12 interior (x, y in [2, 13]): "X" = white,
"D" = dark accent (42, 42, 42), "." = leave the tier-coloured tile alone.
"""

WHITE = (255, 255, 255, 255)
DARK = (42, 42, 42, 255)
KEY = {"X": WHITE, "D": DARK}


def _paint(px, rows):
    assert len(rows) == 12
    for j, row in enumerate(rows):
        assert len(row) == 12, row
        for i, ch in enumerate(row):
            if ch != ".":
                px[2 + i, 2 + j] = KEY[ch]


# An almond eye with a dark pupil.
EYE = [
    "............",
    "............",
    "............",
    "...XXXXXX...",
    "..X......X..",
    ".X..XDDX..X.",
    "X..XXDDXX..X",
    ".X..XDDX..X.",
    "..X......X..",
    "...XXXXXX...",
    "............",
    "............",
]

# A bold, sharp-pointed letter "A".
A_LETTER = [
    "............",
    "....XXXX....",
    "...XX..XX...",
    "...XX..XX...",
    "..XX....XX..",
    "..XX....XX..",
    ".XXXXXXXXXX.",
    ".XXXXXXXXXX.",
    ".XX......XX.",
    "XX........XX",
    "XX........XX",
    "............",
]

# The eye above with a dark diagonal slash through it (culled, not drawn).
EYE_SLASH = [
    "............",
    "..........DD",
    ".........DD.",
    "...XXXXXDD..",
    "..XDD....X..",
    ".XXXDDDX..X.",
    "XXXXXDDXX..X",
    ".DDDDDDX..X.",
    "..XDD....X..",
    "..DDXXXXX...",
    ".DD.........",
    "............",
]

# A winding stair-step path with an up-right arrowhead at its end.
PATH_ARROW = [
    "......XXXXXX",
    "..........XX",
    ".........XX.",
    "......XXX...",
    "......XX....",
    "....XX......",
    "....XX......",
    "..XX........",
    "..XX........",
    "XX..........",
    "XX..........",
    "............",
]

# Three parallel right-pointing arrows, one per worker thread.
THREE_ARROWS = [
    "............",
    ".......XX...",
    "XXXXXXXXXX..",
    ".......XX...",
    "............",
    ".......XX...",
    "XXXXXXXXXX..",
    ".......XX...",
    "............",
    ".......XX...",
    "XXXXXXXXXX..",
    ".......XX...",
]

# Fast-forward: two right-pointing solid triangles (>>).
FAST_FORWARD = [
    "............",
    "............",
    "............",
    ".X....X.....",
    ".XX...XX....",
    ".XXX..XXX...",
    ".XXXX.XXXX..",
    ".XXX..XXX...",
    ".XX...XX....",
    ".X....X.....",
    "............",
    "............",
]


def draw_eye_camera(px):
    """An eye: the camera follows a citizen at eye level."""
    _paint(px, EYE)


def draw_sharp_ui(px):
    """A bold, sharp letter A: UI text and sprites stay crisp at any scale."""
    _paint(px, A_LETTER)


def draw_fast_paths(px):
    """A winding path ending in an arrowhead: faster pathfinding."""
    _paint(px, PATH_ARROW)


def draw_path_workers(px):
    """Three parallel arrows: pathfinding on several threads."""
    _paint(px, THREE_ARROWS)


def draw_render_culling(px):
    """An eye with a slash: work skipped for things off screen."""
    _paint(px, EYE_SLASH)


def draw_anim_batch(px):
    """A fast-forward double triangle: animations step once per frame above 1x."""
    _paint(px, FAST_FORWARD)


ICONS = {
    "eye_camera": ("A", draw_eye_camera),
    "sharp_ui": ("A", draw_sharp_ui),
    "fast_paths": ("B", draw_fast_paths),
    "path_workers": ("B", draw_path_workers),
    "render_culling": ("A", draw_render_culling),
    "anim_batch": ("B", draw_anim_batch),
}
