# SPDX-License-Identifier: MIT
"""Fixed-exposure S6b review of exported GLBs, including the read-only rhino."""
import argparse
import math
from pathlib import Path
import sys

import bpy
import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from zoogen import preview


def load_model(path, offset=0):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    objects = set(bpy.data.objects) - before
    actions = {}
    for obj in objects:
        if obj.type == 'ARMATURE':
            if obj.animation_data is not None:
                for track in obj.animation_data.nla_tracks:
                    actions[track.name] = track.strips[0].action
            obj.animation_data_clear()
            for bone in obj.pose.bones:
                bone.matrix_basis.identity()
        if obj.parent is None:
            obj.location.x += offset
    return objects, actions


def stage():
    scene = bpy.context.scene
    cam, ground = preview.setup_stage(scene, 8)
    scene.render.threads = 3
    scene.cycles.samples = 16
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.look = 'AgX - Medium High Contrast'
    scene.view_settings.exposure = 0
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.70, .80, 1, 1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
    sun = bpy.data.objects['sun']
    sun.rotation_euler = (math.radians(35), 0, math.radians(-35))
    sun.data.energy = 3.2
    sun.data.color = (1, .94, .82)
    sun.data.angle = math.radians(4)
    rgb = [((v / 255 + .055) / 1.055) ** 2.4 for v in (200, 185, 150)]
    ground.data.materials[0].node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (*rgb, 1)
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1000
    scene.render.image_settings.file_format = 'PNG'
    cam.data.type = 'PERSP'
    cam.data.lens = 32
    return scene, cam


def model_bounds(objects):
    bpy.context.view_layer.update()
    # Ignore the importer's hidden Icosphere bone-display helper. Frame only
    # evaluated skinned surfaces, including their inverse bind transforms.
    graph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in objects:
        if obj.type == 'MESH' and any(mod.type == 'ARMATURE' for mod in obj.modifiers):
            evaluated = obj.evaluated_get(graph)
            points.extend(evaluated.matrix_world @ vertex.co for vertex in evaluated.data.vertices)
    points = np.array(points)
    low, high = points.min(axis=0), points.max(axis=0)
    return (low+high)/2, float(np.linalg.norm(high-low))/2


def render_clips(scene, cam, objects, actions, prefix, requested):
    """Review imported animation channels without rebuilding or rebaking an asset."""
    arm = next(obj for obj in objects if obj.type == 'ARMATURE')
    animation = arm.animation_data_create()
    animation.use_nla = False
    animation.action = actions['idle']
    scene.frame_set(round(actions['idle'].frame_range[0]))
    center, radius = model_bounds(objects)
    cam.data.lens = 70
    names = ('idle', 'walk', 'trot', 'eat', 'drink', 'rest', 'sleep', 'vocalize', 'swim')
    for name in names if requested == 'all' else requested.split(','):
        action = actions[name]
        animation.action = action
        first, last = action.frame_range
        tiles = []
        for index in range(6):
            scene.frame_set(round(first + (last-first) * index / 6))
            path = prefix + '_' + name + '_frame.png'
            tiles.append(preview.render_tile(scene, cam, center, radius, 'side', (400, 300),
                                             path, distance=5.2))
        preview.save_sheet(tiles, 4, (400, 300), prefix + '_' + name + '_strip.png')
    arm.animation_data_clear()
    for bone in arm.pose.bones:
        bone.matrix_basis.identity()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', required=True)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--compare', action='store_true')
    parser.add_argument('--views', default='8m,25m,side')
    parser.add_argument('--clips', help='all or comma-separated exported clip names')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    objects, actions = load_model(args.model)
    scene, cam = stage()
    for view in args.views.split(','):
        if view == 'sheet':
            center, radius = model_bounds(objects)
            cam.data.lens = 70
            preview.render_views(scene, cam, center, radius, args.prefix)
            cam.data.lens = 32
            continue
        scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
        if view == 'side':
            cam.location = (10, 0, 1.6)
        else:
            distance = 8 if view == '8m' else 25
            cam.location = (distance * .65, -distance * math.sqrt(1 - .65 ** 2), 1.6)
        preview._look_at(cam, (0, -.25, 1.8))
        scene.render.filepath = args.prefix + '_' + view + '.png'
        bpy.ops.render.render(write_still=True)
    if args.clips:
        render_clips(scene, cam, objects, actions, args.prefix, args.clips)
    if args.compare:
        cam.data.lens = 32
        load_model(Path('assets/animals/rhino.glb'), -4.2)
        cam.location = (5, -12, 1.6)
        preview._look_at(cam, (-1.8, -.3, 1.6))
        scene.render.filepath = args.prefix + '_rhino.png'
        bpy.ops.render.render(write_still=True)


if __name__ == '__main__':
    main()
