"""Ground shadows for buildings/props: the object silhouette projected onto the ground
(sun from the top-left — shadows fall to the bottom-right, steeper than the iso wall
slope so they read as separate from SE-facing walls). Each pixel projects from its
column's own base contact, so the shadow hugs base corners with no gap and keeps the
object's outline. No ambient ring."""
import numpy as np
from PIL import Image, ImageFilter

SHEAR = 0.35      # horizontal ground shift per pixel of height (to the right)
FLATTEN = 0.30    # vertical ground shift per pixel of height (downward)
OPACITY = 0.42
SHADOW_COLOR = (35, 40, 60)
BLUR = 2.5
PAD = 20


def build_object_shadow(img, pivot):
    """img: final RGBA sprite; pivot: (px, py) in top-left image pixels = the ground
    anchor. Returns (shadow RGBA image, shadow pivot)."""
    w, h = img.size
    px, py = int(pivot[0]), int(pivot[1])
    py = max(1, min(py, h - 1))

    # solid silhouette only: sub-0.7 alpha (glow halos, keying residue) casts nothing
    alpha = np.asarray(img.getchannel("A"), dtype=np.float32) / 255.0
    solid = alpha > 0.7
    col_has = solid.any(axis=0)
    if not col_has.any():
        return Image.new("RGBA", (2, 2), (0, 0, 0, 0)), (1, 1)

    xs = np.arange(w)
    top = np.where(col_has, solid.argmax(axis=0), 0)
    bottom = np.where(col_has, h - 1 - solid[::-1, :].argmax(axis=0), 0)
    # base contact per column; overhanging parts (tree crowns) cast from the pivot line
    ground = np.maximum(bottom, py)
    max_h = int(np.max(np.where(col_has, ground - top, 0)))

    W = w + PAD * 2 + int(SHEAR * max_h) + 2
    H = int(max(h, np.max(ground + FLATTEN * (ground - top)))) + PAD

    # project the silhouette: pixel at height hh above its column base lands at
    # (x + SHEAR*hh, ground(x) + FLATTEN*hh) — outline preserved, corners hugged
    acc = np.zeros((H, W), dtype=np.uint8)
    for hh in range(max_h + 1):
        ysrc = ground - hh
        valid = col_has & (ysrc >= 0) & (ysrc < h)
        if not valid.any():
            continue
        vals = solid[np.clip(ysrc, 0, h - 1), xs] & valid
        if not vals.any():
            continue
        X = (xs + SHEAR * hh).astype(np.int32) + PAD
        Y = (ground + FLATTEN * hh).astype(np.int32)
        sel = vals & (X < W) & (Y < H)
        acc[Y[sel], X[sel]] = 255
        acc[Y[sel], np.minimum(X[sel] + 1, W - 1)] = 255

    mask = Image.fromarray(acc, "L").filter(ImageFilter.MaxFilter(3))
    mask = mask.filter(ImageFilter.GaussianBlur(BLUR))
    cast = np.asarray(mask, dtype=np.float32) / 255.0

    # fade with distance from the base line
    ys = np.arange(H, dtype=np.float32)
    fade = 1.0 - 0.3 * np.clip((ys - py) / (FLATTEN * max(max_h, 1) + 1.0), 0.0, 1.0)
    cast *= fade[:, None]

    out = np.zeros((H, W, 4), dtype=np.uint8)
    out[..., 0] = SHADOW_COLOR[0]
    out[..., 1] = SHADOW_COLOR[1]
    out[..., 2] = SHADOW_COLOR[2]
    out[..., 3] = np.clip(cast * OPACITY * 255.0, 0, 255).astype(np.uint8)
    shadow = Image.fromarray(out, "RGBA")

    bbox = shadow.getbbox()
    if bbox:
        shadow = shadow.crop(bbox)
        return shadow, (px + PAD - bbox[0], py - bbox[1])
    return shadow, (px + PAD, py)
