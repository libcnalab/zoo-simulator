# SPDX-License-Identifier: MIT
"""Portable sculpt bind data and the opt-in Blender mesh path.

Only numeric/string NPZ arrays are accepted; pickle is never enabled. Proportion
edits are hierarchical affine bind edits blended with the sculpt's own weights.
"""
from functools import lru_cache
from pathlib import Path

import numpy as np

from .geom import Bone


@lru_cache(maxsize=4)
def read_source(path):
    with np.load(path, allow_pickle=False) as archive:
        return {key: archive[key] for key in archive.files}


def prepare_instance(path, edits, height):
    source = read_source(str(path))
    names = source['bone_names'].tolist()
    parents = source['bone_parents'].tolist()
    transforms = {}
    heads, tails = [], []
    for name, parent, head, tail in zip(names, parents, source['heads'], source['tails']):
        edit = edits.get(name, edits.get(name.split('.')[0], {}))
        local = np.eye(4)
        local[:3, :3] = np.diag(edit.get('scale', [1, 1, 1]))
        local[:3, 3] = head - local[:3, :3] @ head + np.array(edit.get('offset', [0, 0, 0]))
        transform = transforms.get(parent, np.eye(4)) @ local
        transforms[name] = transform
        heads.append((transform @ np.append(head, 1))[:3])
        tails.append((transform @ np.append(tail, 1))[:3])
    vertices = np.zeros_like(source['vertices'], dtype=np.float64)
    for index, name in enumerate(names):
        transform = transforms[name]
        vertices += source['weights'][:, index, None] * (
            source['vertices'] @ transform[:3, :3].T + transform[:3, 3])
    heads, tails = np.array(heads), np.array(tails)
    floor = vertices[:, 2].min()
    vertices[:, 2] -= floor
    heads[1:, 2] -= floor
    tails[1:, 2] -= floor
    chest = heads[names.index('chest')]
    shoulder = (np.abs(vertices[:, 0] - chest[0]) < .12) & (np.abs(vertices[:, 1]) < .13)
    measured = vertices[shoulder, 2].max()
    scale = height / measured
    vertices *= scale
    heads *= scale
    tails *= scale
    bones = [Bone(name, parent or None, head, tail, name != 'root')
             for name, parent, head, tail in zip(names, parents, heads, tails)
             if not name.endswith('.R')]
    return dict(source, vertices=vertices, heads=heads, tails=tails, bones=bones,
                source_path=str(path), scale=scale)


def _map_pixels(path, non_color=False, color=False):
    import bpy
    image = bpy.data.images.load(str(path), check_existing=False)
    if non_color:
        image.colorspace_settings.name = 'Non-Color'
    width, height = image.size
    rgba = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(rgba)
    bpy.data.images.remove(image)
    pixels = rgba.reshape(height, width, 4)
    return pixels[..., :3] if color else pixels[..., 0]


def build_sculpt(spec):
    import bmesh
    import bpy
    from . import build, geom

    data = spec.sculpt
    bones = geom.expand_bones(spec.bones)
    names = [bone.name for bone in bones]
    tags = data['tag_names'].tolist()
    for part in spec.parts:
        if part.tag not in tags:
            tags.append(part.tag)
    vertices = [build.to_blender(data['vertices'])]
    triangles = [data['triangles']]
    source_weights = data['weights'][:, [data['bone_names'].tolist().index(name) for name in names]]
    weights = [source_weights]
    tag_values = [np.pad(data['tags'], ((0, 0), (0, len(tags) - data['tags'].shape[1])))]
    # Big cats with source materials keep the authored atlas whole; the others paint a coat
    # beside it and give the added parts strips of their own.
    source_material = data.get('source_material', False)
    atlas = (1.0 if source_material else .62 if any(part.tag == 'mane' for part in spec.parts)
             else .86 if spec.parts else 1.0)
    uvs = [data['uv'] * [atlas, 1]]
    # Extra lofted hair is unwrapped into a separate strip, never fused with the sculpt.
    for part in geom.expand_parts(spec.parts):
        verts, tris, dictionaries = geom.loft(part)
        extra = build._mesh_object(part.name, build.to_blender(verts), tris)
        build._activate(extra)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(island_margin=.025)
        bpy.ops.object.mode_set(mode='OBJECT')
        uv = np.array([loop.uv[:] for loop in extra.data.uv_layers.active.data]).reshape(-1, 3, 2)
        if source_material:
            # A verified unused corner of the authored atlas holds opaque hair shadows.
            uv = uv * [.14, .20] + [.85, .79] if part.tag == 'mane' else uv * .02 + [.45, .96]
        else:
            # Each part has its own vertical strip cell; separate sampling prevents coat bleeding.
            number = len(vertices) - 1
            count = len(geom.expand_parts(spec.parts))
            columns = 8
            rows = (count + columns - 1) // columns
            if part.name == 'mane_undercoat':
                uv[..., 0] = atlas + .005 + uv[..., 0] * .24
                uv[..., 1] = .005 + .99 * uv[..., 1]
            else:
                strip = .87 if atlas == .62 else atlas
                uv[..., 0] = strip + (number % columns + .03 + .94 * uv[..., 0]) / columns * (1 - strip)
                uv[..., 1] = (number // columns + .03 + .94 * uv[..., 1]) / rows
        uvs.append(uv)
        triangles.append(tris + sum(len(value) for value in vertices))
        vertices.append(build.to_blender(verts))
        w = np.zeros((len(verts), len(names)))
        for index, dictionary in enumerate(dictionaries):
            for name, value in dictionary.items():
                w[index, names.index(name)] = value
        weights.append(w)
        tag = np.zeros((len(verts), len(tags)))
        tag[:, tags.index(part.tag)] = 1
        tag_values.append(tag)
        bpy.data.objects.remove(extra, do_unlink=True)
    vertices, triangles = np.concatenate(vertices), np.concatenate(triangles)
    weights, tag_values, uvs = np.concatenate(weights), np.concatenate(tag_values), np.concatenate(uvs)
    ob = build._mesh_object(spec.name, vertices, triangles)
    mesh = ob.data
    mesh.uv_layers.new(name='sculpt_coat')
    mesh.uv_layers.active.data.foreach_set('uv', uvs.ravel())
    for index, name in enumerate(names):
        group = ob.vertex_groups.new(name=name)
        for vertex in np.flatnonzero(weights[:, index]):
            group.add([int(vertex)], float(weights[vertex, index]), 'REPLACE')
    if spec.target_tris < len(triangles):
        # Cub only: preserve authored UVs and weights through a second modest collapse.
        old_vertices, old_triangles, old_tags = vertices, triangles, tag_values
        build._activate(ob)
        modifier = ob.modifiers.new('cub_budget', 'DECIMATE')
        modifier.ratio = spec.target_tris / len(triangles)
        modifier.use_collapse_triangulate = True
        if source_material:
            # The shared derivative already devotes about 22k triangles to the head.
            # A uniform second collapse retains that bias without starving the round torso.
            low, high = 0., modifier.ratio
            for _ in range(9):
                modifier.ratio = (low + high) / 2
                bpy.context.view_layer.update()
                evaluated = ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
                if len(evaluated.data.polygons) > spec.target_tris:
                    high = modifier.ratio
                else:
                    low = modifier.ratio
            modifier.ratio = low
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        simplified = bmesh.new()
        simplified.from_mesh(ob.data)
        collapsed = [face for face in simplified.faces if face.calc_area() < 1e-12]
        if collapsed:
            bmesh.ops.delete(simplified, geom=collapsed, context='FACES_ONLY')
        simplified.to_mesh(ob.data)
        simplified.free()
        mesh = ob.data
        # Collapse can leave duplicate faces, which glTF validation would discard.
        mesh.validate(clean_customdata=False)
        mesh.update()
        vertices = np.array([vertex.co[:] for vertex in mesh.vertices])
        triangles = np.array([poly.vertices[:] for poly in mesh.polygons])
        uvs = np.array([loop.uv[:] for loop in mesh.uv_layers.active.data]).reshape(-1, 3, 2)
        # Transfer tags only; the modifier already interpolated vertex groups.
        tag_values = build._transfer(old_vertices, old_triangles, old_tags, vertices)
        weights = np.zeros((len(vertices), len(names)))
        for vertex in mesh.vertices:
            for group in vertex.groups:
                weights[vertex.index, group.group] = group.weight
        weights = build._limit_influences(weights)
        for group in ob.vertex_groups:
            group.remove(list(range(len(vertices))))
        for index, group in enumerate(ob.vertex_groups):
            for vertex in np.flatnonzero(weights[:, index]):
                group.add([int(vertex)], float(weights[vertex, index]), 'REPLACE')
    for poly in mesh.polygons:
        poly.use_smooth = True
    mesh.update()
    normals = build.from_blender([vertex.normal[:] for vertex in mesh.vertices])
    animal = build.from_blender(vertices)
    size = spec.texture_size
    tri_id, bary = build._rasterize(uvs, size)
    rgb = np.zeros((size, size, 3), dtype=np.float32)
    source_dir = Path(data['source_path']).parent
    if source_material:
        return _paint_source_material(spec, ob, bones, names, tags, tag_values, weights, triangles,
                                      animal, normals, tri_id, bary, rgb, source_dir)
    detail = _map_pixels(source_dir / 'detail.png', non_color=data.get('detail_non_color', False))
    occlusion = _map_pixels(source_dir / 'occlusion.png')
    # Paint in rows to keep the 2048 atlas well below a gigabyte of working arrays.
    for start in range(0, size, 64):
        end = min(start + 64, size)
        context = build.PaintContext(size, tri_id[start:end], bary[start:end], triangles, animal,
                                     normals, tag_values, tags, weights, names)
        if data.get('photo_texture', False):
            from .birdscan import photo_context
            photo_context(context, data, start, end, size, atlas)
        color = spec.paint(context)
        xx = np.arange(size)[None, :] / size / atlas
        yy = np.arange(start, end)[:, None] / size
        ix = np.minimum((xx * detail.shape[1]).astype(int), detail.shape[1] - 1)
        iy = np.minimum((yy * detail.shape[0]).astype(int), detail.shape[0] - 1)
        factor = (1 + data.get('detail_strength', .34) * (detail[iy, ix] - .5)) * (.87 + .13 * occlusion[iy, ix])
        factor = np.where(xx < 1, factor, 1)
        rgb[start:end] = np.clip(color * factor[..., None], 0, 1)
    rgb = build._dilate(rgb, tri_id >= 0, iters=12)
    image = bpy.data.images.new(spec.name + '_albedo', size, size, alpha=False)
    rgba = np.ones((size, size, 4), dtype=np.float32)
    rgba[..., :3] = rgb
    image.pixels.foreach_set(rgba.ravel())
    image.pack()
    material = bpy.data.materials.new(spec.name)
    material.use_nodes = True
    shader = material.node_tree.nodes['Principled BSDF']
    texture = material.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    material.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
    shader.inputs['Roughness'].default_value = .86
    if data.get('normal_map'):
        attach_source_normal(material, data, atlas)
    mesh.materials.append(material)
    return ob, dict(bones=bones, bone_names=names, triangles=len(mesh.polygons),
                    vertices=len(mesh.vertices), image=image)


def attach_source_normal(material, data, atlas=1.0):
    """Opt-in source normal atlas, with flat normals over added silhouette geometry."""
    import bpy
    source = bpy.data.images.load(str(data['normal_map']), check_existing=False)
    source.colorspace_settings.name = 'Non-Color'
    width, height = source.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    source.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    target = np.ones_like(pixels)
    target[..., :3] = [.5, .5, 1]
    limit = int(width * atlas)
    indices = np.minimum((np.arange(limit) / atlas).astype(int), width - 1)
    target[:, :limit] = pixels[:, indices]
    image = bpy.data.images.new(material.name + '_fur_normal', width, height, alpha=False)
    image.colorspace_settings.name = 'Non-Color'
    image.pixels.foreach_set(target.ravel())
    image.pack()
    texture = material.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    normal = material.node_tree.nodes.new('ShaderNodeNormalMap')
    normal.inputs['Strength'].default_value = data.get('normal_strength', 1.0)
    material.node_tree.links.new(texture.outputs['Color'], normal.inputs['Color'])
    shader = material.node_tree.nodes.get('Principled BSDF')
    material.node_tree.links.new(normal.outputs['Normal'], shader.inputs['Normal'])
    bpy.data.images.remove(source)


def _paint_source_material(spec, ob, bones, names, tags, tag_values, weights, triangles, animal,
                           normals, tri_id, bary, rgb, source_dir):
    """Big cats: the source's own albedo, normal and occlusion maps, recoated, plus hair cards."""
    import bpy
    from . import build
    from .rigged import material as rigged_material

    data = spec.sculpt
    size = spec.texture_size
    mesh = ob.data
    from .bigcat_coat import coat, soften
    base = _map_pixels(source_dir / 'base.png', color=True)
    normal_map = _map_pixels(source_dir / 'normal.png', color=True)
    occlusion = _map_pixels(source_dir / 'occlusion.png', color=True)
    if data['kind'] == 'juvenile':
        base = .65 * base + .35 * soften(base, 2)
    for start in range(0, size, 64):
        end = min(start + 64, size)
        context = build.PaintContext(size, tri_id[start:end], bary[start:end], triangles, animal,
                                     normals, tag_values, tags, weights, names)
        rgb[start:end] = coat(context, base[start:end], data)
        if data['kind'] == 'juvenile':
            muscle = context.bones('scapula', 'upper_arm', 'forearm', 'thigh', 'shin', 'chest', 'hips')
            strength = np.clip(1 - .88 * muscle - .60 * context.bones('head', 'jaw'), .12, 1)
            normal_map[start:end, :, :2] = .5 + (normal_map[start:end, :, :2] - .5) * strength[..., None]
    # The extra opaque volume shares the body's material and therefore casts shadows.
    normal_map[807:1016, 869:1016] = [.5, .5, 1]
    occlusion[807:1016, 869:1016] = 1
    normal_map[982:1005, 459:483] = [.5, .5, 1]
    occlusion[982:1005, 459:483] = 1
    rgb = build._dilate(rgb, tri_id >= 0, iters=6)
    images = {}
    scratch = Path.cwd() / 'build-probe'
    scratch.mkdir(parents=True, exist_ok=True)
    for role, values in [('base', rgb), ('normal', normal_map), ('occlusion', occlusion)]:
        image = bpy.data.images.new(spec.name + '_' + role, size, size, alpha=False)
        image.colorspace_settings.name = 'sRGB' if role == 'base' else 'Non-Color'
        rgba = np.ones((size, size, 4), dtype=np.float32)
        rgba[..., :3] = values
        image.pixels.foreach_set(rgba.ravel())
        image.filepath_raw = str(scratch / (spec.name + '_' + role + '.jpg'))
        image.file_format = 'JPEG'
        image.save(quality=90 if role == 'base' else 97)
        replacement = bpy.data.images.load(image.filepath_raw, check_existing=False)
        replacement.colorspace_settings.name = image.colorspace_settings.name
        images[role] = replacement
    mesh.materials.append(rigged_material(source_dir, spec.name, images=images, roughness=.84))
    from .bigcat_hair import attach_cards
    attach_cards(ob, data, names)
    mesh = ob.data
    return ob, dict(bones=bones, bone_names=names, triangles=len(mesh.polygons),
                    vertices=len(mesh.vertices), image=images['base'])
