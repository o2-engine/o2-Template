#!/usr/bin/env python3
"""Procedural placeholder textures and sprites, used until generated art replaces them.
Footprint metrics (pivots, canvas layout) are the contract with C++ and stay the same for
the real art."""
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

from artgen import SRC


def noise_texture(base, variation=6, blobs=8, blob_alpha=10, size=SRC, seed=1):
    rng = np.random.default_rng(seed)
    arr = np.zeros((size, size, 3), np.float32) + np.array(base, np.float32)
    arr += rng.normal(0, variation, (size, size, 1))
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(blobs):
        x, y, r = rng.integers(0, size), rng.integers(0, size), int(rng.integers(30, 90))
        tint = int(rng.integers(0, 2)) * 30 - 15
        d.ellipse([x - r, y - r, x + r, y + r],
                  fill=(base[0] + tint, base[1] + tint, base[2] + tint, blob_alpha))
    return img.filter(ImageFilter.GaussianBlur(2))


def asphalt_texture():
    return noise_texture((141, 144, 153), variation=4, seed=2)


def pavement_texture():
    img = noise_texture((233, 224, 205), variation=3, blobs=4, seed=3)
    d = ImageDraw.Draw(img)
    step = SRC // 4
    for k in range(0, SRC + 1, step):
        d.line([(k, 0), (k, SRC)], fill=(214, 204, 184), width=3)
        d.line([(0, k), (SRC, k)], fill=(214, 204, 184), width=3)
    return img


def grass_texture():
    return noise_texture((121, 184, 99), variation=7, blobs=10, blob_alpha=14, seed=4)


# ---------------------------------------------------------------- iso sprites

def shade(color, f):
    return tuple(min(255, max(0, int(c * f))) for c in color)


def building_sprite(fw, fh, wall_h, wall_color, roof_color, floors=2, awning=None,
                    chip_logo=False):
    """Extruded iso box building on fw x fh cells. Returns (image, pivot) where pivot is the
    canvas point matching the footprint center cell position on screen."""
    cw, ch = (fw + fh) * 128, wall_h + (fw + fh) * 64
    img = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    nx = fh * 128  # canvas x of the N (top) footprint corner
    gN = (nx, wall_h)
    gE = (nx + fw * 128, wall_h + fw * 64)
    gS = (nx + (fw - fh) * 128, wall_h + (fw + fh) * 64)
    gW = (nx - fh * 128, wall_h + fh * 64)
    up = (0, -wall_h)

    def add(p, o):
        return (p[0] + o[0], p[1] + o[1])

    right = [gE, gS, add(gS, up), add(gE, up)]
    left = [gW, gS, add(gS, up), add(gW, up)]
    roof = [add(p, up) for p in (gN, gE, gS, gW)]
    d.polygon(left, fill=shade(wall_color, 0.82))
    d.polygon(right, fill=shade(wall_color, 1.0))
    d.polygon(roof, fill=roof_color)
    d.line(roof + [roof[0]], fill=shade(roof_color, 0.85), width=3)

    # windows: rows of sheared quads on both visible walls
    win = (225, 236, 246, 255)
    frame = shade(wall_color, 0.7)
    for wall, a, b in ((right, gS, gE), (left, gS, gW)):
        along = ((b[0] - a[0]), (b[1] - a[1]))
        length = fw if wall is right else fh
        cols = max(1, length * 2)
        for fl in range(floors):
            for cidx in range(cols):
                t0 = (cidx + 0.28) / cols
                t1 = (cidx + 0.72) / cols
                y0 = -wall_h * (fl + 0.35) / floors
                y1 = -wall_h * (fl + 0.78) / floors
                quad = [add(add(a, (along[0] * t0, along[1] * t0)), (0, y0)),
                        add(add(a, (along[0] * t1, along[1] * t1)), (0, y0)),
                        add(add(a, (along[0] * t1, along[1] * t1)), (0, y1)),
                        add(add(a, (along[0] * t0, along[1] * t0)), (0, y1))]
                d.polygon(quad, fill=win, outline=frame)

    if awning:  # striped awning strip along the right wall bottom
        stripes = 8
        a, b = gS, gE
        along = ((b[0] - a[0]), (b[1] - a[1]))
        for sidx in range(stripes):
            t0, t1 = sidx / stripes, (sidx + 1) / stripes
            col = awning if sidx % 2 == 0 else (245, 243, 238, 255)
            quad = [add(add(a, (along[0] * t0, along[1] * t0)), (0, -wall_h * 0.30)),
                    add(add(a, (along[0] * t1, along[1] * t1)), (0, -wall_h * 0.30)),
                    add(add(a, (along[0] * t1, along[1] * t1)), (0, -wall_h * 0.44)),
                    add(add(a, (along[0] * t0, along[1] * t0)), (0, -wall_h * 0.44))]
            d.polygon(quad, fill=col)

    if chip_logo:  # blue AI-chip emblem on the right wall
        m = ((gE[0] + gS[0]) // 2, (gE[1] + gS[1]) // 2 - int(wall_h * 0.62))
        r = 26
        d.rounded_rectangle([m[0] - r, m[1] - r, m[0] + r, m[1] + r], radius=8,
                            fill=(58, 140, 235, 255), outline=(255, 255, 255, 255), width=3)
        d.rectangle([m[0] - 10, m[1] - 10, m[0] + 10, m[1] + 10], fill=(140, 196, 250, 255))

    pivot = (nx + (fw - fh) * 64, wall_h + (fw + fh) * 32)
    return img, pivot


def tree_sprite(scale=1.0):
    w, h = int(140 * scale), int(190 * scale)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([w * 0.40, h * 0.78, w * 0.60, h * 0.98], fill=(0, 0, 0, 40))
    d.rectangle([w * 0.46, h * 0.55, w * 0.54, h * 0.88], fill=(122, 85, 60, 255))
    for cx, cy, r, col in ((0.5, 0.32, 0.30, (109, 178, 87)), (0.34, 0.45, 0.20, (99, 168, 79)),
                           (0.66, 0.45, 0.20, (120, 188, 96))):
        d.ellipse([w * cx - w * r, h * cy - w * r, w * cx + w * r, h * cy + w * r],
                  fill=col + (255,))
    return img, (w // 2, int(h * 0.88))


def kiosk_sprite():
    img, pivot = building_sprite(1, 1, 70, (196, 148, 106), (150, 96, 66), floors=1,
                                 awning=(214, 92, 74, 255))
    return img.resize((img.width // 2, img.height // 2), Image.LANCZOS), (pivot[0] // 2, pivot[1] // 2)


def chip_icon(size=96):
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    m, r = size // 2, int(size * 0.34)
    for k in range(8):  # legs
        off = (k - 3.5) * size * 0.09
        d.line([(m + off, m - r - size * 0.10), (m + off, m + r + size * 0.10)],
               fill=(48, 110, 190, 255), width=max(2, size // 24))
        d.line([(m - r - size * 0.10, m + off), (m + r + size * 0.10, m + off)],
               fill=(48, 110, 190, 255), width=max(2, size // 24))
    d.rounded_rectangle([m - r, m - r, m + r, m + r], radius=size // 8,
                        fill=(74, 154, 240, 255), outline=(235, 244, 255, 255),
                        width=max(2, size // 32))
    ir = int(r * 0.5)
    d.rounded_rectangle([m - ir, m - ir, m + ir, m + ir], radius=size // 16,
                        fill=(150, 202, 252, 255))
    return img


def fountain_sprite():
    w, h = 260, 200
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([20, h - 90, w - 20, h - 10], fill=(200, 192, 175, 255),
              outline=(170, 160, 140, 255), width=4)
    d.ellipse([45, h - 78, w - 45, h - 22], fill=(111, 194, 216, 255))
    d.rectangle([w // 2 - 18, h - 120, w // 2 + 18, h - 55], fill=(190, 182, 165, 255))
    chip = chip_icon(120)
    img.alpha_composite(chip, (w // 2 - 60, 5))
    return img, (w // 2, h - 50)


def backdrop(size=2048):
    rng = np.random.default_rng(7)
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32) / size
    base = np.array([196, 214, 186], np.float32)
    edge = np.array([166, 190, 205], np.float32)
    dist = np.sqrt((xx - 0.5) ** 2 + (yy - 0.5) ** 2) * 1.6
    dist = np.clip(dist, 0, 1)[..., None]
    arr = base * (1 - dist) + edge * dist
    img = Image.fromarray(arr.astype(np.uint8), "RGB")
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(60):
        x, y, r = rng.integers(0, size), rng.integers(0, size), int(rng.integers(40, 200))
        tint = int(rng.integers(0, 2)) * 24 - 12
        d.ellipse([x - r, y - r, x + r, y + r], fill=(180 + tint, 200 + tint, 180 + tint, 30))
    return img.filter(ImageFilter.GaussianBlur(24))
