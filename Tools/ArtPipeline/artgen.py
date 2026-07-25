#!/usr/bin/env python3
"""Art pipeline for Token Delivery: builds isometric 2:1 tiles and sprites.

Sources are top-down textures (generated via imagegen MCP or procedural placeholders in
placeholders.py); ground tiles are affine-warped to 256x128 diamonds so they tile perfectly.
Road markings are drawn in top-down space before warping. Free-standing sprites (buildings,
props) are composed separately with the same diamond base metrics.

Tile space convention (must match C++ CityModel):
  cell (i, j) -> screen x = (i - j) * TILE_W/2, screen y = (i + j) * TILE_H/2 (y down)
  E = +i, W = -i, S = +j, N = -j. In top-down source space: u axis = +i, v axis = +j.
"""
import os
from PIL import Image, ImageDraw

TILE_W, TILE_H = 256, 128
SRC = 256  # top-down source tile size


def warp_to_diamond(topdown: Image.Image, out_w=TILE_W, out_h=TILE_H) -> Image.Image:
    """Affine-map a square top-down tile onto a 2:1 diamond (RGBA, transparent corners)."""
    src = topdown.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    s = SRC / 256.0
    # inverse mapping (out x,y) -> (src u,v), derived from x=(u-v)*128+128, y=(u+v)*64
    coeffs = (1 * s, 2 * s, -128 * s, -1 * s, 2 * s, 128 * s)
    out = src.transform((out_w, out_h), Image.AFFINE, coeffs, resample=Image.BILINEAR)
    # mask to exact diamond to kill sampling bleed outside
    mask = Image.new("L", (out_w, out_h), 0)
    d = ImageDraw.Draw(mask)
    d.polygon([(out_w // 2, 0), (out_w - 1, out_h // 2), (out_w // 2, out_h - 1), (0, out_h // 2)],
              fill=255)
    r, g, b, a = out.split()
    a = Image.composite(a, Image.new("L", (out_w, out_h), 0), mask)
    return Image.merge("RGBA", (r, g, b, a))


ROAD_DIRS = {"N": (0, -1), "S": (0, 1), "E": (1, 0), "W": (-1, 0)}
# top-down edge midpoints for each direction (u right = E, v down = S)
EDGE_MID = {"N": (SRC // 2, 0), "S": (SRC // 2, SRC), "E": (SRC, SRC // 2), "W": (0, SRC // 2)}

DASH_COLOR = (245, 244, 238, 255)
CURB_COLOR = (200, 201, 206, 255)


def draw_road_topdown(asphalt: Image.Image, conns: str) -> Image.Image:
    """Road tile in top-down space: asphalt + center dashes along connections + curbs on
    closed edges. conns is a subset string of 'NESW'."""
    img = asphalt.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    c = SRC // 2
    dash_w = 7
    dash_len, gap = 22, 18

    def dash_line(p0, p1):
        x0, y0 = p0
        x1, y1 = p1
        length = max(abs(x1 - x0), abs(y1 - y0))
        if length == 0:
            return
        n = max(1, int(length // (dash_len + gap)))
        for k in range(n + 1):
            t0 = k * (dash_len + gap) / length
            t1 = min(1.0, t0 + dash_len / length)
            if t0 >= 1.0:
                break
            a = (x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0)
            b = (x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1)
            d.line([a, b], fill=DASH_COLOR, width=dash_w)

    conn_set = [k for k in "NESW" if k in conns]
    if len(conn_set) == 2 and set(conn_set) in ({"N", "S"}, {"E", "W"}):
        dash_line(EDGE_MID[conn_set[0]], EDGE_MID[conn_set[1]])
    elif len(conn_set) == 2:  # corner: two half-dashes meeting at center
        for k in conn_set:
            dash_line(EDGE_MID[k], (c, c))
    elif len(conn_set) == 1:  # dead end
        dash_line(EDGE_MID[conn_set[0]], (c, c))
    else:  # T / cross: crosswalk stripes across each connected arm
        stripe_w, stripe_l, margin = 8, 52, 26
        for k in conn_set:
            dx, dy = ROAD_DIRS[k]
            for s in range(-2, 3):
                if dx != 0:  # arm along u: stripes are vertical lines near the edge
                    x = (SRC - margin) if dx > 0 else margin
                    d.line([(x, c + s * 16 - stripe_l // 8), (x, c + s * 16 + stripe_l // 8)],
                           fill=DASH_COLOR, width=stripe_w)
                else:
                    y = (SRC - margin) if dy > 0 else margin
                    d.line([(c + s * 16 - stripe_l // 8, y), (c + s * 16 + stripe_l // 8, y)],
                           fill=DASH_COLOR, width=stripe_w)

    # curbs on closed edges
    curb = 6
    if "N" not in conns:
        d.rectangle([0, 0, SRC, curb], fill=CURB_COLOR)
    if "S" not in conns:
        d.rectangle([0, SRC - curb, SRC, SRC], fill=CURB_COLOR)
    if "W" not in conns:
        d.rectangle([0, 0, curb, SRC], fill=CURB_COLOR)
    if "E" not in conns:
        d.rectangle([SRC - curb, 0, SRC, SRC], fill=CURB_COLOR)
    return img


def build_road_tiles(asphalt: Image.Image, out_dir: str):
    os.makedirs(out_dir, exist_ok=True)
    for mask in range(16):
        conns = "".join(k for bit, k in zip([1, 2, 4, 8], "NESW") if mask & bit)
        name = "road_" + (conns if conns else "O")
        tile = warp_to_diamond(draw_road_topdown(asphalt, conns))
        tile.save(os.path.join(out_dir, name + ".png"))


def build_ground_tile(texture: Image.Image, out_path: str):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    warp_to_diamond(texture).save(out_path)
