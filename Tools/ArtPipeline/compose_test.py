#!/usr/bin/env python3
"""Composes a small demo city from built tiles to visually verify tiling, pivots and
depth order. Mirrors the C++ iso math: screen x=(i-j)*128, y=(i+j)*64 (y down)."""
import json
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "Assets", "Game")

with open(os.path.join(OUT, "art_manifest.json")) as f:
    manifest = json.load(f)

N = 7
road = set()
for k in range(N):
    road |= {(k, 0), (k, N - 1), (0, k), (N - 1, k), (k, 3), (3, k)}

canvas = Image.new("RGBA", (2200, 1400), (196, 214, 186, 255))
ox, oy = 1100, 150


def screen(i, j):
    return ox + (i - j) * 128, oy + (i + j) * 64


def paste(rel, i, j):
    img = Image.open(os.path.join(OUT, rel))
    px, py = manifest["sprites"][rel]["pivot"]
    x, y = screen(i, j)
    canvas.alpha_composite(img.convert("RGBA"), (int(x - px), int(y - py)))


def road_tile(i, j):
    conns = ""
    if (i, j - 1) in road: conns += "N"
    if (i + 1, j) in road: conns += "E"
    if (i, j + 1) in road: conns += "S"
    if (i - 1, j) in road: conns += "W"
    return "Tiles/road_" + (conns if conns else "O") + ".png"


cells = sorted({(i, j) for i in range(N) for j in range(N)}, key=lambda c: c[0] + c[1])
for (i, j) in cells:
    if (i, j) in road:
        paste(road_tile(i, j), i, j)
    else:
        paste("Tiles/pavement.png" if (i + j) % 2 else "Tiles/grass.png", i, j)

# buildings and props, drawn in depth order by far corner of footprint
objs = [("Buildings/house_brick_a.png", 1, 1), ("Buildings/house_double.png", 2, 1),
        ("Buildings/office_glass.png", 4, 1), ("Buildings/house_cream.png", 1, 2),
        ("Buildings/house_tall.png", 2, 2), ("Buildings/office_classic.png", 5, 4),
        ("Buildings/house_corner.png", 4, 4), ("Props/fountain.png", 1, 4),
        ("Props/tree_big.png", 2, 4), ("Props/kiosk.png", 1, 5),
        ("Props/tree_small.png", 5, 2)]
for rel, i, j in sorted(objs, key=lambda o: o[1] + o[2]):
    fp = manifest["sprites"][rel].get("footprint", [1, 1])
    ci, cj = i + fp[0] / 2 - 0.5, j + fp[1] / 2 - 0.5
    paste(rel, ci, cj)

out = os.path.join(ROOT, "ArtSrc", "compose_test.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
canvas.convert("RGB").save(out)
print(out)
