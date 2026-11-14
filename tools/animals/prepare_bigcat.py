# SPDX-License-Identifier: MIT
"""Import the untrusted lioness only through Blender and prepare a portable derivative.

Run from the repository, never from the download directory. The original is not
needed by the generator. Authored UVs and skin weights survive simplification.
"""
import argparse
from pathlib import Path
import sys

import bpy
import numpy as np
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
from zoogen import build, preview
from zoogen.rig import rot_y
from zoogen.bigcat_review import head_comparison


PREFIXES = {
    'Coluna1': 'hips', 'Coluna2': 'spine1', 'Coluna3': 'spine2', 'Coluna4': 'chest',
    'Pescoco1': 'neck1', 'Pescoco2': 'neck1', 'Pescoco3': 'neck2',
    'Cabeca': 'head', 'Maxilar': 'jaw', 'Orelha': 'ear', 'Clavicula': 'scapula',
    'Braco': 'upper_arm', 'Antebraco': 'forearm', 'Mao1': 'hand', 'Mao2': 'fingers',
    'Femur': 'thigh', 'Canela': 'shin', 'Metatarso': 'foot', 'Pe': 'toes',
}
TAG_NAMES = ['body', 'eye', 'mouth', 'teeth', 'claws', 'nose', 'lips']


def mapped(name):
    if name == '_rootJoint':
        return 'root'
    stem = name.split('_')[0]
    side = '.' + stem.split('.')[1] if '.' in stem else ''
    stem = stem.split('.')[0]
    if stem.startswith('Rabo'):
        number = int(stem[4:])
        return 'tail' + str(min(4, (number - 1) // 3 + 1))
    return PREFIXES[stem] + side


def image_array(image):
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    return pixels.reshape(height, width, 4)


def save_grey(path, grey):
    height, width = grey.shape
    image = bpy.data.images.new(path.stem, width, height, alpha=False)
    rgba = np.ones((height, width, 4), dtype=np.float32)
    rgba[..., :3] = grey[..., None]
    image.pixels.foreach_set(rgba.ravel())
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    bpy.data.images.remove(image)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--jaw-close', type=float, default=31.5)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    scene = build.reset_scene()
    bpy.ops.import_scene.gltf(filepath=str(Path(args.source).resolve()))
    arm = next(ob for ob in bpy.data.objects if ob.type == 'ARMATURE')
    print('SOURCE', len(arm.data.bones), 'joints')
    for pose in arm.pose.bones:
        if pose.name not in ('_rootJoint', 'Coluna1_01'):
            pose.matrix_basis = Matrix.Identity(4)
    bpy.context.view_layer.update()
    # Keep the placement-bearing first spine transform during skin evaluation,
    # then remove that common rigid placement, not the mesh's bind deformation.
    undo = arm.data.bones['Coluna1_01'].matrix_local @ arm.pose.bones['Coluna1_01'].matrix.inverted()
    hip = np.array(arm.data.bones['Coluna1_01'].head_local)

    def animal(points):
        points = np.asarray(points) - hip
        return np.stack([-points[..., 1] - .35, points[..., 0], points[..., 2] + .94], axis=-1)

    names = ['root']
    heads = [np.zeros(3)]
    parents = ['']
    source_names = ['_rootJoint']
    mapping = {}
    for bone in arm.data.bones:
        target = mapped(bone.name)
        mapping[bone.name] = target
        if '_end_' in bone.name or target in names:
            continue
        names.append(target)
        heads.append(animal(bone.head_local))
        parents.append(mapped(bone.parent.name))
        source_names.append(bone.name)
    heads = np.asarray(heads)
    by = {name: index for index, name in enumerate(names)}
    # Source roundoff is several micrometres at its offset origin; enforce bilateral joints.
    for name in names:
        if name.endswith('.L'):
            left, right = by[name], by[name[:-1] + 'R']
            mean = (heads[left] + heads[right] * [1, -1, 1]) / 2
            heads[left], heads[right] = mean, mean * [1, -1, 1]
        elif name != 'root' and '.' not in name:
            heads[by[name], 1] = 0
    comparison = []
    material = bpy.data.materials['Leao']
    bsdf = material.node_tree.nodes.get('Principled BSDF')
    base_image = bsdf.inputs['Base Color'].links[0].from_node.image
    base = image_array(base_image)
    normal_node = bsdf.inputs['Normal'].links[0].from_node
    normal_image = normal_node.inputs['Color'].links[0].from_node.image
    image_nodes = [node for node in material.node_tree.nodes if node.type == 'TEX_IMAGE']
    for node in image_nodes:
        print('SOURCE_MAP', node.name, node.image.name, tuple(node.image.size),
              [(link.to_node.name, link.to_socket.name) for link in node.outputs['Color'].links])
    orm_image = next(node.image for node in image_nodes
                     if node.image not in (base_image, normal_image))
    occlusion = image_array(orm_image)[..., 0]
    for filename, image in [('base.png', base_image), ('normal.png', normal_image),
                            ('orm.png', orm_image)]:
        image.filepath_raw = str(out / filename)
        image.file_format = 'PNG'
        image.save()
    luminance = base[..., :3] @ np.array([.2126, .7152, .0722])
    # Retain high-frequency fur and wrinkles without retaining the brown dye or broad lighting.
    blur = luminance.copy()
    for radius in (1, 2, 4, 8):
        blur = (blur + np.roll(blur, radius, 0) + np.roll(blur, -radius, 0)
                + np.roll(blur, radius, 1) + np.roll(blur, -radius, 1)) / 5
    detail = np.clip(.5 + 2.0 * (luminance - blur), 0, 1)
    save_grey(out / 'detail.png', detail)
    save_grey(out / 'occlusion.png', occlusion)
    arrays = {key: [] for key in ('vertices', 'triangles', 'uv', 'weights', 'tags', 'source_color')}
    offset = 0
    for ob in list(bpy.data.objects):
        if ob.type != 'MESH':
            continue
        mesh_name = ob.data.name
        if mesh_name not in ('Base_Leao_0', 'Olhos.002_Leao_0', 'Teeth_LOW.001_Leao_0'):
            bpy.data.objects.remove(ob, do_unlink=True)
            continue
        depsgraph = bpy.context.evaluated_depsgraph_get()
        mesh = bpy.data.meshes.new_from_object(ob.evaluated_get(depsgraph), depsgraph=depsgraph)
        transform = undo @ arm.matrix_world.inverted() @ ob.matrix_world
        coordinates = animal([transform @ vertex.co for vertex in mesh.vertices])
        ob.modifiers.clear()
        ob.parent = None
        ob.matrix_world = Matrix.Identity(4)
        ob.data = mesh
        mesh.vertices.foreach_set('co', build.to_blender(coordinates).ravel())
        build._activate(ob)
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.mesh.remove_doubles(threshold=.00001)
        bpy.ops.object.mode_set(mode='OBJECT')
        original = sum(len(poly.vertices) - 2 for poly in mesh.polygons)
        if mesh_name in ('Base_Leao_0', 'Olhos.002_Leao_0'):
            mesh.calc_loop_triangles()
            comparison.append((build.from_blender([v.co for v in mesh.vertices]),
                               np.array([tri.vertices[:] for tri in mesh.loop_triangles])))
        target = 29000 if mesh_name == 'Base_Leao_0' else 1800 if mesh_name.startswith('Teeth') else original
        if target < original:
            modifier = ob.modifiers.new('preserve_sculpt', 'DECIMATE')
            modifier.ratio = target / original
            modifier.use_collapse_triangulate = True
            if mesh_name == 'Base_Leao_0':
                protected = ob.vertex_groups.new(name='face_density')
                for vertex in mesh.vertices:
                    influence = sum(group.weight for group in vertex.groups
                                    if mapped(ob.vertex_groups[group.group].name)
                                    in ('head', 'jaw', 'ear.L', 'ear.R'))
                    if influence > .05:
                        protected.add([vertex.index], min(1.0, influence * 4), 'REPLACE')
                modifier.vertex_group = protected.name
                modifier.vertex_group_factor = 1000
                modifier.invert_vertex_group = True
                low, high = 0.0, modifier.ratio
                for _ in range(9):
                    modifier.ratio = (low + high) / 2
                    bpy.context.view_layer.update()
                    evaluated = ob.evaluated_get(bpy.context.evaluated_depsgraph_get())
                    count = sum(len(poly.vertices) - 2 for poly in evaluated.data.polygons)
                    if count > target:
                        high = modifier.ratio
                    else:
                        low = modifier.ratio
                modifier.ratio = low
            bpy.ops.object.modifier_apply(modifier=modifier.name)
        modifier = ob.modifiers.new('triangles', 'TRIANGULATE')
        bpy.ops.object.modifier_apply(modifier=modifier.name)
        mesh = ob.data
        vertices = build.from_blender([vertex.co for vertex in mesh.vertices])
        triangles = np.array([poly.vertices[:] for poly in mesh.polygons], dtype=np.int32)
        uv = np.array([loop.uv[:] for loop in mesh.uv_layers.active.data]).reshape(-1, 3, 2)
        weights = np.zeros((len(vertices), len(names)))
        for vertex in mesh.vertices:
            for group in vertex.groups:
                group_name = ob.vertex_groups[group.group].name
                if group_name not in mapping:
                    continue
                name = mapping[group_name]
                weights[vertex.index, by[name]] += group.weight
        weights = build._limit_influences(weights)
        if np.any(weights.sum(axis=1) < .99):
            raise ValueError('Source contains unweighted vertices')
        colors = np.zeros((len(vertices), 3))
        samples = np.zeros(len(vertices))
        width, height = base_image.size
        pixel = np.floor(uv * [width, height]).astype(int) % [width, height]
        for corner in range(3):
            np.add.at(colors, triangles[:, corner], base[pixel[:, corner, 1], pixel[:, corner, 0], :3])
            np.add.at(samples, triangles[:, corner], 1)
        colors /= np.maximum(samples[:, None], 1)
        tags = np.zeros((len(vertices), len(TAG_NAMES)))
        if mesh_name == 'Base_Leao_0':
            tags[:, 0] = 1
            nose = (vertices[:, 0] > 1.17) & (vertices[:, 2] > .98) & (vertices[:, 2] < 1.07)
            nose &= np.abs(vertices[:, 1]) < .055
            lips = ((colors.mean(axis=1) < .012) & (vertices[:, 0] > 1.04)
                    & (vertices[:, 2] < 1.0) & (vertices[:, 2] > .82))
            tags[nose] = np.eye(len(TAG_NAMES))[5]
            tags[lips & ~nose] = np.eye(len(TAG_NAMES))[6]
        elif mesh_name.startswith('Olhos'):
            tags[:, 1] = 1
        else:
            claws = vertices[:, 2] < .3
            teeth = (colors.min(axis=1) > .32) & ~claws
            tags[:, 2] = 1
            tags[teeth] = np.eye(len(TAG_NAMES))[3]
            tags[claws] = np.eye(len(TAG_NAMES))[4]
        # Bake jaw closure into vertices. The same authored jaw influence moves lips,
        # tongue, gums and lower teeth; the upper teeth remain attached to the skull.
        pivot = heads[by['jaw']]
        closed = (vertices - pivot) @ rot_y(-args.jaw_close).T + pivot
        vertices += weights[:, by['jaw'], None] * (closed - vertices)
        if mesh_name.startswith('Teeth'):
            # The source tongue projects through the upper lip when the open jaw is closed.
            # Retract soft mouth tissue into the oral cavity; its jaw weights are unchanged.
            soft = tags[:, 2] > .5
            vertices[soft, 0] -= .025
            # Retract the sculpt's permanently extended claws into their toe sheaths.
            claws = tags[:, 4] > .5
            vertices[claws, 0] -= .025
            vertices[claws, 2] -= .009
            # The open-mouth sculpt has long exposed canines but no closed-jaw sockets.
            # Shorten only the enamel vertically so the tips fit inside the closed lips.
            enamel = tags[:, 3] > .5
            vertices[enamel, 2] = .965 + .45 * (vertices[enamel, 2] - .965)
        for key, value in (('vertices', vertices), ('triangles', triangles + offset), ('uv', uv),
                           ('weights', weights), ('tags', tags), ('source_color', colors)):
            arrays[key].append(value)
        offset += len(vertices)
        print('PREPARED', mesh_name, len(vertices), len(triangles))
        bpy.data.objects.remove(ob, do_unlink=True)
    data = {key: np.concatenate(value) for key, value in arrays.items()}
    floor = data['vertices'][:, 2].min()
    data['vertices'][:, 2] -= floor
    heads[1:, 2] -= floor
    tails = heads.copy()
    next_bone = {'hips': 'spine1', 'spine1': 'spine2', 'spine2': 'chest', 'chest': 'neck1',
                 'neck1': 'neck2', 'neck2': 'head', 'tail1': 'tail2', 'tail2': 'tail3', 'tail3': 'tail4'}
    for side in ('.L', '.R'):
        for chain in (['scapula', 'upper_arm', 'forearm', 'hand', 'fingers'],
                      ['thigh', 'shin', 'foot', 'toes']):
            for start, end in zip(chain, chain[1:]):
                next_bone[start + side] = end + side
    for name, index in by.items():
        if name in next_bone:
            tails[index] = heads[by[next_bone[name]]]
        elif name in ('head', 'jaw'):
            tails[index] += [.23 if name == 'head' else .22, 0, -.025]
        elif name.startswith('ear'):
            tails[index] += [-.01, .035 if name.endswith('L') else -.035, .065]
        elif name.startswith(('fingers', 'toes')):
            tails[index] += [.10, 0, -.025]
        elif name == 'tail4':
            tails[index] += [-.05, 0, .005]
        else:
            tails[index] += [.1, 0, 0]
    data.update(bone_names=np.array(names), bone_parents=np.array(parents), heads=heads, tails=tails,
                tag_names=np.array(TAG_NAMES), mapping_source=np.array(list(mapping)),
                mapping_target=np.array(list(mapping.values())))
    for key in ('vertices', 'uv', 'weights', 'tags', 'source_color', 'heads', 'tails'):
        data[key] = data[key].astype(np.float32)
    np.savez_compressed(out / 'sculpt.npz', **data)
    print('DERIVATIVE', sum(path.stat().st_size for path in out.glob('*') if path.is_file()), 'bytes')
    head_comparison(comparison, data)
    build.reset_scene()
    ob = build._mesh_object('prepared_sculpt', build.to_blender(data['vertices']), data['triangles'])
    grey = bpy.data.materials.new('preparation_grey')
    grey.diffuse_color = (.36, .36, .36, 1)
    ob.data.materials.append(grey)
    for poly in ob.data.polygons:
        poly.use_smooth = True
    cam, _ = preview.setup_stage(bpy.context.scene, 2)
    preview.sculpt_lighting(bpy.context.scene)
    bpy.context.scene.render.threads = 4
    prefix = Path.cwd() / 'build-probe/animal-previews/bigcat_prepared'
    prefix.parent.mkdir(parents=True, exist_ok=True)
    preview.render_views(bpy.context.scene, cam, build.to_blender((.05, 0, .6)), 1.35, str(prefix))


if __name__ == '__main__':
    main()
