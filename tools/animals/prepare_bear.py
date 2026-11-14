# SPDX-License-Identifier: MIT
"""Prepare Pebegou's static bear through Blender's glTF importer only.

Run from the repository. Source downloads are never Python import directories.
The saved numeric derivative is the only input used by normal animal builds.
"""
import argparse
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zoogen import build
from zoogen.scan import landmark_bones


def save_map(image, path, color):
    image.scale(min(image.size[0], 2048), min(image.size[1], 2048))
    image.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
    image.pixels[0]
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--inspect', action='store_true')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    build.reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(Path(args.source).resolve()))
    ob = bpy.data.objects['BearLow_Bear_0']
    source_triangles = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    shader = ob.data.materials[0].node_tree.nodes.get('Principled BSDF')
    base = shader.inputs['Base Color'].links[0].from_node.image
    normal_node = shader.inputs['Normal'].links[0].from_node
    normal = normal_node.inputs['Color'].links[0].from_node.image
    save_map(base, out / 'base.png', True)
    save_map(normal, out / 'normal.png', False)
    other = next(n.image for n in ob.data.materials[0].node_tree.nodes
                 if n.type == 'TEX_IMAGE' and n.image not in (base, normal))
    save_map(other, out / 'orm.png', False)
    points = np.array([ob.matrix_world @ v.co for v in ob.data.vertices])
    points = np.stack([14.7 - points[:, 1], points[:, 0], points[:, 2]], axis=1)
    points[:, 2] -= points[:, 2].min()
    ob.parent = None
    ob.matrix_world = Matrix.Identity(4)
    ob.data.vertices.foreach_set('co', build.to_blender(points).ravel())
    for other_ob in list(bpy.data.objects):
        if other_ob != ob:
            bpy.data.objects.remove(other_ob, do_unlink=True)
    ob.data.calc_loop_triangles()
    triangles = np.array([tri.vertices[:] for tri in ob.data.loop_triangles], dtype=np.int32)
    loops = np.array([tri.loops[:] for tri in ob.data.loop_triangles])
    uv = np.array([loop.uv[:] for loop in ob.data.uv_layers.active.data])[loops]
    if args.inspect:
        np.savez_compressed(out / 'inspection.npz', vertices=points.astype(np.float32),
                            triangles=triangles, uv=uv.astype(np.float32))
    print('SOURCE', source_triangles, 'triangles; static, no skeleton or animation', flush=True)
    if args.inspect:
        return
    from zoogen.bear import LANDMARKS, source_weights, neutralize
    import bmesh
    build._activate(ob)
    mesh = bmesh.new()
    mesh.from_mesh(ob.data)
    bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=.00001)
    mesh.to_mesh(ob.data)
    mesh.free()
    modifier = ob.modifiers.new('bear_budget', 'DECIMATE')
    modifier.ratio = 27000 / source_triangles
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    bones = landmark_bones(LANDMARKS)
    from zoogen.geom import Spec
    arm = build.build_armature(Spec('bear_source', [], []), bones)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    mesh = ob.data
    points = build.from_blender([v.co[:] for v in mesh.vertices])
    mesh.calc_loop_triangles()
    triangles = np.array([tri.vertices[:] for tri in mesh.loop_triangles], dtype=np.int32)
    loops = np.array([tri.loops[:] for tri in mesh.loop_triangles])
    uv = np.array([loop.uv[:] for loop in mesh.uv_layers.active.data])[loops]
    names = [bone.name for bone in bones]
    heat = np.zeros((len(points), len(bones)))
    for vertex in mesh.vertices:
        for group in vertex.groups:
            name = ob.vertex_groups[group.group].name
            if name in names:
                heat[vertex.index, names.index(name)] = group.weight
    weights = source_weights(points, bones, heat)
    weights = build._laplacian(weights, triangles, 2, .12)
    weights = build._limit_influences(weights)
    # Read source pigment only through the imported Blender image.
    width, height = base.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    base.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    colors = np.zeros((len(points), 3))
    samples = np.zeros(len(points))
    indices = (uv * [width, height]).astype(int) % [width, height]
    for corner in range(3):
        np.add.at(colors, triangles[:, corner], pixels[indices[:, corner, 1], indices[:, corner, 0], :3])
        np.add.at(samples, triangles[:, corner], 1)
    colors /= np.maximum(samples[:, None], 1)
    tags = np.zeros((len(points), 6), dtype=np.float32)
    tags[:, 0] = 1
    x, y, z = points.T
    nose = (x > .626) & (z > 2.27) & (z < 2.325)
    mouth = (x > .38) & (z > 1.97) & (z < 2.24) & (np.abs(y) < .075)
    teeth = mouth & (colors.mean(axis=1) > .28)
    eye = (x > .416) & (x < .450) & (z > 2.303) & (z < 2.346) & (np.abs(y) > .055)
    claws = ((np.abs(y) > .99) & (z < 1.17)) | ((z < .10) & (x > .08))
    # The mouth is an open sculpt. Assign each disconnected tooth as a rigid
    # piece so its root and tip cannot straddle the upper/lower jaw weights.
    parent = np.arange(len(points))
    def component(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index
    for triangle in triangles:
        for index in triangle[1:]:
            parent[component(index)] = component(triangle[0])
    groups = {}
    for index in range(len(points)):
        groups.setdefault(component(index), []).append(index)
    teeth[:] = False
    for group in groups.values():
        center = points[group].mean(axis=0)
        if len(group) < 400 and center[0] > .38 and 1.95 < center[2] < 2.27 and abs(center[1]) < .13:
            teeth[group] = True
            target = 'head' if center[2] > 2.15 else 'jaw'
            weights[group] = 0
            weights[group, names.index(target)] = 1
    for index, mask in enumerate((nose, mouth, teeth, eye, claws), 1):
        tags[mask] = np.eye(6)[index]
    data = dict(vertices=points.astype(np.float32), triangles=triangles, uv=uv.astype(np.float32),
                weights=weights.astype(np.float32), tags=tags,
                tag_names=np.array(['body', 'nose', 'mouth', 'teeth', 'eye', 'claws']),
                bone_names=np.array(names), bone_parents=np.array([b.parent or '' for b in bones]),
                heads=np.array([b.head for b in bones], dtype=np.float32),
                tails=np.array([b.tail for b in bones], dtype=np.float32),
                source_color=colors.astype(np.float32))
    data = neutralize(data)
    # The original upright axilla is compressed by a large repose. Smooth the
    # folded skin without changing the face, teeth or paw silhouettes.
    softened = build._laplacian(data['vertices'], triangles, 40, .4)
    trunk = weights[:, [names.index(n) for n in ('hips', 'spine1', 'spine2', 'chest',
                                                'scapula.L', 'scapula.R', 'upper_arm.L', 'upper_arm.R',
                                                'thigh.L', 'thigh.R')]].sum(axis=1)
    data['vertices'] += (softened - data['vertices']) * trunk[:, None]
    enamel = tags[:, 3] > .5
    jaw_z = data['heads'][names.index('head'), 2] + .08
    data['vertices'][enamel, 2] = jaw_z + .23 * (data['vertices'][enamel, 2] - jaw_z)
    np.savez_compressed(out / 'sculpt.npz', **data)
    # Remove baked broad muscle shading while retaining directional fur detail.
    luminance = pixels[..., :3] @ np.array([.2126, .7152, .0722])
    blur = luminance.copy()
    for radius in (1, 2, 4, 8, 16):
        blur = (blur + np.roll(blur, radius, 0) + np.roll(blur, -radius, 0)
                + np.roll(blur, radius, 1) + np.roll(blur, -radius, 1)) / 5
    from prepare_bigcat import save_grey
    save_grey(out / 'detail.png', np.clip(.5 + 2 * (luminance - blur), 0, 1))
    save_grey(out / 'occlusion.png', np.ones_like(luminance))
    print('PREPARED', len(triangles), 'triangles', len(points), 'vertices', len(bones), 'bones')


if __name__ == '__main__':
    main()
