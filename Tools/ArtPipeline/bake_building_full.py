#!/usr/bin/env python3
"""Gemini pass for buildings: adds a properly LIT stone base trim (plinth) AND the
cast shadow in one edit, then recovers true alpha via the white/black double render.
Result replaces the building entry in ArtSrc/baked/ (transparent background, object
pixels untouched — enforced by a silhouette diff that ignores the base zone where
the trim attaches).

Usage: python3 Tools/ArtPipeline/bake_building_full.py [name ...]   (default: all buildings)
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

from gemini_shadows import ASSETS, compose_canvas

OUT_DIR = os.path.join(ROOT, "ArtSrc", "baked")

SYSTEM = (
    "You edit 2D game sprites with pixel precision. The building in the first input image is a "
    "LOCKED layer: you must not redraw, resize, move, restyle or reinterpret it in any way — "
    "every existing pixel of the building stays exactly as it is, same width, same height, same "
    "position on the canvas. You may only paint NEW pixels onto the white background. Output "
    "the same canvas with only the requested additions.")
PROMPT_ADD = (
    "The first image is the locked building sprite. The second image is ONLY a style sample "
    "for the shadow and base trim look — do not copy its architecture, colors or anything "
    "else from it. "
    "Paint exactly two additions onto the white background of the first image: "
    "(1) a thin light cream stone base trim (plinth curb) hugging the bottom contour of the "
    "building where its walls meet the ground, styled and lit like in the sample; "
    "(2) one soft gray-blue cast shadow on the ground to the bottom-right of the building, "
    "same direction, softness, tone and proportional length as in the sample, starting at "
    "the base and touching the trim. "
    "The building itself must remain pixel-identical to the first image. Keep the pure white "
    "background. Nothing else may change.")


def silhouette_core_diff(canvas, result, alpha, origin):
    """Mean per-channel diff inside the eroded silhouette, ignoring its bottom band
    (the trim legitimately changes the base zone)."""
    m = np.asarray(alpha) > 200
    rows = np.where(m.any(axis=1))[0]
    if len(rows):
        cut = rows[0] + int((rows[-1] - rows[0]) * 0.8)
        m[cut:, :] = False
    m[:1, :] = m[-1:, :] = False
    core = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    full = np.zeros(canvas.size[::-1], dtype=bool)
    full[origin[1]:origin[1] + m.shape[0], origin[0]:origin[0] + m.shape[1]] = core
    if not full.any():
        return 0.0
    ca = np.asarray(canvas, dtype=np.int16)
    ra = np.asarray(result, dtype=np.int16)
    return float(np.abs(ca - ra).mean(axis=2)[full].mean())


def grown_outside_share(result, alpha, origin, ground, canvas_size):
    """Share of non-white pixels in the upper-left quadrant outside the dilated
    silhouette: catches the building being redrawn bigger/moved. The shadow falls to
    the bottom-RIGHT (and in iso climbs above the pivot line there) and the trim hugs
    the base contour, so neither may legally appear up-left of the building."""
    px, py = ground
    sil = np.zeros(canvas_size[::-1], dtype=bool)
    m = np.asarray(alpha) > 40
    sil[origin[1]:origin[1] + m.shape[0], origin[0]:origin[0] + m.shape[1]] = m
    dil = np.asarray(Image.fromarray((sil * 255).astype(np.uint8), "L")
                     .filter(ImageFilter.MaxFilter(9))) > 0
    ys = np.arange(canvas_size[1])[:, None]
    xs = np.arange(canvas_size[0])[None, :]
    zone = (~dil) & (ys < py - 10) & (xs < px)
    if not zone.any():
        return 0.0
    ra = np.asarray(result, dtype=np.int16)
    return float(((ra.min(axis=2) < 235) & zone).sum()) / float(zone.sum())


def style_reference():
    """An accepted bake composited on white — every building copies its shadow/trim
    style so the whole set stays consistent."""
    img = Image.open(os.path.join(OUT_DIR, "house_corner.png")).convert("RGBA")
    bg = Image.new("RGB", img.size, (255, 255, 255))
    bg.paste(img, (0, 0), img)
    return bg


PROMPT_ADD_SHORT = (
    "Add only: a soft gray-blue cast shadow on the ground to the bottom-right of this "
    "building, and a thin light cream stone plinth line along its base contour. "
    "Do not change the building. Do not touch anything else. Pure white background stays.")


def process(base, manifest, ref):
    rel = "Buildings/%s.png" % base
    entry = manifest["sprites"][rel]
    img = Image.open(os.path.join(ASSETS, rel)).convert("RGBA")
    canvas, pivot, origin = compose_canvas(img, entry["pivot"])

    if ref is None:
        white, _ = gi.generate(SYSTEM, PROMPT_ADD_SHORT, images=[canvas], aspect="1:1")
    else:
        white, _ = gi.generate(SYSTEM, PROMPT_ADD, images=[canvas, ref], aspect="1:1")
    white = white.convert("RGB").resize(canvas.size, Image.LANCZOS)
    diff = silhouette_core_diff(canvas, white, img.getchannel("A"), origin)
    if diff > 14.0:
        return None, "pass1 altered the building (diff %.1f)" % diff
    grown = grown_outside_share(white, img.getchannel("A"), origin, pivot, canvas.size)
    if grown > 0.004:
        white.save(os.path.join(OUT_DIR, "debug_%s.png" % base))
        return None, "pass1 redrew the building outside its silhouette (%.3f)" % grown

    # canonical white->black step from the ImageGen tool, then its alpha recovery
    black, _ = gi.generate(gi.SYSTEM_BLACK_BG, gi.BLACK_BG_INSTRUCTION, images=[white])
    black = black.convert("RGB").resize(canvas.size, Image.LANCZOS)
    diff2 = silhouette_core_diff(white, black, img.getchannel("A"), origin)
    if diff2 > 14.0:
        return None, "pass2 altered the building (diff %.1f)" % diff2

    rgba = gi.alpha_from_white_black(white, black)

    # a failed black render leaves an opaque background — reject, the caller retries
    a = np.asarray(rgba.getchannel("A"))
    border = np.concatenate([a[2, :], a[-3, :], a[:, 2], a[:, -3]])
    if float((border > 40).mean()) > 0.02:
        return None, "background not transparent after recovery"

    bbox = rgba.getbbox()
    if not bbox:
        return None, "empty result"
    rgba = rgba.crop(bbox)
    return (rgba, (pivot[0] - bbox[0], pivot[1] - bbox[1])), "ok"


def main():
    manifest = json.load(open(os.path.join(ASSETS, "art_manifest.json")))
    os.makedirs(OUT_DIR, exist_ok=True)
    meta_path = os.path.join(OUT_DIR, "meta.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {}

    only = set(sys.argv[1:])
    targets = [f[:-4] for f in sorted(os.listdir(os.path.join(ASSETS, "Buildings")))
               if f.endswith(".png") and (not only or f[:-4] in only)]
    ref = None if os.environ.get("BAKE_NO_REF") else style_reference()
    accepted, rejected = [], []
    for base in targets:
        result, why = None, ""
        for attempt in range(5):
            try:
                result, why = process(base, manifest, ref)
            except gi.GeminiError as e:
                result, why = None, "gemini: %s" % str(e)[:120]
            if result is not None:
                break
            print("retry  %-20s attempt %d: %s" % (base, attempt + 1, why), flush=True)
        if result is None:
            rejected.append(base)
            print("REJECT %-20s %s" % (base, why), flush=True)
            continue
        rgba, pivot = result
        rgba.save(os.path.join(OUT_DIR, base + ".png"))
        meta[base] = {"pivot": [int(pivot[0]), int(pivot[1])]}
        accepted.append(base)
        print("OK     %-20s pivot %s size %s" % (base, pivot, rgba.size), flush=True)

    json.dump(meta, open(meta_path, "w"), indent=1, sort_keys=True)
    print("done: %d accepted, %d rejected %s" % (len(accepted), len(rejected), rejected))


if __name__ == "__main__":
    main()
