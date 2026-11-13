# SPDX-License-Identifier: MIT
"""Cycles preview renders: the visual evidence for an asset (docs/ASSET_PIPELINE.md)."""
import math
import os

import bpy
import numpy as np
from mathutils import Vector


def _look_at(cam, target):
    d = Vector(target) - cam.location
    cam.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()


def setup_stage(scene, size_m):
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 24
    scene.cycles.use_denoising = False
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 6
    scene.view_settings.view_transform = 'Standard'
    world = bpy.data.worlds.new("w")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.74, 0.92, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.9
    scene.world = world
    sun = bpy.data.lights.new("sun", 'SUN')
    sun.energy = 3.2
    sun.angle = math.radians(3.0)
    so = bpy.data.objects.new("sun", sun)
    so.rotation_euler = (math.radians(52), math.radians(8), math.radians(-35))
    scene.collection.objects.link(so)
    bpy.ops.mesh.primitive_plane_add(size=size_m * 30.0)
    plane = bpy.context.active_object
    mat = bpy.data.materials.new("ground")
    mat.use_nodes = True
    mat.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.42, 0.45, 0.33, 1)
    mat.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 1.0
    plane.data.materials.append(mat)
    cam_data = bpy.data.cameras.new("cam")
    cam_data.lens = 70
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam, plane


VIEWS = {            # direction from the target towards the camera (Blender space), unit-ish
    "side": (1.0, 0.0, 0.12),
    "front34": (0.62, -0.78, 0.22),
    "front": (0.0, -1.0, 0.10),
    "rear34": (-0.7, 0.7, 0.30),
    "top": (0.05, -0.02, 1.0),
}


def render_tile(scene, cam, center_b, radius, view, res, path, distance=4.6):
    """Render one view; returns the pixels (rows bottom-up, RGBA float)."""
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.image_settings.file_format = 'PNG'
    d = Vector(VIEWS[view]).normalized()
    cam.location = Vector(center_b) + d * (radius * (distance if view != "top" else distance + 0.6))
    _look_at(cam, center_b)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(res[1], res[0], 4)
    bpy.data.images.remove(img)
    os.remove(path)
    return px


def save_sheet(tiles, cols, res, path):
    """Stitch equally sized tiles (row-major, first tile top-left) into one PNG."""
    rows = (len(tiles) + cols - 1) // cols
    sheet = np.zeros((rows * res[1], cols * res[0], 4), dtype=np.float32)
    sheet[..., 3] = 1.0
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        y0 = (rows - 1 - r) * res[1]
        sheet[y0:y0 + res[1], c * res[0]:(c + 1) * res[0]] = t
    out = bpy.data.images.new("sheet", cols * res[0], rows * res[1], alpha=True)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    bpy.data.images.remove(out)
    return path


def render_views(scene, cam, center_b, radius, out_prefix, views=("side", "front34", "front", "rear34"),
                 res=(640, 480)):
    """Render each view and stitch them into one contact sheet PNG."""
    tiles = [render_tile(scene, cam, center_b, radius, v, res, f"{out_prefix}_{v}.png") for v in views]
    return save_sheet(tiles, 2 if len(tiles) > 1 else 1, res, out_prefix + ".png")


def sculpt_lighting(scene):
    """Opt-in daylight for sculpt review; the legacy species' stage stays unchanged."""
    scene.render.threads = 2
    scene.cycles.samples = 12
    scene.view_settings.view_transform = 'AgX'
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
    bpy.data.lights['sun'].energy = 2.4
    bpy.data.lights['sun'].angle = math.radians(8)


def render_eye_level(scene, cam, height, prefix):
    """Visitor camera: four metres away, 1.6 m high; plus a close anatomical side view."""
    cam.data.type = 'PERSP'
    cam.data.lens = 48
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.image_settings.file_format = 'PNG'
    cam.location = (2.6, -math.sqrt(16 - 2.6 ** 2), 1.6)
    _look_at(cam, (0, -.15, .60 * height))
    scene.render.filepath = prefix + '_eye.png'
    bpy.ops.render.render(write_still=True)
    cam.location = (4, -.05, height * .72)
    _look_at(cam, (0, -.05, height * .60))
    scene.render.filepath = prefix + '_side.png'
    bpy.ops.render.render(write_still=True)
    cam.data.lens = 70
