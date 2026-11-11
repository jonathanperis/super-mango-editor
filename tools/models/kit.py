"""Blender helpers for building Super Mango's 3D models.

Run through Blender, never plain Python:

    blender --background --python tools/models/build.py

A model is a handful of smooth shapes, each with one flat colour (or a
top-to-bottom blend). Blender then *bakes* the lighting: it works out how
much light reaches every vertex from a sun and a soft sky, including the
shading where shapes meet, and stores the result as that vertex's colour.
The game draws those colours as they are, so it needs no lighting code and
the models look the same on every platform.

Modelling units are tiles: 1.0 here is 16 game pixels. Blender's axes are
x right, y into the screen, z up; the exporter turns them into the game's
x right, y up, z toward the viewer.
"""
import math
import struct

import bmesh
import bpy
from mathutils import Vector

PIXELS_PER_UNIT = 16.0


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def linear_to_srgb(c):
    c = max(0.0, min(1.0, c))
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055


def rgb(r, g, b):
    """An sRGB colour (0-255 per channel) as Blender's linear RGBA."""
    return (srgb_to_linear(r / 255), srgb_to_linear(g / 255), srgb_to_linear(b / 255), 1.0)


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 96
    scene.cycles.use_denoising = False
    scene.view_settings.view_transform = "Standard"
    return scene


def light_rig():
    """The one light every model is baked under: a warm sun from the upper
    left front, and a pale sky that fills the shadows."""
    scene = bpy.context.scene
    world = bpy.data.worlds.new("Sky")
    world.use_nodes = True
    background = world.node_tree.nodes["Background"]
    background.inputs["Color"].default_value = rgb(214, 228, 255)
    background.inputs["Strength"].default_value = 0.85
    scene.world = world
    sun = bpy.data.lights.new("Sun", "SUN")
    sun.energy = 3.4
    sun.angle = math.radians(18)   # a wide sun: soft-edged shadows
    sun.color = (1.0, 0.95, 0.86)
    holder = bpy.data.objects.new("Sun", sun)
    scene.collection.objects.link(holder)
    # Aim it: from above, a little left, and from the viewer's side.
    direction = Vector((0.35, 0.55, -1.0)).normalized()
    holder.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    return holder


def material(name, color, blend_to=None, z0=0.0, z1=1.0):
    """A matte material. With blend_to, the colour runs from `color` at
    height z1 down to `blend_to` at height z0 (object space)."""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    tree = mat.node_tree
    shader = tree.nodes["Principled BSDF"]
    shader.inputs["Roughness"].default_value = 1.0
    for socket in ("Specular IOR Level", "Specular"):
        if socket in shader.inputs:
            shader.inputs[socket].default_value = 0.0
    if blend_to is None:
        shader.inputs["Base Color"].default_value = color
        return mat
    coords = tree.nodes.new("ShaderNodeTexCoord")
    split = tree.nodes.new("ShaderNodeSeparateXYZ")
    span = tree.nodes.new("ShaderNodeMapRange")
    span.inputs["From Min"].default_value = z0
    span.inputs["From Max"].default_value = z1
    ramp = tree.nodes.new("ShaderNodeMix")
    ramp.data_type = "RGBA"
    ramp.inputs["A"].default_value = blend_to
    ramp.inputs["B"].default_value = color
    tree.links.new(coords.outputs["Object"], split.inputs["Vector"])
    tree.links.new(split.outputs["Z"], span.inputs["Value"])
    tree.links.new(span.outputs["Result"], ramp.inputs["Factor"])
    tree.links.new(ramp.outputs["Result"], shader.inputs["Base Color"])
    return mat


def _finish(obj, mat, smooth=True):
    obj.data.materials.append(mat)
    if smooth:
        for polygon in obj.data.polygons:
            polygon.use_smooth = True
    return obj


def ball(name, at, radius, mat, scale=(1, 1, 1), segments=24, rings=14):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings,
                                         radius=radius, location=at)
    obj = bpy.context.object
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return _finish(obj, mat)


def block(name, lo, hi, mat, bevel=0.1, segments=4):
    """A box from corner lo to corner hi with rounded edges."""
    lo, hi = Vector(lo), Vector(hi)
    bpy.ops.mesh.primitive_cube_add(size=1, location=(lo + hi) / 2)
    obj = bpy.context.object
    obj.name = name
    obj.scale = hi - lo
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel > 0:
        modifier = obj.modifiers.new("Round", "BEVEL")
        modifier.width = bevel
        modifier.segments = segments
        modifier.limit_method = "ANGLE"
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return _finish(obj, mat)


def disc(name, at, radius, depth, mat, bevel=0.05, vertices=40):
    """A cylinder whose flat faces look along y (toward the viewer)."""
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth,
                                        location=at, rotation=(math.radians(90), 0, 0))
    obj = bpy.context.object
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    if bevel > 0:
        modifier = obj.modifiers.new("Round", "BEVEL")
        modifier.width = bevel
        modifier.segments = 3
        modifier.limit_method = "ANGLE"
        bpy.ops.object.modifier_apply(modifier=modifier.name)
    return _finish(obj, mat)


def tube(name, points, radius, mat, resolution=6):
    """A round tube through the given points (a limb, a stem)."""
    curve = bpy.data.curves.new(name, "CURVE")
    curve.dimensions = "3D"
    curve.bevel_depth = radius
    curve.bevel_resolution = 3
    curve.use_fill_caps = True
    spline = curve.splines.new("BEZIER")
    spline.bezier_points.add(len(points) - 1)
    for point, at in zip(spline.bezier_points, points):
        point.co = at
        point.handle_left_type = point.handle_right_type = "AUTO"
    spline.resolution_u = resolution
    obj = bpy.data.objects.new(name, curve)
    bpy.context.scene.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.ops.object.convert(target="MESH")
    return _finish(bpy.context.object, mat)


def squeeze(obj, fn):
    """Move every vertex with fn(Vector) -> Vector, to shape a primitive."""
    for vertex in obj.data.vertices:
        vertex.co = fn(vertex.co.copy())
    obj.data.update()


def subdivide(obj, levels=1):
    modifier = obj.modifiers.new("Smooth", "SUBSURF")
    modifier.levels = modifier.render_levels = levels
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def bake(objects):
    """Store each object's lit colour in a per-vertex colour attribute."""
    scene = bpy.context.scene
    scene.render.bake.target = "VERTEX_COLORS"
    for obj in objects:
        mesh = obj.data
        attribute = mesh.color_attributes.new("Lit", "FLOAT_COLOR", "POINT")
        mesh.color_attributes.active_color = attribute
        mesh.color_attributes.render_color_index = mesh.color_attributes.find("Lit")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.bake(type="COMBINED")


def join(objects, name):
    """Merge baked objects into one, keeping their baked colours."""
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    merged = bpy.context.object
    merged.name = name
    # Vertex positions become scene positions, which is what cut() measures.
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    return merged


def cut(obj, name, x=None, y=None, z=None, origin=(0.0, 0.0, 0.0)):
    """A copy of obj keeping only what lies inside the given (low, high)
    range on each named axis, moved so that `origin` becomes (0, 0, 0).
    The cuts are left open: pieces are meant to sit side by side, like
    tiles."""
    copy = obj.copy()
    copy.data = obj.data.copy()
    copy.name = name
    bpy.context.scene.collection.objects.link(copy)
    mesh = bmesh.new()
    mesh.from_mesh(copy.data)
    for axis, span in enumerate((x, y, z)):
        if span is None:
            continue
        normal = [0.0, 0.0, 0.0]
        normal[axis] = 1.0
        for at, drop_below in ((span[0], True), (span[1], False)):
            point = [0.0, 0.0, 0.0]
            point[axis] = at
            geometry = mesh.verts[:] + mesh.edges[:] + mesh.faces[:]
            bmesh.ops.bisect_plane(mesh, geom=geometry, plane_co=point, plane_no=normal,
                                   clear_inner=drop_below, clear_outer=not drop_below)
    for vertex in mesh.verts:
        vertex.co -= Vector(origin)
    mesh.to_mesh(copy.data)
    mesh.free()
    return copy


def mirrored_y(obj, name):
    """A copy of obj flipped front to back, colours included."""
    copy = obj.copy()
    copy.data = obj.data.copy()
    copy.name = name
    bpy.context.scene.collection.objects.link(copy)
    for vertex in copy.data.vertices:
        vertex.co.y = -vertex.co.y
    copy.data.update()
    return copy


def export(objects, path, origin=(0.0, 0.0, 0.0)):
    """Write the objects as one .smm file (format: src/shared/model.h)."""
    vertices, indices = [], []
    ox, oy, oz = origin
    for obj in objects:
        mesh = obj.data
        mesh.calc_loop_triangles()
        colors = mesh.color_attributes["Lit"].data
        base = len(vertices)
        world = obj.matrix_world
        for vertex, color in zip(mesh.vertices, colors):
            at = world @ vertex.co
            r, g, b = (int(round(linear_to_srgb(c) * 255)) for c in color.color[:3])
            vertices.append(((at.x - ox) * PIXELS_PER_UNIT, (at.z - oz) * PIXELS_PER_UNIT,
                             -(at.y - oy) * PIXELS_PER_UNIT, r, g, b))
        for triangle in mesh.loop_triangles:
            indices.extend(base + index for index in triangle.vertices)
    if len(vertices) > 65535:
        raise SystemExit(f"{path}: {len(vertices)} vertices; the format holds 65535")
    with open(path, "wb") as out:
        out.write(struct.pack("<4sII", b"SMM1", len(vertices), len(indices)))
        for x, y, z, r, g, b in vertices:
            out.write(struct.pack("<fffBBBB", x, y, z, r, g, b, 255))
        out.write(struct.pack(f"<{len(indices)}H", *indices))
    print(f"model: {path}: {len(vertices)} vertices, {len(indices) // 3} triangles")


def preview(objects, path, width=640, height=480, yaw=28.0):
    """Render the baked colours exactly as the game will draw them."""
    scene = bpy.context.scene
    flat = bpy.data.materials.new("Preview")
    flat.use_nodes = True
    tree = flat.node_tree
    tree.nodes.clear()
    attribute = tree.nodes.new("ShaderNodeVertexColor")
    attribute.layer_name = "Lit"
    emit = tree.nodes.new("ShaderNodeEmission")
    output = tree.nodes.new("ShaderNodeOutputMaterial")
    tree.links.new(attribute.outputs["Color"], emit.inputs["Color"])
    tree.links.new(emit.outputs["Emission"], output.inputs["Surface"])
    visible = set(objects)
    for obj in scene.objects:
        if obj.type == "MESH":
            obj.hide_render = obj not in visible
    for obj in objects:
        obj.data.materials.clear()
        obj.data.materials.append(flat)
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    for obj in objects:
        for corner in obj.bound_box:
            at = obj.matrix_world @ Vector(corner)
            lo = Vector(map(min, lo, at))
            hi = Vector(map(max, hi, at))
    centre = (lo + hi) / 2
    size = max(hi.x - lo.x, hi.z - lo.z, 0.5)
    camera_data = bpy.data.cameras.new("Preview")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = size * 1.5
    camera = bpy.data.objects.new("Preview", camera_data)
    scene.collection.objects.link(camera)
    angle = math.radians(yaw)
    offset = Vector((math.sin(angle) * 20, -math.cos(angle) * 20, 6))
    camera.location = centre + offset
    camera.rotation_euler = (-offset).to_track_quat("-Z", "Y").to_euler()
    scene.camera = camera
    scene.world.node_tree.nodes["Background"].inputs["Color"].default_value = rgb(120, 190, 240)
    scene.render.resolution_x, scene.render.resolution_y = width, height
    scene.cycles.samples = 8
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
