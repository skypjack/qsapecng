#!/usr/bin/env python3
"""Renders the application icon files from the two SVG sources.

  src/gui/images/qsapecng.svg        the icon (32 pixels and up)
  src/gui/images/qsapecng-small.svg  the same, drawn for 16 and 22 pixels

Writes qsapecng-NxN.png (16 ... 256), qsapecng-ico.ico (Windows) and
qsapecng.icns (macOS) next to them. Needs: pip install cairosvg pillow
"""

import io
import os

import cairosvg
from PIL import Image

HERE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    "..", "src", "gui", "images")


def render(name, size):
    png = cairosvg.svg2png(url=os.path.join(HERE, name),
                           output_width=size, output_height=size)
    return Image.open(io.BytesIO(png)).convert("RGBA")


def icon(size):
    return render("qsapecng-small.svg" if size <= 22 else "qsapecng.svg", size)


for size in (16, 22, 32, 48, 64, 128, 256):
    icon(size).save(os.path.join(HERE, "qsapecng-%dx%d.png" % (size, size)))

# Windows: every size in one file (Pillow scales from the largest image;
# the small ones are given as they are drawn)
sizes = [16, 24, 32, 48, 64, 128, 256]
big = icon(256)
big.save(os.path.join(HERE, "qsapecng-ico.ico"), sizes=[(s, s) for s in sizes],
         append_images=[icon(s) for s in sizes if s != 256])

# macOS
icon(1024).save(os.path.join(HERE, "qsapecng.icns"))
