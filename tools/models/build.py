"""Build every 3D model the game ships, from scratch.

    blender --background --python tools/models/build.py -- [--preview DIR] [name ...]

Each model is described below in code, lit and baked by tools/models/kit.py,
and written to assets/models/<name>.smm. With --preview, a picture of each
finished model is also written to DIR. Naming models builds only those.
"""
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kit  # noqa: E402  (needs the path line above)

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "assets", "models")

GRASS = kit.rgb(112, 204, 72)


def ground(color=GRASS):
    """Something to stand on while baking: it shades the feet and bounces
    its colour up, as the level's ground will. Never exported."""
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
    plane = bpy.context.object
    plane.data.materials.append(kit.material("Ground", color))
    return plane


def mango():
    """The player: a round mango with a face, a stem and a leaf. Faces the
    viewer; the game turns it toward where it walks. Feet are a model of
    their own so the game can move them."""
    skin = kit.material("Skin", kit.rgb(234, 62, 46), blend_to=kit.rgb(255, 176, 58),
                        z0=-0.62, z1=0.45)
    body = kit.ball("Body", (0, 0, 0.86), 0.72, skin, scale=(1.0, 0.92, 0.98), segments=32, rings=20)
    # Wider at the bottom than the top, like the fruit.
    kit.squeeze(body, lambda v: v.__class__((v.x * (1 - 0.14 * v.z), v.y * (1 - 0.14 * v.z), v.z)))
    white = kit.material("EyeWhite", kit.rgb(255, 255, 255))
    black = kit.material("Pupil", kit.rgb(30, 22, 32))
    arm = kit.material("Arm", kit.rgb(214, 52, 42))
    parts = [body]
    for side in (-1, 1):
        parts.append(kit.ball("Eye", (side * 0.25, -0.575, 0.99), 0.2, white, scale=(0.82, 0.5, 1.12)))
        parts.append(kit.ball("Pupil", (side * 0.26, -0.655, 0.98), 0.11, black, scale=(0.8, 0.5, 1.12),
                              segments=16, rings=10))
        parts.append(kit.ball("Glint", (side * 0.26 - 0.035, -0.705, 1.045), 0.035, white,
                              segments=10, rings=6))
        parts.append(kit.ball("Arm", (side * 0.74, -0.04, 0.70), 0.16, arm, scale=(0.8, 0.9, 1.15),
                              segments=16, rings=10))
    parts.append(kit.tube("Mouth", [(-0.11, -0.672, 0.735), (0, -0.69, 0.69), (0.11, -0.672, 0.735)],
                          0.026, kit.material("Mouth", kit.rgb(120, 26, 30))))
    parts.append(kit.tube("Stem", [(0, 0, 1.5), (0.02, 0, 1.64), (0.07, 0, 1.76)], 0.055,
                          kit.material("Stem", kit.rgb(122, 82, 46))))
    leaf = kit.ball("Leaf", (0.33, 0, 1.78), 0.3, kit.material("Leaf", kit.rgb(96, 196, 72)),
                    scale=(1, 0.3, 0.42), segments=18, rings=10)
    kit.squeeze(leaf, lambda v: v.__class__((v.x, v.y, v.z + 0.45 * v.x - 0.5 * v.x * v.x)))
    parts.append(leaf)

    shoe = kit.material("Shoe", kit.rgb(126, 40, 44))
    feet = [kit.ball("Foot", (side * 0.3, -0.1, 0.13), 0.22, shoe, scale=(1.0, 1.35, 0.6),
                     segments=18, rings=10) for side in (-1, 1)]
    return {"mango": (parts, (0, 0, 0)), "mango_foot": ([feet[1]], (0.3, 0, 0))}, parts + feet


def spider():
    """A low, wide spider: dark body, bright legs, big eyes."""
    hide = kit.material("Hide", kit.rgb(92, 78, 128), blend_to=kit.rgb(52, 44, 78), z0=-0.25, z1=0.25)
    leg = kit.material("Leg", kit.rgb(246, 150, 48))
    white = kit.material("EyeWhite", kit.rgb(255, 255, 255))
    black = kit.material("Pupil", kit.rgb(24, 18, 28))
    parts = [kit.ball("Body", (0, 0, 0.36), 0.36, hide, scale=(1.2, 1.05, 0.84), segments=28, rings=16)]
    for side in (-1, 1):
        parts.append(kit.ball("Eye", (side * 0.15, -0.325, 0.44), 0.115, white, scale=(0.9, 0.55, 1.1),
                              segments=16, rings=10))
        parts.append(kit.ball("Pupil", (side * 0.155, -0.372, 0.435), 0.06, black, scale=(0.85, 0.5, 1.1),
                              segments=12, rings=8))
        for along in (-0.2, 0.04, 0.26):
            parts.append(kit.tube("Leg", [(side * 0.28, along, 0.34), (side * 0.56, along * 1.25, 0.56),
                                          (side * 0.78, along * 1.5, 0.03)], 0.05, leg))
    return {"spider": (parts, (0, 0, 0))}, parts


def coin():
    """A thick gold coin with a raised middle. Only the front half is lit
    and baked; the back is its mirror image, so the coin looks the same
    from both sides as it spins."""
    gold = kit.material("Gold", kit.rgb(255, 204, 44))
    pale = kit.material("PaleGold", kit.rgb(255, 232, 110))
    parts = [kit.disc("Rim", (0, 0, 0), 0.44, 0.16, gold, bevel=0.05),
             kit.disc("Face", (0, 0, 0), 0.29, 0.22, pale, bevel=0.04)]
    return {"coin": lambda: [kit.cut(obj, obj.name + "Front", y=(-1, 0)) for obj in parts],
            "_mirror": True}, parts


def _lip(parts, mat, count, y, z, radius=0.3):
    """Rounded tufts along the front edge of a grass cap, one per tile."""
    for i in range(count):
        parts.append(kit.ball("Tuft", (i + 0.5, y, z), radius, mat, scale=(1.0, 0.5, 0.72),
                              segments=12, rings=6))


def grass_floor():
    """The ground: a deep bank of earth under a rounded cap of grass. Built
    five tiles long, then cut into a left end, a middle tile that repeats,
    and a right end. The top of the grass is height 0."""
    grass = kit.material("Grass", kit.rgb(118, 208, 72), blend_to=kit.rgb(70, 160, 60), z0=-0.3, z1=0.2)
    earth = kit.material("Earth", kit.rgb(190, 122, 74), blend_to=kit.rgb(128, 76, 52), z0=-1.6, z1=1.6)
    stone = kit.material("Stone", kit.rgb(150, 96, 64))
    parts = [kit.block("Earth", (0.07, -0.9, -4.0), (4.93, 2.8, -0.3), earth, bevel=0.22),
             kit.block("Grass", (-0.06, -1.04, -0.52), (5.06, 2.9, 0.0), grass, bevel=0.2)]
    _lip(parts, grass, 5, -1.0, -0.47)
    # Flat stones set into the earth: the same two in every tile, so the
    # middle piece repeats without a seam.
    for i in range(5):
        parts.append(kit.ball("Stone", (i + 0.3, -0.9, -1.25), 0.2, stone, scale=(1.25, 0.28, 0.8),
                              segments=10, rings=6))
        parts.append(kit.ball("Stone", (i + 0.74, -0.9, -2.2), 0.15, stone, scale=(1.3, 0.28, 0.85),
                              segments=10, rings=6))

    def pieces():
        whole = kit.join(parts, "Floor")
        return {"grass_floor_left": [kit.cut(whole, "L", x=(-1, 1))],
                "grass_floor_mid": [kit.cut(whole, "M", x=(2, 3), origin=(2, 0, 0))],
                "grass_floor_right": [kit.cut(whole, "R", x=(4, 6), origin=(4, 0, 0))]}
    return {"_pieces": pieces}, parts


def grass_pillar():
    """A pillar to stand on: a grass cap that juts toward the viewer over a
    trunk of earth set further back, so the player can walk in front of the
    trunk and stand on the cap. Cut into left, middle and right columns,
    each as a top piece and a trunk tile that repeats downward."""
    grass = kit.material("Grass", kit.rgb(118, 208, 72), blend_to=kit.rgb(70, 160, 60), z0=-0.3, z1=0.2)
    earth = kit.material("Earth", kit.rgb(176, 112, 70), blend_to=kit.rgb(128, 76, 52), z0=-3.0, z1=3.0)
    parts = [kit.block("Trunk", (0.2, 0.75, -6.0), (4.8, 2.75, -0.3), earth, bevel=0.3),
             kit.block("Cap", (-0.1, -0.9, -0.56), (5.1, 2.9, 0.0), grass, bevel=0.22)]
    _lip(parts, grass, 5, -0.86, -0.5)

    def pieces():
        whole = kit.join(parts, "Pillar")
        out = {}
        for column, span, x0 in (("left", (-1, 1), 0), ("mid", (2, 3), 2), ("right", (4, 6), 4)):
            out[f"grass_pillar_top_{column}"] = [kit.cut(whole, "T", x=span, z=(-1, 1), origin=(x0, 0, 0))]
            out[f"grass_pillar_body_{column}"] = [kit.cut(whole, "B", x=span, z=(-3, -2), origin=(x0, 0, -2))]
        return out
    return {"_pieces": pieces}, parts


def float_platform():
    """A hovering slab: pale stone under a red top. One tile high, cut into
    left, middle and right pieces."""
    slab = kit.material("Slab", kit.rgb(226, 228, 238), blend_to=kit.rgb(150, 156, 180), z0=-0.4, z1=0.4)
    top = kit.material("Top", kit.rgb(232, 78, 72))
    parts = [kit.block("Slab", (0.03, -0.82, -0.96), (4.97, 0.82, -0.16), slab, bevel=0.3),
             kit.block("Top", (-0.03, -0.88, -0.34), (5.03, 0.88, 0.0), top, bevel=0.15)]

    def pieces():
        whole = kit.join(parts, "Platform")
        return {"float_platform_left": [kit.cut(whole, "L", x=(-1, 1))],
                "float_platform_mid": [kit.cut(whole, "M", x=(2, 3), origin=(2, 0, 0))],
                "float_platform_right": [kit.cut(whole, "R", x=(4, 6), origin=(4, 0, 0))]}
    return {"_pieces": pieces}, parts


# name -> (builder, stands on the ground while baking)
BUILDERS = {"mango": (mango, True), "spider": (spider, True), "coin": (coin, False),
            "grass_floor": (grass_floor, False), "grass_pillar": (grass_pillar, False),
            "float_platform": (float_platform, False)}



def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    preview_dir = None
    if "--preview" in args:
        at = args.index("--preview")
        preview_dir = args[at + 1]
        del args[at:at + 2]
    names = args or list(BUILDERS)
    os.makedirs(OUT, exist_ok=True)
    for name in names:
        builder, on_ground = BUILDERS[name]
        kit.reset_scene()
        kit.light_rig()
        if on_ground:
            ground()
        outputs, everything = builder()
        kit.bake(everything)
        shown = list(everything)
        if "_pieces" in outputs:
            # Tiles: bake the long shape whole, then cut it up, so each piece
            # is shaded as if its neighbours were there.
            outputs = {key: (objects, (0, 0, 0)) for key, objects in outputs["_pieces"]().items()}
            shown = [obj for objects, _ in outputs.values() for obj in objects]
        elif outputs.pop("_mirror", False):
            (file_name, make), = outputs.items()
            front = make()
            back = [kit.mirrored_y(obj, obj.name + "Back") for obj in front]
            outputs = {file_name: (front + back, (0, 0, 0))}
            shown = front + back
        for file_name, (objects, origin) in outputs.items():
            kit.export(objects, os.path.join(OUT, file_name + ".smm"), origin)
        if preview_dir:
            os.makedirs(preview_dir, exist_ok=True)
            if name.startswith(("grass", "float")):
                # Show the pieces pulled apart, in the order they are named.
                for i, obj in enumerate(shown):
                    obj.location.x += i * 2.4
            kit.preview(shown, os.path.join(preview_dir, name + ".png"))

main()
