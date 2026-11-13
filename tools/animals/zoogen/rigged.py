# SPDX-License-Identifier: MIT
"""Blender construction of portable, textured rigged derivatives.

This path is opt-in; the procedural and big-cat builders keep their previous defaults.
"""
from pathlib import Path

import bpy
import numpy as np

from . import build, preview


def material(source_dir, name, normal_strength=1.0, optional_maps=False):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    shader = mat.node_tree.nodes['Principled BSDF']
    shader.inputs['Roughness'].default_value = .9
    shader.inputs['Metallic'].default_value = 0
    for filename, role in [('base.png', 'base'), ('normal.png', 'normal'), ('occlusion.png', 'occlusion')]:
        if optional_maps and role != 'base' and not (Path(source_dir) / filename).is_file():
            continue
        image = bpy.data.images.load(str(Path(source_dir) / filename), check_existing=False)
        image.colorspace_settings.name = 'sRGB' if role == 'base' else 'Non-Color'
        texture = mat.node_tree.nodes.new('ShaderNodeTexImage')
        texture.image = image
        if role == 'base':
            mat.node_tree.links.new(texture.outputs['Color'], shader.inputs['Base Color'])
        elif role == 'normal':
            normal = mat.node_tree.nodes.new('ShaderNodeNormalMap')
            normal.inputs['Strength'].default_value = normal_strength
            mat.node_tree.links.new(texture.outputs['Color'], normal.inputs['Color'])
            mat.node_tree.links.new(normal.outputs['Normal'], shader.inputs['Normal'])
        else:
            group = bpy.data.node_groups.get('glTF Material Output')
            if group is None:
                group = bpy.data.node_groups.new('glTF Material Output', 'ShaderNodeTree')
                group.interface.new_socket(name='Occlusion', in_out='INPUT', socket_type='NodeSocketFloat')
            output = mat.node_tree.nodes.new('ShaderNodeGroup')
            output.node_tree = group
            mat.node_tree.links.new(texture.outputs['Color'], output.inputs['Occlusion'])
    return mat


def mesh_from_data(data, name):
    ob = build._mesh_object(name, build.to_blender(data['vertices']), data['triangles'])
    uv = ob.data.uv_layers.new(name='source_atlas')
    uv.data.foreach_set('uv', data['uv'].ravel())
    for poly in ob.data.polygons:
        poly.use_smooth = True
    return ob


def prepared_preview(data, source_dir, kind):
    scene = build.reset_scene()
    ob = mesh_from_data(data, kind + '_prepared')
    ob.data.materials.append(material(source_dir, kind))
    lo, hi = data['vertices'].min(axis=0), data['vertices'].max(axis=0)
    radius = float(np.linalg.norm(hi-lo)) / 2
    cam, _ = preview.setup_stage(scene, radius)
    preview.sculpt_lighting(scene)
    prefix = Path.cwd() / 'build-probe/animal-previews' / (kind + '_prepared')
    prefix.parent.mkdir(parents=True, exist_ok=True)
    preview.render_views(scene, cam, build.to_blender((lo+hi)/2), radius, str(prefix))


def calf_forehead_texture(image):
    """Replace baked adult horn caps with nearby photographic skin in each source map."""
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape(height, width, 4)
    original = pixels.copy()
    y, x = np.mgrid[:height, :width]
    for center, radius in [((.4081, .8943), (.020, .024)), ((.4081, .8518), (.022, .016))]:
        distance = np.sqrt(((x / width - center[0]) / radius[0]) ** 2 +
                           ((y / height - center[1]) / radius[1]) ** 2)
        blend = np.clip((1.4 - distance) / .4, 0, 1)
        blend = blend * blend * (3 - 2 * blend)
        donor_x = np.clip(x + round((.465 - center[0]) * width), 0, width - 1)
        donor_y = np.clip(y + round((.900 - center[1]) * height), 0, height - 1)
        pixels = pixels * (1 - blend[..., None]) + original[donor_y, donor_x] * blend[..., None]
    image.pixels.foreach_set(pixels.astype(np.float32).ravel())


def build_rigged(spec):
    from . import geom
    data = spec.sculpt
    ob = mesh_from_data(data, spec.name)
    bones = geom.expand_bones(spec.bones)
    names = [b.name for b in bones]
    source_names = data['bone_names'].tolist()
    weights = data['weights'][:, [source_names.index(n) for n in names]]
    for i, name in enumerate(names):
        group = ob.vertex_groups.new(name=name)
        for vertex in np.flatnonzero(weights[:, i] > 0):
            group.add([int(vertex)], float(weights[vertex, i]), 'REPLACE')
    if spec.name == 'rhino_juvenile':
        import bmesh
        welded = bmesh.new()
        welded.from_mesh(ob.data)
        bmesh.ops.remove_doubles(welded, verts=list(welded.verts), dist=.00001)
        welded.to_mesh(ob.data)
        welded.free()
    if len(ob.data.polygons) > spec.target_tris:
        build._activate(ob)
        modifier = ob.modifiers.new('juvenile_budget', 'DECIMATE')
        modifier.ratio = spec.target_tris / len(ob.data.polygons)
        modifier.use_collapse_triangulate = True
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        weights = np.zeros((len(ob.data.vertices), len(names)))
        for vertex in ob.data.vertices:
            for group in vertex.groups:
                weights[vertex.index, group.group] = group.weight
        weights = build._limit_influences(weights)
        for group in ob.vertex_groups:
            group.remove(list(range(len(ob.data.vertices))))
        for i, group in enumerate(ob.vertex_groups):
            for vertex in np.flatnonzero(weights[:, i] > 0):
                group.add([int(vertex)], float(weights[vertex, i]), 'REPLACE')
    mat = material(Path(data['source_path']).parent, spec.name, data['shape']['normal_strength'],
                   optional_maps=data.get('optional_maps', False))
    scratch = Path.cwd() / 'build-probe'
    for node in mat.node_tree.nodes:
        if node.type != 'TEX_IMAGE':
            continue
        image = node.image
        if spec.name == 'rhino_juvenile':
            calf_forehead_texture(image)
        if image.colorspace_settings.name == 'sRGB':
            width, height = image.size
            rgba = np.empty(width * height * 4, dtype=np.float32)
            image.pixels.foreach_get(rgba)
            rgba = rgba.reshape(height, width, 4)
            rgb = rgba[..., :3]
            lum = rgb @ np.array([.2126, .7152, .0722])
            adjusted = (lum[..., None] + data['shape']['color_saturation'] * (rgb - lum[..., None]))
            adjusted *= data['shape']['color_gain']
            if data['kind'] == 'elephant':
                # Correct the skin tiles only; source ivory and eyes retain their colours.
                skin = np.zeros((height, width), dtype=bool)
                skin[:1040, :1840] = True
                redness = np.clip((rgb[..., 0] - rgb[..., 2]) / (lum + .01) * 3, 0, 1)
                adjusted *= (1 - .50 * redness)[..., None]
                face = np.zeros((height, width), dtype=bool)
                face[:1040, 1040:1840] = True
                corrected = .25 + .22 * (adjusted - .25)
                adjusted = np.where(face[..., None], corrected, adjusted)
                adjusted = np.where(skin[..., None], adjusted, rgb)
            rgba[..., :3] = np.clip(adjusted, 0, 1)
            if 'texture_transform' in data:
                rgba = data['texture_transform'](rgba)
            image.pixels.foreach_set(rgba.ravel())
            image.filepath_raw = str(scratch / (spec.name + '_base.jpg'))
            image.file_format = 'JPEG'
            image.save(quality=90)
            replacement = bpy.data.images.load(image.filepath_raw, check_existing=False)
            node.image = replacement
        elif 'normal' in image.name.lower():
            image.pixels[0]  # Load before changing the lazy image's source filepath.
            image.filepath_raw = str(scratch / (spec.name + '_normal.jpg'))
            image.file_format = 'JPEG'
            image.save(quality=97)
            replacement = bpy.data.images.load(image.filepath_raw, check_existing=False)
            replacement.colorspace_settings.name = 'Non-Color'
            node.image = replacement
        elif spec.name == 'rhino_juvenile':
            image.filepath_raw = str(scratch / (spec.name + '_occlusion.png'))
            image.file_format = 'PNG'
            image.save()
            replacement = bpy.data.images.load(image.filepath_raw, check_existing=False)
            replacement.colorspace_settings.name = 'Non-Color'
            node.image = replacement
    ob.data.materials.append(mat)
    return ob, {'bones': bones, 'bone_names': names, 'triangles': len(ob.data.polygons),
                'vertices': len(ob.data.vertices)}


def model_previews(scene, arm, spec, clips, args):
    import math
    from . import anim
    data = spec.sculpt
    output = Path(args.preview)
    output.mkdir(parents=True, exist_ok=True)
    prefix = str(output / spec.name)
    lo, hi = data['vertices'].min(axis=0), data['vertices'].max(axis=0)
    center = build.to_blender((lo+hi)/2)
    radius = float(np.linalg.norm(hi-lo)) / 2
    cam, _ = preview.setup_stage(scene, radius)
    preview.sculpt_lighting(scene)
    anim.clear_pose(arm)
    if not args.preview_clips_only:
        preview.render_views(scene, cam, center, radius, prefix)
        distance = 9 if data['kind'] == 'elephant' else 6
        cam.data.type = 'PERSP'
        cam.data.lens = 40
        scene.render.resolution_x, scene.render.resolution_y = 1600, 1000
        scene.render.image_settings.file_format = 'PNG'
        cam.location = (distance * .65, -distance * math.sqrt(1-.65**2), 1.6)
        preview._look_at(cam, (0, -.15, .58 * data['height']))
        scene.render.filepath = prefix + '_eye.png'
        bpy.ops.render.render(write_still=True)
        cam.location = (distance, -.05, .65 * data['height'])
        preview._look_at(cam, center)
        scene.render.filepath = prefix + '_side.png'
        bpy.ops.render.render(write_still=True)
        cam.data.lens = 70
    want = args.clips.split(',') if args.clips else [clip['name'] for clip in clips]
    for clip in clips:
        if clip['name'] in want:
            anim.preview_clip(scene, cam, arm, spec.name, clip, center, radius,
                              prefix + '_' + clip['name'] + '_strip', phases=6, res=(400, 300))


def bake_source_occlusion(data, source_dir):
    """Generate the missing elephant AO from its imported neutral geometry only."""
    scene = build.reset_scene()
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.samples = 16
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 2
    ob = mesh_from_data(data, 'occlusion_source')
    mat = bpy.data.materials.new('source_ao_bake')
    mat.use_nodes = True
    ob.data.materials.append(mat)
    image = bpy.data.images.new('source_ao', 1024, 1024, alpha=False)
    image.colorspace_settings.name = 'Non-Color'
    texture = mat.node_tree.nodes.new('ShaderNodeTexImage')
    texture.image = image
    mat.node_tree.nodes.active = texture
    build._activate(ob)
    bpy.ops.object.bake(type='AO', margin=4, use_clear=True)
    image.filepath_raw = str(Path(source_dir) / 'occlusion.png')
    image.file_format = 'PNG'
    image.save()
