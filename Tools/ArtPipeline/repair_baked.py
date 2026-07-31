#!/usr/bin/env python3
"""One-off repair of already-baked sprites (no Gemini calls): closes the transparent
gap between the object and its baked shadow by spreading the shadow under the object,
then re-pasting the clean sprite. Needs clean (non-baked) sprites in Assets/Game —
run build_all.py with ArtSrc/baked/meta.json moved aside first."""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASSETS = os.path.join(ROOT, "Assets", "Game")
BAKED = os.path.join(ROOT, "ArtSrc", "baked")


def repair(sub, base, manifest, meta):
    rel = "%s/%s.png" % (sub, base)
    sprite = Image.open(os.path.join(ASSETS, rel)).convert("RGBA")
    baked = Image.open(os.path.join(BAKED, base + ".png")).convert("RGBA")
    sp = manifest["sprites"][rel]["pivot"]
    bp = meta[base]["pivot"]
    origin = (bp[0] - sp[0], bp[1] - sp[1])

    W, H = baked.size
    a = np.asarray(baked.getchannel("A"), dtype=np.float32) / 255.0

    sil = np.zeros((H, W), dtype=bool)
    m = np.asarray(sprite.getchannel("A")) > 90
    y0, x0 = origin[1], origin[0]
    ys = slice(max(0, y0), min(H, y0 + m.shape[0]))
    xs = slice(max(0, x0), min(W, x0 + m.shape[1]))
    sil[ys, xs] = m[ys.start - y0:ys.stop - y0, xs.start - x0:xs.stop - x0]
    dil = np.asarray(Image.fromarray((sil * 255).astype(np.uint8), "L")
                     .filter(ImageFilter.MaxFilter(7))) > 0

    # pure shadow = baked alpha outside the object zone; refill the zone from outside
    shadow_a = a.copy()
    shadow_a[dil] = 0.0
    fill = shadow_a.copy()
    for _ in range(6):
        fill_img = Image.fromarray((fill * 255).astype(np.uint8), "L")
        fill = np.asarray(fill_img.filter(ImageFilter.MaxFilter(5)), dtype=np.float32) / 255.0
    shadow_a = np.where(dil, fill, shadow_a)

    out = np.zeros((H, W, 4), dtype=np.uint8)
    rgb = np.asarray(baked.convert("RGB"))
    out[..., :3] = rgb
    # keep the shadow tint constant where we refilled (baked rgb there may be sprite-tinted)
    out[..., 0][dil] = 35
    out[..., 1][dil] = 40
    out[..., 2][dil] = 60
    out[..., 3] = np.clip(shadow_a * 255.0, 0, 255).astype(np.uint8)
    result = Image.fromarray(out, "RGBA")
    result.paste(sprite, origin, sprite)
    result.save(os.path.join(BAKED, base + ".png"))
    return True


def main():
    manifest = json.load(open(os.path.join(ASSETS, "art_manifest.json")))
    meta = json.load(open(os.path.join(BAKED, "meta.json.tmp")))
    count = 0
    for sub in ("Buildings", "Props"):
        for f in sorted(os.listdir(os.path.join(ASSETS, sub))):
            base = f[:-4]
            if f.endswith(".png") and base in meta:
                repair(sub, base, manifest, meta)
                count += 1
                print("fixed", base, flush=True)
    print("done:", count)


if __name__ == "__main__":
    main()
