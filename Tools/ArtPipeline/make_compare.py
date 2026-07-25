#!/usr/bin/env python3
"""Builds the iteration comparison image: the latest game screenshot stacked with the
reference concept.png, saved into View/iter_NNN.png (auto-numbered).
Usage: make_compare.py [screenshot_path] [label]"""
import os
import re
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VIEW = os.path.join(ROOT, "View")


def main():
    shot_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        ROOT, "Bin", "Mac", "TestScreenshots", "token_delivery_city.png")
    label = sys.argv[2] if len(sys.argv) > 2 else ""

    os.makedirs(VIEW, exist_ok=True)
    nums = [int(m.group(1)) for f in os.listdir(VIEW)
            if (m := re.match(r"iter_(\d+)", f))]
    idx = max(nums, default=0) + 1

    shot = Image.open(shot_path).convert("RGB")
    ref = Image.open(os.path.join(ROOT, "concept.png")).convert("RGB")

    width = 1600
    shot = shot.resize((width, int(shot.height * width / shot.width)), Image.LANCZOS)
    ref = ref.resize((width, int(ref.height * width / ref.width)), Image.LANCZOS)

    header = 36
    canvas = Image.new("RGB", (width, shot.height + ref.height + header * 2), (20, 20, 24))
    d = ImageDraw.Draw(canvas)
    d.text((12, 10), "GAME  iter %d  %s" % (idx, label), fill=(255, 255, 255))
    canvas.paste(shot, (0, header))
    d.text((12, header + shot.height + 10), "REFERENCE concept.png", fill=(255, 255, 255))
    canvas.paste(ref, (0, header * 2 + shot.height))

    out = os.path.join(VIEW, "iter_%03d.png" % idx)
    canvas.save(out)
    print(out)


if __name__ == "__main__":
    main()
