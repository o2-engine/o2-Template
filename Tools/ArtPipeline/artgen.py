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
import random
from PIL import Image, ImageDraw, ImageFilter

TILE_W, TILE_H = 256, 128
SRC = 256  # top-down source tile size


GROUND_BLEED = 2  # homogeneous ground tiles (plaza/grass base) may slightly overlap


def warp_to_diamond(topdown: Image.Image, bleed=0, feather=0.0) -> Image.Image:
    """Affine-map a square top-down tile onto a 2:1 diamond (RGBA).

    Road tiles use bleed=0 with a hard-edged mask: 256x128 diamonds parquet exactly, so
    detailed edge content (curbs, lines) is never covered by a neighbour. Homogeneous
    tiles may pass a small bleed+feather to melt into each other."""
    out_w, out_h = TILE_W + bleed * 2, TILE_H + bleed * 2
    src = topdown.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    hw, hh = out_w / 2.0, out_h / 2.0
    au, bu = SRC / (2.0 * hw), SRC / (2.0 * hh)
    coeffs = (au, bu, SRC / 2.0 - hw * au - hh * bu,
              -au, bu, SRC / 2.0 + hw * au - hh * bu)
    out = src.transform((out_w, out_h), Image.AFFINE, coeffs, resample=Image.BILINEAR)
    mask = Image.new("L", (out_w, out_h), 0)
    d = ImageDraw.Draw(mask)
    d.polygon([(out_w // 2, 0), (out_w - 1, out_h // 2), (out_w // 2, out_h - 1), (0, out_h // 2)],
              fill=255)
    if feather > 0.0:
        mask = mask.filter(ImageFilter.GaussianBlur(feather))
    r, g, b, a = out.split()
    a = Image.composite(a, Image.new("L", (out_w, out_h), 0), mask)
    return Image.merge("RGBA", (r, g, b, a))


ROAD_DIRS = {"N": (0, -1), "S": (0, 1), "E": (1, 0), "W": (-1, 0)}
# top-down edge midpoints for each direction (u right = E, v down = S)
EDGE_MID = {"N": (SRC // 2, 0), "S": (SRC // 2, SRC), "E": (SRC, SRC // 2), "W": (0, SRC // 2)}

DASH_COLOR = (246, 245, 242, 255)
EDGE_LINE = (236, 233, 230, 235)
CURB_COLOR = (228, 216, 208, 255)
CURB_LIGHT = (243, 236, 228, 255)
CURB_DARK = (150, 140, 148, 255)
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

    def draw_curb_rect(box, horizontal):
        # raised curb: light top face, dark side shadow towards the asphalt below-right
        d.rectangle(box, fill=CURB_COLOR)
        if horizontal:
            d.line([(box[0], box[1] + 1), (box[2], box[1] + 1)], fill=CURB_LIGHT, width=2)
            d.line([(box[0], box[3] - 1), (box[2], box[3] - 1)], fill=CURB_DARK, width=3)
        else:
            d.line([(box[0] + 1, box[1]), (box[0] + 1, box[3])], fill=CURB_LIGHT, width=2)
            d.line([(box[2] - 1, box[1]), (box[2] - 1, box[3])], fill=CURB_DARK, width=3)

    # straight sidewalk strips + raised curbs on the closed edges
    for side in closed:
        paste_pavement_mask(lambda md, b=strip_box(side): md.rectangle(b, fill=255))
        draw_curb_rect(curb_box(side), side in ("N", "S"))

    # white edge lines along the closed sides, full tile length so straights chain into
    # continuous lines; corner shapes are drawn after and cover the excess
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

    # corners. Two kinds:
    #  - junction pocket (both arms open): sidewalk square with a small convex nose and
    #    straight curb segments joining the neighbouring strips;
    #  - sharp chamfer (both sides closed — turns and dead ends): the sidewalk cuts the
    #    road corner with a straight diagonal, reading as a crisp bend in isometry
    NOSE_R = 12
    CHAMFER = 66
    for (corner, (sa, sb), (ang0, ang1)) in TILE_CORNERS:
        open_a, open_b = sa in conns, sb in conns
        cx = CURB_FULL if corner[0] == 0 else SRC - CURB_FULL   # inner corner point
        cy = CURB_FULL if corner[1] == 0 else SRC - CURB_FULL
        dx = 1 if corner[0] == 0 else -1                        # direction into the road
        dy = 1 if corner[1] == 0 else -1

        if open_a and open_b:
            sq = [min(corner[0], cx), min(corner[1], cy), max(corner[0], cx), max(corner[1], cy)]
            nose = [cx - NOSE_R, cy - NOSE_R, cx + NOSE_R, cy + NOSE_R]
            paste_pavement_mask(lambda md: (md.rectangle(sq, fill=255),
                                            md.pieslice(nose, ang0, ang1, fill=255)))
            d.arc(nose, ang0, ang1, fill=CURB_COLOR, width=CURB_W)

            # straight curb continuations from the tile edges to the nose arc
            vx = cx - CURB_W if dx > 0 else cx
            vy0, vy1 = (0, cy - NOSE_R) if dy > 0 else (cy + NOSE_R, SRC)
            draw_curb_rect([vx, vy0, vx + CURB_W, vy1], False)
            hy = cy - CURB_W if dy > 0 else cy
            hx0, hx1 = (0, cx - NOSE_R) if dx > 0 else (cx + NOSE_R, SRC)
            draw_curb_rect([hx0, hy, hx1, hy + CURB_W], True)
        elif not (open_a or open_b):
            p_h = (cx + dx*CHAMFER, cy)   # chamfer ends on the strip curbs
            p_v = (cx, cy + dy*CHAMFER)
            paste_pavement_mask(lambda md: md.polygon([(cx, cy), p_h, p_v], fill=255))
            d.line([p_h, p_v], fill=CURB_COLOR, width=CURB_W)
            # dark side towards the road, white edge line inside the sidewalk
            n = (dx*2, dy*2)
            d.line([(p_h[0] + n[0], p_h[1] + n[1]), (p_v[0] + n[0], p_v[1] + n[1])],
                   fill=CURB_DARK, width=2)
            m = (dx*9, dy*9)
            d.line([(p_h[0] + m[0], p_h[1] + m[1]), (p_v[0] + m[0], p_v[1] + m[1])],
                   fill=EDGE_LINE, width=4)

    # center dashes along the driving directions
    conn_set = [k for k in "NESW" if k in conns]
    if len(conn_set) == 2 and set(conn_set) in ({"N", "S"}, {"E", "W"}):
        dash_line(EDGE_MID[conn_set[0]], EDGE_MID[conn_set[1]])
    elif len(conn_set) == 2:  # corner: two half-dashes meeting at center
        for k in conn_set:
            dash_line(EDGE_MID[k], (c, c))
    elif len(conn_set) == 1:  # dead end
        dash_line(EDGE_MID[conn_set[0]], (c, c))

    # zebra crossings on the approaches of T-junctions and crossroads
    if len(conn_set) >= 3:
        zebra_w, zebra_len, zebra_gap, inset = 9, 40, 10, 16
        lane = (CURB_FULL + 12, SRC - CURB_FULL - 12)  # drivable span across the road
        for k in conn_set:
            for stripe in range(lane[0] + 6, lane[1] - 6, zebra_w + zebra_gap):
                if k == "N":
                    d.rectangle([stripe, inset, stripe + zebra_w, inset + zebra_len],
                                fill=DASH_COLOR)
                elif k == "S":
                    d.rectangle([stripe, SRC - inset - zebra_len, stripe + zebra_w, SRC - inset],
                                fill=DASH_COLOR)
                elif k == "W":
                    d.rectangle([inset, stripe, inset + zebra_len, stripe + zebra_w],
                                fill=DASH_COLOR)
                else:
                    d.rectangle([SRC - inset - zebra_len, stripe, SRC - inset, stripe + zebra_w],
                                fill=DASH_COLOR)

    return img


def draw_sidewalk_topdown(pavement: Image.Image) -> Image.Image:
    """Full sidewalk tile (block ground next to roads)."""
    return pavement.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)


def build_road_tiles(asphalt: Image.Image, pavement: Image.Image, out_dir: str):
    # stage 2: renders the verified vector geometry (road_vector) in top-down space and
    # converts it to the isometric diamond by the exact affine map
    from road_vector import render_topdown
    os.makedirs(out_dir, exist_ok=True)
    for mask in range(16):
        conns = "".join(k for bit, k in zip([1, 2, 4, 8], "NESW") if mask & bit)
        name = "road_" + (conns if conns else "O")
        tile = warp_to_diamond(render_topdown(conns, asphalt, pavement))
        tile.save(os.path.join(out_dir, name + ".png"))


def build_ground_tile(texture: Image.Image, out_path: str):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    warp_to_diamond(texture, bleed=GROUND_BLEED, feather=1.0).save(out_path)


def build_grass_patch(texture: Image.Image, out_path: str, seed=11):
    """Grass tile with an irregular organic edge that overlaps neighbouring tiles: an
    oversized diamond whose mask is jittered by blobs along the border, then feathered."""
    rng = random.Random(seed)
    overlap = 18
    out_w, out_h = TILE_W + overlap * 2, TILE_H + overlap * 2
    src = texture.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    hw, hh = out_w / 2.0, out_h / 2.0
    au, bu = SRC / (2.0 * hw), SRC / (2.0 * hh)
    coeffs = (au, bu, SRC / 2.0 - hw * au - hh * bu,
              -au, bu, SRC / 2.0 + hw * au - hh * bu)
    out = src.transform((out_w, out_h), Image.AFFINE, coeffs, resample=Image.BILINEAR)

    mask = Image.new("L", (out_w, out_h), 0)
    md = ImageDraw.Draw(mask)
    diamond = [(out_w // 2, overlap // 2), (out_w - overlap // 2, out_h // 2),
               (out_w // 2, out_h - overlap // 2), (overlap // 2, out_h // 2)]
    md.polygon(diamond, fill=255)
    # jitter the border with blobs sticking out and notches cut in
    for i in range(4):
        x0, y0 = diamond[i]
        x1, y1 = diamond[(i + 1) % 4]
        for k in range(14):
            t = (k + 0.5) / 14.0
            px, py = x0 + (x1 - x0) * t, y0 + (y1 - y0) * t
            r = rng.randint(5, 13)
            ry = max(3, r // 2)
            if rng.random() < 0.72:
                md.ellipse([px - r, py - ry, px + r, py + ry], fill=255)
            else:
                md.ellipse([px - r + 2, py - ry + 1, px + r - 2, py + ry - 1], fill=0)
    mask = mask.filter(ImageFilter.GaussianBlur(1.4))

    r, g, b, a = out.split()
    a = Image.composite(a, Image.new("L", (out_w, out_h), 0), mask)
    result = Image.merge("RGBA", (r, g, b, a))
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    result.save(out_path)
    return result.size
