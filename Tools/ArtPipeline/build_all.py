#!/usr/bin/env python3
"""Builds the full art set into Assets/Game/. Uses generated source textures from
ArtSrc/ when present, procedural placeholders otherwise. Writes art_manifest.json
with pivots/footprints — the contract consumed by C++."""
import json
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from artgen import build_ground_tile, build_road_tiles
import placeholders as ph

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ART_SRC = os.path.join(ROOT, "ArtSrc")
OUT = os.path.join(ROOT, "Assets", "Game")

manifest = {"tileSize": [256, 128], "sprites": {}}


def src_texture(name, fallback):
    path = os.path.join(ART_SRC, name)
    if os.path.exists(path):
        return Image.open(path)
    return fallback()


def save_sprite(img, rel, pivot, footprint=None):
    path = os.path.join(OUT, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.save(path)
    entry = {"pivot": [int(pivot[0]), int(pivot[1])], "size": [img.width, img.height]}
    if footprint:
        entry["footprint"] = list(footprint)
    manifest["sprites"][rel.replace("\\", "/")] = entry


def rounded(size, radius, fill, outline=None, width=4):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, size[0] - 1, size[1] - 1], radius=radius, fill=fill,
                        outline=outline, width=width)
    return img


def build_tiles():
    build_road_tiles(src_texture("tex_asphalt.png", ph.asphalt_texture), os.path.join(OUT, "Tiles"))
    build_ground_tile(src_texture("tex_pavement.png", ph.pavement_texture),
                      os.path.join(OUT, "Tiles", "pavement.png"))
    build_ground_tile(src_texture("tex_grass.png", ph.grass_texture),
                      os.path.join(OUT, "Tiles", "grass.png"))
    for name in os.listdir(os.path.join(OUT, "Tiles")):
        manifest["sprites"]["Tiles/" + name] = {"pivot": [128, 64], "size": [256, 128]}


HOUSES = [
    # name, fw, fh, wall_h, wall, roof, floors, awning
    ("house_brick_a", 1, 1, 150, (201, 123, 90), (124, 144, 184), 2, None),
    ("house_brick_b", 1, 1, 170, (217, 144, 107), (156, 92, 74), 2, None),
    ("house_cream", 1, 1, 150, (239, 227, 206), (124, 144, 184), 2, None),
    ("house_terracotta", 1, 1, 165, (206, 130, 96), (110, 126, 160), 2, None),
    ("house_shop_awning", 1, 1, 150, (232, 216, 190), (150, 96, 66), 2, (214, 92, 74, 255)),
    ("house_cafe", 1, 1, 140, (226, 200, 168), (124, 144, 184), 2, (90, 132, 200, 255)),
    ("house_tall", 1, 1, 210, (196, 168, 140), (140, 110, 90), 3, None),
    ("house_double", 2, 1, 160, (210, 138, 100), (124, 144, 184), 2, None),
    ("house_long", 1, 2, 160, (238, 226, 204), (156, 92, 74), 2, (214, 92, 74, 255)),
    ("house_corner", 2, 2, 150, (203, 126, 92), (114, 132, 170), 2, None),
]

OFFICES = [
    ("office_glass", 2, 2, 220, (150, 176, 210), (96, 116, 150), 3),
    ("office_loft", 2, 1, 190, (172, 120, 94), (110, 100, 92), 3),
    ("office_classic", 1, 1, 200, (222, 210, 186), (110, 126, 160), 3),
]


def build_buildings():
    for name, fw, fh, wh, wall, roof, floors, awn in HOUSES:
        img, pivot = ph.building_sprite(fw, fh, wh, wall, roof, floors, awn)
        save_sprite(img, os.path.join("Buildings", name + ".png"), pivot, (fw, fh))
    for name, fw, fh, wh, wall, roof, floors in OFFICES:
        img, pivot = ph.building_sprite(fw, fh, wh, wall, roof, floors, None, chip_logo=True)
        save_sprite(img, os.path.join("Buildings", name + ".png"), pivot, (fw, fh))


def build_props():
    img, pivot = ph.tree_sprite(1.0)
    save_sprite(img, "Props/tree_big.png", pivot)
    img, pivot = ph.tree_sprite(0.7)
    save_sprite(img, "Props/tree_small.png", pivot)
    img, pivot = ph.kiosk_sprite()
    save_sprite(img, "Props/kiosk.png", pivot, (1, 1))
    img, pivot = ph.fountain_sprite()
    save_sprite(img, "Props/fountain.png", pivot, (1, 1))
    chip = ph.chip_icon(96)
    save_sprite(chip, "Props/chip.png", (48, 48))

    # simple bench and lamp
    bench = Image.new("RGBA", (110, 70), (0, 0, 0, 0))
    d = ImageDraw.Draw(bench)
    d.polygon([(10, 40), (70, 25), (100, 40), (40, 58)], fill=(158, 110, 74, 255))
    d.rectangle([18, 52, 26, 66], fill=(96, 70, 50, 255))
    d.rectangle([80, 40, 88, 56], fill=(96, 70, 50, 255))
    save_sprite(bench, "Props/bench.png", (55, 58))

    lamp = Image.new("RGBA", (44, 150), (0, 0, 0, 0))
    d = ImageDraw.Draw(lamp)
    d.rectangle([19, 20, 25, 140], fill=(70, 74, 82, 255))
    d.ellipse([12, 4, 32, 26], fill=(255, 232, 160, 255), outline=(70, 74, 82, 255), width=3)
    save_sprite(lamp, "Props/lamp.png", (22, 140))


def build_ui():
    save_sprite(rounded((256, 96), 44, (255, 255, 255, 235), (208, 214, 224, 255)),
                "UI/pill.png", (128, 48))
    save_sprite(rounded((256, 128), 24, (44, 52, 66, 225)), "UI/panel_dark.png", (128, 64))
    save_sprite(rounded((192, 88), 36, (255, 255, 255, 240), (206, 212, 222, 255)),
                "UI/bubble.png", (96, 44))
    save_sprite(rounded((320, 64), 28, (40, 46, 58, 235), (24, 28, 36, 255)),
                "UI/fuel_bg.png", (160, 32))
    fill = Image.new("RGBA", (300, 44), (0, 0, 0, 0))
    d = ImageDraw.Draw(fill)
    for x in range(300):
        t = x / 299.0
        col = (int(232 + (250 - 232) * t), int(90 + (208 - 90) * t), int(40 + (60 - 40) * t), 255)
        d.line([(x, 0), (x, 43)], fill=col)
    mask = rounded((300, 44), 20, (255, 255, 255, 255))
    fill.putalpha(mask.split()[3])
    save_sprite(fill, "UI/fuel_fill.png", (0, 22))  # pivot on the left edge for FillLeftToRight

    pump = Image.new("RGBA", (72, 72), (0, 0, 0, 0))
    d = ImageDraw.Draw(pump)
    d.rounded_rectangle([14, 10, 50, 62], radius=8, fill=(214, 74, 60, 255))
    d.rounded_rectangle([21, 18, 43, 34], radius=4, fill=(240, 238, 230, 255))
    d.line([(50, 24), (62, 30), (62, 52)], fill=(214, 74, 60, 255), width=6)
    save_sprite(pump, "UI/fuel_icon.png", (36, 36))

    boost = rounded((160, 160), 46, (74, 154, 240, 255), (255, 255, 255, 255), 6)
    d = ImageDraw.Draw(boost)
    d.polygon([(80, 28), (108, 96), (80, 82), (52, 96)], fill=(255, 255, 255, 255))
    d.polygon([(66, 100), (94, 100), (80, 130)], fill=(255, 214, 92, 255))
    save_sprite(boost, "UI/boost_btn.png", (80, 80))

    gear = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    d = ImageDraw.Draw(gear)
    d.ellipse([8, 8, 88, 88], fill=(74, 154, 240, 255), outline=(255, 255, 255, 255), width=5)
    d.ellipse([30, 30, 66, 66], fill=(255, 255, 255, 255))
    d.ellipse([40, 40, 56, 56], fill=(74, 154, 240, 255))
    save_sprite(gear, "UI/gear.png", (48, 48))

    check = Image.new("RGBA", (96, 96), (0, 0, 0, 0))
    d = ImageDraw.Draw(check)
    d.ellipse([4, 4, 92, 92], fill=(96, 196, 96, 255), outline=(255, 255, 255, 255), width=5)
    d.line([(26, 50), (44, 68), (72, 32)], fill=(255, 255, 255, 255), width=12, joint="curve")
    save_sprite(check, "UI/check.png", (48, 48))

    arrow = Image.new("RGBA", (120, 120), (0, 0, 0, 0))
    d = ImageDraw.Draw(arrow)
    d.rounded_rectangle([4, 4, 116, 116], radius=28, fill=(255, 255, 255, 200),
                        outline=(160, 170, 185, 255), width=4)
    d.polygon([(60, 26), (94, 74), (60, 60), (26, 74)], fill=(74, 110, 160, 255))
    save_sprite(arrow, "UI/arrow.png", (60, 60))

    btn = rounded((360, 110), 40, (96, 196, 96, 255), (255, 255, 255, 255), 6)
    save_sprite(btn, "UI/button_green.png", (180, 55))
    btn2 = rounded((360, 110), 40, (74, 154, 240, 255), (255, 255, 255, 255), 6)
    save_sprite(btn2, "UI/button_blue.png", (180, 55))
    save_sprite(rounded((760, 520), 48, (250, 248, 242, 250), (200, 206, 218, 255), 6),
                "UI/window.png", (380, 260))


def build_effects():
    smoke = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d = ImageDraw.Draw(smoke)
    for r, a in ((30, 60), (24, 110), (17, 170)):
        d.ellipse([32 - r, 32 - r, 32 + r, 32 + r], fill=(235, 235, 232, a))
    save_sprite(smoke, "Props/smoke.png", (32, 32))


def emit_cpp_manifest():
    path = os.path.join(ROOT, "Sources", "Game", "TokenDelivery", "ArtManifest.g.h")
    lines = ["#pragma once", "",
             "// Generated by Tools/ArtPipeline/build_all.py - do not edit by hand.",
             "// Pivots are in image pixels from the top-left corner.",
             "namespace td::art", "{",
             "\tstruct SpriteMeta { const char* path; int px; int py; int w; int h; int fw; int fh; };",
             "\tinline const SpriteMeta kSprites[] = {"]
    for rel in sorted(manifest["sprites"]):
        e = manifest["sprites"][rel]
        fw, fh = e.get("footprint", [0, 0])
        lines.append('\t\t{ "Game/%s", %d, %d, %d, %d, %d, %d },'
                     % (rel, e["pivot"][0], e["pivot"][1], e["size"][0], e["size"][1], fw, fh))
    lines += ["\t};", "}", ""]
    with open(path, "w") as f:
        f.write("\n".join(lines))


def build_backdrop():
    img = ph.backdrop(2048)
    path = os.path.join(OUT, "backdrop.png")
    os.makedirs(OUT, exist_ok=True)
    img.save(path)
    manifest["sprites"]["backdrop.png"] = {"pivot": [1024, 1024], "size": [2048, 2048]}


def main():
    build_tiles()
    build_buildings()
    build_props()
    build_ui()
    build_effects()
    build_backdrop()
    with open(os.path.join(OUT, "art_manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1, sort_keys=True)
    emit_cpp_manifest()
    print("done:", len(manifest["sprites"]), "sprites ->", OUT)


if __name__ == "__main__":
    main()
