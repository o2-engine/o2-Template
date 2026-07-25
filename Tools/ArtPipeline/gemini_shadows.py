#!/usr/bin/env python3
"""Gemini-drawn ground shadows for buildings/props.

Two passes per sprite (Nano Banana 2 via o2/Tools/ImageGen):
  1) redraw the sprite on white WITH its cast shadow (prompt forbids touching the object);
  2) remove the object, keeping only the shadow on white.
Then the shadow is extracted programmatically to RGBA and stored in
ArtSrc/shadows_gemini/ with its pivot; build_all.py overrides the procedural
shadows with the accepted ones.

Usage: python3 Tools/ArtPipeline/gemini_shadows.py [name ...]   (default: all)
"""
import json
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "o2", "Tools", "ImageGen"))
import gemini_image as gi

ASSETS = os.path.join(ROOT, "Assets", "Game")
OUT_DIR = os.path.join(ROOT, "ArtSrc", "shadows_gemini")

SHADOW_COLOR = (35, 40, 60)

SYSTEM_ADD = (
    "You edit 2D game sprites with pixel precision. The object in the input image must be "
    "preserved EXACTLY: identical pixels, position, scale, colors and outline. You are only "
    "allowed to ADD to the white background, never to repaint or move the object.")
PROMPT_ADD = (
    "Add the cast shadow this object drops on the ground: one soft-edged neutral gray-blue "
    "shadow falling to the bottom-right (sun high in the top-left), starting exactly at the "
    "object's base and lying flat along the ground, cartoon mobile game style. Keep the pure "
    "white background. Do not change the object itself in any way, do not add anything else.")

SYSTEM_STRIP = (
    "You edit 2D game sprites with pixel precision. You only erase what you are told to erase, "
    "painting it pure white; everything else must stay EXACTLY as in the input image.")
PROMPT_STRIP = (
    "Erase the object completely, filling its area with pure white. Keep ONLY the cast shadow "
    "on the ground, exactly as it is: same shape, position, darkness and soft edges. The result "
    "is just the shadow on a pure white background.")


def list_targets():
    names = []
    for sub in ("Buildings", "Props"):
        for f in sorted(os.listdir(os.path.join(ASSETS, sub))):
            base = f[:-4]
            if f.endswith(".png") and base not in ("chip", "smoke"):
                names.append((sub, base))
    return names


def compose_canvas(img, pivot):
    """Sprite over white on a square canvas with room to the bottom-right for the shadow."""
    w, h = img.size
    pad = 40
    room = int(h * 0.55) + 60
    side = max(w + pad + room, h + pad * 2 + int(h * 0.2))
    canvas = Image.new("RGB", (side, side), (255, 255, 255))
    ox, oy = pad, side - pad - h - int(h * 0.08)
    canvas.paste(img, (ox, oy), img)
    return canvas, (ox + pivot[0], oy + pivot[1]), (ox, oy)


def silhouette_diff(canvas, result, alpha, origin):
    """Mean per-channel difference inside the (eroded) object silhouette."""
    a = np.zeros(canvas.size[::-1], dtype=bool)
    m = np.asarray(alpha) > 200
    m[:1, :] = m[-1:, :] = False
    core = m & np.roll(m, 1, 0) & np.roll(m, -1, 0) & np.roll(m, 1, 1) & np.roll(m, -1, 1)
    a[origin[1]:origin[1] + m.shape[0], origin[0]:origin[0] + m.shape[1]] = core
    if not a.any():
        return 0.0, a
    ca = np.asarray(canvas, dtype=np.int16)
    ra = np.asarray(result, dtype=np.int16)
    return float(np.abs(ca - ra).mean(axis=2)[a].mean()), a


def extract_shadow(strip_img):
    """Dark-on-white -> tinted RGBA."""
    arr = np.asarray(strip_img.convert("RGB"), dtype=np.float32)
    lum = arr.min(axis=2)
    a = np.clip((250.0 - lum) / 250.0, 0.0, 1.0)
    a[a < 0.05] = 0.0
    out = np.zeros((arr.shape[0], arr.shape[1], 4), dtype=np.uint8)
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

    strip, _ = gi.generate(SYSTEM_STRIP, PROMPT_STRIP, images=[with_shadow], aspect="1:1")
    strip = strip.convert("RGB").resize(canvas.size, Image.LANCZOS)
    lum_in, sil = silhouette_diff(canvas, strip, img.getchannel("A"), origin)
    interior = np.asarray(strip, dtype=np.float32).mean(axis=2)[sil]
    if interior.size and interior.mean() < 165.0:
        return None, "pass2 kept the object (interior lum %.0f)" % interior.mean()

    shadow = extract_shadow(strip)
    bbox = shadow.getbbox()
    if not bbox:
        return None, "empty shadow"
    shadow = shadow.crop(bbox)
    return (shadow, (pivot[0] - bbox[0], pivot[1] - bbox[1])), "ok"


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
        shadow, pivot = result
        shadow.save(os.path.join(OUT_DIR, base + ".png"))
        meta[base] = {"pivot": [int(pivot[0]), int(pivot[1])]}
        accepted.append(base)
        print("OK     %-20s pivot %s size %s" % (base, pivot, shadow.size), flush=True)

    json.dump(meta, open(meta_path, "w"), indent=1, sort_keys=True)
    json.dump(sorted(meta.keys()), open(os.path.join(OUT_DIR, "accepted.json"), "w"), indent=1)
    print("done: %d accepted, %d rejected %s" % (len(accepted), len(rejected), rejected))


if __name__ == "__main__":
    main()
