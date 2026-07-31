#!/usr/bin/env python3
"""Bakes a Gemini-drawn cast shadow INTO each building/prop sprite.

One Gemini pass per sprite: the final sprite composited on white gets its shadow
drawn (the prompt forbids touching the object; a silhouette diff enforces it).
The shadow is then extracted programmatically (darkening vs white, outside the
object silhouette) and composed UNDER the original untouched sprite — so the
shadow always matches its object exactly. Results go to ArtSrc/baked/ and are
picked up by build_all.py instead of the plain sprites.

Usage: python3 Tools/ArtPipeline/bake_shadows.py [name ...]   (default: all)
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "o2", "Tools", "ImageGen"))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gemini_image as gi

from gemini_shadows import (ASSETS, SYSTEM_ADD, PROMPT_ADD, SHADOW_COLOR,
                            compose_canvas, silhouette_diff, list_targets)

OUT_DIR = os.path.join(ROOT, "ArtSrc", "baked")


def extract_shadow_outside(with_shadow, alpha, origin, canvas_size):
    """Darkening vs white outside the (slightly dilated) object silhouette -> RGBA."""
    arr = np.asarray(with_shadow.convert("RGB"), dtype=np.float32)
    lum = arr.min(axis=2)
    a = np.clip((250.0 - lum) / 250.0, 0.0, 1.0)
    a[a < 0.05] = 0.0

    sil = np.zeros(canvas_size[::-1], dtype=bool)
    m = np.asarray(alpha) > 90
    sil[origin[1]:origin[1] + m.shape[0], origin[0]:origin[0] + m.shape[1]] = m
    sil_img = Image.fromarray((sil * 255).astype(np.uint8), "L").filter(ImageFilter.MaxFilter(5))
    dil = np.asarray(sil_img) > 0
    a[dil] = 0.0

    # continue the shadow under the object: the cut zone (object silhouette + 2px, where
    # the object's own dark AA contour polluted the extraction) is refilled by spreading
    # the true shadow inwards, so the pasted sprite's soft edge lands on shadow, not on
    # a transparent gap
    fill = a.copy()
    for _ in range(5):
        fill_img = Image.fromarray((fill * 255).astype(np.uint8), "L")
        fill = np.asarray(fill_img.filter(ImageFilter.MaxFilter(5)), dtype=np.float32) / 255.0
    a = np.where(dil, fill, a)

    out = np.zeros((canvas_size[1], canvas_size[0], 4), dtype=np.uint8)
    out[..., 0] = SHADOW_COLOR[0]
    out[..., 1] = SHADOW_COLOR[1]
    out[..., 2] = SHADOW_COLOR[2]
    out[..., 3] = (a * 255).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def process(sub, base, manifest):
    rel = "%s/%s.png" % (sub, base)
    entry = manifest["sprites"][rel]
    img = Image.open(os.path.join(ASSETS, rel)).convert("RGBA")
    canvas, pivot, origin = compose_canvas(img, entry["pivot"])

    with_shadow, _ = gi.generate(SYSTEM_ADD, PROMPT_ADD, images=[canvas], aspect="1:1")
    with_shadow = with_shadow.convert("RGB").resize(canvas.size, Image.LANCZOS)
    diff, _ = silhouette_diff(canvas, with_shadow, img.getchannel("A"), origin)
    if diff > 14.0:
        return None, "pass1 altered the object (diff %.1f)" % diff

    baked = extract_shadow_outside(with_shadow, img.getchannel("A"), origin, canvas.size)
    baked.paste(img, origin, img)

    bbox = baked.getbbox()
    if not bbox:
        return None, "empty result"
    baked = baked.crop(bbox)
    return (baked, (pivot[0] - bbox[0], pivot[1] - bbox[1])), "ok"


def main():
    manifest = json.load(open(os.path.join(ASSETS, "art_manifest.json")))
    os.makedirs(OUT_DIR, exist_ok=True)
    meta_path = os.path.join(OUT_DIR, "meta.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}

    only = set(sys.argv[1:])
    targets = [(s, b) for s, b in list_targets() if not only or b in only]
    accepted, rejected = [], []
    for sub, base in targets:
        try:
            result, why = process(sub, base, manifest)
        except gi.GeminiError as e:
            result, why = None, "gemini: %s" % str(e)[:120]
        if result is None:
            rejected.append(base)
            print("REJECT %-20s %s" % (base, why), flush=True)
            continue
        baked, pivot = result
        baked.save(os.path.join(OUT_DIR, base + ".png"))
        meta[base] = {"pivot": [int(pivot[0]), int(pivot[1])]}
        accepted.append(base)
        print("OK     %-20s pivot %s size %s" % (base, pivot, baked.size), flush=True)

    json.dump(meta, open(meta_path, "w"), indent=1, sort_keys=True)
    print("done: %d accepted, %d rejected %s" % (len(accepted), len(rejected), rejected))


if __name__ == "__main__":
    main()
