# SPDX-License-Identifier: MIT
"""Import the licensed pachyderm sources through Blender, never through a raw GLB reader.

Run from the repository root with --source <glb> --out tools/animals/sources/<species>.
The portable derivative contains no pickled objects and needs no source download at build time.
"""
import argparse
import math
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zoogen import build, preview

TAGS = ['body', 'eye', 'horn', 'tusk', 'nails', 'mouth', 'ear', 'trunk']
CREDITS = {
    'rhino': ('Animated Rhinoceros Walking Animal Loop', 'LasquetiSpice',
              'animated-rhinoceros-walking-animal-loop-32abc12d4fe44bd0b116b294194d9615'),
    'elephant': ('Elephant CYCLE', 'exegetpro', 'elephant-cycle-50e17ba5c2dc4e3d9ed0effc7d2f9fd5'),
}


def source_key(name, kind):
    if kind == 'rhino':
        return name.split('.')[0]
    return name.removesuffix('_Armature')


def bone_mapping(arm, kind):
    mapping = {}
    if kind == 'rhino':
        base = {'Root': 'hips', 'Spine': 'spine1', 'Spine1': 'spine2', 'Spine2': 'chest',
                'Spine3': 'neck1', 'Neck': 'neck2', 'Head': 'head', 'Jaw': 'jaw',
                'B_Lip': 'lower_lip', 'Tail': 'tail1'}
        for i in range(1, 5):
            base['Tail' + str(i)] = 'tail' + str(i + 1)
        for side in ('L', 'R'):
            for src, dst in [('Ear1', 'ear'), ('Ear2', 'ear_tip'), ('Clavicle', 'scapula'),
                             ('Shoulder', 'upper_arm'), ('Elbow', 'forearm'), ('Wrist', 'hand'),
                             ('Hand', 'fingers'), ('Hip', 'thigh'), ('Knee', 'shin'),
                             ('Ankle', 'foot'), ('Ball', 'toes')]:
                base[side + '_' + src] = dst + '.' + side
    else:
        # This source's spine runs from chest to tail, opposite to the game skeleton.
        # Preserve world-space bind joints, then explicitly reverse the body hierarchy.
        base = {'Body': 'chest', 'Spine1': 'spine2', 'Spine2': 'spine1', 'Spine3': 'hips',
                'Taz': 'hips', 'Zhivot': 'spine2', 'Sheya': 'neck1', 'Golova': 'head',
                'Chelust': 'jaw', 'Chelust.001': 'lower_lip'}
        for i in range(1, 6):
            base['Hobot' + str(i)] = 'trunk' + str(2 * i - 1)
        for i in range(1, 5):
            base['Hvost' + str(i)] = 'tail' + str(i)
        for side in ('L', 'R'):
            for src, dst in [('Uho1', 'ear'), ('Uho2', 'ear_tip'), ('Uho22', 'ear_tip'),
                             ('Uho11', 'ear'), ('Lopatka', 'scapula'), ('Plecho', 'upper_arm'),
                             ('Lokot', 'forearm'), ('Ruka', 'hand'), ('Bedro', 'thigh'),
                             ('Golen', 'shin'), ('Noga', 'foot')]:
                base[src + '.' + side] = dst + '.' + side
        base['Uho11.R.002'] = 'ear.R'
    for bone in arm.data.bones:
        key = source_key(bone.name, kind)
        mapping[bone.name] = base.get(key, mapping.get(bone.parent.name, 'root') if bone.parent else 'root')
    return mapping, base


def canonical_parent(name):
    parents = {'root': '', 'hips': 'root', 'spine1': 'hips', 'spine2': 'spine1', 'chest': 'spine2',
               'neck1': 'chest', 'neck2': 'neck1', 'head': 'neck2', 'jaw': 'head', 'lower_lip': 'jaw'}
    if name in parents:
        return parents[name]
    if name.startswith(('tail', 'trunk')):
        stem = 'tail' if name.startswith('tail') else 'trunk'
        n = int(name[len(stem):])
        return (stem + str(n - 1)) if n > 1 else ('hips' if stem == 'tail' else 'head')
    stem, side = name.split('.')
    return {'ear': 'head', 'ear_tip': 'ear.' + side, 'scapula': 'chest',
            'upper_arm': 'scapula.' + side, 'forearm': 'upper_arm.' + side,
            'hand': 'forearm.' + side, 'fingers': 'hand.' + side,
            'thigh': 'hips', 'shin': 'thigh.' + side, 'foot': 'shin.' + side,
            'toes': 'foot.' + side}[stem]


def pixels(image):
    w, h = image.size
    data = np.empty(w * h * 4, dtype=np.float32)
    image.pixels.foreach_get(data)
    return data.reshape(h, w, 4)


def save_pixels(path, values, color=False):
    h, w = values.shape[:2]
    image = bpy.data.images.new(path.stem, w, h, alpha=False)
    if not color:
        image.colorspace_settings.name = 'Non-Color'
    rgba = np.ones((h, w, 4), dtype=np.float32)
    rgba[..., :3] = values[..., :3]
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def material_maps(mat):
    shader = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    base = shader.inputs['Base Color'].links[0].from_node.image
    normal = None
    if shader.inputs['Normal'].is_linked:
        normal_node = shader.inputs['Normal'].links[0].from_node
        normal = normal_node.inputs['Color'].links[0].from_node.image
    occ = None
    for node in mat.node_tree.nodes:
        if node.type == 'GROUP' and 'Occlusion' in node.inputs and node.inputs['Occlusion'].is_linked:
            upstream = node.inputs['Occlusion'].links[0].from_node
            if upstream.type == 'SEPARATE_COLOR':
                upstream = upstream.inputs['Color'].links[0].from_node
            occ = upstream.image
    return base, normal, occ


def atlas_maps(objects, out, kind):
    if kind == 'rhino':
        base, normal, occ = material_maps(objects[0].data.materials[0])
        save_pixels(out / 'base.png', pixels(base), True)
        save_pixels(out / 'normal.png', pixels(normal))
        save_pixels(out / 'occlusion.png', pixels(occ))
        return {objects[0].name: (np.zeros(2), np.ones(2))}
    # Rectangular packing preserves UV orientation and the tangent normal basis. Each
    # source receives an eight-pixel extruded gutter, including the small eye island.
    slots = {'Elephant_body': (8, 8, 1024, 1024), 'Elephant_ears': (1320, 8, 512, 1024),
             'Elephant_head': (1048, 8, 256, 1024), 'Elephant_tusk': (8, 1048, 512, 512),
             'Elephant_eye': (536, 1048, 256, 256)}
    base_atlas = np.zeros((2048, 2048, 3), dtype=np.float32)
    normal_atlas = np.zeros_like(base_atlas)
    normal_atlas[:] = [.5, .5, 1]
    transforms = {}
    for ob in objects:
        material = ob.data.materials[0]
        base, normal, _ = material_maps(material)
        x, y, w, h = slots[material.name]
        for image, atlas in ((base, base_atlas), (normal, normal_atlas)):
            if image is None:
                continue
            data = pixels(image)[..., :3]
            if data.shape[:2] != (h, w):
                raise ValueError('Unexpected source image dimensions')
            atlas[y-8:y+h+8, x-8:x+w+8] = np.pad(data, ((8, 8), (8, 8), (0, 0)), mode='wrap')
        transforms[ob.name] = (np.array([x, y]) / 2048, np.array([w, h]) / 2048)
    save_pixels(out / 'base.png', base_atlas, True)
    save_pixels(out / 'normal.png', normal_atlas)
    # Reserve the AO slot; preparation replaces it with a geometry bake below.
    save_pixels(out / 'occlusion.png', np.ones((32, 32, 3), dtype=np.float32))
    return transforms


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    out = Path(args.out).resolve()
    kind = out.name
    if kind not in CREDITS:
        raise ValueError('Output directory must be named rhino or elephant')
    out.mkdir(parents=True, exist_ok=True)
    build.reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(Path(args.source).resolve()))
    arm = next(ob for ob in bpy.data.objects if ob.type == 'ARMATURE')
    objects = sorted([ob for ob in bpy.data.objects if ob.type == 'MESH' and ob.vertex_groups],
                     key=lambda ob: ob.name)
    mapping, explicit = bone_mapping(arm, kind)
    # Elephant faces +Y in imported world space, rhino -Y. Neither conversion reflects geometry.
    conversion = np.array([[0, -1 if kind == 'rhino' else 1, 0],
                           [1 if kind == 'rhino' else -1, 0, 0], [0, 0, 1]], dtype=float)
    def animal(point):
        return conversion @ np.array(point)
    names = ['root']
    source_bones = {'root': None}
    heads = {'root': np.zeros(3)}
    tails = {'root': np.array([.1, 0, 0])}
    for bone in arm.data.bones:
        key = source_key(bone.name, kind)
        target = mapping[bone.name]
        if key not in explicit or target in names:
            continue
        names.append(target)
        source_bones[target] = bone.name
        heads[target] = animal(arm.matrix_world @ bone.head_local)
        tails[target] = animal(arm.matrix_world @ bone.tail_local)
    if kind == 'elephant':
        # Auxiliary midpoint bones do not displace the imported bind surface.
        names.append('neck2')
        heads['neck2'] = (heads['neck1'] + heads['head']) / 2
        tails['neck2'] = heads['head'].copy()
        source_bones['neck2'] = source_bones['neck1']
        for i in range(1, 11, 2):
            name, added = 'trunk' + str(i), 'trunk' + str(i + 1)
            names.append(added)
            heads[added] = (heads[name] + tails[name]) / 2
            tails[added] = tails[name].copy()
            tails[name] = heads[added].copy()
            source_bones[added] = source_bones[name]
        for side in ('.L', '.R'):
            for start, end in [('hand', 'fingers'), ('foot', 'toes')]:
                name, added = start + side, end + side
                names.append(added)
                heads[added] = heads[name] + .75 * (tails[name] - heads[name])
                tails[added] = heads[added] + [.18, 0, 0]
                source_bones[added] = source_bones[name]
    # Stable topological order is also the portable archive's weight-column order.
    ordered = []
    while len(ordered) < len(names):
        ready = [n for n in names if n not in ordered and (not canonical_parent(n) or canonical_parent(n) in ordered)]
        if not ready:
            raise ValueError('Invalid mapped hierarchy')
        ordered.extend(ready)
    names = ordered
    parents = [canonical_parent(n) for n in names]
    by = {n: i for i, n in enumerate(names)}
    transforms = atlas_maps(objects, out, kind)
    arrays = {key: [] for key in ('vertices', 'triangles', 'uv', 'weights', 'tags')}
    offset = 0
    for ob in objects:
        mesh = ob.data
        mesh.calc_loop_triangles()
        vertices = np.array([animal(ob.matrix_world @ v.co) for v in mesh.vertices])
        triangles = np.array([tri.vertices[:] for tri in mesh.loop_triangles], dtype=np.int32)
        loop_ids = np.array([tri.loops[:] for tri in mesh.loop_triangles])
        uv = np.array([loop.uv[:] for loop in mesh.uv_layers[0].data])[loop_ids]
        shift, scale_uv = transforms[ob.name]
        if kind == 'elephant' and (uv.min() < -.02 or uv.max() > 1.02):
            raise ValueError(f'Atlas UV range for {ob.name}: {uv.min(axis=(0, 1))} .. {uv.max(axis=(0, 1))}')
        uv = uv * scale_uv + shift
        weights = np.zeros((len(vertices), len(names)))
        tags = np.zeros((len(vertices), len(TAGS)))
        tags[:, 0] = 1
        for vertex in mesh.vertices:
            for group in vertex.groups:
                src_name = ob.vertex_groups[group.group].name
                target = mapping[src_name]
                weights[vertex.index, by[target]] += group.weight
        if kind == 'elephant':
            mat_name = mesh.materials[0].name
            tag = {'Elephant_eye': 'eye', 'Elephant_tusk': 'tusk', 'Elephant_ears': 'ear'}.get(mat_name)
            if tag:
                tags[:] = 0
                tags[:, TAGS.index(tag)] = 1
            else:
                trunk = weights[:, [by['trunk' + str(i)] for i in range(1, 11, 2)]].sum(axis=1)
                tags[:, 0] -= trunk
                tags[:, TAGS.index('trunk')] = trunk
            for i in range(1, 11, 2):
                start, added = 'trunk' + str(i), 'trunk' + str(i + 1)
                axis = tails[added] - heads[start]
                parameter = np.clip((vertices - heads[start]) @ axis / (axis @ axis), 0, 1)
                # Smooth transition along each original segment, without changing its bind pose.
                share = parameter * parameter * (3 - 2 * parameter)
                weight = weights[:, by[start]].copy()
                weights[:, by[start]] = weight * (1 - share)
                weights[:, by[added]] += weight * share
        else:
            ear = weights[:, [by[n] for n in names if n.startswith('ear')]].sum(axis=1)
            tags[:, 0] -= ear
            tags[:, TAGS.index('ear')] = ear
        if kind == 'rhino':
            for vertex in mesh.vertices:
                for group in vertex.groups:
                    key = source_key(ob.vertex_groups[group.group].name, kind)
                    if '_Eye' in key and group.weight > .5:
                        tags[vertex.index] = np.eye(len(TAGS))[TAGS.index('eye')]
                    elif key.startswith('Tongue') and group.weight > .5:
                        tags[vertex.index] = np.eye(len(TAGS))[TAGS.index('mouth')]
        weights = build._limit_influences(weights)
        if np.any(weights.sum(axis=1) < .999):
            raise ValueError('Unweighted source vertices')
        for key, value in [('vertices', vertices), ('triangles', triangles + offset), ('uv', uv),
                           ('weights', weights), ('tags', tags)]:
            arrays[key].append(value)
        offset += len(vertices)
    data = {key: np.concatenate(values) for key, values in arrays.items()}
    floor = data['vertices'][:, 2].min()
    center = (data['vertices'][:, 1].min() + data['vertices'][:, 1].max()) / 2
    origin = np.array([0, center, floor])
    data['vertices'] -= origin
    joint_heads = np.array([heads[n] - origin if n != 'root' else heads[n] for n in names])
    joint_tails = np.array([tails[n] - origin if n != 'root' else tails[n] for n in names])
    # Measure the shoulder's dorsal surface, excluding the head and the nuchal crest.
    shoulder_x = heads['scapula.L'][0]
    mask = (np.abs(data['vertices'][:, 0] - shoulder_x) < (.1 if kind == 'rhino' else .25))
    mask &= np.abs(data['vertices'][:, 1]) < (.15 if kind == 'rhino' else .4)
    height = data['vertices'][mask, 2].max()
    factor = (1.8 if kind == 'rhino' else 3.2) / height
    data['vertices'] *= factor
    if kind == 'rhino':
        points = data['vertices']
        # The horns are disconnected UV-chart components, not the forehead. Spatial
        # masks alone would include the high nasal ridge and distort the calf's face.
        parents_vertex = list(range(len(points)))
        def component_root(index):
            while parents_vertex[index] != index:
                parents_vertex[index] = parents_vertex[parents_vertex[index]]
                index = parents_vertex[index]
            return index
        for triangle in data['triangles']:
            root = component_root(int(triangle[0]))
            for vertex in triangle[1:]:
                parents_vertex[component_root(int(vertex))] = root
        components = {}
        for index in range(len(points)):
            components.setdefault(component_root(index), []).append(index)
        for indices in components.values():
            positions = points[indices]
            if (100 < len(indices) < 1000 and positions[:, 0].min() > 1.5
                    and positions[:, 2].min() > .8 and np.abs(positions[:, 1]).max() < .15):
                data['tags'][indices] = np.eye(len(TAGS))[TAGS.index('horn')]
    joint_heads *= factor
    joint_tails *= factor
    for side in ('.L', '.R'):
        for foot in ('fingers', 'toes'):
            center_foot = joint_heads[by[foot + side]]
            nail = (data['vertices'][:, 2] < .15)
            nail &= data['vertices'][:, 0] > center_foot[0] + .07
            nail &= data['vertices'][:, 0] < center_foot[0] + .30
            nail &= np.abs(data['vertices'][:, 1] - center_foot[1]) < .22
            data['tags'][nail] = np.eye(len(TAGS))[TAGS.index('nails')]
    # Sample Blender's evaluated authored animation; preserve world deltas for retargeting
    # across the reversed elephant spine, and local rotations for portable playback.
    action = next(a for a in bpy.data.actions if ('Armature' in a.name if kind == 'elephant' else True))
    arm.animation_data.action = action
    arm.animation_data.use_nla = False
    start, end = action.frame_range
    frame_count = 81
    world_rotations = np.zeros((frame_count, len(names), 3, 3))
    walk_positions = np.zeros((frame_count, len(names), 3))
    for frame in range(frame_count):
        t = float(start + (end - start) * frame / (frame_count - 1))
        bpy.context.scene.frame_set(int(t), subframe=t % 1)
        for j, name in enumerate(names):
            source_name = source_bones[name]
            if source_name is None:
                world_rotations[frame, j] = np.eye(3)
                continue
            bone = arm.data.bones[source_name]
            pose = arm.pose.bones[source_name]
            deform = arm.matrix_world @ pose.matrix @ bone.matrix_local.inverted() @ arm.matrix_world.inverted()
            rotation = np.array(deform.to_3x3().normalized())
            world_rotations[frame, j] = conversion @ rotation @ conversion.T
            walk_positions[frame, j] = (animal(arm.matrix_world @ pose.head) - origin) * factor
    local = world_rotations.copy()
    for j, parent in enumerate(parents):
        if parent:
            local[:, j] = np.swapaxes(world_rotations[:, by[parent]], 1, 2) @ world_rotations[:, j]
    loop_error = float(np.max(np.abs(local[-1] - local[0])))
    local[-1] = local[0]
    walk_positions[-1] = walk_positions[0]
    duration = (end-start) / bpy.context.scene.render.fps
    stance_measurements = []
    for foot in ('fingers.L', 'fingers.R', 'toes.L', 'toes.R'):
        positions = walk_positions[:, by[foot]].copy()
        positions[:, :2] -= walk_positions[:, by['hips'], :2]
        speed = -np.diff(positions[:, 0]) / (duration / (frame_count-1))
        cutoff = np.quantile(positions[:, 2], .35)
        contact = (positions[:-1, 2] <= cutoff) & (positions[1:, 2] <= cutoff) & (speed > 0)
        stance_measurements.append(np.quantile(speed[contact], [0, .5, 1]))
    data.update(authored_stance_speeds=np.array(stance_measurements), bone_names=np.array(names),
                bone_parents=np.array(parents), heads=joint_heads,
                tails=joint_tails, tag_names=np.array(TAGS), mapping_source=np.array(list(mapping)),
                mapping_target=np.array(list(mapping.values())), walk_rotations=local,
                walk_positions=walk_positions, walk_duration=np.array((end-start) / bpy.context.scene.render.fps),
                source_scale=np.array(factor), loop_endpoint_error=np.array(loop_error))
    for key, value in data.items():
        if value.dtype.kind == 'f':
            data[key] = value.astype(np.float32)
    np.savez_compressed(out / 'rigged.npz', **data)
    if kind == 'elephant':
        from zoogen.rigged import bake_source_occlusion
        bake_source_occlusion(data, out)
    title, author, slug = CREDITS[kind]
    changes = ['Removed camera, lights, helper shapes and unused animation controls.',
               'Converted the neutral bind mesh and joints to metres and animal axes; grounded feet.',
               'Renamed and merged joints, summed weights, retained at most four normalized influences.',
               'Resampled the authored walk to 81 samples; repeated the first endpoint for exact looping.',
               'Extracted source images through Blender; no executable download content is used.',
               'Tagged eyes, horns/tusks, ears, trunk and mouth where separable; nail tags use foot position.',
               'Measured backward stance velocities in the hip-relative frame, using the lowest 35% of foot heights.']
    if kind == 'elephant':
        changes += ['Repacked five material UV layouts into one 2048 atlas with eight-pixel gutters.',
                    'Subdivided five trunk segments into ten and reduced each ear to two joints.',
                    'The source has NO occlusion map and NO eye normal map. AO is baked from the neutral '
                    'source geometry with Cycles (1024 pixels, 16 samples, two threads). '
                    'The eye normal tile is flat. Neither is claimed as authored source detail.',
                    'The prepared source is Asian; African conversion belongs to the species build.']
    text = f'# {title}\n\nBy {author}.\n\nhttps://sketchfab.com/3d-models/{slug}\n\n'
    text += 'License: [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/).\n\nChanges:\n\n'
    text += ''.join('- ' + change + '\n' for change in changes)
    text += f'\nPrepared: {len(data["triangles"])} triangles, {len(names)} bones. '
    text += f'Original local-rotation endpoint maximum difference: {loop_error:.6g}.\n\n'
    text += 'Authored stance speeds at prepared size (minimum / median / maximum, m/s):\n\n'
    for foot, values in zip(('FL', 'FR', 'HL', 'HR'), stance_measurements):
        text += f'- {foot}: {values[0]:.4f} / {values[1]:.4f} / {values[2]:.4f}\n'
    text += '\nThe game clips use contact-constrained procedural replacement motion: these source '
    text += 'velocities vary beyond the existing 2% foot-sliding tolerance. Original resampled '
    text += 'rotations and positions remain in the NPZ for future animation work.\n\n'
    if kind == 'rhino':
        text += ('Species builds lower the tail, moderately desaturate the skin, and create a calf '
                 'with shorter body/head, larger ears, reduced hump and normal strength. The calf '
                 'replaces the two adult horn components with closed skin patches and a front stub; '
                 'patch UVs continue the surrounding forehead layout. Baked adult horn-cap pixels '
                 'in the calf colour, normal and AO maps are resampled from adjacent photographic '
                 'skin with a feathered border. It is decimated to 21,000 triangles.\n\n')
    else:
        text += ('Species builds mirror the intact left ear over the perforated right ear, '
                 'reshape/enlarge the ears, depress the back, round the crown, '
                 'raise the head, adjust body proportions, remove pink coloration and reduce '
                 'pigment contrast on the face/ears. New curved tusks sample the source ivory tile '
                 'and follow the head rigidly; two new trunk-tip fingers sample the source skin. '
                 'Cow/calf proportions differ; the calf has no tusks and reduced normal strength.\n\n')
    text += ('Builds export baseline JPEG colour (quality 90), JPEG normal (97), PNG occlusion, '
             'standard glTF tangent attributes and roughness 0.9. The neutral source PNGs remain '
             'unaltered by the variant builder.\n\nBone mapping:\n\n')
    text += ''.join(f'- `{src}` → `{dst}`\n' for src, dst in mapping.items())
    (out / 'README.md').write_text(text)
    print('PREPARED', kind, len(data['triangles']), 'triangles', len(names), 'bones',
          sum(p.stat().st_size for p in out.iterdir() if p.is_file()), 'bytes')
    # Reconstruct from the derivative, so the preparation proof also tests portability.
    from zoogen.rigged import prepared_preview
    prepared_preview(data, out, kind)


if __name__ == '__main__':
    main()
