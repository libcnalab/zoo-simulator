# SPDX-License-Identifier: MIT
"""Import trusted-by-provenance, untrusted-by-content bird data through Blender only.

Run from the repository, never from a download directory. The portable derivative
contains numeric arrays and resized images, not source scripts or Blender files.
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bpy
import bmesh
import numpy as np

from zoogen import build, preview, birdscan, geom


def imported_mesh(source, budget):
    bpy.ops.import_scene.gltf(filepath=str(source))
    meshes = [ob for ob in bpy.context.scene.objects if ob.type == 'MESH']
    for ob in meshes:
        print('SOURCE_OBJECT', ob.name, dict(ob), flush=True)
        world = ob.matrix_world.copy()
        ob.parent = None
        ob.matrix_world = world
    bpy.ops.object.select_all(action='DESELECT')
    for ob in meshes:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.join()
    ob = bpy.context.object
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    for other in list(bpy.context.scene.objects):
        if other != ob:
            bpy.data.objects.remove(other, do_unlink=True)
    print('SOURCE_MATERIALS', [(m.name, dict(m)) for m in ob.data.materials], flush=True)
    # The scan is tiled into mesh nodes; weld their coincident boundaries before
    # collapse so that separate tile budgets cannot destroy the silhouette.
    mesh = bmesh.new()
    mesh.from_mesh(ob.data)
    extent = max(v.co.z for v in mesh.verts) - min(v.co.z for v in mesh.verts)
    bmesh.ops.remove_doubles(mesh, verts=list(mesh.verts), dist=extent * .0005)
    bmesh.ops.triangulate(mesh, faces=list(mesh.faces))
    loose = [v for v in mesh.verts if not v.link_faces]
    bmesh.ops.delete(mesh, geom=loose, context='VERTS')
    mesh.to_mesh(ob.data)
    mesh.free()
    count = len(ob.data.polygons)
    modifier = ob.modifiers.new('portable_budget', 'DECIMATE')
    modifier.ratio = min(1., budget / count)
    modifier.use_collapse_triangulate = True
    bpy.ops.object.modifier_apply(modifier=modifier.name)
    print('SOURCE_TRIANGLES', count, 'PREPARED_TRIANGLES', len(ob.data.polygons), flush=True)
    return ob


def inspect(args):
    scene = build.reset_scene()
    ob = imported_mesh(args.source, 12500)
    vertices = np.array([v.co[:] for v in ob.data.vertices])
    triangles = np.array([p.vertices[:] for p in ob.data.polygons])
    if ob.data.uv_layers:
        uv = np.array([loop.uv[:] for loop in ob.data.uv_layers.active.data]).reshape(-1, 3, 2)
    else:
        uv = np.zeros((len(triangles), 3, 2))
    material_ids = np.array([p.material_index for p in ob.data.polygons])
    print('BOUNDS', vertices.min(axis=0), vertices.max(axis=0), flush=True)
    args.out.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.out / 'import.npz', vertices=vertices, triangles=triangles,
                        uv=uv, material_ids=material_ids)
    saved = set()
    for mat in ob.data.materials:
        for node in mat.node_tree.nodes:
            if node.type == 'TEX_IMAGE' and node.image.name not in saved:
                im = node.image
                print('SOURCE_IMAGE', im.name, tuple(im.size), im.colorspace_settings.name, flush=True)
                im.scale(min(2048, im.size[0]), min(2048, im.size[1]))
                im.filepath_raw = str(args.out / (im.name.replace('/', '_') + '.png'))
                im.file_format = 'PNG'
                im.save()
                saved.add(im.name)
    lo, hi = vertices.min(axis=0), vertices.max(axis=0)
    vertices -= [(lo[0]+hi[0])/2, (lo[1]+hi[1])/2, lo[2]]
    vertices /= hi[2]-lo[2]
    ob.data.vertices.foreach_set('co', vertices.ravel())
    for poly in ob.data.polygons:
        poly.use_smooth = True
    cam, _ = preview.setup_stage(scene, 1)
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    preview.render_views(scene, cam, (0, 0, .5), .65,
                         'build-probe/animal-previews/' + args.kind + '_source', res=(640, 640))


def write_image(path, pixels):
    height, width = pixels.shape[:2]
    image = bpy.data.images.new(path.stem, width, height, alpha=False)
    rgba = np.ones((height, width, 4), dtype=np.float32)
    rgba[..., :3] = pixels
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = str(path.resolve())
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def read_pixels(path):
    image = bpy.data.images.load(str(path), check_existing=False)
    image.scale(1024, 1024)
    values = np.empty(1024 * 1024 * 4, dtype=np.float32)
    image.pixels.foreach_get(values)
    bpy.data.images.remove(image)
    return values.reshape(1024, 1024, 4)[..., :3]


def connected_components(triangles):
    """Yield mesh islands in stable vertex order, including detached webbing."""
    adjacency = {}
    for triangle in triangles:
        for vertex in triangle:
            adjacency.setdefault(int(vertex), set()).update(int(value) for value in triangle)
    visited = set()
    for first in adjacency:
        if first in visited:
            continue
        pending, component = [first], []
        visited.add(first)
        while pending:
            current = pending.pop()
            component.append(current)
            for neighbor in sorted(adjacency[current] - visited):
                visited.add(neighbor)
                pending.append(neighbor)
        yield component


def prepare(args):
    with np.load(args.from_inspection / 'import.npz', allow_pickle=False) as archive:
        data = {key: archive[key] for key in archive.files}
    vertices = birdscan.neutral_vertices(data['vertices'], args.kind)
    triangles, uv = data['triangles'], data['uv'].copy()
    materials = data['material_ids']
    side_hints = np.where(vertices[:, 1] >= 0, 1, -1)
    if args.kind == 'penguin':
        feet = triangles[np.all(vertices[triangles, 2] < .07, axis=1)]
        for component in connected_components(feet):
            side_hints[component] = 1 if vertices[component, 1].mean() >= 0 else -1
    if args.kind == 'flamingo':
        # Replace only the lifted leg with a reflection of the planted one. The
        # pelvis, folded wings, head and S-neck remain the artist's sculpt.
        remove = np.all((vertices[triangles, 2] < .73) & (vertices[triangles, 1] > 0), axis=1)
        copy = np.all((vertices[triangles, 2] < .73) & (vertices[triangles, 1] < 0)
                      & (vertices[triangles, 0] > -.30), axis=1)
        # Some webbing crosses the centre plane. Follow connected lower-leg
        # faces instead of cutting those toes at y=0 and leaving floating scraps.
        lower = triangles[np.all(vertices[triangles, 2] < .65, axis=1)]
        for component in connected_components(lower):
            if len(component) < 100:
                continue
            side_hints[component] = 1 if vertices[component, 1].mean() >= 0 else -1
            touches = np.any(np.isin(triangles, component), axis=1)
            if vertices[component, 1].mean() > 0:
                remove |= touches
            else:
                copy |= touches
        mirror = vertices.copy() * [1, -1, 1]
        triangles = np.concatenate([triangles[~remove], triangles[copy, ::-1] + len(vertices)])
        vertices = np.concatenate([vertices, mirror])
        side_hints = np.concatenate([side_hints, -side_hints])
        uv = np.concatenate([uv[~remove], uv[copy, ::-1]])
        materials = np.concatenate([materials[~remove], materials[copy]])
        # The two source materials share an atlas; each image keeps its own UV tile.
        uv[..., 0] = (uv[..., 0] + materials[:, None]) * .5
    used, inverse = np.unique(triangles, return_inverse=True)
    vertices, triangles = vertices[used], inverse.reshape(-1, 3)
    side_hints = side_hints[used]
    if args.kind == 'penguin':
        # Close the lower mandible around its hinge; vocalize reopens it through jaw weights.
        x, y, z = vertices.T
        bill = (x > .118) & (z > .54) & (z < .605 + .12 * (x-.12))
        vertices[bill, 2] += .009 * np.clip((vertices[bill, 0]-.10)/.07, 0, 1)
        for side in [-1, 1]:
            feet = (vertices[:, 1] * side > 0) & (vertices[:, 2] < .07) & (vertices[:, 0] > -.055)
            low = vertices[feet, 2].min()
            vertices[feet, 2] -= low * np.clip((.07-vertices[feet, 2])/.04, 0, 1)
    # Welding scan tiles can expose coincident faces; keep one UV-bearing face
    # per vertex triple before the portable data reaches the glTF exporter.
    _, unique_faces = np.unique(np.sort(triangles, axis=1), axis=0, return_index=True)
    unique_faces.sort()
    triangles, uv, materials = triangles[unique_faces], uv[unique_faces], materials[unique_faces]
    if args.kind == 'flamingo':
        debris = []
        for component in connected_components(triangles):
            points = vertices[component]
            if points[:, 2].max() < .6 and points[:, 2].min() > .03:
                debris.extend(component)
        keep = ~np.any(np.isin(triangles, debris), axis=1)
        triangles, uv, materials = triangles[keep], uv[keep], materials[keep]
        used, inverse = np.unique(triangles, return_inverse=True)
        vertices, triangles = vertices[used], inverse.reshape(-1, 3)
        side_hints = side_hints[used]
    bones, _ = birdscan.skeleton(args.kind)
    weights, tags, tag_names = birdscan.chain_weights(vertices, bones, args.kind, side_hints)
    if args.kind == 'flamingo':
        eyes = np.unique(triangles[materials == 1])
        tags[eyes] = 0
        tags[eyes, tag_names.index('eye')] = 1
        weights[eyes] = 0
        weights[eyes, [b.name for b in geom.expand_bones(bones)].index('head')] = 1
    expanded = geom.expand_bones(bones)
    args.out.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(args.out / 'mesh.npz', vertices=vertices.astype(np.float32),
                        triangles=triangles.astype(np.int32), uv=uv.astype(np.float32),
                        weights=weights, tags=tags, tag_names=np.array(tag_names),
                        bone_names=np.array([b.name for b in expanded]),
                        bone_parents=np.array([b.parent or '' for b in expanded]),
                        heads=np.array([b.head for b in expanded]), tails=np.array([b.tail for b in expanded]))
    base = read_pixels(args.from_inspection / 'Image_0.png')
    if args.kind == 'flamingo':
        second = read_pixels(args.from_inspection / 'Image_1.png')
        base = np.concatenate([base[:, ::2], second[:, ::2]], axis=1)
    write_image(args.out / 'base.png', base)
    luminance = base @ np.array([.2126, .7152, .0722])
    smooth = sum(np.roll(np.roll(luminance, dy, 0), dx, 1)
                 for dx in [-8, -4, 0, 4, 8] for dy in [-8, -4, 0, 4, 8]) / 25
    detail = np.clip(.5 + 1.5 * (luminance-smooth), 0, 1)
    write_image(args.out / 'detail.png', np.repeat(detail[..., None], 3, axis=2))
    write_image(args.out / 'occlusion.png', np.ones_like(base))
    print('BIRD_DERIVATIVE', args.kind, len(vertices), 'vertices', len(triangles), 'triangles', flush=True)
    if args.no_preview:
        return
    scene = build.reset_scene()
    ob = build._mesh_object('prepared', build.to_blender(vertices), triangles)
    for poly in ob.data.polygons:
        poly.use_smooth = True
    cam, _ = preview.setup_stage(scene, 1)
    preview.sculpt_lighting(scene)
    scene.render.threads = 3
    height = .65 if args.kind == 'penguin' else 1.4
    preview.render_views(scene, cam, build.to_blender((0, 0, height*.5)), height*.65,
                         'build-probe/animal-previews/' + args.kind + '_prepared', res=(640, 640))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--kind', choices=['penguin', 'flamingo'], required=True)
    parser.add_argument('--no-preview', action='store_true')
    parser.add_argument('--from-inspection', type=Path,
                        help='Numeric/image output of this script, never an untrusted download directory')
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    if args.from_inspection:
        prepare(args)
    elif args.source:
        # Inspection output lives in scratch, even when the destination is a source directory.
        destination = args.out
        args.out = Path.cwd() / 'build-probe' / (args.kind + '-source')
        inspect(args)
        args.from_inspection = args.out
        args.out = destination
        prepare(args)
    else:
        parser.error('--source or --from-inspection is required')


if __name__ == '__main__':
    main()
