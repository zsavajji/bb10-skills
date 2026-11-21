#!/usr/bin/env python3
"""Draws the launcher icon and the splash screens (app/icon.png, app/splash-*.png) from the app's
glyph, app/art/fire.svg: "Fire", created by Michael Appleford from Noun Project (the attribution is
shown in About). The icon is the flame in white on an orange (#FF6A1A) rounded square;
a splash is the flame in orange on the dark (#121212) the app opens on, a third of the short side
high. Needs Pillow and ImageMagick's `magick` (which renders the SVG). Run by hand when the look
changes:
    python3 scripts/gen-app-icon.py
"""
import os
import subprocess
import tempfile

from PIL import Image, ImageDraw

ORANGE = (0xFF, 0x6A, 0x1A)
DARK = (0x12, 0x12, 0x12)
here = os.path.dirname(os.path.abspath(__file__))
app = os.path.join(here, "..", "app")


def flame(height, colour):
    """The glyph `height` px high, in `colour`, cropped to its own box."""
    with tempfile.TemporaryDirectory() as tmp:
        png = os.path.join(tmp, "fire.png")
        subprocess.run(["magick", "-background", "none", "-density", "1200", os.path.join(app, "art", "fire.svg"),
                        "-resize", "2048x2048", png], check=True)
        alpha = Image.open(png).getchannel("A")
    alpha = alpha.crop(alpha.getbbox())
    alpha = alpha.resize((max(1, round(alpha.width * height / alpha.height)), height), Image.LANCZOS)
    out = Image.new("RGBA", alpha.size, colour + (0,))
    out.putalpha(alpha)
    return out


def icon(path, size):
    s = 4   # supersampling for the tile's round corners
    tile = Image.new("L", (size * s, size * s), 0)
    ImageDraw.Draw(tile).rounded_rectangle((0, 0, size * s - 1, size * s - 1), radius=size * s * 0.18, fill=255)
    out = Image.new("RGBA", (size, size), ORANGE + (0,))
    out.putalpha(tile.resize((size, size), Image.LANCZOS))
    glyph = flame(round(size * 0.72), (255, 255, 255))
    out.alpha_composite(glyph, ((size - glyph.width) // 2, (size - glyph.height) // 2))
    out.save(path)


def splash(path, w, h):
    out = Image.new("RGBA", (w, h), DARK + (255,))
    glyph = flame(min(w, h) // 3, ORANGE)
    out.alpha_composite(glyph, ((w - glyph.width) // 2, (h - glyph.height) // 2))
    out.convert("RGB").save(path)


icon(os.path.join(app, "icon.png"), 90)   # the Q10's size
icon(os.path.join(app, "assets", "images", "icon-144.png"), 144)   # the About page's, 16 du on the Q10
for w, h in ((720, 720), (720, 1280), (768, 1280), (1440, 1440)):   # Q10 Q5 Q20, Z30, Z10, Passport
    splash(os.path.join(app, "splash-%dx%d.png" % (w, h)), w, h)
