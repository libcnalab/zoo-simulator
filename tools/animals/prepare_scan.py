# SPDX-License-Identifier: MIT
"""Prepare a static quadruped using Blender's importer and species landmarks.

Run from the repository, never from the untrusted source directory:
blender -b -t 3 --factory-startup --python tools/animals/prepare_scan.py -- \
    --species zebra --source <download.glb> --out tools/animals/sources/zebra
"""
import argparse
import importlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bmesh
import bpy
import numpy as np
from mathutils import Matrix
from zoogen import build, preview
from zoogen.scan import corrected_weights, landmark_bones, repose_bind


def save_image(image, path, duplicate=False, color=True):
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    if duplicate:
        pixels = np.concatenate([pixels, pixels], axis=1)
    result = bpy.data.images.new(path.stem, pixels.shape[1], pixels.shape[0], alpha=False)
    result.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
    result.pixels.foreach_set(pixels.ravel())
    result.filepath_raw = str(path)
    result.file_format = 'PNG'
    result.save()
    bpy.data.images.remove(result)


def halve_and_mirror(ob, side):
    """Bisect at animal Y=0, preserve loop UVs, mirror and weld only the seam."""
    mesh = bmesh.new()
    mesh.from_mesh(ob.data)
    bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=.00001)
    bmesh.ops.bisect_plane(mesh, geom=list(mesh.verts) + list(mesh.edges) + list(mesh.faces),
                          plane_co=(0, 0, 0), plane_no=(0, 1, 0), dist=.00001,
                          clear_outer=side < 0, clear_inner=side > 0)
    uv = mesh.loops.layers.uv.active
    for face in mesh.faces:
        for loop in face.loops:
            loop[uv].uv.x *= .5
    original_faces = set(mesh.faces)
    result = bmesh.ops.duplicate(mesh, geom=list(mesh.verts) + list(mesh.edges) + list(mesh.faces))
    mirrored = [v for v in result['geom'] if isinstance(v, bmesh.types.BMVert)]
    bmesh.ops.transform(mesh, verts=mirrored, matrix=Matrix.Diagonal((1, -1, 1, 1)))
    faces = [face for face in mesh.faces if face not in original_faces]
    bmesh.ops.reverse_faces(mesh, faces=faces)
    for face in faces:
        for loop in face.loops:
            loop[uv].uv.x += .5
    seam = [v for v in mesh.verts if abs(v.co.y) < .0001]
    for vertex in seam:
        vertex.co.y = 0
    bmesh.ops.remove_doubles(mesh, verts=seam, dist=.0001)
    bmesh.ops.recalc_face_normals(mesh, faces=list(mesh.faces))
    mesh.to_mesh(ob.data)
    mesh.free()



def replace_ridge(ob, config):
    """Replace a duplicated narrow sagittal crest by one short closed hair ridge.

    Colour is projected from the adjacent intact neck, so its stripes continue
    into the added mane instead of introducing an unrelated procedural pattern.
    """
    from mathutils import Vector
    from mathutils.bvhtree import BVHTree
    from mathutils.geometry import barycentric_transform
    mesh = ob.data
    mesh.calc_loop_triangles()
    points = [vertex.co.copy() for vertex in mesh.vertices]
    triangles = [triangle.vertices[:] for triangle in mesh.loop_triangles]
    uv_values = [[mesh.uv_layers.active.data[index].uv.copy() for index in triangle.loops]
                 for triangle in mesh.loop_triangles]
    tree = BVHTree.FromPolygons(points, triangles, all_triangles=True)
    for vertex in mesh.vertices:
        x, y, z = vertex.co
        cap = config['base_z'] + config['slope'] * x
        if config['start'] < x < config['remove_end'] and abs(y) < config['half_width'] and z > cap:
            vertex.co.z = cap
    bm = bmesh.new()
    bm.from_mesh(mesh)
    uv_layer = bm.loops.layers.uv.active
    rings, samples = [], []
    count = 32
    for index in range(count):
        t = index / (count - 1)
        x = config['start'] * (1 - t) + config['end'] * t
        base = config['base_z'] + config['slope'] * x - .008
        height = config['height'] * max(0, np.sin(np.pi * t)) ** .35
        height += .003 * (index % 3)
        ring = [bm.verts.new((x, y, z)) for y, z in
                [(-.016, base), (.016, base), (.005, base + height), (-.005, base + height)]]
        rings.append(ring)
        query = Vector((x, -.10, base - .035))
        nearest, _, triangle_index, _ = tree.find_nearest(query)
        tri = triangles[triangle_index]
        uv_triangle = [Vector((uv.x, uv.y, 0)) for uv in uv_values[triangle_index]]
        sampled = barycentric_transform(nearest, *[points[i] for i in tri], *uv_triangle)
        samples.append(sampled.xy)
    for index in range(count - 1):
        for side in range(4):
            after = (side + 1) % 4
            face = bm.faces.new((rings[index][side], rings[index+1][side],
                                 rings[index+1][after], rings[index][after]))
            for loop, uv in zip(face.loops, [samples[index], samples[index+1],
                                            samples[index+1], samples[index]]):
                local_base = config['base_z'] + config['slope'] * loop.vert.co.x - .008
                loop[uv_layer].uv = uv + Vector((.02 * loop.vert.co.y,
                                                       .10 * (loop.vert.co.z - local_base)))
    for index in (0, count - 1):
        face = bm.faces.new(rings[index])
        for loop in face.loops:
            local_base = config['base_z'] + config['slope'] * loop.vert.co.x - .008
            loop[uv_layer].uv = samples[index] + Vector((.02 * loop.vert.co.y,
                                                        .10 * (loop.vert.co.z - local_base)))
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(mesh)
    bm.free()


def split_mouth(ob, config):
    """Separate the lip edges so jaw rotation opens a slit instead of stretching it."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    normal = (config['slope'], 0, 1)
    result = bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces),
                                    plane_co=(0, 0, config['level']), plane_no=normal,
                                    dist=.000001)
    edges = [edge for edge in result['geom_cut'] if isinstance(edge, bmesh.types.BMEdge)
             and all(vertex.co.x > config['front'] for vertex in edge.verts)]
    bmesh.ops.split_edges(bm, edges=edges)
    lower = ob.vertex_groups.new(name='scan_mouth_lower')
    upper = ob.vertex_groups.new(name='scan_mouth_upper')
    layer = bm.verts.layers.deform.verify()
    for vertex in bm.verts:
        if vertex.co.x <= config['front'] or not vertex.link_faces:
            continue
        center = sum((face.calc_center_median() for face in vertex.link_faces), vertex.co * 0)
        center /= len(vertex.link_faces)
        signed = config['slope'] * center.x + center.z - config['level']
        vertex[layer][lower.index if signed < 0 else upper.index] = 1
    bm.to_mesh(ob.data)
    bm.free()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source')
    parser.add_argument('--reweight', action='store_true',
                        help='refit saved numeric heat data without importing the original')
    parser.add_argument('--out', required=True)
    parser.add_argument('--no-preview', action='store_true')
    parser.add_argument('--species', default='zebra')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    species = importlib.import_module('species.' + args.species)
    config = species.PREPARATION
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if args.reweight:
        from zoogen.scan import reweight_source
        with np.load(out / 'scan.npz', allow_pickle=False) as archive:
            data = {key: archive[key] for key in archive.files}
        data = reweight_source(data, species.LANDMARKS, config.get('neutral_rotations', {}))
        np.savez_compressed(out / 'scan.npz', **data)
        print('REWEIGHTED', len(data['vertices']), 'vertices', flush=True)
        if not args.no_preview:
            render_prepared(data, out)
        return
    if not args.source:
        parser.error('--source is required unless --reweight is used')
    scene = build.reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(Path(args.source).resolve()))
    objects = [ob for ob in scene.objects if ob.type == 'MESH']
    if len(objects) != 1:
        raise ValueError('Prepare one joined static surface; multi-material atlasing is not implicit')
    ob = objects[0]
    if any(other.type == 'ARMATURE' for other in scene.objects):
        raise ValueError('Use prepare_rigged for a source with an authored skeleton')
    mat = ob.data.materials[0]
    shader = next(node for node in mat.node_tree.nodes if node.type == 'BSDF_PRINCIPLED')
    base = shader.inputs['Base Color'].links[0].from_node.image
    if max(base.size) > 1024 and config.get('keep_side'):
        base.scale(1024, 1024)
    elif max(base.size) > 2048:
        base.scale(2048, 2048)
    save_image(base, out / 'base.png', bool(config.get('keep_side')))
    if shader.inputs['Normal'].is_linked:
        normal_node = shader.inputs['Normal'].links[0].from_node
        normal = normal_node.inputs['Color'].links[0].from_node.image
        normal.scale(*base.size)
        save_image(normal, out / 'normal.png', bool(config.get('keep_side')), color=False)
    rotation = np.array(config['rotation'])
    origin = np.array(config['origin'])
    vertices = np.array([ob.matrix_world @ v.co for v in ob.data.vertices])
    vertices = (vertices @ rotation.T - origin) * config['scale']
    ob.matrix_world = Matrix.Identity(4)
    for vertex, point in zip(ob.data.vertices, vertices):
        vertex.co = point
    if config.get('keep_side'):
        halve_and_mirror(ob, config['keep_side'])
    print('SCAN mirrored', flush=True)
    # Surface corrections belong to species data, so the importer can be reused.
    for edit in config.get('surface_edits', []):
        for vertex in ob.data.vertices:
            point = np.array(vertex.co)
            if np.all(point >= edit['min']) and np.all(point <= edit['max']):
                anchor = np.array(edit['anchor'])
                vertex.co = anchor + (point - anchor) * edit['scale']
    if 'ridge' in config:
        replace_ridge(ob, config['ridge'])
    if 'mouth' in config:
        split_mouth(ob, config['mouth'])
    mesh = ob.data
    mesh.calc_loop_triangles()
    build._activate(ob)
    landmarks = {name: head for name, _, head, _ in species.LANDMARKS}
    landmark_scale = landmarks['chest'][2] / 1.20
    head_start = landmarks['head'][0] - .05 * landmark_scale
    protect = ob.vertex_groups.new(name='scan_detail')
    for vertex in mesh.vertices:
        weight = 1.0 if vertex.co.x > head_start or vertex.co.z < .20 * landmark_scale else .08
        protect.add([vertex.index], weight, 'REPLACE')
    modifier = ob.modifiers.new('scan_budget', 'DECIMATE')
    modifier.ratio = config['triangles'] / len(mesh.loop_triangles)
    modifier.vertex_group = protect.name
    modifier.vertex_group_factor = 3
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    print('SCAN decimated', len(ob.data.polygons), flush=True)
    ob.vertex_groups.remove(ob.vertex_groups['scan_detail'])
    cleaned = bmesh.new()
    cleaned.from_mesh(ob.data)
    bmesh.ops.dissolve_degenerate(cleaned, edges=list(cleaned.edges), dist=.000001)
    cleaned.to_mesh(ob.data)
    cleaned.free()
    ob.data.validate()
    bones = landmark_bones(species.LANDMARKS)
    # build_armature uses render coordinates; transform the surface consistently.
    for vertex in ob.data.vertices:
        vertex.co = build.to_blender(vertex.co)
    from zoogen.geom import Spec
    arm = build.build_armature(Spec('scan', [], []), bones)
    bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    print('SCAN heat begin', flush=True)
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    print('SCAN heat complete', flush=True)
    mesh = ob.data
    mesh.calc_loop_triangles()
    vertices = build.from_blender([v.co[:] for v in mesh.vertices])
    triangles = np.array([tri.vertices[:] for tri in mesh.loop_triangles], dtype=np.int32)
    loops = np.array([tri.loops[:] for tri in mesh.loop_triangles])
    uv = np.array([loop.uv[:] for loop in mesh.uv_layers.active.data])[loops]
    heat = np.zeros((len(vertices), len(bones)))
    names = [bone.name for bone in bones]
    for vertex in mesh.vertices:
        for group in vertex.groups:
            name = ob.vertex_groups[group.group].name
            if name in names:
                heat[vertex.index, names.index(name)] = group.weight
    if not np.any(heat):
        # Heat needs a closed, non-intersecting volume. A proxy avoids destroying
        # the source UV surface merely to satisfy the solver's topology demands.
        proxy = ob.copy()
        proxy.data = ob.data.copy()
        scene.collection.objects.link(proxy)
        proxy.modifiers.clear()
        proxy.vertex_groups.clear()
        build._activate(proxy)
        remesh = proxy.modifiers.new('heat_volume', 'REMESH')
        remesh.mode = 'VOXEL'
        remesh.voxel_size = .025
        remesh.use_smooth_shade = True
        bpy.ops.object.modifier_apply(modifier=remesh.name)
        for bone in arm.data.bones:
            bone.use_deform = not (bone.name == 'root' or bone.name.startswith(('ear', 'tail', 'jaw')))
        bpy.ops.object.select_all(action='DESELECT')
        proxy.select_set(True)
        arm.select_set(True)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.object.parent_set(type='ARMATURE_AUTO')
        proxy_weights = np.zeros((len(proxy.data.vertices), len(bones)))
        for vertex in proxy.data.vertices:
            for group in vertex.groups:
                name = proxy.vertex_groups[group.group].name
                if name in names:
                    proxy_weights[vertex.index, names.index(name)] = group.weight
        if not np.any(proxy_weights):
            raise ValueError('Both surface and manifold-proxy bone heat failed')
        proxy.data.calc_loop_triangles()
        proxy_vertices = build.from_blender([v.co[:] for v in proxy.data.vertices])
        proxy_triangles = np.array([tri.vertices[:] for tri in proxy.data.loop_triangles])
        heat = build._transfer(proxy_vertices, proxy_triangles, proxy_weights, vertices)
        bpy.data.objects.remove(proxy, do_unlink=True)
        for bone in arm.data.bones:
            bone.use_deform = bone.name != 'root'
        print('SCAN heat transferred from manifold proxy', flush=True)
    weights = corrected_weights(vertices, bones, heat)
    weights = build._laplacian(weights, triangles, 2, .18)
    weights = build._limit_influences(weights)
    mouth_targets = np.full(len(vertices), -1, dtype=np.int16)
    for vertex in mesh.vertices:
        for group in vertex.groups:
            group_name = ob.vertex_groups[group.group].name
            if group_name in ('scan_mouth_lower', 'scan_mouth_upper') and group.weight > .5:
                target = 'jaw' if group_name.endswith('lower') else 'head'
                weights[vertex.index] = 0
                weights[vertex.index, names.index(target)] = 1
                mouth_targets[vertex.index] = names.index(target)
    tag_names = ['body', 'head', 'jaw', 'ear', 'tail', 'hoof']
    tags = np.zeros((len(vertices), len(tag_names)), dtype=np.float32)
    for index, name in enumerate(names):
        stem = name.split('.')[0]
        tag = ('tail' if stem.startswith('tail') else 'hoof' if stem in ('fingers', 'toes')
               else stem if stem in tag_names else 'body')
        tags[:, tag_names.index(tag)] += weights[:, index]
    data = dict(raw_vertices=vertices.astype(np.float32), heat_weights=heat.astype(np.float32),
                mouth_targets=mouth_targets, vertices=vertices.astype(np.float32),
                triangles=triangles, uv=uv.astype(np.float32),
                weights=weights.astype(np.float32), tags=tags, tag_names=np.array(tag_names),
                bone_names=np.array(names), bone_parents=np.array([b.parent or '' for b in bones]),
                heads=np.array([b.head for b in bones], dtype=np.float32),
                tails=np.array([b.tail for b in bones], dtype=np.float32),
                heat_weighted_vertices=np.array(np.count_nonzero(heat.sum(axis=1))))
    data = repose_bind(data, config.get('neutral_rotations', {}))
    np.savez_compressed(out / 'scan.npz', **data)
    print('PREPARED',len(triangles),'triangles',len(vertices),'vertices',len(bones),'bones',
          data['heat_weighted_vertices'],'heat-weighted vertices',flush=True)
    if not args.no_preview:
        render_prepared(data, out)


def render_prepared(data, out):
    from zoogen.rigged import mesh_from_data, material
    scene = build.reset_scene()
    prepared = mesh_from_data(data, 'prepared_scan')
    prepared.data.materials.append(material(out, 'prepared_scan', optional_maps=True))
    cam, _ = preview.setup_stage(scene, 1.6)
    preview.sculpt_lighting(scene)
    prefix = str(Path.cwd() / 'build-probe/animal-previews/zebra_prepared')
    preview.render_views(scene, cam, build.to_blender((.15, 0, .8)), 1.25, prefix)
    grey = bpy.data.materials.new('clay')
    grey.diffuse_color = (.45, .45, .45, 1)
    prepared.data.materials.clear()
    prepared.data.materials.append(grey)
    preview.render_views(scene, cam, build.to_blender((.15, 0, .8)), 1.25, prefix + '_grey')


if __name__ == '__main__':
    main()
