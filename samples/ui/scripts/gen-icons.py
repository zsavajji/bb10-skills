#!/usr/bin/env python3
"""Draws the action and tab icons (app/assets/images/ic_*.png): white on transparent,
81 x 81 px (9 du on a Q10). Run again after changing a shape.

    python3 scripts/gen-icons.py

A shape is a function (x, y) -> bool over the unit square, y down.
"""
import math, os, struct, sys, zlib

SS = 4   # samples per pixel along each axis


# ---- shapes -------------------------------------------------------------------------------
def rect(x0, y0, x1, y1):
    return lambda x, y: x0 <= x <= x1 and y0 <= y <= y1


def circle(cx, cy, r):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def ring(cx, cy, r, w):
    return lambda x, y: (r - w / 2) ** 2 <= (x - cx) ** 2 + (y - cy) ** 2 <= (r + w / 2) ** 2


def ellipse(cx, cy, rx, ry):
    return lambda x, y: ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1


def seg(x0, y0, x1, y1, w):   # a line with round ends
    dx, dy = x1 - x0, y1 - y0
    n = dx * dx + dy * dy

    def f(x, y):
        t = max(0.0, min(1.0, ((x - x0) * dx + (y - y0) * dy) / n))
        return (x - x0 - t * dx) ** 2 + (y - y0 - t * dy) ** 2 <= (w / 2) ** 2
    return f


def tri(ax, ay, bx, by, cx, cy):
    def side(px, py, qx, qy, x, y):
        return (qx - px) * (y - py) - (qy - py) * (x - px)

    def f(x, y):
        s = (side(ax, ay, bx, by, x, y), side(bx, by, cx, cy, x, y), side(cx, cy, ax, ay, x, y))
        return all(v >= 0 for v in s) or all(v <= 0 for v in s)
    return f


def any_of(*shapes):
    return lambda x, y: any(s(x, y) for s in shapes)


def minus(a, b):
    return lambda x, y: a(x, y) and not b(x, y)


def both(a, b):
    return lambda x, y: a(x, y) and b(x, y)


def rrect(x0, y0, x1, y1, r):   # a rectangle with round corners
    def f(x, y):
        dx, dy = max(x0 + r - x, 0, x - x1 + r), max(y0 + r - y, 0, y - y1 + r)
        return x0 <= x <= x1 and y0 <= y <= y1 and dx * dx + dy * dy <= r * r
    return f


def moved(shape, ox, oy, s):   # shape drawn at scale s with its origin at (ox, oy)
    return lambda x, y: shape((x - ox) / s, (y - oy) / s)


def path(points, w):
    return any_of(*[seg(*points[i], *points[i + 1], w) for i in range(len(points) - 1)])


def head(x, y, d, s=0.11):   # an arrowhead with its tip s past (x, y), pointing d: r l u d
    if d == "r":
        return tri(x, y - s, x, y + s, x + s * 1.3, y)
    if d == "l":
        return tri(x, y - s, x, y + s, x - s * 1.3, y)
    if d == "u":
        return tri(x - s, y, x + s, y, x, y - s * 1.3)
    return tri(x - s, y, x + s, y, x, y + s * 1.3)


def raster(size, inside):   # -> rows of coverage, 0..255
    rows = []
    for py in range(size):
        row = []
        for px in range(size):
            hit = sum(inside((px + (i + 0.5) / SS) / size, (py + (j + 0.5) / SS) / size)
                      for i in range(SS) for j in range(SS))
            row.append(hit * 255 // (SS * SS))
        rows.append(row)
    return rows


def write_png(path, size, pixels):   # pixels: rows of (r, g, b, a)
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    raw = b"".join(b"\x00" + b"".join(bytes(p) for p in row) for row in pixels)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, len(pixels), 8, 6, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))



# ---- action and tab icons: white on transparent, 81 x 81 px -------------------------------
W = 0.075   # stroke width
# ic_up's edge moved W/2 inwards (apex, right, stem, back round to the apex).
UP_EDGE = [(0.50, 0.2186), (0.7199, 0.4825), (0.5625, 0.4825), (0.5625, 0.8025), (0.4375, 0.8025),
           (0.4375, 0.4825), (0.2801, 0.4825), (0.50, 0.2186)]
ICONS = {
    "ic_home": any_of(tri(0.50, 0.16, 0.14, 0.50, 0.86, 0.50),
                      minus(rect(0.24, 0.48, 0.76, 0.82), rect(0.43, 0.60, 0.57, 0.82))),
    "ic_subs": any_of(*[seg(0.34, y, 0.82, y, W) for y in (0.30, 0.50, 0.70)],
                      *[circle(0.20, y, 0.05) for y in (0.30, 0.50, 0.70)]),
    "ic_search": any_of(ring(0.43, 0.43, 0.22, W), seg(0.60, 0.60, 0.82, 0.82, 0.09)),
    "ic_user": any_of(circle(0.50, 0.34, 0.15),
                      minus(ellipse(0.50, 0.86, 0.30, 0.30), rect(0.0, 0.84, 1.0, 1.0))),
    "ic_refresh": any_of(minus(ring(0.50, 0.52, 0.28, W), rect(0.50, 0.0, 1.0, 0.52)),
                         tri(0.50, 0.14, 0.50, 0.36, 0.68, 0.25)),
    "ic_sort": any_of(seg(0.18, 0.30, 0.82, 0.30, W), seg(0.18, 0.50, 0.62, 0.50, W),
                      seg(0.18, 0.70, 0.42, 0.70, W)),
    "ic_share": any_of(circle(0.28, 0.50, 0.10), circle(0.70, 0.26, 0.10), circle(0.70, 0.74, 0.10),
                       seg(0.28, 0.50, 0.70, 0.26, 0.05), seg(0.28, 0.50, 0.70, 0.74, 0.05)),
    "ic_save": any_of(seg(0.50, 0.16, 0.50, 0.58, W), head(0.50, 0.58, "d", 0.13),
                      path([(0.20, 0.62), (0.20, 0.82), (0.80, 0.82), (0.80, 0.62)], W)),
    "ic_open": any_of(path([(0.42, 0.22), (0.20, 0.22), (0.20, 0.80), (0.78, 0.80), (0.78, 0.58)], W),
                      seg(0.46, 0.54, 0.78, 0.22, W), tri(0.56, 0.14, 0.86, 0.14, 0.86, 0.44)),
    "ic_copy": any_of(minus(rrect(0.18, 0.32, 0.64, 0.88, 0.05), rrect(0.26, 0.40, 0.56, 0.80, 0.02)),
                      path([(0.36, 0.24), (0.36, 0.14), (0.80, 0.14), (0.80, 0.68), (0.72, 0.68)], 0.06)),
    "ic_play": tri(0.30, 0.24, 0.30, 0.76, 0.76, 0.50),
    "ic_pause": any_of(rect(0.28, 0.24, 0.43, 0.76), rect(0.57, 0.24, 0.72, 0.76)),
    "ic_previous": any_of(rect(0.16, 0.28, 0.23, 0.72), tri(0.52, 0.28, 0.52, 0.72, 0.24, 0.50),
                          tri(0.82, 0.28, 0.82, 0.72, 0.52, 0.50)),
    "ic_next": any_of(rect(0.77, 0.28, 0.84, 0.72), tri(0.48, 0.28, 0.48, 0.72, 0.76, 0.50),
                      tri(0.18, 0.28, 0.18, 0.72, 0.48, 0.50)),
    "ic_info": any_of(ring(0.50, 0.50, 0.33, W), circle(0.50, 0.34, 0.05), seg(0.50, 0.46, 0.50, 0.66, W)),
    # a subreddit's Subscribe and Unsubscribe: a plus and a minus in ic_info's ring
    "ic_sub_add": any_of(ring(0.50, 0.50, 0.33, W), seg(0.50, 0.36, 0.50, 0.64, W), seg(0.36, 0.50, 0.64, 0.50, W)),
    "ic_sub_remove": any_of(ring(0.50, 0.50, 0.33, W), seg(0.36, 0.50, 0.64, 0.50, W)),
    # the account's actions
    "ic_up": any_of(tri(0.20, 0.52, 0.80, 0.52, 0.50, 0.16), rect(0.40, 0.48, 0.60, 0.84)),
    "ic_down": any_of(tri(0.20, 0.48, 0.80, 0.48, 0.50, 0.84), rect(0.40, 0.16, 0.60, 0.52)),
    # the same arrows not given (the user, 2026-11-21: empty until voted): ic_up's edge drawn half
    # a stroke inside it, so that the outline is as big as the filled arrow.
    "ic_up_outline": path(UP_EDGE, W),
    "ic_down_outline": path([(x, 1 - y) for x, y in UP_EDGE], W),
    "ic_bookmark": minus(rect(0.28, 0.14, 0.72, 0.86), tri(0.26, 0.90, 0.74, 0.90, 0.50, 0.62)),
    "ic_reply": any_of(tri(0.14, 0.40, 0.46, 0.16, 0.46, 0.64),
                       path([(0.40, 0.40), (0.64, 0.40), (0.80, 0.54), (0.80, 0.80)], W)),
    "ic_edit": any_of(seg(0.34, 0.66, 0.70, 0.30, 0.13), tri(0.16, 0.84, 0.22, 0.62, 0.38, 0.78),
                      seg(0.50, 0.84, 0.84, 0.84, 0.05)),
    "ic_delete": any_of(rect(0.22, 0.24, 0.78, 0.31), rect(0.40, 0.15, 0.60, 0.24),
                        minus(rect(0.30, 0.33, 0.70, 0.84), rect(0.38, 0.33, 0.62, 0.76))),
    "ic_inbox": any_of(path([(0.18, 0.52), (0.30, 0.24), (0.70, 0.24), (0.82, 0.52), (0.82, 0.78), (0.18, 0.78),
                             (0.18, 0.52)], W),
                       path([(0.18, 0.52), (0.38, 0.52), (0.42, 0.62), (0.58, 0.62), (0.62, 0.52), (0.82, 0.52)], W)),
    "ic_newpost": any_of(seg(0.50, 0.20, 0.50, 0.80, 0.09), seg(0.20, 0.50, 0.80, 0.50, 0.09)),
    "ic_send": minus(tri(0.14, 0.20, 0.86, 0.50, 0.14, 0.80), tri(0.10, 0.40, 0.10, 0.60, 0.42, 0.50)),
    "ic_hide": any_of(minus(ellipse(0.50, 0.50, 0.36, 0.22), ellipse(0.50, 0.50, 0.27, 0.14)), circle(0.50, 0.50, 0.08),
                      seg(0.22, 0.80, 0.78, 0.20, 0.07)),
}


def action_icon(path, inside, size=81):
    write_png(path, size, [[(255, 255, 255, a) for a in row] for row in raster(size, inside)])


ORANGE = (0xFF, 0x6A, 0x1A)
DARK = (0x12, 0x12, 0x12)

# A flame: a round belly with a tip, and a smaller flame cut out of it (the glyph is an outline).
FLAME_OUTER = any_of(circle(0.50, 0.62, 0.24), tri(0.285, 0.55, 0.715, 0.55, 0.58, 0.12))
INNER = moved(FLAME_OUTER, 0.275, 0.421, 0.45)
FG = minus(FLAME_OUTER, INNER)
TILE = rrect(0.03, 0.03, 0.97, 0.97, 0.20)
GLYPH = (0.26, 0.12, 0.74, 0.86)   # FG's box, in tile units


def paint(path, w, h, bg, ox, oy, s, tile):
    """bg: the colour behind; the glyph at scale s (pixels per unit) with its origin at (ox, oy)."""
    pixels = []
    for py in range(h):
        row = []
        for px in range(w):
            n = g = 0
            for i in range(SS):
                for j in range(SS):
                    x, y = px + (i + 0.5) / SS, py + (j + 0.5) / SS
                    if tile and not TILE(x / w, y / h):
                        continue
                    n += 1
                    g += FG((x - ox) / s, (y - oy) / s)
            if not n:
                row.append((0, 0, 0, 0))
                continue
            c = tuple((255 * g + bg[k] * (n - g)) // n for k in range(3))
            row.append(c + (n * 255 // (SS * SS),))
        pixels.append(row)
    write_png(path, w, pixels)


def app_icon(path, size):
    paint(path, size, size, ORANGE, 0, 0, size, True)


def splash(path, w, h):   # the glyph in the middle, a third of the short side high
    s = min(w, h) / 3 / (GLYPH[3] - GLYPH[1])
    paint(path, w, h, DARK, w / 2 - (GLYPH[0] + GLYPH[2]) / 2 * s, h / 2 - (GLYPH[1] + GLYPH[3]) / 2 * s, s, False)


here = os.path.dirname(os.path.abspath(__file__))
# The launcher icon and the splash screens are scripts/gen-app-icon.py's now (the flame of
# app/art/fire.svg): this script draws the action icons only.

images = os.path.join(here, "..", "app", "assets", "images")
os.makedirs(images, exist_ok=True)
for name, shape in ICONS.items():
    action_icon(os.path.join(images, name + ".png"), shape)
