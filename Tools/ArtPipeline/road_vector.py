#!/usr/bin/env python3
"""Stage 1+2 of the road tile pipeline.

Stage 1 — vector design in flat top-down space: every tile is a unit square SRC x SRC,
the road geometry is a single asphalt region built from axis-aligned strips, a center
square and two corner-rounding operations:
  * add-rounding on every pair of adjacent OPEN sides (junction / inner turn corner);
  * cut-rounding on every pair of adjacent CLOSED sides (outer turn corner, dead ends).
Curbs, edge lines, dashes and zebras all derive from the same region, so geometry can
never disagree with itself. Tiling correctness is verified mathematically: the boundary
signature of every side depends only on open/closed, hence any two tiles match.

Stage 2 — the same geometry is warped into the 2:1 isometric diamond by the exact
affine map in artgen.warp_to_diamond.
"""
import os

from PIL import Image, ImageDraw, ImageFilter

SRC = 256          # top-down tile size
ROAD_HALF = 75     # half width of the drivable road
C0 = SRC // 2 - ROAD_HALF   # 53  — left/top edge of the drivable band
C1 = SRC // 2 + ROAD_HALF   # 203 — right/bottom edge
CURB_W = 7
R_ADD = 22         # curb radius at junction corners (adjacent open pair)
R_CUT = 24         # outer turn / dead-end corner radius — small, a sharp bend with a
                   # short rounding like the reference, not a sweeping arc

DASH_COLOR = (246, 245, 242, 255)
EDGE_LINE = (238, 235, 232, 255)
CURB_COLOR = (228, 216, 208, 255)
CURB_LIGHT = (243, 236, 228, 255)
CURB_DARK = (152, 142, 150, 255)

# corner of the tile <-> the pair of adjacent sides meeting there, and the matching
# corner of the drivable band (the point where two curb lines would intersect)
CORNERS = [
    (("N", "W"), (0, 0), (C0, C0)),
    (("N", "E"), (SRC, 0), (C1, C0)),
    (("S", "E"), (SRC, SRC), (C1, C1)),
    (("S", "W"), (0, SRC), (C0, C1)),
]

STRIPS = {
    "N": (C0, 0, C1, C0), "S": (C0, C1, C1, SRC),
    "W": (0, C0, C0, C1), "E": (C1, C0, SRC, C1),
}


def asphalt_mask(conns: str) -> Image.Image:
    """The exact (aliased) asphalt region of a tile."""
    m = Image.new("L", (SRC, SRC), 0)
    d = ImageDraw.Draw(m)
    d.rectangle((C0, C0, C1, C1), fill=255)
    for side in conns:
        d.rectangle(STRIPS[side], fill=255)

    for (pair, tile_corner, band_corner) in CORNERS:
        a_open, b_open = pair[0] in conns, pair[1] in conns
        bx, by = band_corner
        dx = 1 if tile_corner[0] == 0 else -1   # direction from the corner into the tile
        dy = 1 if tile_corner[1] == 0 else -1

        if a_open and b_open:
            # junction corner: asphalt gains the square outside the curb intersection,
            # minus the rounding circle centered inside the sidewalk corner
            r = R_ADD
            cx, cy = bx - dx*r, by - dy*r     # circle center, inside the sidewalk corner
            sq = (min(bx, cx), min(by, cy), max(bx, cx), max(by, cy))
            d.rectangle(sq, fill=255)
            # carve back the circle quadrant
            circle = Image.new("L", (SRC, SRC), 0)
            ImageDraw.Draw(circle).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
            m.paste(0, (0, 0), Image.composite(circle, Image.new("L", (SRC, SRC), 0),
                                               mask_rect(sq)))
        elif not (a_open or b_open):
            # outer corner of a turn / dead end bottom: asphalt loses the region of its
            # square corner that lies outside the big rounding circle
            r = R_CUT
            cx, cy = bx + dx*r, by + dy*r     # circle center, inside the asphalt
            sq = (min(bx, cx), min(by, cy), max(bx, cx), max(by, cy))
            circle = Image.new("L", (SRC, SRC), 0)
            ImageDraw.Draw(circle).ellipse((cx - r, cy - r, cx + r, cy + r), fill=255)
            cut = Image.composite(Image.new("L", (SRC, SRC), 0), mask_rect(sq), circle)
            m.paste(0, (0, 0), cut)
    return m


def mask_rect(box) -> Image.Image:
    m = Image.new("L", (SRC, SRC), 0)
    ImageDraw.Draw(m).rectangle(box, fill=255)
    return m


def _spread(mask: Image.Image, radius: float, threshold: int) -> Image.Image:
    """Euclidean-ish dilate (low threshold) / erode (high threshold) via blur."""
    return mask.filter(ImageFilter.GaussianBlur(radius)).point(
        lambda v: 255 if v >= threshold else 0)


def dilate(mask, r):
    return _spread(mask, r, 40)


def erode(mask, r):
    return _spread(mask, r, 215)


def sub(a: Image.Image, b: Image.Image) -> Image.Image:
    from PIL import ImageChops
    return ImageChops.subtract(a, b)


def band_or(a, b):
    from PIL import ImageChops
    return ImageChops.lighter(a, b)


def dash_path(conns: str):
    """Center-line polyline segments: from each open side midpoint to the tile center."""
    mids = {"N": (SRC // 2, 0), "S": (SRC // 2, SRC), "W": (0, SRC // 2), "E": (SRC, SRC // 2)}
    return [(mids[s], (SRC // 2, SRC // 2)) for s in conns]


def render_topdown(conns: str, asphalt_tex: Image.Image, sidewalk_tex: Image.Image) -> Image.Image:
    """Stage 2 rendering in top-down space from the vector geometry."""
    am = asphalt_mask(conns)
    img = sidewalk_tex.convert("RGBA").resize((SRC, SRC), Image.LANCZOS).copy()
    asphalt = asphalt_tex.convert("RGBA").resize((SRC, SRC), Image.LANCZOS)
    img.paste(asphalt, (0, 0), am)

    d = ImageDraw.Draw(img)

    # curb band around the asphalt, with a light outer rim and dark inner rim
    curb = sub(dilate(am, CURB_W*0.72), am)
    img.paste(Image.new("RGBA", img.size, CURB_COLOR), (0, 0), curb)
    light = sub(dilate(am, CURB_W*0.72), dilate(am, CURB_W*0.5))
    img.paste(Image.new("RGBA", img.size, CURB_LIGHT), (0, 0), light)
    dark = sub(dilate(am, 1.6), am)
    img.paste(Image.new("RGBA", img.size, CURB_DARK), (0, 0), dark)

    # white edge line: constant offset inside the asphalt, follows every arc
    line = sub(erode(am, 7.5), erode(am, 10.5))
    img.paste(Image.new("RGBA", img.size, EDGE_LINE), (0, 0), line)

    open_count = len(conns)
    if open_count in (1, 2):
        # dashed center line: straight through or a sharp V at the tile center
        dash_len, gap = 21, 17
        for (p0, p1) in dash_path(conns):
            length = max(abs(p1[0] - p0[0]), abs(p1[1] - p0[1]))
            n = max(1, int(length // (dash_len + gap)) + 1)
            for k in range(n):
                t0 = k*(dash_len + gap)/length
                t1 = min(1.0, t0 + dash_len/length)
                if t0 >= 1.0:
                    break
                a = (p0[0] + (p1[0] - p0[0])*t0, p0[1] + (p1[1] - p0[1])*t0)
                b = (p0[0] + (p1[0] - p0[0])*t1, p0[1] + (p1[1] - p0[1])*t1)
                d.line([a, b], fill=DASH_COLOR, width=7)

    if open_count >= 3:
        # zebra crossings on every arm, clipped to the asphalt
        zebra = Image.new("L", (SRC, SRC), 0)
        zd = ImageDraw.Draw(zebra)
        inset, zlen, zw, zgap = 14, 38, 9, 10
        for side in conns:
            for s in range(C0 + 8, C1 - 8 - zw, zw + zgap):
                if side == "N":
                    zd.rectangle((s, inset, s + zw, inset + zlen), fill=255)
                elif side == "S":
                    zd.rectangle((s, SRC - inset - zlen, s + zw, SRC - inset), fill=255)
                elif side == "W":
                    zd.rectangle((inset, s, inset + zlen, s + zw), fill=255)
                else:
                    zd.rectangle((SRC - inset - zlen, s, SRC - inset, s + zw), fill=255)
        from PIL import ImageChops
        zebra = ImageChops.multiply(zebra, erode(am, 3))
        img.paste(Image.new("RGBA", img.size, DASH_COLOR), (0, 0), zebra)

    return img


def boundary_signature(conns: str, side: str, samples=64):
    """Classification of the tile border along `side` — used for the tiling proof."""
    am = asphalt_mask(conns)
    sig = []
    for k in range(samples):
        t = int((k + 0.5)*SRC/samples)
        if side == "N":
            p = (t, 0)
        elif side == "S":
            p = (t, SRC - 1)
        elif side == "W":
            p = (0, t)
        else:
            p = (SRC - 1, t)
        sig.append(1 if am.getpixel(p) > 127 else 0)
    return tuple(sig)


def verify_tiling() -> list:
    """Checks that every side's boundary signature depends only on open/closed, so any
    two adjacent tiles always match. Returns a list of problem strings."""
    problems = []
    reference = {True: None, False: None}
    for mask_bits in range(16):
        conns = "".join(k for bit, k in zip([1, 2, 4, 8], "NESW") if mask_bits & bit)
        for side in "NESW":
            is_open = side in conns
            sig = boundary_signature(conns, side)
            if reference[is_open] is None:
                reference[is_open] = sig
            elif reference[is_open] != sig:
                problems.append(f"tile '{conns or 'O'}' side {side}: signature mismatch")
    # open and closed must differ (sanity)
    if reference[True] == reference[False]:
        problems.append("open and closed signatures are identical")
    return problems


def stage1_presentation(out_path: str):
    """Line-art of all 16 tiles plus a chained 2D test map — the vector design proof."""
    cell = SRC + 24
    sheet = Image.new("RGB", (4*cell + 24, 4*cell + 24 + 3*SRC + 48), (250, 250, 252))
    d = ImageDraw.Draw(sheet)

    for idx in range(16):
        conns = "".join(k for bit, k in zip([1, 2, 4, 8], "NESW") if idx & bit)
        am = asphalt_mask(conns)
        x0 = 12 + (idx % 4)*cell
        y0 = 12 + (idx // 4)*cell
        # tile frame
        d.rectangle((x0, y0, x0 + SRC, y0 + SRC), outline=(200, 200, 210), width=1)
        # asphalt outline: edge of the mask
        outline = sub(dilate(am, 1.2), am)
        sheet.paste(Image.new("RGB", (SRC, SRC), (40, 60, 160)), (x0, y0), outline)
        # fill hint
        fill = am.point(lambda v: 24 if v else 0)
        sheet.paste(Image.new("RGB", (SRC, SRC), (120, 130, 150)), (x0, y0), fill)
        d.text((x0 + 4, y0 + 4), conns or "O", fill=(60, 60, 70))

    # chained 2D map: straights, a cross, T and turns in one connected layout
    layout = [
        ["SE", "EW", "ESW", "EW", "SW"],
        ["NS", "", "NS", "", "NS"],
        ["NES", "EW", "NESW", "EW", "NSW"],
        ["NS", "", "NS", "", "NS"],
        ["NE", "EW", "NEW", "EW", "NW"],
    ]
    base_y = 4*cell + 36
    d.text((12, base_y - 14), "chained 2D map (tiling test):", fill=(60, 60, 70))
    for row_i, row in enumerate(layout):
        for col_i, name in enumerate(row):
            x0 = 12 + col_i*SRC
            y0 = base_y + row_i*SRC
            if not name:
                d.rectangle((x0, y0, x0 + SRC, y0 + SRC), fill=(238, 238, 242))
                continue
            conns = "".join(c for c in "NESW" if c in name)
            am = asphalt_mask(conns)
            fill = am.point(lambda v: 255 if v else 0)
            sheet.paste(Image.new("RGB", (SRC, SRC), (120, 130, 150)), (x0, y0), fill)
            outline = sub(dilate(am, 1.2), am)
            sheet.paste(Image.new("RGB", (SRC, SRC), (40, 60, 160)), (x0, y0), outline)

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    sheet.save(out_path)
