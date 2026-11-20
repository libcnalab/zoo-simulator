# SPDX-License-Identifier: MIT
"""Species-data proportion edits and anatomy for the two portable pachyderm skins."""
from functools import partial
from pathlib import Path

import numpy as np

from .geom import Bone, Spec
from .rig import Leg, Rig, euler
from .sculpt import read_source

SOURCE_DIR = Path(__file__).resolve().parents[1] / 'sources'


def smoothstep(low, high, value):
    t = np.clip((value - low) / (high - low), 0, 1)
    return t * t * (3 - 2 * t)


def edit_bind(source, edits):
    """Hierarchical affine edits, blended with the authored skin weights."""
    names, parents = source['bone_names'].tolist(), source['bone_parents'].tolist()
    transforms = {}
    heads, tails = [], []
    for name, parent, head, tail in zip(names, parents, source['heads'], source['tails']):
        edit = edits.get(name, edits.get(name.split('.')[0], {}))
        affine = np.eye(4)
        rotation = edit.get('rotate', [0, 0, 0])
        affine[:3, :3] = euler(*rotation) @ np.diag(edit.get('scale', [1, 1, 1]))
        affine[:3, 3] = head - affine[:3, :3] @ head + np.array(edit.get('offset', [0, 0, 0]))
        transform = transforms.get(parent, np.eye(4)) @ affine
        transforms[name] = transform
        heads.append((transform @ np.append(head, 1))[:3])
        tails.append((transform @ np.append(tail, 1))[:3])
    vertices = np.zeros_like(source['vertices'], dtype=np.float64)
    for i, name in enumerate(names):
        transform = transforms[name]
        vertices += source['weights'][:, i, None] * (source['vertices'] @ transform[:3, :3].T + transform[:3, 3])
    return vertices, np.array(heads), np.array(tails)


def elephant_shape(data, shape):
    vertices, heads, tails = data['vertices'], data['heads'], data['tails']
    names = data['bone_names'].tolist()
    tags = data['tag_names'].tolist()
    ear = data['tags'][:, tags.index('ear')]
    # The ears are separate source surfaces. Move the entire surface, rather than
    # stretching just its ear-bone weights and leaving an unintended crease at the rim.
    for side, sign in [('.L', 1), ('.R', -1)]:
        pivot = heads[names.index('ear' + side)].copy()
        selected = (ear > .5) & (vertices[:, 1] * sign > 0)
        delta = vertices[selected] - pivot
        delta *= shape['ear_scale']
        delta[:, 0] -= shape.get('ear_sweep', .12) * smoothstep(.05, .8, np.abs(delta[:, 1]))
        vertices[selected] = pivot + delta
        for name in ('ear' + side, 'ear_tip' + side):
            index = names.index(name)
            heads[index] = pivot + (heads[index] - pivot) * shape['ear_scale']
            tails[index] = pivot + (tails[index] - pivot) * shape['ear_scale']
    # A smooth dorsal-only displacement removes the Asian arch while preserving the belly.
    x, z = vertices[:, 0], vertices[:, 2]
    dorsal = smoothstep(2.35, 3.05, z) * (1 - ear)
    vertices[:, 2] -= shape['saddle'] * np.exp(-((x - .05) / .65) ** 2) * dorsal
    if shape.get('calf_skin'):
        slim_calf(data)
    # Fill the medial crown depression without widening the temples.
    head_weight = data['weights'][:, names.index('head')]
    vertices[:, 2] += shape['dome'] * np.exp(-(vertices[:, 1] / .23) ** 2) * head_weight * smoothstep(2.8, 3.2, z)
    # Recede the upper forehead into a flatter slope, preserving the rounded
    # medial dome and leaving the eyes and trunk attachment undisturbed.
    upper = head_weight * (1 - ear) * smoothstep(3.05, 3.65, vertices[:, 2])
    vertices[:, 0] -= shape.get('forehead_recede', 0) * upper * smoothstep(2.1, 2.5, vertices[:, 0])
    vertices[:, 2] -= shape.get('crown_lower', 0) * upper
    from .elephant_look import ear_uv
    # Lower the broad upper lobe below the crown without shortening the lower tip.
    crown = vertices[(head_weight > .65) & (ear < .5), 2].max()
    for sign in (-1, 1):
        selected = (ear > .5) & (vertices[:, 1] * sign > 0)
        high = vertices[selected, 2].max()
        vertices[selected, 2] -= max(0, high - crown + .12) * smoothstep(2.2, high, vertices[selected, 2])
    ear_uv(data, shape)
    replace_tusks(data, shape)
    add_trunk_fingers(data)


def slim_calf(data):
    """Narrow limb cross-sections without changing the stance or leg IK lengths."""
    vertices = data['vertices']
    names = data['bone_names'].tolist()
    original = vertices.copy()
    for index, name in enumerate(names):
        if not name.startswith(('upper_arm', 'forearm', 'thigh', 'shin')):
            continue
        axis = data['tails'][index] - data['heads'][index]
        axis /= np.linalg.norm(axis)
        delta = original - data['heads'][index]
        radial = delta - (delta @ axis)[:, None] * axis
        vertices -= .12 * radial * data['weights'][:, index, None]
    core = [names.index(name) for name in ('hips', 'spine1', 'spine2', 'chest')]
    body = data['weights'][:, core].sum(axis=1)
    belly = (1 - smoothstep(1.5, 2.2, original[:, 2])) * np.exp(-(original[:, 0] / .85) ** 2)
    vertices[:, 2] += .18 * body * belly


def append_loft(data, part, tile, tag):
    from .geom import loft
    vertices, triangles, _ = loft(part)
    count = len(data['vertices'])
    names = data['bone_names'].tolist()
    weights = np.zeros((len(vertices), len(names)))
    for bone, weight in part.weights.items():
        weights[:, names.index(bone)] = weight
    tags = np.zeros((len(vertices), len(data['tag_names'])))
    tags[:, data['tag_names'].tolist().index(tag)] = 1
    # Cylindrical coordinates in the source ivory tile keep its real colour variation.
    uv_vertex = np.zeros((len(vertices), 2))
    uv_vertex[:, 0] = (np.arange(len(vertices)) % part.segments) / part.segments
    uv_vertex[:, 1] = np.linspace(.04, .96, len(vertices))
    if tag == 'tusk':
        # Constant longitudinal UV per cross-section gives an unstretched ivory
        # gradient and lets tests recover the actual exported centerline.
        rings = (len(vertices) - 2) // part.segments
        uv_vertex[:-2, 1] = np.repeat(np.linspace(.04, .96, rings), part.segments)
        uv_vertex[-2:, 1] = [.04, .96]
        uv_vertex[:, 0] = .5 + (uv_vertex[:, 0] - .5) * .35 * (1 - uv_vertex[:, 1])
        uv_vertex[:, 1] = .25 + .6 * uv_vertex[:, 1]
    uv = uv_vertex[triangles]
    uv = np.array(tile[:2]) + uv * np.array(tile[2:])
    data['vertices'] = np.concatenate([data['vertices'], vertices])
    data['triangles'] = np.concatenate([data['triangles'], triangles + count])
    data['uv'] = np.concatenate([data['uv'], uv])
    data['weights'] = np.concatenate([data['weights'], weights])
    data['tags'] = np.concatenate([data['tags'], tags])


def replace_tusks(data, shape):
    from .geom import Part, R
    tusk = data['tags'][:, data['tag_names'].tolist().index('tusk')] > .5
    keep = ~np.any(tusk[data['triangles']], axis=1)
    data['triangles'], data['uv'] = data['triangles'][keep], data['uv'][keep]
    # The original short tusks include trunk weights; new longer tusks must be rigid
    # in the maxilla. Their source ivory atlas tile remains the only material.
    if shape.get('hide_tusks'):
        return
    source_vertices = data['vertices'].copy()
    for sign in (-1, 1):
        points = source_vertices[tusk & (source_vertices[:, 1] * sign > 0)]
        base = points[np.argmax(points[:, 2])].copy()
        base[0] -= .04
        length = shape['tusk_length']
        radius = .095 * shape['tusk_width']
        rings = []
        for t in np.linspace(0, 1, 9):
            center = base + length * np.array([.78 * t, sign * (.04 * t - .10 * t*t), -.72 * t + .55 * t*t])
            width = max(.0025, radius * (1-t) ** .7)
            rings.append(R(center, width, width))
        part = Part('tusk', rings, weights={'head': 1}, segments=16, subdiv=2,
                    cap_start='flat', cap_end='point', fuse=False)
        append_loft(data, part, (8/2048, 1048/2048, 512/2048, 512/2048), 'tusk')


def add_trunk_fingers(data):
    from .geom import Part, R
    index = data['bone_names'].tolist().index('trunk10')
    tip = data['tails'][index].copy()
    axis = tip - data['heads'][index]
    axis /= np.linalg.norm(axis)
    trunk = data['tags'][:, data['tag_names'].tolist().index('trunk')] > .8
    candidates = data['vertices'][trunk]
    nearest = np.argsort(np.linalg.norm(candidates - tip, axis=1))[:12]
    # The source bone tail extends beyond the actual skin; attach to the skin itself.
    tip = candidates[nearest].mean(axis=0) - .018 * axis
    across = np.array([axis[2], 0, -axis[0]])
    for sign in (-1, 1):
        base = tip + sign * .017 * across
        middle = base + .024 * axis + sign * .012 * across
        end = base + .055 * axis + sign * .008 * across
        part = Part('trunk_finger', [R(base, .018, .018), R(middle, .014, .013),
                    R(end, .003, .003)], weights={'trunk10': 1}, segments=10, subdiv=3,
                    cap_start='flat', cap_end='round', fuse=False)
        append_loft(data, part, (1072/2048, 250/2048, 24/2048, 40/2048), 'trunk')


def rhino_shape(data, shape):
    v = data['vertices']
    v[:, 2] -= shape.get('hump_reduce', 0) * np.exp(-((v[:, 0] - .35) / .7) ** 2) * smoothstep(1.2, 1.8, v[:, 2])


def calf_horns(original, scale):
    """Close both adult horn sockets with skin; retain a small dome on the front one."""
    source = {key: value.copy() for key, value in original.items()}
    points = original['vertices']
    horn = original['tags'][:, original['tag_names'].tolist().index('horn')] > .5
    body_coordinates = {tuple(point): i for i, point in enumerate(np.round(points, 4)) if not horn[i]}
    vertex_uv = np.zeros((len(points), 2))
    vertex_uv[original['triangles'].ravel()] = original['uv'].reshape(-1, 2)
    keep = ~np.any(horn[original['triangles']], axis=1)
    triangles = [original['triangles'][keep]]
    uvs = [original['uv'][keep]]
    for front in (True, False):
        selected = horn & ((points[:, 0] > 1.76) if front else (points[:, 0] <= 1.76))
        indices = np.flatnonzero(selected)
        boundary = [i for i in indices if tuple(np.round(points[i], 4)) in body_coordinates]
        # UV seams can duplicate a ring endpoint; keep just one geometric point.
        unique = {}
        for index in boundary:
            unique[tuple(np.round(points[index], 4))] = index
        ring = np.array(list(unique.values()))
        pivot = points[ring].mean(axis=0)
        _, _, axes = np.linalg.svd(points[ring] - pivot)
        normal = axes[-1]
        normal *= np.sign(np.dot(points[indices].mean(axis=0) - pivot, normal))
        angles = np.arctan2((points[ring]-pivot) @ axes[1], (points[ring]-pivot) @ axes[0])
        ring = ring[np.argsort(angles)]
        # Continue the surrounding forehead UVs through the closed socket. Sampling
        # an unrelated skin patch here left a conspicuous pale disk on the calf.
        border_uv = np.array([vertex_uv[body_coordinates[tuple(np.round(points[i], 4))]] for i in ring])
        center_uv = border_uv.mean(axis=0)
        patch_vertices, patch_uv, patch = [], [], []
        first_new = len(source['vertices'])
        previous = ring
        previous_uv = border_uv
        for radius, rise in ((.65, .008), (.25, .30 * scale)) if front else ():
            next_ring = np.arange(len(ring)) + first_new + len(patch_vertices)
            positions = pivot + radius * (points[ring] - pivot) + rise * normal
            coordinates = center_uv + radius * (border_uv - center_uv)
            patch_vertices.extend(positions)
            for i in range(len(ring)):
                j = (i + 1) % len(ring)
                patch.extend([[previous[i], previous[j], next_ring[j]],
                              [previous[i], next_ring[j], next_ring[i]]])
                patch_uv.extend([[previous_uv[i], previous_uv[j], coordinates[j]],
                                 [previous_uv[i], coordinates[j], coordinates[i]]])
            previous, previous_uv = next_ring, coordinates
        center_id = first_new + len(patch_vertices)
        patch_vertices.append(pivot + normal * (.4 * scale if front else 0))
        for i in range(len(ring)):
            j = (i + 1) % len(ring)
            patch.append([previous[i], previous[j], center_id])
            patch_uv.append([previous_uv[i], previous_uv[j], center_uv])
        source['vertices'] = np.concatenate([source['vertices'], patch_vertices])
        weight = np.zeros((len(patch_vertices), original['weights'].shape[1]))
        weight[:, original['bone_names'].tolist().index('head')] = 1
        source['weights'] = np.concatenate([source['weights'], weight])
        tag = np.zeros((len(patch_vertices), len(original['tag_names'])))
        tag[:, 0] = 1
        source['tags'] = np.concatenate([source['tags'], tag])
        patch, patch_uv = np.array(patch), np.array(patch_uv)
        triangle = source['vertices'][patch[0]]
        if np.dot(np.cross(triangle[1] - triangle[0], triangle[2] - triangle[0]), normal) < 0:
            patch, patch_uv = patch[:, [1, 0, 2]], patch_uv[:, [1, 0, 2]]
        triangles.append(patch)
        uvs.append(patch_uv)
    source['triangles'], source['uv'] = np.concatenate(triangles), np.concatenate(uvs)
    return source


def intact_elephant_ears(original):
    """Mirror the intact left ear over the source's perforated right ear."""
    source = {key: value.copy() for key, value in original.items()}
    ear = original['tags'][:, original['tag_names'].tolist().index('ear')] > .5
    left = ear & (original['vertices'][:, 1] > 0)
    right = ear & (original['vertices'][:, 1] < 0)
    selected = np.all(left[original['triangles']], axis=1)
    keep = ~np.any(right[original['triangles']], axis=1)
    indices = np.flatnonzero(left)
    remap = np.zeros(len(left), dtype=int)
    remap[indices] = np.arange(len(indices)) + len(left)
    mirrored = remap[original['triangles'][selected]][:, [0, 2, 1]]
    source['vertices'] = np.concatenate([original['vertices'], original['vertices'][indices] * [1, -1, 1]])
    source['triangles'] = np.concatenate([original['triangles'][keep], mirrored])
    source['uv'] = np.concatenate([original['uv'][keep], original['uv'][selected][:, [0, 2, 1]]])
    names = original['bone_names'].tolist()
    columns = [names.index(n[:-1] + ('R' if n.endswith('.L') else 'L'))
               if n.endswith(('.L', '.R')) else i for i, n in enumerate(names)]
    source['weights'] = np.concatenate([original['weights'], original['weights'][indices][:, columns]])
    source['tags'] = np.concatenate([original['tags'], original['tags'][indices]])
    return source


def make_pachyderm(model_name, kind, height, edits, shape, walk, fast, meta):
    source_path = SOURCE_DIR / kind / 'rigged.npz'
    source = read_source(str(source_path))
    if kind == 'elephant':
        shape = dict(shape)
        source = intact_elephant_ears(source)
    if kind == 'rhino' and shape.get('horn_scale', 1) != 1:
        source = calf_horns(source, shape['horn_scale'])
    data = {key: value.copy() for key, value in source.items()}
    vertices, heads, tails = edit_bind(source, edits)
    data.update(vertices=vertices, heads=heads, tails=tails)
    if kind == 'elephant':
        elephant_shape(data, shape)
    else:
        rhino_shape(data, shape)
    used = np.unique(data['triangles'])
    remap = np.zeros(len(data['vertices']), dtype=int)
    remap[used] = np.arange(len(used))
    data['triangles'] = remap[data['triangles']]
    for key in ('vertices', 'weights', 'tags'):
        data[key] = data[key][used]
    names = data['bone_names'].tolist()
    # Remove source numerical asymmetry in joints. The mesh itself is not mirrored or repainted.
    for name in names:
        if name.endswith('.L'):
            left, right = names.index(name), names.index(name[:-1] + 'R')
            for values in (heads, tails):
                average = (values[left] + values[right] * [1, -1, 1]) / 2
                values[left], values[right] = average, average * [1, -1, 1]
    floor = data['vertices'][:, 2].min()
    data['vertices'][:, 2] -= floor
    heads[1:, 2] -= floor
    tails[1:, 2] -= floor
    chest_x = heads[names.index('scapula.L'), 0]
    shoulder = (np.abs(data['vertices'][:, 0] - chest_x) < .13) & (np.abs(data['vertices'][:, 1]) < .18)
    measured = data['vertices'][shoulder, 2].max()
    scale = height / measured
    for key in ('vertices', 'heads', 'tails'):
        data[key] *= scale
    bones = [Bone(n, p or None, h, t, n != 'root') for n, p, h, t in
             zip(names, data['bone_parents'], heads, tails) if not n.endswith('.R')]
    legs = {}
    for side in ('L', 'R'):
        suffix = '.' + side
        legs['F' + side] = Leg('upper_arm' + suffix, 'forearm' + suffix, 'hand' + suffix,
                               'fingers' + suffix, 'scapula' + suffix)
        legs['H' + side] = Leg('thigh' + suffix, 'shin' + suffix, 'foot' + suffix, 'toes' + suffix)
    if kind == 'elephant':
        muzzle = tails[names.index('trunk10')].copy()
        mouth = heads[names.index('jaw')].copy() + [.10 * height, 0, -.12 * height]
        muzzle_bone = 'trunk10'
    else:
        points = data['vertices']
        mask = (points[:, 0] > .72 * points[:, 0].max()) & (np.abs(points[:, 1]) < .08 * height)
        muzzle = points[mask][np.argmin(points[mask, 2])].copy()
        muzzle_bone, mouth = 'head', muzzle.copy()
    rig = Rig('hips', legs, ['spine1', 'spine2'], 'chest', ['neck1', 'neck2'], 'head', 'jaw',
              [n for n in names if n.startswith('tail')], ['ear.L', 'ear.R'],
              {'muzzle': (muzzle_bone, muzzle), 'mouth': mouth})
    data.update(source_path=str(source_path), rigged=True, kind=kind, height=height, shape=shape,
                variant=meta.get('variant', 'adult'), scale=scale, target_walk=walk, target_fast=fast)
    meta = dict(meta, shoulder_height_m=height, swim_depth_m=.82 * height, roughness=.9,
                asset_license='CC-BY-4.0', source_author='LasquetiSpice' if kind == 'rhino' else 'exegetpro',
                motion_source='procedural replacement; authored source samples retained in derivative',
                authored_stance_speed_medians=source['authored_stance_speeds'][:, 1].tolist(),
                occlusion_source='source texture' if kind == 'rhino' else 'Cycles bake of neutral source geometry (source has no AO image)')
    if kind == 'elephant':
        # The game's clip for a still activity (AnimalRenderer).
        meta['activity_clips'] = {'dust_bathe': 'dust_bathe', 'bathe': 'dust_bathe'}
    else:
        meta['activity_clips'] = {'bathe': 'bathe'}
    from .pachyderm_motion import clips
    return Spec(model_name, bones, [], texture_size=1024 if kind == 'rhino' else 2048,
                target_tris=21000 if meta.get('variant') == 'juvenile' else 34000,
                meta=meta, rig=rig, clips=partial(clips, data=data), sculpt=data, key_all_locations=True)
