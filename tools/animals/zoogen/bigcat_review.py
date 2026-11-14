# SPDX-License-Identifier: MIT
"""Matched source/derivative head views and close material inspection."""
from pathlib import Path

import numpy as np


def head_comparison(source_meshes, data):
    import bpy
    from . import build, preview
    scene = build.reset_scene()
    objects = []
    for label, meshes in [('source', source_meshes),
                          ('prepared', [(data['vertices'], data['triangles'])])]:
        vertices, triangles = [], []
        for points, faces in meshes:
            triangles.append(faces + sum(len(v) for v in vertices))
            vertices.append(points)
        ob = build._mesh_object(label, build.to_blender(np.concatenate(vertices)), np.concatenate(triangles))
        mat = bpy.data.materials.new(label + '_clay')
        mat.diffuse_color = (.38, .38, .38, 1)
        ob.data.materials.append(mat)
        for polygon in ob.data.polygons:
            polygon.use_smooth = True
        objects.append(ob)
    cam, _ = preview.setup_stage(scene, 2)
    preview.sculpt_lighting(scene)
    scene.render.threads = 4
    prefix = Path.cwd() / 'build-probe/animal-previews/bigcat_head_compare'
    prefix.parent.mkdir(parents=True, exist_ok=True)
    tiles = []
    # Each row is source on the left and the closed, reduced derivative on the right.
    for view in ('front', 'front34', 'side'):
        for index in range(2):
            for other, ob in enumerate(objects):
                ob.hide_render = other != index
            tiles.append(preview.render_tile(scene, cam, build.to_blender((1.06, 0, 1.05)),
                                             .30, view, (700, 600), str(prefix) + '_tile.png'))
    preview.save_sheet(tiles, 2, (700, 600), str(prefix) + '.png')


def face_views(scene, cam, data, prefix):
    import bpy
    from mathutils import Vector
    from . import build, preview
    head = data['heads'][data['bone_names'].tolist().index('head')].copy()
    head[0] += .10 * data['height']
    center = Vector(build.to_blender(head))
    cam.location = center + Vector((.88, -1.18, .28)).normalized() * 1.5
    cam.data.lens = 72
    preview._look_at(cam, center)
    scene.render.resolution_x, scene.render.resolution_y = 1200, 1000
    scene.render.filepath = prefix + '_face.png'
    bpy.ops.render.render(write_still=True)

    sun = bpy.data.objects['sun']
    original_rotation = sun.rotation_euler.copy()
    scene.render.resolution_x, scene.render.resolution_y = 600, 500
    tiles = []
    for angle in (-1.1, 1.1):
        sun.rotation_euler[2] = angle
        path = prefix + '_normal_tile.png'
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        image = bpy.data.images.load(path)
        values = np.empty(600 * 500 * 4, dtype=np.float32)
        image.pixels.foreach_get(values)
        tiles.append(values.reshape(500, 600, 4))
        bpy.data.images.remove(image)
        Path(path).unlink()
    preview.save_sheet(tiles, 2, (600, 500), prefix + '_normal_check.png')
    sun.rotation_euler = original_rotation
