# SPDX-License-Identifier: MIT
"""Prepare the locally supplied taxidermy scan through Blender's glTF importer only.

Run from the repository root. The original is read-only; all temporary data stays
in build-probe. Generation subsequently needs only the numeric derivative and PNGs.
"""
import argparse
from pathlib import Path
import sys

import bpy
import bmesh
import numpy as np
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zoogen import build, preview


def image_save(image, path):
    image.filepath_raw = str(path.resolve())
    image.file_format = 'PNG'
    image.save()


def prepare(source, output):
    output.mkdir(parents=True, exist_ok=True)
    scene = build.reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(source.resolve()))
    meshes = [ob for ob in bpy.data.objects if ob.type == 'MESH']
    for ob in meshes:
        build._activate(ob)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        ob.parent = None
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_world = Matrix.Identity(4)
        for vertex in ob.data.vertices:
            x, y, z = vertex.co
            vertex.co = (y * .001, .20 - x * .001, z * .001)
    bpy.ops.object.select_all(action='DESELECT')
    for ob in meshes:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    high = bpy.context.object
    high.name = 'giraffe_full_scan'
    print('FULL_TRIANGLES', len(high.data.polygons), flush=True)
    source_image = next(node.image for mat in high.data.materials for node in mat.node_tree.nodes
                        if node.type == 'TEX_IMAGE')
    source_image.scale(2048, 2048)
    image_save(source_image, output / 'photo.png')
    low = high.copy()
    low.data = high.data.copy()
    scene.collection.objects.link(low)
    low.name = 'giraffe_derivative'
    build._activate(low)
    bm = bmesh.new()
    bm.from_mesh(low.data)
    bmesh.ops.remove_doubles(bm, verts=list(bm.verts), dist=.00002)
    # The cut mount end is buried inside the new shoulder, never exported.
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < -2.0], context='VERTS')
    bm.to_mesh(low.data)
    bm.free()
    group = low.vertex_groups.new(name='head_priority')
    for vertex in low.data.vertices:
        if vertex.co.z > -.55:
            group.add([vertex.index], 1.0, 'REPLACE')
    mod = low.modifiers.new('scan_budget', 'DECIMATE')
    mod.ratio = 13000 / len(low.data.polygons)
    mod.use_collapse_triangulate = True
    mod.vertex_group = group.name
    mod.vertex_group_factor = 12.0
    bpy.ops.object.modifier_apply(modifier=mod.name)
    low.data.validate(verbose=False, clean_customdata=False)
    print('LOW_TRIANGLES', len(low.data.polygons), flush=True)
    for poly in low.data.polygons:
        poly.use_smooth = True
    # Bake in the original UV layout before any living-animal proportion edit.
    normal = bpy.data.images.new('giraffe_scan_normal', 2048, 2048, alpha=False)
    normal.colorspace_settings.name = 'Non-Color'
    bake_mat = bpy.data.materials.new('giraffe_normal_bake')
    bake_mat.use_nodes = True
    low.data.materials.clear()
    low.data.materials.append(bake_mat)
    for poly in low.data.polygons:
        poly.material_index = 0
    tex = bake_mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = normal
    bake_mat.node_tree.nodes.active = tex
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 8
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 3
    build._activate(low)
    high.select_set(True)
    bpy.ops.object.bake(type='NORMAL', use_selected_to_active=True, cage_extrusion=.04,
                        max_ray_distance=.08, margin=8)
    image_save(normal, output / 'normal.png')
    vertices = np.array([v.co[:] for v in low.data.vertices], dtype=np.float32)
    triangles = np.array([p.vertices[:] for p in low.data.polygons], dtype=np.int32)
    uv = np.empty(len(low.data.loops) * 2, dtype=np.float32)
    low.data.uv_layers.active.data.foreach_get('uv', uv)
    np.savez_compressed(output / 'scan.npz', vertices=vertices, triangles=triangles,
                        uv=uv.reshape(-1, 3, 2))
    bpy.data.objects.remove(high, do_unlink=True)
    # Preview the derivative in metres, upright, with its photographic colour.
    low.data.materials.clear()
    mat = bpy.data.materials.new('scan_photo')
    mat.use_nodes = True
    shader = mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Roughness'].default_value = .95
    tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
    tex.image = source_image
    mat.node_tree.links.new(tex.outputs['Color'], shader.inputs['Base Color'])
    low.data.materials.append(mat)
    for v in low.data.vertices:
        v.co = build.to_blender(v.co)
    cam, plane = preview.setup_stage(scene, 3)
    plane.hide_render = True
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    lo, hi = vertices.min(0), vertices.max(0)
    preview.render_views(scene, cam, build.to_blender((lo+hi)/2), float(np.linalg.norm(hi-lo))/2,
                         'build-probe/animal-previews/giraffe_prepared', res=(800, 800))
    print('PREPARED', vertices.shape, triangles.shape, 'head triangles',
          int((vertices[triangles].mean(1)[:, 2] > -.55).sum()), flush=True)


if __name__ == '__main__':
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    prepare(args.source, args.out)
