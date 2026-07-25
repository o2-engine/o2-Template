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

DASH_COLOR = (246, 245, 242, 255)
EDGE_LINE = (236, 233, 230, 235)
CURB_COLOR = (228, 216, 208, 255)
CURB_SHADE = (196, 184, 176, 255)

SIDEWALK = 46  # sidewalk strip width on closed edges, px of the 256 top-down tile
CURB_W = 7
CURB_FULL = SIDEWALK + CURB_W

# tile corners: (corner point, adjacent side letters, pieslice angle range)
TILE_CORNERS = [((0, 0), ("N", "W"), (0, 90)),
                ((SRC, 0), ("N", "E"), (90, 180)),
                ((SRC, SRC), ("S", "E"), (180, 270)),
                ((0, SRC), ("S", "W"), (270, 360))]


def draw_road_topdown(asphalt: Image.Image, pavement: Image.Image, conns: str) -> Image.Image:
    """Road tile in top-down space: asphalt, sidewalk strips with curbs on the closed
    edges, rounded quarter-circle curbs on every corner (reference style), white edge
    lines along the curbs and center dashes. conns is a subset string of 'NESW'."""
    img = asphalt.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    pave = pavement.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    d = ImageDraw.Draw(img)
    c = SRC // 2
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
            d.line([a, b], fill=DASH_COLOR, width=7)

    def paste_pavement_mask(mask_draw_fn):
        mask = Image.new("L", (SRC, SRC), 0)
        mask_draw_fn(ImageDraw.Draw(mask))
        img.paste(pave, (0, 0), mask)
        return mask

    def strip_box(side):
        return {"N": (0, 0, SRC, SIDEWALK), "S": (0, SRC - SIDEWALK, SRC, SRC),
                "W": (0, 0, SIDEWALK, SRC), "E": (SRC - SIDEWALK, 0, SRC, SRC)}[side]

    def curb_box(side):
        return {"N": (0, SIDEWALK, SRC, CURB_FULL), "S": (0, SRC - CURB_FULL, SRC, SRC - SIDEWALK),
                "W": (SIDEWALK, 0, CURB_FULL, SRC), "E": (SRC - CURB_FULL, 0, SRC - SIDEWALK, SRC)
                }[side]

    closed = [s for s in "NESW" if s not in conns]

    # straight sidewalk strips + curbs on the closed edges
    for side in closed:
        paste_pavement_mask(lambda md, b=strip_box(side): md.rectangle(b, fill=255))
        d.rectangle(curb_box(side), fill=CURB_COLOR)

    # rounded corners: big quarter-circle sidewalk pockets on junction corners (radius
    # noticeably larger than the strip, reference style) and rounded inner curbs between
    # two closed sides
    POCKET_R = 86
    for (corner, (sa, sb), (ang0, ang1)) in TILE_CORNERS:
        open_a, open_b = sa in conns, sb in conns
        if open_a and open_b:
            bbox = [corner[0] - POCKET_R, corner[1] - POCKET_R,
                    corner[0] + POCKET_R, corner[1] + POCKET_R]
            paste_pavement_mask(lambda md: md.pieslice(bbox, ang0, ang1, fill=255))
            d.arc(bbox, ang0, ang1, fill=CURB_COLOR, width=CURB_W)
        elif not open_a and not open_b:
            inner = [corner[0] - POCKET_R - 12, corner[1] - POCKET_R - 12,
                     corner[0] + POCKET_R + 12, corner[1] + POCKET_R + 12]
            paste_pavement_mask(lambda md: md.pieslice(inner, ang0, ang1, fill=255))
            d.arc(inner, ang0, ang1, fill=CURB_COLOR, width=CURB_W)

    # white edge lines along the closed sides, like the reference road markings
    line_off = CURB_FULL + 9
    for side in closed:
        if side == "N":
            d.line([(0, line_off), (SRC, line_off)], fill=EDGE_LINE, width=4)
        elif side == "S":
            d.line([(0, SRC - line_off), (SRC, SRC - line_off)], fill=EDGE_LINE, width=4)
        elif side == "W":
            d.line([(line_off, 0), (line_off, SRC)], fill=EDGE_LINE, width=4)
        else:
            d.line([(SRC - line_off, 0), (SRC - line_off, SRC)], fill=EDGE_LINE, width=4)

    # center dashes along the driving directions
    conn_set = [k for k in "NESW" if k in conns]
    if len(conn_set) == 2 and set(conn_set) in ({"N", "S"}, {"E", "W"}):
        dash_line(EDGE_MID[conn_set[0]], EDGE_MID[conn_set[1]])
    elif len(conn_set) == 2:  # corner: two half-dashes meeting at center
        for k in conn_set:
            dash_line(EDGE_MID[k], (c, c))
    elif len(conn_set) == 1:  # dead end
        dash_line(EDGE_MID[conn_set[0]], (c, c))

    return img


def draw_sidewalk_topdown(pavement: Image.Image) -> Image.Image:
    """Full sidewalk tile (block ground next to roads)."""
    return pavement.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)


def build_road_tiles(asphalt: Image.Image, pavement: Image.Image, out_dir: str):
    os.makedirs(out_dir, exist_ok=True)
    for mask in range(16):
        conns = "".join(k for bit, k in zip([1, 2, 4, 8], "NESW") if mask & bit)
        name = "road_" + (conns if conns else "O")
        tile = warp_to_diamond(draw_road_topdown(asphalt, pavement, conns))
        tile.save(os.path.join(out_dir, name + ".png"))


def build_ground_tile(texture: Image.Image, out_path: str):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    warp_to_diamond(texture).save(out_path)
